#!/usr/bin/env node
import { statSync, writeFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { buildProjectMap, GENERATOR } from '../src/mapper.js';
import { renderMarkdown } from '../src/render.js';

const HELP = `llm-project-mapper v${GENERATOR.version}
Map any codebase into a structured .llm-project-mapper.json so AI agents
understand a project before they program it.

Usage:
  llm-project-mapper [path] [options]

Arguments:
  path                  project directory to map (default ".")

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
`;

/**
 * @param {string[]} argv
 */
function parseArgs(argv) {
  const opts = {
    path: '.',
    out: /** @type {string|null} */ (null),
    stdout: false,
    summary: false,
    maxDepth: 8,
    maxFileBytes: 2_000_000,
    /** @type {string[]} */ ignore: [],
    help: false,
    version: false,
  };
  let sawPath = false;

  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    const need = (flag) => {
      if (i + 1 >= argv.length) {
        process.stderr.write(`error: missing value for ${flag}\n`);
        process.exit(2);
      }
      return argv[++i];
    };
    switch (a) {
      case '-h':
      case '--help':
        opts.help = true;
        break;
      case '-v':
      case '--version':
        opts.version = true;
        break;
      case '-o':
      case '--out':
        opts.out = need(a);
        break;
      case '--stdout':
        opts.stdout = true;
        break;
      case '--summary':
        opts.summary = true;
        break;
      case '--max-depth':
        opts.maxDepth = Number(need(a));
        break;
      case '--max-file-bytes':
        opts.maxFileBytes = Number(need(a));
        break;
      case '--ignore':
        opts.ignore.push(need(a));
        break;
      default:
        if (a.startsWith('-')) {
          process.stderr.write(`error: unknown option: ${a}\n`);
          process.exit(2);
        }
        if (!sawPath) {
          opts.path = a;
          sawPath = true;
        } else {
          process.stderr.write(`error: unexpected argument: ${a}\n`);
          process.exit(2);
        }
    }
  }
  return opts;
}

function main() {
  const opts = parseArgs(process.argv.slice(2));
  if (opts.help) {
    process.stdout.write(HELP);
    return;
  }
  if (opts.version) {
    process.stdout.write(GENERATOR.version + '\n');
    return;
  }

  const root = resolve(opts.path);
  try {
    if (!statSync(root).isDirectory()) {
      process.stderr.write(`error: not a directory: ${root}\n`);
      process.exit(1);
    }
  } catch {
    process.stderr.write(`error: path does not exist: ${root}\n`);
    process.exit(1);
  }

  if (!Number.isFinite(opts.maxDepth) || opts.maxDepth < 0) {
    process.stderr.write('error: --max-depth must be a non-negative number\n');
    process.exit(2);
  }

  const map = buildProjectMap(root, {
    maxDepth: opts.maxDepth,
    maxFileBytes: opts.maxFileBytes,
    ignore: opts.ignore,
  });
  const jsonText = JSON.stringify(map, null, 2) + '\n';

  if (opts.summary) {
    process.stderr.write(renderMarkdown(map) + '\n');
  }

  if (opts.stdout) {
    process.stdout.write(jsonText);
    return;
  }

  const outPath = opts.out
    ? resolve(opts.out)
    : resolve(root, '.llm-project-mapper.json');
  writeFileSync(outPath, jsonText);

  const langs = map.languages
    .slice(0, 3)
    .map((l) => l.language)
    .join(', ');
  process.stdout.write(
    `Mapped "${map.name}": ${map.metrics.totalFiles} files, ` +
      `${map.metrics.totalLines} lines, ${map.stacks.length} stack(s)` +
      (langs ? `, top: ${langs}` : '') +
      `\n-> ${outPath}\n`,
  );
}

main();
