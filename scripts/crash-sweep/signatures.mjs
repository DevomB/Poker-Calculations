// Parse `export interface PokerCalculations` in index.d.ts into Map<name, params[]> (first overload wins).
import fs from 'node:fs';

export function parseSignatures(dtsPath) {
  const src = fs.readFileSync(dtsPath, 'utf8');
  const start = src.indexOf('export interface PokerCalculations');
  let i = src.indexOf('{', start) + 1;
  let depth = 1;
  let body = '';
  for (; i < src.length && depth > 0; i++) {
    const c = src[i];
    if (c === '{') depth++;
    if (c === '}') depth--;
    if (depth > 0) body += c;
  }
  body = body.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/.*$/gm, '');

  // Members sit one level in; match that indent exactly so nested object types are skipped.
  const indent = body.match(/\n([ \t]+)\S/)?.[1] ?? '  ';
  const sigs = new Map();
  const re = new RegExp(`(^|\\n)${indent}([A-Za-z_]\\w*)\\s*(<[^>]*>)?\\(`, 'g');
  let m;
  while ((m = re.exec(body))) {
    const name = m[2];
    let j = re.lastIndex;
    let d = 1;
    let params = '';
    for (; j < body.length && d > 0; j++) {
      const c = body[j];
      if ('([{<'.includes(c)) d++;
      if (')]}>'.includes(c)) d--;
      if (d > 0) params += c;
    }
    re.lastIndex = j;
    if (sigs.has(name)) continue; // keep first overload
    const parts = [];
    let cur = '';
    let dd = 0;
    for (const c of params) {
      if ('([{<'.includes(c)) dd++;
      if (')]}>'.includes(c)) dd--;
      if (c === ',' && dd === 0) {
        parts.push(cur);
        cur = '';
      } else cur += c;
    }
    if (cur.trim()) parts.push(cur);
    sigs.set(
      name,
      parts
        .map((p) => p.trim())
        .filter(Boolean)
        .map((p) => {
          const mm = p.match(/^(\.\.\.)?([A-Za-z_]\w*)(\?)?\s*:\s*([\s\S]+)$/);
          return mm
            ? { name: mm[2], optional: !!mm[3], type: mm[4].replace(/\s+/g, ' ').trim() }
            : { name: p, optional: false, type: 'unknown' };
        })
    );
  }
  return sigs;
}
