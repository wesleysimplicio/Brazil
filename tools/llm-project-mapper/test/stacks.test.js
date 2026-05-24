import assert from 'node:assert/strict';
import { mkdtempSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { test } from 'node:test';

import { detectStacks } from '../src/stacks.js';

/** @returns {string} */
function fixture() {
  return mkdtempSync(join(tmpdir(), 'lpm-stacks-'));
}

test('detects a node project with deps and manager', () => {
  const dir = fixture();
  writeFileSync(
    join(dir, 'package.json'),
    JSON.stringify({
      name: 'demo-app',
      description: 'a demo',
      dependencies: { react: '^18', express: '^4' },
      devDependencies: { vitest: '^1' },
      main: 'src/index.js',
    }),
  );
  writeFileSync(join(dir, 'package-lock.json'), '{}');

  const { stacks, dependencies, meta } = detectStacks(dir);
  const node = stacks.find((s) => s.kind === 'node');
  assert.ok(node);
  assert.equal(node.name, 'demo-app');
  assert.equal(node.manager, 'npm');
  assert.deepEqual(node.frameworks?.sort(), ['express', 'react', 'vitest']);

  const deps = dependencies.find((d) => d.stack === 'node');
  assert.ok(deps);
  assert.deepEqual(deps.runtime.sort(), ['express', 'react']);
  assert.deepEqual(deps.dev, ['vitest']);
  assert.equal(meta.name, 'demo-app');
  assert.equal(meta.main, 'src/index.js');
});

test('detects go modules and their requires', () => {
  const dir = fixture();
  writeFileSync(
    join(dir, 'go.mod'),
    [
      'module github.com/acme/widget',
      '',
      'go 1.22',
      '',
      'require (',
      '\tgithub.com/spf13/cobra v1.8.0',
      '\tgithub.com/stretchr/testify v1.9.0',
      ')',
      '',
    ].join('\n'),
  );

  const { stacks, dependencies } = detectStacks(dir);
  const go = stacks.find((s) => s.kind === 'go');
  assert.ok(go);
  assert.equal(go.name, 'github.com/acme/widget');
  const deps = dependencies.find((d) => d.stack === 'go');
  assert.ok(deps?.runtime.includes('github.com/spf13/cobra'));
  assert.ok(deps?.runtime.includes('github.com/stretchr/testify'));
});

test('detects cmake project name', () => {
  const dir = fixture();
  writeFileSync(
    join(dir, 'CMakeLists.txt'),
    'cmake_minimum_required(VERSION 3.27)\nproject(us4_runtime VERSION 0.1.0)\n',
  );
  const { stacks, meta } = detectStacks(dir);
  const cmake = stacks.find((s) => s.kind === 'cmake');
  assert.ok(cmake);
  assert.equal(cmake.name, 'us4_runtime');
  assert.equal(meta.name, 'us4_runtime');
});

test('parses python requirements', () => {
  const dir = fixture();
  writeFileSync(
    join(dir, 'requirements.txt'),
    '# deps\nrequests>=2.0\nflask==3.0  # web\n-e .\nnumpy\n',
  );
  const { stacks, dependencies } = detectStacks(dir);
  assert.ok(stacks.some((s) => s.kind === 'python'));
  const deps = dependencies.find((d) => d.stack === 'python');
  assert.deepEqual(deps?.runtime.sort(), ['flask', 'numpy', 'requests']);
});
