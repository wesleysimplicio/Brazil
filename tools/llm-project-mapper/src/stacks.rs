//! Detect build/runtime stacks at a project root and parse their manifests.

use std::collections::HashSet;
use std::fs;
use std::path::Path;

use crate::json;
use crate::types::{DependencyGroup, Stack};

/// Project metadata gleaned from the primary manifest.
#[derive(Debug, Default, Clone)]
pub struct Meta {
    pub name: Option<String>,
    pub description: Option<String>,
    pub main: Option<String>,
    pub bins: Vec<String>,
}

pub struct Detection {
    pub stacks: Vec<Stack>,
    pub dependencies: Vec<DependencyGroup>,
    pub meta: Meta,
}

const FRAMEWORK_HINTS: &[(&str, &str)] = &[
    ("react", "react"),
    ("next", "next"),
    ("vue", "vue"),
    ("nuxt", "nuxt"),
    ("svelte", "svelte"),
    ("@angular/core", "angular"),
    ("express", "express"),
    ("fastify", "fastify"),
    ("koa", "koa"),
    ("@nestjs/core", "nestjs"),
    ("electron", "electron"),
    ("vite", "vite"),
    ("webpack", "webpack"),
    ("jest", "jest"),
    ("vitest", "vitest"),
    ("@playwright/test", "playwright"),
    ("playwright", "playwright"),
];

fn read(path: &Path) -> Option<String> {
    fs::read_to_string(path).ok()
}

fn detect_node_manager(root: &Path) -> &'static str {
    if root.join("pnpm-lock.yaml").exists() {
        "pnpm"
    } else if root.join("yarn.lock").exists() {
        "yarn"
    } else if root.join("bun.lockb").exists() {
        "bun"
    } else {
        "npm"
    }
}

fn detect_frameworks(dep_keys: &HashSet<String>) -> Vec<String> {
    let mut out = Vec::new();
    for (key, label) in FRAMEWORK_HINTS {
        if dep_keys.contains(*key) && !out.iter().any(|x: &String| x == label) {
            out.push((*label).to_string());
        }
    }
    out
}

/// Reduce a `requirements.txt` line to a bare package name.
fn req_name(line: &str) -> Option<String> {
    let no_comment = line.split('#').next().unwrap_or("").trim();
    if no_comment.is_empty() || no_comment.starts_with('-') {
        return None;
    }
    let no_marker = no_comment.split(';').next().unwrap_or("").trim();
    let name: String = no_marker
        .chars()
        .take_while(|c| c.is_ascii_alphanumeric() || matches!(c, '.' | '_' | '-'))
        .collect();
    if name.is_empty() {
        None
    } else {
        Some(name)
    }
}

/// Keys under a TOML `[section]` heading.
fn toml_section_keys(text: &str, section: &str) -> Vec<String> {
    let header = format!("[{}]", section);
    let mut keys = Vec::new();
    let mut in_section = false;
    for raw in text.lines() {
        let line = raw.trim();
        if line.starts_with('[') {
            in_section = line == header;
            continue;
        }
        if in_section {
            if let Some(eq) = line.find('=') {
                let key = line[..eq].trim();
                if !key.is_empty() && !key.starts_with('#') {
                    keys.push(key.to_string());
                }
            }
        }
    }
    keys
}

/// First `key = "value"` scalar found in TOML/simple text.
fn scalar(text: &str, key: &str) -> Option<String> {
    for raw in text.lines() {
        let line = raw.trim();
        if let Some(rest) = line.strip_prefix(key) {
            let rest = rest.trim_start();
            if let Some(rest) = rest.strip_prefix('=') {
                let v = rest.trim();
                if let Some(inner) = v.strip_prefix('"').and_then(|s| s.strip_suffix('"')) {
                    return Some(inner.to_string());
                }
            }
        }
    }
    None
}

/// Quoted strings inside `key = [ ... ]`.
fn array_strings(text: &str, key: &str) -> Vec<String> {
    let needle = format!("{} =", key);
    let alt = format!("{}=", key);
    let start = text.find(&needle).or_else(|| text.find(&alt));
    let Some(start) = start else {
        return Vec::new();
    };
    let after = &text[start..];
    let Some(open) = after.find('[') else {
        return Vec::new();
    };
    let Some(close) = after[open..].find(']') else {
        return Vec::new();
    };
    let block = &after[open + 1..open + close];
    let mut out = Vec::new();
    let mut chars = block.chars().peekable();
    while let Some(c) = chars.next() {
        if c == '"' {
            let mut s = String::new();
            for c2 in chars.by_ref() {
                if c2 == '"' {
                    break;
                }
                s.push(c2);
            }
            out.push(s);
        }
    }
    out
}

