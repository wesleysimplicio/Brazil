#include "us4/agents.hpp"

#include <algorithm>
#include <chrono>
#include <sstream>
#include <thread>

#include "us4/hash.hpp"

namespace us4::agents {

namespace {

double now_s() {
  using namespace std::chrono;
  return duration<double>(steady_clock::now().time_since_epoch()).count();
}

std::string map_key(const std::vector<std::string>& map_pos) {
  std::string k;
  for (std::size_t i = 0; i < map_pos.size(); ++i) {
    if (i) k.push_back('/');
    k += map_pos[i];
  }
  return k;
}

template <typename... Parts>
std::string receipt_hash(Parts&&... parts) {
  std::ostringstream os;
  ((os << '|' << parts), ...);
  return short_hash(os.str());
}

}  // namespace

// ----------------------------- HAMT ---------------------------------------

int Hamt::slot_at(std::uint32_t h, int level) {
  const int shift = (kHamtMaxLevels - 1 - level) * kHamtBits;
  return static_cast<int>((h >> shift) & 0x1f);
}

bool Hamt::insert_into(std::shared_ptr<Node>& node, int level,
                       const std::string& key, std::uint64_t value,
                       std::uint32_t h) {
  const int slot = slot_at(h, level);
  auto it = node->children.find(slot);
  if (it == node->children.end()) {
    auto leaf = std::make_shared<Node>();
    leaf->kind = Node::Kind::Leaf;
    leaf->key = key;
    leaf->value = value;
    node->children[slot] = leaf;
    return true;
  }

  auto& child = it->second;
  if (child->kind == Node::Kind::Leaf) {
    if (child->key == key) {
      child->value = value;
      return false;
    }
    if (level + 1 < kHamtMaxLevels) {
      auto internal = std::make_shared<Node>();
      internal->kind = Node::Kind::Internal;
      insert_into(internal, level + 1, child->key, child->value,
                  hamt_hash30(child->key));
      bool added = insert_into(internal, level + 1, key, value, h);
      node->children[slot] = internal;
      return added;
    }
    auto coll = std::make_shared<Node>();
    coll->kind = Node::Kind::Collision;
    coll->leaves.push_back({child->key, child->value});
    coll->leaves.push_back({key, value});
    node->children[slot] = coll;
    return true;
  }

  if (child->kind == Node::Kind::Collision) {
    for (auto& leaf : child->leaves) {
      if (leaf.key == key) {
        leaf.value = value;
        return false;
      }
    }
    child->leaves.push_back({key, value});
    return true;
  }

  return insert_into(child, level + 1, key, value, h);
}

void Hamt::insert(const std::string& key, std::uint64_t value) {
  if (!root_) {
    root_ = std::make_shared<Node>();
    root_->kind = Node::Kind::Internal;
  }
  if (insert_into(root_, 0, key, value, hamt_hash30(key))) ++size_;
}

std::optional<std::uint64_t> Hamt::lookup(const std::string& key) const {
  if (!root_) return std::nullopt;
  const std::uint32_t h = hamt_hash30(key);
  Node* node = root_.get();
  for (int level = 0; level < kHamtMaxLevels; ++level) {
    const int slot = slot_at(h, level);
    auto it = node->children.find(slot);
    if (it == node->children.end()) return std::nullopt;
    Node* child = it->second.get();
    if (child->kind == Node::Kind::Leaf) {
      return child->key == key ? std::optional<std::uint64_t>(child->value)
                               : std::nullopt;
    }
    if (child->kind == Node::Kind::Collision) {
      for (const auto& leaf : child->leaves) {
        if (leaf.key == key) return leaf.value;
      }
      return std::nullopt;
    }
    node = child;
  }
  return std::nullopt;
}

// --------------------------- RuntimePolicy ---------------------------------

int RuntimePolicy::concurrency_for(int queued_roots,
                                   std::optional<double> ewma_latency_ms,
                                   double error_rate) const {
  int requested = lane_concurrency;
  if (requested <= 0) {
    unsigned hw = std::thread::hardware_concurrency();
    requested = std::min(std::max(1, queued_roots),
                         std::max(1, static_cast<int>(hw ? hw : 1)));
  }
  int ceiling = std::max(1, std::min(max_lane_concurrency, std::max(queued_roots, 1)));
  int concurrency = std::max(1, std::min(requested, ceiling));
  if (queued_roots > requested * 4) {
    concurrency = std::min(ceiling, std::max(concurrency, requested * 2));
  }
  if (ewma_latency_ms && *ewma_latency_ms > 250.0) {
    concurrency = std::min(ceiling, std::max(concurrency, concurrency * 2));
  }
  if (error_rate >= 0.2) {
    concurrency = std::max(1, concurrency / 2);
  }
  return std::max(1, concurrency);
}

// --------------------------- BackoffPolicy ---------------------------------

BackoffPolicy BackoffPolicy::from_runtime(const RuntimePolicy& p) {
  return BackoffPolicy{p.api_max_retries, p.api_backoff_base_ms,
                       p.api_backoff_max_ms};
}

double BackoffPolicy::base_delay_ms(int attempt) const {
  double capped = static_cast<double>(base_ms) * static_cast<double>(1u << attempt);
  return std::min(static_cast<double>(max_ms), capped);
}

// --------------------------- CircuitBreaker --------------------------------

CircuitBreaker::CircuitBreaker(int failure_threshold, double cooldown_s)
    : failure_threshold_(std::max(1, failure_threshold)),
      cooldown_s_(std::max(0.001, cooldown_s)) {}

void CircuitBreaker::before_call(const std::string& provider) {
  std::lock_guard<std::mutex> lock(mu_);
  auto& s = states_[provider];
  if (s.opened_until > now_s()) {
    throw CircuitOpenError("circuit open for provider '" + provider + "'");
  }
}

void CircuitBreaker::record_success(const std::string& provider) {
  std::lock_guard<std::mutex> lock(mu_);
  auto& s = states_[provider];
  s.failures = 0;
  s.opened_until = 0.0;
  ++s.successes;
}

void CircuitBreaker::record_failure(const std::string& provider) {
  std::lock_guard<std::mutex> lock(mu_);
  auto& s = states_[provider];
  ++s.failures;
  if (s.failures >= failure_threshold_) s.opened_until = now_s() + cooldown_s_;
}

bool CircuitBreaker::is_open(const std::string& provider) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = states_.find(provider);
  return it != states_.end() && it->second.opened_until > now_s();
}

