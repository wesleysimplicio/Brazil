//! Directory traversal: builds the structure tree and the flat file list.
//! Line counting is performed separately (in parallel) by `count_lines`.

use std::fs;
use std::path::Path;

use crate::ignore::Ignore;
use crate::languages::language_for;
use crate::types::{FileEntry, TreeNode};

pub struct WalkResult {
    pub tree: TreeNode,
    pub files: Vec<FileEntry>,
    pub total_files: u64,
    pub total_dirs: u64,
    pub total_bytes: u64,
    pub max_depth: u64,
}

/// Walk `root` (cheap: only readdir + metadata), honouring `ignore`. Files keep
/// their byte size; `lines` is filled later by the parallel counting pass.
pub fn walk(root: &Path, ignore: &Ignore, max_depth: u64) -> WalkResult {
    let mut result = WalkResult {
        tree: TreeNode::dir("."),
        files: Vec::new(),
        total_files: 0,
        total_dirs: 0,
        total_bytes: 0,
        max_depth: 0,
    };
    let tree = visit(root, "", 0, ignore, max_depth, &mut result);
    result.tree = tree;
    result
}

fn visit(
    abs_dir: &Path,
    rel_dir: &str,
    depth: u64,
    ignore: &Ignore,
    max_depth: u64,
    out: &mut WalkResult,
) -> TreeNode {
    out.max_depth = out.max_depth.max(depth);
    let name = if rel_dir.is_empty() {
        ".".to_string()
    } else {
        rel_dir.rsplit('/').next().unwrap_or(rel_dir).to_string()
    };
    let mut node = TreeNode::dir(name);

    let mut names: Vec<String> = match fs::read_dir(abs_dir) {
        Ok(rd) => rd
            .filter_map(|e| e.ok())
            .map(|e| e.file_name().to_string_lossy().to_string())
            .collect(),
        Err(_) => return node,
    };
    names.sort();

    for name in names {
        let abs = abs_dir.join(&name);
        let rel = if rel_dir.is_empty() {
            name.clone()
        } else {
            format!("{}/{}", rel_dir, name)
        };
        let meta = match fs::symlink_metadata(&abs) {
            Ok(m) => m,
            Err(_) => continue,
        };
        let is_dir = meta.is_dir();
        if ignore.is_ignored(&rel, &name, is_dir) {
            continue;
        }

        if is_dir {
            out.total_dirs += 1;
            // Always recurse so metrics and the flat file list (with stable,
            // sequential file ids) stay complete; only the retained tree is
            // gated by depth.
            let child = visit(&abs, &rel, depth + 1, ignore, max_depth, out);
            if depth >= max_depth {
                let mut truncated = TreeNode::dir(name);
                truncated.truncated = true;
                node.children.push(truncated);
            } else {
                node.children.push(child);
            }
        } else if meta.is_file() {
            let ext = extension(&name);
            let language = language_for(&name, &ext);
            let bytes = meta.len();
            let file_id = out.files.len();
            out.total_files += 1;
            out.total_bytes += bytes;
            out.files.push(FileEntry {
                path: rel.clone(),
                abs_path: abs.clone(),
                language: language.to_string(),
                bytes,
                lines: None,
                file_id,
            });
            if depth < max_depth {
                node.children.push(TreeNode {
                    name,
                    is_file: true,
                    language: Some(language.to_string()),
                    bytes: Some(bytes),
                    lines: None,
                    children: Vec::new(),
                    truncated: false,
                    file_id: Some(file_id),
                });
            }
        }
    }
    node
}

fn extension(name: &str) -> String {
    match name.rfind('.') {
        Some(i) if i > 0 => name[i..].to_lowercase(),
        _ => String::new(),
    }
}

/// Count newline-terminated lines in a file, or `None` if it is binary or
/// cannot be read. Files larger than `max_bytes` are skipped by the caller.
pub fn count_lines(path: &Path) -> Option<i64> {
    let data = fs::read(path).ok()?;
    let limit = data.len().min(8000);
    if data[..limit].contains(&0) {
        return None; // binary
    }
    if data.is_empty() {
        return Some(0);
    }
    let mut lines = data.iter().filter(|&&b| b == b'\n').count() as i64;
    if *data.last().unwrap() != b'\n' {
        lines += 1;
    }
    Some(lines)
}
