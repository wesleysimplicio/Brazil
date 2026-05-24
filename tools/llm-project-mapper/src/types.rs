//! Domain model for a project map and its conversion to JSON.

use std::path::PathBuf;

use crate::json::Json;

pub const SCHEMA_VERSION: &str = "llm-project-mapper/v1";
pub const GENERATOR_NAME: &str = "@wesleysimplicio/llm-project-mapper";
pub const GENERATOR_VERSION: &str = "0.1.0";

#[derive(Debug, Clone)]
pub struct Stack {
    pub kind: String,
    pub marker: String,
    pub name: Option<String>,
    pub manager: Option<String>,
    pub frameworks: Vec<String>,
}

#[derive(Debug, Clone)]
pub struct DependencyGroup {
    pub stack: String,
    pub source: String,
    pub runtime: Vec<String>,
    pub dev: Vec<String>,
}

#[derive(Debug, Clone)]
pub struct LanguageStat {
    pub language: String,
    pub files: u64,
    pub lines: i64,
    pub bytes: u64,
}

/// Internal per-file record. Not serialized directly; feeds language stats and
/// carries the absolute path for the parallel line-counting phase.
#[derive(Debug, Clone)]
pub struct FileEntry {
    pub path: String,
    pub abs_path: PathBuf,
    pub language: String,
    pub bytes: u64,
    pub lines: Option<i64>,
    pub file_id: usize,
}

#[derive(Debug, Clone)]
pub struct TreeNode {
    pub name: String,
    pub is_file: bool,
    pub language: Option<String>,
    pub bytes: Option<u64>,
    pub lines: Option<i64>,
    pub children: Vec<TreeNode>,
    pub truncated: bool,
    pub file_id: Option<usize>,
}

impl TreeNode {
    pub fn dir(name: impl Into<String>) -> TreeNode {
        TreeNode {
            name: name.into(),
            is_file: false,
            language: None,
            bytes: None,
            lines: None,
            children: Vec::new(),
            truncated: false,
            file_id: None,
        }
    }

    fn to_json(&self) -> Json {
        if self.is_file {
            Json::Obj(vec![
                ("name".into(), Json::str(self.name.clone())),
                ("type".into(), Json::str("file")),
                (
                    "language".into(),
                    Json::str(self.language.clone().unwrap_or_default()),
                ),
                ("bytes".into(), Json::Int(self.bytes.unwrap_or(0) as i64)),
                (
                    "lines".into(),
                    match self.lines {
                        Some(n) => Json::Int(n),
                        None => Json::Null,
                    },
                ),
            ])
        } else if self.truncated {
            Json::Obj(vec![
                ("name".into(), Json::str(self.name.clone())),
                ("type".into(), Json::str("dir")),
                ("truncated".into(), Json::Bool(true)),
            ])
        } else {
            Json::Obj(vec![
                ("name".into(), Json::str(self.name.clone())),
                ("type".into(), Json::str("dir")),
                (
                    "children".into(),
                    Json::arr(self.children.iter().map(TreeNode::to_json)),
                ),
            ])
        }
    }
}

#[derive(Debug, Clone)]
pub struct ProjectMetrics {
    pub total_files: u64,
    pub total_dirs: u64,
    pub total_lines: i64,
    pub total_bytes: u64,
    pub max_depth: u64,
}

#[derive(Debug, Clone)]
pub struct ProjectMap {
    pub schema: String,
    pub generated_at: String,
    pub root: String,
    pub name: String,
    pub description: Option<String>,
    pub stacks: Vec<Stack>,
    pub languages: Vec<LanguageStat>,
    pub dependencies: Vec<DependencyGroup>,
    pub entry_points: Vec<String>,
    pub metrics: ProjectMetrics,
    pub structure: TreeNode,
}

impl ProjectMap {
    pub fn to_json(&self) -> Json {
        let mut obj: Vec<(String, Json)> = vec![
            ("schema".into(), Json::str(self.schema.clone())),
            ("generatedAt".into(), Json::str(self.generated_at.clone())),
            (
                "generator".into(),
                Json::Obj(vec![
                    ("name".into(), Json::str(GENERATOR_NAME)),
                    ("version".into(), Json::str(GENERATOR_VERSION)),
                ]),
            ),
            ("root".into(), Json::str(self.root.clone())),
            ("name".into(), Json::str(self.name.clone())),
        ];
        if let Some(d) = &self.description {
            obj.push(("description".into(), Json::str(d.clone())));
        }

        obj.push((
            "stacks".into(),
            Json::arr(self.stacks.iter().map(|s| {
                let mut pairs = vec![
                    ("kind".into(), Json::str(s.kind.clone())),
                    ("marker".into(), Json::str(s.marker.clone())),
                ];
                if let Some(n) = &s.name {
                    pairs.push(("name".into(), Json::str(n.clone())));
                }
                if let Some(m) = &s.manager {
                    pairs.push(("manager".into(), Json::str(m.clone())));
                }
                if !s.frameworks.is_empty() {
                    pairs.push((
                        "frameworks".into(),
                        Json::arr(s.frameworks.iter().map(|f| Json::str(f.clone()))),
                    ));
                }
                Json::Obj(pairs)
            })),
        ));

        obj.push((
            "languages".into(),
            Json::arr(self.languages.iter().map(|l| {
                Json::Obj(vec![
                    ("language".into(), Json::str(l.language.clone())),
                    ("files".into(), Json::Int(l.files as i64)),
                    ("lines".into(), Json::Int(l.lines)),
                    ("bytes".into(), Json::Int(l.bytes as i64)),
                ])
            })),
        ));

        obj.push((
            "dependencies".into(),
            Json::arr(self.dependencies.iter().map(|d| {
                Json::Obj(vec![
                    ("stack".into(), Json::str(d.stack.clone())),
                    ("source".into(), Json::str(d.source.clone())),
                    (
                        "runtime".into(),
                        Json::arr(d.runtime.iter().map(|r| Json::str(r.clone()))),
                    ),
                    (
                        "dev".into(),
                        Json::arr(d.dev.iter().map(|r| Json::str(r.clone()))),
                    ),
                ])
            })),
        ));

        obj.push((
            "entryPoints".into(),
            Json::arr(self.entry_points.iter().map(|e| Json::str(e.clone()))),
        ));

        obj.push((
            "metrics".into(),
            Json::Obj(vec![
                ("totalFiles".into(), Json::Int(self.metrics.total_files as i64)),
                ("totalDirs".into(), Json::Int(self.metrics.total_dirs as i64)),
                ("totalLines".into(), Json::Int(self.metrics.total_lines)),
                ("totalBytes".into(), Json::Int(self.metrics.total_bytes as i64)),
                ("maxDepth".into(), Json::Int(self.metrics.max_depth as i64)),
            ]),
        ));

        obj.push(("structure".into(), self.structure.to_json()));
        Json::Obj(obj)
    }
}
