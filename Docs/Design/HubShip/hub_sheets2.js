/* Saltbound hub ship plans, sheets 6-11: the deck plans, the locked zones and their gates, the four service spaces. */
'use strict';

/* ---------- plan drawing (shared by the deck plans and the small plans on the space sheets) ---------- */
function planTools(c, P, s) {
  const T = {};
  T.pts = a => a.map(([x, y]) => P(x, y));
  T.rect = r => [P(r.x0, r.y0), P(r.x1, r.y0), P(r.x1, r.y1), P(r.x0, r.y1)];
  T.W = Math.max(2.6, s * 0.28);
  /** Walls round a rectangle; gaps: [[side, a, b], ...] with side 'x0' | 'x1' | 'y0' | 'y1' (a, b along that side). */
  T.walls = (r, gaps = [], w = T.W) => {
    const sides = { x0: [[r.x0, r.y0], [r.x0, r.y1], 'y'], x1: [[r.x1, r.y0], [r.x1, r.y1], 'y'], y0: [[r.x0, r.y0], [r.x1, r.y0], 'x'], y1: [[r.x0, r.y1], [r.x1, r.y1], 'x'] };
    for (const [k, [a, b, ax]] of Object.entries(sides)) {
      const g = gaps.filter(q => q[0] === k).map(q => [Math.min(q[1], q[2]), Math.max(q[1], q[2])]).sort((p, q) => p[0] - q[0]);
      let from = ax === 'y' ? a[1] : a[0]; const to = ax === 'y' ? b[1] : b[0];
      const at = v => ax === 'y' ? [a[0], v] : [v, a[1]];
      for (const [g0, g1] of g) { if (g0 > from) seg(c, P(...at(from)), P(...at(g0)), { w, amp: 0.3 }); from = g1; }
      if (to > from) seg(c, P(...at(from)), P(...at(to)), { w, amp: 0.3 });
    }
  };
  T.box = (x0, x1, y0, y1, col = C.timber, a = 0.9, w = 1.4) => { const p = T.rect({ x0, x1, y0, y1 }); fill(c, p, rgba(col, a)); line(c, p, { closed: true, w, amp: 0.2 }); return p; };
  T.stairs = (r, dir, label = 'up') => {
    const p = T.rect(r); fill(c, p, rgba(C.open, 0.5)); line(c, p, { closed: true, w: 1.4, amp: 0.2 });
    const alongX = dir === '+x' || dir === '-x', n = Math.round((alongX ? r.x1 - r.x0 : r.y1 - r.y0) / 0.64);
    for (let i = 1; i < n; i++) { const t = i / n; if (alongX) { const x = r.x0 + (r.x1 - r.x0) * t; seg(c, P(x, r.y0), P(x, r.y1), { w: 0.8, amp: 0.1 }); } else { const y = r.y0 + (r.y1 - r.y0) * t; seg(c, P(r.x0, y), P(r.x1, y), { w: 0.8, amp: 0.1 }); } }
    const cx = (r.x0 + r.x1) / 2, cy = (r.y0 + r.y1) / 2, L = alongX ? (r.x1 - r.x0) * 0.4 : (r.y1 - r.y0) * 0.4;
    const a = { '+x': [[cx - L, cy], [cx + L, cy]], '-x': [[cx + L, cy], [cx - L, cy]], '+y': [[cx, cy - L], [cx, cy + L]], '-y': [[cx, cy + L], [cx, cy - L]] }[dir];
    arrow(c, P(...a[0]), P(...a[1]), { w: 1.6, head: Math.max(7, s * 0.7) });
    if (label && s > 8) txt(c, label, P(...a[1])[0] + 4, P(...a[1])[1] - 4, { size: Math.max(12, s * 1.1), style: 'italic', halo: true });
  };
  T.boat = (x0, L, yc, B, o = {}) => {
    const { bow = '+x', transom = false, col = C.oxblood, dash = null, a = 0.85 } = o, pts = [];
    for (let i = 0; i <= 24; i++) { const u = i / 24 * 2 - 1, e = transom && u < 0 ? Math.abs(u) * 0.55 : Math.abs(u), hb = B / 2 * Math.pow(Math.max(0, 1 - Math.pow(e, 2.2)), 0.6) + (transom && u < 0 ? B * 0.18 * Math.abs(u) : 0); const x = bow === '+x' ? x0 + (u + 1) / 2 * L : x0 + L - (u + 1) / 2 * L; pts.push([x, yc + hb]); }
    for (let i = 24; i >= 0; i--) { const u = i / 24 * 2 - 1, e = transom && u < 0 ? Math.abs(u) * 0.55 : Math.abs(u), hb = B / 2 * Math.pow(Math.max(0, 1 - Math.pow(e, 2.2)), 0.6) + (transom && u < 0 ? B * 0.18 * Math.abs(u) : 0); const x = bow === '+x' ? x0 + (u + 1) / 2 * L : x0 + L - (u + 1) / 2 * L; pts.push([x, yc - hb]); }
    const q = T.pts(pts); if (!dash) fill(c, q, rgba(col, a)); line(c, q, { closed: true, w: 1.6, dash, amp: 0.2 }); return q;
  };
  T.circle = (x, y, r, col = null, w = 1.4) => { const p = ellipsePts(...P(x, y), r * s, r * s, 28); if (col) fill(c, p, rgba(col)); line(c, p, { closed: true, w, amp: 0.15 }); };
  T.gate = (x, y, g) => { const [px, py] = P(x, y), r = Math.max(11, s * 0.95); c.save(); c.fillStyle = 'rgba(240,228,198,0.95)'; c.beginPath(); c.arc(px, py, r, 0, 7); c.fill(); c.restore(); line(c, ellipsePts(px, py, r, r, 24), { closed: true, w: 1.6, amp: 0.2, col: g === 'G5' ? rgba(C.glow, 1) : INK }); txt(c, g, px, py + r * 0.36, { size: r * 0.95, f: FSC, align: 'center' }); };
  T.voidX = (r, label = null) => { const p = T.rect(r); fill(c, p, PAPER); line(c, p, { closed: true, w: 1.2, dash: [6, 5] }); seg(c, p[0], p[2], { w: 0.8, col: ink(0.4) }); seg(c, p[1], p[3], { w: 0.8, col: ink(0.4) }); if (label) txt(c, label, P((r.x0 + r.x1) / 2, 0)[0], P(0, (r.y0 + r.y1) / 2)[1] - 8, { size: Math.max(13, s * 1.25), style: 'italic', align: 'center', halo: true }); };
  T.E = (x, y, label = null, dx = 1, dy = 0) => { const [px, py] = P(x, y), sz = Math.max(18, Math.min(30, s * 1.5)); keyScrap(c, px, py, 'E', sz); if (label) txt(c, label, px + dx * (sz * 0.8), py + sz * 0.25 + dy, { size: Math.max(13, Math.min(19, s * 1.05)), style: 'italic', halo: true, align: dx < 0 ? 'right' : 'left' }); };
  T.npc = (x, y, face) => { const [px, py] = P(x, y), r = Math.max(9, s * 0.55); line(c, ellipsePts(px, py, r, r, 24), { closed: true, w: 2, dash: [4, 3], col: rgba(C.route, 1), amp: 0.15 }); arrow(c, [px, py], [px + Math.cos(face) * r * 1.9, py + Math.sin(face) * r * 1.9], { w: 1.6, col: rgba(C.route, 1), head: 8 }); };
  T.route = (pts) => line(c, T.pts(pts), { w: Math.max(2.6, s * 0.26), col: rgba(C.route, 1), dash: [2, Math.max(6, s * 0.6)] });
  T.name = (n, x, y, size, locked = false) => { const [px, py] = P(x, y); if (locked) zoneName(c, n, px, py + size * 0.35, 'locked', size); else txt(c, n, px, py + size * 0.35, { size, f: FL, align: 'center', halo: true }); };
  return T;
}

