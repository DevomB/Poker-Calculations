/**
 * CI: tag a published release as v<package.json version>. Never moves a tag.
 *
 *   node scripts/tag-release.js --commit <sha>   tag <sha> (the commit the publish job just shipped)
 *   node scripts/tag-release.js                  tag the commit npm recorded as this version's gitHead
 *
 * Without --commit the version must already be on npm, so an unpublished or failed release is never
 * tagged. If the tag exists at the same commit this is a no-op; if it exists anywhere else the
 * script fails and leaves it alone.
 */
'use strict';

const { execFileSync } = require('child_process');
const path = require('path');

const pkg = require(path.join(__dirname, '..', 'package.json'));
const tag = `v${pkg.version}`;
const run = (cmd, args) => execFileSync(cmd, args, { encoding: 'utf8', stdio: ['ignore', 'pipe', 'pipe'] }).trim();

function publishedGitHead() {
  try {
    return run('npm', ['view', `${pkg.name}@${pkg.version}`, 'gitHead']);
  } catch {
    return '';
  }
}

const i = process.argv.indexOf('--commit');
const target = i >= 0 ? process.argv[i + 1] : publishedGitHead();
if (!target) {
  console.error(`${pkg.name}@${pkg.version} is not on npm (or has no gitHead); not tagging.`);
  process.exit(1);
}
const commit = run('git', ['rev-parse', '--verify', `${target}^{commit}`]);

// `refs/tags/v1.2.3^{}` is the peeled commit of an annotated tag; a lightweight tag lists only itself.
const remote = run('git', ['ls-remote', '--tags', 'origin', `refs/tags/${tag}`, `refs/tags/${tag}^{}`]);
if (remote) {
  const lines = remote.split('\n').map((l) => l.split('\t'));
  const peeled = lines.find(([, ref]) => ref.endsWith('^{}')) ?? lines[0];
  if (peeled[0] === commit) {
    console.log(`${tag} already points at ${commit}.`);
    process.exit(0);
  }
  console.error(`${tag} already exists at ${peeled[0]}, not ${commit}; leaving it unchanged.`);
  process.exit(1);
}

run('git', ['tag', tag, commit]);
// Pushing a new ref fails if the tag appeared meanwhile, so this cannot move an existing tag.
run('git', ['push', 'origin', `refs/tags/${tag}`]);
console.log(`Tagged ${commit} as ${tag}.`);
