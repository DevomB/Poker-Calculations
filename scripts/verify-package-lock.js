/**
 * Ensure package-lock.json matches package.json (same regeneration npm ci expects).
 * Runs npm install --package-lock-only; locally updates the lock and succeeds.
 * In CI (CI=true), fails if the committed lockfile was stale so PRs must include updates.
 */
'use strict';

const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');

const root = path.join(__dirname, '..');
const lockPath = path.join(root, 'package-lock.json');
const isCI = process.env.CI === 'true';

if (!fs.existsSync(lockPath)) {
  console.error('Missing package-lock.json. Run: npm install');
  process.exit(1);
}

const before = fs.readFileSync(lockPath, 'utf8');

execSync('npm install --package-lock-only', {
  cwd: root,
  stdio: 'inherit',
  shell: true,
  env: process.env,
});

const after = fs.readFileSync(lockPath, 'utf8');
if (before !== after) {
  if (isCI) {
    console.error(
      'package-lock.json is out of sync with package.json. Regenerate it (e.g. npm install --package-lock-only), commit package-lock.json, and push.',
    );
    process.exit(1);
  }
  console.log('package-lock.json was updated to match package.json.');
  process.exit(0);
}

console.log('package-lock.json matches package.json');