/** Everything on one deck at plan scale. o: { labels, route, mini } */
function drawDeckPlan(c, lv, P, s, o = {}) {
  const L = LEVELS.find(l => l.id === lv), k = L.width || 1, T = planTools(c, P, s), Z = HUB.Z, big = s >= 14;
  let hull = HUB.outline(k, 100).map(([x, y]) => P(x, y));
  c.save(); pathOf(c, hull); fill(c, hull, PAPER); c.clip();
  for (const z of LOCKED.filter(z => z.levels[lv])) for (const r of z.levels[lv]) { const p = T.rect(r); zoneFill(c, p, 'locked', r.x0 * 3 + r.y0, 0.2); }
  for (const oz of OPEN.filter(q => q.level === lv)) for (const r of oz.rects) { const p = T.rect(r); fill(c, p, PAPER); zoneFill(c, p, 'open', r.x0 + 7, 0.55); }
  for (const r of L.water || []) { const p = T.rect(r); fill(c, p, PAPER); zoneFill(c, p, 'water', 4, 0.5); }
  c.restore();

  const sh = HUB.shaft, ring = HUB.ring;
  if (lv === 'quay') {
    // Dock: quays, arcade posts, bollards, ladders, berths, the slipway.
    for (const sg of [-1, 1]) {
      seg(c, P(0, sg * 14), P(44, sg * 14), { w: T.W });
      for (let x = 3; x < 44; x += 6) T.box(x - 0.3, x + 0.3, sg * 10.2 - 0.3, sg * 10.2 + 0.3, C.timberDark, 1, 1);
      for (let x = 6; x < 44; x += 6) T.circle(x, sg * 10.6, 0.3, C.iron, 1);
      for (const x of [1.5, 20]) { const p = T.rect({ x0: x, x1: x + 1, y0: sg * 10 - 0.25, y1: sg * 10 + 0.25 }); line(c, p, { closed: true, w: 1 }); }
    }
    seg(c, P(0, -10), P(44, -10), { w: 1.6, col: rgba(C.water, 1) }); seg(c, P(0, 10), P(44, 10), { w: 1.6, col: rgba(C.water, 1) });
    const rp = T.rect({ x0: HUB.ramp.x0 + 2, x1: 44, y0: -5, y1: 5 }); hatch(c, rp, { ang: 90, gap: Math.max(4, s * 0.5), col: ink(0.45) }); line(c, rp, { closed: true, w: 1.4 });
    T.boat(32, 10, 7.9, 3.4, { bow: '+x' });
    T.boat(8, 10, -7.9, 3.4, { bow: '+x', dash: [5, 4] }); T.boat(22, 14, -7.5, 4.4, { bow: '+x', transom: true, dash: [5, 4] });
    T.circle(40, 12.5, 0.45, C.timberDark);
    T.stairs(HUB.stair, '+x');
    // The Slip: hall and shop.
    T.walls({ x0: 44, x1: 64, y0: -9, y1: 19 }, [['x0', -9, 9], ['x0', 10.3, 13.6], ['x1', 5, 7.5], ['y0', 48, 60]]);
    for (let x = 48.4; x < 60; x += 0.8) seg(c, P(x, -9.25), P(x, -8.75), { w: 1.6 });
    T.boat(47, 14, 0, 4.4, { bow: '+x', transom: true }); for (const x of [50, 54, 58]) T.box(x - 0.3, x + 0.3, -3, 3, C.timberDark, 0.9, 1);
    T.box(46, 62, -5.7, -5.3, C.timber); // the mast on trestles
    T.box(52, 60, 9.6, 10.6, C.timberLight); T.box(46, 50, 16.5, 18.5, C.timber); T.box(48, 60, 18.4, 18.9, C.cream, 1, 1.2);
    // Keelgate chamber.
    T.walls(HUB.chamber, [['x0', 5, 7.5], ['x1', -7, -4], ['x1', 4, 7], ['y1', 79, 86]]);
    line(c, T.rect(sh), { closed: true, w: 1.2, dash: [6, 5] });
    if (big) txt(c, 'the Well above', P(84, 0)[0], P(0, -6.8)[1], { size: 15, style: 'italic', align: 'center', halo: true });
    const kg = HUB.keelgate; T.circle(kg.x, kg.y, kg.ring, C.ancient, 1.6); T.circle(kg.x, kg.y, kg.r, mix(C.ancient, C.glow, 0.25), 1.2);
    line(c, ellipsePts(...P(kg.x, kg.y), kg.r * 0.6 * s, kg.r * 0.6 * s, 30), { closed: true, w: 1.6, col: rgba(C.glow, 1) });
    for (let i = 0; i < 7; i++) { const t = i / 7 * Math.PI * 2 - Math.PI / 2; T.circle(kg.x + Math.cos(t) * (kg.r + 0.5), kg.y + Math.sin(t) * (kg.r + 0.5), 0.18, C.iron, 0.8); }
    const fl = HUB.flights[0]; T.stairs(fl, '-x');
    const ho = HUB.hoist; line(c, T.rect(ho), { closed: true, w: 1.6 }); seg(c, P(ho.x0, ho.y0), P(ho.x1, ho.y1), { w: 1 }); seg(c, P(ho.x1, ho.y0), P(ho.x0, ho.y1), { w: 1 });
    T.box(74, 77.5, -9, -7, C.timber); T.box(73, 92, -12, -11.4, C.timberDark, 0.9, 1);
    if (o.labels) {
      T.name('Stern Dock', 20, 0, Math.max(17, s * 1.7)); T.name('The Slip', 54, 13.5, s * 1.5); T.name('Keelgate', 89, -8.6, s * 1.4);
      T.name('Quay stores', 22, 17, s * 1.1, true); T.name('Quay stores', 22, -17, s * 1.1, true); T.name('Timber store', 54, -14.5, s * 1.1, true);
      T.name('Keel Walk', 68, -14, s * 0.95, true); T.name('The Holds', 128, 0, s * 1.6, true); T.name('Holds', 84, 16, s * 1.1, true); T.name('Holds', 84, -16, s * 1.1, true);
      txt(c, 'fitting berth', P(37, 0)[0], P(0, 6.1)[1], { size: s * 1.05, style: 'italic', align: 'center', halo: true });
      txt(c, 'fleet berths', P(22, 0)[0], P(0, -4.6)[1], { size: s * 1.05, style: 'italic', align: 'center', halo: true });
      txt(c, 'slipway', P(39, 0)[0], P(0, 0.6)[1], { size: s * 1.05, style: 'italic', align: 'center', halo: true });
    }
  }
  if (lv === 'lower' || lv === 'middle' || lv === 'upper') {
    for (const v of L.voids || []) if (v !== HUB.shaft) T.voidX(v, v === HUB.basin ? 'open: the dock below' : 'open: the Slip below');
    T.voidX(sh, null);
    T.walls(ring, ({ lower: [['x0', 6, 11], ['x0', -4, -1], ['y0', 82, 86], ['x1', 2, 5], ['y1', 88, 91]], middle: [['x0', -2, 2], ['x1', 2, 5], ['y0', 87, 89], ['y1', 87, 89]], upper: [['x0', -5.5, -2.5], ['x1', -2, 2], ['y0', 87, 90], ['y1', 82.5, 85.5]] })[lv]);
    line(c, T.rect(sh), { closed: true, w: 1.8 });
    const ho = HUB.hoist; line(c, T.rect(ho), { closed: true, w: 1.2, dash: [4, 3] });
    for (const f of HUB.flights) { if (f.from === lv) T.stairs(f, f.up, 'up'); if (f.to === lv) { const p = T.rect(f); line(c, p, { closed: true, w: 1, dash: [4, 4] }); } }
    if (lv === 'lower') {
      T.walls(HUB.tally, [['x0', 14, 17], ['x1', 9, 12], ['x1', 18.6, 19.8]]);
      for (let x = 46; x < 62; x += 4) { seg(c, P(x, 8.8), P(x + 3, 8.8), { w: 1 }); seg(c, P(x, 9.2), P(x + 3, 9.2), { w: 1 }); }
      T.walls(HUB.lane, [['x0', 6, 12], ['x1', 6, 12]]);
      fill(c, T.rect({ x0: 42, x1: 44, y0: 14, y1: 17 }), rgba(C.open, 0.7));
      T.box(63.4, 64, 13, 18.5, C.cream, 1, 1.2); T.box(50, 56, 17.5, 20, C.timber); T.box(57, 62, 18.5, 20, C.timberLight); T.box(49, 58, 12.6, 13.8, C.timberLight);
      if (o.labels) { T.name('Tally House', 54, 11.6, s * 1.4); T.name('Wing quarters', 22, 15, s * 1.1, true); T.name('Wing quarters', 22, -15, s * 1.1, true); T.name('The Ropewalk', 112, -15, s * 1.3, true); T.name('Mess and galley', 128, 4, s * 1.4, true); T.name('Purser’s', 68.5, -5, s * 0.9, true); T.name('stores', 68.5, -3.4, s * 0.9, true); T.name('lane', 68.5, 9, s * 0.95); }
    }
    if (o.labels && lv === 'middle') { T.name('Guild rooms', 30, 15, s * 1.3, true); T.name('Guild rooms', 60, -15, s * 1.3, true); T.name('Lantern Market', 128, 0, s * 1.5, true); }
    if (o.labels && lv === 'upper') { T.name('Wing quarters', 22, 15, s * 1.1, true); T.name('Archive', 58, -15, s * 1.3, true); T.name('Inn and bunkrooms', 128, 0, s * 1.5, true); }
    if (o.labels) T.name('The Well', 84, -8.4, s * 1.2);
  }
  if (lv === 'main') {
    const inner = HUB.outline(0.965, 100).filter(([x]) => x >= 44.5).map(([x, y]) => P(x, y));
    line(c, inner.slice(0, inner.length / 2), { w: 1 }); line(c, inner.slice(inner.length / 2), { w: 1 });
    T.voidX(HUB.basin, 'open: the dock below');
    seg(c, P(44, -10), P(44, 10), { w: 1.6 }); for (let y = -9.5; y <= 9.5; y += 1) T.circle(44.3, y, 0.12, INK, 0.8);
    T.walls({ x0: 0, x1: 44, y0: 10, y1: 19.6 }, [['y0', 0, 44], ['x0', 10, 20]]); T.walls({ x0: 0, x1: 44, y0: -19.6, y1: -10 }, [['y1', 0, 44], ['x0', -20, -10]]);
    // The Well opening, rail, gantry, the stair hatch, the hoist gate.
    T.voidX(sh, null); line(c, T.rect({ x0: sh.x0 - 0.3, x1: sh.x1 + 0.3, y0: sh.y0 - 0.3, y1: sh.y1 + 0.3 }), { closed: true, w: 2.2 });
    line(c, T.pts([[76.5, 0], [91.5, 0]]), { w: 3, dash: [10, 5], col: rgba(C.timberDark) });
    T.stairs(HUB.flights[3], '-y', 'down'); const ho = HUB.hoist; line(c, T.rect(ho), { closed: true, w: 1.4 });
    // Masts and fife rails.
    for (const m of HUB.masts) { T.circle(m.x, 0, 1.0, C.timberDark); line(c, T.rect({ x0: m.x - 2, x1: m.x + 2, y0: -2, y1: 2 }), { closed: true, w: 1.2 }); for (const sg of [-1, 1]) for (let d = -4; d <= 4; d += 2) { const e = HUB.hb(m.x + d) - 0.4; T.box(m.x + d - 0.5, m.x + d + 0.5, sg * e - 0.4, sg * e + 0.4, C.iron, 1, 0.8); } }
    // The Chandlery.
    const ch = HUB.chandlery; T.walls(ch, [['y1', 100, 110], ['y1', 111, 113]]); fill(c, T.rect({ x0: 100, x1: 110, y0: -8.4, y1: -7.6 }), rgba(C.timberLight)); T.box(98.4, 113.6, -18.8, -18.2, C.timber); T.box(98.2, 98.8, -18, -9, C.timber); T.box(103, 108, -14.5, -12.5, C.timberLight);
    line(c, T.rect({ x0: 99.5, x1: 112.5, y0: -8, y1: -5.6 }), { closed: true, w: 1, dash: [3, 4] });
    T.circle(106, -4.5, 0.4, C.brass); T.circle(110, -4.5, 0.45, C.timber);
    // Cargo deck: the rope, hatches, boats, cargo, capstan, chimney.
    line(c, T.pts([[116, -19], [116, 19]]), { w: 2, dash: [2, 3], col: rgba([150, 110, 60], 1) });
    for (const [x0, x1] of [[118, 122], [123, 127]]) { const p = T.rect({ x0, x1, y0: -3, y1: 3 }); hatch(c, p, { ang: 0, gap: Math.max(3, s * 0.4) }); hatch(c, p, { ang: 90, gap: Math.max(3, s * 0.4) }); line(c, p, { closed: true, w: 1.4 }); }
    T.boat(119, 10, 13.5, 3, { col: C.timberLight }); T.boat(119, 10, -13.5, 3, { col: C.timberLight });
    for (const [x, y] of [[129, 7], [130, -7], [118, 7], [128, -3.8]]) T.box(x, x + 2.4, y, y + 2.4, C.timberLight, 0.8, 1);
    T.circle(131.5, 0, 1.1, C.timber); T.box(120.3, 121.3, 7.5, 8.5, C.iron, 1, 1);
    line(c, T.pts([[160, 0], [HUB.sprit, 0]]), { w: Math.max(3, s * 0.7), col: rgba(C.timberDark) });
    seg(c, P(134, -HUB.hb(134)), P(134, HUB.hb(134)), { w: T.W });
    if (o.labels) {
      T.name('Main deck, the Waist', 60, 9, s * 1.7); T.name('Chandlery', 106, -16, s * 1.25); T.name('Cargo deck', 125, 9, s * 1.15, true); T.name('Forecastle', 148, 0, s * 1.4, true);
      T.name('Sterncastle', 22, 15, s * 1.2, true); T.name('Sterncastle', 22, -15, s * 1.2, true); txt(c, 'the Well', P(84, 0)[0], P(0, -6.8)[1], { size: s * 1.2, f: FL, align: 'center', halo: true });
      txt(c, 'dock head rail', P(45.5, 0)[0], P(0, -10.8)[1], { size: s * 1.0, style: 'italic', halo: true });
    }
  }
  line(c, hull, { closed: true, w: Math.max(2.4, s * 0.25), seed: 3 });
  for (const gm of GATE_MARKS.filter(q => q.level === lv)) T.gate(gm.x, gm.y, gm.g);
  // Services: badges, NPC spots, interaction points.
  const svc = {
    quay: () => { T.npc(56, 11.6, -Math.PI / 2); T.E(56, 8.4, o.labels ? 'counter' : null); T.E(54, 17.4, o.labels ? 'plans' : null); T.E(57.5, -3.2, o.labels ? 'the boat' : null); T.npc(76.5, -5.8, -0.6); T.E(78.6, -3.6, o.labels ? 'talk' : null); T.E(81, 1.7, o.labels ? 'the gate' : null, -1); badge(c, ...P(50, 13.6), 'S', Math.max(12, s * 1.1)); badge(c, ...P(80, -10), 'Q', Math.max(12, s * 1.1)); },
    lower: () => { T.npc(53, 18.8, -Math.PI / 2); T.E(53, 16.6, o.labels ? 'pay hatch' : null); T.E(62.4, 15.8, o.labels ? 'job board' : null, -1); T.E(59.5, 17.4, o.labels ? 'deliver' : null); badge(c, ...P(47, 11), 'J', Math.max(12, s * 1.1)); },
    main: () => { T.npc(105, -9.4, Math.PI / 2); T.E(105, -6.8, o.labels ? 'counter' : null); T.E(106, -3.4, o.labels ? 'spyglass' : null, -1); T.E(110.4, -3.4, o.labels ? 'compass' : null); badge(c, ...P(100.4, -16), 'E', Math.max(12, s * 1.1)); },
  };
  if (svc[lv]) svc[lv]();
  if (o.route) {
    const R = {
      quay: [[[-6, 0], [34, 7.9]], [[37, 12], [46, 12], [56, 9.2]], [[42, 12.6], [33, 15.5], [44, 15.5]], [[86, 9.5], [88, 7], [78, -4.6]]],
      lower: [[[44, 15.5], [58, 15.5], [64, 10.5], [73, 9], [79, 9.5]], [[79, 9.5], [93.5, 6], [93.5, -1]]],
      middle: [[[93.5, -7], [90, -9.5], [86, -9.5]]], upper: [[[79, -9.5], [74.5, -8], [74.5, 0]]], main: [[[74.5, 7], [94, 7.5], [102, -3], [105, -6.4]]],
    };
    for (const r of R[lv] || []) T.route(r);
  }
}