// ---------------------------- ReceiptCache ---------------------------------

ReceiptCache::ReceiptCache(std::size_t max_entries, double ttl_s)
    : max_entries_(std::max<std::size_t>(1, max_entries)),
      ttl_s_(std::max(0.001, ttl_s)) {}

ReceiptCache::Hit ReceiptCache::get(const std::vector<std::string>& keys) {
  std::lock_guard<std::mutex> lock(mu_);
  const double now = now_s();
  for (const auto& key : keys) {
    auto it = index_.find(key);
    if (it == index_.end()) continue;
    if (now - it->second->created_at > ttl_s_) {
      order_.erase(it->second);
      index_.erase(it);
      continue;
    }
    Entry entry = *it->second;
    order_.erase(it->second);
    order_.push_back(entry);
    it->second = std::prev(order_.end());
    ++hits_;
    return Hit{true, entry.value, key};
  }
  return Hit{};
}

void ReceiptCache::set(const std::vector<std::string>& keys,
                       const std::string& value) {
  std::lock_guard<std::mutex> lock(mu_);
  const double now = now_s();
  for (const auto& key : keys) {
    auto it = index_.find(key);
    if (it != index_.end()) order_.erase(it->second);
    order_.push_back(Entry{key, value, now});
    index_[key] = std::prev(order_.end());
  }
  while (order_.size() > max_entries_) {
    index_.erase(order_.front().key);
    order_.pop_front();
  }
}

std::size_t ReceiptCache::size() const {
  std::lock_guard<std::mutex> lock(mu_);
  return order_.size();
}

// ------------------------------ Receipt ------------------------------------

std::string Receipt::compute_id() const {
  std::string raw = yool + "|" + std::to_string(exit_code) + "|" + stdout_sha +
                    "|" + stderr_sha;
  for (const auto& a : artifacts) raw += "|" + a;
  return "sha256:" + sha256_hex(raw);
}

// ------------------------------- Tuple -------------------------------------

std::string Tuple::canonical() const {
  std::ostringstream os;
  os << "yool=" << yool << ";map=" << map_key(map_pos)
     << ";authority=" << authority << ";lane=" << lane << ";source=" << source
     << ";parent=" << parent_id << ";data={";
  for (const auto& [k, v] : data) os << k << '=' << v << ',';  // std::map is sorted
  os << '}';
  return os.str();
}

void Tuple::compute_id() { id = "sha256:" + sha256_hex(canonical()); }

void Tuple::touch() { last_active = now_s(); }

