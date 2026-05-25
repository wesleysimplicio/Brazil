#pragma once

// Native port of the simplicio-prompt orchestration kernel:
// YOOL (atomic capabilities) -> Tuples (content-addressed work envelopes)
// -> HAMT registry -> Linda tuple-space with lazy hierarchical batch_spawn,
// receipt caching, circuit breaker, backoff, and bounded lane concurrency.

#include <cstdint>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace us4::agents {

// ---------------------------------------------------------------------------
// HAMT registry: 32-way trie, 5 bits/level, 6 levels, 30-bit address space.
// ---------------------------------------------------------------------------

inline constexpr int kHamtBits = 5;
inline constexpr int kHamtBranching = 32;
inline constexpr int kHamtMaxLevels = 6;

class Hamt {
 public:
  void insert(const std::string& key, std::uint64_t value);
  std::optional<std::uint64_t> lookup(const std::string& key) const;
  std::size_t size() const { return size_; }

 private:
  struct Leaf {
    std::string key;
    std::uint64_t value;
  };
  struct Node {
    enum class Kind { Internal, Leaf, Collision } kind = Kind::Internal;
    std::map<int, std::shared_ptr<Node>> children;  // Internal
    std::string key;                                 // Leaf
    std::uint64_t value = 0;                         // Leaf
    std::vector<Leaf> leaves;                        // Collision
  };

  static int slot_at(std::uint32_t h, int level);
  bool insert_into(std::shared_ptr<Node>& node, int level, const std::string& key,
                   std::uint64_t value, std::uint32_t h);

  std::shared_ptr<Node> root_;
  std::size_t size_ = 0;
};

// ---------------------------------------------------------------------------
// Policy + resilience
// ---------------------------------------------------------------------------

struct RuntimePolicy {
  int lane_concurrency = 32;
  int max_lane_concurrency = 64;
  int cpu_quota_pct = 100;
  int queue_maxsize = 1024;
  int compression_threshold = 1024;
  int cache_max_entries = 4096;
  double cache_ttl_s = 300.0;
  int api_max_retries = 3;
  int api_backoff_base_ms = 50;
  int api_backoff_max_ms = 5000;
  int circuit_failure_threshold = 5;
  double circuit_cooldown_s = 30.0;
  int batch_small_task_size = 16;
  int context_compression_chars = 8000;

  // Dynamic lane concurrency from queue depth and health signals.
  int concurrency_for(int queued_roots, std::optional<double> ewma_latency_ms,
                      double error_rate) const;
};

struct BackoffPolicy {
  int max_retries;
  int base_ms;
  int max_ms;
  double jitter_ratio = 0.25;

  static BackoffPolicy from_runtime(const RuntimePolicy& p);
  // Deterministic capped delay (no jitter), in milliseconds.
  double base_delay_ms(int attempt) const;
};

class CircuitOpenError : public std::runtime_error {
 public:
  explicit CircuitOpenError(const std::string& m) : std::runtime_error(m) {}
};

class CircuitBreaker {
 public:
  CircuitBreaker(int failure_threshold, double cooldown_s);
  void before_call(const std::string& provider);  // throws CircuitOpenError
  void record_success(const std::string& provider);
  void record_failure(const std::string& provider);
  bool is_open(const std::string& provider) const;

 private:
  struct State {
    int failures = 0;
    int successes = 0;
    double opened_until = 0.0;  // steady seconds
  };
  int failure_threshold_;
  double cooldown_s_;
  mutable std::mutex mu_;
  std::map<std::string, State> states_;
};

// Small LRU + TTL cache keyed by content-addressable receipt/input digests.
class ReceiptCache {
 public:
  ReceiptCache(std::size_t max_entries, double ttl_s);
  struct Hit {
    bool hit = false;
    std::string value;
    std::string key;
  };
  Hit get(const std::vector<std::string>& keys);
  void set(const std::vector<std::string>& keys, const std::string& value);
  std::size_t size() const;
  std::uint64_t hits() const { return hits_; }

 private:
  struct Entry {
    std::string key;
    std::string value;
    double created_at;
  };
  std::size_t max_entries_;
  double ttl_s_;
  mutable std::mutex mu_;
  std::list<Entry> order_;  // front = LRU, back = MRU
  std::map<std::string, std::list<Entry>::iterator> index_;
  std::uint64_t hits_ = 0;
};

// ---------------------------------------------------------------------------
// YOOL / Tuple / Receipt
// ---------------------------------------------------------------------------

struct Receipt {
  std::string id;
  std::string yool;
  int exit_code = 0;
  std::string stdout_sha;
  std::string stderr_sha;
  std::vector<std::string> artifacts;
  std::string compute_id() const;
};