/* ======================================================================================================== 6 */
defSheet('plans', 'Deck plans', 'Bow to the right, starboard down. The open 20 % in warm wash, planned and locked space in grey. Gate codes: see the register.', 2400, 3060, (c) => {
  const s = 10.5, ox = 150, P0 = rowY => (x, y) => [ox + x * s, rowY + y * s];
  const rows = [['main', 480], ['upper', 1010], ['middle', 1540], ['lower', 2070], ['quay', 2600]];
  const side = {
    main: ['Open from the dock head rail (x 44) to the cargo rope (x 116), 72 m.', 'The Well’s opening is railed; the gantry stands over it; the stair comes up beside it (aft).', 'The Chandlery is a deckhouse on the port side, its counter facing the open deck and the main mast.', 'Locked: the sterncastle doors, the cargo deck behind its rope, the forecastle, the masts.'],
    upper: ['Only the Well’s gallery is open (5 m wide round the 12 m shaft).', 'Four closed doors: the Archive aft, the Inn forward and to both sides.', 'The stair continues up from the aft side of the gallery to the main deck.'],
    middle: ['Only the gallery. A rope with a board across the forward door: “Market closed”; shutters to the sides.', 'Painted guild crests on the aft door.'],
    lower: ['You come up the dock stair into the Tally House: jobs on the board ahead, the pay hatch to the left, windows down into the Slip to the right.', 'The lane leads forward to the Well. Doors to the Ropewalk, the mess and the purser’s stores are closed.'],
    quay: ['Sail in through the stern, moor at the fitting berth by the crane (starboard) or a fleet berth (port).', 'The Slip: the next boat on the slipway, the shop and counter beside it.', 'The Keelgate chamber at the foot of the Well: reached down the Well’s stair or by the hoist.', 'Swimmers climb out on rung ladders at the quays.'],
  };
  for (const [lv, y] of rows) {
    const L = LEVELS.find(l => l.id === lv), a = AREAS.levels[lv];
    txt(c, L.name, ox, y - 250, { size: 34, f: FL });
    txt(c, `${L.z > 0 ? '+' : '−'}${Math.abs(L.z)} m above the waterline · ${fmt(a.open)} of ${fmt(a.total)} m² open`, ox + measure(c, L.name, 34, FL) + 24, y - 250, { size: 21, style: 'italic' });
    drawDeckPlan(c, lv, P0(y), s, { labels: true, route: true });
    notes(c, [[null, ...side[lv]]], 1990, lv === 'main' ? y + 40 : y - 170, 350, { size: 18, gap: 0 });
  }
  // Dimensions on the quay plan.
  const Pq = P0(2600), dim = (a, b, label, off = 0) => { arrow(c, a, b, { w: 1, head: 8 }); arrow(c, b, a, { w: 1, head: 8 }); txt(c, label, (a[0] + b[0]) / 2, (a[1] + b[1]) / 2 - 6 + off, { size: 16, align: 'center', style: 'italic', halo: true }); };
  dim(Pq(0, 24), Pq(44, 24), 'basin 44 m'); dim(Pq(44, 24), Pq(64, 24), 'Slip 20 m'); dim(Pq(72, 24), Pq(96, 24), 'chamber 24 m');
  dim(Pq(-4, -10), Pq(-4, 10), '20 m', 0); dim(Pq(0, 24), Pq(160, 24), '', 0);
  txt(c, '160 m on deck, 188 m with the bowsprit · 40 m beam', Pq(110, 24)[0], Pq(110, 24)[1] - 8, { size: 17, style: 'italic', align: 'center', halo: true });
  legend(c, 150, 2900, ['open', 'locked', 'water', 'void', 'route', 'npc', 'e'], { cols: 4, colW: 430, size: 18 });
  txt(c, 'G1 barred door · G2 rope and board · G3 chained ladder or hatch · G4 shutters · G5 ancient seal · G6 iron grille', 150, 3000, { size: 19, style: 'italic' });
  scaleBar(c, 1990, 2860, s, 20, 5);
});

/* ======================================================================================================== 7 */
defSheet('zones', 'Planned, locked, not designed', 'The 80 %: space reserved for later, with working names only. Each zone has a gate that says why it is closed, and nothing behind it is built yet.', 2400, 1880, (c) => {
  // The register.
  const X = [70, 128, 420, 760, 860, 1060], W = [52, 280, 330, 90, 190, 520], top = 210;
  const head = ['', 'Zone (working name)', 'Where', 'm²', 'Gate', 'What the player sees from outside'];
  head.forEach((h, i) => txt(c, h, X[i] + (i === 3 ? W[3] - 10 : 0), top, { size: 21, f: FSC, align: i === 3 ? 'right' : 'left' }));
  seg(c, [X[0], top + 14], [1590, top + 14], { w: 2 });
  let y = top + 46;
  for (const z of LOCKED) {
    const a = AREAS.locked[z.id] || 0, g = GATES[z.gate];
    const cells = [[z.id, 18, FSC], [z.name, 20, FL], [WHERE[z.id], 17, FT], [a ? fmt(a) : '–', 18, FT], [`${z.gate} ${g.name}`, 17, FT], [z.sees, 17, FT]];
    let h = 1;
    const lines = cells.map(([t, sz, f], i) => { const l = wrap(c, t, W[i] - 12, sz, f); h = Math.max(h, l.length); return l; });
    lines.forEach((l, i) => l.forEach((t, k) => txt(c, t, X[i] + (i === 3 ? W[3] - 10 : 0), y + k * 22, { size: cells[i][1], f: cells[i][2], align: i === 3 ? 'right' : 'left', style: i === 5 || i === 2 ? 'italic' : '' })));
    y += h * 22 + 14; seg(c, [X[0], y - 18], [1590, y - 18], { w: 0.8, col: ink(0.3) });
  }
  const lockedSum = AREAS.total - AREAS.openTotal;
  txt(c, `Planned and locked: ${fmt(lockedSum)} m² (${pct(lockedSum, AREAS.total)}); open: ${fmt(AREAS.openTotal)} m² (${pct(AREAS.openTotal, AREAS.total)}).`, X[0], y + 14, { size: 20, style: 'italic' });
  notes(c, [['How a locked zone is built now',
    'Only its shell: the outside of the ship, and the gate. No room behind it, no props, no collision beyond the gate.',
    'Every opening into it is closed: doors barred, windows shuttered, grilles with a shallow set 2 to 3 m deep behind them. Nothing that would need an interior is visible.',
    'Each gate answers E with one short line of why it is closed (for example “Barred from inside.”). No promise of what is behind it.',
    'Each zone has an id (L1 to L17) in the save: unlocked or not. Opening one later means designing it then.',
    'Working names are placeholders; the owner names and designs each zone when it is opened.']], X[0], y + 70, 1500, { size: 20, head: 25 });
  // Gate types.
  txt(c, 'The six gate types', 1660, top, { size: 30, f: FL });
  const gx = [1660, 2010], gy = [240, 600, 960];
  const draws = {
    G1: iso => { wallX(c, iso, 0, 0, 3.2, 0, 2.8); opening(c, iso, 'x', 0, 0.9, 2.3, 0, 2.3, C.timberDark); for (let u = 1.05; u < 2.25; u += 0.28) seg(c, iso.P(0, u, 0), iso.P(0, u, 2.3), { w: 0.8, col: ink(0.5) }); seg(c, iso.P(0.02, 0.7, 1.25), iso.P(0.02, 2.5, 1.25), { w: 5, col: rgba(C.iron) }); quad(c, iso, [[0.05, 1.5, 1.05], [0.05, 1.75, 1.05], [0.05, 1.75, 1.3], [0.05, 1.5, 1.3]], C.brass); },
    G2: iso => { quad(c, iso, [[0, 0, 0], [3.2, 0, 0], [3.2, 3.2, 0], [0, 3.2, 0]], C.timberLight, { w: 1 }); floorPlanks(c, iso, 0, 3.2, 0, 3.2, 0, 0.32); cyl(c, iso, 0.3, 2.9, 0.1, 0, 1.2, C.timber); cyl(c, iso, 2.9, 0.3, 0.1, 0, 1.2, C.timber); const rope = []; for (let i = 0; i <= 12; i++) { const t = i / 12; rope.push(iso.P(0.3 + 2.6 * t, 2.9 - 2.6 * t, 1.1 - Math.sin(t * Math.PI) * 0.25)); } line(c, rope, { w: 3, col: rgba([150, 110, 60]) }); quad(c, iso, [[1.25, 1.95, 0.5], [1.95, 1.25, 0.5], [1.95, 1.25, 0.9], [1.25, 1.95, 0.9]], C.cream); for (let k = 0; k < 2; k++) seg(c, iso.P(1.35, 1.85, 0.78 - k * 0.14), iso.P(1.85, 1.35, 0.78 - k * 0.14), { w: 1, col: ink(0.6) }); },
    G3: iso => { wallX(c, iso, 0, 0, 3, 0, 3); for (const u of [1, 1.8]) box(c, iso, { x0: 0, x1: 0.1, y0: u, y1: u + 0.08, z0: 0, z1: 2.9, col: C.timber, w: 1 }); for (let z = 0.3; z < 2.9; z += 0.3) seg(c, iso.P(0.06, 1.05, z), iso.P(0.06, 1.85, z), { w: 2, col: rgba(C.timberDark) }); const ch = []; for (let i = 0; i <= 10; i++) { const t = i / 10; ch.push(iso.P(0.15, 0.9 + 1.1 * t, 1.2 + Math.sin(t * Math.PI * 3) * 0.08)); } line(c, ch, { w: 3, col: rgba(C.iron), dash: [4, 2] }); quad(c, iso, [[0.18, 1.35, 1.0], [0.18, 1.55, 1.0], [0.18, 1.55, 1.2], [0.18, 1.35, 1.2]], C.brass); },
    G4: iso => { wallY(c, iso, 0, 0, 3.4, 0, 2.6); for (const u of [0.4, 1.9]) { opening(c, iso, 'y', 0, u, u + 1.1, 0.8, 2.1, C.timber); for (let k = 1; k < 4; k++) seg(c, iso.P(u + k * 0.275, 0, 0.8), iso.P(u + k * 0.275, 0, 2.1), { w: 0.8, col: ink(0.6) }); seg(c, iso.P(u, 0, 0.9), iso.P(u + 1.1, 0, 2.0), { w: 1.6, col: ink(0.7) }); } },
    G5: iso => { quad(c, iso, [[0, 0, 0], [3.4, 0, 0], [3.4, 3.4, 0], [0, 3.4, 0]], C.timberLight, { w: 1 }); floorPlanks(c, iso, 0, 3.4, 0, 3.4, 0, 0.3); disc(c, iso, 1.7, 1.7, 0.02, 1.4, C.ancient); disc(c, iso, 1.7, 1.7, 0.03, 1.05, mix(C.ancient, C.glow, 0.15)); const p = []; for (let i = 0; i <= 40; i++) { const t = i / 40 * Math.PI * 2; p.push(iso.P(1.7 + Math.cos(t) * 0.6, 1.7 + Math.sin(t) * 0.6, 0.04)); } line(c, p, { w: 2.4, col: rgba(C.glow, 0.95) }); for (let k = 0; k < 7; k++) { const t = k / 7 * Math.PI * 2; seg(c, iso.P(1.7 + Math.cos(t) * 0.6, 1.7 + Math.sin(t) * 0.6, 0.04), iso.P(1.7 + Math.cos(t) * 1.0, 1.7 + Math.sin(t) * 1.0, 0.04), { w: 1.8, col: rgba(C.glow, 0.8) }); } },
    G6: iso => { quad(c, iso, [[0, 0, 0], [2.2, 0, 0], [2.2, 3.2, 0], [0, 3.2, 0]], [120, 104, 84], { w: 1 }); box(c, iso, { x0: 0.2, x1: 1.0, y0: 0.4, y1: 1.2, z0: 0, z1: 0.8, col: C.timberLight }); box(c, iso, { x0: 0.2, x1: 1.0, y0: 1.4, y1: 2.2, z0: 0, z1: 0.8, col: C.timberLight }); box(c, iso, { x0: 0.3, x1: 0.9, y0: 0.5, y1: 1.1, z0: 0.8, z1: 1.4, col: C.timberLight }); for (let u = 0.15; u < 3.2; u += 0.26) seg(c, iso.P(2.2, u, 0), iso.P(2.2, u, 2.4), { w: 2.4, col: rgba(C.iron) }); seg(c, iso.P(2.2, 0, 2.4), iso.P(2.2, 3.2, 2.4), { w: 3, col: rgba(C.iron) }); seg(c, iso.P(2.2, 0, 1.2), iso.P(2.2, 3.2, 1.2), { w: 2, col: rgba(C.iron) }); },
  };
  const main = c;
  Object.keys(GATES).forEach((g, i) => {
    const x = gx[i % 2], y = gy[Math.floor(i / 2)], off = canvas(700, 700);
    c = off.getContext('2d'); draws[g](Iso(350, 420, 52)); c = main;
    const d = off.getContext('2d').getImageData(0, 0, 700, 700).data; let x0 = 700, y0 = 700, x1 = 0, y1 = 0;
    for (let q = 0; q < 700; q++) for (let p = 0; p < 700; p++) if (d[(q * 700 + p) * 4 + 3] > 8) { x0 = Math.min(x0, p); x1 = Math.max(x1, p); y0 = Math.min(y0, q); y1 = Math.max(y1, q); }
    c.drawImage(off, x0, y0, x1 - x0 + 1, y1 - y0 + 1, x + (320 - (x1 - x0)) / 2, y + 20 + (210 - (y1 - y0)) / 2, x1 - x0 + 1, y1 - y0 + 1);
    txt(c, `${g} · ${GATES[g].name}`, x, y + 268, { size: 22, f: FL });
    para(c, GATES[g].why, x, y + 294, 320, { size: 17, style: 'italic' });
  });
  // Open and locked per deck.
  const bx = 1660, by0 = 1330, bw = 640; txt(c, 'Open and locked per floor', bx, by0 - 40, { size: 26, f: FL });
  const order = ['sc3', 'sc2', 'sc1', 'fc1', 'main', 'upper', 'middle', 'lower', 'quay', 'deep'], maxT = Math.max(...order.map(k => AREAS.levels[k].total));
  order.forEach((k, i) => {
    const a = AREAS.levels[k], yy = by0 + i * 40, L = LEVELS.find(l => l.id === k), w = bw * 0.62 * a.total / maxT, wo = bw * 0.62 * a.open / maxT;
    txt(c, L.name.replace('Sterncastle, ', 'Sterncastle ').replace(' storey', ''), bx, yy + 20, { size: 16 });
    const x0 = bx + 230; const pl = rectPts(x0, yy + 2, w, 24); wash(c, pl, C.locked, 0.3); hatch(c, pl, { gap: 6 }); if (wo > 0) wash(c, rectPts(x0, yy + 2, wo, 24), C.open, 0.7); line(c, pl, { closed: true, w: 1 });
    txt(c, a.open ? `${fmt(a.open)} / ${fmt(a.total)}` : fmt(a.total), x0 + w + 8, yy + 21, { size: 15, style: 'italic' });
  });
});

