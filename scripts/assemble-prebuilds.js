/**
 * CI / maintainer: place flat `${tuple}.napi.node` files into prebuilds/.
 *
 * Usage: node scripts/assemble-prebuilds.js <directory>
 *
 * Expects filenames like:
 *   win32-x64.napi.node           -> prebuilds/win32-x64/node.napi.node
 *   linux-x64-musl.napi.node      -> prebuilds/linux-x64/node.napi.musl.node
 */
'use strict';

const fs = require('fs');
const path = require('path');

const srcDir = process.argv[2];
if (!srcDir) {
  console.error('Usage: node scripts/assemble-prebuilds.js <directory-with-*.napi.node>');
  process.exit(1);
}

const abs = path.resolve(srcDir);
if (!fs.existsSync(abs)) {
  console.error(`Directory not found: ${abs}`);
  process.exit(1);
}

const root = path.join(__dirname, '..');
const suffix = '.napi.node';
let count = 0;

for (const name of fs.readdirSync(abs)) {
  if (!name.endsWith(suffix)) {
    continue;
  }
  const baseName = name.slice(0, -suffix.length);
  if (!baseName || !/^[a-z0-9]+(?:-[a-z0-9]+)*$/i.test(baseName)) {
    continue;
  }

  let destFile;
  if (/-musl$/i.test(baseName)) {
    const tupleDir = baseName.replace(/-musl$/i, '');
    if (!/^linux-/.test(tupleDir)) {
      console.warn(`Skipping unsupported musl artifact name: ${name}`);
      continue;
    }
    destFile = path.join(root, 'prebuilds', tupleDir, 'node.napi.musl.node');
  } else {
    destFile = path.join(root, 'prebuilds', baseName, 'node.napi.node');
  }

  const from = path.join(abs, name);
  fs.mkdirSync(path.dirname(destFile), { recursive: true });
  fs.copyFileSync(from, destFile);
  console.log(`${from} -> ${destFile}`);
  count++;
}

if (count === 0) {
  console.error(`No *${suffix} files in ${abs}`);
  process.exit(1);
}
