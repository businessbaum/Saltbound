/* Saltbound hub ship plans, sheets 1-6: the idea, arriving, the long section, the exploded decks, the cross-sections. */
'use strict';

const SHEETS = [];
function defSheet(id, title, sub, w, h, draw) { SHEETS.push({ id, title, sub, w, h, draw }); }
const AREAS = countAreas();
const fmt = n => Math.round(n).toLocaleString('en-GB');
const pct = (a, b) => (100 * a / b).toFixed(0) + ' %';

/* Helpers shared by the elevation-type views. */
function zoneFill(c, pts, status, seed = 1, a = null) {
  if (status === 'open') wash(c, pts, C.open, a ?? 0.5, seed);
  else if (status === 'water') wash(c, pts, C.water, a ?? 0.45, seed);
  else if (status === 'ancient') { wash(c, pts, C.ancient, a ?? 0.85, seed); }
  else if (status === 'locked') { wash(c, pts, C.locked, a ?? 0.26, seed); hatch(c, pts, { gap: 8, col: ink(0.22), seed }); }
}
function zoneName(c, s, x, y, status, size = 19) {
  if (status === 'locked') txt(c, s.toUpperCase(), x, y, { size: size * 0.85, f: FSC, align: 'center', col: ink(0.62), halo: true, spacing: 1 });
  else txt(c, s, x, y, { size, f: FL, align: 'center', halo: true });
}
/** Furled sail on a yard: a row of bulges on top of the spar from (x0, y) to (x1, y). */
function furled(c, x0, x1, y, t, col = [228, 216, 184], seed = 1) {
  const r = rng(seed), n = Math.max(2, Math.round(Math.abs(x1 - x0) / (t * 2.2)));
  for (let i = 0; i < n; i++) {
    const a = x0 + (x1 - x0) * i / n, b = x0 + (x1 - x0) * (i + 1) / n, m = (a + b) / 2, h = t * (0.9 + r() * 0.5);
    const p = ellipsePts(m, y - h * 0.35, Math.abs(b - a) * 0.56, h * 0.75, 18);
    fill(c, p, rgba(col)); line(c, p, { closed: true, w: 1, col: ink(0.55), amp: 0.2, seed: seed + i });
  }
}

/* ======================================================================================================== 1 */
defSheet('concept', 'The hub ship: the idea', 'A ship as big as a town, already lying in whichever harbour you sail to. Her shops, her jobs and her mystery.', 2400, 1600, (c) => {
  // Panel 1: the loop.
  txt(c, '1 · The loop', 80, 230, { size: 36, f: FL });
  const cx = 440, cy = 600, R = 245;
  line(c, ellipsePts(cx, cy, R, R, 90), { closed: true, w: 2.4, amp: 0.8, seed: 3 });
  const st = [
    ['Sail out', 'from the hub’s dock', -90], ['Explore', 'islands, ruins, wrecks', -30], ['Gather', 'finds, jobs done', 30],
    ['Sail back', 'to the nearest harbour', 90], ['Get paid', 'salt at the Tally House', 150], ['Spend', 'boats, upgrades, gear', 210],
  ];
  for (const [i, [a, b, deg]] of st.entries()) {
    const t = deg * Math.PI / 180, x = cx + Math.cos(t) * R, y = cy + Math.sin(t) * R;
    const t2 = (deg + 52) * Math.PI / 180, t1 = (deg + 8) * Math.PI / 180;
    arrow(c, [cx + Math.cos(t1) * R, cy + Math.sin(t1) * R], [cx + Math.cos(t2) * R, cy + Math.sin(t2) * R], { w: 0.01, head: 16, seed: i });
    c.save(); c.fillStyle = 'rgba(236,222,186,0.95)'; c.beginPath(); c.arc(x, y, 14, 0, 7); c.fill(); c.restore();
    line(c, ellipsePts(x, y, 14, 14, 20), { closed: true, w: 2, seed: i + 4 });
    const right = Math.cos(t) > 0.2, left = Math.cos(t) < -0.2, al = right ? 'left' : left ? 'right' : 'center';
    const lx = x + (right ? 26 : left ? -26 : 0), ly = y + (Math.sin(t) < -0.5 ? -34 : Math.sin(t) > 0.5 ? 50 : 6);
    txt(c, a, lx, ly, { size: 27, f: FL, align: al, halo: true }); txt(c, b, lx, ly + 26, { size: 19, style: 'italic', align: al, halo: true });
  }
  // The hub in the middle with its four services.
  const hs = 1.35, hx = cx - 80, hy = cy + 20;
  const hull = [[0, -14], [104, -14], [118, -24], [124, -36], [118, -36], [108, -26], [6, -26], [0, -50], [-6, -50], [-6, -14]].map(p => [hx + p[0] * hs * 0.8, hy + p[1] * hs * 0.8]);
  for (const mx of [30, 62, 92]) seg(c, [hx + mx * hs * 0.8, hy - 26 * hs * 0.8], [hx + mx * hs * 0.8, hy - 110 * hs * 0.8 + (mx === 62 ? -18 : 0)], { w: 2.2, seed: mx });
  fill(c, hull, rgba(C.timberDark, 0.85)); line(c, hull, { closed: true, w: 2, seed: 8 });
  seg(c, [hx - 30, hy - 10], [hx + 175, hy - 10], { w: 1.5, col: rgba(C.water, 1), seed: 2 });
  txt(c, 'THE HUB SHIP', cx, cy + 62, { size: 22, f: FSC, align: 'center', spacing: 2 });
  ['S', 'J', 'Q', 'E'].forEach((k, i) => badge(c, cx - 66 + i * 44, cy + 104, k, 16));
  let y = 1000;
  y = para(c, 'Saltbound stays a sailing game: the sea is where you play. The hub is where a voyage ends and the next one begins: you sell nothing to a menu, you walk her decks to the people who build boats, pay for jobs, sell gear, and know something about the sea’s past.', 80, y, 720, { size: 22 });
  y = notes(c, [['Four services, one each', 'S · Shipwright: boats and upgrades', 'J · Tally House: side jobs, paid in salt', 'Q · the Keelgate: the main quest', 'E · Chandlery: equipment']], 80, y + 26, 720, { size: 22, head: 26 });

  // Panel 2: always already there.
  const px = 900; txt(c, '2 · Always already there', px, 230, { size: 36, f: FL });
  const A = [px + 150, 760], Bh = [px + 560, 360];
  for (const [p, name, ghost] of [[A, 'Saltbound Cove', true], [Bh, 'a second harbour (later)', false]]) {
    line(c, ellipsePts(p[0], p[1], 150, 150, 70), { closed: true, w: 1.6, dash: [7, 7], col: ink(0.6), seed: p[0] });
    const coast = ellipsePts(p[0] - 18, p[1] + 22, 46, 26, 26).map((q, i) => [q[0] + Math.sin(i * 1.7) * 5, q[1] + Math.cos(i * 2.3) * 4]);
    wash(c, coast, [190, 168, 120], 0.7); line(c, coast, { closed: true, w: 1.8 });
    txt(c, name, p[0] - 20, p[1] + 80, { size: 21, style: 'italic', align: 'center', halo: true });
    const sx = p[0] + 46, sy = p[1] - 40, shipPts = [[0, 0], [44, 0], [52, -8], [44, -10], [0, -10]].map(q => [sx + q[0], sy + q[1]]);
    if (ghost) { line(c, shipPts, { closed: true, w: 1.8, dash: [4, 5], col: ink(0.55) }); txt(c, 'she was here', sx + 26, sy - 20, { size: 18, style: 'italic', align: 'center', col: ink(0.7) }); }
    else { fill(c, shipPts, rgba(C.timberDark, 0.9)); line(c, shipPts, { closed: true, w: 1.8 }); for (const mx of [12, 24, 36]) seg(c, [sx + mx, sy - 10], [sx + mx, sy - 34], { w: 1.4 }); txt(c, 'and is already here', sx + 26, sy - 46, { size: 18, style: 'italic', align: 'center' }); }
  }
  txt(c, '2.5 km: lost in the haze', A[0] + 96, A[1] - 148, { size: 18, style: 'italic', col: ink(0.75) });
  const track = []; for (let i = 0; i <= 40; i++) { const t = i / 40; track.push([A[0] + 60 + (Bh[0] - A[0] - 100) * t + Math.sin(t * 7) * 18, A[1] - 60 + (Bh[1] - A[1] + 60) * t]); }
  line(c, track, { w: 3, col: rgba(C.route, 1), dash: [2, 8], seed: 5 });
  const mid = track[20]; txt(c, 'your voyage', mid[0] + 20, mid[1] + 6, { size: 20, style: 'italic', col: rgba(C.route, 1), halo: true });
  const zone = track.slice(14, 27); line(c, zone, { w: 9, col: rgba(C.route, 0.18), cap: 'butt' });
  txt(c, 'she moves here, unseen', track[26][0] + 22, track[26][1] - 4, { size: 18, style: 'italic', col: ink(0.75), halo: true });
  notes(c, [['The rule',
    'She lies at the berth of the harbour you last moored at or are sailing to.',
    'She moves only while you are more than 2.5 km from both berths, so the haze hides both.',
    'No sailing, no animation: one moment she is behind you in the haze, the next she lies ahead.',
    'Today there is one harbour, so she simply lies off Saltbound Cove. The rule matters from the second harbour on.']], px, 1010, 690, { size: 21 });

  // Panel 3: salt.
  const qx = 1690; txt(c, '3 · Salt, the currency', qx, 230, { size: 36, f: FL });
  const iso = Iso(qx + 250, 470, 120);
  box(c, iso, { x0: -0.55, x1: 0.55, y0: -0.4, y1: 0.4, z0: 0, z1: 0.32, col: [238, 236, 226], w: 2 });
  // The waxed-paper band round its middle (only the faces you see) and the twine.
  const band = (z0, z1) => { quad(c, iso, [[0.56, -0.41, z0], [0.56, 0.41, z0], [0.56, 0.41, z1], [0.56, -0.41, z1]], [206, 182, 132], { w: 1.4 }); quad(c, iso, [[-0.56, 0.41, z0], [0.56, 0.41, z0], [0.56, 0.41, z1], [-0.56, 0.41, z1]], [186, 162, 114], { w: 1.4 }); };
  band(0.08, 0.2);
  const tp = (x, y) => iso.P(x, y, 0.32);
  const ring = []; for (let i = 0; i <= 40; i++) { const t = i / 40 * Math.PI * 2; ring.push(tp(Math.cos(t) * 0.24, Math.sin(t) * 0.24)); }
  line(c, ring, { closed: true, w: 2.2, col: ink(0.55) });
  for (let k = 0; k < 8; k++) { const t = k / 8 * Math.PI * 2, rr = k % 2 ? 0.1 : 0.2; seg(c, tp(0, 0), tp(Math.cos(t) * rr, Math.sin(t) * rr), { w: 1.6, col: ink(0.5) }); }
  seg(c, tp(-0.57, 0.0), tp(0.57, 0.0), { w: 2.4, col: rgba([150, 110, 60], 1) }); seg(c, tp(0.57, 0.0), iso.P(0.57, 0.0, 0.08), { w: 2.4, col: rgba([150, 110, 60], 1) });
  callout(c, 'a cake of pressed sea salt', qx + 470, 380, tp(0.2, -0.1), { size: 21, sub: 'stamped with the knotted ring' });
  callout(c, 'waxed paper and twine', qx + 470, 600, iso.P(0.56, 0.2, 0.14), { size: 21 });
  // The purse as the UI would show it (counts, not a satchel item).
  const ux = qx + 30, uy = 690;
  fill(c, rectPts(ux, uy, 250, 64), 'rgba(70,44,26,0.92)'); line(c, rectPts(ux, uy, 250, 64), { closed: true, w: 2, col: rgba(C.brass, 1) });
  box(c, Iso(ux + 44, uy + 38, 34), { x0: -0.5, x1: 0.5, y0: -0.36, y1: 0.36, z0: 0, z1: 0.3, col: [238, 236, 226], w: 1.2 });
  txt(c, '240 salt', ux + 92, uy + 43, { size: 30, f: FL, col: 'rgb(243,230,200)' });
  txt(c, 'the purse: a number, not a satchel item', ux + 270, uy + 40, { size: 19, style: 'italic' });
  notes(c, [
    ['Why salt', 'The game is called Saltbound; sailors are “worth their salt” (the word salary comes from it).', 'The sea gives salt, but clean pressed salt is what every harbour needs: it keeps fish, it trades.', 'One unit and no change to make: easy to read, easy to price.'],
    ['Earn and spend', 'Earn: jobs at the Tally House (and, if wanted, selling finds at its weighing bench).', 'Spend: the Shipwright and the Chandlery. The main quest costs nothing.'],
    ['Price bands (to tune)', '|a job 40–150 · a piece of gear 60–400 · a boat upgrade 150–900 · the second boat about 2 500'],
    ['Alternatives', '|Knots (tallies tied in a trade cord, like the logo’s knot; clashes with the speed unit) · Bells (brass tokens)'],
  ], qx, 810, 660, { size: 20, head: 25, gap: 12 });

  // The 20 %.
  const by = 1420; txt(c, 'You walk about a fifth of her', 80, by - 22, { size: 30, f: FL });
  const bw = 1500, op = AREAS.openTotal / AREAS.total;
  const bo = rectPts(80, by, bw * op, 44), bl = rectPts(80 + bw * op, by, bw * (1 - op), 44);
  wash(c, bo, C.open, 0.7); wash(c, bl, C.locked, 0.3); hatch(c, bl, { gap: 7 }); line(c, rectPts(80, by, bw, 44), { closed: true, w: 1.8 });
  txt(c, `open at the start: ${fmt(AREAS.openTotal)} m² (${pct(AREAS.openTotal, AREAS.total)})`, 92, by + 30, { size: 21, halo: true });
  txt(c, `planned and locked: ${fmt(AREAS.total - AREAS.openTotal)} m² – space only, not designed`, 80 + bw * op + 14, by + 30, { size: 21, halo: true });
  txt(c, `of ${fmt(AREAS.total)} m² of floor, sea dock included`, 80, by + 76, { size: 19, style: 'italic' });
});