/* ---------- the space sheets ---------- */
function spaceSheet(c, o) {
  // o: { badge, title, plan: { lv, x0, x1, y0, y1, s }, notes }
  const px = 1600, py = 200, pw = 740;
  badge(c, px + 20, py, o.badge, 20); txt(c, o.title, px + 54, py + 10, { size: 32, f: FL });
  const p = o.plan, s = p.s, ph = (p.y1 - p.y0) * s, pwid = (p.x1 - p.x0) * s, ox = px + (pw - pwid) / 2, oy = py + 50;
  const P = (x, y) => [ox + (x - p.x0) * s, oy + (y - p.y0) * s];
  c.save(); pathOf(c, rectPts(ox, oy, pwid, ph)); c.clip(); drawDeckPlan(c, p.lv, P, s, { labels: false, route: false }); if (p.extra) p.extra(c, P, s); c.restore();
  line(c, rectPts(ox, oy, pwid, ph), { closed: true, w: 1.2, col: ink(0.6) });
  txt(c, 'plan (bow to the right, starboard down)', ox, oy + ph + 24, { size: 16, style: 'italic' });
  scaleBar(c, ox + pwid - 5 * s * 2 - 70, oy + ph + 12, s, 10, 5, { label: null });
  notes(c, o.notes, px, oy + ph + 80, pw, { size: 19, head: 23, gap: 10 });
}
const dimIso = (c, iso, a, b, label, off = [0, -10]) => { const p = iso.P(...a), q = iso.P(...b); arrow(c, p, q, { w: 1, head: 8 }); arrow(c, q, p, { w: 1, head: 8 }); txt(c, label, (p[0] + q[0]) / 2 + off[0], (p[1] + q[1]) / 2 + off[1], { size: 18, style: 'italic', align: 'center', halo: true }); };
function shelfUnit(c, iso, x0, x1, y0, y1, z1, plane, seed) {
  box(c, iso, { x0, x1, y0, y1, z0: 0, z1, col: C.timberDark, w: 1.2 });
  const r = rng(seed), along = plane === 'y' ? [x0, x1] : [y0, y1];
  for (let z = 0.5; z < z1 - 0.2; z += 0.75) {
    if (plane === 'y') seg(c, iso.P(x0, y1, z), iso.P(x1, y1, z), { w: 1.4, col: rgba(C.timberLight) }); else seg(c, iso.P(x1, y0, z), iso.P(x1, y1, z), { w: 1.4, col: rgba(C.timberLight) });
    for (let u = along[0] + 0.15; u < along[1] - 0.3; u += 0.25 + r() * 0.35) {
      const w = 0.12 + r() * 0.22, h = 0.15 + r() * 0.35, col = [C.cream, C.brass, C.timberLight, [150, 160, 150], C.oxblood, [190, 180, 160]][Math.floor(r() * 6)];
      if (plane === 'y') box(c, iso, { x0: u, x1: u + w, y0: y1 - 0.25, y1: y1 + 0.02, z0: z, z1: z + h, col, w: 0.7 }); else box(c, iso, { x0: x1 - 0.25, x1: x1 + 0.02, y0: u, y1: u + w, z0: z, z1: z + h, col, w: 0.7 });
    }
  }
}
function barrel(c, iso, x, y, r = 0.32, h = 0.9) { cyl(c, iso, x, y, r, 0, h, C.timber, { hoops: [h * 0.18, h * 0.82] }); }
function crate(c, iso, x, y, z = 0, d = 0.6) { box(c, iso, { x0: x, x1: x + d, y0: y, y1: y + d, z0: z, z1: z + d * 0.8, col: C.timberLight, w: 1.1, detail: (cc, ii, b) => seg(cc, ii.P(b.x1, b.y0, b.z0), ii.P(b.x1, b.y1, b.z1), { w: 0.8, col: ink(0.4) }) }); }

