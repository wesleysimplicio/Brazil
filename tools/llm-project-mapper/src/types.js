// JSDoc type definitions for the project map schema. No runtime exports.

/**
 * @typedef {Object} Stack
 * @property {string} kind        Stack identifier, e.g. "node", "python", "cmake".
 * @property {string} marker      File that triggered detection, e.g. "package.json".
 * @property {string} [name]      Project/module name parsed from the marker.
 * @property {string} [manager]   Package/build manager, e.g. "npm", "cargo".
 * @property {string[]} [frameworks] Detected frameworks, e.g. ["react", "next"].
 */

/**
 * @typedef {Object} DependencyGroup
 * @property {string} stack       Stack kind the deps belong to.
 * @property {string} source      Marker file the deps were parsed from.
 * @property {string[]} runtime   Runtime/production dependencies.
 * @property {string[]} dev       Dev/build dependencies.
 */

/**
 * @typedef {Object} LanguageStat
 * @property {string} language
 * @property {number} files
 * @property {number} lines
 * @property {number} bytes
 */

/**
 * @typedef {Object} FileEntry
 * @property {string} path        Path relative to the project root (POSIX style).
 * @property {string} language
 * @property {number} bytes
 * @property {number|null} lines  null when not counted (binary or too large).
 */

/**
 * @typedef {Object} TreeNode
 * @property {string} name
 * @property {"dir"|"file"} type
 * @property {string} [language]
 * @property {number} [bytes]
 * @property {number|null} [lines]
 * @property {TreeNode[]} [children]
 * @property {boolean} [truncated] True when children were cut off by max depth.
 */

/**
 * @typedef {Object} ProjectMetrics
 * @property {number} totalFiles
 * @property {number} totalDirs
 * @property {number} totalLines
 * @property {number} totalBytes
 * @property {number} maxDepth
 */

/**
 * @typedef {Object} ProjectMap
 * @property {string} schema      Schema version identifier.
 * @property {string} generatedAt ISO timestamp.
 * @property {{name: string, version: string}} generator
 * @property {string} root        Resolved root path that was scanned.
 * @property {string} name        Best-guess project name.
 * @property {string} [description]
 * @property {Stack[]} stacks
 * @property {LanguageStat[]} languages
 * @property {DependencyGroup[]} dependencies
 * @property {string[]} entryPoints
 * @property {ProjectMetrics} metrics
 * @property {TreeNode} structure
 */

export {};
