//! Human-readable Markdown summary of a project map.

use crate::types::ProjectMap;

fn list_capped(items: &[String], cap: usize) -> String {
    if items.is_empty() {
        return "_none_".to_string();
    }
    let shown = items
        .iter()
        .take(cap)
        .cloned()
        .collect::<Vec<_>>()
        .join(", ");
    if items.len() > cap {
        format!("{} _(+{} more)_", shown, items.len() - cap)
    } else {
        shown
    }
}

/// Render a Markdown overview of `map`.
pub fn render_markdown(map: &ProjectMap) -> String {
    let mut out = String::new();
    let mut line = |s: String| {
        out.push_str(&s);
        out.push('\n');
    };

    line(format!("# Project Map: {}", map.name));
    if let Some(d) = &map.description {
        line(String::new());
        line(d.clone());
    }
    line(String::new());
    line(format!("- Generated: {}", map.generated_at));
    line(format!("- Root: {}", map.root));
    line(format!("- Schema: {}", map.schema));

    line(String::new());
    line("## Stacks".into());
    if map.stacks.is_empty() {
        line("_No known stack markers found._".into());
    } else {
        for s in &map.stacks {
            let mut bits = Vec::new();
            if let Some(n) = &s.name {
                bits.push(format!("name: {}", n));
            }
            if let Some(m) = &s.manager {
                bits.push(format!("manager: {}", m));
            }
            if !s.frameworks.is_empty() {
                bits.push(format!("frameworks: {}", s.frameworks.join(", ")));
            }
            let detail = if bits.is_empty() {
                String::new()
            } else {
                format!(" — {}", bits.join(", "))
            };
            line(format!("- **{}** ({}){}", s.kind, s.marker, detail));
        }
    }

    line(String::new());
    line("## Languages".into());
    if map.languages.is_empty() {
        line("_No files analyzed._".into());
    } else {
        line("| Language | Files | Lines | Bytes |".into());
        line("| --- | ---: | ---: | ---: |".into());
        for l in map.languages.iter().take(12) {
            line(format!(
                "| {} | {} | {} | {} |",
                l.language, l.files, l.lines, l.bytes
            ));
        }
    }

    line(String::new());
    line("## Dependencies".into());
    if map.dependencies.is_empty() {
        line("_No dependency manifests parsed._".into());
    } else {
        for d in &map.dependencies {
            line(String::new());
            line(format!("### {} ({})", d.stack, d.source));
            line(format!(
                "- runtime ({}): {}",
                d.runtime.len(),
                list_capped(&d.runtime, 30)
            ));
            if !d.dev.is_empty() {
                line(format!("- dev ({}): {}", d.dev.len(), list_capped(&d.dev, 30)));
            }
        }
    }

    line(String::new());
    line("## Entry points".into());
    if map.entry_points.is_empty() {
        line("_None detected._".into());
    }
    for e in &map.entry_points {
        line(format!("- {}", e));
    }

    line(String::new());
    line("## Metrics".into());
    line(format!("- files: {}", map.metrics.total_files));
    line(format!("- directories: {}", map.metrics.total_dirs));
    line(format!("- lines of code: {}", map.metrics.total_lines));
    line(format!("- bytes: {}", map.metrics.total_bytes));
    line(format!("- max depth: {}", map.metrics.max_depth));

    out
}