/* ======================================================================================================== 8 */
defSheet('slip', 'The Slip: the shipwright’s space', 'Quay deck, at the head of the Stern Dock. Seen from the dock, aft and to port; the near walls and the decks above are cut away.', 2400, 1600, (c) => {
  // Local: X = 64 - x (forward bulkhead at X 0), Y = 19 - y (shop's hull wall at Y 0); Z = 0 at the quay deck (+1.5).
  const iso = Iso(735, 560, 26), P = iso.P;
  // Water and the basin floor first (they are lowest).
  quad(c, iso, [[18, 9, -1.5], [34, 9, -1.5], [34, 29, -1.5], [18, 29, -1.5]], C.water, { a: 0.75, w: 1.2 });
  for (let i = 0; i < 26; i++) { const r = rng(i + 3); const x = 20 + r() * 13, y = 10 + r() * 18; seg(c, P(x, y, -1.5), P(x + 0.8, y, -1.5), { w: 1.2, col: 'rgba(255,255,255,0.45)' }); }
  // The slipway rising out of the water into the hall.
  quad(c, iso, [[18, 14, 0], [32, 14, -4.5], [32, 24, -4.5], [18, 24, 0]], C.timberLight, { a: 0.8, w: 1.4 });
  for (let x = 19; x < 32; x += 1.2) seg(c, P(x, 14, -(x - 18) * 4.5 / 14), P(x, 24, -(x - 18) * 4.5 / 14), { w: 0.9, col: ink(0.4) });
  quad(c, iso, [[18, 9, -1.5], [34, 9, -1.5], [34, 29, -1.5], [18, 29, -1.5]], C.water, { a: 0.35, noInk: true });
  // Floors: the hall and shop, the quay.
  quad(c, iso, [[0, 0, 0], [20, 0, 0], [20, 28, 0], [0, 28, 0]], C.timberLight, { w: 1.6 }); floorPlanks(c, iso, 0, 20, 0, 28, 0, 0.7);
  quad(c, iso, [[20, 5, 0], [34, 5, 0], [34, 9, 0], [20, 9, 0]], mul(C.timberLight, 0.92), { w: 1.4 }); floorPlanks(c, iso, 20, 34, 5, 9, 0, 0.5, 'y');
  // Back walls: the forward bulkhead (12 m in the hall, 4 m at the shop), the shop's hull wall, the wing wall behind the quay.
  wallX(c, iso, 0, 10, 28, 0, 12); wallX(c, iso, 0, 0, 10, 0, 4); wallY(c, iso, 0, 0, 20, 0, 4); wallY(c, iso, 2, 20, 34, 0, 4, C.timberDark);
  // On the bulkhead: the timber rack, the Keel Walk door (G1), half-models over the shop.
  for (const z of [0.6, 1.5, 2.4, 3.3]) { box(c, iso, { x0: 0, x1: 0.6, y0: 15, y1: 27, z0: z, z1: z + 0.22, col: C.timberLight, w: 1 }); }
  for (const y of [15.5, 21, 26.5]) box(c, iso, { x0: 0, x1: 0.7, y0: y - 0.15, y1: y + 0.15, z0: 0, z1: 3.6, col: C.iron, w: 1 });
  opening(c, iso, 'x', 0.02, 11.5, 14, 0, 2.6, C.timberDark); seg(c, P(0.05, 11.2, 1.4), P(0.05, 14.3, 1.4), { w: 5, col: rgba(C.iron) }); quad(c, iso, [[0.08, 12.6, 1.15], [0.08, 12.9, 1.15], [0.08, 12.9, 1.45], [0.08, 12.6, 1.45]], C.brass);
  for (const [i, y] of [3, 5.5, 8].entries()) { const hm = []; for (let k = 0; k <= 16; k++) { const t = k / 16; hm.push(P(0.02, y - 1 + 2 * t, 3.0 - Math.sin(t * Math.PI) * 0.45)); } hm.push(P(0.02, y + 1, 3.05), P(0.02, y - 1, 3.05)); fill(c, hm, rgba([C.oxblood, [70, 90, 70], C.timber][i])); line(c, hm, { closed: true, w: 1.2 }); }
  // On the hull wall: windows and the plans board (the upgrades).
  for (const x of [1.5, 17]) { opening(c, iso, 'y', 0.02, x, x + 1.4, 1.6, 3.2, [250, 236, 200]); }
  quad(c, iso, [[4, 0.04, 1.1], [15.5, 0.04, 1.1], [15.5, 0.04, 3.5], [4, 0.04, 3.5]], C.timber, { w: 1.4 });
  { const r = rng(5); for (let x = 4.3; x < 15.2; x += 1.25) for (const z of [1.3, 2.35]) { const w = 0.9 + r() * 0.2, h = 0.8 + r() * 0.15; quad(c, iso, [[x, 0.06, z], [x + w, 0.06, z], [x + w, 0.06, z + h], [x, 0.06, z + h]], [238, 226, 196], { w: 0.8 }); line(c, [P(x + 0.15, 0.07, z + 0.3), P(x + w * 0.5, 0.07, z + 0.5), P(x + w - 0.15, 0.07, z + 0.3)], { w: 0.9, col: ink(0.55) }); } }
  // The wing wall behind the quay: shuttered bays (G4), the dock stair going up through it.
  for (const x of [28.5, 31.2]) { opening(c, iso, 'y', 2.02, x, x + 1.6, 0.4, 2.8, C.timber); seg(c, P(x + 0.8, 2.03, 0.4), P(x + 0.8, 2.03, 2.8), { w: 1, col: ink(0.6) }); }
  for (let i = 0; i < 12; i++) { const x = 34 - i * (14 / 12) - 14 / 12, z = (i + 1) * 4 / 13; box(c, iso, { x0: x, x1: x + 14 / 12, y0: 2, y1: 5, z0: 0, z1: z, col: C.timberLight, w: 0.9, seed: i }); }
  // The display boat on the slip (the next boat for sale), cradles, the mast on trestles, a ladder.
  for (const x of [5, 10, 15]) box(c, iso, { x0: x - 0.3, x1: x + 0.3, y0: 16.4, y1: 21.6, z0: 0, z1: 1.3, col: C.timberDark, w: 1.2 });
  isoHull(c, iso, 3, 19, 14, 4.4, 0.6, 4.0, { doubleEnder: false, flip: true, rise: 0.45 });
  for (const x of [4, 16]) box(c, iso, { x0: x - 0.4, x1: x + 0.4, y0: 23.6, y1: 25.4, z0: 0, z1: 0.9, col: C.timber, w: 1 });
  box(c, iso, { x0: 2, x1: 18, y0: 24.3, y1: 24.7, z0: 0.9, z1: 1.3, col: [150, 104, 60], w: 1.2 });
  line(c, [P(12, 22.6, 0), P(12, 21.4, 3.9)], { w: 2.2, col: rgba(C.timber) }); line(c, [P(13, 22.6, 0), P(13, 21.4, 3.9)], { w: 2.2, col: rgba(C.timber) }); for (let k = 1; k < 7; k++) seg(c, P(12, 22.6 - k * 0.17, k * 0.56), P(13, 22.6 - k * 0.17, k * 0.56), { w: 1.6, col: rgba(C.timber) });
  // Shop: workbench, counter, the NPC's place.
  box(c, iso, { x0: 14, x1: 18, y0: 1, y1: 2.6, z0: 0, z1: 0.9, col: C.timber });
  crate(c, iso, 18.4, 3.2); barrel(c, iso, 19.2, 6.5);
  const npc = P(8, 7.4, 0); npcSpot(c, npc[0], npc[1], 22, Math.PI / 2 - 0.4);
  box(c, iso, { x0: 4, x1: 12, y0: 8.4, y1: 9.4, z0: 0, z1: 1.1, col: C.timberLight, w: 1.6 });
  box(c, iso, { x0: 5, x1: 6.2, y0: 8.6, y1: 9.2, z0: 1.1, z1: 1.2, col: [238, 226, 196], w: 0.8 });
  // The Tally House above the shop: its floor's edge and window sills (cut).
  box(c, iso, { x0: 0, x1: 20, y0: 9.6, y1: 10, z0: 3.7, z1: 4.1, col: C.timberDark, w: 1.2 });
  box(c, iso, { x0: 0, x1: 20, y0: 9.7, y1: 10, z0: 4.1, z1: 5.0, col: C.timber, w: 1 });
  for (const x of [2, 6, 10, 14]) opening(c, iso, 'y', 10.01, x, x + 3, 4.9, 5.0, [250, 236, 200], { w: 0.6 });
  // The quay: arcade posts, bollards, a ladder, the derrick crane.
  for (const x of [22, 28, 34]) box(c, iso, { x0: x - 0.25, x1: x + 0.25, y0: 8.6, y1: 9.1, z0: 0, z1: 4, col: C.timber, w: 1.1 });
  box(c, iso, { x0: 20, x1: 34, y0: 8.6, y1: 9.1, z0: 4, z1: 4.4, col: C.timberDark, w: 1.1 });
  for (const x of [24, 30]) cyl(c, iso, x, 8.3, 0.22, 0, 0.6, C.iron);
  for (let k = 0; k < 4; k++) seg(c, P(33.2, 9.05, -1.4 + k * 0.4), P(33.8, 9.05, -1.4 + k * 0.4), { w: 1.6 });
  cyl(c, iso, 25, 6.4, 0.3, 0, 9, C.timber);
  line(c, [P(25, 6.4, 8.6), P(27, 11.2, 7.4)], { w: 5, col: rgba(C.timber) }); seg(c, P(25, 6.4, 9), P(27, 11.2, 7.4), { w: 1, col: ink(0.7) }); seg(c, P(27, 11.2, 7.4), P(27, 11.2, 2.4), { w: 1.2, col: ink(0.8) });
  line(c, [P(26.85, 11.2, 2.4), P(27, 11.2, 2.1), P(27.15, 11.2, 2.4)], { w: 2, col: rgba(C.iron) });
  // Your boat at the fitting berth.
  isoHull(c, iso, 22, 10.7, 10, 3.4, -1.5, -0.75, { flip: true, rise: 0.3 });
  cyl(c, iso, 25.8, 10.7, 0.12, -0.7, 9.8, C.timberDark, { top: false, w: 1 });
  line(c, [P(25.8, 10.7, 1.2), P(30.8, 10.7, 1.0)], { w: 3, col: rgba(C.timberDark) }); fill(c, [P(25.8, 10.6, 1.3), P(30.6, 10.6, 1.1), P(30.6, 10.6, 1.5), P(25.8, 10.6, 1.6)], rgba([226, 214, 182]));
  // Lanterns, the trade sign, you.
  for (const [x, y] of [[6, 15], [6, 23], [14, 19]]) lantern(c, iso, x, y, 8, 12);
  isoFigure(c, iso, 21.5, 7, 0, 'you');
  // Interaction points.
  const E = (pt, label, dx = 16, dy = 6) => { const q = P(...pt); keyScrap(c, q[0], q[1], 'E', 26); if (label) txt(c, label, q[0] + dx, q[1] + dy, { size: 18, style: 'italic', halo: true, align: dx < 0 ? 'right' : 'left' }); };
  E([8, 10.4, 1.4], 'counter: buy, talk'); E([10, 0.2, 3.8], 'the plans: upgrades', 18, -6); E([10, 19, 4.9], 'look her over'); E([27, 9, 0.4], 'moor here: fitting berth', 18, 26);
  // Callouts and dimensions.
  callout(c, 'Keel Walk door, barred (G1): the first shortcut to the Keelgate', 120, 330, P(0, 12.7, 2.6), { size: 19 });
  callout(c, 'Half-models: the boats for sale', 120, 390, P(0, 5.5, 3.2), { size: 19 });
  callout(c, 'Timber racks', 120, 690, P(0, 24, 3.5), { size: 19 });
  callout(c, 'The next boat on the slip (14 m cutter, unrigged)', 120, 1130, P(9, 21.4, 2.6), { size: 19 });
  callout(c, 'Her mast on trestles', 120, 1210, P(6, 24.7, 1.2), { size: 19 });
  callout(c, 'Slipway out of the water', 120, 1300, P(26, 22, -2.6), { size: 19 });
  callout(c, 'Tally House above: its windows look down here', 1180, 300, P(16, 10, 5), { size: 19 });
  callout(c, 'Shop: counter, workbench, the plans on the wall', 1180, 380, P(16, 2, 1), { size: 19 });
  callout(c, 'Dock stair up to the Tally House', 1180, 460, P(30, 3.5, 1.4), { size: 19 });
  callout(c, 'Derrick for fitting upgrades', 1180, 540, P(25, 6.4, 7), { size: 19 });
  callout(c, 'Your boat at the fitting berth', 1180, 1380, P(27, 12.2, -0.8), { size: 19 });
  dimIso(c, iso, [0, 28.6, 0], [20, 28.6, 0], 'hall 20 m', [0, 26]); dimIso(c, iso, [20.4, 10, 0], [20.4, 28, 0], '18 m', [26, 14]);
  txt(c, 'NPC', npc[0] - 34, npc[1] - 4, { size: 17, f: FSC, col: rgba(C.route, 1), halo: true });
  spaceSheet(c, {
    badge: 'S', title: 'Shipwright: ships and upgrades',
    plan: { lv: 'quay', x0: 28, x1: 66, y0: -11, y1: 20, s: 17.5 },
    notes: [
      ['The space', 'Boat hall 20 × 18 m, 12 m high (quay deck to the upper deck), open to the dock at its aft end; the slipway runs down into the water.', 'Shop 20 × 10 m beside it, under the Tally House: counter, workbench, the plans board on the hull wall.'],
      ['What it is for', 'Show the next boat for sale, whole, on the slip. Fit upgrades to your boat at the fitting berth by the derrick. Choose and pay at the counter.', 'Later: boats you own lie at the fleet berths on the port quay; you change boats here.'],
      ['The NPC’s place', 'Behind the counter, facing the hall and the slip; the customer side is open floor at the hall’s edge.'],
      ['Seen first', 'Sailing in under the arch: the lit hall and the boat on the slip straight ahead.'],
      ['Closed edges', 'Keel Walk door (G1), timber store grille (G6), quay stores (G4).'],
    ],
  });
});

