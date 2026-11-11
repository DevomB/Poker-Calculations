/**
 * Block `npm publish` / `npm pack` unless every supported platform's prebuilt addon is staged.
 * A package with only the local platform's binary installs everywhere but fails to load on the
 * rest, so one binary is not enough. The list matches the build matrix in npm-publish.yml.
 */
const fs = require('fs');
const path = require('path');

if (process.env.SKIP_PREBUILD_CHECK === '1') {
  process.exit(0);
}

const REQUIRED = [
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

const pb = path.join(__dirname, '..', 'prebuilds');
const missing = REQUIRED.filter((rel) => !fs.existsSync(path.join(pb, rel)));
if (missing.length > 0) {
  console.error(`Missing ${missing.length} of ${REQUIRED.length} prebuilt addons under ${pb}:`);
  for (const rel of missing) console.error(`  ${rel}`);
  console.error('Publish through the npm-publish workflow, which builds every target.');
  console.error('Set SKIP_PREBUILD_CHECK=1 only to pack a local test build.');
  process.exit(1);
}

console.log(`Prebuild check OK: all ${REQUIRED.length} targets present.`);
