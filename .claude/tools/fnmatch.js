// Find the F2 counterpart of F1 engine functions by BODY FINGERPRINT.
// Both CE trees are clang-formatted (signature on one line, '{' next line, '}' at column 0),
// and both decompile the same Interplay code, so constants, strings, enum names and field
// accesses survive the renaming. usage: node fnmatch.js <names.txt|all.json> <out.json>
const fs = require('fs');
const path = require('path');
const F1 = path.join(__dirname, 'f1up/src');        // upstream F1, no edits of ours
const F2 = 'C:/Users/MS/Documents/GameDev/fallout2-ce/src';

function walk(d, out = []) {
  for (const e of fs.readdirSync(d, { withFileTypes: true })) {
    const p = path.join(d, e.name);
    if (e.isDirectory()) walk(p, out); else if (/\.cc$/.test(e.name)) out.push(p);
  }
  return out;
}
const strip = s => s.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/.*$/gm, '');
const sigRe = /^(?!static_assert|return|if|for|while|switch|else|namespace)((?:static\s+|inline\s+|const\s+|unsigned\s+|signed\s+|struct\s+)*[A-Za-z_][\w:<>]*[\s\*&]+)([A-Za-z_]\w*)\(([^;{]*)\)\s*$/;

function parse(root) {
  const fns = [];
  for (const f of walk(root)) {
    const lines = fs.readFileSync(f, 'utf8').split(/\r?\n/);
    for (let i = 0; i < lines.length - 1; i++) {
      const m = lines[i].match(sigRe);
      if (!m || lines[i + 1] !== '{') continue;
      let j = i + 2;
      while (j < lines.length && lines[j] !== '}') j++;
      const body = strip(lines.slice(i + 2, j).join('\n'));
      const params = m[3].trim() === '' || m[3].trim() === 'void' ? [] : m[3].split(',');
      fns.push({ name: m[2], ret: m[1].replace(/\bstatic\b|\binline\b/g, '').replace(/\s+/g, '').trim(), np: params.length,
        ptypes: params.map(p => p.trim().replace(/\s*\b[A-Za-z_]\w*\s*$/, '').replace(/\s+/g, '')), file: path.basename(f), body, lines: j - i });
      i = j;
    }
  }
  return fns;
}
function features(fn) {
  const t = fn.body;
  const fe = new Map();
  const add = (k, w = 1) => fe.set(k, (fe.get(k) || 0) + w);
  for (const m of t.matchAll(/"((?:\\.|[^"\\])*)"/g)) add('s:' + m[1]);
  const noStr = t.replace(/"(?:\\.|[^"\\])*"/g, '""');
  for (const m of noStr.matchAll(/(?<![\w.])(0x[0-9A-Fa-f]+|\d+(?:\.\d+)?)/g)) if (!['0', '1'].includes(m[1])) add('n:' + parseInt(m[1]));
  for (const m of noStr.matchAll(/\b([A-Z][A-Z0-9_]{2,})\b/g)) add('k:' + m[1]);
  for (const m of noStr.matchAll(/(?:->|\.)\s*([A-Za-z_]\w*)/g)) add('f:' + m[1]);
  for (const m of noStr.matchAll(/\b(if|for|while|switch|return|case)\b/g)) add('c:' + m[1]);
  return fe;
}
const f1 = parse(F1), f2 = parse(F2);
const df = new Map();
for (const fn of [...f1, ...f2]) { fn.fe = features(fn); for (const k of fn.fe.keys()) df.set(k, (df.get(k) || 0) + 1); }
const N = f1.length + f2.length;
const idf = k => Math.log(N / (df.get(k) || 1));
function sim(a, b) {
  let inter = 0, uni = 0;
  const keys = new Set([...a.fe.keys(), ...b.fe.keys()]);
  for (const k of keys) {
    const w = idf(k), x = a.fe.get(k) || 0, y = b.fe.get(k) || 0;
    inter += Math.min(x, y) * w; uni += Math.max(x, y) * w;
  }
  let s = uni ? inter / uni : 0;
  if (a.np !== b.np) s *= 0.5;
  if (a.ret !== b.ret) s *= 0.85;
  const lr = Math.min(a.lines, b.lines) / Math.max(a.lines, b.lines);
  return s * (0.7 + 0.3 * lr);
}
const [inp, out] = process.argv.slice(2);
let wanted;
if (inp.endsWith('.json')) { const j = JSON.parse(fs.readFileSync(inp)); wanted = [...j.A_map.map(x => x[0]), ...j.A_underscore]; }
else wanted = fs.readFileSync(inp, 'utf8').split(/\s+/).filter(Boolean);
// Name likeness: share of F1 name words that start some F2 name word (obj ~ object, dist ~ distance).
const ABBR = { obj: 'object', w: 'weapon', inven: 'inventory', intface: 'interface', gsound: 'sound', gmouse: 'mouse', win: 'window', num: 'number', pid: 'pid', fid: 'fid', ptr: 'pointer', ar: 'armor', d: 'drug', text: 'font', db: 'file', gdialog: 'dialog', scr: 'script', anim: 'animation', register: 'reg', combatai: 'ai', proto: 'proto', stat: 'stat', curr: 'current', mp: 'action', elev: 'elevation', exps: 'experience' };
const words1 = n => n.replace(/^_+/, '').split('_').filter(Boolean).map(w => w.toLowerCase());
const words2 = n => n.replace(/^_+/, '').replace(/([a-z0-9])([A-Z])/g, '$1_$2').split('_').filter(Boolean).map(w => w.toLowerCase());
function nameSim(n1, n2) {
  const a = words1(n1), b = words2(n2);
  if (!a.length || !b.length) return 0;
  let hit = 0;
  for (const w of a) {
    const full = ABBR[w] || w;
    if (b.some(x => x.startsWith(w) || x.startsWith(full) || full.startsWith(x) && x.length >= 3)) hit++;
  }
  return hit / Math.max(a.length, Math.min(b.length, a.length + 1));
}
const byName1 = new Map(f1.map(f => [f.name, f]));
const res = {};
for (const w of wanted) {
  const a = byName1.get(w);
  if (!a) { res[w] = { kind: 'not-a-function' }; continue; }
  const scored = f2.map(b => [b, 0.65 * sim(a, b) + 0.35 * nameSim(a.name, b.name)]).sort((x, y) => y[1] - x[1]).slice(0, 3);
  res[w] = { f1file: a.file, np: a.np, cands: scored.map(([b, s]) => ({ name: b.name, file: b.file, np: b.np, s: +s.toFixed(3) })) };
}
fs.writeFileSync(out, JSON.stringify(res, null, 1));
let hi = 0, mid = 0, lo = 0, nf = 0;
for (const [k, v] of Object.entries(res)) {
  if (!v.cands) { nf++; continue; }
  const [c1, c2] = v.cands; const margin = c1.s - (c2 ? c2.s : 0);
  if (c1.s >= 0.45 && margin >= 0.1) hi++; else if (c1.s >= 0.25) mid++; else lo++;
}
console.log(`f1 fns ${f1.length}, f2 fns ${f2.length}; wanted ${wanted.length}: high ${hi}, mid ${mid}, low ${lo}, non-functions ${nf}`);
// compact table for review
const lines = [];
for (const [k, v] of Object.entries(res)) lines.push(v.cands ? `${k} [${v.f1file}/${v.np}] => ${v.cands.map(c => `${c.name}[${c.file}/${c.np}]${c.s}`).join(' | ')}` : `${k} => ${v.kind}`);
fs.writeFileSync(out.replace(/\.json$/, '.txt'), lines.join('\n'));
