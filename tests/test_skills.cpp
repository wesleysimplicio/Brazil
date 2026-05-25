#include "us4/skills.hpp"

#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "us4_test.hpp"

using namespace us4::skills;

static void parse_frontmatter() {
  std::string md =
      "---\n"
      "name: conventional-commits\n"
      "description: padronizar mensagens de commit\n"
      "status: always-on\n"
      "source: https://example.com\n"
      "---\n\n# body\n";
  SkillDef d = parse_skill_md(md, "fallback");
  US4_CHECK(d.name == "conventional-commits");
  US4_CHECK(d.description == "padronizar mensagens de commit");
  US4_CHECK(d.status == "always-on");
  US4_CHECK(d.always_on);
  US4_CHECK(d.source == "https://example.com");
}

static void parse_fallback_name_when_missing() {
  SkillDef d = parse_skill_md("# no frontmatter here\n", "myskill");
  US4_CHECK(d.name == "myskill");
  US4_CHECK(!d.always_on);
}

static void registry_loads_dir() {
  namespace fs = std::filesystem;
  fs::path root = fs::temp_directory_path() /
                  ("lpm-skills-" + std::to_string(::getpid()));
  fs::create_directories(root / "alpha");
  fs::create_directories(root / "beta");
  {
    std::ofstream(root / "alpha" / "SKILL.md")
        << "---\nname: alpha\ndescription: first\n---\n";
    std::ofstream(root / "beta" / "SKILL.md")
        << "---\nname: beta\ndescription: second\nstatus: always-on\n---\n";
  }
  SkillRegistry reg;
  std::size_t n = reg.load_dir(root.string());
  US4_CHECK(n == 2);
  US4_CHECK(reg.find("alpha") != nullptr);
  US4_CHECK(reg.find("beta") != nullptr);
  US4_CHECK(reg.find("beta")->always_on);
  US4_CHECK(reg.find("missing") == nullptr);
  std::filesystem::remove_all(root);
}

static void commit_lint_valid_cases() {
  auto a = lint_conventional_commit("feat: add probe command");
  US4_CHECK(a.valid);
  US4_CHECK(a.type == "feat");
  US4_CHECK(!a.breaking);

  auto b = lint_conventional_commit("fix(runtime): handle null backend");
  US4_CHECK(b.valid);
  US4_CHECK(b.type == "fix");
  US4_CHECK(b.scope == "runtime");

  auto c = lint_conventional_commit("feat!: drop legacy API");
  US4_CHECK(c.valid);
  US4_CHECK(c.breaking);

  auto d = lint_conventional_commit("refactor(core)!: rework");
  US4_CHECK(d.valid);
  US4_CHECK(d.scope == "core");
  US4_CHECK(d.breaking);
}

static void commit_lint_invalid_cases() {
  US4_CHECK(!lint_conventional_commit("frobnicate: x").valid);  // unknown type
  US4_CHECK(!lint_conventional_commit("no colon at all").valid);
  US4_CHECK(!lint_conventional_commit("fix: ").valid);          // empty subject
  US4_CHECK(!lint_conventional_commit("fix: trailing period.").valid);
}

static void commit_lint_breaking_footer() {
  auto r = lint_conventional_commit("feat: x\n\nBREAKING CHANGE: drops y");
  US4_CHECK(r.valid);
  US4_CHECK(r.breaking);
}

int main() {
  US4_RUN(parse_frontmatter);
  US4_RUN(parse_fallback_name_when_missing);
  US4_RUN(registry_loads_dir);
  US4_RUN(commit_lint_valid_cases);
  US4_RUN(commit_lint_invalid_cases);
  US4_RUN(commit_lint_breaking_footer);
  US4_MAIN_END();
}
