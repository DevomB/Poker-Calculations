/**
 * CI gate: decide whether to build + publish based only on package.json "version".
 * If npm already has that exact version, we skip the expensive native matrix and npm publish.
 *
 * Release hygiene: if a run fails before publish, fix the code and re-run the workflow
 * without bumping version again — only increment after the previous number is on npm.
 *
 * Writes publish=true|false to GITHUB_OUTPUT when that env var is set (GitHub Actions).
 */
'use strict';

const fs = require('fs');
const path = require('path');
const { execFileSync } = require('child_process');

const pkgPath = path.join(__dirname, '..', 'package.json');
const pkg = JSON.parse(fs.readFileSync(pkgPath, 'utf8'));
const { name, version } = pkg;

function versionExistsOnNpm(packageName, ver) {
  try {
    const out = execFileSync(
      'npm',
      ['view', `${packageName}@${ver}`, 'version', '--silent'],
      { encoding: 'utf8', stdio: ['ignore', 'pipe', 'pipe'] },
    );
    return String(out).trim() === ver;
  } catch {
    return false;
  }
}

const exists = versionExistsOnNpm(name, version);
const publish = !exists;

const outPath = process.env.GITHUB_OUTPUT;
if (outPath) {
  fs.appendFileSync(outPath, `publish=${publish}\n`);
}

if (exists) {
  console.log(
    `${name}@${version} is already on npm — skipping native build and publish.`,
  );
} else {
  console.log(`${name}@${version} is not on npm — will build and publish.`);
}
