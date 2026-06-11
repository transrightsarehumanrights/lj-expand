// Validates that all LJE_SET_FUNC entries in lj_expand_lib.c are documented
// in the corresponding docs/api/*.json files, and vice versa.
// Run with: npm run validate

const fs = require('fs');
const path = require('path');

const C_FILE = path.resolve(__dirname, '../../src/lj_expand_lib.c');
const API_DIR = path.resolve(__dirname, '../api');

// JSON files that are Lua-defined and not in the C file — skip for C comparison
const LUA_ONLY = new Set(['lua']);

// --- Parse C file ---

function parseCFile(filePath) {
  const src = fs.readFileSync(filePath, 'utf8');
  const lines = src.split('\n');

  const namespaces = {}; // namespace -> string[]
  let pendingFuncs = [];  // functions collected since last LJE_NEW_SECTION
  let inSection = false;
  let currentNamespace = 'base';

  for (const line of lines) {
    const trimmed = line.trim();

    if (trimmed.startsWith('LJE_NEW_SECTION()')) {
      // Flush pending base-level functions
      if (!inSection && pendingFuncs.length > 0) {
        namespaces['base'] = (namespaces['base'] || []).concat(pendingFuncs);
        pendingFuncs = [];
      }
      inSection = true;
      pendingFuncs = [];
      continue;
    }

    const endMatch = trimmed.match(/^LJE_END_SECTION\("(\w+)"\)/);
    if (endMatch) {
      currentNamespace = endMatch[1];
      namespaces[currentNamespace] = (namespaces[currentNamespace] || []).concat(pendingFuncs);
      pendingFuncs = [];
      inSection = false;
      continue;
    }

    const funcMatch = trimmed.match(/^LJE_SET_FUNC\("([^"]+)"/);
    if (funcMatch) {
      pendingFuncs.push(funcMatch[1]);
    }
  }

  // Flush any remaining base-level functions
  if (pendingFuncs.length > 0) {
    namespaces['base'] = (namespaces['base'] || []).concat(pendingFuncs);
  }

  return namespaces;
}

// --- Load JSON docs ---

function loadJsonDocs(apiDir) {
  const files = fs.readdirSync(apiDir).filter(f => f.endsWith('.json'));
  const docs = {};

  for (const file of files) {
    const data = JSON.parse(fs.readFileSync(path.join(apiDir, file), 'utf8'));
    const ns = path.basename(file, '.json');
    // Entries marked "defined_in": "lua" live in a C namespace but are
    // implemented in the preinit Lua — exclude them from the C comparison.
    docs[ns] = (data.functions || [])
      .filter(f => f.defined_in !== 'lua')
      .map(f => f.name);
  }

  return docs;
}

// --- Compare ---

function validate(cNamespaces, jsonDocs) {
  let errors = 0;
  let warnings = 0;

  console.log('Validating API documentation against lj_expand_lib.c...\n');

  // Check every C function is documented
  for (const [ns, funcs] of Object.entries(cNamespaces)) {
    const jsonFuncs = jsonDocs[ns] || [];
    const missing = funcs.filter(f => !jsonFuncs.includes(f));

    if (missing.length > 0) {
      console.error(`[MISSING] Namespace "${ns}" — documented in C but missing from docs/api/${ns}.json:`);
      for (const f of missing) console.error(`  - ${f}`);
      errors += missing.length;
    }
  }

  // Check every documented function (excluding Lua-only) exists in C
  for (const [ns, funcs] of Object.entries(jsonDocs)) {
    if (LUA_ONLY.has(ns)) continue;

    const cFuncs = cNamespaces[ns] || [];
    const extra = funcs.filter(f => !cFuncs.includes(f));

    if (extra.length > 0) {
      console.warn(`[EXTRA] Namespace "${ns}" — in docs/api/${ns}.json but not found in C file:`);
      for (const f of extra) console.warn(`  - ${f}`);
      warnings += extra.length;
    }
  }

  // Check for entire namespaces present in C but with no JSON file
  for (const ns of Object.keys(cNamespaces)) {
    if (!jsonDocs[ns]) {
      console.error(`[MISSING] No docs/api/${ns}.json found for namespace "${ns}"`);
      errors++;
    }
  }

  // Summary
  console.log('');
  if (errors === 0 && warnings === 0) {
    console.log('All good! Every C function is documented.');
  } else {
    if (errors > 0) console.error(`${errors} missing entry/entries (need to be documented).`);
    if (warnings > 0) console.warn(`${warnings} extra entry/entries (may be stale or Lua-defined — add to LUA_ONLY if intentional).`);
    process.exit(1);
  }
}

// --- Run ---

const cNamespaces = parseCFile(C_FILE);
const jsonDocs = loadJsonDocs(API_DIR);
validate(cNamespaces, jsonDocs);