/* ======================================================================================================== 9 */
defSheet('tally', 'The Tally House: jobs and pay', 'Lower deck, at the top of the dock stair, over the shipwright’s shop. Seen from aft and inboard; the near walls are cut away.', 2400, 1600, (c) => {
  // Local: X = 64 - x (forward wall at X 0), Y = 20 - y (hull wall at Y 0); Z = 0 at the lower deck (+5.5).
  const iso = Iso(590, 600, 52), P = iso.P;
  quad(c, iso, [[0, 0, 0], [20, 0, 0], [20, 11, 0], [0, 11, 0]], C.timberLight, { w: 1.6 }); floorPlanks(c, iso, 0, 20, 0, 11, 0, 0.5);
  wallX(c, iso, 0, 0, 11, 0, 4); wallY(c, iso, 0, 0, 20, 0, 4);
  // The forward wall: the lane door (to the Well), the job board, the purser's door (G1).
  opening(c, iso, 'x', 0.02, 8, 10.8, 0, 2.8, [60, 40, 24]);
  { const a = P(0.05, 9.4, 2.1); txt(c, 'to the Well', a[0], a[1], { size: 16, style: 'italic', align: 'center', col: 'rgba(240,226,196,0.95)' }); }
  opening(c, iso, 'x', 0.02, 0.3, 1.4, 0, 2.3, C.timberDark); seg(c, P(0.05, 0.2, 1.2), P(0.05, 1.5, 1.2), { w: 4, col: rgba(C.iron) });
  quad(c, iso, [[0.03, 1.8, 0.9], [0.03, 7.2, 0.9], [0.03, 7.2, 3.3], [0.03, 1.8, 3.3]], C.timber, { w: 1.6 });
  quad(c, iso, [[0.04, 2.6, 3.35], [0.04, 6.4, 3.35], [0.04, 6.4, 3.85], [0.04, 2.6, 3.85]], C.cream, { w: 1.2 });
  { const a = P(0.05, 4.5, 3.47); txt(c, 'TALLY', a[0], a[1], { size: 22, f: FSC, align: 'center', rot: -0.524, spacing: 2 }); }
  { const r = rng(9); for (let k = 0; k < 6; k++) { const y = 2.1 + (k % 3) * 1.7, z = k < 3 ? 2.15 : 1.15; quad(c, iso, [[0.05, y, z], [0.05, y + 1.2, z], [0.05, y + 1.2, z + 0.95], [0.05, y, z + 0.95]], [240, 230, 204], { w: 0.9 }); for (let l = 0; l < 3; l++) seg(c, P(0.06, y + 0.15, z + 0.7 - l * 0.2), P(0.06, y + 0.5 + r() * 0.5, z + 0.7 - l * 0.2), { w: 1, col: ink(0.5) }); cyl(c, iso, 0.06, y + 0.6, 0.04, z + 0.85, z + 0.9, C.brass, { top: false, w: 0.5 }); } }
  // The hull wall: two open ports, the pay booth, the weighing bench.
  for (const x of [3, 16]) { opening(c, iso, 'y', 0.02, x, x + 1.6, 1.4, 2.6, [250, 238, 206]); }
  box(c, iso, { x0: 2, x1: 7, y0: 0, y1: 1.4, z0: 0, z1: 0.9, col: C.timber });
  cyl(c, iso, 4.5, 0.6, 0.05, 0.9, 2.0, C.brass, { top: false, w: 0.8 }); seg(c, P(3.6, 0.6, 1.95), P(5.4, 0.6, 1.95), { w: 2.4, col: rgba(C.brass) });
  for (const x of [3.6, 5.4]) { seg(c, P(x, 0.6, 1.95), P(x, 0.6, 1.5), { w: 0.8 }); line(c, ellipsePts(P(x, 0.6, 1.45)[0], P(x, 0.6, 1.45)[1], 14, 5, 16), { closed: true, w: 1.4, col: rgba(C.brass) }); }
  crate(c, iso, 2.2, 1.7); crate(c, iso, 3.0, 1.9, 0, 0.5); barrel(c, iso, 6.5, 2.0, 0.3, 0.8);
  box(c, iso, { x0: 8, x1: 14, y0: 0, y1: 2.6, z0: 0, z1: 2.6, col: C.timber, detail: (cc, ii, b) => { quad(cc, ii, [[9.5, 2.61, 1.0], [12.5, 2.61, 1.0], [12.5, 2.61, 2.0], [9.5, 2.61, 2.0]], [60, 40, 24], { w: 1.2 }); for (let x = 9.8; x < 12.5; x += 0.35) seg(cc, ii.P(x, 2.62, 1.0), ii.P(x, 2.62, 2.0), { w: 1.4, col: rgba(C.brass) }); } });
  box(c, iso, { x0: 9.3, x1: 12.7, y0: 2.6, y1: 3.0, z0: 0.95, z1: 1.05, col: C.timberLight, w: 1 });
  box(c, iso, { x0: 7.8, x1: 14.2, y0: -0.1, y1: 2.8, z0: 2.6, z1: 2.8, col: C.timberDark, w: 1.2 });
  quad(c, iso, [[10, 2.81, 2.85], [12, 2.81, 2.85], [12, 2.81, 3.35], [10, 2.81, 3.35]], C.cream, { w: 1 });
  { const a = P(11, 2.82, 3.0); txt(c, 'PAY', a[0], a[1], { size: 17, f: FSC, align: 'center', rot: 0.524 }); }
  for (let k = 0; k < 4; k++) box(c, iso, { x0: 14.6, x1: 15.4, y0: 0.4 + k * 0.05, y1: 1.0 + k * 0.05, z0: k * 0.22, z1: k * 0.22 + 0.2, col: [238, 236, 226], w: 0.8 });
  const npc = P(11, 1.2, 0);
  // The long table and benches, lanterns.
  box(c, iso, { x0: 6, x1: 15, y0: 5.4, y1: 5.8, z0: 0, z1: 0.45, col: C.timber, w: 1.2 });
  box(c, iso, { x0: 6, x1: 15, y0: 6.2, y1: 7.4, z0: 0, z1: 0.8, col: C.timberLight, w: 1.4 });
  box(c, iso, { x0: 6, x1: 15, y0: 7.8, y1: 8.2, z0: 0, z1: 0.45, col: C.timber, w: 1.2 });
  cyl(c, iso, 8, 6.8, 0.12, 0.8, 1.0, C.cream); cyl(c, iso, 12.5, 6.6, 0.1, 0.8, 0.95, C.timber);
  for (const [x, y] of [[4, 5], [10.5, 6.8], [17, 5]]) lantern(c, iso, x, y, 3.0, 4);
  // The cut walls: the inboard wall's sill with the windows into the Slip, the aft wall with the stair door.
  box(c, iso, { x0: 0, x1: 20, y0: 10.7, y1: 11, z0: 0, z1: 1.0, col: C.timber, w: 1.4 });
  for (const x of [2, 6, 10, 14]) quad(c, iso, [[x, 11, 1.0], [x + 3, 11, 1.0], [x + 3, 10.7, 1.0], [x, 10.7, 1.0]], [250, 236, 200], { w: 0.8 });
  box(c, iso, { x0: 19.7, x1: 20, y0: 0, y1: 3, z0: 0, z1: 0.6, col: C.timber, w: 1.2 }); box(c, iso, { x0: 19.7, x1: 20, y0: 6, y1: 10.7, z0: 0, z1: 0.6, col: C.timber, w: 1.2 });
  isoFigure(c, iso, 18.4, 4.6, 0, 'you, up from the dock');
  npcSpot(c, npc[0], npc[1], 26, Math.PI / 2 - 0.5); txt(c, 'NPC, behind the hatch', npc[0] + 34, npc[1] + 34, { size: 17, f: FSC, col: rgba(C.route, 1), halo: true });
  const E = (pt, label, dx = 18, dy = 6) => { const q = P(...pt); keyScrap(c, q[0], q[1], 'E', 28); if (label) txt(c, label, q[0] + dx, q[1] + dy, { size: 19, style: 'italic', halo: true, align: dx < 0 ? 'right' : 'left' }); };
  E([0.3, 4.5, 0.6], 'read the board', -20, 6); E([11, 3.4, 1.3], 'pay hatch: hand in, get paid'); E([4.5, 1.8, 1.1], 'deliver goods', -20);
  callout(c, 'Job board: six slips, the TALLY header, a chalk slate', 120, 300, P(0.05, 3, 3.1), { size: 19 });
  callout(c, 'Lane door forward to the Well', 120, 1000, P(0.05, 9.4, 1.2), { size: 19 });
  callout(c, 'Purser’s door, barred (G1)', 120, 430, P(0.05, 0.85, 2.2), { size: 19 });
  callout(c, 'Pay booth with a grilled hatch; salt cakes stacked beside it', 1180, 290, P(12, 0.4, 2.8), { size: 19 });
  callout(c, 'Weighing bench for delivered goods', 1180, 360, P(4.5, 0.4, 2.0), { size: 19 });
  callout(c, 'Open ports in the hull', 1180, 430, P(17, 0.02, 2.4), { size: 19 });
  callout(c, 'Long table and benches: sailors waiting for pay', 1180, 1420, P(14, 7.4, 0.6), { size: 19 });
  callout(c, 'Windows down into the Slip (inboard wall, cut at the sill)', 120, 1420, P(5, 11, 1.0), { size: 19 });
  callout(c, 'Stair door from the dock (aft wall, cut)', 1180, 1340, P(20, 4.5, 0.3), { size: 19 });
  dimIso(c, iso, [0, 11.6, 0], [20, 11.6, 0], '20 m', [-6, 30]); dimIso(c, iso, [20.6, 0, 0], [20.6, 11, 0], '11 m', [30, 14]);
  spaceSheet(c, {
    badge: 'J', title: 'Tally House: side jobs, paid in salt',
    plan: { lv: 'lower', x0: 40, x1: 76, y0: 4, y1: 21, s: 20 },
    notes: [
      ['The space', 'One room 20 × 11 m, 4 m high, on the lower deck over the shipwright’s shop. The dock stair comes up through its aft wall; the lane leaves forward to the Well.'],
      ['What it is for', 'Read the jobs on the board, take one, bring it back, be paid in salt at the hatch. Goods for delivery jobs go on the weighing bench.'],
      ['The NPC’s place', 'Inside the pay booth, behind the grilled hatch, facing into the room. The board and the bench are in reach of the hatch.'],
      ['Seen first', 'Up the dock stair: the board lit ahead, the hatch to the left, the boat on the slip below through the windows to the right.'],
      ['Closed edges', 'The purser’s door (G1). The room is otherwise whole.'],
    ],
  });
});