// ----------------------------- ipow ----------------------------------------

std::uint64_t ipow_saturating(std::uint64_t base, std::uint32_t exp) {
  std::uint64_t result = 1;
  for (std::uint32_t i = 0; i < exp; ++i) {
    if (base != 0 && result > UINT64_MAX / base) return UINT64_MAX;  // saturate
    result *= base;
  }
  return result;
}

// ---------------------------- TupleSpace -----------------------------------

TupleSpace::TupleSpace(RuntimePolicy policy)
    : policy_(policy),
      cache_(static_cast<std::size_t>(policy.cache_max_entries), policy.cache_ttl_s),
      circuit_(policy.circuit_failure_threshold, policy.circuit_cooldown_s) {}

void TupleSpace::out_tuple(const TuplePtr& t) {
  std::lock_guard<std::recursive_mutex> lock(mu_);
  space_[map_key(t->map_pos)].push_back(t);
  lane_index_[t->lane].push_back(t);
  t->receipts.push_back(
      "out@" + receipt_hash(map_key(t->map_pos), t->yool, t->receipts.size()));
  t->touch();
}

bool TupleSpace::matches(const Tuple& t,
                         const std::map<std::string, std::string>& tmpl) {
  for (const auto& [key, expected] : tmpl) {
    std::string value;
    if (key == "yool") {
      value = t.yool;
    } else if (key == "lane") {
      value = t.lane;
    } else if (key == "authority") {
      value = t.authority;
    } else if (key == "source") {
      value = t.source;
    } else {
      auto it = t.data.find(key);
      value = it == t.data.end() ? std::string() : it->second;
    }
    if (value != expected) return false;
  }
  return true;
}

TuplePtr TupleSpace::find_match(
    const std::map<std::string, std::string>& tmpl) const {
  std::vector<TuplePtr> candidates;
  auto lane_it = tmpl.find("lane");
  if (lane_it != tmpl.end()) {
    auto it = lane_index_.find(lane_it->second);
    if (it != lane_index_.end()) candidates = it->second;
  } else {
    for (const auto& [_, items] : space_)
      candidates.insert(candidates.end(), items.begin(), items.end());
  }
  for (const auto& t : candidates) {
    if (matches(*t, tmpl)) return t;
  }
  return nullptr;
}

void TupleSpace::remove_tuple(const TuplePtr& t) {
  const std::string mk = map_key(t->map_pos);
  auto sit = space_.find(mk);
  if (sit != space_.end()) {
    auto& vec = sit->second;
    vec.erase(std::remove(vec.begin(), vec.end(), t), vec.end());
    if (vec.empty()) space_.erase(sit);
  }
  auto lit = lane_index_.find(t->lane);
  if (lit != lane_index_.end()) {
    auto& vec = lit->second;
    vec.erase(std::remove(vec.begin(), vec.end(), t), vec.end());
    if (vec.empty()) lane_index_.erase(lit);
  }
}

TuplePtr TupleSpace::in_tuple(
    const std::map<std::string, std::string>& tmpl) {
  std::lock_guard<std::recursive_mutex> lock(mu_);
  auto match = find_match(tmpl);
  if (match) {
    remove_tuple(match);
    match->touch();
  }
  return match;
}

TuplePtr TupleSpace::rd_tuple(
    const std::map<std::string, std::string>& tmpl) {
  std::lock_guard<std::recursive_mutex> lock(mu_);
  auto match = find_match(tmpl);
  if (match) match->touch();
  return match;
}

std::vector<TuplePtr> TupleSpace::scan_index(
    const std::optional<std::string>& lane,
    const std::optional<std::string>& yool, std::size_t limit) const {
  std::lock_guard<std::recursive_mutex> lock(mu_);
  std::vector<TuplePtr> candidates;
  if (lane) {
    auto it = lane_index_.find(*lane);
    if (it != lane_index_.end()) candidates = it->second;
  } else {
    for (const auto& [_, items] : space_)
      candidates.insert(candidates.end(), items.begin(), items.end());
  }
  std::vector<TuplePtr> out;
  for (const auto& t : candidates) {
    if (!yool || t->yool == *yool) out.push_back(t);
    if (out.size() >= limit) break;
  }
  return out;
}

void TupleSpace::register_local_yool(const std::string& yool, Executor exec) {
  std::lock_guard<std::recursive_mutex> lock(mu_);
  local_yools_[yool] = std::move(exec);
  registry_.insert(yool, registry_.size() + 1);
}

