/* Saltbound hub ship plans: the drawing kit (ink and wash on parchment).
   Shared by every sheet in HubShip_Plans.html. Plain canvas 2D, no libraries. All sizes in px unless noted. */
'use strict';

const FL = '"IM Fell English","SB Fell",Georgia,serif';
const FSC = '"IM Fell English SC","SB Fell SC",Georgia,serif';
const FT = '"Alegreya","SB Alegreya",Georgia,serif';
const INK = 'rgba(52,32,18,.94)';
const ink = a => `rgba(52,32,18,${a})`;
const PAPER = 'rgb(230,212,170)';

// Colours of the washes (sRGB). Turquoise is only for the ancient things (UI spec, section 3).
const C = {
  open: [222, 158, 70],     // warm honey: open at the start (the 20 %)
  locked: [118, 128, 136],  // cool grey, hatched: planned space, locked, not designed
  water: [84, 140, 164],
  ancient: [226, 222, 204], // pale seamless ancient material
  glow: [70, 205, 186],     // restrained turquoise (MI_AncientGlow)
  timber: [122, 80, 44],
  timberDark: [70, 46, 27],
  timberLight: [176, 128, 78],
  ochre: [190, 140, 62],
  cream: [226, 206, 160],
  oxblood: [138, 44, 30],
  iron: [44, 40, 36],
  brass: [196, 150, 66],
  route: [150, 38, 28],
  sky: [200, 212, 210],
  lamp: [255, 200, 110],
};
const rgba = (c, a = 1) => `rgba(${c[0] | 0},${c[1] | 0},${c[2] | 0},${a})`;
const mul = (c, k) => c.map(v => Math.max(0, Math.min(255, v * k)));
const mix = (a, b, t) => a.map((v, i) => v + (b[i] - v) * t);

function rng(seed) { let s = seed >>> 0 || 1; return () => { s = Math.imul(s ^ s >>> 15, 1 | s); s = s + Math.imul(s ^ s >>> 7, 61 | s) ^ s; return ((s ^ s >>> 14) >>> 0) / 4294967296; }; }
function canvas(w, h) { const c = document.createElement('canvas'); c.width = w; c.height = h; return c; }
function bbox(pts) { let x0 = 1e9, y0 = 1e9, x1 = -1e9, y1 = -1e9; for (const p of pts) { x0 = Math.min(x0, p[0]); y0 = Math.min(y0, p[1]); x1 = Math.max(x1, p[0]); y1 = Math.max(y1, p[1]); } return { x: x0, y: y0, w: x1 - x0, h: y1 - y0 }; }

/* ---------- parchment ---------- */
const PARCH_CACHE = {};
function parchment(w, h, seed = 7) {
  const key = w + 'x' + h + '_' + seed; if (PARCH_CACHE[key]) return PARCH_CACHE[key];
  const cv = canvas(w, h), c = cv.getContext('2d'), r = rng(seed);
  c.fillStyle = '#e4cf9e'; c.fillRect(0, 0, w, h);
  for (let i = 0; i < 300; i++) {
    const x = r() * w, y = r() * h, rad = 80 + r() * 460, dark = r() < 0.5;
    const g = c.createRadialGradient(x, y, 0, x, y, rad), col = dark ? [150, 108, 58] : [252, 238, 204];
    g.addColorStop(0, rgba(col, 0.045 + r() * 0.06)); g.addColorStop(1, rgba(col, 0));
    c.fillStyle = g; c.fillRect(x - rad, y - rad, rad * 2, rad * 2);
  }
  for (let i = 0; i < w * h / 1400; i++) {
    const x = r() * w, y = r() * h, l = 4 + r() * 16, a = r() * Math.PI;
    c.strokeStyle = ink(0.025 + r() * 0.035); c.lineWidth = 0.5 + r() * 0.6;
    c.beginPath(); c.moveTo(x, y); c.lineTo(x + Math.cos(a) * l, y + Math.sin(a) * l); c.stroke();
  }
  for (let i = 0; i < w * h / 3000; i++) { c.fillStyle = ink(0.04 + r() * 0.1); c.beginPath(); c.arc(r() * w, r() * h, 0.4 + r() * 1.2, 0, 7); c.fill(); }
  for (let i = 0; i < 3; i++) {
    const x = r() * w, y = r() * h, rad = 30 + r() * 60;
    c.strokeStyle = rgba([140, 96, 46], 0.035 + r() * 0.03); c.lineWidth = 1.5 + r() * 2;
    c.beginPath(); c.ellipse(x, y, rad, rad * (0.6 + r() * 0.4), r() * 3, 0, 7); c.stroke();
  }
  const g = c.createRadialGradient(w / 2, h / 2, Math.min(w, h) * 0.38, w / 2, h / 2, Math.hypot(w, h) * 0.56);
  g.addColorStop(0, 'rgba(120,70,20,0)'); g.addColorStop(1, 'rgba(105,58,16,0.40)');
  c.fillStyle = g; c.fillRect(0, 0, w, h);
  PARCH_CACHE[key] = cv; return cv;
}

