#include "us4/virality.hpp"

#include <algorithm>
#include <cmath>

namespace us4::virality {

namespace {

double clamp01(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }

std::string to_lower(const std::string& s) {
  std::string out = s;
  std::transform(out.begin(), out.end(), out.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return out;
}

}  // namespace

Score score_post(const Signals& s, const Weights& w, const Context& ctx) {
  Score out;

  // Hard-limit filters remove the post entirely (score below ranking floor).
  const std::pair<bool, const char*> filters[] = {
      {ctx.violence, "violence label"},
      {ctx.nsfw, "nsfw label"},
      {ctx.policy_violation, "policy violation"},
      {ctx.muted_keyword, "viewer muted-keyword match"},
      {ctx.blocked, "socialgraph block"},
      {ctx.muted, "socialgraph mute"},
      {ctx.duplicate_impression, "duplicate impression"},
      {ctx.engagement_bait, "self-engagement-bait loop"},
  };
  for (const auto& [hit, reason] : filters) {
    if (hit) {
      out.removed = true;
      out.removal_reason = reason;
      out.final = -1.0;  // removed from feed
      return out;
    }
  }

  // Video gating: VQV weight is zeroed below the minimum duration.
  const double vqv_w =
      s.video_duration_ms > w.min_video_duration_ms ? w.vqv : 0.0;
  const double quoted_vqv_w =
      s.quoted_video_duration_ms > w.min_video_duration_ms ? w.quoted_vqv : 0.0;

  auto add = [&](const char* name, double prob, double weight) {
    out.contributions.push_back({name, prob, weight, prob * weight});
    out.combined += prob * weight;
  };

  add("follow_author", s.follow_author, w.follow_author);
  add("reply", s.reply, w.reply);
  add("profile_click", s.profile_click, w.profile_click);
  add("retweet", s.retweet, w.retweet);
  add("quote", s.quote, w.quote);
  add("share", s.share, w.share);
  add("share_via_dm", s.share_via_dm, w.share_via_dm);
  add("share_via_copy_link", s.share_via_copy_link, w.share_via_copy_link);
  add("favorite", s.favorite, w.favorite);
  add("dwell", s.dwell, w.dwell);
  add("click", s.click, w.click);
  add("quoted_click", s.quoted_click, w.quoted_click);
  add("photo_expand", s.photo_expand, w.photo_expand);
  add("vqv", s.vqv, vqv_w);
  add("quoted_vqv", s.quoted_vqv, quoted_vqv_w);
  add("cont_dwell_time", s.cont_dwell_time, w.cont_dwell_time);
  add("click_dwell_time", s.click_dwell_time, w.click_dwell_time);
  add("not_interested", s.not_interested, w.not_interested);
  add("not_dwelled", s.not_dwelled, w.not_dwelled);
  add("mute_author", s.mute_author, w.mute_author);
  add("block_author", s.block_author, w.block_author);
  add("report", s.report, w.report);

  // Negative-score offset: a net-negative post sinks to the floor.
  out.offset_base =
      out.combined >= 0.0 ? out.combined : std::max(0.0, w.negative_scores_offset);

  // Out-of-network penalty and author-diversity decay.
  out.oon_multiplier = ctx.in_network ? 1.0 : w.oon_factor;
  out.diversity_multiplier =
      (1.0 - w.author_diversity_floor) *
          std::pow(w.author_diversity_decay,
                   static_cast<double>(std::max(0, ctx.author_position))) +
      w.author_diversity_floor;

  out.final = out.offset_base * out.oon_multiplier * out.diversity_multiplier;

  std::sort(out.contributions.begin(), out.contributions.end(),
            [](const Contribution& a, const Contribution& b) {
              return std::fabs(a.value) > std::fabs(b.value);
            });
  return out;
}

Analysis analyze_post(const std::string& text, const Weights& w) {
  Analysis a;
  const std::string lower = to_lower(text);
  const std::size_t len = text.size();

  // Text features.
  const bool has_question = text.find('?') != std::string::npos;
  const bool has_url =
      lower.find("http://") != std::string::npos ||
      lower.find("https://") != std::string::npos;
  const bool has_list = text.find("\n- ") != std::string::npos ||
                        text.find("\n1.") != std::string::npos ||
                        text.find("\n* ") != std::string::npos;
  const bool has_follow_cta = lower.find("follow") != std::string::npos;
  const bool has_video = lower.find("[video]") != std::string::npos;
  const bool has_image = lower.find("[image]") != std::string::npos ||
                         lower.find("[photo]") != std::string::npos;

  std::size_t first_line = text.find('\n');
  const std::size_t first_line_len =
      first_line == std::string::npos ? len : first_line;

  int hashes = 0;
  for (char c : text)
    if (c == '#') ++hashes;
  const bool mostly_hashtags = hashes >= 3 && len < 80;

  // Engagement-bait detection (hard-filter trigger).
  static const char* bait[] = {"like if",        "rt to win",
                               "retweet to win", "follow for follow",
                               "f4f",            "like and retweet to",
                               "tag a friend to win"};
  bool is_bait = false;
  for (const char* phrase : bait)
    if (lower.find(phrase) != std::string::npos) is_bait = true;

  // Estimated signals (documented proxy from features).
  Signals& s = a.estimated;
  s.dwell = clamp01(static_cast<double>(len) / 1500.0) * 0.7;
  s.cont_dwell_time = s.dwell;
  s.reply = clamp01(has_question ? 0.5 : 0.15);
  s.favorite = 0.30;
  s.retweet = (len > 80 && len < 280) ? 0.25 : 0.15;
  s.quote = 0.15;
  s.follow_author = clamp01(has_follow_cta ? 0.35 : 0.12);
  s.share = clamp01((has_list || has_url) ? 0.30 : 0.10);
  s.share_via_dm = s.share * 0.8;
  s.share_via_copy_link = has_url ? 0.30 : 0.10;
  s.profile_click = 0.12;
  s.click = has_url ? 0.40 : 0.08;
  s.quoted_click = 0.05;
  s.photo_expand = has_image ? 0.20 : 0.0;
  if (has_video) {
    s.video_duration_ms = 8000;  // assume an eligible (>5s) clip
    s.vqv = 0.30;
  }
  s.not_interested = (len < 40 || mostly_hashtags) ? 0.30 : 0.05;
  s.not_dwelled = len < 40 ? 0.40 : 0.10;

  a.context.engagement_bait = is_bait;
  a.score = score_post(s, w, a.context);

  // Checklist grounded in the documented strategic framework.
  auto item = [&](const std::string& id, bool pass, const std::string& note) {
    a.checklist.push_back({id, pass, note});
    if (!pass) a.tips.push_back(note);
  };

  item("hook", first_line_len > 0 && first_line_len <= 100,
       "Lead with a short stop-scroll hook (<=100 chars on the first line).");
  item("reply_loop", has_question,
       "Add an open loop or question - reply is the 2nd-highest weight (13.5).");
  item("follow_reason", has_follow_cta,
       "Give a concrete reason to follow - follow_author is the top weight (24.0).");
  item("length",
       len < 280 || (len >= 1000 && len <= 1500),
       "Be concise (<280) or go long-form (1000-1500 chars) to maximize dwell; "
       "avoid the 280-1000 dead zone.");
  item("share_worthy", has_list || has_url,
       "Make it screenshot-able / add an insider list or link to earn DM shares.");
  if (has_video) {
    item("video_duration", s.video_duration_ms > w.min_video_duration_ms,
         "Video must exceed 5s or its VQV weight is zeroed.");
  }
  item("no_bait", !is_bait,
       "Remove engagement-bait - self-engagement-bait loops are hard-filtered.");

  a.tips.push_back(
      "Space posts out: rapid successive posts decay via author diversity "
      "(decay 0.7); earn early first-hour engagement velocity.");
  return a;
}

}  // namespace us4::virality
