#!/usr/bin/env node
// Usage: node scripts/check-doc-links.mjs [Markdown file or directory ...]
// Default: root entry documents and docs/. No network requests or dependencies.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
if (process.argv.includes('--help')) {
  console.log('Usage: node scripts/check-doc-links.mjs [Markdown file or directory ...]\n'
    + 'Checks local inline links and Markdown heading/HTML id anchors.\n'
    + 'Defaults to README.md, AGENTS.md, CLAUDE.md and docs/.\n'
    + 'Ignores code examples and external URLs; no network requests are made.\n'
    + 'Unsupported (not checked): reference-style links, autolinks, HTML href links,\n'
    + 'links inside indented code blocks, labels nested more than one bracket level,\n'
    + 'setext headings, and Markdown renderer-specific anchor rules.');
  process.exit(0);
}

// Keep line positions stable while excluding fenced code and HTML comments.
const blank = (value) => value.replace(/[^\n]/g, ' ');
function prose(source) {
  let fence;
  return source.replace(/<!--[\s\S]*?-->/g, blank).split('\n').map((line) => {
    const marker = line.match(/^ {0,3}(`{3,}|~{3,})(.*)$/);
    if (!fence && marker) {
      fence = marker[1];
      return blank(line);
    }
    if (fence) {
      if (marker && marker[1][0] === fence[0] && marker[1].length >= fence.length
        && !marker[2].trim()) fence = undefined;
      return blank(line);
    }
    return line;
  }).join('\n');
}

// Blank inline code spans. A backslash-escaped backtick does not open a span.
function withoutInlineCode(text) {
  return text.replace(/\\[\s\S]|(?<!`)(`+)(?!`)[\s\S]*?(?<!`)\1(?!`)/g, (span) => (span[0] === '\\' ? span : blank(span)));
}

function headings(source) {
  const text = prose(source);
  const result = new Set();
  for (const tag of withoutInlineCode(text).matchAll(/<[A-Za-z][^>]*>/g)) {
    for (const attribute of tag[0].matchAll(/\s(?:id|name)\s*=\s*(["'])(.+?)\1/g)) result.add(attribute[2]);
  }
  for (const match of text.matchAll(/^ {0,3}#{1,6}[ \t]+(.+?)[ \t]*#*[ \t]*\r?$/gm)) {
    const base = match[1].toLowerCase()
      .replace(/!?\[([^\]]*)\]\([^)]*\)/g, '$1')
      .replace(/<[^>]*>/g, '')
      .replace(/[^\p{L}\p{M}\p{N}_\- ]/gu, '').replace(/ /g, '-');
    let slug = base;
    for (let suffix = 1; result.has(slug); suffix++) slug = `${base}-${suffix}`;
    result.add(slug);
  }
  return result;
}

function* links(source) {
  const text = withoutInlineCode(prose(source));
  // The label may contain one level of nested brackets, e.g. [outer [inner]](target.md).
  for (const match of text.matchAll(/!?\[(?:\\.|[^[\]\\\n]|\[(?:\\.|[^[\]\\\n])*\])*\]\(/g)) {
    let index = match.index + match[0].length;
    while (/\s/.test(text[index] ?? '') && index < text.length) index++;
    const start = index;
    if (text[index] === '<') {
      const end = text.indexOf('>', index + 1);
      if (end >= 0) yield { target: text.slice(index + 1, end), offset: match.index };
      continue;
    }
    let depth = 0;
    for (; index < text.length; index++) {
      const char = text[index];
      if (char === '\\') { index++; continue; }
      if (char === '(') depth++;
      if ((char === ')' && depth === 0) || (/\s/.test(char) && depth === 0)) break;
      if (char === ')') depth--;
    }
    yield { target: text.slice(start, index).replace(/\\([() ])/g, '$1'), offset: match.index };
  }
}

function markdownFiles(entry) {
  if (fs.statSync(entry).isDirectory()) {
    return fs.readdirSync(entry).sort().flatMap((name) => markdownFiles(path.join(entry, name)));
  }
  return path.extname(entry).toLowerCase() === '.md' ? [entry] : [];
}

try {
  const inputs = process.argv.slice(2);
  const entries = inputs.length ? inputs : ['README.md', 'AGENTS.md', 'CLAUDE.md', 'docs'];
  const files = [...new Set(entries.flatMap((entry) => markdownFiles(path.resolve(root, entry))))];
  if (!files.length) throw new Error('No Markdown files selected.');
  const cache = new Map();
  let local = 0, anchors = 0, external = 0, errors = 0;
  for (const file of files) {
    const source = fs.readFileSync(file, 'utf8');
    for (const { target, offset } of links(source)) {
      if (/^[a-z][a-z\d+.-]*:/i.test(target) || target.startsWith('//')) { external++; continue; }
      local++;
      const line = source.slice(0, offset).split('\n').length;
      const fail = (reason) => {
        console.error(`${path.relative(root, file)}:${line}: ${reason}: ${target}`);
        errors++;
      };
      let relative, fragment;
      try {
        const hash = target.indexOf('#');
        relative = decodeURIComponent((hash < 0 ? target : target.slice(0, hash)).split('?')[0]);
        fragment = hash < 0 ? '' : decodeURIComponent(target.slice(hash + 1));
      } catch { fail('invalid URL encoding'); continue; }
      const resolved = relative
        ? path.resolve(relative.startsWith('/') ? root : path.dirname(file), relative.replace(/^\//, ''))
        : file;
      if (!fs.existsSync(resolved)) { fail('missing file'); continue; }
      if (fragment && path.extname(resolved).toLowerCase() === '.md') {
        anchors++;
        if (!cache.has(resolved)) cache.set(resolved, headings(fs.readFileSync(resolved, 'utf8')));
        if (!cache.get(resolved).has(fragment)) fail('missing anchor');
      }
    }
  }
  console.log(`Markdown files: ${files.length}; local links: ${local}; anchors: ${anchors}; external URLs skipped: ${external}; errors: ${errors}`);
  process.exitCode = errors ? 1 : 0;
} catch (error) {
  console.error(error.message);
  process.exitCode = 2;
}