/* ---------- ink lines with a slight hand wobble ---------- */
function wobbler(seed, amp) { const r = rng(seed * 7919 + 13), p = [r() * 99, r() * 99, r() * 99]; return d => amp * (Math.sin(d * 0.021 + p[0]) * 0.55 + Math.sin(d * 0.057 + p[1]) * 0.3 + Math.sin(d * 0.13 + p[2]) * 0.15); }
function resample(pts, closed, step = 5) {
  const out = [], n = pts.length, segs = closed ? n : n - 1; let dist = 0;
  for (let i = 0; i < segs; i++) {
    const a = pts[i], b = pts[(i + 1) % n], L = Math.hypot(b[0] - a[0], b[1] - a[1]) || 1e-6, k = Math.max(1, Math.ceil(L / step));
    const nx = -(b[1] - a[1]) / L, ny = (b[0] - a[0]) / L;
    for (let j = 0; j < k; j++) { const t = j / k; out.push([a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, nx, ny, dist + L * t]); }
    dist += L;
  }
  if (!closed) { const a = pts[n - 2], b = pts[n - 1], L = Math.hypot(b[0] - a[0], b[1] - a[1]) || 1; out.push([b[0], b[1], -(b[1] - a[1]) / L, (b[0] - a[0]) / L, dist]); }
  return out;
}
function line(c, pts, o = {}) {
  const { w = 2, col = INK, amp = 0.7, closed = false, seed = 1, dash = null, cap = 'round', alpha = 1 } = o;
  if (pts.length < 2) return;
  const f = wobbler(seed, amp), rs = resample(pts, closed);
  c.save(); c.globalAlpha *= alpha; c.strokeStyle = col; c.lineWidth = w; c.lineCap = cap; c.lineJoin = 'round'; if (dash) c.setLineDash(dash);
  c.beginPath(); rs.forEach((p, i) => { const d = f(p[4]), x = p[0] + p[2] * d, y = p[1] + p[3] * d; i ? c.lineTo(x, y) : c.moveTo(x, y); });
  if (closed) c.closePath(); c.stroke(); c.restore();
}
const seg = (c, a, b, o) => line(c, [a, b], o);
function pathOf(c, pts) { c.beginPath(); pts.forEach((p, i) => i ? c.lineTo(p[0], p[1]) : c.moveTo(p[0], p[1])); c.closePath(); }
function fill(c, pts, col) { c.save(); pathOf(c, pts); c.fillStyle = col; c.fill(); c.restore(); }
/** A watercolour wash: flat colour, a darker soft edge where the pigment pooled, a little granulation. */
function wash(c, pts, col, a = 0.45, seed = 3) {
  c.save(); pathOf(c, pts); c.fillStyle = rgba(col, a); c.fill(); c.clip();
  c.filter = 'blur(3px)'; c.strokeStyle = rgba(mul(col, 0.72), Math.min(1, a * 0.85)); c.lineWidth = 6; pathOf(c, pts); c.stroke(); c.filter = 'none';
  const r = rng(seed), bb = bbox(pts), n = Math.min(4000, bb.w * bb.h / 700);
  for (let i = 0; i < n; i++) { c.fillStyle = rgba(mul(col, 0.66), 0.05 + r() * 0.08); c.fillRect(bb.x + r() * bb.w, bb.y + r() * bb.h, 1.6, 1.6); }
  c.restore();
}
function hatch(c, pts, o = {}) {
  const { ang = 45, gap = 9, col = ink(0.32), w = 1.1, seed = 5 } = o, bb = bbox(pts);
  c.save(); pathOf(c, pts); c.clip();
  const a = ang * Math.PI / 180, dx = Math.cos(a), dy = Math.sin(a), cx = bb.x + bb.w / 2, cy = bb.y + bb.h / 2, R = Math.hypot(bb.w, bb.h) / 2 + 6, nx = -dy, ny = dx;
  for (let t = -R, k = 0; t <= R; t += gap, k++) seg(c, [cx + nx * t - dx * R, cy + ny * t - dy * R], [cx + nx * t + dx * R, cy + ny * t + dy * R], { w, col, amp: 0.35, seed: seed + k });
  c.restore();
}
const rectPts = (x, y, w, h) => [[x, y], [x + w, y], [x + w, y + h], [x, y + h]];
function ellipsePts(cx, cy, rx, ry, n = 48, a0 = 0, a1 = Math.PI * 2) { const p = []; for (let i = 0; i <= n; i++) { const a = a0 + (a1 - a0) * i / n; p.push([cx + Math.cos(a) * rx, cy + Math.sin(a) * ry]); } return p; }

