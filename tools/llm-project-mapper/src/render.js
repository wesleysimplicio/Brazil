/** @typedef {import('./types.js').ProjectMap} ProjectMap */

/**
 * @param {string[]} items
 * @param {number} cap
 * @returns {string}
 */
function listCapped(items, cap = 30) {
  if (items.length === 0) return '_none_';
  const shown = items.slice(0, cap).join(', ');
  const extra = items.length - cap;
  return extra > 0 ? `${shown} _(+${extra} more)_` : shown;
}

/**
 * Render a human-readable Markdown summary of a project map.
 * @param {ProjectMap} map
 * @returns {string}
 */
export function renderMarkdown(map) {
  const lines = [];
  lines.push(`# Project Map: ${map.name}`);
  if (map.description) lines.push('', map.description);
  lines.push('');
  lines.push(`- Generated: ${map.generatedAt}`);
  lines.push(`- Root: ${map.root}`);
  lines.push(`- Schema: ${map.schema}`);

  lines.push('', '## Stacks');
  if (map.stacks.length === 0) {
    lines.push('_No known stack markers found._');
  } else {
    for (const s of map.stacks) {
      const bits = [];
      if (s.name) bits.push(`name: ${s.name}`);
      if (s.manager) bits.push(`manager: ${s.manager}`);
      if (s.frameworks && s.frameworks.length) {
        bits.push(`frameworks: ${s.frameworks.join(', ')}`);
      }
      const detail = bits.length ? ` — ${bits.join(', ')}` : '';
      lines.push(`- **${s.kind}** (${s.marker})${detail}`);
    }
  }

  lines.push('', '## Languages');
  if (map.languages.length === 0) {
    lines.push('_No files analyzed._');
  } else {
    lines.push('| Language | Files | Lines | Bytes |');
    lines.push('| --- | ---: | ---: | ---: |');
    for (const l of map.languages.slice(0, 12)) {
      lines.push(`| ${l.language} | ${l.files} | ${l.lines} | ${l.bytes} |`);
    }
  }

  lines.push('', '## Dependencies');
  if (map.dependencies.length === 0) {
    lines.push('_No dependency manifests parsed._');
  } else {
    for (const d of map.dependencies) {
      lines.push('', `### ${d.stack} (${d.source})`);
      lines.push(`- runtime (${d.runtime.length}): ${listCapped(d.runtime)}`);
      if (d.dev.length) {
        lines.push(`- dev (${d.dev.length}): ${listCapped(d.dev)}`);
      }
    }
  }

  lines.push('', '## Entry points');
  lines.push(map.entryPoints.length ? '' : '_None detected._');
  for (const e of map.entryPoints) lines.push(`- ${e}`);

  lines.push('', '## Metrics');
  lines.push(`- files: ${map.metrics.totalFiles}`);
  lines.push(`- directories: ${map.metrics.totalDirs}`);
  lines.push(`- lines of code: ${map.metrics.totalLines}`);
  lines.push(`- bytes: ${map.metrics.totalBytes}`);
  lines.push(`- max depth: ${map.metrics.maxDepth}`);

  return lines.join('\n') + '\n';
}