struct Tuple {
  std::int64_t agent_id = -1;
  std::int64_t parent_id = -1;
  std::string id;  // content address ("sha256:...")
  std::string yool;
  std::vector<std::string> map_pos;
  std::string authority;
  std::string lane;
  std::string source;
  std::vector<std::string> receipts;  // append-only
  std::map<std::string, std::string> data;
  double last_active = 0.0;

  std::string canonical() const;  // deterministic, excludes id/receipts
  void compute_id();
  void touch();
};

using TuplePtr = std::shared_ptr<Tuple>;
using Executor = std::function<std::string(Tuple&)>;

struct BatchSpawnReceipt {
  std::int64_t root_agent_id;
  int depth;
  int branching;
  std::uint64_t virtual_agents;
  int compression_threshold;
  std::string receipt_id;
};

struct SpaceSnapshot {
  std::size_t tuples = 0;
  std::map<std::string, std::size_t> lanes;
  std::size_t active_agents = 0;
  std::size_t compressed_agents = 0;
  std::uint64_t virtual_agents = 0;
  std::uint64_t total_agents = 0;
  std::size_t cache_entries = 0;
};

// Saturating integer power (branching ** depth), clamped to UINT64_MAX.
std::uint64_t ipow_saturating(std::uint64_t base, std::uint32_t exp);

// ---------------------------------------------------------------------------
// TupleSpace: Linda-style coordination with lazy hierarchical agents.
// ---------------------------------------------------------------------------

class TupleSpace {
 public:
  explicit TupleSpace(RuntimePolicy policy = RuntimePolicy{});

  // Linda primitives.
  void out_tuple(const TuplePtr& t);
  TuplePtr in_tuple(const std::map<std::string, std::string>& tmpl);  // destructive
  TuplePtr rd_tuple(const std::map<std::string, std::string>& tmpl);  // non-destructive
  std::vector<TuplePtr> scan_index(const std::optional<std::string>& lane,
                                   const std::optional<std::string>& yool,
                                   std::size_t limit) const;

  // Capability registry + local routing.
  void register_local_yool(const std::string& yool, Executor exec);
  std::optional<std::uint64_t> lookup_yool(const std::string& yool) const;

  // Hierarchical spawning.
  std::int64_t spawn_agent(const Tuple& parent, const std::string& agent_yool,
                           const std::map<std::string, std::string>& data);
  BatchSpawnReceipt batch_spawn(const Tuple& parent, const std::string& agent_yool,
                                int depth, int branching,
                                std::optional<int> compression_threshold,
                                std::map<std::string, std::string> data = {});

  // Compression / pruning of inactive agents.
  bool compress_token(std::int64_t agent_id);
  std::size_t prune_idle(std::optional<int> max_active);

  // Execution with local routing, receipt cache and resilient provider calls.
  std::string execute_tuple(const TuplePtr& t, const Executor& exec,
                            const std::string& provider = "local",
                            bool use_cache = true);

  // Capability walls (hook/check/unhook).
  bool hookwall(const std::string& wall, const std::string& capability,
                const std::string& action);

  SpaceSnapshot snapshot() const;
  const RuntimePolicy& policy() const { return policy_; }
  CircuitBreaker& circuit_breaker() { return circuit_; }

  std::vector<std::string> cache_keys_for(const Tuple& t) const;

 private:
  std::int64_t allocate_agent_id();
  TuplePtr find_match(const std::map<std::string, std::string>& tmpl) const;
  static bool matches(const Tuple& t, const std::map<std::string, std::string>& tmpl);
  void remove_tuple(const TuplePtr& t);

  RuntimePolicy policy_;
  mutable std::recursive_mutex mu_;
  std::map<std::string, std::vector<TuplePtr>> space_;       // map key -> tuples
  std::map<std::string, std::vector<TuplePtr>> lane_index_;  // lane -> tuples
  std::map<std::int64_t, TuplePtr> agents_;
  std::map<std::int64_t, Tuple> compressed_agents_;          // snapshot tokens
  std::map<std::string, std::vector<std::string>> walls_;
  std::map<std::string, Executor> local_yools_;
  Hamt registry_;
  ReceiptCache cache_;
  CircuitBreaker circuit_;
  std::uint64_t virtual_agent_count_ = 0;
  std::int64_t next_agent_id_ = 0;
};

// Bounded per-lane worker fan-out.
class LaneWorkerPool {
 public:
  explicit LaneWorkerPool(TupleSpace& space) : space_(space) {}
  // Drains the lane, executing each tuple via `exec` across bounded threads.
  std::vector<std::string> run_lane(const std::string& lane, const Executor& exec);

 private:
  TupleSpace& space_;
};

// Builds a default space seeded with a kernel-root tuple.
std::pair<std::unique_ptr<TupleSpace>, TuplePtr> build_default_space();

}  // namespace us4::agents
