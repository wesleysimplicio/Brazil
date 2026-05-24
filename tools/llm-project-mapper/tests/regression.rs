//! Golden-snapshot regression test.
//!
//! Maps a fixed fixture project and compares the serialized output against a
//! committed golden file, so any unintended change to the map (schema,
//! language detection, stack parsing, line counts, ordering, ...) fails the
//! build. The two volatile fields (`generatedAt`, `root`) are normalized.
//!
//! When the output changes intentionally, regenerate the golden with:
//!     UPDATE_GOLDEN=1 cargo test --test regression

use std::path::Path;

use llm_project_mapper::mapper::{build_project_map, Options};

/// Replace machine/time-dependent values so the snapshot is stable.
fn normalize(json: &str) -> String {
    json.lines()
        .map(|line| {
            if line.starts_with("  \"generatedAt\":") {
                "  \"generatedAt\": \"<normalized>\","
            } else if line.starts_with("  \"root\":") {
                "  \"root\": \"<normalized>\","
            } else {
                return line.to_string();
            }
            .to_string()
        })
        .collect::<Vec<_>>()
        .join("\n")
}

#[test]
fn golden_snapshot_sample_project() {
    let manifest = env!("CARGO_MANIFEST_DIR");
    let fixture = Path::new(manifest).join("tests/fixtures/sample");
    let golden = Path::new(manifest).join("tests/golden/sample.json");

    let map = build_project_map(&fixture, &Options::default());
    let actual = normalize(&map.to_json().to_pretty());

    if std::env::var("UPDATE_GOLDEN").is_ok() {
        std::fs::create_dir_all(golden.parent().unwrap()).unwrap();
        std::fs::write(&golden, format!("{}\n", actual)).unwrap();
        eprintln!("updated golden: {}", golden.display());
        return;
    }

    let expected = std::fs::read_to_string(&golden)
        .expect("golden file missing; run UPDATE_GOLDEN=1 cargo test --test regression");

    assert_eq!(
        actual.trim_end(),
        expected.trim_end(),
        "project map output changed. If this is intentional, regenerate with: \
         UPDATE_GOLDEN=1 cargo test --test regression"
    );
}
