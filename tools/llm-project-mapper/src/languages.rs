//! Maps file names / extensions to a human language label.

/// Human language for a file, by special name first then by extension.
/// `ext` is the lowercased extension including the leading dot (may be empty).
pub fn language_for(filename: &str, ext: &str) -> &'static str {
    match filename {
        "CMakeLists.txt" => return "CMake",
        "Dockerfile" => return "Dockerfile",
        "Makefile" => return "Makefile",
        "go.mod" | "go.sum" => return "Go Module",
        "Gemfile" | "Rakefile" => return "Ruby",
        _ => {}
    }
    match ext {
        ".ts" | ".tsx" | ".mts" | ".cts" => "TypeScript",
        ".js" | ".jsx" | ".mjs" | ".cjs" => "JavaScript",
        ".py" => "Python",
        ".go" => "Go",
        ".rs" => "Rust",
        ".java" => "Java",
        ".kt" | ".kts" => "Kotlin",
        ".cs" => "C#",
        ".cpp" | ".cc" | ".cxx" | ".hpp" | ".hh" | ".hxx" => "C++",
        ".c" => "C",
        ".h" => "C/C++ Header",
        ".m" => "Objective-C",
        ".mm" => "Objective-C++",
        ".swift" => "Swift",
        ".rb" => "Ruby",
        ".php" => "PHP",
        ".dart" => "Dart",
        ".ex" | ".exs" => "Elixir",
        ".scala" => "Scala",
        ".sh" | ".bash" | ".zsh" => "Shell",
        ".ps1" | ".psm1" => "PowerShell",
        ".lua" => "Lua",
        ".r" => "R",
        ".sql" => "SQL",
        ".html" => "HTML",
        ".css" => "CSS",
        ".scss" => "SCSS",
        ".vue" => "Vue",
        ".svelte" => "Svelte",
        ".md" | ".mdx" => "Markdown",
        ".json" => "JSON",
        ".yml" | ".yaml" => "YAML",
        ".toml" => "TOML",
        ".xml" => "XML",
        ".proto" => "Protobuf",
        ".cmake" => "CMake",
        ".gradle" => "Gradle",
        ".tf" => "Terraform",
        ".dockerfile" => "Dockerfile",
        _ => "Other",
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn maps_sources_and_specials() {
        assert_eq!(language_for("main.cpp", ".cpp"), "C++");
        assert_eq!(language_for("runtime.hpp", ".hpp"), "C++");
        assert_eq!(language_for("index.ts", ".ts"), "TypeScript");
        assert_eq!(language_for("main.py", ".py"), "Python");
        assert_eq!(language_for("CMakeLists.txt", ".txt"), "CMake");
        assert_eq!(language_for("go.mod", ".mod"), "Go Module");
        assert_eq!(language_for("mystery.qwop", ".qwop"), "Other");
    }
}