/* ---------- text ---------- */
function font(size, f = FT, style = '', weight = 400) { return `${style} ${weight} ${size}px ${f}`; }
function txt(c, s, x, y, o = {}) {
  const { size = 22, f = FT, col = INK, align = 'left', style = '', weight = 400, base = 'alphabetic', alpha = 1, rot = 0, halo = false, spacing = 0 } = o;
  c.save(); c.globalAlpha *= alpha; c.font = font(size, f, style, weight); c.textAlign = align; c.textBaseline = base;
  if (spacing) c.letterSpacing = spacing + 'px';
  c.translate(x, y); if (rot) c.rotate(rot);
  if (halo) { c.strokeStyle = 'rgba(232,214,172,0.9)'; c.lineWidth = Math.max(4, size * 0.28); c.lineJoin = 'round'; c.strokeText(s, 0, 0); }
  c.fillStyle = col; c.fillText(s, 0, 0); c.restore();
}
function measure(c, s, size, f = FT, style = '', weight = 400) { c.save(); c.font = font(size, f, style, weight); const w = c.measureText(s).width; c.restore(); return w; }
function wrap(c, s, maxW, size, f = FT, style = '', weight = 400) {
  c.save(); c.font = font(size, f, style, weight); const out = [];
  for (const par of s.split('\n')) { let cur = ''; for (const w of par.split(' ')) { const t = cur ? cur + ' ' + w : w; if (c.measureText(t).width > maxW && cur) { out.push(cur); cur = w; } else cur = t; } out.push(cur); }
  c.restore(); return out;
}
/** A wrapped paragraph; returns the y under it. */
function para(c, s, x, y, maxW, o = {}) {
  const { size = 21, lh = Math.round(size * 1.36), f = FT, style = '', col = INK, weight = 400 } = o;
  const lines = wrap(c, s, maxW, size, f, style, weight);
  lines.forEach((l, i) => txt(c, l, x, y + i * lh, { size, f, style, col, weight }));
  return y + lines.length * lh;
}
/** Notes: [['Heading', 'text', 'text'], ...] in a column; returns the y under them. */
function notes(c, list, x, y, maxW, o = {}) {
  const { size = 20, head = 25, gap = 16 } = o;
  for (const [h, ...lines] of list) {
    if (h) { txt(c, h, x, y, { size: head, f: FSC }); y += head * 0.5 + 12; }
    for (const l of lines) {
      const bullet = !l.startsWith('|'), s = bullet ? l : l.slice(1);
      if (bullet) { c.save(); c.fillStyle = INK; c.beginPath(); c.arc(x + 5, y - size * 0.32, 2.6, 0, 7); c.fill(); c.restore(); }
      y = para(c, s, x + (bullet ? 18 : 0), y, maxW - 18, { size }) + 2;
    }
    y += gap;
  }
  return y;
}
function leader(c, from, to, o = {}) {
  const { w = 1.3, col = ink(0.8), dot = true, seed = 9 } = o;
  seg(c, from, to, { w, col, amp: 0.4, seed });
  if (dot) { c.save(); c.fillStyle = col; c.beginPath(); c.arc(to[0], to[1], 3.2, 0, 7); c.fill(); c.restore(); }
}
/** A label at (x, y) with a leader to a target point. */
function callout(c, s, x, y, target, o = {}) {
  const { size = 20, align = 'left', f = FT, style = '', halo = true, sub = null } = o;
  const w = measure(c, s, size, f, style), lx = align === 'left' ? x - 6 : align === 'right' ? x + 6 : x;
  txt(c, s, x, y, { size, f, style, align, halo });
  if (sub) txt(c, sub, x, y + size * 1.15, { size: size * 0.82, style: 'italic', align, halo, col: ink(0.8) });
  if (target) {
    const ax = align === 'left' ? (target[0] < x ? x - 6 : x + w + 6) : align === 'right' ? (target[0] > x ? x + 6 : x - w - 6) : x;
    leader(c, [ax, y - size * 0.35], target, o);
  }
  return lx;
}
function arrow(c, a, b, o = {}) {
  const { w = 2, col = INK, head = 12, dash = null, seed = 4 } = o;
  seg(c, a, b, { w, col, dash, seed });
  const ang = Math.atan2(b[1] - a[1], b[0] - a[0]);
  c.save(); c.fillStyle = col; c.beginPath(); c.moveTo(b[0], b[1]);
  c.lineTo(b[0] - Math.cos(ang - 0.38) * head, b[1] - Math.sin(ang - 0.38) * head);
  c.lineTo(b[0] - Math.cos(ang + 0.38) * head, b[1] - Math.sin(ang + 0.38) * head); c.closePath(); c.fill(); c.restore();
}
function scaleBar(c, x, y, s, metres, step, o = {}) {
  const { label = metres + ' m', h = 9 } = o;
  for (let i = 0; i < metres / step; i++) { const p = rectPts(x + i * step * s, y, step * s, h); fill(c, p, i % 2 ? PAPER : INK); line(c, p, { closed: true, w: 1.2, amp: 0.2 }); }
  for (let i = 0; i <= metres / step; i += metres / step > 6 ? 2 : 1) txt(c, String(i * step), x + i * step * s, y + h + 22, { size: 17, align: 'center' });
  txt(c, label === null ? '' : 'metres', x + metres * s + 14, y + h + 2, { size: 18, style: 'italic' });
}
/** A badge for a service: S shipwright, J jobs, Q main quest, E equipment. */
const SERVICE = {
  S: { name: 'Shipwright', what: 'ships and upgrades' },
  J: { name: 'Tally House', what: 'side jobs, pay in salt' },
  Q: { name: 'Keelgate', what: 'the main quest' },
  E: { name: 'Chandlery', what: 'equipment' },
};
function badge(c, x, y, letter, r = 17) {
  c.save(); c.fillStyle = rgba(C.cream, 1); c.beginPath(); c.arc(x, y, r, 0, 7); c.fill(); c.restore();
  line(c, ellipsePts(x, y, r, r, 28), { w: 2.4, col: rgba(C.route, 1), amp: 0.3, seed: letter.charCodeAt(0) });
  txt(c, letter, x, y + r * 0.42, { size: r * 1.25, f: FSC, align: 'center', col: rgba(C.route, 1) });
}
/** The NPC's spot (the space only: a dashed ring on the floor and the way they face). */
function npcSpot(c, x, y, r, facing = null) {
  line(c, ellipsePts(x, y, r, r * 0.6, 28), { w: 2, col: rgba(C.route, 0.95), dash: [5, 4], amp: 0.2, seed: 3 });
  if (facing !== null) arrow(c, [x, y], [x + Math.cos(facing) * r * 1.9, y + Math.sin(facing) * r * 1.15], { w: 1.8, col: rgba(C.route, 0.95), head: 9 });
}
/** An interaction point: the key scrap of the UI spec, tiny. */
function keyScrap(c, x, y, letter = 'E', size = 26) {
  const r = rng(letter.charCodeAt(0) + (x | 0)), pts = [];
  for (let i = 0; i < 16; i++) { const a = i / 16 * Math.PI * 2 + Math.PI / 4, k = 0.5 * size * (Math.SQRT2 - (r() * 0.08)); const sq = Math.min(1 / Math.abs(Math.cos(a)), 1 / Math.abs(Math.sin(a))); pts.push([x + Math.cos(a) * Math.min(k * 1.05, sq * size * 0.5) * (0.96 + r() * 0.06), y + Math.sin(a) * Math.min(k * 1.05, sq * size * 0.5) * (0.96 + r() * 0.06)]); }
  c.save(); c.translate(x, y); c.rotate(-0.05); c.translate(-x, -y);
  c.shadowColor = 'rgba(40,20,5,0.4)'; c.shadowBlur = 5; c.shadowOffsetY = 2; fill(c, pts, 'rgb(236,220,182)'); c.shadowColor = 'transparent';
  line(c, pts, { closed: true, w: 1, col: ink(0.6), amp: 0.2 });
  txt(c, letter, x, y + size * 0.3, { size: size * 0.78, f: FSC, align: 'center' }); c.restore();
}
/** A standing figure for scale, feet at (x, y), `px` pixels per metre, `h` metres tall. */
function figure(c, x, y, px, o = {}) {
  const { h = 1.4, col = INK, w = 2 } = o, H = h * px;
  const head = H * 0.085, neck = y - H + head * 2.1, hip = y - H * 0.47, sh = neck + H * 0.04;
  c.save(); c.fillStyle = col; c.beginPath(); c.arc(x, y - H + head, head, 0, 7); c.fill(); c.restore();
  line(c, [[x, neck], [x, hip]], { w: w * 1.6, col, amp: 0.2 });
  line(c, [[x - H * 0.09, y], [x, hip], [x + H * 0.09, y]], { w, col, amp: 0.2 });
  line(c, [[x - H * 0.12, hip + H * 0.05], [x, sh], [x + H * 0.12, hip + H * 0.05]], { w, col, amp: 0.2 });
}

