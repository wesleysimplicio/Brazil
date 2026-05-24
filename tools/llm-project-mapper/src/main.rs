use std::path::{Path, PathBuf};
use std::process::ExitCode;

use llm_project_mapper::mapper::{build_project_map, Options};
use llm_project_mapper::render::render_markdown;
use llm_project_mapper::types::GENERATOR_VERSION;

const HELP: &str = "\
llm-project-mapper
Map any codebase into a structured .llm-project-mapper.json so AI agents
understand a project before they program it.

Usage:
  llm-project-mapper [path] [options]

Arguments:
  path                  project directory to map (default \".\")

Options:
  -o, --out <file>      output JSON path
                        (default <path>/.llm-project-mapper.json)
  --stdout              print JSON to stdout, do not write a file
  --summary             also print a Markdown summary to stderr
  --max-depth <n>       structure tree depth (default 8)
  --max-file-bytes <n>  skip line counting above this size (default 2000000)
  --ignore <glob>       extra ignore pattern (repeatable)
  -h, --help            show this help
  -v, --version         print version
";

struct Cli {
    path: String,
    out: Option<String>,
    stdout: bool,
    summary: bool,
    opts: Options,
    help: bool,
    version: bool,
}

fn parse_args(args: &[String]) -> Result<Cli, String> {
    let mut cli = Cli {
        path: ".".to_string(),
        out: None,
        stdout: false,
        summary: false,
        opts: Options::default(),
        help: false,
        version: false,
    };
    let mut saw_path = false;
    let mut i = 0;
    while i < args.len() {
        let a = &args[i];
        let mut need = || -> Result<String, String> {
            i += 1;
            args.get(i)
                .cloned()
                .ok_or_else(|| format!("missing value for {}", a))
        };
        match a.as_str() {
            "-h" | "--help" => cli.help = true,
            "-v" | "--version" => cli.version = true,
            "-o" | "--out" => cli.out = Some(need()?),
            "--stdout" => cli.stdout = true,
            "--summary" => cli.summary = true,
            "--max-depth" => {
                cli.opts.max_depth = need()?
                    .parse()
                    .map_err(|_| "--max-depth must be a non-negative integer".to_string())?
            }
            "--max-file-bytes" => {
                cli.opts.max_file_bytes = need()?
                    .parse()
                    .map_err(|_| "--max-file-bytes must be a non-negative integer".to_string())?
            }
            "--ignore" => cli.opts.extra_ignores.push(need()?),
            other if other.starts_with('-') => {
                return Err(format!("unknown option: {}", other))
            }
            other => {
                if saw_path {
                    return Err(format!("unexpected argument: {}", other));
                }
                cli.path = other.to_string();
                saw_path = true;
            }
        }
        i += 1;
    }
    Ok(cli)
}

fn main() -> ExitCode {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let cli = match parse_args(&args) {
        Ok(c) => c,
        Err(e) => {
            eprintln!("error: {}", e);
            return ExitCode::from(2);
        }
    };

    if cli.help {
        print!("{}", HELP);
        return ExitCode::SUCCESS;
    }
    if cli.version {
        println!("{}", GENERATOR_VERSION);
        return ExitCode::SUCCESS;
    }

    let root = PathBuf::from(&cli.path);
    match std::fs::metadata(&root) {
        Ok(m) if m.is_dir() => {}
        Ok(_) => {
            eprintln!("error: not a directory: {}", root.display());
            return ExitCode::from(1);
        }
        Err(_) => {
            eprintln!("error: path does not exist: {}", root.display());
            return ExitCode::from(1);
        }
    }

    let map = build_project_map(&root, &cli.opts);
    let json_text = map.to_json().to_pretty() + "\n";

    if cli.summary {
        eprintln!("{}", render_markdown(&map));
    }

    if cli.stdout {
        print!("{}", json_text);
        return ExitCode::SUCCESS;
    }

    let out_path: PathBuf = match &cli.out {
        Some(o) => PathBuf::from(o),
        None => Path::new(&map.root).join(".llm-project-mapper.json"),
    };
    if let Err(e) = std::fs::write(&out_path, &json_text) {
        eprintln!("error: failed to write {}: {}", out_path.display(), e);
        return ExitCode::from(1);
    }

    let top: Vec<String> = map
        .languages
        .iter()
        .take(3)
        .map(|l| l.language.clone())
        .collect();
    let top_str = if top.is_empty() {
        String::new()
    } else {
        format!(", top: {}", top.join(", "))
    };
    println!(
        "Mapped \"{}\": {} files, {} lines, {} stack(s){}\n-> {}",
        map.name,
        map.metrics.total_files,
        map.metrics.total_lines,
        map.stacks.len(),
        top_str,
        out_path.display()
    );
    ExitCode::SUCCESS
}