std::optional<std::uint64_t> TupleSpace::lookup_yool(
    const std::string& yool) const {
  std::lock_guard<std::recursive_mutex> lock(mu_);
  return registry_.lookup(yool);
}

std::int64_t TupleSpace::allocate_agent_id() { return next_agent_id_++; }

std::int64_t TupleSpace::spawn_agent(
    const Tuple& parent, const std::string& agent_yool,
    const std::map<std::string, std::string>& data) {
  std::lock_guard<std::recursive_mutex> lock(mu_);
  const std::int64_t agent_id = allocate_agent_id();
  auto t = std::make_shared<Tuple>();
  t->agent_id = agent_id;
  t->parent_id = parent.agent_id;
  t->yool = agent_yool;
  t->map_pos = parent.map_pos;
  t->map_pos.push_back(std::to_string(agent_id));
  t->authority = "subagent_" + std::to_string(agent_id);
  auto lane_it = data.find("lane");
  t->lane = lane_it != data.end() ? lane_it->second : parent.lane;
  t->source = "spawned_from_" + parent.authority;
  t->data = data;
  t->compute_id();
  agents_[agent_id] = t;
  out_tuple(t);
  prune_idle(policy_.compression_threshold);
  return agent_id;
}

BatchSpawnReceipt TupleSpace::batch_spawn(
    const Tuple& parent, const std::string& agent_yool, int depth, int branching,
    std::optional<int> compression_threshold,
    std::map<std::string, std::string> data) {
  if (depth < 1) throw std::invalid_argument("depth must be >= 1");
  if (branching < 1) throw std::invalid_argument("branching must be >= 1");
  const int threshold = compression_threshold.value_or(policy_.compression_threshold);
  const std::uint64_t virtual_agents =
      ipow_saturating(static_cast<std::uint64_t>(branching),
                      static_cast<std::uint32_t>(depth));

  data["lazy_batch"] = "true";
  data["depth"] = std::to_string(depth);
  data["branching"] = std::to_string(branching);
  data["virtual_agents"] = std::to_string(virtual_agents);
  data["compression_threshold"] = std::to_string(threshold);

  const std::int64_t controller_id = spawn_agent(parent, agent_yool, data);

  std::lock_guard<std::recursive_mutex> lock(mu_);
  virtual_agent_count_ += virtual_agents;
  const std::string receipt_id =
      receipt_hash(controller_id, depth, branching, agent_yool, virtual_agents);
  auto it = agents_.find(controller_id);
  if (it != agents_.end()) {
    it->second->receipts.push_back("batch_spawn@" + receipt_id);
    if (agents_.size() > static_cast<std::size_t>(threshold)) {
      compress_token(controller_id);
    }
  }
  return BatchSpawnReceipt{controller_id, depth, branching, virtual_agents,
                           threshold, receipt_id};
}

bool TupleSpace::compress_token(std::int64_t agent_id) {
  std::lock_guard<std::recursive_mutex> lock(mu_);
  auto it = agents_.find(agent_id);
  if (it == agents_.end()) return false;
  Tuple token = *it->second;
  std::string raw;
  for (const auto& [k, v] : token.data) raw += k + "=" + v + ";";
  token.data.clear();
  token.data["digest"] = sha256_hex(raw);
  remove_tuple(it->second);
  compressed_agents_[agent_id] = token;
  agents_.erase(it);
  return true;
}

std::size_t TupleSpace::prune_idle(std::optional<int> max_active) {
  std::lock_guard<std::recursive_mutex> lock(mu_);
  const std::size_t limit = static_cast<std::size_t>(
      std::max(1, max_active.value_or(policy_.compression_threshold)));
  if (agents_.size() <= limit) return 0;
  std::vector<std::pair<double, std::int64_t>> active;
  active.reserve(agents_.size());
  for (const auto& [id, t] : agents_) active.push_back({t->last_active, id});
  std::sort(active.begin(), active.end());
  const std::size_t to_compress = agents_.size() - limit;
  for (std::size_t i = 0; i < to_compress; ++i) compress_token(active[i].second);
  return to_compress;
}

std::vector<std::string> TupleSpace::cache_keys_for(const Tuple& t) const {
  std::string data_raw;
  for (const auto& [k, v] : t.data) data_raw += k + "=" + v + ";";
  const std::string input_key = sha256_hex("yool=" + t.yool + ";data=" + data_raw);
  std::string receipts_raw;
  for (const auto& r : t.receipts) receipts_raw += r + ";";
  const std::string receipt_key =
      sha256_hex("receipts=" + receipts_raw + ";input=" + input_key);
  return {"input:" + input_key, "receipt:" + receipt_key};
}