/* ======================================================================================================== 2 */
defSheet('approach', 'Arriving: the stern arch from the sea', 'Elevation from astern, everything at one scale (your boat too). A mood sketch, not a measured drawing.', 2400, 1600, (c) => {
  const s = 14.5, WL = 1440, X0 = 1200, E = (y, z) => [X0 + y * s, WL - z * s];
  const half = z => 19.6 - 0.0048 * (z - 6) * (z - 6);      // tumblehome: widest a little above the water
  // Sky and haze.
  const sky = c.createLinearGradient(0, 170, 0, WL);
  sky.addColorStop(0, 'rgba(150,176,190,0.38)'); sky.addColorStop(0.75, 'rgba(226,214,186,0.22)'); sky.addColorStop(1, 'rgba(246,226,186,0.35)');
  c.fillStyle = sky; c.fillRect(40, 40, 2320, WL - 40);
  const r = rng(11);
  for (let i = 0; i < 14; i++) { const x = 80 + r() * 2200, y = 200 + r() * 520, rx = 120 + r() * 260, ry = 30 + r() * 60; const g = c.createRadialGradient(x, y, 0, x, y, rx); g.addColorStop(0, 'rgba(255,250,238,0.35)'); g.addColorStop(1, 'rgba(255,250,238,0)'); c.save(); c.translate(x, y); c.scale(1, ry / rx); c.fillStyle = g; c.beginPath(); c.arc(0, 0, rx, 0, 7); c.fill(); c.restore(); }
  for (const [x0, w, h] of [[90, 520, 38], [500, 260, 22], [1900, 420, 30]]) { const p = [[x0, WL - 6]]; for (let i = 0; i <= 20; i++) p.push([x0 + w * i / 20, WL - 6 - Math.sin(i / 20 * Math.PI) * h * (0.7 + 0.3 * Math.sin(i * 1.3))]); p.push([x0 + w, WL - 6]); fill(c, p, 'rgba(110,124,130,0.22)'); }
  // Masts, far to near (the haze makes the far ones paler).
  const haze = (col, a) => rgba(mix(col, [200, 205, 205], 1 - a));
  const mastStern = (m, a, wk) => {
    const H = HUB.Z.main + m.h, base = E(0, HUB.Z.main), top = E(0, H), w0 = 2.1 * s * wk, w1 = 0.7 * s * wk;
    for (const [f, sp] of [[0.38, 7], [0.68, 3.2]]) { const z = HUB.Z.main + m.h * f; for (const sg of [-1, 1]) for (const k of [0, 1, 2]) { const lo = f < 0.5 ? E(sg * (half(18.4) - k * 2.2), HUB.Z.main + 1.2) : E(sg * 7 * (1 - k * 0.15), HUB.Z.main + m.h * 0.38); seg(c, E(sg * sp, z), lo, { w: 1, col: ink(a * 0.55), amp: 0.2, seed: k * 3 + f + sg }); } }
    const mp = [[base[0] - w0 / 2, base[1]], [top[0] - w1 / 2, top[1]], [top[0] + w1 / 2, top[1]], [base[0] + w0 / 2, base[1]]];
    fill(c, mp, haze(C.timberDark, a)); line(c, mp, { w: 1.4, col: ink(a), closed: true });
    for (const [f, sp] of [[0.38, 7], [0.68, 3.2]]) { const z = HUB.Z.main + m.h * f; fill(c, rectPts(E(-sp, z)[0], E(0, z)[1] - 6, sp * 2 * s, 9), haze(C.timberDark, a)); }
    for (const [f, hl] of [[0.27, 0.42], [0.47, 0.35], [0.62, 0.26], [0.8, 0.18]]) {
      const z = HUB.Z.main + m.h * f, hw = HUB.B * hl * wk, a1 = E(-hw, z), b1 = E(hw, z), t = 1.0 * s * wk;
      furled(c, a1[0], b1[0], a1[1] - t * 0.3, t * 0.9, mix([228, 216, 184], [215, 218, 214], 1 - a), m.x + f * 100);
      seg(c, a1, b1, { w: t * 0.55, col: haze(C.timberDark, a), cap: 'round', amp: 0.3 });
      for (const sg of [-1, 1]) seg(c, E(sg * hw * 0.98, z), E(sg * hw * 0.5, z - m.h * 0.06), { w: 0.8, col: ink(a * 0.5) });
    }
    return top;
  };
  mastStern(HUB.masts[2], 0.42, 0.92);
  const mtop = mastStern(HUB.masts[1], 0.58, 1);
  mastStern(HUB.masts[0], 0.74, 0.8);
  const pen = []; for (let i = 0; i <= 20; i++) { const t = i / 20; pen.push([mtop[0] + t * 260, mtop[1] + 8 + Math.sin(t * 6) * 10 * t + t * 30]); }
  const pen2 = pen.map(([x, y], i) => [x, y + 16 * (1 - i / 20)]);
  fill(c, pen.concat(pen2.reverse()), rgba(C.oxblood, 0.8)); line(c, pen, { w: 1.2 });

  // The stern face. Outline: the sides with their tumblehome, the taffrail rising to a low crest in the middle.
  const sideP = sg => { const p = []; for (let i = 0; i <= 24; i++) { const z = -1 + 35.4 * i / 24; p.push(E(sg * half(z), z)); } return p; };
  const taff = []; for (let i = 0; i <= 24; i++) { const t = i / 24 * 2 - 1; taff.push(E(-t * half(34.4), 34.4 + 1.0 * (1 - t * t))); }
  const outer = sideP(-1).concat(taff.slice(1, -1).reverse().map(p => p)).concat(sideP(1).reverse());
  // The great wheel first: it shows above the taffrail.
  const wc = E(0, 36.4), wr = HUB.helm.r * s;
  seg(c, E(0, 33.5), wc, { w: 6, col: rgba(C.timberDark) });
  for (let k = 0; k < 10; k++) { const t = k / 10 * Math.PI * 2; seg(c, wc, [wc[0] + Math.cos(t) * wr * 1.22, wc[1] + Math.sin(t) * wr * 1.22], { w: 3, col: rgba(C.timberDark) }); }
  line(c, ellipsePts(wc[0], wc[1], wr, wr, 40), { closed: true, w: 6, col: rgba(C.timberDark) });
  line(c, ellipsePts(wc[0], wc[1], wr * 0.25, wr * 0.25, 20), { closed: true, w: 4, col: rgba(C.brass) });
  seg(c, [wc[0] - wr * 0.8, wc[1] - wr * 0.8], [wc[0] + wr * 0.8, wc[1] + wr * 0.8], { w: 3.4, col: rgba([214, 190, 140]) });
  seg(c, [wc[0] - wr * 0.8, wc[1] + wr * 0.8], [wc[0] + wr * 0.8, wc[1] - wr * 0.8], { w: 3.4, col: rgba([214, 190, 140]) });
  fill(c, outer, rgba(C.timberDark, 0.94));
  c.save(); pathOf(c, outer); c.clip();
  for (let z = 0.5; z < 36; z += 0.9) seg(c, E(-21, z), E(21, z), { w: 0.6, col: 'rgba(20,10,4,0.32)', amp: 0.4, seed: z * 11 });
  for (const [z, k] of [[5.5, 0.45], [9.5, 0.25], [13.5, 0.25], [17.5, 0.5], [21.5, 0.25], [25.5, 0.25], [29.5, 0.25], [33.5, 0.5]]) fill(c, [E(-21, z + k), E(21, z + k), E(21, z - k), E(-21, z - k)], rgba(C.ochre, 0.85));
  fill(c, [E(-21, -1.5), E(21, -1.5), E(21, 3), E(-21, 3)], rgba(C.ancient, 0.97));
  for (const z of [0.6, 2.2]) seg(c, E(-21, z), E(21, z), { w: 2, col: rgba(C.glow, 0.7), amp: 0.2 });
  c.restore();
  // Taffrail: a carved, gilded cap with the crest.
  const taffIn = taff.map(([x, y]) => [x, y + 0.9 * s]);
  fill(c, taff.concat(taffIn.reverse()), rgba(C.brass, 0.8)); line(c, taff, { w: 1.6 });
  for (let i = 1; i < 12; i++) { const p = taff[i * 2]; line(c, ellipsePts(p[0], p[1] + 7, 5, 5, 10, 0, Math.PI * 1.5), { w: 1, col: ink(0.55) }); }
  // The dock opening: wide at the quays, the wings' overhang above, an arch over the top.
  const open = [E(-14, -1.5), E(-14, 5.5), E(-10, 5.5), E(-10, 21.5)];
  for (let i = 1; i < 16; i++) { const t = i / 16 * Math.PI; open.push(E(-10 * Math.cos(t), 21.5 + 4 * Math.sin(t))); }
  open.push(E(10, 21.5), E(10, 5.5), E(14, 5.5), E(14, -1.5));
  c.save(); pathOf(c, open); c.clip();
  // Through it: sky over the canyon, the mizzen, the dock head, the Slip lit at the far end, quays and water.
  c.fillStyle = 'rgba(214,214,198,1)'; c.fillRect(E(-14, 0)[0], E(0, 26)[1], 28 * s, 8 * s);
  const mz = [E(-0.9, 18), E(-0.6, 27), E(0.6, 27), E(0.9, 18)]; fill(c, mz, ink(0.5));
  fill(c, [E(-10, 18.4), E(10, 18.4), E(10, -1.5), E(-10, -1.5)], rgba(mul(C.timberDark, 1.15), 1));
  for (const yy of [-6, -2, 2, 6]) { const p = rectPts(E(yy - 0.7, 16.6)[0], E(0, 16.6)[1], 1.4 * s, 2 * s); fill(c, p, rgba(C.lamp, 0.55)); line(c, p, { closed: true, w: 1, col: ink(0.6) }); }
  fill(c, rectPts(E(-10, 18.4)[0], E(0, 18.4)[1], 20 * s, 0.5 * s), rgba(C.ochre, 0.8));
  for (let yy = -9.5; yy <= 9.6; yy += 1.2) seg(c, E(yy, 17.6), E(yy, 18.4), { w: 1.2, col: ink(0.75) });
  figure(c, ...E(3.2, 17.6), s, { w: 1.5 }); figure(c, ...E(-5.5, 17.6), s, { w: 1.5 });
  const hall = c.createLinearGradient(0, E(0, 13.5)[1], 0, E(0, 1.5)[1]); hall.addColorStop(0, 'rgba(255,196,110,0.8)'); hall.addColorStop(1, 'rgba(255,218,150,0.98)');
  c.fillStyle = hall; c.fillRect(E(-9, 0)[0], E(0, 13.5)[1], 18 * s, 12 * s);
  for (const sg of [-1, 1]) { const g = c.createLinearGradient(E(sg * 10, 0)[0], 0, E(sg * 8, 0)[0], 0); g.addColorStop(0, 'rgba(30,16,6,0.55)'); g.addColorStop(1, 'rgba(30,16,6,0)'); c.fillStyle = g; c.fillRect(Math.min(E(sg * 10, 0)[0], E(sg * 8, 0)[0]), E(0, 25)[1], 2 * s, 26 * s); }
  // The cutter on the slip, bow on, in its cradle.
  const cut = [E(-2.2, 6.2), E(2.2, 6.2), E(2.1, 4.7), E(1.6, 3.3), E(0.8, 2.5), E(0.22, 2.2), E(0.18, 1.5), E(-0.18, 1.5), E(-0.22, 2.2), E(-0.8, 2.5), E(-1.6, 3.3), E(-2.1, 4.7)];
  fill(c, cut, rgba(C.oxblood, 0.95)); line(c, cut, { closed: true, w: 1.6 });
  fill(c, [E(-2.25, 6.2), E(2.25, 6.2), E(2.25, 6.7), E(-2.25, 6.7)], rgba(C.cream, 1));
  seg(c, E(0, 6.7), E(0, 7.4), { w: 3, col: rgba(C.timberDark) });
  for (const sg of [-1, 1]) { fill(c, [E(sg * 2.3, 1.5), E(sg * 2.75, 1.5), E(sg * 2.6, 4.2), E(sg * 2.1, 4.2)], rgba(C.timber)); line(c, [E(sg * 2.3, 1.5), E(sg * 2.75, 1.5), E(sg * 2.6, 4.2), E(sg * 2.1, 4.2)], { closed: true, w: 1 }); }
  fill(c, rectPts(E(-0.6, 1.5)[0], E(0, 1.5)[1] - 0.1 * s, 1.2 * s, 0.1 * s), ink(0.8));
  for (const yy of [-7, 0, 7]) lantern(c, { s, P: () => E(yy, 10.5) }, 0, 0, 0, 0, { r: 0.35 });
  for (const yy of [-7, 0, 7]) seg(c, E(yy, 13.5), E(yy, 10.9), { w: 1, col: ink(0.7) });
  fill(c, rectPts(E(-3.4, 13.2)[0], E(0, 13.2)[1], 6.8 * s, 1.3 * s), rgba(C.cream, 1)); line(c, rectPts(E(-3.4, 13.2)[0], E(0, 13.2)[1], 6.8 * s, 1.3 * s), { closed: true, w: 1.4 });
  txt(c, 'SHIPWRIGHT', E(0, 12.3)[0], E(0, 12.3)[1], { size: 15, f: FSC, align: 'center', spacing: 1 });
  fill(c, [E(-14, 0.05), E(14, 0.05), E(14, -1.5), E(-14, -1.5)], rgba(C.water, 0.95));
  for (let i = 0; i < 40; i++) { const yy = -8 + r() * 16, l = 0.3 + r() * 0.9, z = -0.1 - r() * 1.2; seg(c, E(yy, z), E(yy + l, z), { w: 2, col: rgba(C.lamp, 0.55) }); }
  for (const sg of [-1, 1]) {
    fill(c, [E(sg * 10, 1.5), E(sg * 14, 1.5), E(sg * 14, 0), E(sg * 10, 0)], rgba(C.timberLight, 0.95));
    fill(c, [E(sg * 10, 5.5), E(sg * 14, 5.5), E(sg * 14, 1.5), E(sg * 10, 1.5)], rgba(mul(C.timberDark, 0.7), 1));
    for (const yy of [11.2, 13]) { const p = rectPts(E(sg * yy - 0.6, 4.4)[0], E(0, 4.4)[1], 1.2 * s, 2.6 * s); fill(c, p, rgba(C.timber, 0.95)); line(c, p, { closed: true, w: 1 }); }
    fill(c, [E(sg * 10, 5.5), E(sg * 10.5, 5.5), E(sg * 10.5, 1.5), E(sg * 10, 1.5)], rgba(C.timber, 1));
    lantern(c, { s, P: () => E(sg * 12.1, 4.2) }, 0, 0, 0, 0, { r: 0.3 });
  }
  figure(c, ...E(-12.1, 1.5), s, { w: 1.5 });
  c.restore();
  line(c, open, { w: 2.4, seed: 4 });
  // Windows: small lidded ports below the main deck, tall framed windows above; most shut, a few lit.
  for (const sg of [-1, 1]) {
    for (const z of [7, 11, 15]) for (const yy of [12.5, 16]) { const p = rectPts(E(sg * yy - 0.5, 0)[0], E(0, z + 1)[1], 1 * s, 1 * s); fill(c, p, rgba(mul(C.timber, 0.8))); line(c, p, { closed: true, w: 1.1, col: rgba(C.ochre, 0.9) }); seg(c, p[0], p[1], { w: 2.4, col: rgba(C.iron) }); }
    for (const z of [18.6, 22.6, 26.6, 30.6]) for (const yy of [12.4, 14.6]) {
      if (Math.abs(sg * yy) > half(z + 1) - 0.9) continue;
      const lit = r() < 0.3, x0 = E(sg * yy - 0.65, 0)[0], y0 = E(0, z + 2)[1], w = 1.3 * s, h = 2 * s, p = rectPts(x0, y0, w, h);
      fill(c, p, lit ? rgba(C.lamp, 0.85) : rgba(C.timber, 1)); line(c, p, { closed: true, w: 1.2, col: rgba(C.cream, 0.9) });
      if (!lit) { seg(c, [x0 + w / 2, y0], [x0 + w / 2, y0 + h], { w: 1, col: ink(0.7) }); seg(c, [x0, y0 + h], [x0 + w / 2, y0], { w: 0.8, col: ink(0.5) }); }
    }
    // Quarter gallery: a bay of windows standing out from the side, a drop below, a little dome on top.
    const qg = z => sg * (half(z) + 2.4), qi = z => sg * (half(z) - 0.6);
    const g = [E(qi(21.5), 21.5), E(qg(21.5), 21.5), E(qg(32), 32), E(qi(32), 32)];
    fill(c, g, rgba(mul(C.timberDark, 1.15))); line(c, g, { closed: true, w: 1.8 });
    for (const z of [22.4, 27.2]) { const p = rectPts(Math.min(E(qg(z) - sg * 0.6, 0)[0], E(qg(z) - sg * 2.2, 0)[0]), E(0, z + 3.6)[1], 1.6 * s, 3.2 * s); fill(c, p, rgba(C.lamp, r() < 0.5 ? 0.8 : 0.45)); line(c, p, { closed: true, w: 1.2, col: rgba(C.cream, 1) }); }
    const drop = [E(qi(21.5), 21.5), E(qg(21.5), 21.5), E(sg * (half(19) + 0.9), 18.6), E(sg * (half(19) + 0.5), 17.8)]; fill(c, drop, rgba(C.brass, 0.85)); line(c, drop, { closed: true, w: 1.4 });
    const dome = []; for (let i = 0; i <= 12; i++) { const t = i / 12 * Math.PI; dome.push(E((qi(32) + qg(32)) / 2 - Math.cos(t) * Math.abs(qg(32) - qi(32)) / 2 * sg, 32 + Math.sin(t) * 1.6)); }
    fill(c, dome, rgba(C.brass, 0.85)); line(c, dome, { w: 1.4 });
    const lp = E(sg * (half(34.4) - 0.6), 37.6); seg(c, E(sg * (half(34.4) - 0.6), 34.4), lp, { w: 3, col: rgba(C.iron) }); lantern(c, { s, P: () => lp }, 0, 0, 0, 0, { r: 0.6 });
  }
  // The Stern Arch over the opening: the great windows and the frieze with the name.
  for (let i = 0; i < 7; i++) { const yy = -8.4 + i * 2.8, zb = 26.2 + 0.25 * Math.cos(yy / 10 * Math.PI / 2), p = [E(yy - 0.9, zb), E(yy + 0.9, zb), E(yy + 0.9, 30.8), E(yy, 31.5), E(yy - 0.9, 30.8)]; fill(c, p, rgba(C.lamp, i % 3 === 1 ? 0.9 : 0.55)); line(c, p, { closed: true, w: 1.4, col: rgba(C.cream, 1) }); seg(c, E(yy, zb), E(yy, 31.4), { w: 1, col: ink(0.6) }); }
  fill(c, [E(-10, 32), E(10, 32), E(10, 33.3), E(-10, 33.3)], rgba(C.brass, 0.8)); line(c, [E(-10, 32), E(10, 32), E(10, 33.3), E(-10, 33.3)], { closed: true, w: 1.2 });
  txt(c, 'HOLLOWKEEL', E(0, 32.25)[0], E(0, 32.25)[1], { size: 17, f: FSC, align: 'center', col: ink(0.85), spacing: 3 });
  line(c, outer, { closed: true, w: 2.6, seed: 2 });
  // The sea, with reflections.
  c.fillStyle = 'rgba(84,140,164,0.30)'; c.fillRect(40, WL, 2320, 1560 - WL);
  for (let i = 0; i < 260; i++) { const x = 60 + r() * 2280, y = WL + 8 + Math.pow(r(), 1.4) * 100, l = 10 + r() * 34 * (1 + (y - WL) / 60); seg(c, [x, y], [x + l, y], { w: 1, col: ink(0.18 + r() * 0.12), amp: 0.5 }); }
  for (let i = 0; i < 140; i++) { const yy = -19 + r() * 38, z = -0.4 - r() * 7; seg(c, E(yy, z), E(yy + 0.4 + r() * 1.2, z), { w: 2.2, col: Math.abs(yy) < 10 && r() < 0.5 ? rgba(C.lamp, 0.5) : 'rgba(40,24,12,0.35)' }); }
  // Your boat, from astern, about to sail in.
  const by = -4.2, bz = 0;
  const hullB = [E(by - 1.7, bz + 1.05), E(by + 1.7, bz + 1.05), E(by + 1.3, bz + 0.1), E(by + 0.15, bz - 0.5), E(by - 0.15, bz - 0.5), E(by - 1.3, bz + 0.1)];
  fill(c, hullB, rgba(C.oxblood)); line(c, hullB, { closed: true, w: 1.6 });
  fill(c, [E(by - 1.72, bz + 1.05), E(by + 1.72, bz + 1.05), E(by + 1.72, bz + 0.85), E(by - 1.72, bz + 0.85)], rgba(C.cream));
  seg(c, E(by, bz + 1.05), E(by, bz + 11.5), { w: 2.4, col: rgba(C.timberDark) });
  const sail = [E(by + 0.1, bz + 3), E(by + 0.1, bz + 10.4), E(by + 1.6, bz + 9), E(by + 1.2, bz + 3)];
  fill(c, sail, rgba([236, 226, 198], 0.95)); line(c, sail, { closed: true, w: 1.2 });
  fill(c, [E(by, bz + 11.5), E(by + 1.3, bz + 11.3), E(by, bz + 11.1)], rgba(C.oxblood));
  for (let i = 0; i < 14; i++) seg(c, E(by - 2 + r() * 4, bz - 0.2 - r() * 1.4), E(by - 1 + r() * 4, bz - 0.3 - r() * 1.4), { w: 1.6, col: 'rgba(255,255,250,0.7)' });
  for (let i = 0; i < 7; i++) { const x = 700 + r() * 1100, y = 260 + r() * 500, w = 9 + r() * 8; line(c, [[x - w, y - 3], [x - w * 0.4, y - 7], [x, y], [x + w * 0.4, y - 7], [x + w, y - 3]], { w: 1.6, col: ink(0.75) }); }

  // Callouts: left column right-aligned, right column left-aligned, short leaders.
  const L = 860, Rx = 1580;
  callout(c, 'Masts 54 to 76 m above the main deck', L, 330, E(-13.6, 53.7), { size: 22, align: 'right', sub: 'sails always furled: she is never seen sailing' });
  callout(c, 'The Stern Arch: the great cabin (locked)', L, 860, E(-7.5, 29), { size: 22, align: 'right', sub: 'its windows glow at night' });
  callout(c, 'Quarter galleries (locked)', L, 980, E(-21.2, 26), { size: 22, align: 'right' });
  callout(c, 'Wing quarters, shuttered (locked)', L, 1110, E(-16.6, 12.5), { size: 22, align: 'right' });
  callout(c, 'Arcaded quays with lanterns', L, 1260, E(-12.6, 4.6), { size: 22, align: 'right' });
  callout(c, 'Ancient hull at the waterline', L, 1380, E(-18.8, 1.4), { size: 22, align: 'right', sub: 'pale, seamless, faint turquoise lines' });
  callout(c, 'The great wheel: lashed, it never turns', Rx, 820, [wc[0] + wr * 1.25, wc[1]], { size: 22 });
  callout(c, 'The Stern Dock: sail in under the arch', Rx, 1090, E(7.5, 9), { size: 22, sub: '21 to 25 m clear above the water; calm inside' });
  callout(c, 'The Slip at the far end, lit (shipwright)', Rx, 1210, E(4, 4.5), { size: 22 });
  callout(c, 'Your boat: 10 m, mast 10.5 m', Rx, 1330, E(by + 0.4, 8), { size: 22 });
  scaleBar(c, 110, 1500, s, 20, 5);
});