/// Inspect a project root and report detected stacks plus their dependencies.
pub fn detect_stacks(root: &Path) -> Detection {
    let mut stacks = Vec::new();
    let mut dependencies = Vec::new();
    let mut meta = Meta::default();

    let entries: HashSet<String> = fs::read_dir(root)
        .map(|rd| {
            rd.filter_map(|e| e.ok())
                .map(|e| e.file_name().to_string_lossy().to_string())
                .collect()
        })
        .unwrap_or_default();
    let has = |f: &str| entries.contains(f);

    // Node / TypeScript
    if has("package.json") {
        if let Some(pkg) = read(&root.join("package.json")).and_then(|t| json::parse(&t).ok()) {
            let runtime = pkg.get("dependencies").map(|d| d.keys()).unwrap_or_default();
            let dev = pkg
                .get("devDependencies")
                .map(|d| d.keys())
                .unwrap_or_default();
            let mut all: HashSet<String> = HashSet::new();
            all.extend(runtime.iter().cloned());
            all.extend(dev.iter().cloned());

            stacks.push(Stack {
                kind: "node".into(),
                marker: "package.json".into(),
                name: pkg.get("name").and_then(|v| v.as_str()).map(String::from),
                manager: Some(detect_node_manager(root).into()),
                frameworks: detect_frameworks(&all),
            });
            dependencies.push(DependencyGroup {
                stack: "node".into(),
                source: "package.json".into(),
                runtime,
                dev,
            });
            meta.name = pkg.get("name").and_then(|v| v.as_str()).map(String::from);
            meta.description = pkg
                .get("description")
                .and_then(|v| v.as_str())
                .map(String::from);
            meta.main = pkg.get("main").and_then(|v| v.as_str()).map(String::from);
            if let Some(bin) = pkg.get("bin") {
                match bin {
                    json::Json::Str(s) => meta.bins.push(s.clone()),
                    json::Json::Obj(_) => {
                        if let Some(obj) = bin.as_object() {
                            for (_, v) in obj {
                                if let Some(s) = v.as_str() {
                                    meta.bins.push(s.to_string());
                                }
                            }
                        }
                    }
                    _ => {}
                }
            }
        }
    }

    // Python
    if has("pyproject.toml") {
        let text = read(&root.join("pyproject.toml")).unwrap_or_default();
        let mut runtime: Vec<String> = array_strings(&text, "dependencies")
            .iter()
            .filter_map(|s| req_name(s))
            .collect();
        runtime.extend(
            toml_section_keys(&text, "tool.poetry.dependencies")
                .into_iter()
                .filter(|k| k != "python"),
        );
        let name = scalar(&text, "name");
        stacks.push(Stack {
            kind: "python".into(),
            marker: "pyproject.toml".into(),
            name: name.clone(),
            manager: Some(if text.contains("[tool.poetry]") {
                "poetry".into()
            } else {
                "pip".into()
            }),
            frameworks: Vec::new(),
        });
        dependencies.push(DependencyGroup {
            stack: "python".into(),
            source: "pyproject.toml".into(),
            runtime,
            dev: Vec::new(),
        });
        if meta.name.is_none() {
            meta.name = name;
        }
    } else if has("requirements.txt") {
        let text = read(&root.join("requirements.txt")).unwrap_or_default();
        let runtime: Vec<String> = text.lines().filter_map(req_name).collect();
        stacks.push(Stack {
            kind: "python".into(),
            marker: "requirements.txt".into(),
            name: None,
            manager: Some("pip".into()),
            frameworks: Vec::new(),
        });
        dependencies.push(DependencyGroup {
            stack: "python".into(),
            source: "requirements.txt".into(),
            runtime,
            dev: Vec::new(),
        });
    } else if has("setup.py") {
        stacks.push(Stack {
            kind: "python".into(),
            marker: "setup.py".into(),
            name: None,
            manager: Some("pip".into()),
            frameworks: Vec::new(),
        });
    }

    // Go
    if has("go.mod") {
        let text = read(&root.join("go.mod")).unwrap_or_default();
        let mut name = None;
        let mut runtime = Vec::new();
        let mut in_block = false;
        for raw in text.lines() {
            let line = raw.trim();
            if let Some(rest) = line.strip_prefix("module ") {
                name = Some(rest.trim().to_string());
            } else if line.starts_with("require (") {
                in_block = true;
            } else if in_block && line == ")" {
                in_block = false;
            } else if in_block {
                if let Some(dep) = line.split_whitespace().next() {
                    if !dep.is_empty() {
                        runtime.push(dep.to_string());
                    }
                }
            } else if let Some(rest) = line.strip_prefix("require ") {
                if let Some(dep) = rest.split_whitespace().next() {
                    runtime.push(dep.to_string());
                }
            }
        }
        stacks.push(Stack {
            kind: "go".into(),
            marker: "go.mod".into(),
            name,
            manager: Some("go".into()),
            frameworks: Vec::new(),
        });
        dependencies.push(DependencyGroup {
            stack: "go".into(),
            source: "go.mod".into(),
            runtime,
            dev: Vec::new(),
        });
    }

    // Rust
    if has("Cargo.toml") {
        let text = read(&root.join("Cargo.toml")).unwrap_or_default();
        stacks.push(Stack {
            kind: "rust".into(),
            marker: "Cargo.toml".into(),
            name: scalar(&text, "name"),
            manager: Some("cargo".into()),
            frameworks: Vec::new(),
        });
        dependencies.push(DependencyGroup {
            stack: "rust".into(),
            source: "Cargo.toml".into(),
            runtime: toml_section_keys(&text, "dependencies"),
            dev: toml_section_keys(&text, "dev-dependencies"),
        });
        if meta.name.is_none() {
            meta.name = scalar(&text, "name");
        }
    }

    // PHP
    if has("composer.json") {
        if let Some(c) = read(&root.join("composer.json")).and_then(|t| json::parse(&t).ok()) {
            stacks.push(Stack {
                kind: "php".into(),
                marker: "composer.json".into(),
                name: c.get("name").and_then(|v| v.as_str()).map(String::from),
                manager: Some("composer".into()),
                frameworks: Vec::new(),
            });
            dependencies.push(DependencyGroup {
                stack: "php".into(),
                source: "composer.json".into(),
                runtime: c.get("require").map(|d| d.keys()).unwrap_or_default(),
                dev: c.get("require-dev").map(|d| d.keys()).unwrap_or_default(),
            });
        }
    }

    // C / C++ (CMake)
    if has("CMakeLists.txt") {
        let text = read(&root.join("CMakeLists.txt")).unwrap_or_default();
        let name = cmake_project_name(&text);
        stacks.push(Stack {
            kind: "cmake".into(),
            marker: "CMakeLists.txt".into(),
            name: name.clone(),
            manager: Some("cmake".into()),
            frameworks: Vec::new(),
        });
        if meta.name.is_none() {
            meta.name = name;
        }
    }

    // Presence-only detections.
    for (file, kind, manager) in [
        ("Gemfile", "ruby", "bundler"),
        ("pom.xml", "jvm", "maven"),
        ("build.gradle", "jvm", "gradle"),
        ("build.gradle.kts", "jvm", "gradle"),
        ("pubspec.yaml", "dart", "pub"),
        ("mix.exs", "elixir", "mix"),
        ("Package.swift", "swift", "spm"),
    ] {
        if has(file) {
            stacks.push(Stack {
                kind: kind.into(),
                marker: file.into(),
                name: None,
                manager: Some(manager.into()),
                frameworks: Vec::new(),
            });
        }
    }

    // .NET uses globbed marker files.
    let dotnet = entries
        .iter()
        .find(|e| e.ends_with(".csproj"))
        .or_else(|| entries.iter().find(|e| e.ends_with(".sln")));
    if let Some(marker) = dotnet {
        let name = marker
            .trim_end_matches(".csproj")
            .trim_end_matches(".sln")
            .to_string();
        stacks.push(Stack {
            kind: "dotnet".into(),
            marker: marker.clone(),
            name: Some(name),
            manager: Some("dotnet".into()),
            frameworks: Vec::new(),
        });
    }

    Detection {
        stacks,
        dependencies,
        meta,
    }
}

