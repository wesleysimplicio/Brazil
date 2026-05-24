//! Orchestrates detection + walk + parallel line counting into a ProjectMap.

use std::collections::BTreeMap;
use std::path::{Path, PathBuf};
use std::thread;

use crate::datetime::now_iso8601;
use crate::ignore::Ignore;
use crate::stacks::{detect_stacks, Meta};
use crate::types::{
    LanguageStat, ProjectMap, ProjectMetrics, TreeNode, SCHEMA_VERSION,
};
use crate::walk::{count_lines, walk};

#[derive(Debug, Clone)]
pub struct Options {
    pub max_depth: u64,
    pub max_file_bytes: u64,
    pub extra_ignores: Vec<String>,
}

impl Default for Options {
    fn default() -> Self {
        Options {
            max_depth: 8,
            max_file_bytes: 2_000_000,
            extra_ignores: Vec::new(),
        }
    }
}

/// Analyze a project directory and produce a structured map.
pub fn build_project_map(root_input: &Path, opts: &Options) -> ProjectMap {
    let root = std::fs::canonicalize(root_input).unwrap_or_else(|_| root_input.to_path_buf());
    let ignore = Ignore::new(&root, &opts.extra_ignores);
    let detection = detect_stacks(&root);
    let mut walked = walk(&root, &ignore, opts.max_depth);

    let line_results = count_lines_parallel(&walked, opts.max_file_bytes);
    apply_line_counts(&mut walked.files, &mut walked.tree, &line_results);
    let total_lines: i64 = line_results.iter().filter_map(|x| *x).sum();

    let languages = aggregate_languages(&walked.files);
    let entry_points = detect_entry_points(&walked.files, &detection.meta);

    ProjectMap {
        schema: SCHEMA_VERSION.to_string(),
        generated_at: now_iso8601(),
        root: root.to_string_lossy().to_string(),
        name: detection
            .meta
            .name
            .clone()
            .unwrap_or_else(|| basename(&root)),
        description: detection.meta.description.clone(),
        stacks: detection.stacks,
        languages,
        dependencies: detection.dependencies,
        entry_points,
        metrics: ProjectMetrics {
            total_files: walked.total_files,
            total_dirs: walked.total_dirs,
            total_lines,
            total_bytes: walked.total_bytes,
            max_depth: walked.max_depth,
        },
        structure: walked.tree,
    }
}

/// Count lines for every in-budget file across all CPU cores.
fn count_lines_parallel(
    walked: &crate::walk::WalkResult,
    max_file_bytes: u64,
) -> Vec<Option<i64>> {
    let mut results = vec![None; walked.files.len()];
    let work: Vec<(usize, PathBuf)> = walked
        .files
        .iter()
        .filter(|f| f.bytes <= max_file_bytes)
        .map(|f| (f.file_id, f.abs_path.clone()))
        .collect();
    if work.is_empty() {
        return results;
    }

    let threads = thread::available_parallelism()
        .map(|n| n.get())
        .unwrap_or(1)
        .min(work.len());
    let chunk = work.len().div_ceil(threads);

    thread::scope(|scope| {
        let mut handles = Vec::new();
        for slice in work.chunks(chunk) {
            handles.push(scope.spawn(move || {
                slice
                    .iter()
                    .map(|(id, path)| (*id, count_lines(path)))
                    .collect::<Vec<(usize, Option<i64>)>>()
            }));
        }
        for handle in handles {
            for (id, lines) in handle.join().unwrap() {
                results[id] = lines;
            }
        }
    });
    results
}

fn apply_line_counts(
    files: &mut [crate::types::FileEntry],
    tree: &mut TreeNode,
    results: &[Option<i64>],
) {
    for f in files.iter_mut() {
        f.lines = results.get(f.file_id).copied().flatten();
    }
    fill_tree(tree, results);
}

fn fill_tree(node: &mut TreeNode, results: &[Option<i64>]) {
    if node.is_file {
        if let Some(id) = node.file_id {
            node.lines = results.get(id).copied().flatten();
        }
    } else {
        for child in node.children.iter_mut() {
            fill_tree(child, results);
        }
    }
}

fn aggregate_languages(files: &[crate::types::FileEntry]) -> Vec<LanguageStat> {
    let mut by_lang: BTreeMap<String, LanguageStat> = BTreeMap::new();
    for f in files {
        let stat = by_lang.entry(f.language.clone()).or_insert(LanguageStat {
            language: f.language.clone(),
            files: 0,
            lines: 0,
            bytes: 0,
        });
        stat.files += 1;
        stat.bytes += f.bytes;
        stat.lines += f.lines.unwrap_or(0);
    }
    let mut out: Vec<LanguageStat> = by_lang.into_values().collect();
    out.sort_by(|a, b| {
        b.lines
            .cmp(&a.lines)
            .then(b.bytes.cmp(&a.bytes))
            .then(a.language.cmp(&b.language))
    });
    out
}

