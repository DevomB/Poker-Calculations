/**
 * Maintainer / CI only: compile the native addon from C++ (CMake + cmake-js).
 * End users never run this — they load prebuilt binaries under prebuilds (node-gyp-build).
 */
const { spawnSync } = require('child_process');
const path = require('path');

const cmakeJs = require.resolve('cmake-js/bin/cmake-js');
const argv = process.argv.slice(2);
const args = argv.length ? argv : ['compile'];
const r = spawnSync(process.execPath, [cmakeJs, ...args], {
  cwd: path.join(__dirname, '..'),
  stdio: 'inherit',
  shell: false,
  env: process.env,
});
process.exit(r.status === null ? 1 : r.status);
