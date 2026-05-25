#include "us4/virality.hpp"

#include <cmath>
#include <string>

#include "us4_test.hpp"

using namespace us4::virality;

static bool approx(double a, double b) { return std::fabs(a - b) < 1e-6; }

static void follow_is_highest_leverage() {
  Signals s;
  s.follow_author = 1.0;
  Score r = score_post(s, Weights{}, Context{});
  US4_CHECK(!r.removed);
  US4_CHECK(approx(r.combined, 24.0));
  US4_CHECK(approx(r.final, 24.0));            // in-network, position 0
  US4_CHECK(r.contributions.front().name == "follow_author");
}

static void video_gating_zeroes_short_clips() {
  Signals s;
  s.vqv = 1.0;
  s.video_duration_ms = 3000;  // <= 5000 -> zeroed
  US4_CHECK(approx(score_post(s, Weights{}, Context{}).combined, 0.0));
  s.video_duration_ms = 8000;  // > 5000 -> counts
  US4_CHECK(approx(score_post(s, Weights{}, Context{}).combined, 0.005));
}

static void hard_filter_removes_post() {
  Signals s;
  s.follow_author = 1.0;
  Context ctx;
  ctx.nsfw = true;
  Score r = score_post(s, Weights{}, ctx);
  US4_CHECK(r.removed);
  US4_CHECK(r.removal_reason == "nsfw label");
  US4_CHECK(r.final < 0.0);
}

static void oon_and_diversity_adjustments() {
  Signals s;
  s.follow_author = 1.0;

  Context oon;
  oon.in_network = false;
  US4_CHECK(approx(score_post(s, Weights{}, oon).final, 12.0));  // 24 * 0.5

  Context pos1;
  pos1.author_position = 1;  // (0.7*0.7)+0.3 = 0.79
  US4_CHECK(approx(score_post(s, Weights{}, pos1).diversity_multiplier, 0.79));
  US4_CHECK(approx(score_post(s, Weights{}, Context{}).diversity_multiplier, 1.0));
}

static void negative_post_sinks_to_floor() {
  Signals s;
  s.report = 1.0;  // weight -100
  Score r = score_post(s, Weights{}, Context{});
  US4_CHECK(r.combined < 0.0);
  US4_CHECK(approx(r.final, 0.0));  // offset floor
}

static void analyze_flags_engagement_bait() {
  Analysis a = analyze_post("Like if you agree! RT to win a prize");
  US4_CHECK(a.context.engagement_bait);
  US4_CHECK(a.score.removed);
}

static void analyze_good_post_passes_checklist() {
  std::string text = "One counterintuitive lesson from 5 years shipping:\n";
  text += "What would you cut first?\n";
  text += "Follow for more field notes.\n";
  text += "\n- ship the skeleton first\n- measure before optimizing\n";
  while (text.size() < 1100) text += "Here is one more grounded, useful detail. ";
  Analysis a = analyze_post(text);
  US4_CHECK(!a.score.removed);
  US4_CHECK(a.score.final > 0.0);
  auto passed = [&](const std::string& id) {
    for (const auto& c : a.checklist)
      if (c.id == id) return c.pass;
    return false;
  };
  US4_CHECK(passed("reply_loop"));
  US4_CHECK(passed("follow_reason"));
  US4_CHECK(passed("share_worthy"));
  US4_CHECK(passed("length"));
  US4_CHECK(passed("no_bait"));
}

static void analyze_short_post_warns() {
  Analysis a = analyze_post("gm");
  US4_CHECK(!a.score.removed);
  US4_CHECK(!a.tips.empty());  // short post triggers guidance
}

int main() {
  US4_RUN(follow_is_highest_leverage);
  US4_RUN(video_gating_zeroes_short_clips);
  US4_RUN(hard_filter_removes_post);
  US4_RUN(oon_and_diversity_adjustments);
  US4_RUN(negative_post_sinks_to_floor);
  US4_RUN(analyze_flags_engagement_bait);
  US4_RUN(analyze_good_post_passes_checklist);
  US4_RUN(analyze_short_post_warns);
  US4_MAIN_END();
}
