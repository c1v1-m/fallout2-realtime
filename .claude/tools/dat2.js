// Lists and extracts files from a Fallout 2 .DAT (the DAT2 format: a directory tree at the end of the
// file, entries zlib-compressed). Read-only; the archive is never written.
//
//   node dat2.js <master.dat> list [regex]          -- names (and sizes) matching regex
//   node dat2.js <master.dat> cat <name>            -- one file to stdout (name as listed, any case)
//   node dat2.js <master.dat> extract <regex> <dir> -- every match into <dir>, keeping the paths
const fs = require('fs');
const path = require('path');
const zlib = require('zlib');

const [datPath, cmd, arg1, arg2] = process.argv.slice(2);
if (!datPath || !cmd) {
    console.error('usage: node dat2.js <file.dat> list [regex] | cat <name> | extract <regex> <dir>');
    process.exit(1);
}

const fd = fs.openSync(datPath, 'r');
const size = fs.fstatSync(fd).size;
const tail = Buffer.alloc(8);
fs.readSync(fd, tail, 0, 8, size - 8);
const treeSize = tail.readUInt32LE(0);
const tree = Buffer.alloc(treeSize);
fs.readSync(fd, tree, 0, treeSize, size - 8 - treeSize);

const entries = [];
let off = 0;
const count = tree.readUInt32LE(off);
off += 4;
for (let i = 0; i < count; i++) {
    const nameLen = tree.readUInt32LE(off);
    off += 4;
    const name = tree.toString('latin1', off, off + nameLen);
    off += nameLen;
    const compressed = tree[off];
    off += 1;
    const realSize = tree.readUInt32LE(off);
    const packedSize = tree.readUInt32LE(off + 4);
    const offset = tree.readUInt32LE(off + 8);
    off += 12;
    entries.push({ name, compressed, realSize, packedSize, offset });
}

function read(e) {
    const buf = Buffer.alloc(e.packedSize);
    fs.readSync(fd, buf, 0, e.packedSize, e.offset);
    return e.compressed ? zlib.inflateSync(buf) : buf;
}

if (cmd === 'list') {
    const re = new RegExp(arg1 || '.', 'i');
    for (const e of entries) {
        if (re.test(e.name)) console.log(`${e.name}\t${e.realSize}`);
    }
} else if (cmd === 'cat') {
    const e = entries.find(x => x.name.toLowerCase() === arg1.toLowerCase().replace(/\//g, '\\'));
    if (!e) {
        console.error('not found: ' + arg1);
        process.exit(1);
    }
    process.stdout.write(read(e));
} else if (cmd === 'extract') {
    const re = new RegExp(arg1, 'i');
    let n = 0;
    for (const e of entries) {
        if (!re.test(e.name)) continue;
        const out = path.join(arg2, ...e.name.split('\\'));
        fs.mkdirSync(path.dirname(out), { recursive: true });
        fs.writeFileSync(out, read(e));
        n++;
    }
    console.log(`extracted ${n} files`);
}
