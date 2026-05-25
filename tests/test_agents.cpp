#include "us4/agents.hpp"

#include <string>

#include "us4_test.hpp"

using namespace us4::agents;

static void hamt_insert_lookup() {
  Hamt h;
  h.insert("agent.dev.python", 1);
  h.insert("agent.test.dotnet", 2);
  h.insert("fs.read_sector", 3);
  US4_CHECK(h.size() == 3);
  US4_CHECK(h.lookup("agent.dev.python").value_or(0) == 1);
  US4_CHECK(h.lookup("agent.test.dotnet").value_or(0) == 2);
  US4_CHECK(h.lookup("fs.read_sector").value_or(0) == 3);
  US4_CHECK(!h.lookup("missing.key").has_value());
  // Update existing key keeps size stable.
  h.insert("agent.dev.python", 99);
  US4_CHECK(h.size() == 3);
  US4_CHECK(h.lookup("agent.dev.python").value_or(0) == 99);
}

static void hamt_many_keys() {
  Hamt h;
  for (int i = 0; i < 2000; ++i) h.insert("yool." + std::to_string(i), i);
  US4_CHECK(h.size() == 2000);
  for (int i = 0; i < 2000; ++i)
    US4_CHECK(h.lookup("yool." + std::to_string(i)).value_or(UINT64_MAX) ==
              static_cast<std::uint64_t>(i));
}

static void tuple_content_addressing() {
  Tuple a;
  a.yool = "agent.dev.python";
  a.lane = "build";
  a.authority = "root";
  a.data["x"] = "1";
  a.compute_id();

  Tuple b = a;  // identical work
  b.compute_id();
  US4_CHECK(a.id == b.id);  // content-addressable: same input -> same id
  US4_CHECK(a.id.rfind("sha256:", 0) == 0);

  b.data["x"] = "2";  // different input
  b.compute_id();
  US4_CHECK(a.id != b.id);
}

static void ipow_saturates() {
  US4_CHECK(ipow_saturating(32, 4) == 1048576u);
  US4_CHECK(ipow_saturating(2, 10) == 1024u);
  US4_CHECK(ipow_saturating(1000000000ull, 4) == UINT64_MAX);  // saturates
}

static void linda_primitives() {
  auto [space, root] = build_default_space();
  std::int64_t id = space->spawn_agent(*root, "agent.dev.python",
                                       {{"lane", "build"}, {"k", "v"}});
  US4_CHECK(id >= 0);

  auto read = space->rd_tuple({{"lane", "build"}});
  US4_CHECK(read != nullptr);  // non-destructive
  auto read2 = space->rd_tuple({{"lane", "build"}});
  US4_CHECK(read2 != nullptr);

  auto taken = space->in_tuple({{"lane", "build"}});
  US4_CHECK(taken != nullptr);  // destructive
  auto empty = space->in_tuple({{"lane", "build"}});
  US4_CHECK(empty == nullptr);  // removed
}

static void batch_spawn_virtual_counts() {
  auto [space, root] = build_default_space();
  BatchSpawnReceipt br =
      space->batch_spawn(*root, "agent.dev.python", 4, 32, std::nullopt);
  US4_CHECK(br.virtual_agents == 1048576u);
  US4_CHECK(!br.receipt_id.empty());

  SpaceSnapshot s = space->snapshot();
  US4_CHECK(s.virtual_agents == 1048576u);
  // 1M+ agents represented, but only the controller is actually active.
  US4_CHECK(s.active_agents <= 1);
  US4_CHECK(s.total_agents >= 1048576u);
}

static void prune_compresses_idle_agents() {
  RuntimePolicy p;
  p.compression_threshold = 4;
  TupleSpace space(p);
  Tuple root;
  root.yool = "kernel_root";
  root.map_pos = {"0"};
  root.authority = "root";
  root.lane = "main";
  space.out_tuple(std::make_shared<Tuple>(root));
  for (int i = 0; i < 10; ++i)
    space.spawn_agent(root, "agent.work", {{"lane", "w"}});
  SpaceSnapshot s = space.snapshot();
  US4_CHECK(s.active_agents <= 4);     // pruned to threshold
  US4_CHECK(s.compressed_agents >= 6);  // rest compressed to tokens
}

static void receipt_cache_hit() {
  ReceiptCache cache(8, 60.0);
  auto miss = cache.get({"k1"});
  US4_CHECK(!miss.hit);
  cache.set({"k1"}, "value-1");
  auto hit = cache.get({"k1"});
  US4_CHECK(hit.hit);
  US4_CHECK(hit.value == "value-1");
}

static void circuit_breaker_opens() {
  CircuitBreaker cb(2, 30.0);
  cb.before_call("p");  // ok
  cb.record_failure("p");
  cb.record_failure("p");  // threshold reached -> open
  US4_CHECK(cb.is_open("p"));
  bool threw = false;
  try {
    cb.before_call("p");
  } catch (const CircuitOpenError&) {
    threw = true;
  }
  US4_CHECK(threw);
  cb.record_success("p");  // closes
  US4_CHECK(!cb.is_open("p"));
}

static void backoff_caps_and_grows() {
  RuntimePolicy p;
  p.api_backoff_base_ms = 50;
  p.api_backoff_max_ms = 400;
  BackoffPolicy b = BackoffPolicy::from_runtime(p);
  US4_CHECK(b.base_delay_ms(0) == 50.0);
  US4_CHECK(b.base_delay_ms(1) == 100.0);
  US4_CHECK(b.base_delay_ms(2) == 200.0);
  US4_CHECK(b.base_delay_ms(10) == 400.0);  // capped
}

static void local_yool_routing() {
  auto [space, root] = build_default_space();
  space->register_local_yool("echo",
                             [](Tuple& t) { return "echo:" + t.data["msg"]; });
  US4_CHECK(space->lookup_yool("echo").has_value());

  auto t = std::make_shared<Tuple>();
  t->yool = "echo";
  t->lane = "x";
  t->data["msg"] = "hi";
  std::string out = space->execute_tuple(
      t, [](Tuple&) { return std::string("should-not-run"); });
  US4_CHECK(out == "echo:hi");
}

static void lane_worker_pool_drains() {
  auto [space, root] = build_default_space();
  space->register_local_yool("work", [](Tuple& t) { return "done:" + t.data["i"]; });
  for (int i = 0; i < 8; ++i)
    space->spawn_agent(*root, "work",
                       {{"lane", "L"}, {"i", std::to_string(i)}});
  LaneWorkerPool pool(*space);
  auto results = pool.run_lane("L", [](Tuple&) { return std::string("x"); });
  US4_CHECK(results.size() == 8);
  // Lane fully drained.
  US4_CHECK(space->in_tuple({{"lane", "L"}}) == nullptr);
}

int main() {
  US4_RUN(hamt_insert_lookup);
  US4_RUN(hamt_many_keys);
  US4_RUN(tuple_content_addressing);
  US4_RUN(ipow_saturates);
  US4_RUN(linda_primitives);
  US4_RUN(batch_spawn_virtual_counts);
  US4_RUN(prune_compresses_idle_agents);
  US4_RUN(receipt_cache_hit);
  US4_RUN(circuit_breaker_opens);
  US4_RUN(backoff_caps_and_grows);
  US4_RUN(local_yool_routing);
  US4_RUN(lane_worker_pool_drains);
  US4_MAIN_END();
}