/* ======================================================================================================== 3 */
defSheet('section', 'Long section on the centreline', 'Looking to port, bow to the right. Inside cut open; port-side parts beyond the cut are drawn behind.', 2400, 1600, (c) => {
  const s = 10.4, ox = 300, oy = 1190, P = (x, z) => [ox + x * s, oy - z * s];
  const prof = HUB.profile().map(([x, z]) => P(x, z));
  // Sea.
  c.fillStyle = 'rgba(84,140,164,0.18)'; c.fillRect(40, oy, 2320, (HUB.keel - 3) * -s);
  // Hull body: timber above +3, ancient below.
  c.save(); pathOf(c, prof); c.clip();
  fill(c, prof, rgba(C.timber, 0.55));
  fill(c, [P(-5, 3), P(170, 3), P(170, -14), P(-5, -14)], rgba(C.ancient, 1));
  for (const z of [-2.5, -6.5, -10]) line(c, [P(8, z), P(140, z)], { w: 1.6, col: rgba(C.glow, 0.6) });
  c.restore();
  // Zones on the cut (x0, x1, z0, z1, status, name, badge).
  const Z = HUB.Z, box2 = (x0, x1, z0, z1) => [P(x0, z1), P(x1, z1), P(x1, z0), P(x0, z0)];
  const zones = [
    [46, 150, Z.deep, 1.1, 'locked', 'The Deep (ancient)'],
    [64, 72, Z.quay, Z.lower, 'locked', ''],
    [96, 157, Z.quay, Z.lower, 'locked', 'The Holds'],
    [64, 73, Z.lower, Z.middle, 'locked', ''], [95, 158.5, Z.lower, Z.middle, 'locked', 'Mess and galley'],
    [64, 73, Z.middle, Z.upper, 'locked', ''], [95, 159.5, Z.middle, Z.upper, 'locked', 'Lantern Market'],
    [44, 73, Z.upper, Z.main, 'locked', 'Archive'], [95, 160, Z.upper, Z.main, 'locked', 'Inn and bunkrooms'],
    [134.6, 161, Z.main, Z.fcTop, 'locked', 'Forecastle'],
    [44, 64, Z.quay, Z.upper, 'open', ''], [72, 96, Z.quay, Z.lower, 'open', ''],
    [73, 95, Z.lower, Z.main, 'open', ''],
  ];
  for (const [i, [x0, x1, z0, z1, st, name]] of zones.entries()) {
    c.save(); pathOf(c, prof); c.clip(); fill(c, box2(x0, x1, z0, z1), PAPER); zoneFill(c, box2(x0, x1, z0, z1), st, i + 3); c.restore();
    if (name) zoneName(c, name, P((x0 + x1) / 2, 0)[0], P(0, (z0 + z1) / 2)[1] + 7, st, 20);
  }
  // Locked sterncastle wing beyond the basin, the arch on the cut.
  fill(c, box2(0, 44, Z.lower, Z.poop), rgba(C.timber, 0.35)); zoneFill(c, box2(0, 44, Z.lower, Z.poop), 'locked', 31);
  zoneName(c, 'Sterncastle (port wing beyond)', P(27, 0)[0], P(0, 27.5)[1], 'locked', 20);
  zoneName(c, 'Wing quarters', P(22, 0)[0], P(0, 11.8)[1], 'locked', 20);
  fill(c, box2(0, 10, Z.sc2, Z.poop), rgba(C.timberDark, 0.5)); zoneFill(c, box2(0, 10, Z.sc2, Z.poop), 'locked', 32);
  zoneName(c, 'Arch', P(5, 0)[0], P(0, 29.8)[1], 'locked', 18);
  for (let x = 2; x < 44; x += 4) { fill(c, box2(x, x + 0.6, Z.quay, Z.lower), rgba(C.timber, 1)); }
  fill(c, box2(0, 44, Z.quay, Z.lower), 'rgba(0,0,0,0)');
  txt(c, 'arcade, quay stores beyond (shuttered)', P(22, 0)[0], P(0, 3.6)[1], { size: 16, style: 'italic', align: 'center', halo: true });
  // Basin water, its floor and the slipway.
  fill(c, [P(0, 0), P(44, 0), P(44, -0.0), P(HUB.ramp.x0, HUB.ramp.z0), P(HUB.ramp.x0, HUB.basin.floor), P(0, HUB.basin.floor)], rgba(C.water, 0.55));
  line(c, [P(0, HUB.basin.floor), P(HUB.ramp.x0, HUB.basin.floor), P(HUB.ramp.x0, HUB.ramp.z0), P(HUB.ramp.x1, HUB.ramp.z1)], { w: 2.2 });
  txt(c, 'slipway', P(39, 0)[0], P(0, -2.4)[1], { size: 17, style: 'italic', rot: -0.31, align: 'center' });
  // Floors.
  const floor = (x0, x1, z, k = 0.45) => { const p = box2(x0, x1, z - k, z); fill(c, p, rgba(C.timberDark, 0.9)); };
  floor(44, 157.5, Z.quay); floor(64, 78, Z.lower); floor(90, 158.6, Z.lower); floor(64, 78, Z.middle); floor(90, 159.4, Z.middle);
  floor(44, 78, Z.upper); floor(90, 160, Z.upper); floor(44.6, 78, Z.main); floor(90, 134.6, Z.main); floor(134.6, 161, Z.fcTop);
  for (const z of [Z.sc1, Z.sc2, Z.sc3]) line(c, [P(0, z), P(44, z)], { w: 1.6, dash: [8, 6], col: ink(0.6) });
  floor(0, 44, Z.poop); floor(0, 10, Z.sc2); for (const z of [Z.lower, Z.middle, Z.upper, Z.main]) line(c, [P(0, z), P(44, z)], { w: 1.6, dash: [8, 6], col: ink(0.6) });
  // Gallery floors at the Well's sides (the ring), rails.
  for (const z of [Z.lower, Z.middle, Z.upper]) { floor(73, 78, z); floor(90, 95, z); seg(c, P(78, z), P(78, z + 1), { w: 2 }); seg(c, P(90, z), P(90, z + 1), { w: 2 }); seg(c, P(77.6, z + 1), P(78, z + 1), { w: 2 }); }
  // The Keelgate: the ancient ring in the floor and its light.
  fill(c, box2(80, 88, Z.quay - 0.6, Z.quay + 0.05), rgba(C.ancient)); line(c, box2(80, 88, Z.quay - 0.6, Z.quay + 0.05), { closed: true, w: 1.6 });
  seg(c, P(81, Z.quay - 0.3), P(87, Z.quay - 0.3), { w: 2.4, col: rgba(C.glow, 0.9) });
  const lg = c.createLinearGradient(0, P(0, Z.main)[1], 0, P(0, Z.quay)[1]); lg.addColorStop(0, 'rgba(255,248,226,0.0)'); lg.addColorStop(1, 'rgba(255,248,226,0.55)');
  c.fillStyle = lg; c.beginPath(); c.moveTo(...P(78.5, Z.main)); c.lineTo(...P(89.5, Z.main)); c.lineTo(...P(88, Z.quay)); c.lineTo(...P(80, Z.quay)); c.fill();
  // Hoist (beside the cut, starboard: dashed), the gantry over the Well.
  line(c, box2(78.2, 80.8, Z.lower, Z.lower + 2.4), { closed: true, w: 1.6, dash: [5, 4] });
  seg(c, P(79.5, Z.lower + 2.4), P(79.5, Z.main + 7), { w: 1.2, dash: [5, 4] });
  line(c, [P(76.5, Z.main), P(84, Z.main + 7.5), P(91.5, Z.main)], { w: 3.2, col: rgba(C.timberDark) }); seg(c, P(79.5, Z.main + 6.9), P(84, Z.main + 7.5), { w: 3.2, col: rgba(C.timberDark) });
  // Stair flights (schematic; the starboard ones are in front of the cut).
  const flight = (x0, x1, z0, z1, solid) => { const n = 10; const p = []; for (let i = 0; i <= n; i++) { const x = x0 + (x1 - x0) * i / n, z = z0 + (z1 - z0) * i / n; p.push(P(x, z)); if (i < n) p.push(P(x + (x1 - x0) / n, z)); } line(c, p, { w: 1.6, dash: solid ? null : [5, 4], col: ink(solid ? 0.9 : 0.6) }); };
  flight(86, 79, Z.quay, Z.lower, false); flight(79, 86, Z.middle, Z.upper, true); flight(92.5, 94.5, Z.lower, Z.middle, true); flight(73.5, 75.5, Z.upper, Z.main, true);
  // The hall's display boat (14 m cutter, unrigged) and its mast on trestles.
  const bx0 = 47, bz = Z.quay + 0.8, bp = [];
  for (let i = 0; i <= 30; i++) { const t = i / 30, x = bx0 + 14 * t; bp.push(P(x, bz + 3.4 + 0.5 * Math.pow(t * 2 - 1, 2))); }
  for (let i = 30; i >= 0; i--) { const t = i / 30, x = bx0 + 14 * t, u = t * 2 - 1; bp.push(P(x, bz + 3.4 * (1 - Math.pow(Math.max(0, 1 - Math.pow(Math.abs(u), 2)), 0.6)) * 0.9 + (u > 0.85 ? (u - 0.85) * 9 : 0))); }
  fill(c, bp, rgba(C.oxblood, 0.95)); line(c, bp, { closed: true, w: 1.8 });
  for (const x of [50, 54, 58]) fill(c, box2(x - 0.4, x + 0.4, Z.quay, bz + 0.6), rgba(C.timberDark));
  line(c, [P(46, Z.quay + 0.9), P(62, Z.quay + 0.9)], { w: 5, col: rgba(C.timber) });
  for (const x of [45, 63]) lantern(c, { s, P: (a, b, z) => P(x, z) }, 0, 0, 10, Z.upper, { r: 0.3 });
  // The Chandlery beyond (port side) on the main deck.
  const ch = HUB.chandlery; const chp = [P(ch.x0, Z.main), P(ch.x0, Z.main + ch.h), P((ch.x0 + ch.x1) / 2, Z.main + ch.h + 1.2), P(ch.x1, Z.main + ch.h), P(ch.x1, Z.main)];
  wash(c, chp, C.open, 0.7); line(c, chp, { closed: true, w: 1.8 });
  fill(c, box2(100, 110, Z.main + 1.1, Z.main + 3.2), rgba(C.lamp, 0.6)); line(c, box2(100, 110, Z.main + 1.1, Z.main + 3.2), { closed: true, w: 1.2 });
  // Main deck: rail, deck life.
  line(c, [P(44.6, Z.main + HUB.rail), P(134, Z.main + HUB.rail)], { w: 1.4 });
  for (let x = 46; x < 134; x += 2) seg(c, P(x, Z.main), P(x, Z.main + HUB.rail), { w: 0.8, col: ink(0.5) });
  fill(c, box2(118, 128, Z.main + 0.4, Z.main + 2.2), rgba(C.timberLight, 0.9)); line(c, box2(118, 128, Z.main + 0.4, Z.main + 2.2), { closed: true, w: 1.2 });
  txt(c, 'ship’s boat', P(123, 0)[0], P(0, Z.main + 2.8)[1], { size: 15, style: 'italic', align: 'center', halo: true });
  fill(c, box2(119.5, 120.5, Z.main, Z.main + 4.5), rgba(C.iron, 0.9));
  for (let i = 0; i < 4; i++) { const g = c.createRadialGradient(...P(120 + i * 1.4, Z.main + 5.5 + i * 1.6), 0, ...P(120 + i * 1.4, Z.main + 5.5 + i * 1.6), 18 + i * 6); g.addColorStop(0, 'rgba(120,110,100,0.35)'); g.addColorStop(1, 'rgba(120,110,100,0)'); c.fillStyle = g; c.fillRect(...P(117 + i * 1.4, Z.main + 9 + i * 1.6), 60, 60); }
  // Masts and rig.
  const tops = {};
  for (const m of HUB.masts) {
    const base = m.x, top = Z.main + m.h;
    seg(c, P(base, Z.quay), P(base, Z.main), { w: 3, dash: [6, 5], col: ink(0.55) });
    fill(c, [P(base - 0.9, Z.main), P(base - 0.35, top), P(base + 0.35, top), P(base + 0.9, Z.main)], rgba(C.timberDark)); line(c, [P(base - 0.9, Z.main), P(base - 0.35, top), P(base + 0.35, top), P(base + 0.9, Z.main)], { closed: true, w: 1.2 });
    for (const [f, w] of [[0.38, 3.2], [0.68, 1.6]]) fill(c, box2(base - w, base + w, Z.main + m.h * f - 0.3, Z.main + m.h * f + 0.3), rgba(C.timberDark));
    for (const f of [0.27, 0.47, 0.62, 0.8]) { const p = P(base, Z.main + m.h * f); c.save(); c.fillStyle = 'rgb(226,214,182)'; c.beginPath(); c.ellipse(p[0], p[1] - 3, 9, 6, 0, 0, 7); c.fill(); c.restore(); line(c, ellipsePts(p[0], p[1] - 3, 9, 6, 16), { closed: true, w: 1 }); }
    for (const k of [0, 1, 2, 3]) seg(c, P(base, Z.main + m.h * 0.38), P(base - 4 - k * 1.5, Z.main + HUB.rail), { w: 0.9, col: ink(0.6) });
    seg(c, P(base, top - 1), P(base - 18, Z.main + HUB.rail), { w: 0.9, col: ink(0.6) });
    tops[m.id] = top; fill(c, [P(base, top + 0.3), P(base + 7, top - 0.5), P(base, top - 1.3)], rgba(C.oxblood, 0.85));
  }
  const sp0 = P(154, 24.5), sp1 = P(HUB.sprit, 31.5); line(c, [sp0, sp1], { w: 7, col: rgba(C.timberDark) });
  seg(c, P(68, tops.mizzen - 2), P(100, Z.main + 76 * 0.38), { w: 1, col: ink(0.7) });
  seg(c, P(100, tops.main - 2), P(140, Z.main + 64 * 0.68), { w: 1, col: ink(0.7) });
  seg(c, P(140, tops.fore - 2), sp1, { w: 1, col: ink(0.7) }); seg(c, P(140, Z.main + 64 * 0.68), P(176, 28.7), { w: 1, col: ink(0.7) });
  // The poop: the lashed wheel and the stern lanterns.
  const wc = P(HUB.helm.x, Z.poop + 2.2); line(c, ellipsePts(wc[0], wc[1], 3, HUB.helm.r * s, 20), { closed: true, w: 3, col: rgba(C.timberDark) }); seg(c, P(HUB.helm.x, Z.poop), wc, { w: 3 });
  for (const x of [1, 40]) { seg(c, P(x, Z.poop), P(x, Z.poop + 3), { w: 2.4, col: rgba(C.iron) }); lantern(c, { s, P: (a, b, z) => P(x, z) }, 0, 0, Z.poop + 3, Z.poop + 3, { r: 0.6 }); }
  // Your boat at the fitting berth (starboard quay, in front of the cut: drawn light, dashed).
  const fb = SPACES.dock.props.find(q => q.id === 'fitting_berth'), pb = [], b0 = fb.x0;
  for (let i = 0; i <= 24; i++) { const t = i / 24; pb.push(P(b0 + 10 * t, 0.75 + 0.3 * Math.pow(t * 2 - 1, 2))); }
  for (let i = 24; i >= 0; i--) { const t = i / 24, u = t * 2 - 1; pb.push(P(b0 + 10 * t, -0.65 * Math.pow(Math.max(0, 1 - u * u), 0.6) - (Math.abs(u) < 0.7 ? 0.8 : 0))); }
  fill(c, pb, rgba(C.oxblood, 0.45)); line(c, pb, { closed: true, w: 1.4, dash: [5, 4] });
  seg(c, P(b0 + 6.2, 0.75), P(b0 + 6.2, 11.25), { w: 2.4, dash: [5, 4], col: rgba(C.timberDark) }); line(c, [P(b0 + 6.2, 2.8), P(b0 + 1.2, 2.5)], { w: 4, col: rgba([226, 214, 182], 0.6) });
  // The rudder on the port wing's sternpost (its twin on the starboard wing), and the bow's cables and bobstay.
  const ru = HUB.rudders, rp = box2(ru.x0, ru.x1, ru.z0, ru.z1); fill(c, rp, rgba(C.timberDark)); line(c, rp, { closed: true, w: 1.4 });
  for (const cb of HUB.cables) if (cb.id === 'bow' && cb.from[1] < 0) seg(c, P(cb.from[0], cb.from[2]), P(cb.to[0], cb.to[2]), { w: 1.6, col: ink(0.8) });
  const bst = SPACES.main_deck.props.find(q => q.id === 'bobstay'); seg(c, P(bst.from[0], bst.from[2]), P(bst.to[0], bst.to[2]), { w: 1.2, col: ink(0.8) });
  // Figures for scale.
  for (const [x, z] of [[30, Z.quay], [76, Z.quay], [92.5, Z.middle], [62, Z.main], [108, Z.main]]) figure(c, ...P(x, z), s, { w: 1.4 });
  // Outline and waterline.
  line(c, prof, { closed: true, w: 2.8, seed: 5 });
  line(c, [P(-12, 0), P(194, 0)], { w: 1.4, col: rgba(C.water, 1) }); txt(c, 'DWL', P(190, 0)[0], P(0, 0)[1] - 6, { size: 17, col: rgba(C.water, 1) });
  // Service badges and open names.
  const open = [[54, 9.5, 'The Slip', 'S'], [84, 3.5, 'Keelgate', 'Q'], [84, 12.2, 'The Well', ''], [106, 23.6, 'Chandlery', 'E']];
  for (const [x, z, n, b] of open) { txt(c, n, P(x, 0)[0], P(0, z)[1], { size: 22, f: FL, align: 'center', halo: true }); if (b) badge(c, P(x, 0)[0], P(0, z)[1] + 26, b, 15); }
  txt(c, 'Stern Dock', P(22, 0)[0], P(0, -1.7)[1], { size: 22, f: FL, align: 'center', halo: true });
  txt(c, 'Tally House (starboard, in front of the cut)', P(54, 0)[0], P(0, 14.6)[1], { size: 16, style: 'italic', align: 'center', halo: true }); badge(c, P(54, 0)[0] + 186, P(0, 14.6)[1] - 6, 'J', 13);
  txt(c, 'MAIN DECK (open)', P(56, 0)[0], P(0, Z.main + 2.1)[1], { size: 15, f: FSC, align: 'center', halo: true, spacing: 1 });
  zoneName(c, 'Keel', P(68, 0)[0], P(0, 3.9)[1], 'locked', 13); zoneName(c, 'Walk', P(68, 0)[0], P(0, 2.4)[1], 'locked', 13);
  const cg = box2(116, 134, Z.main, Z.main + HUB.rail); fill(c, cg, PAPER); zoneFill(c, cg, 'locked', 61); line(c, cg, { closed: true, w: 1.2 });
  zoneName(c, 'cargo deck', P(125, 0)[0], P(0, Z.main + 4.6)[1], 'locked', 14);
  // Level marks at the left margin.
  const marks = [['Poop', Z.poop], ['Sterncastle', Z.sc2], ['Main deck', Z.main], ['Upper deck', Z.upper], ['Middle deck', Z.middle], ['Lower deck', Z.lower], ['Quay deck', Z.quay], ['The Deep', Z.deep], ['Keel', HUB.keel]];
  for (const [n, z] of marks) { const y = P(0, z)[1]; txt(c, n, 60, y + 6, { size: 19 }); txt(c, (z > 0 ? '+' : z < 0 ? '−' : '') + Math.abs(z), 282, y + 6, { size: 19, align: 'right', style: 'italic' }); seg(c, [288, y], [P(0, 0)[0] - 4, y], { w: 0.8, col: ink(0.4), dash: [2, 5] }); }
  txt(c, 'storeys at +21.5, +25.5, +29.5', 60, P(0, Z.sc2)[1] + 28, { size: 15, style: 'italic' });
  callout(c, 'Main mast 76 m', P(100, 0)[0] + 30, P(0, 90)[1], P(100.4, Z.main + 74), { size: 20 });
  callout(c, 'The lashed wheel', P(18, 0)[0], P(0, 42)[1], [wc[0] + 4, wc[1] - 10], { size: 19 });
  callout(c, 'the ancient lower hull', P(150, 0)[0], P(0, -14.3)[1] + 6, P(140, -9), { size: 19, style: 'italic' });
  callout(c, 'your boat at the fitting berth (starboard)', P(1, 0)[0], P(0, -15.2)[1], P(b0 + 3, 0.2), { size: 18, style: 'italic' });
  callout(c, 'rudder', P(-9, 0)[0], P(0, -9.5)[1], P(ru.x0, ru.z0 + 1.5), { size: 17, style: 'italic' });
  callout(c, 'anchor cables', P(176, 0)[0], P(0, 9)[1], P(165, 10.1), { size: 17, style: 'italic' });
  legend(c, 1000, 1410, ['open', 'locked', 'water', 'ancient'], { cols: 2, colW: 470 });
  scaleBar(c, 300, 1450, s, 40, 10);
});

