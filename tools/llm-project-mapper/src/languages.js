// Maps file names / extensions to a human language label.

/** @type {Record<string, string>} */
const BY_EXTENSION = {
  '.ts': 'TypeScript',
  '.tsx': 'TypeScript',
  '.mts': 'TypeScript',
  '.cts': 'TypeScript',
  '.js': 'JavaScript',
  '.jsx': 'JavaScript',
  '.mjs': 'JavaScript',
  '.cjs': 'JavaScript',
  '.py': 'Python',
  '.go': 'Go',
  '.rs': 'Rust',
  '.java': 'Java',
  '.kt': 'Kotlin',
  '.kts': 'Kotlin',
  '.cs': 'C#',
  '.cpp': 'C++',
  '.cc': 'C++',
  '.cxx': 'C++',
  '.hpp': 'C++',
  '.hh': 'C++',
  '.hxx': 'C++',
  '.c': 'C',
  '.h': 'C/C++ Header',
  '.m': 'Objective-C',
  '.mm': 'Objective-C++',
  '.swift': 'Swift',
  '.rb': 'Ruby',
  '.php': 'PHP',
  '.dart': 'Dart',
  '.ex': 'Elixir',
  '.exs': 'Elixir',
  '.scala': 'Scala',
  '.sh': 'Shell',
  '.bash': 'Shell',
  '.zsh': 'Shell',
  '.ps1': 'PowerShell',
  '.psm1': 'PowerShell',
  '.lua': 'Lua',
  '.r': 'R',
  '.sql': 'SQL',
  '.html': 'HTML',
  '.css': 'CSS',
  '.scss': 'SCSS',
  '.vue': 'Vue',
  '.svelte': 'Svelte',
  '.md': 'Markdown',
  '.mdx': 'Markdown',
  '.json': 'JSON',
  '.yml': 'YAML',
  '.yaml': 'YAML',
  '.toml': 'TOML',
  '.xml': 'XML',
  '.proto': 'Protobuf',
  '.cmake': 'CMake',
  '.gradle': 'Gradle',
  '.tf': 'Terraform',
  '.dockerfile': 'Dockerfile',
};

/** @type {Record<string, string>} */
const BY_FILENAME = {
  'CMakeLists.txt': 'CMake',
  Dockerfile: 'Dockerfile',
  Makefile: 'Makefile',
  'go.mod': 'Go Module',
  'go.sum': 'Go Module',
  Gemfile: 'Ruby',
  Rakefile: 'Ruby',
};

const TEXT_LANGUAGES = new Set(['JSON', 'YAML', 'TOML', 'XML', 'Markdown']);

/**
 * @param {string} ext lowercased extension including the dot
 * @returns {boolean}
 */
export function extLooksTextual(ext) {
  return Boolean(BY_EXTENSION[ext]);
}

/**
 * @param {string} filename base name of the file
 * @param {string} ext lowercased extension including the dot
 * @returns {string}
 */
export function languageFor(filename, ext) {
  if (BY_FILENAME[filename]) return BY_FILENAME[filename];
  if (BY_EXTENSION[ext]) return BY_EXTENSION[ext];
  return 'Other';
}

/**
 * Whether a language is primarily data/markup rather than source code.
 * @param {string} language
 * @returns {boolean}
 */
export function isDataLanguage(language) {
  return TEXT_LANGUAGES.has(language);
}