fn detect_entry_points(files: &[crate::types::FileEntry], meta: &Meta) -> Vec<String> {
    let mut set: std::collections::BTreeSet<String> = std::collections::BTreeSet::new();
    let norm = |p: &str| p.trim_start_matches("./").to_string();
    if let Some(m) = &meta.main {
        set.insert(norm(m));
    }
    for b in &meta.bins {
        set.insert(norm(b));
    }
    for f in files {
        if is_entry_point(&f.path) {
            set.insert(f.path.clone());
        }
    }
    set.into_iter().take(25).collect()
}

/// Recognise conventional entry-point paths without a regex engine.
fn is_entry_point(path: &str) -> bool {
    let base = path.rsplit('/').next().unwrap_or(path);
    let top_or_src = path == base || path == format!("src/{}", base);

    // (src/)?index.{js,mjs,cjs,ts,tsx}
    if top_or_src
        && matches!(
            base,
            "index.js" | "index.mjs" | "index.cjs" | "index.ts" | "index.tsx"
        )
    {
        return true;
    }
    // (src/)?main.{ts,js,py,go,rs,cpp,cc}
    if top_or_src
        && matches!(
            base,
            "main.ts" | "main.js" | "main.py" | "main.go" | "main.rs" | "main.cpp" | "main.cc"
        )
    {
        return true;
    }
    // main.{go,rs,cpp,cc} anywhere
    if matches!(base, "main.go" | "main.rs" | "main.cpp" | "main.cc") {
        return true;
    }
    // python conventional entry files
    if matches!(base, "app.py" | "manage.py" | "wsgi.py" | "asgi.py" | "__main__.py") {
        return true;
    }
    // bin/*.{js,mjs,cjs}
    if path.starts_with("bin/")
        && (path.ends_with(".js") || path.ends_with(".mjs") || path.ends_with(".cjs"))
    {
        return true;
    }
    false
}

fn basename(path: &Path) -> String {
    path.file_name()
        .map(|s| s.to_string_lossy().to_string())
        .unwrap_or_else(|| path.to_string_lossy().to_string())
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;
    use std::io::Write;

    fn fixture() -> PathBuf {
        let mut p = std::env::temp_dir();
        p.push(format!(
            "lpm-map-{}-{}",
            std::process::id(),
            std::time::SystemTime::now()
                .duration_since(std::time::UNIX_EPOCH)
                .unwrap()
                .as_nanos()
        ));
        fs::create_dir_all(p.join("src")).unwrap();
        let mut pkg = fs::File::create(p.join("package.json")).unwrap();
        pkg.write_all(
            br#"{"name":"sample-project","description":"fixture",
                 "dependencies":{"express":"^4"},"main":"src/index.js"}"#,
        )
        .unwrap();
        fs::write(p.join("src").join("index.js"), b"console.log(\"hi\");\n").unwrap();
        fs::write(p.join("src").join("util.py"), b"def f():\n    return 1\n").unwrap();
        p
    }

    #[test]
    fn builds_complete_map() {
        let dir = fixture();
        let map = build_project_map(&dir, &Options::default());
        assert_eq!(map.schema, SCHEMA_VERSION);
        assert_eq!(map.name, "sample-project");
        assert_eq!(map.description.as_deref(), Some("fixture"));
        assert!(map.stacks.iter().any(|s| s.kind == "node"));
        assert!(map.languages.iter().any(|l| l.language == "JavaScript"));
        assert!(map.languages.iter().any(|l| l.language == "Python"));
        assert!(map.entry_points.iter().any(|e| e == "src/index.js"));
        assert!(map.metrics.total_files >= 3);
        // JSON round-trips.
        let text = map.to_json().to_pretty();
        let parsed = crate::json::parse(&text).unwrap();
        assert_eq!(parsed.get("schema").and_then(|v| v.as_str()), Some(SCHEMA_VERSION));
    }

    #[test]
    fn counts_lines_accurately() {
        let dir = fixture();
        let map = build_project_map(&dir, &Options::default());
        let js = map.languages.iter().find(|l| l.language == "JavaScript").unwrap();
        assert_eq!(js.lines, 1);
        let py = map.languages.iter().find(|l| l.language == "Python").unwrap();
        assert_eq!(py.lines, 2);
    }
}
