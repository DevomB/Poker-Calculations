/**
 * Block `npm publish` / `npm pack` without prebuilt native addons (unless skipped).
 */
const fs = require('fs');
const path = require('path');

if (process.env.SKIP_PREBUILD_CHECK === '1') {
  process.exit(0);
}

const root = path.join(__dirname, '..');
const pb = path.join(root, 'prebuilds');
if (!fs.existsSync(pb)) {
  console.error(`prebuilds/ missing at ${pb}`);
  console.error('Build binaries in CI or locally: npm run build:native && node scripts/stage-prebuild.js <tuple>');
  process.exit(1);
}

const tuples = fs.readdirSync(pb).filter((n) => !n.endsWith('.md'));
const valid = tuples.some((t) => fs.existsSync(path.join(pb, t, 'node.napi.node')));
if (!valid) {
  console.error('No prebuilds/*/node.napi.node found.');
  console.error('See README → Maintainers: publishing. Set SKIP_PREBUILD_CHECK=1 to force pack (not recommended).');
  process.exit(1);
}

console.log('Prebuild check OK:', tuples.join(', '));