/// Extract the name from `project(NAME ...)` in a CMakeLists.txt.
fn cmake_project_name(text: &str) -> Option<String> {
    let idx = text.find("project")?;
    let after = &text[idx + "project".len()..];
    let open = after.find('(')?;
    let rest = &after[open + 1..];
    let name: String = rest
        .trim_start()
        .chars()
        .take_while(|c| c.is_ascii_alphanumeric() || matches!(c, '_' | '.' | '-'))
        .collect();
    if name.is_empty() {
        None
    } else {
        Some(name)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::Write;

    fn tmp() -> std::path::PathBuf {
        let mut p = std::env::temp_dir();
        p.push(format!("lpm-stacks-{}-{:?}", std::process::id(), now()));
        fs::create_dir_all(&p).unwrap();
        p
    }

    fn now() -> u128 {
        std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap()
            .as_nanos()
    }

    fn write(dir: &Path, name: &str, body: &str) {
        let mut f = fs::File::create(dir.join(name)).unwrap();
        f.write_all(body.as_bytes()).unwrap();
    }

    #[test]
    fn node_project() {
        let dir = tmp();
        write(
            &dir,
            "package.json",
            r#"{"name":"demo-app","description":"a demo",
                "dependencies":{"react":"^18","express":"^4"},
                "devDependencies":{"vitest":"^1"},"main":"src/index.js"}"#,
        );
        write(&dir, "package-lock.json", "{}");
        let d = detect_stacks(&dir);
        let node = d.stacks.iter().find(|s| s.kind == "node").unwrap();
        assert_eq!(node.name.as_deref(), Some("demo-app"));
        assert_eq!(node.manager.as_deref(), Some("npm"));
        let mut fw = node.frameworks.clone();
        fw.sort();
        assert_eq!(fw, vec!["express", "react", "vitest"]);
        let deps = d.dependencies.iter().find(|x| x.stack == "node").unwrap();
        let mut rt = deps.runtime.clone();
        rt.sort();
        assert_eq!(rt, vec!["express", "react"]);
        assert_eq!(deps.dev, vec!["vitest"]);
        assert_eq!(d.meta.main.as_deref(), Some("src/index.js"));
    }

    #[test]
    fn go_modules() {
        let dir = tmp();
        write(
            &dir,
            "go.mod",
            "module github.com/acme/widget\n\ngo 1.22\n\nrequire (\n\tgithub.com/spf13/cobra v1.8.0\n\tgithub.com/stretchr/testify v1.9.0\n)\n",
        );
        let d = detect_stacks(&dir);
        let go = d.stacks.iter().find(|s| s.kind == "go").unwrap();
        assert_eq!(go.name.as_deref(), Some("github.com/acme/widget"));
        let deps = d.dependencies.iter().find(|x| x.stack == "go").unwrap();
        assert!(deps.runtime.contains(&"github.com/spf13/cobra".to_string()));
        assert!(deps.runtime.contains(&"github.com/stretchr/testify".to_string()));
    }

    #[test]
    fn cmake_name() {
        let dir = tmp();
        write(
            &dir,
            "CMakeLists.txt",
            "cmake_minimum_required(VERSION 3.27)\nproject(us4_runtime VERSION 0.1.0)\n",
        );
        let d = detect_stacks(&dir);
        let cmake = d.stacks.iter().find(|s| s.kind == "cmake").unwrap();
        assert_eq!(cmake.name.as_deref(), Some("us4_runtime"));
        assert_eq!(d.meta.name.as_deref(), Some("us4_runtime"));
    }

    #[test]
    fn python_requirements() {
        let dir = tmp();
        write(
            &dir,
            "requirements.txt",
            "# deps\nrequests>=2.0\nflask==3.0  # web\n-e .\nnumpy\n",
        );
        let d = detect_stacks(&dir);
        assert!(d.stacks.iter().any(|s| s.kind == "python"));
        let deps = d.dependencies.iter().find(|x| x.stack == "python").unwrap();
        let mut rt = deps.runtime.clone();
        rt.sort();
        assert_eq!(rt, vec!["flask", "numpy", "requests"]);
    }
}
