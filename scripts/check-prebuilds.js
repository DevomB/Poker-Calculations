/**
 * Block `npm publish` / `npm pack` without prebuilt native addons (unless SKIP_PREBUILD_CHECK=1).
 *
 *   node scripts/check-prebuilds.js        at least one prebuilds/<tuple>/node.napi.node (prepack)
 *   node scripts/check-prebuilds.js --all  every binary the release workflow builds
 *
 * Messages go to stderr so `npm pack --dry-run --json` output stays valid JSON.
 */
const fs = require('fs');
const path = require('path');

// Keep in step with the build jobs in .github/workflows/npm-publish.yml.
const RELEASE_BINARIES = [
  'darwin-arm64/node.napi.node',
  'darwin-x64/node.napi.node',
  'linux-arm/node.napi.node',
  'linux-arm64/node.napi.node',
  'linux-arm64/node.napi.musl.node',
  'linux-x64/node.napi.node',
  'linux-x64/node.napi.musl.node',
  'win32-arm64/node.napi.node',
  'win32-x64/node.napi.node',
];

if (process.env.SKIP_PREBUILD_CHECK === '1') {
  process.exit(0);
}

const pb = path.join(__dirname, '..', 'prebuilds');
const present = [];
if (fs.existsSync(pb)) {
  for (const tuple of fs.readdirSync(pb)) {
    const dir = path.join(pb, tuple);
    if (!fs.statSync(dir).isDirectory()) continue;
    for (const file of fs.readdirSync(dir)) {
      if (file.endsWith('.node')) present.push(`${tuple}/${file}`);
    }
  }
}

if (process.argv.includes('--all')) {
  const missing = RELEASE_BINARIES.filter((f) => !present.includes(f));
  if (missing.length) {
    console.error(`Missing release prebuilds: ${missing.join(', ')}`);
    process.exit(1);
  }
  console.error(`Prebuild check OK: all ${RELEASE_BINARIES.length} release binaries present`);
  process.exit(0);
}

if (!present.some((f) => f.endsWith('/node.napi.node'))) {
  console.error('No prebuilds/<platform>-<arch>/node.napi.node found.');
  console.error('Build one: npm run build:native && node scripts/stage-prebuild.js <platform-arch>');
  console.error('(README, Development). Set SKIP_PREBUILD_CHECK=1 only to pack deliberately without binaries.');
  process.exit(1);
}
console.error(`Prebuild check OK: ${present.join(', ')}`);
