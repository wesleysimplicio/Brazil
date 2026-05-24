import assert from 'node:assert/strict';
import { mkdirSync, mkdtempSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { test } from 'node:test';

import { buildProjectMap, SCHEMA_VERSION } from '../src/mapper.js';
import { renderMarkdown } from '../src/render.js';

function fixture() {
  const dir = mkdtempSync(join(tmpdir(), 'lpm-map-'));
  mkdirSync(join(dir, 'src'));
  writeFileSync(
    join(dir, 'package.json'),
    JSON.stringify({
      name: 'sample-project',
      description: 'fixture',
      dependencies: { express: '^4' },
      main: 'src/index.js',
    }),
  );
  writeFileSync(join(dir, 'src', 'index.js'), 'console.log("hi");\n');
  writeFileSync(join(dir, 'src', 'util.py'), 'def f():\n    return 1\n');
  return dir;
}

test('builds a complete, serializable project map', () => {
  const dir = fixture();
  const map = buildProjectMap(dir);

  assert.equal(map.schema, SCHEMA_VERSION);
  assert.equal(map.name, 'sample-project');
  assert.equal(map.description, 'fixture');
  assert.ok(map.stacks.some((s) => s.kind === 'node'));
  assert.ok(map.languages.some((l) => l.language === 'JavaScript'));
  assert.ok(map.languages.some((l) => l.language === 'Python'));
  assert.ok(map.entryPoints.includes('src/index.js'));
  assert.ok(map.metrics.totalFiles >= 3);
  assert.equal(typeof map.generatedAt, 'string');

  // Must round-trip through JSON without throwing or losing the schema.
  const round = JSON.parse(JSON.stringify(map));
  assert.equal(round.schema, SCHEMA_VERSION);
});

test('detects entry points from package.json main', () => {
  const dir = fixture();
  const map = buildProjectMap(dir);
  assert.ok(map.entryPoints.includes('src/index.js'));
});

test('renders a markdown summary', () => {
  const dir = fixture();
  const md = renderMarkdown(buildProjectMap(dir));
  assert.match(md, /# Project Map: sample-project/);
  assert.match(md, /## Languages/);
  assert.match(md, /## Stacks/);
  assert.match(md, /express/);
});
