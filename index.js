'use strict';

const path = require('path');

let native;
try {
  native = require('node-gyp-build')(path.join(__dirname));
} catch (err) {
  const msg =
    '[poker-calculations] Native addon failed to load.\n' +
    '  Published installs ship prebuilt binaries (no compiler needed).\n' +
    '  From a git clone: npm ci && npm run build:native && node scripts/stage-prebuild.js <platform-arch>\n' +
    '  (CMake + C++20 toolchain required; see README.)\n';
  err.message = msg + '\nOriginal error: ' + err.message;
  throw err;
}

module.exports = native;
