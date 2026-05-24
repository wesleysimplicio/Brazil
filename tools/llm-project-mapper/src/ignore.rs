//! Path-ignore rules: a built-in default set plus `.gitignore` patterns,
//! matched with a small `*`/`?` glob engine (no regex dependency).

use std::fs;
use std::path::Path;

const DEFAULT_DIRS: &[&str] = &[
    ".git",
    ".hg",
    ".svn",
    "node_modules",
    "bower_components",
    ".pnpm-store",
    "dist",
    "build",
    "out",
    "output",
    ".next",
    ".nuxt",
    ".svelte-kit",
    ".turbo",
    "target",
    "obj",
    "__pycache__",
    ".pytest_cache",
    ".mypy_cache",
    ".ruff_cache",
    ".venv",
    "venv",
    "env",
    ".tox",
    ".gradle",
    ".mvn",
    "vendor",
    "Pods",
    "coverage",
    ".cache",
    ".idea",
    ".vscode",
    ".terraform",
    "DerivedData",
];

const DEFAULT_FILES: &[&str] = &[
    ".DS_Store",
    "Thumbs.db",
    ".llm-project-mapper.json",
    "package-lock.json",
    "pnpm-lock.yaml",
    "yarn.lock",
    "bun.lockb",
    "Cargo.lock",
    "poetry.lock",
    "composer.lock",
];

/// Compiled ignore rules for one project root.
pub struct Ignore {
    patterns: Vec<String>,
}

impl Ignore {
    /// Build from the project's `.gitignore` (if present) plus extra patterns.
    pub fn new(root: &Path, extra: &[String]) -> Ignore {
        let mut patterns = Vec::new();
        for e in extra {
            patterns.push(clean_pattern(e));
        }
        if let Ok(text) = fs::read_to_string(root.join(".gitignore")) {
            for raw in text.lines() {
                let line = raw.trim();
                if line.is_empty() || line.starts_with('#') || line.starts_with('!') {
                    continue;
                }
                let cleaned = clean_pattern(line);
                if !cleaned.is_empty() {
                    patterns.push(cleaned);
                }
            }
        }
        patterns.retain(|p| !p.is_empty());
        Ignore { patterns }
    }

    /// Whether a path (relative to root) should be skipped.
    pub fn is_ignored(&self, rel_path: &str, name: &str, is_dir: bool) -> bool {
        if is_dir && DEFAULT_DIRS.contains(&name) {
            return true;
        }
        if !is_dir && DEFAULT_FILES.contains(&name) {
            return true;
        }
        let probe = if is_dir {
            format!("{}/", rel_path)
        } else {
            rel_path.to_string()
        };
        self.patterns.iter().any(|p| glob_anchored(p, &probe))
    }
}

fn clean_pattern(line: &str) -> String {
    line.trim().trim_matches('/').to_string()
}

/// Match `pattern` against `text` like the `.gitignore`-ish `(^|/)pat(/|$)`
/// rule: anchored at the start of a path segment, `*` does not cross `/`.
fn glob_anchored(pattern: &str, text: &str) -> bool {
    let pat: Vec<char> = pattern.chars().collect();
    let txt: Vec<char> = text.chars().collect();

    // Candidate anchors: index 0 and every position right after a '/'.
    let mut anchors = vec![0usize];
    for (i, &c) in txt.iter().enumerate() {
        if c == '/' {
            anchors.push(i + 1);
        }
    }
    for start in anchors {
        if let Some(end) = match_at(&pat, 0, &txt, start) {
            if end == txt.len() || txt[end] == '/' {
                return true;
            }
        }
    }
    false
}

/// Try to match pattern[pi..] against text starting at ti; return the text end
/// index on success. `*` matches a run of non-`/` characters (with backtracking).
fn match_at(pat: &[char], pi: usize, txt: &[char], ti: usize) -> Option<usize> {
    if pi == pat.len() {
        return Some(ti);
    }
    match pat[pi] {
        '*' => {
            let mut k = ti;
            loop {
                if let Some(end) = match_at(pat, pi + 1, txt, k) {
                    return Some(end);
                }
                if k < txt.len() && txt[k] != '/' {
                    k += 1;
                } else {
                    return None;
                }
            }
        }
        '?' => {
            if ti < txt.len() && txt[ti] != '/' {
                match_at(pat, pi + 1, txt, ti + 1)
            } else {
                None
            }
        }
        c => {
            if ti < txt.len() && txt[ti] == c {
                match_at(pat, pi + 1, txt, ti + 1)
            } else {
                None
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn default_dirs_and_files() {
        let ig = Ignore { patterns: vec![] };
        assert!(ig.is_ignored("node_modules", "node_modules", true));
        assert!(ig.is_ignored("build", "build", true));
        assert!(!ig.is_ignored("src", "src", true));
        assert!(ig.is_ignored(".DS_Store", ".DS_Store", false));
    }

    #[test]
    fn extension_glob() {
        let ig = Ignore {
            patterns: vec!["*.md".into()],
        };
        assert!(ig.is_ignored("README.md", "README.md", false));
        assert!(ig.is_ignored("docs/guide.md", "guide.md", false));
        assert!(!ig.is_ignored("src/main.rs", "main.rs", false));
    }

    #[test]
    fn directory_name_glob() {
        let ig = Ignore {
            patterns: vec!["CMakeFiles".into()],
        };
        assert!(ig.is_ignored("a/CMakeFiles", "CMakeFiles", true));
        assert!(ig.is_ignored("a/CMakeFiles/x.o", "x.o", false));
    }
}
