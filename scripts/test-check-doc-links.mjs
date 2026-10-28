import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const checker = fileURLToPath(new URL('./check-doc-links.mjs', import.meta.url));
const fixture = fs.mkdtempSync(path.join(os.tmpdir(), 'digether-doc-links-'));
const run = (file) => spawnSync(process.execPath, [checker, file], { encoding: 'utf8' });
try {
  fs.writeFileSync(path.join(fixture, '대상 (문서).md'), '# 한국어 `API`\n\n## 같은 제목\n\n## 같은 제목\n\n<a id="explicit"></a>\n');
  const document = path.join(fixture, 'entry.md');
  fs.writeFileSync(document, [
    '# Entry',
    '[encoded](%EB%8C%80%EC%83%81%20(%EB%AC%B8%EC%84%9C).md#한국어-api)',
    '[duplicate](<대상 (문서).md#같은-제목-1>)',
    '[html](<대상 (문서).md#explicit>)',
    '[self](#entry)',
    '[external](https://example.invalid/not-queried)',
    '`[inline example](missing-inline.md)`',
    '```markdown', '[fenced example](missing-fence.md)', '```',
    '<!-- [comment](missing-comment.md) -->',
  ].join('\n'));
  const valid = run(document);
  assert.equal(valid.status, 0, valid.stderr);
  assert.match(valid.stdout, /local links: 4; anchors: 4; external URLs skipped: 1; errors: 0/);
  fs.appendFileSync(document, '\n[missing](absent.md)\n[anchor](#absent)\n');
  const invalid = run(document);
  assert.equal(invalid.status, 1);
  assert.match(invalid.stderr, /entry\.md:12: missing file: absent\.md/);
  assert.match(invalid.stderr, /entry\.md:13: missing anchor: #absent/);
  assert.match(invalid.stdout, /errors: 2/);
  fs.writeFileSync(path.join(fixture, 'anchors.md'), [
    '# Anchors', '', '`<a id="inline-fake"></a>`', '', 'Plain text id="plain-fake" is not a tag.', '',
    '```html', '<a id="fence-fake"></a>', '```', '', '<a id="real"></a>', '<span class=x name=\'named\'></span>',
  ].join('\n'));
  const edge = path.join(fixture, 'edge.md');
  fs.writeFileSync(edge, [
    '[real](anchors.md#real)',
    '[named](anchors.md#named)',
    '[inline](anchors.md#inline-fake)',
    '[plain](anchors.md#plain-fake)',
    '[fence](anchors.md#fence-fake)',
    '[outer [inner]](absent-nested.md)',
    '\\` [escaped](absent-escaped.md)',
    '`code [skipped](absent-code.md)` and ``a ` b [skipped](absent-double.md)``',
    '[ok outer [inner]](anchors.md#real)',
  ].join('\n'));
  const edgeResult = run(edge);
  assert.equal(edgeResult.status, 1);
  for (const expected of [
    /edge\.md:3: missing anchor: anchors\.md#inline-fake/, /edge\.md:4: missing anchor: anchors\.md#plain-fake/,
    /edge\.md:5: missing anchor: anchors\.md#fence-fake/, /edge\.md:6: missing file: absent-nested\.md/,
    /edge\.md:7: missing file: absent-escaped\.md/,
  ]) assert.match(edgeResult.stderr, expected);
  assert.doesNotMatch(edgeResult.stderr, /edge\.md:(?:1|2|8|9):/);
  assert.match(edgeResult.stdout, /errors: 5/);
  const missingInput = run(path.join(fixture, 'not-present.md'));
  assert.equal(missingInput.status, 2);
  console.log('Document link regression checks passed (valid, broken file/anchor, fake ids, nested labels, escaped backticks, missing input).');
} finally {
  assert.equal(path.dirname(path.resolve(fixture)), path.resolve(os.tmpdir()));
  assert.ok(path.basename(fixture).startsWith('digether-doc-links-'));
  fs.rmSync(fixture, { recursive: true, force: true });
}
