import { existsSync, readdirSync, readFileSync } from 'node:fs';
import { basename, join } from 'node:path';

/** @typedef {import('./types.js').Stack} Stack */
/** @typedef {import('./types.js').DependencyGroup} DependencyGroup */

/** @param {string} p @returns {string|null} */
function read(p) {
  try {
    return readFileSync(p, 'utf8');
  } catch {
    return null;
  }
}

/** @param {string|null} text @returns {any} */
function json(text) {
  if (text == null) return null;
  try {
    return JSON.parse(text);
  } catch {
    return null;
  }
}

const FRAMEWORK_HINTS = [
  ['react', 'react'],
  ['next', 'next'],
  ['vue', 'vue'],
  ['nuxt', 'nuxt'],
  ['svelte', 'svelte'],
  ['@angular/core', 'angular'],
  ['express', 'express'],
  ['fastify', 'fastify'],
  ['koa', 'koa'],
  ['@nestjs/core', 'nestjs'],
  ['electron', 'electron'],
  ['vite', 'vite'],
  ['webpack', 'webpack'],
  ['jest', 'jest'],
  ['vitest', 'vitest'],
  ['@playwright/test', 'playwright'],
  ['playwright', 'playwright'],
];

/**
 * @param {Record<string, unknown>} deps
 * @returns {string[]}
 */
function detectFrameworks(deps) {
  const out = [];
  for (const [key, label] of FRAMEWORK_HINTS) {
    if (key in deps && !out.includes(label)) out.push(label);
  }
  return out;
}

/** @param {string} root @returns {string} */
function detectNodeManager(root) {
  if (existsSync(join(root, 'pnpm-lock.yaml'))) return 'pnpm';
  if (existsSync(join(root, 'yarn.lock'))) return 'yarn';
  if (existsSync(join(root, 'bun.lockb'))) return 'bun';
  return 'npm';
}

/** Strip a requirements.txt line down to the bare package name. */
function reqName(line) {
  const noComment = line.split('#')[0].trim();
  if (!noComment || noComment.startsWith('-')) return '';
  const noMarker = noComment.split(';')[0].trim();
  const m = noMarker.match(/^[A-Za-z0-9._-]+/);
  return m ? m[0] : '';
}

/** Extract keys from a TOML section like [dependencies]. */
function tomlSectionKeys(text, section) {
  const lines = text.split(/\r?\n/);
  const keys = [];
  let inSection = false;
  for (const raw of lines) {
    const line = raw.trim();
    if (line.startsWith('[')) {
      inSection = line === '[' + section + ']';
      continue;
    }
    if (inSection) {
      const m = line.match(/^([A-Za-z0-9_.-]+)\s*=/);
      if (m) keys.push(m[1]);
    }
  }
  return keys;
}

/** Extract a top-level scalar like `name = "x"` from TOML/simple text. */
function scalar(text, key) {
  const re = new RegExp('^\\s*' + key + '\\s*=\\s*"([^"]*)"', 'm');
  const m = text.match(re);
  return m ? m[1] : undefined;
}

/** Extract quoted strings inside `dependencies = [ ... ]`. */
function arrayStrings(text, key) {
  const re = new RegExp(key + '\\s*=\\s*\\[([\\s\\S]*?)\\]');
  const block = text.match(re);
  if (!block) return [];
  return [...block[1].matchAll(/"([^"]+)"/g)].map((m) => m[1]);
}

/**
 * Inspect a project root and report detected stacks plus their dependencies.
 * @param {string} root
 * @returns {{stacks: Stack[], dependencies: DependencyGroup[],
 *            meta: {name?: string, description?: string,
 *                   main?: string, bins?: string[]}}}
 */
