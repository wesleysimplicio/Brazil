#pragma once

// Native skills subsystem: load the .claude/skills/*/SKILL.md catalog, parse its
// frontmatter, and expose skills as addressable capabilities. Skills whose logic
// maps to a runtime capability get a real native implementation (e.g.
// conventional-commits); the rest remain catalogued, addressable agent guidance.

#include <string>
#include <vector>

namespace us4::skills {

struct SkillDef {
  std::string name;
  std::string description;
  std::string status;  // e.g. "always-on", empty otherwise
  std::string source;  // optional upstream URL
  bool always_on = false;
};

// Parse a SKILL.md (YAML-ish frontmatter between leading '---' fences).
SkillDef parse_skill_md(const std::string& text, const std::string& fallback_name = "");

class SkillRegistry {
 public:
  // Scan <dir>/<name>/SKILL.md and load each skill. Returns count loaded.
  std::size_t load_dir(const std::string& dir);
  const std::vector<SkillDef>& all() const { return skills_; }
  const SkillDef* find(const std::string& name) const;
  std::size_t size() const { return skills_.size(); }

 private:
  std::vector<SkillDef> skills_;
};

// --- Native implementation: conventional-commits ---------------------------

struct CommitLint {
  bool valid = false;
  std::string type;
  std::string scope;
  bool breaking = false;
  std::string subject;
  std::vector<std::string> errors;
};

// Validate a commit message against the Conventional Commits spec.
CommitLint lint_conventional_commit(const std::string& message);

}  // namespace us4::skills
