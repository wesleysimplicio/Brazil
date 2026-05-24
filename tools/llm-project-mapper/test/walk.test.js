import assert from 'node:assert/strict';
import { mkdirSync, mkdtempSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { test } from 'node:test';

import { makeIgnore } from '../src/ignore.js';
import { walkProject } from '../src/walk.js';

function fixture() {
  const dir = mkdtempSync(join(tmpdir(), 'lpm-walk-'));
  mkdirSync(join(dir, 'src'));
  mkdirSync(join(dir, 'node_modules'));
  mkdirSync(join(dir, 'node_modules', 'pkg'));
  writeFileSync(join(dir, 'src', 'a.js'), 'const a = 1;\nconst b = 2;\n');
  writeFileSync(join(dir, 'src', 'b.ts'), 'export const x = 1;\n');
  writeFileSync(join(dir, 'README.md'), '# hi\n');
  writeFileSync(join(dir, 'node_modules', 'pkg', 'index.js'), 'module.exports={};\n');
  return dir;
}

test('skips default-ignored directories', () => {
  const dir = fixture();
  const { files, metrics } = walkProject(dir, { ignore: makeIgnore(dir) });
  const paths = files.map((f) => f.path).sort();
  assert.deepEqual(paths, ['README.md', 'src/a.js', 'src/b.ts']);
  assert.equal(metrics.totalFiles, 3);
  assert.ok(!paths.some((p) => p.includes('node_modules')));
});

test('counts lines and bytes', () => {
  const dir = fixture();
  const { files } = walkProject(dir, { ignore: makeIgnore(dir) });
  const a = files.find((f) => f.path === 'src/a.js');
  assert.ok(a);
  assert.equal(a.lines, 2);
  assert.ok(a.bytes > 0);
});

test('honours extra ignore patterns', () => {
  const dir = fixture();
  const ignore = makeIgnore(dir, ['*.md']);
  const { files } = walkProject(dir, { ignore });
  assert.ok(!files.some((f) => f.path.endsWith('.md')));
});

test('builds a directory tree', () => {
  const dir = fixture();
  const { tree } = walkProject(dir, { ignore: makeIgnore(dir) });
  assert.equal(tree.type, 'dir');
  const src = tree.children?.find((c) => c.name === 'src');
  assert.ok(src);
  assert.equal(src.type, 'dir');
  assert.ok(src.children?.some((c) => c.name === 'a.js'));
});
