import { readdirSync, readFileSync, statSync } from 'node:fs';
import { extname, join } from 'node:path';

import { languageFor } from './languages.js';

/** @typedef {import('./types.js').FileEntry} FileEntry */
/** @typedef {import('./types.js').TreeNode} TreeNode */

const NULL_BYTE = 0;

/**
 * @param {Buffer} buf
 * @returns {boolean} true if the buffer looks like binary content
 */
function looksBinary(buf) {
  const limit = Math.min(buf.length, 8000);
  for (let i = 0; i < limit; i++) {
    if (buf[i] === NULL_BYTE) return true;
  }
  return false;
}

/**
 * @param {Buffer} buf
 * @returns {number}
 */
function countLines(buf) {
  if (buf.length === 0) return 0;
  let lines = 0;
  for (let i = 0; i < buf.length; i++) {
    if (buf[i] === 0x0a) lines++;
  }
  if (buf[buf.length - 1] !== 0x0a) lines++;
  return lines;
}

/**
 * Recursively walk a project tree, honouring an ignore predicate.
 *
 * @param {string} root absolute project root
 * @param {Object} [options]
 * @param {(relPath: string, name: string, isDir: boolean) => boolean} [options.ignore]
 * @param {number} [options.maxDepth] tree depth retained in the structure
 * @param {number} [options.maxFileBytes] skip line counting above this size
 * @returns {{tree: TreeNode, files: FileEntry[],
 *            metrics: {totalFiles: number, totalDirs: number,
 *                      totalLines: number, totalBytes: number,
 *                      maxDepth: number}}}
 */
export function walkProject(root, options = {}) {
  const ignore = options.ignore ?? (() => false);
  const maxDepth = options.maxDepth ?? 8;
  const maxFileBytes = options.maxFileBytes ?? 2_000_000;

  /** @type {FileEntry[]} */
  const files = [];
  const metrics = {
    totalFiles: 0,
    totalDirs: 0,
    totalLines: 0,
    totalBytes: 0,
    maxDepth: 0,
  };

  /**
   * @param {string} absDir
   * @param {string} relDir
   * @param {number} depth
   * @returns {TreeNode}
   */
  function visit(absDir, relDir, depth) {
    metrics.maxDepth = Math.max(metrics.maxDepth, depth);
    /** @type {TreeNode} */
    const node = {
      name: relDir === '' ? '.' : relDir.split('/').pop() || relDir,
      type: 'dir',
      children: [],
    };

    let names = [];
    try {
      names = readdirSync(absDir).sort((a, b) => a.localeCompare(b));
    } catch {
      return node;
    }

    for (const name of names) {
      const abs = join(absDir, name);
      const rel = relDir === '' ? name : relDir + '/' + name;
      let st;
      try {
        st = statSync(abs);
      } catch {
        continue;
      }
      const isDir = st.isDirectory();
      if (ignore(rel, name, isDir)) continue;

      if (isDir) {
        metrics.totalDirs++;
        const child = visit(abs, rel, depth + 1);
        if (depth + 1 > maxDepth) {
          node.children?.push({ name, type: 'dir', truncated: true });
        } else {
          node.children?.push(child);
        }
      } else if (st.isFile()) {
        const ext = extname(name).toLowerCase();
        const language = languageFor(name, ext);
        const bytes = st.size;
        /** @type {number|null} */
        let lines = null;
        if (bytes <= maxFileBytes) {
          try {
            const buf = readFileSync(abs);
            if (!looksBinary(buf)) lines = countLines(buf);
          } catch {
            lines = null;
          }
        }
        metrics.totalFiles++;
        metrics.totalBytes += bytes;
        if (lines != null) metrics.totalLines += lines;
        files.push({ path: rel, language, bytes, lines });
        if (depth + 1 <= maxDepth) {
          node.children?.push({ name, type: 'file', language, bytes, lines });
        }
      }
    }
    return node;
  }

  const tree = visit(root, '', 0);
  return { tree, files, metrics };
}
