import assert from 'node:assert/strict';
import { test } from 'node:test';

import { isDataLanguage, languageFor } from '../src/languages.js';

test('maps common source extensions', () => {
  assert.equal(languageFor('main.cpp', '.cpp'), 'C++');
  assert.equal(languageFor('runtime.hpp', '.hpp'), 'C++');
  assert.equal(languageFor('index.ts', '.ts'), 'TypeScript');
  assert.equal(languageFor('cli.js', '.js'), 'JavaScript');
  assert.equal(languageFor('main.py', '.py'), 'Python');
  assert.equal(languageFor('main.go', '.go'), 'Go');
  assert.equal(languageFor('lib.rs', '.rs'), 'Rust');
});

test('maps special filenames', () => {
  assert.equal(languageFor('CMakeLists.txt', '.txt'), 'CMake');
  assert.equal(languageFor('Dockerfile', ''), 'Dockerfile');
  assert.equal(languageFor('go.mod', '.mod'), 'Go Module');
});

test('unknown extensions fall back to Other', () => {
  assert.equal(languageFor('mystery.qwop', '.qwop'), 'Other');
});

test('classifies data languages', () => {
  assert.equal(isDataLanguage('JSON'), true);
  assert.equal(isDataLanguage('TypeScript'), false);
});
