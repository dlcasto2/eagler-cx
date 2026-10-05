// nspGL browser runtime: minimal DOM, events, timers, CSS cascade.
// Natives provided by C (in global __n): log, now, loadText, loadImage, parseHTML,
// createWebGL, create2D, alert, dirty, navigate, title, rect
(function (G) {
'use strict';
const N = G.__n;
const VOID = new Set(['br','img','hr','input','meta','link','area','base','col','embed','param','source','track','wbr']);

// ---------------- events ----------------
class Event {
  constructor(type, init) {
    init = init || {};
    this.type = type; this.bubbles = !!init.bubbles; this.cancelable = !!init.cancelable;
    this.defaultPrevented = false; this._stop = false; this.target = null; this.currentTarget = null;
    this.timeStamp = N.now();
  }
  preventDefault() { this.defaultPrevented = true; }
  stopPropagation() { this._stop = true; }
  stopImmediatePropagation() { this._stop = true; this._stopImm = true; }
}
class UIEvent extends Event {}
class KeyboardEvent extends UIEvent {
  constructor(type, init) { super(type, init); init = init || {};
    for (const k of ['key','code','keyCode','which','charCode','shiftKey','ctrlKey','altKey','metaKey','repeat']) this[k] = init[k] !== undefined ? init[k] : (k === 'key' || k === 'code' ? '' : (k.endsWith('Key') || k === 'repeat') ? false : 0); }
  getModifierState() { return false; }
}
class MouseEvent extends UIEvent {
  constructor(type, init) { super(type, init); init = init || {};
    for (const k of ['clientX','clientY','pageX','pageY','screenX','screenY','offsetX','offsetY','movementX','movementY','button','buttons']) this[k] = init[k] || 0;
    this.shiftKey = this.ctrlKey = this.altKey = this.metaKey = false; }
}
class WheelEvent extends MouseEvent { constructor(t, i) { super(t, i); i = i || {}; this.deltaX = i.deltaX || 0; this.deltaY = i.deltaY || 0; this.deltaZ = 0; this.deltaMode = 0; } }
class PointerEvent extends MouseEvent { constructor(t, i) { super(t, i); this.pointerId = 1; this.pointerType = 'mouse'; this.isPrimary = true; } }
class TouchEvent extends UIEvent { constructor(t, i) { super(t, i); i = i || {}; this.touches = i.touches || []; this.changedTouches = i.changedTouches || []; this.targetTouches = this.touches; } }
class CustomEvent extends Event { constructor(t, i) { super(t, i); this.detail = i && i.detail; } }

class EventTarget {
  addEventListener(type, fn, opts) {
    if (!fn) return;
    const l = this.__ls || (this.__ls = {});
    (l[type] || (l[type] = [])).push({ fn, once: !!(opts && opts.once) });
    if (type.startsWith('key')) G.__keyListeners = true;
    if (type.startsWith('mouse') || type.startsWith('pointer') || type === 'click' || type.startsWith('touch') || type === 'wheel') G.__mouseListeners = true;
  }
  removeEventListener(type, fn) {
    const l = this.__ls && this.__ls[type];
    if (!l) return;
    const i = l.findIndex(e => e.fn === fn);
    if (i >= 0) l.splice(i, 1);
  }
  dispatchEvent(ev) {
    ev.target = ev.target || this;
    const path = [];
    let n = this;
    while (n) { path.push(n); if (n === G) break; n = n.parentNode || (n.nodeType === 9 ? G : null); }
    for (const n of (ev.bubbles ? path : [this])) {
      ev.currentTarget = n;
      __fire(n, ev);
      if (ev._stop) break;
    }
    return !ev.defaultPrevented;
  }
}
function __fire(n, ev) {
  const h = n['on' + ev.type];
  if (typeof h === 'function') { try { if (h.call(n, ev) === false) ev.preventDefault(); } catch (e) { __err(e); } }
  const l = n.__ls && n.__ls[ev.type];
  if (!l) return;
  for (const e of l.slice()) {
    if (e.once) n.removeEventListener(ev.type, e.fn);
    try { if (typeof e.fn === 'function') e.fn.call(n, ev); else if (e.fn && e.fn.handleEvent) e.fn.handleEvent(ev); } catch (x) { __err(x); }
    if (ev._stopImm) break;
  }
}
function __err(e) {
  N.log(2, (e && e.stack) ? (String(e) + '\n' + e.stack) : String(e));
}

// ---------------- DOM ----------------
function dirty() { N.dirty(); }

class Node extends EventTarget {
  constructor(type, name) { super(); this.nodeType = type; this.nodeName = name; this.childNodes = []; this.parentNode = null; this.ownerDocument = G.document || null; }
  get parentElement() { return this.parentNode && this.parentNode.nodeType === 1 ? this.parentNode : null; }
  get firstChild() { return this.childNodes[0] || null; }
  get lastChild() { return this.childNodes[this.childNodes.length - 1] || null; }
  get nextSibling() { const p = this.parentNode; if (!p) return null; return p.childNodes[p.childNodes.indexOf(this) + 1] || null; }
  get previousSibling() { const p = this.parentNode; if (!p) return null; const i = p.childNodes.indexOf(this); return i > 0 ? p.childNodes[i - 1] : null; }
  get isConnected() { let n = this; while (n.parentNode) n = n.parentNode; return n.nodeType === 9; }
  hasChildNodes() { return this.childNodes.length > 0; }
  appendChild(c) { return this.insertBefore(c, null); }
  insertBefore(c, ref) {
    if (c.nodeType === 11) { for (const k of c.childNodes.slice()) this.insertBefore(k, ref); return c; }
    if (c.parentNode) c.parentNode.removeChild(c);
    const i = ref ? this.childNodes.indexOf(ref) : -1;
    if (i < 0) this.childNodes.push(c); else this.childNodes.splice(i, 0, c);
    c.parentNode = this;
    dirty();
    if (c.nodeType === 1 && c.isConnected) __connected(c);
    return c;
  }
  removeChild(c) { const i = this.childNodes.indexOf(c); if (i >= 0) { this.childNodes.splice(i, 1); c.parentNode = null; dirty(); } return c; }
  replaceChild(n, o) { this.insertBefore(n, o); return this.removeChild(o); }
  remove() { if (this.parentNode) this.parentNode.removeChild(this); }
  append(...a) { for (const x of a) this.appendChild(typeof x === 'string' ? new Text(x) : x); }
  prepend(...a) { const f = this.firstChild; for (const x of a) this.insertBefore(typeof x === 'string' ? new Text(x) : x, f); }
  contains(n) { for (; n; n = n.parentNode) if (n === this) return true; return false; }
  get textContent() {
    if (this.nodeType === 3 || this.nodeType === 8) return this.data;
    let s = ''; for (const c of this.childNodes) if (c.nodeType !== 8) s += c.textContent; return s;
  }
  set textContent(v) {
    if (this.nodeType === 3) { this.data = String(v); dirty(); return; }
    for (const c of this.childNodes) c.parentNode = null;
    this.childNodes = [];
    if (v !== '' && v != null) this.appendChild(new Text(String(v)));
    dirty();
  }
  cloneNode(deep) {
    let n;
    if (this.nodeType === 3) n = new Text(this.data);
    else if (this.nodeType === 8) n = new Comment(this.data);
    else if (this.nodeType === 11) n = new DocumentFragment();
    else { n = createEl(this.localName); for (const k in this._attrs) n.setAttribute(k, this._attrs[k]); }
    if (deep) for (const c of this.childNodes) n.appendChild(c.cloneNode(true));
    return n;
  }
}
class Text extends Node {
  constructor(d) { super(3, '#text'); this._d = String(d); }
  get data() { return this._d; } set data(v) { this._d = String(v); dirty(); }
  get nodeValue() { return this._d; } set nodeValue(v) { this.data = v; }
  get length() { return this._d.length; }
}
class Comment extends Node { constructor(d) { super(8, '#comment'); this.data = d; } }
class DocumentFragment extends Node {
  constructor() { super(11, '#document-fragment'); }
  querySelector(s) { return qsa(this, s, true)[0] || null; }
  querySelectorAll(s) { return qsa(this, s, false); }
  getElementById(id) { return findFirst(this, n => n.nodeType === 1 && n._attrs.id === id); }
  get children() { return this.childNodes.filter(n => n.nodeType === 1); }
}

function camel(s) { return s.replace(/-([a-z])/g, (m, c) => c.toUpperCase()); }
function dash(s) { return s.replace(/[A-Z]/g, c => '-' + c.toLowerCase()); }
function styleProxy(el) {
  const st = {};
  Object.defineProperties(st, {
    setProperty: { value(k, v) { st[camel(k)] = String(v); dirty(); } },
    getPropertyValue: { value(k) { return st[camel(k)] || ''; } },
    removeProperty: { value(k) { delete st[camel(k)]; dirty(); } },
    cssText: { get() { return Object.keys(st).map(k => dash(k) + ':' + st[k]).join(';'); },
               set(v) { for (const k of Object.keys(st)) delete st[k]; parseInline(String(v), st); dirty(); } },
  });
  return new Proxy(st, { set(t, k, v) { t[k] = v == null ? '' : String(v); dirty(); return true; } });
}
function parseInline(s, st) {
  for (const d of s.split(';')) {
    const i = d.indexOf(':'); if (i < 0) continue;
    const k = d.slice(0, i).trim(), v = d.slice(i + 1).trim();
    if (k) st[camel(k.toLowerCase())] = v;
  }
}

class DOMTokenList {
  constructor(el) { this._el = el; }
  _get() { return (this._el._attrs['class'] || '').split(/\s+/).filter(x => x); }
  _set(a) { this._el.setAttribute('class', a.join(' ')); }
  contains(c) { return this._get().includes(c); }
  add(...c) { const a = this._get(); for (const x of c) if (!a.includes(x)) a.push(x); this._set(a); }
  remove(...c) { this._set(this._get().filter(x => !c.includes(x))); }
  toggle(c, f) { const has = this.contains(c); if (f === undefined) f = !has; if (f) this.add(c); else this.remove(c); return f; }
  replace(a, b) { this._set(this._get().map(x => x === a ? b : x)); }
  get length() { return this._get().length; }
  item(i) { return this._get()[i]; }
  [Symbol.iterator]() { return this._get()[Symbol.iterator](); }
  toString() { return this._get().join(' '); }
}

class Element extends Node {
  constructor(tag) {
    super(1, tag.toUpperCase());
    this.localName = tag.toLowerCase(); this.tagName = this.nodeName;
    this._attrs = {}; this._style = null;
  }
  get style() { return this._style || (this._style = styleProxy(this)); }
  set style(v) { this.style.cssText = v; }
  getAttribute(k) { k = k.toLowerCase(); return k in this._attrs ? this._attrs[k] : null; }
  setAttribute(k, v) {
    k = k.toLowerCase(); v = String(v); this._attrs[k] = v;
    if (k === 'style') { this.style.cssText = v; }
    this._attrChanged && this._attrChanged(k, v);
    dirty();
  }
  hasAttribute(k) { return k.toLowerCase() in this._attrs; }
  removeAttribute(k) { delete this._attrs[k.toLowerCase()]; dirty(); }
  getAttributeNames() { return Object.keys(this._attrs); }
  get attributes() { return Object.keys(this._attrs).map(k => ({ name: k, value: this._attrs[k] })); }
  get id() { return this._attrs.id || ''; } set id(v) { this.setAttribute('id', v); }
  get className() { return this._attrs['class'] || ''; } set className(v) { this.setAttribute('class', v); }
  get classList() { return new DOMTokenList(this); }
  get dataset() { const el = this; return new Proxy({}, { get(t, k) { return el.getAttribute('data-' + dash(String(k))) ?? undefined; }, set(t, k, v) { el.setAttribute('data-' + dash(String(k)), v); return true; } }); }
  get children() { return this.childNodes.filter(n => n.nodeType === 1); }
  get childElementCount() { return this.children.length; }
  get firstElementChild() { return this.children[0] || null; }
  get lastElementChild() { const c = this.children; return c[c.length - 1] || null; }
  get nextElementSibling() { let n = this.nextSibling; while (n && n.nodeType !== 1) n = n.nextSibling; return n; }
  get previousElementSibling() { let n = this.previousSibling; while (n && n.nodeType !== 1) n = n.previousSibling; return n; }
  get innerHTML() { return this.childNodes.map(serialize).join(''); }
  set innerHTML(h) {
    for (const c of this.childNodes) c.parentNode = null;
    this.childNodes = [];
    const tree = N.parseHTML(String(h));
    for (const t of tree) this.appendChild(build(t));
    dirty();
  }
  get outerHTML() { return serialize(this); }
  insertAdjacentHTML(pos, h) {
    const frag = new DocumentFragment();
    for (const t of N.parseHTML(String(h))) frag.appendChild(build(t));
    pos = pos.toLowerCase();
    if (pos === 'beforeend') this.appendChild(frag);
    else if (pos === 'afterbegin') this.insertBefore(frag, this.firstChild);
    else if (pos === 'beforebegin' && this.parentNode) this.parentNode.insertBefore(frag, this);
    else if (pos === 'afterend' && this.parentNode) this.parentNode.insertBefore(frag, this.nextSibling);
  }
  get innerText() { return this.textContent; } set innerText(v) { this.textContent = v; }
  getElementsByTagName(t) { t = t.toUpperCase(); return findAll(this, n => n.nodeType === 1 && (t === '*' || n.tagName === t)); }
  getElementsByClassName(c) { const cs = c.split(/\s+/); return findAll(this, n => n.nodeType === 1 && cs.every(x => n.classList.contains(x))); }
  querySelector(s) { return qsa(this, s, true)[0] || null; }
  querySelectorAll(s) { return qsa(this, s, false); }
  matches(s) { return parseSel(s).some(sel => matchSel(this, sel)); }
  closest(s) { for (let n = this; n && n.nodeType === 1; n = n.parentNode) if (n.matches(s)) return n; return null; }
  getBoundingClientRect() {
    const r = N.rect(this) || [0, 0, 0, 0];
    return { x: r[0], y: r[1], left: r[0], top: r[1], width: r[2], height: r[3], right: r[0] + r[2], bottom: r[1] + r[3] };
  }
  getClientRects() { return [this.getBoundingClientRect()]; }
  get clientWidth() { return (N.rect(this) || [0, 0, 0])[2]; }
  get clientHeight() { return (N.rect(this) || [0, 0, 0, 0])[3]; }
  get offsetWidth() { return this.clientWidth; } get offsetHeight() { return this.clientHeight; }
  get offsetLeft() { return (N.rect(this) || [0])[0]; } get offsetTop() { return (N.rect(this) || [0, 0])[1]; }
  get clientLeft() { return 0; } get clientTop() { return 0; }
  get scrollWidth() { return this.clientWidth; } get scrollHeight() { return this.clientHeight; }
  get offsetParent() { return G.document.body; }
  focus() { G.document.activeElement = this; } blur() { if (G.document.activeElement === this) G.document.activeElement = G.document.body; }
  click() { this.dispatchEvent(new MouseEvent('click', { bubbles: true })); }
  requestPointerLock() { G.document.pointerLockElement = this; setTimeout(() => G.document.dispatchEvent(new Event('pointerlockchange')), 0); }
  requestFullscreen() { G.document.fullscreenElement = this; return Promise.resolve(); }
  setPointerCapture() {} releasePointerCapture() {}
  scrollIntoView() {}
  animate() { return { finished: Promise.resolve(), cancel() {} }; }
  attachShadow() { return this; }
  get hidden() { return this.hasAttribute('hidden'); } set hidden(v) { if (v) this.setAttribute('hidden', ''); else this.removeAttribute('hidden'); }
  get title() { return this._attrs.title || ''; } set title(v) { this.setAttribute('title', v); }
  get tabIndex() { return +(this._attrs.tabindex || -1); } set tabIndex(v) { this.setAttribute('tabindex', v); }
}
// reflect common attributes
for (const a of ['href', 'src', 'type', 'name', 'alt', 'rel', 'lang', 'placeholder', 'target'])
  Object.defineProperty(Element.prototype, a, { get() { return this._attrs[a] || ''; }, set(v) { this.setAttribute(a, v); }, configurable: true });
Object.defineProperty(Element.prototype, 'value', {
  get() { return this._value !== undefined ? this._value : (this._attrs.value || (this.tagName === 'TEXTAREA' ? this.textContent : '')); },
  set(v) { this._value = String(v); dirty(); }, configurable: true });
Object.defineProperty(Element.prototype, 'checked', {
  get() { return this._checked !== undefined ? this._checked : this.hasAttribute('checked'); },
  set(v) { this._checked = !!v; dirty(); }, configurable: true });
Object.defineProperty(Element.prototype, 'disabled', { get() { return this.hasAttribute('disabled'); }, set(v) { if (v) this.setAttribute('disabled', ''); else this.removeAttribute('disabled'); }, configurable: true });

class HTMLElement extends Element {}
class HTMLScriptElement extends HTMLElement {
  get text() { return this.textContent; } set text(v) { this.textContent = v; }
}
class HTMLCanvasElement extends HTMLElement {
  constructor(t) { super(t || 'canvas'); this._ctx = null; this._ctxType = null; }
  get width() { const v = parseInt(this._attrs.width); return isNaN(v) ? 300 : v; }
  set width(v) { this._attrs.width = String(v | 0); this._resized(); }
  get height() { const v = parseInt(this._attrs.height); return isNaN(v) ? 150 : v; }
  set height(v) { this._attrs.height = String(v | 0); this._resized(); }
  _attrChanged(k) { if (k === 'width' || k === 'height') this._resized(); }
  _resized() { if (this._ctx && this._ctx.__resize) this._ctx.__resize(this.width, this.height); dirty(); }
  getContext(type, attrs) {
    type = String(type).toLowerCase();
    if (this._ctx) return (this._ctxType === type || (type.indexOf('webgl') >= 0 && this._ctxType.indexOf('webgl') >= 0)) ? this._ctx : null;
    if (type === 'webgl' || type === 'experimental-webgl') this._ctx = N.createWebGL(this, attrs || {});
    else if (type === '2d') this._ctx = N.create2D(this, attrs || {});
    else return null;
    if (this._ctx) { this._ctxType = type; dirty(); }
    return this._ctx;
  }
  toDataURL() { return 'data:,'; }
  toBlob(cb) { setTimeout(() => cb(null), 0); }
  captureStream() { return {}; }
  transferControlToOffscreen() { return this; }
}
class HTMLImageElement extends HTMLElement {
  constructor() { super('img'); this.complete = false; this.naturalWidth = 0; this.naturalHeight = 0; this._px = null; this.crossOrigin = null; this.decoding = 'auto'; }
  get src() { return this._attrs.src || ''; }
  set src(v) {
    this._attrs.src = String(v); this.complete = false;
    const img = this;
    const r = N.loadImage(img._attrs.src);
    setTimeout(() => {
      if (r) { img._px = r.data; img.naturalWidth = r.width; img.naturalHeight = r.height; img.complete = true; dirty(); img.dispatchEvent(new Event('load')); }
      else { img.complete = true; N.log(1, 'image not found: ' + v); img.dispatchEvent(new Event('error')); }
    }, 0);
  }
  get width() { return parseInt(this._attrs.width) || this.naturalWidth; } set width(v) { this.setAttribute('width', v); }
  get height() { return parseInt(this._attrs.height) || this.naturalHeight; } set height(v) { this.setAttribute('height', v); }
  decode() { return new Promise(res => { if (this.complete) res(); else this.addEventListener('load', () => res()); }); }
}
class HTMLInputElement extends HTMLElement {}
class HTMLAnchorElement extends HTMLElement {}
class HTMLMediaElement extends HTMLElement { play() { return Promise.resolve(); } pause() {} load() {} }

function createEl(tag) {
  tag = String(tag).toLowerCase();
  let el;
  if (tag === 'canvas') el = new HTMLCanvasElement();
  else if (tag === 'img') el = new HTMLImageElement();
  else if (tag === 'script') el = new HTMLScriptElement('script');
  else if (tag === 'input' || tag === 'textarea' || tag === 'select' || tag === 'button') el = new HTMLInputElement(tag);
  else if (tag === 'a') el = new HTMLAnchorElement('a');
  else if (tag === 'audio' || tag === 'video') el = new HTMLMediaElement(tag);
  else el = new HTMLElement(tag);
  return el;
}

// build from native tree: string = text, [tag, attrs, ...children], {c: comment}
function build(t) {
  if (typeof t === 'string') return new Text(t);
  if (!Array.isArray(t)) return new Comment(t.c || '');
  const el = createEl(t[0]);
  const at = t[1];
  for (const k in at) { el._attrs[k] = at[k]; if (k === 'style') parseInline(at[k], el.style); }
  for (let i = 2; i < t.length; i++) { const c = build(t[i]); c.parentNode = el; el.childNodes.push(c); }
  if (el.tagName === 'IMG' && at.src) el.src = at.src;
  return el;
}
function __connected(el) {}

function esc(s) { return s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;'); }
function serialize(n) {
  if (n.nodeType === 3) return (n.parentNode && (n.parentNode.tagName === 'SCRIPT' || n.parentNode.tagName === 'STYLE')) ? n.data : esc(n.data);
  if (n.nodeType === 8) return '<!--' + n.data + '-->';
  if (n.nodeType !== 1) return n.childNodes.map(serialize).join('');
  let s = '<' + n.localName;
  for (const k in n._attrs) s += ' ' + k + '="' + String(n._attrs[k]).replace(/"/g, '&quot;') + '"';
  s += '>';
  if (VOID.has(n.localName)) return s;
  return s + n.childNodes.map(serialize).join('') + '</' + n.localName + '>';
}
function findAll(root, pred) { const r = []; (function w(n) { for (const c of n.childNodes) { if (pred(c)) r.push(c); w(c); } })(root); return r; }
function findFirst(root, pred) {
  for (const c of root.childNodes) { if (pred(c)) return c; const f = findFirst(c, pred); if (f) return f; }
  return null;
}

// ---------------- selectors ----------------
// supports: tag, #id, .class, [attr], [attr=v], *, :first-child, :last-child, :not(x), descendant ' ', child '>', comma
const selCache = new Map();
function parseCompound(s) {
  const c = { tag: null, id: null, cls: [], attrs: [], pseudo: [] };
  const re = /([#.]?[-\w]+|\*|\[[^\]]+\]|:[-\w]+(\([^)]*\))?)/g;
  let m;
  while ((m = re.exec(s))) {
    const t = m[1];
    if (t === '*') continue;
    if (t[0] === '#') c.id = t.slice(1);
    else if (t[0] === '.') c.cls.push(t.slice(1));
    else if (t[0] === '[') {
      const mm = /^\[\s*([-\w]+)\s*(?:([~|^$*]?=)\s*["']?([^"'\]]*)["']?)?\s*\]$/.exec(t);
      if (mm) c.attrs.push({ k: mm[1].toLowerCase(), op: mm[2], v: mm[3] });
    } else if (t[0] === ':') c.pseudo.push(t.slice(1));
    else c.tag = t.toUpperCase();
  }
  return c;
}
function parseSel(s) {
  if (selCache.has(s)) return selCache.get(s);
  const out = [];
  for (const part of s.split(',')) {
    const toks = part.trim().replace(/\s*>\s*/g, ' > ').split(/\s+/).filter(x => x);
    const chain = []; let comb = ' ';
    for (const t of toks) { if (t === '>') { comb = '>'; continue; } chain.push({ comb, c: parseCompound(t) }); comb = ' '; }
    if (chain.length) out.push(chain);
  }
  selCache.set(s, out);
  return out;
}
function matchCompound(el, c) {
  if (el.nodeType !== 1) return false;
  if (c.tag && el.tagName !== c.tag) return false;
  if (c.id && el._attrs.id !== c.id) return false;
  if (c.cls.length) { const cl = (el._attrs['class'] || '').split(/\s+/); for (const x of c.cls) if (!cl.includes(x)) return false; }
  for (const a of c.attrs) {
    const v = el._attrs[a.k];
    if (v === undefined) return false;
    if (a.op === '=' && v !== a.v) return false;
    if (a.op === '^=' && !v.startsWith(a.v)) return false;
    if (a.op === '$=' && !v.endsWith(a.v)) return false;
    if (a.op === '*=' && v.indexOf(a.v) < 0) return false;
    if (a.op === '~=' && !v.split(/\s+/).includes(a.v)) return false;
  }
  for (const p of c.pseudo) {
    const par = el.parentNode;
    if (p === 'first-child' && par && par.children[0] !== el) return false;
    if (p === 'last-child' && par && par.children[par.children.length - 1] !== el) return false;
    if (p.startsWith('not(') && parseSel(p.slice(4, -1)).some(s => matchSel(el, s))) return false;
    if (p === 'hover' || p === 'active' || p === 'focus' || p === 'visited' || p === 'checked') return false;
  }
  return true;
}
function matchSel(el, chain, i) {
  if (i === undefined) i = chain.length - 1;
  if (!matchCompound(el, chain[i].c)) return false;
  if (i === 0) return true;
  const comb = chain[i].comb;
  if (comb === '>') return el.parentNode && el.parentNode.nodeType === 1 ? matchSel(el.parentNode, chain, i - 1) : false;
  for (let p = el.parentNode; p && p.nodeType === 1; p = p.parentNode) if (matchSel(p, chain, i - 1)) return true;
  return false;
}
function qsa(root, s, first) {
  const sels = parseSel(s); const r = [];
  (function w(n) {
    for (const c of n.childNodes) {
      if (c.nodeType !== 1) continue;
      if (sels.some(x => matchSel(c, x))) { r.push(c); if (first) return true; }
      if (w(c) && first) return true;
    }
  })(root);
  return r;
}

// ---------------- CSS cascade ----------------
let sheets = [];   // [{chain, spec, decl, order}]
function specificity(chain) {
  let a = 0, b = 0, c = 0;
  for (const p of chain) { if (p.c.id) a++; b += p.c.cls.length + p.c.attrs.length + p.c.pseudo.length; if (p.c.tag) c++; }
  return a * 10000 + b * 100 + c;
}
function parseCSS(text) {
  text = text.replace(/\/\*[\s\S]*?\*\//g, '');
  // drop @media/@keyframes blocks conservatively: keep inner rules of @media
  const re = /([^{}]+)\{([^{}]*)\}/g;
  let m, order = sheets.length;
  text = text.replace(/@(keyframes|font-face|-webkit-keyframes)[^{]*\{(?:[^{}]*\{[^{}]*\})*[^{}]*\}/g, '');
  text = text.replace(/@media[^{]*\{/g, '');
  while ((m = re.exec(text))) {
    let sel = m[1].trim();
    if (sel.startsWith('@')) continue;
    const decl = {}; parseInline(m[2], decl);
    for (const s of sel.split(',')) {
      const ss = s.trim().replace(/::?(before|after|placeholder|selection|-webkit-[-\w]+)/g, '');
      if (!ss || /::/.test(s)) continue;
      const chains = parseSel(ss);
      for (const ch of chains) sheets.push({ chain: ch, spec: specificity(ch), decl, order: order++ });
    }
  }
  sheets.sort((x, y) => x.spec - y.spec || x.order - y.order);
}
const INHERIT = ['color', 'fontWeight', 'fontStyle', 'fontFamily', 'fontSize', 'textAlign', 'whiteSpace', 'visibility', 'lineHeight', 'textDecoration', 'cursor'];
function computeStyles() {
  (function w(n, inh) {
    for (const c of n.childNodes) {
      if (c.nodeType !== 1) continue;
      const cs = {};
      for (const k of INHERIT) if (inh[k] !== undefined) cs[k] = inh[k];
      if (c.tagName === 'A' && c._attrs.href !== undefined) cs.color = '#0645ad';
      for (const r of sheets) if (matchSel(c, r.chain)) Object.assign(cs, r.decl);
      if (c._style) for (const k of Object.keys(c._style)) cs[k] = c._style[k];
      if (c._attrs.hidden !== undefined) cs.display = 'none';
      if (c._attrs.bgcolor) cs.backgroundColor = cs.backgroundColor || c._attrs.bgcolor;
      if (c._attrs.color && c.tagName === 'FONT') cs.color = c._attrs.color;
      if (c._attrs.align) cs.textAlign = cs.textAlign || c._attrs.align;
      if (c.tagName === 'CENTER') cs.textAlign = 'center';
      if (c._attrs.text && c.tagName === 'BODY') cs.color = cs.color || c._attrs.text;
      for (const k in cs) if (cs[k] === 'inherit') cs[k] = inh[k];
      c.__cs = cs;
      w(c, cs);
    }
  })(G.document, {});
}

// ---------------- document ----------------
class Document extends Node {
  constructor() {
    super(9, '#document');
    this.readyState = 'loading'; this.activeElement = null; this.pointerLockElement = null; this.fullscreenElement = null;
    this.visibilityState = 'visible'; this.hidden = false; this.cookie = '';
    this.defaultView = G;
  }
  get documentElement() { return this.childNodes.find(n => n.nodeType === 1) || null; }
  get head() { return this.querySelector('head'); }
  get body() { return this.querySelector('body'); }
  get title() { const t = this.querySelector('title'); return t ? t.textContent : ''; }
  set title(v) { let t = this.querySelector('title'); if (!t) { t = createEl('title'); if (this.head) this.head.appendChild(t); } t.textContent = v; N.title(String(v)); }
  createElement(t) { return createEl(t); }
  createElementNS(ns, t) { return createEl(t); }
  createTextNode(d) { return new Text(d); }
  createComment(d) { return new Comment(d); }
  createDocumentFragment() { return new DocumentFragment(); }
  createEvent() { return new Event(''); }
  getElementById(id) { return findFirst(this, n => n.nodeType === 1 && n._attrs.id === id); }
  getElementsByTagName(t) { t = t.toUpperCase(); return findAll(this, n => n.nodeType === 1 && (t === '*' || n.tagName === t)); }
  getElementsByClassName(c) { return Element.prototype.getElementsByClassName.call(this, c); }
  getElementsByName(nm) { return findAll(this, n => n.nodeType === 1 && n._attrs.name === nm); }
  querySelector(s) { return qsa(this, s, true)[0] || null; }
  querySelectorAll(s) { return qsa(this, s, false); }
  exitPointerLock() { this.pointerLockElement = null; this.dispatchEvent(new Event('pointerlockchange')); }
  exitFullscreen() { this.fullscreenElement = null; return Promise.resolve(); }
  hasFocus() { return true; }
  write(s) { const b = this.body || this.documentElement; if (b) b.insertAdjacentHTML('beforeend', s); }
  writeln(s) { this.write(s + '\n'); }
  open() {} close() {}
  elementFromPoint() { return this.body; }
}

// ---------------- timers ----------------
let timers = [], timerId = 1, rafs = [], rafId = 1;
function setTimeout(fn, ms, ...args) {
  const id = timerId++;
  timers.push({ id, fn, at: N.now() + (+ms || 0), args, iv: 0 });
  return id;
}
function setInterval(fn, ms, ...args) {
  const id = timerId++; ms = Math.max(+ms || 0, 10);
  timers.push({ id, fn, at: N.now() + ms, args, iv: ms });
  return id;
}
function clearTimeout(id) { timers = timers.filter(t => t.id !== id); }
function requestAnimationFrame(fn) { const id = rafId++; rafs.push({ id, fn }); return id; }
function cancelAnimationFrame(id) { rafs = rafs.filter(r => r.id !== id); }
function queueMicrotask(fn) { Promise.resolve().then(fn); }

function tick(now) {
  // timers due
  let guard = 0;
  for (;;) {
    let best = null;
    for (const t of timers) if (t.at <= now && (!best || t.at < best.at)) best = t;
    if (!best || ++guard > 64) break;
    if (best.iv) best.at = Math.max(best.at + best.iv, now - best.iv); else timers.splice(timers.indexOf(best), 1);
    try { if (typeof best.fn === 'function') best.fn(...best.args); else (0, eval)(String(best.fn)); } catch (e) { __err(e); }
  }
  const r = rafs; rafs = [];
  for (const x of r) { try { x.fn(now); } catch (e) { __err(e); } }
  return rafs.length > 0;
}
function nextTimer() { let m = Infinity; for (const t of timers) if (t.at < m) m = t.at; return rafs.length ? 0 : m; }

// ---------------- window ----------------
const store = () => { const d = new Map(); return {
  getItem: k => d.has(String(k)) ? d.get(String(k)) : null, setItem: (k, v) => d.set(String(k), String(v)),
  removeItem: k => d.delete(String(k)), clear: () => d.clear(), key: i => [...d.keys()][i] ?? null, get length() { return d.size; } }; };

function fmt(a) {
  return a.map(x => {
    if (typeof x === 'string') return x;
    if (x instanceof Error) return String(x);
    if (typeof x === 'object' && x !== null) { try { const s = JSON.stringify(x); return s && s.length > 200 ? s.slice(0, 200) + '…' : s; } catch (e) { return String(x); } }
    return String(x);
  }).join(' ');
}
const console = {
  log: (...a) => N.log(0, fmt(a)), info: (...a) => N.log(0, fmt(a)), debug: (...a) => N.log(0, fmt(a)),
  warn: (...a) => N.log(1, fmt(a)), error: (...a) => N.log(2, fmt(a)), trace: (...a) => N.log(0, fmt(a)),
  assert: (c, ...a) => { if (!c) N.log(2, 'Assertion failed: ' + fmt(a)); },
  time() {}, timeEnd() {}, group() {}, groupEnd() {}, groupCollapsed() {}, table: (x) => N.log(0, fmt([x])), dir: (x) => N.log(0, fmt([x])), clear() {},
};

class Response {
  constructor(body, ok) { this._b = body; this.ok = ok; this.status = ok ? 200 : 404; this.statusText = ok ? 'OK' : 'Not Found'; this.headers = { get: () => null }; }
  text() { return Promise.resolve(this._b == null ? '' : (typeof this._b === 'string' ? this._b : '')); }
  json() { return this.text().then(t => JSON.parse(t)); }
  arrayBuffer() { const b = N.loadBinary(this._url); return Promise.resolve(b || new ArrayBuffer(0)); }
  blob() { return this.arrayBuffer(); }
}
function fetch(url) {
  url = typeof url === 'string' ? url : (url && url.url) || String(url);
  return new Promise((res, rej) => {
    setTimeout(() => {
      const t = N.loadText(url);
      if (t === null && !N.exists(url)) { const r = new Response(null, false); r._url = url; res(r); return; }
      const r = new Response(t, true); r._url = url; res(r);
    }, 0);
  });
}
class XMLHttpRequest extends EventTarget {
  constructor() { super(); this.readyState = 0; this.status = 0; this.responseType = ''; this.response = null; this.responseText = ''; }
  open(m, url) { this._url = url; this.readyState = 1; }
  setRequestHeader() {} overrideMimeType() {} getAllResponseHeaders() { return ''; } getResponseHeader() { return null; } abort() {}
  send() {
    setTimeout(() => {
      const bin = this.responseType === 'arraybuffer' || this.responseType === 'blob';
      const v = bin ? N.loadBinary(this._url) : N.loadText(this._url);
      this.readyState = 4; this.status = v == null ? 404 : 200;
      if (this.responseType === 'json') { try { this.response = JSON.parse(v); } catch (e) { this.response = null; } }
      else this.response = v;
      if (!bin) this.responseText = v || '';
      this.dispatchEvent(new Event('readystatechange'));
      this.dispatchEvent(new Event(v == null ? 'error' : 'load'));
      this.dispatchEvent(new Event('loadend'));
    }, 0);
  }
}

const perfStart = N.now();
const location = { href: N.url(), protocol: 'file:', host: '', hostname: '', port: '', pathname: N.url(), search: '', hash: '', origin: 'null',
  reload() { N.navigate(N.url()); }, assign(u) { N.navigate(String(u)); }, replace(u) { N.navigate(String(u)); }, toString() { return this.href; } };

Object.setPrototypeOf(G, EventTarget.prototype);
const doc = new Document();
Object.assign(G, {
  window: G, self: G, top: G, parent: G, globalThis: G, frames: [],
  document: doc, console, location,
  navigator: { userAgent: 'Mozilla/5.0 (TI-Nspire CX; Ndless) nspGL/1.0', platform: 'TI-Nspire', language: 'en-US', languages: ['en-US'],
               hardwareConcurrency: 1, maxTouchPoints: 0, onLine: false, vendor: '', appVersion: '5.0', getGamepads: () => [], vibrate: () => false },
  screen: { width: 320, height: 240, availWidth: 320, availHeight: 240, colorDepth: 16, orientation: { type: 'landscape-primary', angle: 0 } },
  history: { length: 1, state: null, back() { N.back(); }, forward() {}, go() {}, pushState() {}, replaceState() {} },
  performance: { now: () => N.now() - perfStart, timeOrigin: perfStart, mark() {}, measure() {}, getEntriesByName: () => [] },
  innerWidth: 320, innerHeight: 240, outerWidth: 320, outerHeight: 240, devicePixelRatio: 1, scrollX: 0, scrollY: 0, pageXOffset: 0, pageYOffset: 0,
  setTimeout, setInterval, clearTimeout, clearInterval: clearTimeout, requestAnimationFrame, cancelAnimationFrame,
  webkitRequestAnimationFrame: requestAnimationFrame, mozRequestAnimationFrame: requestAnimationFrame, queueMicrotask,
  alert: (m) => N.alert(String(m === undefined ? '' : m)), confirm: (m) => { N.alert(String(m)); return true; }, prompt: (m, d) => d || '',
  getComputedStyle: (el) => { computeStyles(); const cs = Object.assign({}, el.__cs || {}); cs.getPropertyValue = k => cs[camel(k)] || ''; return cs; },
  matchMedia: (q) => ({ matches: false, media: q, addListener() {}, removeListener() {}, addEventListener() {}, removeEventListener() {} }),
  localStorage: store(), sessionStorage: store(),
  scrollTo() {}, scrollBy() {}, scroll() {}, focus() {}, blur() {}, open() { return null; }, close() {}, print() {}, postMessage() {},
  fetch, XMLHttpRequest, Response,
  Event, UIEvent, KeyboardEvent, MouseEvent, WheelEvent, PointerEvent, TouchEvent, CustomEvent, EventTarget,
  Node, Element, HTMLElement, HTMLCanvasElement, HTMLImageElement, HTMLScriptElement, HTMLInputElement, HTMLAnchorElement, HTMLMediaElement,
  Text, Comment, DocumentFragment, Document, DOMTokenList,
  Image: function Image(w, h) { const i = new HTMLImageElement(); if (w) i.setAttribute('width', w); if (h) i.setAttribute('height', h); return i; },
  Audio: function Audio() { return new HTMLMediaElement('audio'); },
  AudioContext: undefined, Worker: undefined,
  ResizeObserver: class { constructor(cb) { this.cb = cb; } observe(el) { setTimeout(() => this.cb([{ target: el, contentRect: el.getBoundingClientRect() }], this), 0); } unobserve() {} disconnect() {} },
  MutationObserver: class { observe() {} disconnect() {} takeRecords() { return []; } },
  IntersectionObserver: class { constructor(cb) { this.cb = cb; } observe(el) { setTimeout(() => this.cb([{ target: el, isIntersecting: true, intersectionRatio: 1 }], this), 0); } unobserve() {} disconnect() {} },
  URL: class { constructor(u) { this.href = String(u); } toString() { return this.href; } static createObjectURL() { return ''; } static revokeObjectURL() {} },
  Blob: class { constructor(parts) { this.parts = parts; this.size = 0; } },
  TextDecoder: class { decode(b) { const u = b instanceof ArrayBuffer ? new Uint8Array(b) : new Uint8Array(b.buffer, b.byteOffset, b.byteLength); let s = ''; for (let i = 0; i < u.length; i++) s += String.fromCharCode(u[i]); try { return decodeURIComponent(escape(s)); } catch (e) { return s; } } },
  TextEncoder: class { encode(s) { s = unescape(encodeURIComponent(s)); const u = new Uint8Array(s.length); for (let i = 0; i < s.length; i++) u[i] = s.charCodeAt(i); return u; } },
  atob: (s) => { const k = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/'; let o = '', b = 0, n = 0; for (const c of String(s).replace(/[^A-Za-z0-9+/]/g, '')) { b = (b << 6) | k.indexOf(c); n += 6; if (n >= 8) { n -= 8; o += String.fromCharCode((b >> n) & 255); } } return o; },
  btoa: (s) => { const k = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/'; let o = ''; s = String(s); for (let i = 0; i < s.length; i += 3) { const a = s.charCodeAt(i), b = s.charCodeAt(i + 1), c = s.charCodeAt(i + 2); o += k[a >> 2] + k[((a & 3) << 4) | (b >> 4 || 0)] + (isNaN(b) ? '=' : k[((b & 15) << 2) | (c >> 6 || 0)]) + (isNaN(c) ? '=' : k[c & 63]); } return o; },
  structuredClone: (x) => JSON.parse(JSON.stringify(x)),
});
doc.ownerDocument = null;
for (const k of ['addEventListener', 'removeEventListener', 'dispatchEvent']) G[k] = EventTarget.prototype[k].bind(G);

// internal API for the C side
G.__rt = {
  build(tree) {
    for (const t of tree) { const n = build(t); n.parentNode = doc; doc.childNodes.push(n); }
    // ensure html/head/body
    let html = doc.documentElement;
    if (!html || html.tagName !== 'HTML') {
      html = createEl('html'); const kids = doc.childNodes.slice(); doc.childNodes = [];
      doc.appendChild(html); for (const k of kids) if (k.nodeType === 1 || k.nodeType === 3) html.appendChild(k);
    }
    if (!html.querySelector('head')) html.insertBefore(createEl('head'), html.firstChild);
    if (!html.querySelector('body')) {
      const body = createEl('body');
      for (const k of html.childNodes.slice()) if (k.tagName !== 'HEAD') body.appendChild(k);
      html.appendChild(body);
    }
    doc.activeElement = doc.body;
    for (const s of doc.querySelectorAll('style')) parseCSS(s.textContent);
    const t = doc.querySelector('title'); if (t) N.title(t.textContent);
  },
  addCSS: parseCSS,
  styles() { computeStyles(); },
  scripts() { return doc.querySelectorAll('script'); },
  tick, nextTimer,
  ready() {
    doc.readyState = 'interactive';
    doc.dispatchEvent(new Event('readystatechange'));
    doc.dispatchEvent(new Event('DOMContentLoaded', { bubbles: true }));
    doc.readyState = 'complete';
    doc.dispatchEvent(new Event('readystatechange'));
    const ev = new Event('load'); ev.target = doc;
    __fire(G, ev);
  },
  key(type, key, code, keyCode, shift, repeat) {
    const t = doc.activeElement || doc.body || doc;
    const ev = new KeyboardEvent(type, { key, code, keyCode, which: keyCode, charCode: type === 'keypress' ? keyCode : 0, shiftKey: !!shift, repeat: !!repeat, bubbles: true, cancelable: true });
    t.dispatchEvent(ev);
    return ev.defaultPrevented;
  },
  mouse(type, x, y, el, dx, dy, buttons) {
    const target = el || doc.body || doc;
    const r = target.getBoundingClientRect ? target.getBoundingClientRect() : { left: 0, top: 0 };
    const init = { clientX: x, clientY: y, pageX: x, pageY: y, screenX: x, screenY: y, offsetX: x - r.left, offsetY: y - r.top, movementX: dx || 0, movementY: dy || 0, button: 0, buttons: buttons || 0, bubbles: true, cancelable: true };
    const ev = type === 'wheel' ? new WheelEvent(type, Object.assign(init, { deltaY: dy })) : type.startsWith('pointer') ? new PointerEvent(type, init) : new MouseEvent(type, init);
    target.dispatchEvent(ev);
    if (type === 'click' && !ev.defaultPrevented) {
      for (let n = target; n && n.nodeType === 1; n = n.parentNode) if (n.tagName === 'A' && n._attrs.href) { N.navigate(n._attrs.href); break; }
    }
  },
  resize() { const e = new Event('resize'); __fire(G, e); },
  hasKeys() { return !!G.__keyListeners; },
  hasMouse() { return !!G.__mouseListeners; },
  err: __err,
};
})(globalThis);
