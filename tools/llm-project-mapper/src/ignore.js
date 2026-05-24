import { readFileSync } from 'node:fs';
import { join } from 'node:path';

// Directories and files skipped by default. Build output, VCS internals and
// dependency caches add noise without helping an agent understand the project.
const DEFAULT_DIRS = new Set([
  '.git',
  '.hg',
  '.svn',
  'node_modules',
  'bower_components',
  '.pnpm-store',
  'dist',
  'build',
  'out',
  'output',
  '.next',
  '.nuxt',
  '.svelte-kit',
  '.turbo',
  'target',
  'obj',
  '__pycache__',
  '.pytest_cache',
  '.mypy_cache',
  '.ruff_cache',
  '.venv',
  'venv',
  'env',
  '.tox',
  '.gradle',
  '.mvn',
  'vendor',
  'Pods',
  'coverage',
  '.cache',
  '.idea',
  '.vscode',
  '.terraform',
  'DerivedData',
]);

const DEFAULT_FILES = new Set([
  '.DS_Store',
  'Thumbs.db',
  '.llm-project-mapper.json',
  'package-lock.json',
  'pnpm-lock.yaml',
  'yarn.lock',
  'bun.lockb',
  'Cargo.lock',
  'poetry.lock',
  'composer.lock',
]);

/**
 * Translate a simple .gitignore-style glob line into a RegExp.
 * Supports `*` and `?`; ignores negations and anchored complexity.
 * @param {string} pattern
 * @returns {RegExp}
 */
function globToRegExp(pattern) {
  const escaped = pattern
    .replace(/[.+^${}()|[\]\\]/g, '\\$&')
    .replace(/\*/g, '[^/]*')
    .replace(/\?/g, '[^/]');
  return new RegExp('(^|/)' + escaped + '(/|$)');
}

/**
 * @param {string} root absolute project root
 * @param {string[]} [extra] additional patterns to ignore
 * @returns {(relPath: string, name: string, isDir: boolean) => boolean}
 */
export function makeIgnore(root, extra = []) {
  /** @type {RegExp[]} */
  const patterns = [];

  const lines = [...extra];
  try {
    const gitignore = readFileSync(join(root, '.gitignore'), 'utf8');
    lines.push(...gitignore.split(/\r?\n/));
  } catch {
    // No .gitignore is fine.
  }

  for (let raw of lines) {
    const line = raw.trim();
    if (!line || line.startsWith('#') || line.startsWith('!')) continue;
    const cleaned = line.replace(/^\/+/, '').replace(/\/+$/, '');
    if (!cleaned) continue;
    patterns.push(globToRegExp(cleaned));
  }

  return function isIgnored(relPath, name, isDir) {
    if (isDir && DEFAULT_DIRS.has(name)) return true;
    if (!isDir && DEFAULT_FILES.has(name)) return true;
    const probe = isDir ? relPath + '/' : relPath;
    for (const re of patterns) {
      if (re.test(probe)) return true;
    }
    return false;
  };
}