/* ======================================================================================================== 10 */
defSheet('keelgate', 'The Keelgate: the main quest’s space', 'Quay deck, at the foot of the Well. Seen from aft and to starboard; the near walls and the stair’s side are cut away.', 2400, 1600, (c) => {
  // Local: X = 96 - x (forward wall at X 0), Y = y + 12 (port wall at Y 0); Z = 0 at the quay deck (+1.5).
  const iso = Iso(810, 600, 34), P = iso.P;
  quad(c, iso, [[0, 0, 0], [24, 0, 0], [24, 24, 0], [0, 24, 0]], C.timberLight, { w: 1.6 }); floorPlanks(c, iso, 0, 24, 0, 24, 0, 0.6);
  wallX(c, iso, 0, 0, 24, 0, 4, C.timberDark); wallY(c, iso, 0, 0, 24, 0, 4, C.timberDark);
  // The lower deck's floor round the shaft (cut at the back), its rail, the middle deck above it; a figure for scale.
  for (const zf of [4, 8]) {
    box(c, iso, { x0: 0, x1: 24, y0: 0, y1: 6, z0: zf - 0.4, z1: zf, col: C.timberDark, w: 1.2 }); box(c, iso, { x0: 0, x1: 6, y0: 6, y1: 24, z0: zf - 0.4, z1: zf, col: C.timberDark, w: 1.2 });
    seg(c, P(6, 6, zf + 1.1), P(18, 6, zf + 1.1), { w: 2, col: rgba(C.timber) }); seg(c, P(6, 6, zf + 1.1), P(6, 18, zf + 1.1), { w: 2, col: rgba(C.timber) });
    for (let u = 6.5; u < 18; u += 1.5) { seg(c, P(u, 6, zf), P(u, 6, zf + 1.1), { w: 1.2 }); seg(c, P(6, u, zf), P(6, u, zf + 1.1), { w: 1.2 }); }
  }
  wallX(c, iso, 0, 0, 24, 4, 7.6, C.timber); wallY(c, iso, 0, 0, 24, 4, 7.6, C.timber);
  for (const [pl, u] of [['x', 9], ['y', 12]]) { opening(c, iso, pl, 0.02, u, u + 1.8, 4, 6.4, C.timberDark); seg(c, pl === 'x' ? P(0.05, u - 0.2, 5.2) : P(u - 0.2, 0.05, 5.2), pl === 'x' ? P(0.05, u + 2, 5.2) : P(u + 2, 0.05, 5.2), { w: 4, col: rgba(C.iron) }); }
  isoFigure(c, iso, 3, 4, 4);
  // Shelves of salvaged ancient pieces along the port wall; rubbings between the grilles.
  box(c, iso, { x0: 2, x1: 17, y0: 0, y1: 0.6, z0: 0, z1: 3.2, col: C.timberDark, w: 1.2 });
  { const r = rng(4); for (const z of [0.6, 1.4, 2.2]) { seg(c, P(2, 0.6, z), P(17, 0.6, z), { w: 1.4, col: rgba(C.timberLight) }); for (let x = 2.3; x < 16.6; x += 0.5 + r() * 0.5) { const glow = r() < 0.25; cyl(c, iso, x, 0.3, 0.13, z, z + 0.3 + r() * 0.2, glow ? mix(C.ancient, C.glow, 0.5) : [200, 196, 184], { w: 0.7 }); } } }
  for (const [y0] of [[5], [16]]) { opening(c, iso, 'x', 0.02, y0, y0 + 3, 0, 2.6, [56, 48, 40]); for (let u = y0 + 0.2; u < y0 + 3; u += 0.3) seg(c, P(0.05, u, 0), P(0.05, u, 2.6), { w: 2.2, col: rgba(C.iron) }); seg(c, P(0.05, y0, 1.3), P(0.05, y0 + 3, 1.3), { w: 2, col: rgba(C.iron) }); }
  for (let k = 0; k < 4; k++) { const y = 9.2 + k * 1.5; quad(c, iso, [[0.05, y, 1.4], [0.05, y + 1.2, 1.4], [0.05, y + 1.2, 2.9], [0.05, y, 2.9]], [232, 226, 212], { w: 0.9 }); const cc = P(0.06, y + 0.6, 2.15); line(c, ellipsePts(cc[0], cc[1], 9, 13, 16), { closed: true, w: 1.2, col: ink(0.55) }); }
  // The Keelgate.
  const kg = { x: 12, y: 12 };
  disc(c, iso, kg.x, kg.y, 0.02, 4.0, C.ancient, { w: 2 });
  disc(c, iso, kg.x, kg.y, 0.06, 3.0, mix(C.ancient, C.glow, 0.12), { w: 1.6 });
  { const g = P(kg.x, kg.y, 0.06), gr = c.createRadialGradient(g[0], g[1], 0, g[0], g[1], 3.2 * 34 * 1.22); gr.addColorStop(0, 'rgba(110,230,210,0.30)'); gr.addColorStop(1, 'rgba(110,230,210,0)'); c.save(); c.scale(1, 1); c.fillStyle = gr; c.beginPath(); c.ellipse(g[0], g[1], 3.2 * 34 * 1.22, 3.2 * 34 * 0.71, 0, 0, 7); c.fill(); c.restore(); }
  for (const rr of [1.0, 2.1]) { const p = []; for (let i = 0; i <= 60; i++) { const t = i / 60 * Math.PI * 2; p.push(P(kg.x + Math.cos(t) * rr, kg.y + Math.sin(t) * rr, 0.07)); } line(c, p, { w: 2.4, col: rgba(C.glow, 0.95) }); }
  for (let k = 0; k < 7; k++) { const t = k / 7 * Math.PI * 2 - Math.PI / 2; seg(c, P(kg.x + Math.cos(t) * 1.0, kg.y + Math.sin(t) * 1.0, 0.07), P(kg.x + Math.cos(t) * 2.8, kg.y + Math.sin(t) * 2.8, 0.07), { w: 1.8, col: rgba(C.glow, 0.85) }); const sk = P(kg.x + Math.cos(t) * 3.5, kg.y + Math.sin(t) * 3.5, 0.07); c.save(); c.fillStyle = ink(0.75); c.beginPath(); c.ellipse(sk[0], sk[1], 6, 3.6, 0, 0, 7); c.fill(); c.restore(); }
  // Daylight falling down the Well onto the gate.
  { const a = [P(6, 6, 16), P(18, 6, 16), P(18, 18, 16), P(6, 18, 16)], b = [P(7.5, 7.5, 0), P(16.5, 7.5, 0), P(16.5, 16.5, 0), P(7.5, 16.5, 0)];
    const g = c.createLinearGradient(0, a[0][1], 0, b[2][1]); g.addColorStop(0, 'rgba(255,250,232,0.0)'); g.addColorStop(1, 'rgba(255,250,232,0.32)');
    c.save(); c.fillStyle = g; c.beginPath(); c.moveTo(...a[3]); c.lineTo(...a[0]); c.lineTo(...a[1]); c.lineTo(...b[1]); c.lineTo(...b[2]); c.lineTo(...b[3]); c.closePath(); c.fill(); c.restore(); }
  // The desk: the NPC's place. A chair, books, a lamp, rubbings.
  const npc = P(19.5, 6.2, 0);
  box(c, iso, { x0: 18.5, x1: 22, y0: 3, y1: 5, z0: 0, z1: 0.85, col: C.timber });
  box(c, iso, { x0: 19, x1: 20.2, y0: 3.4, y1: 4.4, z0: 0.85, z1: 1.05, col: [232, 226, 212], w: 0.8 }); box(c, iso, { x0: 20.6, x1: 21.4, y0: 3.3, y1: 3.9, z0: 0.85, z1: 1.25, col: C.oxblood, w: 0.8 });
  cyl(c, iso, 21.6, 4.6, 0.1, 0.85, 1.4, C.brass); lantern(c, iso, 21.6, 4.6, 1.55, 1.55, { r: 0.18 });
  box(c, iso, { x0: 22.4, x1: 23.2, y0: 3.6, y1: 4.4, z0: 0, z1: 0.5, col: C.timberDark, w: 1 });
  // The hoist cage in the shaft's corner, its ropes going up.
  box(c, iso, { x0: 15.2, x1: 17.8, y0: 15.2, y1: 17.8, z0: 0, z1: 0.2, col: C.timberLight, w: 1.2 });
  for (const [x, y] of [[15.2, 15.2], [17.8, 15.2], [17.8, 17.8], [15.2, 17.8]]) seg(c, P(x, y, 0.2), P(x, y, 2.4), { w: 1.6 });
  line(c, [P(15.2, 15.2, 2.4), P(17.8, 15.2, 2.4), P(17.8, 17.8, 2.4), P(15.2, 17.8, 2.4)], { closed: true, w: 1.6 });
  seg(c, P(16.5, 16.5, 2.4), P(16.5, 16.5, 14), { w: 1.2 });
  // The stair down from the lower deck along the starboard side (cut), you at its foot.
  for (let i = 0; i < 12; i++) { const x = 10 + i * (7 / 12), z = 4 - i * 4 / 12; box(c, iso, { x0: x, x1: x + 7 / 12, y0: 20, y1: 23, z0: 0, z1: z, col: C.timberLight, w: 0.8, seed: i }); }
  isoFigure(c, iso, 9, 21, 0, 'you');
  npcSpot(c, npc[0], npc[1], 22, Math.PI * 0.75); txt(c, 'NPC', npc[0] + 26, npc[1] + 26, { size: 17, f: FSC, col: rgba(C.route, 1), halo: true });
  for (const [x, y] of [[2, 12, 3], [12, 2, 3]]) lantern(c, iso, x, y, 3.0, 4);
  const E = (pt, label, dx = 18, dy = 6) => { const q = P(...pt); keyScrap(c, q[0], q[1], 'E', 28); if (label) txt(c, label, q[0] + dx, q[1] + dy, { size: 19, style: 'italic', halo: true, align: dx < 0 ? 'right' : 'left' }); };
  E([17.6, 7.6, 1.4], 'talk'); E([9, 9, 0.3], 'look at the gate', -20); E([16.5, 17.8, 1.4], 'hoist: up');
  callout(c, 'Grilles into the Holds (G6)', 120, 330, P(0.05, 6.5, 2.2), { size: 19 });
  callout(c, 'Rubbings of the gate’s lines pinned up', 120, 400, P(0.05, 11, 2.6), { size: 19 });
  callout(c, 'The Well’s galleries above, doors closed', 120, 470, P(0.05, 9.9, 5.8), { size: 19 });
  callout(c, 'The Keelgate: the ancient seal in the floor (G5)', 120, 1200, P(9.5, 14.5, 0.1), { size: 20, sub: 'pale ring 8 m across, turquoise lines, seven empty sockets (an idea, not a rule)' });
  callout(c, 'Daylight falls 16 m down the Well onto it', 120, 1330, P(8, 15, 3.5), { size: 19 });
  callout(c, 'Shelves of salvaged ancient pieces, a few faintly glowing', 1240, 300, P(15, 0.3, 2.6), { size: 19 });
  callout(c, 'The reading desk: books, a lamp, a chair', 1240, 380, P(21, 3, 1.0), { size: 19 });
  callout(c, 'The hoist cage (to the lower and main decks)', 1240, 1200, P(17.8, 17.8, 1.6), { size: 19 });
  callout(c, 'Stair down from the lower deck (cut)', 1240, 1300, P(14, 23, 1.6), { size: 19 });
  callout(c, 'Keel Walk door (aft wall, cut): locked shortcut to the Slip', 1240, 1400, P(24, 18, 0.3), { size: 19 });
  spaceSheet(c, {
    badge: 'Q', title: 'Keelgate: the main quest',
    plan: { lv: 'quay', x0: 70, x1: 98, y0: -13, y1: 13, s: 24 },
    notes: [
      ['The space', 'Chamber 24 × 24 m, 4 m high round its edge; the middle 12 × 12 m is the foot of the Well, open 16 m up to the sky.'],
      ['What it is for', 'The quiet heart of the ship: the place where the main quest is given and returned to. The gate in the floor is the way into the ancient Deep, sealed for now.'],
      ['The NPC’s place', 'At the reading desk by the port wall, facing the gate; the gate between them and the stair you come down.'],
      ['Mood', 'Warm lamp at the desk against the pale gate and its faint turquoise; daylight from above by day, the gate’s glow by night.'],
      ['Closed edges', 'Holds grilles (G6), Keel Walk door (G1), the Keelgate itself (G5).'],
    ],
  });
});