/* ---------- sheet frame ---------- */
function sheet(w, h, title, sub, num, total) {
  const cv = canvas(w, h), c = cv.getContext('2d');
  c.drawImage(parchment(w, h, num * 13 + 5), 0, 0);
  line(c, rectPts(22, 22, w - 44, h - 44), { closed: true, w: 3.2, amp: 0.4, seed: num });
  line(c, rectPts(34, 34, w - 68, h - 68), { closed: true, w: 1.2, amp: 0.3, seed: num + 1 });
  txt(c, 'SALTBOUND', 72, 104, { size: 46, f: FSC, spacing: 2 });
  const tx = 72 + measure(c, 'SALTBOUND', 46, FSC) + 2 * 9 + 28;
  txt(c, '–  ' + title, tx, 104, { size: 46, f: FL });
  if (sub) txt(c, sub, 74, 146, { size: 24, style: 'italic' });
  // Cartouche, bottom right.
  const cw = 470, ch = 74, cx = w - 60 - cw, cy = h - 60 - ch;
  fill(c, rectPts(cx, cy, cw, ch), 'rgba(236,222,186,0.6)');
  line(c, rectPts(cx, cy, cw, ch), { closed: true, w: 1.6, amp: 0.3, seed: 77 });
  txt(c, 'The hub ship · design proposal (D60)', cx + 18, cy + 31, { size: 20, f: FL });
  txt(c, `sheet ${num} of ${total} · 2026-10-05 · working name: Hollowkeel`, cx + 18, cy + 58, { size: 16, style: 'italic' });
  return { cv, c };
}
/** The legend of the washes. */
function legend(c, x, y, items = ['open', 'locked', 'water', 'ancient', 'route', 'npc', 'e'], o = {}) {
  const { size = 19, colW = 300, cols = 99 } = o; let i = 0;
  const L = {
    open: ['Open at the start (the 20 %)', p => wash(c, p, C.open, 0.55)],
    locked: ['Planned space, locked, not designed', p => { wash(c, p, C.locked, 0.3); hatch(c, p, { gap: 6 }); }],
    water: ['Sea water', p => wash(c, p, C.water, 0.5)],
    ancient: ['Ancient hull (lost civilisation)', p => { wash(c, p, C.ancient, 0.9); seg(c, [p[0][0] + 4, p[0][1] + 13], [p[1][0] - 4, p[0][1] + 13], { col: rgba(C.glow, 0.9), w: 2 }); }],
    void: ['Open void (seen through)', p => { line(c, [p[0], p[2]], { w: 1, col: ink(0.5) }); line(c, [p[1], p[3]], { w: 1, col: ink(0.5) }); }],
    route: ['The player’s way through', p => seg(c, [p[0][0], p[0][1] + 13], [p[1][0], p[0][1] + 13], { w: 3.2, col: rgba(C.route, 1), dash: [2, 7] })],
    npc: ['Where the NPC stands (space only)', p => npcSpot(c, p[0][0] + 22, p[0][1] + 13, 14)],
    e: ['Interaction point (E)', p => keyScrap(c, p[0][0] + 22, p[0][1] + 13, 'E', 24)],
  };
  for (const k of items) {
    const col = i % cols, row = Math.floor(i / cols), px = x + col * colW, py = y + row * 40;
    const p = rectPts(px, py, 46, 26); L[k][1](p);
    if (k !== 'route' && k !== 'npc' && k !== 'e') line(c, p, { closed: true, w: 1, col: ink(0.6), amp: 0.2 });
    txt(c, L[k][0], px + 60, py + 20, { size }); i++;
  }
}

