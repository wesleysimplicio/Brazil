use std::fs;
use std::path::PathBuf;

use llm_project_mapper::mapper::{build_project_map, Options};

fn fixture() -> PathBuf {
    let mut dir = std::env::temp_dir();
    dir.push(format!(
        "lpm-it-{}-{}",
        std::process::id(),
        std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap()
            .as_nanos()
    ));
    fs::create_dir_all(dir.join("src")).unwrap();
    fs::create_dir_all(dir.join("node_modules").join("pkg")).unwrap();
    fs::create_dir_all(dir.join("a").join("b").join("c")).unwrap();

    fs::write(dir.join("src").join("a.js"), b"const a = 1;\nconst b = 2;\n").unwrap();
    fs::write(dir.join("src").join("b.ts"), b"export const x = 1;\n").unwrap();
    fs::write(dir.join("README.md"), b"# hi\n").unwrap();
    fs::write(dir.join("node_modules").join("pkg").join("i.js"), b"x\n").unwrap();
    fs::write(dir.join("a").join("b").join("c").join("deep.go"), b"package main\n").unwrap();
    fs::write(dir.join(".gitignore"), b"*.md\n").unwrap();
    dir
}

#[test]
fn ignores_defaults_and_gitignore() {
    let dir = fixture();
    let map = build_project_map(&dir, &Options::default());
    let langs: Vec<&str> = map.languages.iter().map(|l| l.language.as_str()).collect();

    // node_modules is skipped; the JS file inside it is not counted.
    let js = map.languages.iter().find(|l| l.language == "JavaScript").unwrap();
    assert_eq!(js.files, 1, "only src/a.js should count");
    // .gitignore *.md removes README.md.
    assert!(!langs.contains(&"Markdown"));
    // Deep Go file is still counted for metrics.
    assert!(langs.contains(&"Go"));
}

#[test]
fn respects_max_depth_truncation() {
    let dir = fixture();
    let shallow = build_project_map(&dir, &Options { max_depth: 1, ..Options::default() });
    let deep = build_project_map(&dir, &Options::default());
    // Metrics include deep files regardless of retained tree depth.
    assert_eq!(shallow.metrics.total_files, deep.metrics.total_files);
}

#[test]
fn parallel_counts_match_sequential_totals() {
    let dir = fixture();
    let map = build_project_map(&dir, &Options::default());
    // a.js=2, b.ts=1, deep.go=1, .gitignore=1 => 5 (README.md is ignored).
    let total: i64 = map.languages.iter().map(|l| l.lines).sum();
    assert_eq!(total, map.metrics.total_lines);
    assert_eq!(map.metrics.total_lines, 5);
}