export function detectStacks(root) {
  /** @type {Stack[]} */
  const stacks = [];
  /** @type {DependencyGroup[]} */
  const dependencies = [];
  /** @type {{name?: string, description?: string, main?: string, bins?: string[]}} */
  const meta = {};

  let entries = [];
  try {
    entries = readdirSync(root);
  } catch {
    entries = [];
  }
  const has = (f) => entries.includes(f);

  // Node / TypeScript
  if (has('package.json')) {
    const pkg = json(read(join(root, 'package.json'))) || {};
    const runtime = Object.keys(pkg.dependencies || {});
    const dev = Object.keys(pkg.devDependencies || {});
    stacks.push({
      kind: 'node',
      marker: 'package.json',
      name: pkg.name,
      manager: detectNodeManager(root),
      frameworks: detectFrameworks({ ...pkg.dependencies, ...pkg.devDependencies }),
    });
    dependencies.push({ stack: 'node', source: 'package.json', runtime, dev });
    if (pkg.name) meta.name = pkg.name;
    if (pkg.description) meta.description = pkg.description;
    if (typeof pkg.main === 'string') meta.main = pkg.main;
    if (pkg.bin) {
      meta.bins =
        typeof pkg.bin === 'string'
          ? [pkg.bin]
          : Object.values(pkg.bin).filter((v) => typeof v === 'string');
    }
  }

  // Python
  if (has('pyproject.toml')) {
    const text = read(join(root, 'pyproject.toml')) || '';
    const runtime = [
      ...arrayStrings(text, 'dependencies').map(reqName).filter(Boolean),
      ...tomlSectionKeys(text, 'tool.poetry.dependencies').filter(
        (k) => k !== 'python',
      ),
    ];
    stacks.push({
      kind: 'python',
      marker: 'pyproject.toml',
      name: scalar(text, 'name'),
      manager: text.includes('[tool.poetry]') ? 'poetry' : 'pip',
    });
    dependencies.push({
      stack: 'python',
      source: 'pyproject.toml',
      runtime,
      dev: [],
    });
    if (!meta.name) meta.name = scalar(text, 'name');
  } else if (has('requirements.txt')) {
    const text = read(join(root, 'requirements.txt')) || '';
    const runtime = text.split(/\r?\n/).map(reqName).filter(Boolean);
    stacks.push({ kind: 'python', marker: 'requirements.txt', manager: 'pip' });
    dependencies.push({
      stack: 'python',
      source: 'requirements.txt',
      runtime,
      dev: [],
    });
  } else if (has('setup.py')) {
    stacks.push({ kind: 'python', marker: 'setup.py', manager: 'pip' });
  }

  // Go
  if (has('go.mod')) {
    const text = read(join(root, 'go.mod')) || '';
    const modName = text.match(/^module\s+(\S+)/m);
    const runtime = [];
    const block = text.match(/require\s*\(([\s\S]*?)\)/);
    if (block) {
      for (const line of block[1].split(/\r?\n/)) {
        const m = line.trim().match(/^(\S+)\s+v\S+/);
        if (m) runtime.push(m[1]);
      }
    }
    for (const m of text.matchAll(/^require\s+(\S+)\s+v\S+/gm)) {
      runtime.push(m[1]);
    }
    stacks.push({
      kind: 'go',
      marker: 'go.mod',
      name: modName ? modName[1] : undefined,
      manager: 'go',
    });
    dependencies.push({ stack: 'go', source: 'go.mod', runtime, dev: [] });
  }

  // Rust
  if (has('Cargo.toml')) {
    const text = read(join(root, 'Cargo.toml')) || '';
    stacks.push({
      kind: 'rust',
      marker: 'Cargo.toml',
      name: scalar(text, 'name'),
      manager: 'cargo',
    });
    dependencies.push({
      stack: 'rust',
      source: 'Cargo.toml',
      runtime: tomlSectionKeys(text, 'dependencies'),
      dev: tomlSectionKeys(text, 'dev-dependencies'),
    });
  }

  // PHP
  if (has('composer.json')) {
    const c = json(read(join(root, 'composer.json'))) || {};
    stacks.push({
      kind: 'php',
      marker: 'composer.json',
      name: c.name,
      manager: 'composer',
    });
    dependencies.push({
      stack: 'php',
      source: 'composer.json',
      runtime: Object.keys(c.require || {}),
      dev: Object.keys(c['require-dev'] || {}),
    });
  }

  // C / C++ (CMake)
  if (has('CMakeLists.txt')) {
    const text = read(join(root, 'CMakeLists.txt')) || '';
    const m = text.match(/project\s*\(\s*([A-Za-z0-9_.-]+)/);
    stacks.push({
      kind: 'cmake',
      marker: 'CMakeLists.txt',
      name: m ? m[1] : undefined,
      manager: 'cmake',
    });
    if (!meta.name && m) meta.name = m[1];
  }

  // Presence-only detections for ecosystems we don't deep-parse yet.
  /** @type {Array<[string, string, string]>} */
  const simple = [
    ['Gemfile', 'ruby', 'bundler'],
    ['pom.xml', 'jvm', 'maven'],
    ['build.gradle', 'jvm', 'gradle'],
    ['build.gradle.kts', 'jvm', 'gradle'],
    ['pubspec.yaml', 'dart', 'pub'],
    ['mix.exs', 'elixir', 'mix'],
    ['Package.swift', 'swift', 'spm'],
  ];
  for (const [file, kind, manager] of simple) {
    if (has(file)) stacks.push({ kind, marker: file, manager });
  }

  // .NET projects use globbed marker files.
  const csproj = entries.find((e) => e.endsWith('.csproj'));
  const sln = entries.find((e) => e.endsWith('.sln'));
  if (csproj || sln) {
    const marker = csproj || /** @type {string} */ (sln);
    stacks.push({
      kind: 'dotnet',
      marker,
      name: basename(marker).replace(/\.(csproj|sln)$/, ''),
      manager: 'dotnet',
    });
  }

  return { stacks, dependencies, meta };
}
