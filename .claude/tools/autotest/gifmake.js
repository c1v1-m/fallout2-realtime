// Animated GIF from the game's 8-bit BMP frames (the harness `record` / `screen` dumps): the BMP's own palette becomes the
// GIF's global colour table, so no colour is lost or dithered.
//   node gifmake.js <out.gif> <half 0|1> <delay_cs> [--crop x,y,w,h] <bmp...>
// half=1 takes every second pixel (853x480 -> 426x240). delay_cs = hundredths of a second per frame.
const fs = require('fs');

function readBmp(file) {
  const b = fs.readFileSync(file);
  if (b[0] !== 0x42 || b[1] !== 0x4d) throw new Error('not a BMP: ' + file);
  const off = b.readUInt32LE(10);
  const w = b.readInt32LE(18);
  const hRaw = b.readInt32LE(22);
  const bpp = b.readUInt16LE(28);
  if (bpp !== 8) throw new Error('not 8-bit: ' + file + ' bpp=' + bpp);
  const h = Math.abs(hRaw);
  const bottomUp = hRaw > 0;
  const palette = Buffer.alloc(768);
  for (let i = 0; i < 256; i++) {
    palette[i * 3] = b[54 + i * 4 + 2];
    palette[i * 3 + 1] = b[54 + i * 4 + 1];
    palette[i * 3 + 2] = b[54 + i * 4];
  }
  const stride = b.length - off === w * h ? w : ((w + 3) & ~3);
  const px = Buffer.alloc(w * h);
  for (let y = 0; y < h; y++) {
    const src = off + (bottomUp ? h - 1 - y : y) * stride;
    b.copy(px, y * w, src, src + w);
  }
  return { w, h, palette, px };
}

function lzw(indices, minCodeSize) {
  const clear = 1 << minCodeSize;
  const eoi = clear + 1;
  const table = new Int32Array(4096 * 256).fill(-1);
  let codeSize = minCodeSize + 1;
  let next = eoi + 1;
  const out = [];
  let cur = 0;
  let bits = 0;
  function emit(code) {
    cur |= code << bits;
    bits += codeSize;
    while (bits >= 8) {
      out.push(cur & 0xff);
      cur >>>= 8;
      bits -= 8;
    }
  }
  // After a code is written and before its new entry is added: the decoder widens when its next free code passes the
  // largest code of the current width (the classic compress / gif.js order).
  function widen() {
    if (next > (1 << codeSize) - 1 && codeSize < 12) {
      codeSize++;
    }
  }
  emit(clear);
  let prefix = indices[0];
  for (let i = 1; i < indices.length; i++) {
    const c = indices[i];
    const key = prefix * 256 + c;
    const code = table[key];
    if (code >= 0) {
      prefix = code;
      continue;
    }
    emit(prefix);
    widen();
    if (next < 4096) {
      table[key] = next++;
    } else {
      emit(clear);
      table.fill(-1);
      codeSize = minCodeSize + 1;
      next = eoi + 1;
    }
    prefix = c;
  }
  emit(prefix);
  widen();
  emit(eoi);
  if (bits > 0) {
    out.push(cur & 0xff);
  }
  return Buffer.from(out);
}

function subBlocks(data) {
  const parts = [];
  for (let i = 0; i < data.length; i += 255) {
    const chunk = data.subarray(i, Math.min(i + 255, data.length));
    parts.push(Buffer.from([chunk.length]), chunk);
  }
  parts.push(Buffer.from([0]));
  return Buffer.concat(parts);
}

const [out, halfArg, delayArg, ...rest] = process.argv.slice(2);
let crop = null;
const files = [];
for (let i = 0; i < rest.length; i++) {
  if (rest[i] === '--crop') { crop = rest[++i].split(',').map(Number); } else { files.push(rest[i]); }
}
const half = halfArg === '1';
const delay = parseInt(delayArg, 10) || 10;
if (!out || files.length === 0) {
  console.error('usage: node gifmake.js <out.gif> <half 0|1> <delay_cs> <bmp...>');
  process.exit(1);
}
const first = readBmp(files[0]);
const CX = crop ? crop[0] : 0;
const CY = crop ? crop[1] : 0;
const CW = crop ? crop[2] : first.w;
const CH = crop ? crop[3] : first.h;
const W = half ? Math.floor(CW / 2) : CW;
const H = half ? Math.floor(CH / 2) : CH;
const parts = [];
const head = Buffer.alloc(13);
head.write('GIF89a', 0, 'ascii');
head.writeUInt16LE(W, 6);
head.writeUInt16LE(H, 8);
head[10] = 0xf7; // global colour table, 8-bit colour resolution, 256 entries
head[11] = 0;
head[12] = 0;
parts.push(head, first.palette);
parts.push(Buffer.from([0x21, 0xff, 0x0b]), Buffer.from('NETSCAPE2.0', 'ascii'), Buffer.from([0x03, 0x01, 0x00, 0x00, 0x00]));
let n = 0;
for (const f of files) {
  const fr = readBmp(f);
  const idx = Buffer.alloc(W * H);
  for (let y = 0; y < H; y++) {
    for (let x = 0; x < W; x++) {
      idx[y * W + x] = fr.px[(CY + (half ? y * 2 : y)) * fr.w + CX + (half ? x * 2 : x)];
    }
  }
  const gce = Buffer.from([0x21, 0xf9, 0x04, 0x04, delay & 0xff, (delay >> 8) & 0xff, 0x00, 0x00]);
  const desc = Buffer.alloc(10);
  desc[0] = 0x2c;
  desc.writeUInt16LE(0, 1);
  desc.writeUInt16LE(0, 3);
  desc.writeUInt16LE(W, 5);
  desc.writeUInt16LE(H, 7);
  desc[9] = 0;
  parts.push(gce, desc, Buffer.from([8]), subBlocks(lzw(idx, 8)));
  n++;
}
parts.push(Buffer.from([0x3b]));
const gif = Buffer.concat(parts);
fs.writeFileSync(out, gif);
console.log('wrote ' + out + ' ' + W + 'x' + H + ' frames=' + n + ' bytes=' + gif.length);
