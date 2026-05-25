#pragma once

// Native port of the x-virality-skills knowledge: a source-grounded model of
// X's "For You" ranking. combined = Sum(phoenix_score[i] * weight[i]) over 22
// signals, then offset + out-of-network + author-diversity adjustments, with
// hard-limit filters that remove a post entirely.
//
// Weights are the documented illustrative defaults; the upstream skill stresses
// they are runtime-tunable, so ViralityWeights is fully configurable.

#include <string>
#include <vector>

namespace us4::virality {

// Predicted action probabilities (Phoenix-style), each in [0, 1].
struct Signals {
  // Positive engagement signals.
  double favorite = 0.0;
  double reply = 0.0;
  double retweet = 0.0;
  double photo_expand = 0.0;
  double click = 0.0;
  double profile_click = 0.0;
  double vqv = 0.0;  // video quality view
  double share = 0.0;
  double share_via_dm = 0.0;
  double share_via_copy_link = 0.0;
  double dwell = 0.0;
  double quote = 0.0;
  double quoted_click = 0.0;
  double quoted_vqv = 0.0;
  double cont_dwell_time = 0.0;
  double click_dwell_time = 0.0;
  double follow_author = 0.0;
  // Negative signals.
  double not_interested = 0.0;
  double block_author = 0.0;
  double mute_author = 0.0;
  double report = 0.0;
  double not_dwelled = 0.0;
  // Gating inputs.
  long video_duration_ms = 0;
  long quoted_video_duration_ms = 0;
};

struct Weights {
  // Positive (documented illustrative defaults).
  double favorite = 0.5;
  double reply = 13.5;
  double retweet = 1.0;
  double photo_expand = 0.1;
  double click = 0.1;
  double profile_click = 12.0;
  double vqv = 0.005;
  double share = 1.0;
  double share_via_dm = 1.0;
  double share_via_copy_link = 1.0;
  double dwell = 0.5;
  double quote = 1.0;
  double quoted_click = 0.1;
  double quoted_vqv = 0.005;
  double cont_dwell_time = 0.0001;
  double click_dwell_time = 0.0001;
  double follow_author = 24.0;
  // Negative.
  double not_interested = -8.0;
  double block_author = -80.0;
  double mute_author = -40.0;
  double report = -100.0;
  double not_dwelled = -0.5;
  // Gating + adjustments.
  long min_video_duration_ms = 5000;
  double oon_factor = 0.5;
  double author_diversity_decay = 0.7;
  double author_diversity_floor = 0.3;
  double negative_scores_offset = 0.0;
};

struct Context {
  bool in_network = true;
  int author_position = 0;  // nth post by this author in the feed
  // Hard-limit filters: any true removes the post from ranking.
  bool violence = false;
  bool nsfw = false;
  bool policy_violation = false;
  bool muted_keyword = false;
  bool blocked = false;
  bool muted = false;
  bool duplicate_impression = false;
  bool engagement_bait = false;
};

struct Contribution {
  std::string name;
  double prob;
  double weight;
  double value;  // prob * weight (effective)
};

struct Score {
  bool removed = false;
  std::string removal_reason;
  double combined = 0.0;             // raw weighted sum
  double offset_base = 0.0;          // after negative-score offset
  double oon_multiplier = 1.0;
  double diversity_multiplier = 1.0;
  double final = 0.0;                // ranking score
  std::vector<Contribution> contributions;  // sorted by |value| desc
};

// Compute the For You ranking score for given signals/weights/context.
Score score_post(const Signals& s, const Weights& w, const Context& ctx);

// --- Text heuristic ("our LLM knows how to optimize a post") --------------

struct ChecklistItem {
  std::string id;
  bool pass;
  std::string note;
};

struct Analysis {
  Signals estimated;
  Context context;
  Score score;
  std::vector<ChecklistItem> checklist;
  std::vector<std::string> tips;
};

// Estimate action probabilities from post text (a documented proxy, not a real
// Phoenix model), score it, and produce an actionable checklist + tips.
Analysis analyze_post(const std::string& text, const Weights& w = Weights{});

}  // namespace us4::virality