std::string TupleSpace::execute_tuple(const TuplePtr& t, const Executor& exec,
                                      const std::string& provider, bool use_cache) {
  Executor local;
  {
    std::lock_guard<std::recursive_mutex> lock(mu_);
    auto it = local_yools_.find(t->yool);
    if (it != local_yools_.end()) local = it->second;
  }
  if (local) {
    t->receipts.push_back("local_route@" + receipt_hash(t->yool, map_key(t->map_pos)));
    return local(*t);
  }

  const auto keys = cache_keys_for(*t);
  if (use_cache) {
    auto hit = cache_.get(keys);
    if (hit.hit) {
      t->receipts.push_back("cache_hit@" + hit.key);
      return hit.value;
    }
  }

  std::string result;
  if (provider == "local") {
    result = exec(*t);
  } else {
    BackoffPolicy backoff = BackoffPolicy::from_runtime(policy_);
    int attempt = 0;
    while (true) {
      circuit_.before_call(provider);
      try {
        result = exec(*t);
        circuit_.record_success(provider);
        break;
      } catch (const std::exception&) {
        circuit_.record_failure(provider);
        if (attempt >= backoff.max_retries) throw;
        ++attempt;
      }
    }
  }
  if (use_cache) {
    cache_.set(keys, result);
    t->receipts.push_back("cache_store@" + receipt_hash(provider, keys.size()));
  }
  return result;
}

bool TupleSpace::hookwall(const std::string& wall, const std::string& capability,
                          const std::string& action) {
  std::lock_guard<std::recursive_mutex> lock(mu_);
  auto& caps = walls_[wall];
  if (action == "hook") {
    if (std::find(caps.begin(), caps.end(), capability) == caps.end())
      caps.push_back(capability);
    return true;
  }
  if (action == "check") {
    return std::find(caps.begin(), caps.end(), capability) != caps.end();
  }
  if (action == "unhook") {
    auto it = std::find(caps.begin(), caps.end(), capability);
    if (it != caps.end()) {
      caps.erase(it);
      return true;
    }
  }
  return false;
}

SpaceSnapshot TupleSpace::snapshot() const {
  std::lock_guard<std::recursive_mutex> lock(mu_);
  SpaceSnapshot s;
  for (const auto& [_, items] : space_) s.tuples += items.size();
  for (const auto& [lane, items] : lane_index_) s.lanes[lane] = items.size();
  s.active_agents = agents_.size();
  s.compressed_agents = compressed_agents_.size();
  s.virtual_agents = virtual_agent_count_;
  s.total_agents = static_cast<std::uint64_t>(agents_.size()) +
                   compressed_agents_.size() + virtual_agent_count_;
  s.cache_entries = cache_.size();
  return s;
}

// --------------------------- LaneWorkerPool --------------------------------

std::vector<std::string> LaneWorkerPool::run_lane(const std::string& lane,
                                                  const Executor& exec) {
  auto pending = space_.scan_index(lane, std::nullopt, space_.policy().queue_maxsize);
  if (pending.empty()) return {};

  const int concurrency = space_.policy().concurrency_for(
      static_cast<int>(pending.size()), std::nullopt, 0.0);

  std::mutex out_mu;
  std::vector<std::string> results;
  auto worker = [&]() {
    while (true) {
      auto t = space_.in_tuple({{"lane", lane}});
      if (!t) return;
      std::string r = space_.execute_tuple(t, exec);
      std::lock_guard<std::mutex> lk(out_mu);
      results.push_back(std::move(r));
    }
  };

  std::vector<std::thread> threads;
  const int n = std::max(1, std::min(concurrency, static_cast<int>(pending.size())));
  threads.reserve(n);
  for (int i = 0; i < n; ++i) threads.emplace_back(worker);
  for (auto& th : threads) th.join();
  return results;
}

// --------------------------- default space ---------------------------------

std::pair<std::unique_ptr<TupleSpace>, TuplePtr> build_default_space() {
  auto ts = std::make_unique<TupleSpace>();
  auto root = std::make_shared<Tuple>();
  root->agent_id = 0;
  root->yool = "kernel_root";
  root->map_pos = {"0"};
  root->authority = "root";
  root->lane = "main";
  root->source = "user";
  root->compute_id();
  ts->out_tuple(root);
  return {std::move(ts), root};
}

}  // namespace us4::agents