/* ======================================================================================================== 4 */
defSheet('exploded', 'The decks, pulled apart', 'Every floor of the ship, one above the other, with what is open (warm) and what is planned and locked (grey).', 2400, 2000, (c) => {
  const s = 11.4, ox = 330, kx = 0.3, ky = 0.33;
  const rows = { main: 560, upper: 800, middle: 1040, lower: 1280, quay: 1520, deep: 1760 };
  const lv = id => LEVELS.find(l => l.id === id);
  const P = (x, y, z, row) => [ox + x * s - y * s * kx, rows[row] + y * s * ky - (z - lv(row).z) * s];
  const rect3 = (r, row, z) => [P(r.x0, r.y0, z, row), P(r.x1, r.y0, z, row), P(r.x1, r.y1, z, row), P(r.x0, r.y1, z, row)];
  const outlineAt = (row) => { const L = lv(row), k = L.width || 1; return HUB.outline(k, 70).map(([x, y]) => P(x, y, L.z, row)); };
  const clipRect = (row, r) => { const L = lv(row); const out = []; const n = 24; for (let i = 0; i <= n; i++) { const x = r.x0 + (r.x1 - r.x0) * i / n; out.push([x, Math.max(r.y0, -HUB.hb(x) * (L.width || 1))]); } for (let i = n; i >= 0; i--) { const x = r.x0 + (r.x1 - r.x0) * i / n; out.push([x, Math.min(r.y1, HUB.hb(x) * (L.width || 1))]); } return out.map(([x, y]) => P(x, y, L.z, row)); };
  const drawLevel = (row) => {
    const L = lv(row);
    let base = outlineAt(row);
    if (row === 'deep') base = clipRect('deep', { x0: 30, x1: 150, y0: -20, y1: 20 });
    // Slab edge.
    c.save(); c.translate(0, 7); fill(c, base, ink(0.25)); c.restore();
    fill(c, base, PAPER);
    for (const z of LOCKED.filter(z => z.levels[row])) for (const r of z.levels[row]) { const p = clipRect(row, r); zoneFill(c, p, 'locked', z.id.length * 7 + r.x0, 0.22); }
    if (row === 'deep') { wash(c, base, C.ancient, 0.8); hatch(c, base, { gap: 9, col: ink(0.18) }); }
    for (const o of OPEN.filter(o => o.level === row)) for (const r of o.rects) { const p = clipRect(row, r); fill(c, p, PAPER); zoneFill(c, p, 'open', r.x0 + 1, 0.6); }
    if (L.water) for (const r of L.water) { const p = rect3(r, row, L.z); fill(c, p, PAPER); zoneFill(c, p, 'water', 9, 0.55); }
    for (const v of L.voids || []) { const p = rect3(v, row, L.z); fill(c, p, PAPER); line(c, p, { closed: true, w: 1.2, dash: [5, 4] }); seg(c, p[0], p[2], { w: 0.8, col: ink(0.35) }); seg(c, p[1], p[3], { w: 0.8, col: ink(0.35) }); }
    line(c, base, { closed: true, w: 2.4, seed: rows[row] });
    txt(c, L.name, 60, rows[row] + 2, { size: 26, f: FL }); txt(c, (L.z > 0 ? '+' : '−') + Math.abs(L.z) + ' m', 60, rows[row] + 30, { size: 19, style: 'italic' });
    const lt = AREAS.levels[row]; txt(c, `${fmt(lt.open)} of ${fmt(lt.total)} m² open`, 60, rows[row] + 56, { size: 17, style: 'italic', col: ink(0.75) });
  };
  for (const row of ['deep', 'quay', 'lower', 'middle', 'upper', 'main']) drawLevel(row);
  // The Well and the hall as connectors between the rows.
  const sh = HUB.shaft;
  const conn = (x, y, a, b, o = {}) => seg(c, P(x, y, lv(a).z, a), P(x, y, lv(b).z, b), { w: 1.2, dash: [4, 5], col: ink(0.55), ...o });
  for (const [x, y] of [[sh.x0, sh.y0], [sh.x1, sh.y0], [sh.x1, sh.y1], [sh.x0, sh.y1]]) for (const [a, b] of [['quay', 'lower'], ['lower', 'middle'], ['middle', 'upper'], ['upper', 'main']]) conn(x, y, a, b);
  for (const [x, y] of [[44, 9], [64, 9], [44, -9], [64, -9]]) for (const [a, b] of [['quay', 'lower'], ['lower', 'middle']]) conn(x, y, a, b, { col: ink(0.35) });
  // The dock stair and the Keelgate disc.
  const st = HUB.stair; fill(c, rect3(st, 'quay', HUB.Z.quay), rgba(C.open, 0.9)); for (let x = st.x0; x <= st.x1; x += 1) seg(c, P(x, st.y0, HUB.Z.quay, 'quay'), P(x, st.y1, HUB.Z.quay, 'quay'), { w: 0.8 });
  conn(44, 15.5, 'quay', 'lower', { col: rgba(C.route, 0.9), w: 2.4 });
  const kg = HUB.keelgate, kd = []; for (let i = 0; i < 40; i++) { const t = i / 40 * Math.PI * 2; kd.push(P(kg.x + Math.cos(t) * kg.ring, kg.y + Math.sin(t) * kg.ring, HUB.Z.quay, 'quay')); }
  fill(c, kd, rgba(C.ancient)); line(c, kd, { closed: true, w: 1.6, col: rgba(C.glow, 1) });
  // Castles and masts on the main deck.
  const bx = (x0, x1, y0, y1, z0, z1, col, a = 1) => {
    const p = (x, y, z) => P(x, y, z, 'main');
    const top = [p(x0, y0, z1), p(x1, y0, z1), p(x1, y1, z1), p(x0, y1, z1)], front = [p(x0, y1, z0), p(x1, y1, z0), p(x1, y1, z1), p(x0, y1, z1)], right = [p(x1, y0, z0), p(x1, y1, z0), p(x1, y1, z1), p(x1, y0, z1)];
    for (const [f, k] of [[right, 0.75], [front, 0.9], [top, 1.08]]) { fill(c, f, rgba(mul(col, k), a)); hatch(c, f, { gap: 8, col: ink(0.16) }); line(c, f, { closed: true, w: 1.6 }); }
  };
  const Z = HUB.Z;
  const LK = [178, 184, 186];
  bx(0, 44, -20, -10, Z.main, Z.poop, LK); bx(0, 10, -10, 10, Z.sc2, Z.poop, LK); bx(0, 44, 10, 19.6, Z.main, Z.poop, LK);
  txt(c, 'Sterncastle and Stern Arch', P(22, 19.6, Z.poop + 3.4, 'main')[0], P(22, 19.6, Z.poop + 3.4, 'main')[1], { size: 20, f: FSC, align: 'center', col: ink(0.7), halo: true });
  const fcp = (z) => { const o = []; for (let i = 0; i <= 14; i++) { const x = 134 + 26 * i / 14; o.push([x, HUB.hb(x)]); } for (let i = 14; i >= 0; i--) { const x = 134 + 26 * i / 14; o.push([x, -HUB.hb(x)]); } return o.map(([x, y]) => P(x, y, z, 'main')); };
  const fcTop = fcp(Z.fcTop), fcBot = fcp(Z.main);
  fill(c, fcBot.slice(0, 15).concat(fcTop.slice(0, 15).reverse()), rgba(mul(LK, 0.9))); fill(c, fcTop, rgba(mul(LK, 1.08))); hatch(c, fcTop, { gap: 8, col: ink(0.16) }); line(c, fcTop, { closed: true, w: 1.6 }); line(c, fcBot.slice(0, 15), { w: 1.6 }); seg(c, fcBot[0], fcTop[0], { w: 1.6 }); seg(c, fcBot[14], fcTop[14], { w: 1.6 });
  txt(c, 'Forecastle', P(153, 8, Z.fcTop + 1, 'main')[0], P(153, 8, Z.fcTop + 1, 'main')[1], { size: 20, f: FSC, align: 'center', col: ink(0.7), halo: true });
  const ch = HUB.chandlery; bx(ch.x0, ch.x1, ch.y0, ch.y1, Z.main, Z.main + ch.h, C.open, 0.9);
  for (const m of HUB.masts) { const b = P(m.x, 0, Z.main, 'main'), t = P(m.x, 0, Z.main + 26, 'main'); fill(c, [[b[0] - 7, b[1]], [t[0] - 4, t[1]], [t[0] + 4, t[1]], [b[0] + 7, b[1]]], rgba(C.timberDark)); txt(c, `${m.name} ${m.h} m ↑`, t[0], t[1] - 10, { size: 17, align: 'center', style: 'italic', halo: true }); }
  const gl = P(84, 0, Z.main + 6, 'main'); line(c, [P(76.5, 0, Z.main, 'main'), gl, P(91.5, 0, Z.main, 'main')], { w: 3, col: rgba(C.timberDark) });
  // Names and badges.
  const nm = (n, x, y, row, size = 20, b = null, sub = null) => { const p = P(x, y, lv(row).z, row); txt(c, n, p[0], p[1] + 7, { size, f: FL, align: 'center', halo: true }); if (sub) txt(c, sub, p[0], p[1] + 28, { size: size * 0.78, style: 'italic', align: 'center', halo: true }); if (b) badge(c, p[0] - measure(c, n, size, FL) / 2 - 24, p[1], b, 14); };
  const lk = (n, x, y, row, size = 16) => { const p = P(x, y, lv(row).z, row); zoneName(c, n, p[0], p[1] + 6, 'locked', size); };
  nm('Stern Dock', 22, 0, 'quay', 22, null, 'sail in'); nm('The Slip', 54, 0, 'quay', 22, 'S', 'boat hall, shop'); nm('Keelgate', 84, -8.5, 'quay', 20, 'Q');
  nm('Tally House', 54, 14.5, 'lower', 20, 'J'); nm('lane', 68.5, 9, 'lower', 16); nm('Well', 84, -8.6, 'lower', 18); nm('Well', 84, -8.6, 'middle', 18); nm('Well', 84, -8.6, 'upper', 18);
  nm('Main deck, the Waist', 54, 8, 'main', 22); lk('Cargo deck', 125, 9, 'main', 16);
  { const p = P(106, -13.5, Z.main + ch.h + 2.4, 'main'); txt(c, 'Chandlery', p[0], p[1], { size: 20, f: FL, align: 'center', halo: true }); badge(c, p[0] - 70, p[1] - 7, 'E', 14); }
  lk('Quay stores', 22, 17, 'quay'); lk('Timber store', 54, -14.5, 'quay'); lk('Keel Walk', 68, -15, 'quay', 14); lk('The Holds', 128, 0, 'quay', 20);
  lk('Wing quarters', 22, 15, 'lower'); lk('Ropewalk', 110, -15, 'lower', 18); lk('Mess and galley', 128, 4, 'lower', 18); lk('Purser’s', 68.5, -4, 'lower', 13);
  lk('Guild rooms', 30, 15, 'middle', 18); lk('Lantern Market', 128, 0, 'middle', 20);
  lk('Wing quarters', 22, 15, 'upper'); lk('Archive', 58, -14, 'upper', 18); lk('Inn and bunkrooms', 128, 0, 'upper', 20);
  lk('The Deep: ancient, behind the Keelgate', 92, 0, 'deep', 20);
  // The route, level by level.
  const route = (pts, row) => line(c, pts.map(([x, y]) => P(x, y, lv(row).z, row)), { w: 3.4, col: rgba(C.route, 1), dash: [2, 8] });
  route([[-6, 0], [34, 7.9]], 'quay'); route([[37, 12], [46, 12], [56, 9.4]], 'quay'); route([[42, 12.6], [33, 15.5], [44, 15.5]], 'quay');
  route([[44, 15.5], [58, 15.5], [64, 10.5], [73, 9], [79, 9.5]], 'lower');
  route([[86, 9.5], [90, 8], [78, -5.4]], 'quay'); conn(82.5, 9.5, 'quay', 'lower', { col: rgba(C.route, 0.9), w: 2.4 });
  route([[79, 9.5], [93.5, 6], [93.5, -3]], 'lower'); conn(93.5, -3, 'lower', 'middle', { col: rgba(C.route, 0.9), w: 2.4 });
  route([[93.5, -3], [90, -9.5], [82.5, -9.5]], 'middle'); conn(82.5, -9.5, 'middle', 'upper', { col: rgba(C.route, 0.9), w: 2.4 });
  route([[82.5, -9.5], [74.5, -8], [74.5, 3.5]], 'upper'); conn(74.5, 3.5, 'upper', 'main', { col: rgba(C.route, 0.9), w: 2.4 });
  route([[74.5, 7], [94, 7], [100, -4.5], [105, -6.6]], 'main');
  legend(c, 640, 1858, ['open', 'locked', 'water', 'route'], { cols: 2, colW: 470, size: 18 });
  txt(c, `Open at the start: ${fmt(AREAS.openTotal)} of ${fmt(AREAS.total)} m² = ${pct(AREAS.openTotal, AREAS.total)} of her floor (counted on a 0.5 m grid, sea dock included).`, 60, 1960, { size: 20, style: 'italic' });
});

