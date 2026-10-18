// Classify every identifier our new F1 modules use:
//   A = upstream F1 engine symbol (exists at merge-base 0609bcf) -> needs an F2 name
//   B = symbol WE added inside a vanilla F1 file -> port that addition or stub it
//   C = symbol of another of our new modules -> comes with that module (or stub in phase 1)
// usage: node symmap2.js <out.json> module.cc [module.cc ...]
const fs = require('fs');
const path = require('path');
const SP = __dirname;
const F1 = 'C:/Users/MS/Documents/GameDev/fallout1-ce/src';
const F1UP = path.join(SP, 'f1up/src');
const F2 = 'C:/Users/MS/Documents/GameDev/fallout2-ce/src';

function walk(d, out = []) {
  for (const e of fs.readdirSync(d, { withFileTypes: true })) {
    const p = path.join(d, e.name);
    if (e.isDirectory()) walk(p, out); else if (/\.(h|cc)$/.test(e.name)) out.push(p);
  }
  return out;
}
const rel = (root, f) => path.relative(root, f).replace(/\\/g, '/');
const strip = s => s.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/.*$/gm, '').replace(/"(?:\\.|[^"\\])*"/g, '""').replace(/'(?:\\.|[^'\\])'/g, "''");

// Declarations: function names in headers, extern globals, #defines, enum constants, struct/typedef names.
function declsOf(text) {
  const t = strip(text);
  const s = new Set();
  for (const m of t.matchAll(/\b([A-Za-z_]\w*)\s*\(/g)) s.add(m[1]);
  for (const m of t.matchAll(/extern\s+[^;(]*?\b([A-Za-z_]\w*)\s*(?:\[[^\]]*\])*\s*;/g)) s.add(m[1]);
  for (const m of t.matchAll(/#define\s+([A-Za-z_]\w*)/g)) s.add(m[1]);
  for (const m of t.matchAll(/\b(?:struct|class|enum|union)\s+([A-Za-z_]\w*)/g)) s.add(m[1]);
  for (const m of t.matchAll(/typedef\s[^;]*?\b([A-Za-z_]\w*)\s*;/g)) s.add(m[1]);
  // enum bodies
  for (const m of t.matchAll(/enum\s*\w*\s*(?::\s*\w+\s*)?\{([^}]*)\}/g))
    for (const e of m[1].matchAll(/\b([A-Za-z_]\w*)\s*(?:=[^,]*)?(?:,|$)/g)) s.add(e[1]);
  return s;
}

const f1Files = walk(F1).map(f => rel(F1, f));
const upFiles = new Set(walk(F1UP).map(f => rel(F1UP, f)));
const newFiles = f1Files.filter(f => !upFiles.has(f));

const upDecl = new Set();
for (const f of walk(F1UP)) if (f.endsWith('.h')) for (const d of declsOf(fs.readFileSync(f, 'utf8'))) upDecl.add(d);
const curDecl = new Map(); // name -> header that declares it now (vanilla headers only)
for (const f of walk(F1)) {
  const r = rel(F1, f);
  if (!f.endsWith('.h') || newFiles.includes(r)) continue;
  for (const d of declsOf(fs.readFileSync(f, 'utf8'))) if (!curDecl.has(d)) curDecl.set(d, r);
}
const newDecl = new Map();
for (const r of newFiles) if (r.endsWith('.h')) for (const d of declsOf(fs.readFileSync(path.join(F1, r), 'utf8'))) newDecl.set(d, r);

const f2Ids = new Set();
for (const f of walk(F2)) for (const m of strip(fs.readFileSync(f, 'utf8')).matchAll(/\b[A-Za-z_]\w*\b/g)) f2Ids.add(m[0]);

const [out, ...mods] = process.argv.slice(2);
const own = new Set();
for (const m of mods) for (const d of declsOf(fs.readFileSync(path.join(F1, m.replace(/\.cc$/, '.h')), 'utf8') + '')) own.add(d);
const used = new Map();
for (const m of mods) {
  const t = strip(fs.readFileSync(path.join(F1, m), 'utf8'));
  for (const x of t.matchAll(/\b([A-Za-z_]\w*)\b/g)) {
    const id = x[1];
    used.set(id, (used.get(id) || 0) + 1);
  }
}
const res = { A_same: [], A_underscore: [], A_map: [], B: {}, C: {} };
for (const [id, c] of used) {
  if (own.has(id) && !upDecl.has(id)) continue;
  if (upDecl.has(id)) {
    if (f2Ids.has(id)) res.A_same.push(id);
    else if (f2Ids.has('_' + id)) res.A_underscore.push(id);
    else res.A_map.push([id, c]);
  } else if (curDecl.has(id)) {
    (res.B[curDecl.get(id)] ||= []).push(id);
  } else if (newDecl.has(id)) {
    (res.C[newDecl.get(id)] ||= []).push(id);
  }
}
res.A_map.sort((a, b) => b[1] - a[1]);
fs.writeFileSync(out, JSON.stringify(res, null, 1));
console.log('new files:', newFiles.join(' '));
console.log(`A same=${res.A_same.length} underscore=${res.A_underscore.length} needMap=${res.A_map.length}`);
console.log('B (our additions in vanilla files):'); for (const [h, l] of Object.entries(res.B)) console.log(`  ${h}: ${l.length} ${l.slice(0, 25).join(' ')}`);
console.log('C (other new modules):'); for (const [h, l] of Object.entries(res.C)) console.log(`  ${h}: ${l.length}`);
