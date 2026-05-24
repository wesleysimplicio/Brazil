import { basename, resolve } from 'node:path';

import { makeIgnore } from './ignore.js';
import { detectStacks } from './stacks.js';
import { walkProject } from './walk.js';

/** @typedef {import('./types.js').ProjectMap} ProjectMap */
/** @typedef {import('./types.js').FileEntry} FileEntry */
/** @typedef {import('./types.js').LanguageStat} LanguageStat */

export const SCHEMA_VERSION = 'llm-project-mapper/v1';
export const GENERATOR = {
  name: '@wesleysimplicio/llm-project-mapper',
  version: '0.1.0',
};

const ENTRY_PATTERNS = [
  /^(src\/)?index\.(js|mjs|cjs|ts|tsx)$/,
  /^(src\/)?main\.(ts|js|py|go|rs|cpp|cc)$/,
  /(^|\/)main\.(go|rs|cpp|cc)$/,
  /^(app|manage|wsgi|asgi)\.py$/,
  /(^|\/)__main__\.py$/,
  /^bin\/.+\.(js|mjs|cjs)$/,
];

/**
 * @param {FileEntry[]} files
 * @returns {LanguageStat[]}
 */
function aggregateLanguages(files) {
  /** @type {Map<string, LanguageStat>} */
  const byLang = new Map();
  for (const f of files) {
    let stat = byLang.get(f.language);
    if (!stat) {
      stat = { language: f.language, files: 0, lines: 0, bytes: 0 };
      byLang.set(f.language, stat);
    }
    stat.files++;
    stat.bytes += f.bytes;
    stat.lines += f.lines ?? 0;
  }
  return [...byLang.values()].sort(
    (a, b) => b.lines - a.lines || b.bytes - a.bytes || a.language.localeCompare(b.language),
  );
}

/**
 * @param {FileEntry[]} files
 * @param {{main?: string, bins?: string[]}} meta
 * @returns {string[]}
 */
function detectEntryPoints(files, meta) {
  /** @type {Set<string>} */
  const out = new Set();
  const norm = (p) => p.replace(/^\.\//, '');
  if (meta.main) out.add(norm(meta.main));
  for (const b of meta.bins ?? []) out.add(norm(b));

  for (const f of files) {
    if (ENTRY_PATTERNS.some((re) => re.test(f.path))) out.add(f.path);
  }
  return [...out].sort().slice(0, 25);
}

/**
 * Analyze a project directory and produce a structured map.
 *
 * @param {string} rootInput path to the project (relative or absolute)
 * @param {Object} [options]
 * @param {number} [options.maxDepth]
 * @param {number} [options.maxFileBytes]
 * @param {string[]} [options.ignore] extra ignore patterns
 * @returns {ProjectMap}
 */
export function buildProjectMap(rootInput, options = {}) {
  const root = resolve(rootInput);
  const ignore = makeIgnore(root, options.ignore ?? []);
  const { stacks, dependencies, meta } = detectStacks(root);
  const { tree, files, metrics } = walkProject(root, {
    ignore,
    maxDepth: options.maxDepth,
    maxFileBytes: options.maxFileBytes,
  });

  const languages = aggregateLanguages(files);
  const entryPoints = detectEntryPoints(files, meta);

  /** @type {ProjectMap} */
  const map = {
    schema: SCHEMA_VERSION,
    generatedAt: new Date().toISOString(),
    generator: GENERATOR,
    root,
    name: meta.name || basename(root),
    stacks,
    languages,
    dependencies,
    entryPoints,
    metrics,
    structure: tree,
  };
  if (meta.description) map.description = meta.description;
  return map;
}