/* ======================================================================================================== 11 */
defSheet('chandlery', 'The Chandlery: the equipment space', 'Main deck, a deckhouse on the port side forward of the Well. Seen from the open deck; roof cut away.', 2400, 1600, (c) => {
  // Local: X = x - 98 (aft wall at X 0), Y = y + 19 (outboard wall at Y 0); Z = 0 at the main deck (+17.5).
  const iso = Iso(880, 580, 40), P = iso.P;
  quad(c, iso, [[-2, 0, 0], [18, 0, 0], [18, 21, 0], [-2, 21, 0]], [196, 150, 98], { w: 1.4 }); floorPlanks(c, iso, -2, 18, 0, 21, 0, 0.7);
  quad(c, iso, [[0, 0, 0], [16, 0, 0], [16, 11, 0], [0, 11, 0]], C.timberLight, { w: 1.6 }); floorPlanks(c, iso, 0, 16, 0, 11, 0, 0.45);
  wallX(c, iso, 0, 0, 11, 0, 4.2, [160, 112, 64]); wallY(c, iso, 0, 0, 16, 0, 4.2, [160, 112, 64]);
  shelfUnit(c, iso, 0.5, 15.5, 0, 0.6, 3.6, 'y', 3); shelfUnit(c, iso, 0, 0.6, 0.8, 9.8, 3.6, 'x', 7);
  line(c, [P(9, 0.7, 0), P(9.5, 0.65, 3.6)], { w: 2.2, col: rgba(C.timber) }); line(c, [P(9.8, 0.7, 0), P(10.3, 0.65, 3.6)], { w: 2.2, col: rgba(C.timber) });
  for (let k = 1; k < 9; k++) seg(c, P(9 + k * 0.055, 0.7, k * 0.4), P(9.8 + k * 0.055, 0.7, k * 0.4), { w: 1.4, col: rgba(C.timber) });
  // Roof beams with stock hanging from them.
  for (const x of [4, 8, 12]) box(c, iso, { x0: x - 0.15, x1: x + 0.15, y0: 0, y1: 11, z0: 3.9, z1: 4.2, col: C.timberDark, w: 1 });
  lantern(c, iso, 4, 3, 3.1, 3.9, { r: 0.2 }); lantern(c, iso, 8, 6, 3.0, 3.9, { r: 0.2 }); lantern(c, iso, 12, 3.5, 3.1, 3.9, { r: 0.2 });
  for (const [x, y] of [[4, 6.5], [12, 7]]) { seg(c, P(x, y, 3.9), P(x, y, 3.3), { w: 1 }); const q = P(x, y, 3.1); line(c, ellipsePts(q[0], q[1], 16, 8, 20), { closed: true, w: 3, col: rgba([196, 160, 104]) }); line(c, ellipsePts(q[0], q[1] + 4, 12, 6, 20), { closed: true, w: 2.4, col: rgba([180, 146, 94]) }); }
  { const net = []; for (let i = 0; i <= 10; i++) net.push(P(8, 1.5 + i * 0.4, 3.9 - Math.sin(i / 10 * Math.PI) * 0.9)); line(c, net, { w: 1.4, col: ink(0.6) }); }
  // Display table with the tools of the wheel (compass, spyglass, lantern) and room for more.
  box(c, iso, { x0: 5, x1: 10, y0: 4.5, y1: 6.5, z0: 0, z1: 0.85, col: C.timber });
  cyl(c, iso, 6, 5.4, 0.22, 0.85, 0.95, C.brass); seg(c, P(7.2, 5.2, 0.95), P(8.6, 5.8, 1.0), { w: 5, col: rgba(C.brass) }); cyl(c, iso, 9.3, 5.5, 0.15, 0.85, 1.25, C.iron);
  barrel(c, iso, 2, 9.6, 0.32, 0.9); barrel(c, iso, 13.5, 2.2, 0.32, 0.9); crate(c, iso, 14, 8.5);
  const npc = P(7, 9.4, 0); npcSpot(c, npc[0], npc[1], 24, Math.PI / 2 - 0.4); txt(c, 'NPC', npc[0] - 40, npc[1] + 6, { size: 17, f: FSC, col: rgba(C.route, 1), halo: true });
  // The shop front: counter wall, posts, the door, the awning, the sign.
  box(c, iso, { x0: 1.5, x1: 12, y0: 10.6, y1: 11, z0: 0, z1: 1.1, col: [160, 112, 64] }); box(c, iso, { x0: 1.3, x1: 12.2, y0: 10.5, y1: 11.2, z0: 1.1, z1: 1.22, col: C.timberLight, w: 1.2 });
  for (const [x0, x1] of [[0, 1.5], [12, 13], [15, 16]]) box(c, iso, { x0, x1, y0: 10.6, y1: 11, z0: 0, z1: 4.2, col: [160, 112, 64] });
  box(c, iso, { x0: 15.6, x1: 16, y0: 0, y1: 10.6, z0: 0, z1: 4.2, col: [150, 104, 60] });
  opening(c, iso, 'x', 16.01, 4, 6.5, 1.4, 2.8, [250, 236, 206]);
  { const aw = [P(1, 11, 3.3), P(12.5, 11, 3.3), P(12.5, 13.2, 2.75), P(1, 13.2, 2.75)]; fill(c, aw, rgba([226, 214, 186])); c.save(); pathOf(c, aw); c.clip(); for (let x = 1; x < 12.5; x += 1.15) fill(c, [P(x, 11, 3.3), P(x + 0.57, 11, 3.3), P(x + 0.57, 13.2, 2.75), P(x, 13.2, 2.75)], rgba(C.oxblood, 0.55)); c.restore(); line(c, aw, { closed: true, w: 1.6 }); }
  for (const x of [1.2, 12.3]) seg(c, P(x, 13.2, 0), P(x, 13.2, 2.75), { w: 2.4, col: rgba(C.timberDark) });
  box(c, iso, { x0: 3, x1: 11, y0: 10.8, y1: 11, z0: 3.5, z1: 4.4, col: C.cream, w: 1.4 });
  { const a = P(7, 11.01, 3.82); txt(c, 'CHANDLER', a[0], a[1], { size: 26, f: FSC, align: 'center', rot: 0.524, spacing: 2 }); }
  dashedRoof();
  function dashedRoof() { line(c, [P(0, 0, 4.2), P(16, 0, 4.2), P(16, 11, 4.2), P(0, 11, 4.2)], { closed: true, w: 1, dash: [6, 6], col: ink(0.5) }); line(c, [P(0, 5.5, 5.4), P(16, 5.5, 5.4)], { w: 1, dash: [6, 6], col: ink(0.5) }); }
  // On deck: barrels and crates by the door, the try-it spyglass and binnacle, the main mast with its fife rail.
  barrel(c, iso, 14.4, 12.4); crate(c, iso, 15.2, 11.6); crate(c, iso, 15.5, 12.6, 0, 0.55);
  { const t = [8, 14.5]; for (const a of [0, 2.1, 4.2]) seg(c, P(t[0] + Math.cos(a) * 0.5, t[1] + Math.sin(a) * 0.5, 0), P(t[0], t[1], 1.4), { w: 2, col: rgba(C.timber) }); seg(c, P(t[0] - 0.6, t[1] - 0.3, 1.5), P(t[0] + 0.7, t[1] + 0.2, 1.62), { w: 6, col: rgba(C.brass) }); }
  box(c, iso, { x0: 11.6, x1: 12.4, y0: 14.1, y1: 14.9, z0: 0, z1: 1.0, col: C.timber }); cyl(c, iso, 12, 14.5, 0.38, 1.0, 1.25, C.brass);
  box(c, iso, { x0: 0, x1: 4, y0: 17, y1: 21, z0: 0.75, z1: 0.9, col: C.timberLight, w: 1.2 });
  for (const [x, y] of [[0.2, 17.2], [3.6, 17.2], [3.6, 20.6], [0.2, 20.6]]) box(c, iso, { x0: x, x1: x + 0.2, y0: y, y1: y + 0.2, z0: 0, z1: 0.75, col: C.timber, w: 0.8 });
  for (let k = 0; k < 7; k++) { const x = 0.4 + k * 0.5; seg(c, P(x, 17.05, 0.9), P(x, 17.05, 1.15), { w: 2.4, col: rgba(C.timberDark) }); }
  cyl(c, iso, 2, 19, 1.0, 0, 9, C.timberDark, { hoops: [1.4, 3, 6], top: false });
  isoFigure(c, iso, 5, 16.4, 0, 'you');
  const E = (pt, label, dx = 18, dy = 6) => { const q = P(...pt); keyScrap(c, q[0], q[1], 'E', 28); if (label) txt(c, label, q[0] + dx, q[1] + dy, { size: 19, style: 'italic', halo: true, align: dx < 0 ? 'right' : 'left' }); };
  E([7, 12.4, 1.5], 'counter: buy, talk'); E([8, 14.5, 2.0], 'try the spyglass', -20); E([12, 14.5, 1.6], 'try the compass');
  callout(c, 'Shelves floor to roof: tins, jars, coils, boxes; a ladder to the top', 120, 320, P(4, 0.3, 3.3), { size: 19 });
  callout(c, 'Stock hanging from the beams: lanterns, coils, a net', 120, 400, P(4, 6.5, 3.1), { size: 19 });
  callout(c, 'Display table: the gear of the tool wheel, room for what comes later', 120, 1180, P(5.5, 6.5, 0.7), { size: 19 });
  callout(c, 'Main mast and its fife rail: the shop faces it', 120, 1300, P(1, 19.5, 2.5), { size: 19 });
  callout(c, 'Awning over the counter (muted oxblood and cream)', 1260, 300, P(12.5, 12, 3.0), { size: 19 });
  callout(c, 'Trade sign on the roof edge', 1260, 370, P(11, 11, 4.2), { size: 19 });
  callout(c, 'Window to the open deck', 1260, 440, P(16, 5.2, 2.5), { size: 19 });
  callout(c, 'Door: walk in among the shelves', 1260, 1060, P(14, 11, 1.0), { size: 19 });
  callout(c, 'Try-it spots on deck: spyglass on a tripod, a compass binnacle', 1260, 1340, P(10, 14.6, 1.2), { size: 19 });
  dimIso(c, iso, [0, -0.4, 4.8], [16, -0.4, 4.8], '16 m', [0, -14]); dimIso(c, iso, [16.8, 0, 0], [16.8, 11, 0], '11 m', [30, 14]);
  spaceSheet(c, {
    badge: 'E', title: 'Chandlery: equipment',
    plan: { lv: 'main', x0: 94, x1: 116, y0: -20, y1: 3, s: 26 },
    notes: [
      ['The space', 'A deckhouse 16 × 11 m, 4.2 m to the eaves, on the port side of the main deck between the Well and the main mast. Shop front with an awning facing the open deck.'],
      ['What it is for', 'Equipment: the tools of the wheel today (compass, spyglass, lantern) and later gear. Buy at the counter from the deck, or walk in and browse the shelves.'],
      ['The NPC’s place', 'Behind the counter inside, facing out through the open front to the deck and the mast.'],
      ['Seen first', 'Up the Well’s stair onto the main deck: the awning and the sign ahead to port, under the masts.'],
      ['Closed edges', 'None in the shop; the cargo deck’s rope (G2) is 2 m forward of it.'],
    ],
  });
});
