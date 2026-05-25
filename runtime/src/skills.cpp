#include "us4/skills.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace us4::skills {

namespace {

std::string trim(const std::string& s) {
  std::size_t a = s.find_first_not_of(" \t\r\n");
  if (a == std::string::npos) return "";
  std::size_t b = s.find_last_not_of(" \t\r\n");
  return s.substr(a, b - a + 1);
}

std::string read_file(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return "";
  std::ostringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

}  // namespace

SkillDef parse_skill_md(const std::string& text, const std::string& fallback_name) {
  SkillDef def;
  def.name = fallback_name;

  std::istringstream in(text);
  std::string line;
  if (!std::getline(in, line) || trim(line) != "---") {
    return def;  // no frontmatter
  }
  while (std::getline(in, line)) {
    if (trim(line) == "---") break;
    std::size_t colon = line.find(':');
    if (colon == std::string::npos) continue;
    std::string key = trim(line.substr(0, colon));
    std::string value = trim(line.substr(colon + 1));
    if (key == "name") {
      def.name = value;
    } else if (key == "description") {
      def.description = value;
    } else if (key == "status") {
      def.status = value;
    } else if (key == "source") {
      def.source = value;
    }
  }
  def.always_on = (def.status == "always-on");
  return def;
}

std::size_t SkillRegistry::load_dir(const std::string& dir) {
  namespace fs = std::filesystem;
  std::error_code ec;
  if (!fs::is_directory(dir, ec)) return 0;

  std::vector<fs::path> entries;
  for (const auto& e : fs::directory_iterator(dir, ec)) {
    if (e.is_directory(ec)) entries.push_back(e.path());
  }
  std::sort(entries.begin(), entries.end());

  std::size_t loaded = 0;
  for (const auto& path : entries) {
    fs::path md = path / "SKILL.md";
    if (!fs::is_regular_file(md, ec)) continue;
    SkillDef def =
        parse_skill_md(read_file(md.string()), path.filename().string());
    if (def.name.empty()) def.name = path.filename().string();
    skills_.push_back(std::move(def));
    ++loaded;
  }
  return loaded;
}

const SkillDef* SkillRegistry::find(const std::string& name) const {
  for (const auto& s : skills_) {
    if (s.name == name) return &s;
  }
  return nullptr;
}

// --- conventional-commits --------------------------------------------------

CommitLint lint_conventional_commit(const std::string& message) {
  static constexpr std::array<const char*, 11> kTypes = {
      "feat", "fix",   "docs", "style",  "refactor", "perf",
      "test", "build", "ci",   "chore",  "revert"};

  CommitLint r;
  const std::string header =
      message.substr(0, message.find('\n'));  // first line

  // Breaking change can also be declared in a footer.
  if (message.find("BREAKING CHANGE:") != std::string::npos) r.breaking = true;

  const std::size_t colon = header.find(':');
  if (colon == std::string::npos) {
    r.errors.push_back("header must be 'type(scope)?: subject'");
    return r;
  }

  std::string left = trim(header.substr(0, colon));
  r.subject = trim(header.substr(colon + 1));

  // Trailing '!' on the type/scope marks a breaking change.
  if (!left.empty() && left.back() == '!') {
    r.breaking = true;
    left.pop_back();
    left = trim(left);
  }

  // Optional scope in parentheses: type(scope).
  std::size_t open = left.find('(');
  if (open != std::string::npos) {
    if (left.empty() || left.back() != ')') {
      r.errors.push_back("malformed scope: expected 'type(scope)'");
      r.type = trim(left.substr(0, open));
    } else {
      r.type = trim(left.substr(0, open));
      r.scope = left.substr(open + 1, left.size() - open - 2);
      if (trim(r.scope).empty()) r.errors.push_back("empty scope '()'");
    }
  } else {
    r.type = left;
  }

  const bool known =
      std::find_if(kTypes.begin(), kTypes.end(), [&](const char* t) {
        return r.type == t;
      }) != kTypes.end();
  if (!known) {
    r.errors.push_back("unknown type '" + r.type +
                       "' (use feat|fix|docs|style|refactor|perf|test|build|ci|"
                       "chore|revert)");
  }

  if (r.subject.empty()) {
    r.errors.push_back("empty subject");
  } else if (r.subject.back() == '.') {
    r.errors.push_back("subject must not end with a period");
  }
  if (header.size() > 72) {
    r.errors.push_back("header too long (" + std::to_string(header.size()) +
                       " > 72 chars)");
  }

  r.valid = r.errors.empty();
  return r;
}

}  // namespace us4::skills
