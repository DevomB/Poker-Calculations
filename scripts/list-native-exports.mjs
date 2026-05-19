/**
 * Prints every **callable** name on the loaded N-API addon. This list must stay in sync with
 * `exports.Set(...)` in `native/binding.cpp` (and `index.d.ts`).
 *
 * Run from NPM/: `node scripts/list-native-exports.mjs`
 *
 * Not shown here: C++-only engine APIs (`GameEngine`, deck shuffle/deal, `BotConfig` file load/save, etc.).
 * Those are covered under **Engine and integration** in FEATURES_ADDED.md and in `tests/*.cpp`.
 */
import { createRequire } from 'module';

const require = createRequire(import.meta.url);
const poker = require('../index.js');

const names = Object.keys(poker)
  .filter((k) => typeof poker[k] === 'function')
  .sort();

console.log('\n--- Native binding: all exported functions ---');
console.log(`Count: ${names.length}`);
for (const n of names) {
  console.log(`  ${n}`);
}