/* ---------- isometric renderer for the vignettes ---------- */
const C30 = Math.cos(Math.PI / 6);
function Iso(ox, oy, s) { return { s, ox, oy, P: (x, y, z) => [ox + (x - y) * C30 * s, oy + (x + y) * 0.5 * s - z * s] }; }
/** A 3D polygon (list of [x, y, z]) filled and inked. */
function quad(c, iso, pts3, col, o = {}) {
  const { a = 1, w = 1.4, inkA = 0.9, seed = 2, noInk = false } = o, p = pts3.map(q => iso.P(...q));
  fill(c, p, rgba(col, a)); if (!noInk) line(c, p, { closed: true, w, col: ink(inkA), amp: 0.35, seed });
  return p;
}
/** An axis-aligned box: the three faces a viewer at +x +y +z sees. */
function box(c, iso, b) {
  const { x0, x1, y0, y1, z0, z1 } = b, P = iso.P, col = b.col || C.timber, a = b.a ?? 1;
  const top = [P(x0, y0, z1), P(x1, y0, z1), P(x1, y1, z1), P(x0, y1, z1)];
  const fx = [P(x1, y0, z0), P(x1, y1, z0), P(x1, y1, z1), P(x1, y0, z1)];
  const fy = [P(x0, y1, z0), P(x1, y1, z0), P(x1, y1, z1), P(x0, y1, z1)];
  fill(c, fx, rgba(mul(col, b.kx ?? 0.8), a)); fill(c, fy, rgba(mul(col, b.ky ?? 0.62), a)); fill(c, top, rgba(mul(col, b.kt ?? 1.08), a));
  if (b.detail) b.detail(c, iso, b);
  if (b.noInk) return;
  const lw = b.w ?? 1.5, sd = b.seed ?? ((x0 * 7 + y0 * 13 + z0 * 3) | 0);
  line(c, [P(x0, y0, z1), P(x1, y0, z1), P(x1, y0, z0), P(x1, y1, z0), P(x0, y1, z0), P(x0, y1, z1)], { closed: true, w: lw, seed: sd, amp: 0.45 });
  line(c, [P(x1, y0, z1), P(x1, y1, z1), P(x0, y1, z1)], { w: lw * 0.7, seed: sd + 1, amp: 0.35 });
  seg(c, P(x1, y1, z1), P(x1, y1, z0), { w: lw * 0.7, seed: sd + 2, amp: 0.35 });
}
/** A vertical cylinder (barrel, mast, post). */
function cyl(c, iso, cx, cy, r, z0, z1, col, o = {}) {
  const { hoops = [], top = true, w = 1.4 } = o, s = iso.s, rx = 1.2247 * r * s, ry = 0.7071 * r * s;
  const b = iso.P(cx, cy, z0), t = iso.P(cx, cy, z1);
  const g = c.createLinearGradient(b[0] - rx, 0, b[0] + rx, 0);
  g.addColorStop(0, rgba(mul(col, 0.7))); g.addColorStop(0.45, rgba(mul(col, 1.05))); g.addColorStop(1, rgba(mul(col, 0.55)));
  c.save(); c.beginPath(); c.ellipse(b[0], b[1], rx, ry, 0, 0, Math.PI); c.lineTo(t[0] - rx, t[1]); c.ellipse(t[0], t[1], rx, ry, 0, Math.PI, 0, true); c.closePath(); c.fillStyle = g; c.fill(); c.restore();
  for (const hz of hoops) { const hp = iso.P(cx, cy, hz); line(c, ellipsePts(hp[0], hp[1], rx, ry, 24, 0, Math.PI), { w: 2.2, col: rgba(C.iron, 0.9), amp: 0.2 }); }
  if (top) { c.save(); c.beginPath(); c.ellipse(t[0], t[1], rx, ry, 0, 0, 7); c.fillStyle = rgba(mul(col, 1.15)); c.fill(); c.restore(); line(c, ellipsePts(t[0], t[1], rx, ry, 28), { w: w * 0.8, amp: 0.2 }); }
  line(c, [[b[0] - rx, b[1]], [t[0] - rx, t[1]]], { w, amp: 0.3 }); line(c, [[b[0] + rx, b[1]], [t[0] + rx, t[1]]], { w, amp: 0.3 });
  line(c, ellipsePts(b[0], b[1], rx, ry, 24, 0, Math.PI), { w, amp: 0.2 });
}
/** A flat disc on a floor (the Keelgate, a hatch, a spot). */
function disc(c, iso, cx, cy, z, r, col, o = {}) {
  const { a = 1, w = 1.6, n = 64, inkA = 0.9 } = o, p = [];
  for (let i = 0; i < n; i++) { const t = i / n * Math.PI * 2; p.push(iso.P(cx + Math.cos(t) * r, cy + Math.sin(t) * r, z)); }
  if (col) fill(c, p, rgba(col, a)); if (w) line(c, p, { closed: true, w, col: ink(inkA), amp: 0.25 }); return p;
}
/** A hanging lantern with its warm light. */
function lantern(c, iso, x, y, z, zHook, o = {}) {
  const { r = 0.22 } = o, p = iso.P(x, y, z), h = iso.P(x, y, zHook), s = iso.s;
  const g = c.createRadialGradient(p[0], p[1], 0, p[0], p[1], s * 2.4); g.addColorStop(0, rgba(C.lamp, 0.42)); g.addColorStop(1, rgba(C.lamp, 0));
  c.save(); c.fillStyle = g; c.fillRect(p[0] - s * 2.4, p[1] - s * 2.4, s * 4.8, s * 4.8); c.restore();
  seg(c, h, [p[0], p[1] - r * s * 1.4], { w: 1.2, col: ink(0.8), amp: 0.2 });
  fill(c, rectPts(p[0] - r * s * 0.7, p[1] - r * s * 1.3, r * s * 1.4, r * s * 2), rgba(C.lamp, 0.95));
  line(c, rectPts(p[0] - r * s * 0.7, p[1] - r * s * 1.3, r * s * 1.4, r * s * 2), { closed: true, w: 1.4, amp: 0.2 });
}
/** A standing figure in an iso scene (feet at x, y, z). */
function isoFigure(c, iso, x, y, z, label = null) {
  const p = iso.P(x, y, z); figure(c, p[0], p[1], iso.s, { w: 2 });
  if (label) txt(c, label, p[0] + 10, p[1] + 20, { size: 17, style: 'italic', halo: true });
}
/** A boat hull in an iso scene: L long along +x from x0, beam B, from keel zk to sheer zs, centred on y. */
function isoHull(c, iso, x0, y, L, B, zk, zs, o = {}) {
  const { hull = C.oxblood, deck = C.timberLight, rise = 0.5, keelBelow = 0, stations = 22, doubleEnder = true, flip = false } = o;
  const P = iso.P, pts = [], sheerS = [], sheerP = [];
  for (let i = 0; i <= stations; i++) {
    const u = i / stations * 2 - 1, x = flip ? x0 + L - (u + 1) / 2 * L : x0 + (u + 1) / 2 * L;
    const end = doubleEnder ? Math.abs(u) : (u > 0 ? u : Math.abs(u) * 0.55);
    const hb = B / 2 * Math.pow(Math.max(0, 1 - Math.pow(end, 2.2)), 0.6) + (doubleEnder ? 0 : (u < 0 ? B * 0.12 * Math.abs(u) : 0));
    const zsh = zs + rise * u * u, zb = zk + (zs - zk) * (1 - Math.pow(Math.max(0, 1 - Math.pow(end, 2)), 0.7)) * 0.85 - keelBelow;
    for (let k = 0; k <= 8; k++) { const ph = k / 8 * Math.PI / 2, yy = hb * Math.pow(Math.sin(ph), 0.6), zz = zb + (zsh - zb) * (1 - Math.cos(ph)); pts.push(P(x, y + yy, zz)); pts.push(P(x, y - yy, zz)); }
    sheerS.push(P(x, y + hb, zsh)); sheerP.push(P(x, y - hb, zsh));
  }
  const hullPoly = convexHull(pts); fill(c, hullPoly, rgba(hull)); line(c, hullPoly, { closed: true, w: 2, amp: 0.4 });
  // Strakes on the near (+y) side.
  for (const ph of [0.35, 0.6, 0.82]) {
    const s = []; for (let i = 1; i < stations; i++) {
      const u = i / stations * 2 - 1, x = flip ? x0 + L - (u + 1) / 2 * L : x0 + (u + 1) / 2 * L, end = doubleEnder ? Math.abs(u) : (u > 0 ? u : Math.abs(u) * 0.55);
      const hb = B / 2 * Math.pow(Math.max(0, 1 - Math.pow(end, 2.2)), 0.6) + (doubleEnder ? 0 : (u < 0 ? B * 0.12 * Math.abs(u) : 0));
      const zsh = zs + rise * u * u, zb = zk + (zs - zk) * (1 - Math.pow(Math.max(0, 1 - Math.pow(end, 2)), 0.7)) * 0.85 - keelBelow;
      const a = ph * Math.PI / 2; s.push(P(x, y + hb * Math.pow(Math.sin(a), 0.6), zb + (zsh - zb) * (1 - Math.cos(a))));
    }
    line(c, s, { w: 1, col: ink(0.45), amp: 0.3 });
  }
  const deckPoly = sheerS.concat(sheerP.slice().reverse()); fill(c, deckPoly, rgba(deck)); line(c, deckPoly, { closed: true, w: 1.8, amp: 0.3 });
  return { sheerS, sheerP };
}
function convexHull(pts) {
  const p = pts.slice().sort((a, b) => a[0] - b[0] || a[1] - b[1]), cr = (o, a, b) => (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0]);
  const lo = [], up = [];
  for (const q of p) { while (lo.length >= 2 && cr(lo[lo.length - 2], lo[lo.length - 1], q) <= 0) lo.pop(); lo.push(q); }
  for (let i = p.length - 1; i >= 0; i--) { const q = p[i]; while (up.length >= 2 && cr(up[up.length - 2], up[up.length - 1], q) <= 0) up.pop(); up.push(q); }
  up.pop(); lo.pop(); return lo.concat(up);
}
/** Planks drawn along a floor rectangle (lines along x every `gap` metres). */
function floorPlanks(c, iso, x0, x1, y0, y1, z, gap = 0.6, along = 'x') {
  if (along === 'x') for (let y = y0 + gap; y < y1 - 0.01; y += gap) seg(c, iso.P(x0, y, z), iso.P(x1, y, z), { w: 0.8, col: ink(0.22), amp: 0.3, seed: y * 10 });
  else for (let x = x0 + gap; x < x1 - 0.01; x += gap) seg(c, iso.P(x, y0, z), iso.P(x, y1, z), { w: 0.8, col: ink(0.22), amp: 0.3, seed: x * 10 });
}
/** Back walls: x = const plane (spans y, z) and y = const plane (spans x, z), with planks. */
function wallX(c, iso, x, y0, y1, z0, z1, col = C.timber, o = {}) {
  quad(c, iso, [[x, y0, z0], [x, y1, z0], [x, y1, z1], [x, y0, z1]], mul(col, o.k ?? 0.86), { w: 1.6 });
  for (let z = z0 + 0.45; z < z1; z += 0.45) seg(c, iso.P(x, y0, z), iso.P(x, y1, z), { w: 0.7, col: ink(0.2), amp: 0.3, seed: z * 31 });
}
function wallY(c, iso, y, x0, x1, z0, z1, col = C.timber, o = {}) {
  quad(c, iso, [[x0, y, z0], [x1, y, z0], [x1, y, z1], [x0, y, z1]], mul(col, o.k ?? 0.74), { w: 1.6 });
  for (let z = z0 + 0.45; z < z1; z += 0.45) seg(c, iso.P(x0, y, z), iso.P(x1, y, z), { w: 0.7, col: ink(0.2), amp: 0.3, seed: z * 37 });
}
/** An opening drawn on a wall plane: plane 'x' at x (u = y) or 'y' at y (u = x). */
function opening(c, iso, plane, at, u0, u1, z0, z1, col, o = {}) {
  const P = (u, z) => plane === 'x' ? [at, u, z] : [u, at, z];
  return quad(c, iso, [P(u0, z0), P(u1, z0), P(u1, z1), P(u0, z1)], col, o);
}