/* ======================================================================================================== 5 */
defSheet('cross', 'Cross-sections', 'A: through the Stern Dock at x 22 m, looking aft.   B: through the Well at x 84 m, looking forward.', 2400, 1600, (c) => {
  const s = 19, oy = 1180, cxA = 560, cxB = 1560, Z = HUB.Z;
  const A = (y, z) => [cxA - y * s, oy - z * s], B = (y, z) => [cxB + y * s, oy - z * s];
  const hullSec = (F) => { const top = F === A ? 34.4 : 18.4; const p = []; const half = z => z < -6 ? 6 + (z + 12) * (10 / 6) : z < 0 ? 16 + (z + 6) * 0.5 : z < 26 ? 19 + 0.6 * Math.sin(Math.min(1, z / 26) * Math.PI) : 19 - (z - 26) * 0.12; for (let z = -12; z <= top + 0.01; z += 0.5) p.push([half(Math.min(z, top)), Math.min(z, top)]); return p; };
  for (const [F, cx, label] of [[A, cxA, 'A'], [B, cxB, 'B']]) {
    c.fillStyle = 'rgba(84,140,164,0.18)'; c.fillRect(cx - 21 * s - 40, oy, 42 * s + 80, 13 * s);
    const sec = hullSec(F), outer = sec.map(([y, z]) => F(y, z)).concat(sec.map(([y, z]) => F(-y, z)).reverse());
    c.save(); pathOf(c, outer); c.clip(); fill(c, outer, rgba(C.timber, 0.5));
    fill(c, [F(-25, 3), F(25, 3), F(25, -13), F(-25, -13)], rgba(C.ancient)); for (const z of [-3, -8]) seg(c, F(-22, z), F(22, z), { w: 1.6, col: rgba(C.glow, 0.6) }); c.restore();
    txt(c, label, cx + (F === A ? 300 : -330), 300, { size: 64, f: FSC, align: 'center' });
    line(c, [F(-23, 0), F(23, 0)], { w: 1.2, col: rgba(C.water, 1) });
  }
  // ---- A: the dock, looking aft.
  {
    const F = A;
    // Sky and sea seen through the dock mouth beyond (aft), under the arch.
    fill(c, [F(-10, 25.5), F(10, 25.5), F(10, 0), F(-10, 0)], 'rgba(214,220,214,0.9)');
    seg(c, F(-10, 1.2), F(10, 1.2), { w: 1, col: ink(0.4) }); fill(c, [F(-10, 1.2), F(10, 1.2), F(10, 0), F(-10, 0)], rgba(C.water, 0.4));
    // The arch beyond.
    const ar = [F(-10, 25.5), F(-6, 26.4), F(0, 26.4), F(6, 26.4), F(10, 25.5), F(10, 33.5), F(-10, 33.5)]; fill(c, ar, rgba(C.timberDark, 0.85)); zoneFill(c, ar, 'locked', 4); line(c, ar, { closed: true, w: 1.8 });
    zoneName(c, 'Stern Arch (beyond)', F(0, 0)[0], F(0, 30)[1], 'locked', 18);
    // Basin: water, floor, side walls of ancient stone up to the quays.
    fill(c, [F(-10, 0), F(10, 0), F(10, HUB.basin.floor), F(-10, HUB.basin.floor)], rgba(C.water, 0.55));
    for (const sg of [-1, 1]) {
      fill(c, [F(sg * 10, 1.5), F(sg * 19.2, 1.5), F(sg * 19.2, -5), F(sg * 10, -5)], rgba(C.ancient));
      const q = [F(sg * 10, 1.5), F(sg * 14, 1.5), F(sg * 14, 1.0), F(sg * 10, 1.0)]; wash(c, q, C.open, 0.8);
      const arc = [F(sg * 10, 5.5), F(sg * 14, 5.5), F(sg * 14, 1.5), F(sg * 10, 1.5)]; wash(c, arc, C.open, 0.35);
      fill(c, [F(sg * 10, 5.5), F(sg * 10.5, 5.5), F(sg * 10.5, 1.5), F(sg * 10, 1.5)], rgba(C.timber));
      const qs = [F(sg * 14, 5.5), F(sg * 19.2, 5.5), F(sg * 19.2, 1.5), F(sg * 14, 1.5)]; zoneFill(c, qs, 'locked', 5 + sg);
      const wing = [F(sg * 10, Z.poop), F(sg * 18.4, Z.poop), F(sg * 19.6, 15), F(sg * 19.4, 5.5), F(sg * 10, 5.5)]; fill(c, wing, rgba(C.timber, 0.4)); zoneFill(c, wing, 'locked', 7 + sg);
      for (const z of [5.5, 9.5, 13.5, 17.5, 21.5, 25.5, 29.5, 33.5]) fill(c, [F(sg * 10, z), F(sg * 19.4, z), F(sg * 19.4, z - 0.4), F(sg * 10, z - 0.4)], rgba(C.timberDark, 0.9));
      fill(c, [F(sg * 10, 1.5), F(sg * 19.2, 1.5), F(sg * 19.2, 1.1), F(sg * 10, 1.1)], rgba(C.timberDark, 0.9));
      lantern(c, { s, P: (x, y, z) => F(sg * 12, z) }, 0, 0, 4.3, 5.1, { r: 0.25 });
    }
    zoneName(c, 'Wing quarters', F(15, 0)[0], F(0, 11.5)[1], 'locked', 17); zoneName(c, 'Sterncastle', F(15, 0)[0], F(0, 23.5)[1], 'locked', 17);
    zoneName(c, 'Wing quarters', F(-15, 0)[0], F(0, 11.5)[1], 'locked', 17); zoneName(c, 'Sterncastle', F(-15, 0)[0], F(0, 23.5)[1], 'locked', 17);
    zoneName(c, 'stores', F(16.6, 0)[0], F(0, 3.2)[1], 'locked', 13); zoneName(c, 'stores', F(-16.6, 0)[0], F(0, 3.2)[1], 'locked', 13);
    // Your boat at the starboard quay (end-on), the fitting crane.
    const bc = 7.9, bh = [F(bc - 1.7, 0.95), F(bc + 1.7, 0.95), F(bc + 1.3, 0.1), F(bc + 0.15, -0.6), F(bc + 0.12, -1.45), F(bc - 0.12, -1.45), F(bc - 0.15, -0.6), F(bc - 1.3, 0.1)];
    fill(c, bh, rgba(C.oxblood)); line(c, bh, { closed: true, w: 1.4 }); seg(c, F(bc, 0.95), F(bc, 11.45), { w: 2.4, col: rgba(C.timberDark) });
    figure(c, ...F(12, 1.5), s, { w: 1.6 }); figure(c, ...F(-11.5, 1.5), s, { w: 1.6 });
    callout(c, '25.5 m clear under the arch', 120, 210, F(-2, 25.7), { size: 19 });
    callout(c, 'the sea and the horizon through the dock mouth', 120, 262, F(-5, 14), { size: 19, style: 'italic' });
    callout(c, 'basin 20 m wide, 5 m deep', 120, 314, F(-6, -3.5), { size: 19 });
    callout(c, 'your boat at the fitting berth', 120, 366, F(bc - 0.2, 8), { size: 19, style: 'italic' });
    callout(c, 'starboard quay, under the arcade', 120, 418, F(13.2, 4.6), { size: 19 });
    txt(c, '← starboard', F(19, 0)[0], 1445, { size: 18, style: 'italic' }); txt(c, 'port →', F(-19, 0)[0], 1445, { size: 18, style: 'italic', align: 'right' });
  }
  // ---- B: the Well, looking forward.
  {
    const F = B;
    const floors = [[Z.quay, 18.6], [Z.lower, 19.5], [Z.middle, 19.6], [Z.upper, 19.6], [Z.main, 19.6]];
    // Locked rooms beyond the galleries (y 11..20) and the holds beside the chamber (y 12..19).
    for (const sg of [-1, 1]) {
      const h = [F(sg * 12, Z.lower - 0.4), F(sg * 18.6, Z.lower - 0.4), F(sg * 18.6, Z.quay), F(sg * 12, Z.quay)]; zoneFill(c, h, 'locked', 21 + sg);
      for (const [z0, z1] of [[Z.lower, Z.middle], [Z.middle, Z.upper], [Z.upper, Z.main]]) { const p = [F(sg * 11, z1 - 0.4), F(sg * 19.6, z1 - 0.4), F(sg * 19.6, z0), F(sg * 11, z0)]; zoneFill(c, p, 'locked', 30 + z0 + sg); }
    }
    zoneName(c, 'Holds', F(15.5, 0)[0], F(0, 3.6)[1], 'locked', 16); zoneName(c, 'Holds', F(-15.5, 0)[0], F(0, 3.6)[1], 'locked', 16);
    zoneName(c, 'Mess', F(15.5, 0)[0], F(0, 7.6)[1], 'locked', 16); zoneName(c, 'Ropewalk', F(-15.5, 0)[0], F(0, 7.6)[1], 'locked', 16);
    zoneName(c, 'Market', F(15.5, 0)[0], F(0, 11.6)[1], 'locked', 16); zoneName(c, 'Market', F(-15.5, 0)[0], F(0, 11.6)[1], 'locked', 16);
    zoneName(c, 'Inn', F(15.5, 0)[0], F(0, 15.6)[1], 'locked', 16); zoneName(c, 'Inn', F(-15.5, 0)[0], F(0, 15.6)[1], 'locked', 16);
    // Open: the chamber, the galleries, the shaft.
    wash(c, [F(-12, Z.lower - 0.4), F(12, Z.lower - 0.4), F(12, Z.quay), F(-12, Z.quay)], C.open, 0.5);
    wash(c, [F(-11, Z.main), F(11, Z.main), F(11, Z.lower), F(-11, Z.lower)], C.open, 0.35);
    const lg = c.createLinearGradient(0, F(0, Z.main)[1], 0, F(0, Z.quay)[1]); lg.addColorStop(0, 'rgba(255,250,232,0)'); lg.addColorStop(1, 'rgba(255,250,232,0.6)');
    c.fillStyle = lg; c.beginPath(); c.moveTo(...F(-6, Z.main)); c.lineTo(...F(6, Z.main)); c.lineTo(...F(4, Z.quay)); c.lineTo(...F(-4, Z.quay)); c.fill();
    // Floors (cut) with the shaft opening, walls with closed doors (gates).
    for (const [z, hw] of floors) {
      if (z === Z.quay) { fill(c, [F(-hw, z), F(hw, z), F(hw, z - 0.45), F(-hw, z - 0.45)], rgba(C.timberDark)); continue; }
      for (const sg of [-1, 1]) { fill(c, [F(sg * 6, z), F(sg * hw, z), F(sg * hw, z - 0.45), F(sg * 6, z - 0.45)], rgba(C.timberDark)); if (z < Z.main) { seg(c, F(sg * 6, z), F(sg * 6, z + 1.05), { w: 2 }); seg(c, F(sg * 6, z + 1.05), F(sg * 6.6, z + 1.05), { w: 2 }); } }
    }
    for (const sg of [-1, 1]) {
      seg(c, F(sg * 12, Z.quay), F(sg * 12, Z.lower - 0.45), { w: 2.4 });
      for (const z of [Z.lower, Z.middle, Z.upper]) { seg(c, F(sg * 11, z), F(sg * 11, z + 3.55), { w: 2.4 }); const d = [F(sg * 11, z), F(sg * 11, z + 2.4)]; seg(c, d[0], d[1], { w: 6, col: rgba(C.timberDark) }); seg(c, F(sg * 10.8, z + 1.2), F(sg * 11.2, z + 1.2), { w: 3, col: rgba(C.iron) }); figure(c, ...F(sg * 8.4, z), s, { w: 1.5 }); }
    }
    // Main deck rail round the shaft, the gantry, the main mast beyond.
    for (const sg of [-1, 1]) { seg(c, F(sg * 6, Z.main), F(sg * 6, Z.main + 1.0), { w: 2 }); seg(c, F(sg * 6, Z.main + 1.0), F(sg * 8, Z.main + 1.0), { w: 2 }); fill(c, [F(sg * 19.6, Z.main + HUB.rail), F(sg * 18.8, Z.main + HUB.rail), F(sg * 18.8, Z.main), F(sg * 19.6, Z.main)], rgba(C.timberDark)); }
    fill(c, [F(-0.9, Z.main), F(-0.6, 36), F(0.6, 36), F(0.9, Z.main)], rgba(C.timberDark, 0.35));
    line(c, [F(-7.5, Z.main), F(0, Z.main + 7.5), F(7.5, Z.main)], { w: 4, col: rgba(C.timberDark) }); seg(c, F(0, Z.main + 7.5), F(4.5, Z.main + 7.5), { w: 4, col: rgba(C.timberDark) });
    seg(c, F(4.5, Z.main + 7.5), F(4.5, Z.lower + 2.4), { w: 1.2 });
    const cage = [F(3.2, Z.lower + 2.4), F(5.8, Z.lower + 2.4), F(5.8, Z.lower), F(3.2, Z.lower)]; fill(c, cage, rgba(C.timberLight, 0.6)); line(c, cage, { closed: true, w: 1.6 });
    for (let y = 3.5; y < 5.8; y += 0.5) seg(c, F(y, Z.lower), F(y, Z.lower + 2.4), { w: 0.8 });
    // The Chandlery beyond (port side), on the main deck.
    const chd = HUB.chandlery, chm = (chd.y0 + chd.y1) / 2; const ch = [F(chd.y0, Z.main), F(chd.y0, Z.main + chd.h), F(chm, Z.main + 5.4), F(chd.y1, Z.main + chd.h), F(chd.y1, Z.main)]; wash(c, ch, C.open, 0.7); line(c, ch, { closed: true, w: 1.6 });
    txt(c, 'Chandlery (beyond)', F(chm, 0)[0], F(0, Z.main + 2)[1], { size: 18, f: FL, align: 'center', halo: true }); badge(c, F(chm, 0)[0], F(0, Z.main + 6.8)[1], 'E', 14);
    // The Keelgate in the floor and the Deep below.
    fill(c, [F(-4, Z.quay + 0.05), F(4, Z.quay + 0.05), F(4, Z.quay - 0.7), F(-4, Z.quay - 0.7)], rgba(C.ancient)); line(c, [F(-4, Z.quay + 0.05), F(4, Z.quay + 0.05), F(4, Z.quay - 0.7), F(-4, Z.quay - 0.7)], { closed: true, w: 1.6 });
    seg(c, F(-3, Z.quay - 0.35), F(3, Z.quay - 0.35), { w: 2.6, col: rgba(C.glow, 0.95) });
    const dp = [F(-15, Z.quay - 0.75), F(15, Z.quay - 0.75), F(13, Z.deep), F(-13, Z.deep)]; zoneFill(c, dp, 'locked', 44);
    zoneName(c, 'The Deep (ancient, sealed)', F(0, 0)[0], F(0, -3.6)[1], 'locked', 18);
    const dk = SPACES.keelgate.props.find(q => q.id === 'desk'); const desk = [F(dk.y0, Z.quay + dk.h), F(dk.y1, Z.quay + dk.h), F(dk.y1, Z.quay), F(dk.y0, Z.quay)]; fill(c, desk, rgba(C.timber)); line(c, desk, { closed: true, w: 1.2 });
    figure(c, ...F(SPACES.keelgate.npc.y, Z.quay), s, { w: 1.6 });
    const RX = 1990;
    callout(c, 'gantry over the Well', RX, 520, F(3, Z.main + 7.5), { size: 19 });
    callout(c, 'the Well: 12 × 12 m,', RX, 690, F(2, 14), { size: 19, sub: 'daylight from above' });
    callout(c, 'galleries 5 m wide,', RX, 820, F(11, Z.middle + 1.2), { size: 19, sub: 'closed doors (locked)' });
    callout(c, 'the hoist (E: up, down)', RX, 950, F(5.8, Z.lower + 1.2), { size: 19 });
    callout(c, 'the Keelgate', RX, 1110, F(2.5, Z.quay), { size: 19, sub: 'the main quest' }); badge(c, RX + measure(c, 'the Keelgate', 19) + 26, 1104, 'Q', 13);
    callout(c, 'reading desk (NPC)', 1150, 1240, F(-7.8, Z.quay + 0.9), { size: 18, style: 'italic', align: 'right' });
    txt(c, 'main mast (beyond)', F(1.4, 0)[0] + 8, F(0, 33)[1], { size: 16, style: 'italic', halo: true });
    txt(c, 'port ←', F(-19, 0)[0], 1445, { size: 18, style: 'italic' }); txt(c, '→ starboard', F(19, 0)[0], 1445, { size: 18, style: 'italic', align: 'right' });
  }
  for (const [F, cx] of [[A, cxA], [B, cxB]]) { const sec = hullSec(F), outer = sec.map(([y, z]) => F(y, z)).concat(sec.map(([y, z]) => F(-y, z)).reverse()); const u = sec.map(([y, z]) => F(-y, z)).reverse().concat(sec.map(([y, z]) => F(y, z))); line(c, F === A ? outer : u, { closed: F === A, w: 2.8 }); }
  const marks = [['main', Z.main], ['upper', Z.upper], ['middle', Z.middle], ['lower', Z.lower], ['quay', Z.quay], ['WL', 0]];
  for (const [n, z] of marks) txt(c, n + ' ' + (z ? '+' + z : '0'), 1060, oy - z * s + 6, { size: 17, align: 'center', style: 'italic', col: ink(0.75) });
  legend(c, 760, 1478, ['open', 'locked', 'water', 'ancient'], { cols: 2, colW: 470, size: 18 });
  scaleBar(c, 160, 1500, s, 20, 5);
});
