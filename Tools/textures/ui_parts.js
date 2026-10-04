/* Saltbound UI parts (D55): each part is drawn alone on a transparent canvas with the approved design's own drawing code
   (the toolkit and the named functions are pulled verbatim out of Docs/Design/UI/*.html by gen_ui_parts.py and put in
   front of this file). PARTS lists them; every part draws at 1080p pixel units into a canvas scaled by its `scale`.
   Pivots and sizes are noted per part; the widgets in Source/Saltbound/UI place them by the same numbers. */

const PARTS = [];
function part(name, w, h, scale, draw) { PARTS.push({ name, w, h, scale, draw }); }

/* ---------- shared textures the parts sample (as in the reference pages' start code) ---------- */
function setupShared() {
  PARCH = parchment(720, 320, 6);
  PARCH_BIG = parchment(1400, 260, 8);
  SPECK = tex(1400, 80, (x, y) => { const n = N3(x / 1.7, y / 1.7), m = N1(x / 12, y / 12); return [95, 62, 30, clamp(sstep(.28, .62, n) * .9 + sstep(.35, .75, m) * .45) * 255]; });
  LEATHER_BIG = leather(1400, 800, [118, 70, 40], 1);
  LEATHER_DISC = leather(200, 200, [70, 42, 24], 2, { scuff: .6 });
  LEATHER_SMALL = leather(140, 110, [128, 78, 44], 3);
  PARCH_SMALL = parchment(100, 90, 4);
  PARCH_TAG = parchment(300, 460, 5);
  WOOD = wood(1000, 700, [156, 104, 60], 1, [[220, 140, 26], [640, 420, 22], [860, 90, 18]]);
}

/* ---------- 1. shared pieces ---------- */
/* The key scrap (section 7): 58 x 58 torn parchment, seed 12, tear 4, unrotated (the widget turns it -3 deg), drawn in a
   120 x 120 frame centred on the scrap so its drop shadow fits. */
const KS_FRAME = 120;
part('T_UI_KeyScrap', KS_FRAME, KS_FRAME, 2, c => { const o = (KS_FRAME - KS) / 2; scrap(c, o, o, KS, KS, { seed: KSEED, rot: 0, tear: 4 }); });
/* The scrap's paper alone (white where the paper is): the ink-soak material keeps the ink on the paper. */
part('T_UI_KeyScrap_Paper', KS_FRAME, KS_FRAME, 2, c => { const o = (KS_FRAME - KS) / 2; c.fillStyle = '#fff'; c.fill(scrapPath(o, o, KS, KS, KSEED, 4)); });
/* Key letters inked on the scrap (IM Fell English SC 32 px, ink with bleed), same frame: baseline 11 px below the centre.
   The light letter is what shows where the ink has soaked through (#EFDFB8). */
for (const [k, label] of [['E', 'E'], ['F', 'F'], ['R', 'R'], ['G', 'G'], ['Q', 'Q'], ['PadX', 'X']]) {
  part('T_UI_KeyLetter_' + k, KS_FRAME, KS_FRAME, 2, c => inkText(c, label, KS_FRAME / 2, KS_FRAME / 2 + 11, 32, { align: 'center', sc: true }));
  part('T_UI_KeyLetterLight_' + k, KS_FRAME, KS_FRAME, 2, c => txt(c, label, KS_FRAME / 2, KS_FRAME / 2 + 11, `400 32px ${FSC}`, '#efdfb8', { align: 'center' }));
}
/* The keycap for leather screens (section 3), without its letter: a 9-slice brush, 40 x 40 in a 48 x 48 frame. */
part('T_UI_Keycap', 48, 48, 2, c => {
  const x = 4, y = 3, s = 40, p = rr(x, y, s, s, 7);
  dropShadow(c, p, 4, 'rgba(0,0,0,.6)', 0, 2);
  const g = c.createLinearGradient(0, y, 0, y + s); g.addColorStop(0, '#5b3a22'); g.addColorStop(1, '#2a170b'); c.fillStyle = g; c.fill(p);
  c.save(); c.strokeStyle = brassGrad(c, x, y, x + s, y + s); c.lineWidth = 2.4; c.stroke(p); c.restore();
});
/* The count coin (section 3) without its number: dark-leather disc r 17 with a 2.6 px brass rim, in a 48 x 48 frame. */
part('T_UI_Coin', 48, 48, 2, c => {
  const x = 23, y = 22, r = 17, p = new Path2D(); p.arc(x, y, r, 0, TAU);
  dropShadow(c, p, 5, 'rgba(0,0,0,.7)', 1, 3);
  const g = c.createRadialGradient(x - r * .3, y - r * .4, 1, x, y, r); g.addColorStop(0, '#6a4228'); g.addColorStop(1, '#2a160b'); c.fillStyle = g; c.fill(p);
  c.save(); c.lineWidth = 2.6; c.strokeStyle = brassGrad(c, x - r, y - r, x + r, y + r); c.stroke(p); c.restore();
});
/* The painted lettering's speckle (section 3), as in the reference: brown specks, 1400 x 80. */
part('T_UI_PaintSpeck', 1400, 80, 1, c => c.drawImage(SPECK, 0, 0));

/* ---------- 4. the satchel (section 5) ---------- */
const circlePts = (cx, cy, r, n = 64) => Array.from({ length: n + 1 }, (_, i) => [cx + Math.cos(i / n * TAU) * r, cy + Math.sin(i / n * TAU) * r]);
/* The bag without anything that changes: flap, body, strap and buckle, "SATCHEL" debossed, the 24 pressed dishes (8 x 3 at
   401 + 124 col, 616 + 140 row, radius 50), the front lip with its key hints, the tab, the D-ring and the twine down to the
   tag's hole (the tag swings about that hole, so the twine stays). Frame: 1080p x 200..1720, y 180..1080. */
const BAG_X0 = 200, BAG_Y0 = 180;
part('T_UI_SatchelBag', 1520, 900, 1, c => {
  c.translate(-BAG_X0, -BAG_Y0);
  const flap = rr(400, 236, 1120, 230, [150, 150, 10, 10]); dropShadow(c, flap, 26, 'rgba(0,0,0,.7)', 0, 10); leatherAt(c, flap, 400, 236, 1120, 230, 1, .62); bevel(c, flap);
  stitches(c, rrPts(424, 260, 1072, 260, 128), { step: 16, len: 9 });
  const body = rr(290, 392, 1340, 760, [50, 50, 0, 0]); dropShadow(c, body, 40, 'rgba(0,0,0,.8)', 0, -6); leatherAt(c, body, 290, 392, 1340, 760, 0, 1);
  innerShadow(c, body, 60, 'rgba(0,0,0,.75)', 0, 30); bevel(c, body, 1.4);
  stitches(c, rrPts(314, 416, 1292, 760, 30), { step: 16, len: 9 });
  const strap = rr(922, 300, 76, 236, [6, 6, 24, 24]); dropShadow(c, strap, 14, 'rgba(0,0,0,.75)', 0, 8); leatherAt(c, strap, 922, 300, 76, 236, 4, .7); bevel(c, strap, .7);
  stitches(c, [[934, 312], [934, 520]], { step: 13, len: 7, w: 2.4 }); stitches(c, [[986, 312], [986, 520]], { step: 13, len: 7, w: 2.4 });
  c.save(); const bk = new Path2D(); bk.addPath(rr(906, 470, 108, 66, 10)); bk.addPath(rr(918, 482, 84, 42, 5)); dropShadow(c, bk, 8, 'rgba(0,0,0,.8)', 2, 6, 'evenodd'); c.fillStyle = brassGrad(c, 906, 470, 1014, 536); c.fill(bk, 'evenodd'); c.strokeStyle = 'rgba(40,24,5,.8)'; c.lineWidth = 1.4; c.stroke(bk);
  c.fillStyle = brassGrad(c, 955, 470, 965, 540); c.fillRect(956, 474, 8, 58); c.restore();
  deboss(c, 'SATCHEL', 352, 490, '400 46px "IM Fell English SC",Georgia,serif', { ls: 8 });
  for (let i = 0; i < 24; i++) ringSlot(c, 401 + (i % 8) * 124, 616 + (i / 8 | 0) * 140, null, false);
  const lip = new Path2D(); lip.moveTo(262, 1004); lip.quadraticCurveTo(960, 968, 1658, 1004); lip.lineTo(1658, 1090); lip.lineTo(262, 1090); lip.closePath();
  let g = c.createLinearGradient(0, 940, 0, 995); g.addColorStop(0, 'rgba(0,0,0,0)'); g.addColorStop(1, 'rgba(0,0,0,.6)'); c.fillStyle = g; c.fillRect(290, 940, 1340, 60);
  dropShadow(c, lip, 30, 'rgba(0,0,0,.85)', 0, -10); leatherAt(c, lip, 262, 968, 1396, 122, 0, .95); bevel(c, lip, 1.2);
  c.save(); c.strokeStyle = 'rgba(255,224,176,.45)'; c.lineWidth = 3; c.beginPath(); c.moveTo(270, 1006); c.quadraticCurveTo(960, 971, 1650, 1006); c.stroke(); c.restore();
  const lp = []; for (let i = 0; i <= 60; i++) { const t = i / 60, x = 280 + t * 1360; lp.push([x, 1020 + ((1 - (2 * t - 1) ** 2) * -18)]); } stitches(c, lp, { step: 16, len: 9 });
  let x = 700; const y = 1062; x += keycap(c, 'I', x, y - 27, 36) + 12; txt(c, 'close', x, y, '400 26px Alegreya,Georgia,serif', '#f0e2c4', { shadow: ['rgba(0,0,0,.9)', 6, 0, 2] }); x += 110;
  txt(c, 'mouse or d-pad: choose a pocket', x, y, '400 26px Alegreya,Georgia,serif', '#f0e2c4', { shadow: ['rgba(0,0,0,.9)', 6, 0, 2] });
  const tab = rr(1580, 404, 46, 40, 6); leatherAt(c, tab, 1580, 404, 46, 40, 6, .75); bevel(c, tab, .6);
  c.save(); c.lineWidth = 7; c.strokeStyle = brassGrad(c, 1585, 430, 1625, 470); c.shadowBlur = 6; c.shadowColor = 'rgba(0,0,0,.8)'; c.shadowOffsetY = 3; c.beginPath(); c.moveTo(1586, 440); c.lineTo(1620, 440); c.arc(1603, 452, 17, 0, Math.PI); c.closePath(); c.stroke(); c.restore();
  const hx = 1498, hy = 540;
  c.save(); c.lineCap = 'round'; [[0, 'rgba(0,0,0,.6)', 5, 3], [0, '#c7ae80', 3.4, 0], [1, 'rgba(255,240,200,.5)', 1, -.8]].forEach(([_, col, lw, o]) => { c.strokeStyle = col; c.lineWidth = lw;
    [[-3], [3]].forEach(([d]) => { c.beginPath(); c.moveTo(1603 + d, 468 + o); c.bezierCurveTo(1600 + d, 520 + o, 1530 + d, 520 + o, hx + d * .5, hy - 6 + o); c.stroke(); }); }); c.restore();
});
/* The parchment tag (no text): pivot = its hole, frame 400 x 600 with the hole at (190, 80); the brass grommet in the hole. */
const TAG_HX = 190, TAG_HY = 80;
part('T_UI_Tag', 400, 600, 1, c => {
  c.translate(TAG_HX, TAG_HY);
  const w = 300, h = 500, top = -38, tag = new Path2D(); tag.moveTo(-w / 2 + 46, top); tag.lineTo(w / 2 - 46, top); tag.lineTo(w / 2, top + 46); tag.lineTo(w / 2, top + h - 10); tag.quadraticCurveTo(w / 2, top + h, w / 2 - 10, top + h);
  tag.lineTo(-w / 2 + 10, top + h); tag.quadraticCurveTo(-w / 2, top + h, -w / 2, top + h - 10); tag.lineTo(-w / 2, top + 46); tag.closePath();
  dropShadow(c, tag, 26, 'rgba(0,0,0,.7)', 10, 18); fillTex(c, tag, PARCH_TAG, -w / 2, top);
  innerShadow(c, tag, 30, 'rgba(120,72,28,.55)', 0, 0); innerShadow(c, tag, 4, 'rgba(90,55,20,.5)', 0, 0);
  const re = new Path2D(); re.arc(0, 0, 30, 0, TAU); c.save(); c.globalAlpha = .35; c.fillStyle = '#b8955c'; c.fill(re); c.restore();
  c.fillStyle = '#120b06'; c.beginPath(); c.arc(0, 0, 8, 0, TAU); c.fill(); brassRing(c, 0, 0, 14, 8);
});
/* The twine's strand that goes through the hole in front of the tag (frame 80 x 80, the hole at (40, 52)). */
part('T_UI_TagTwineFront', 80, 80, 2, c => {
  c.translate(40 - 1498, 52 - 540);
  c.save(); c.lineCap = 'round'; c.strokeStyle = '#c7ae80'; c.lineWidth = 3.4; c.shadowBlur = 3; c.shadowColor = 'rgba(0,0,0,.6)'; c.shadowOffsetY = 2; c.beginPath(); c.moveTo(1522, 522); c.quadraticCurveTo(1498 + 4, 540 - 30, 1498 + 2, 540 + 2); c.stroke(); c.restore();
});
/* Selection: the warm glow in the dish (radius 80 for a 50 px dish) and the gold ring (radius 67), each in a 200 px frame. */
part('T_UI_SlotGlow', 200, 200, 1, c => selGlow(c, 100, 100, 80));
part('T_UI_SlotRing', 200, 200, 1, c => { c.save(); c.strokeStyle = 'rgba(255,214,140,.9)'; c.lineWidth = 2.4; c.shadowBlur = 14; c.shadowColor = 'rgba(255,190,90,.9)'; c.beginPath(); c.arc(100, 100, 67, 0, TAU); c.stroke(); c.restore(); });
/* The item's soft contact shadow on the leather (ellipse 64 x 18, 20 px below the dish's centre), frame 100 x 50. */
part('T_UI_ContactShadow', 100, 50, 1, c => { c.save(); c.fillStyle = 'rgba(0,0,0,.35)'; c.filter = 'blur(5px)'; c.beginPath(); c.ellipse(50, 25, 32, 9, 0, 0, TAU); c.fill(); c.restore(); });
/* The ancient shard's own glow (turquoise, the lost civilisation only): frame 200 px, radius 0.8 x 96. */
part('T_UI_ShardGlow', 200, 200, 1, c => { const g = c.createRadialGradient(100, 100, 2, 100, 100, 77); g.addColorStop(0, 'rgba(110,255,230,.5)'); g.addColorStop(1, 'rgba(90,230,210,0)'); c.fillStyle = g; c.fillRect(0, 0, 200, 200); });
/* The screen's vignette: dark towards the edges (~70 %), stretched over the screen. */
part('T_UI_Vignette', 256, 144, 1, c => { const g = c.createRadialGradient(128, 79, 43, 128, 79, 151); g.addColorStop(0, 'rgba(0,0,0,0)'); g.addColorStop(1, 'rgba(0,0,0,.7)'); c.fillStyle = g; c.fillRect(0, 0, 256, 144); });

/* ---------- 5. the stores (section 6) ---------- */
/* The satchel beside a store: the leather panel 780 x 790 at (120, 180), "SATCHEL" debossed, the 24 dishes (radius 44,
   6 x 4 at 225 + 114 col, 374 + 150 row). The count is a widget. Frame: 1080p x 60..960, y 130..1040. */
const SS_X0 = 60, SS_Y0 = 130;
part('T_UI_StoreSatchel', 900, 910, 1, c => {
  c.translate(-SS_X0, -SS_Y0);
  const sp = rr(120, 180, 780, 790, 40); dropShadow(c, sp, 40, 'rgba(0,0,0,.8)', 0, 16); leatherAt(c, sp, 120, 180, 780, 790, 0, 1); innerShadow(c, sp, 50, 'rgba(0,0,0,.55)', 0, 20); bevel(c, sp, 1.3);
  stitches(c, rrPts(142, 202, 736, 746, 24), { step: 16, len: 9 });
  deboss(c, 'SATCHEL', 178, 272, '400 44px "IM Fell English SC",Georgia,serif', { ls: 7 });
  for (let i = 0; i < 24; i++) ringSlot(c, 225 + (i % 6) * 114, 374 + (i / 6 | 0) * 150, null, false, 44);
});
/* The open crate, as the design draws it: the lid propped open behind with "CRATE" burned in, the box with its rim and
   iron L-plates, the far and side walls, the floor planks and the dividers into 5 x 2 compartments. Not in it: the count,
   the items and the target outline (widgets). Compartment i: x 1060 + 136 (i % 5), y 482 + 161 (i / 5), 136 x 161.
   Frame: 1080p x 950..1850, y 40..980. */
const CR_X0 = 950, CR_Y0 = 40;
part('T_UI_StoreCrate', 900, 940, 1, c => {
  c.translate(-CR_X0, -CR_Y0);
  const X0 = 1010, X1 = 1790, Y0 = 380, Y1 = 840, t = 36, iX0 = X0 + t, iX1 = X1 - t, iY0 = Y0 + t, iY1 = Y1 - t, fy = iY0 + 66, sl = 26;
  const lid = poly([[X0 + 6, Y0 + 4], [X1 - 6, Y0 + 4], [X1 - 18, 96], [X0 + 18, 96]]); dropShadow(c, lid, 30, 'rgba(0,0,0,.75)', 0, 10);
  for (let k = 0; k < 4; k++) { const ya = 96 + k * (Y0 + 4 - 96) / 4, yb = 96 + (k + 1) * (Y0 + 4 - 96) / 4, ia = 18 - (12 * (ya - 96) / (Y0 - 92)), ib = 18 - (12 * (yb - 96) / (Y0 - 92));
    const pl = poly([[X0 + ia, ya], [X1 - ia, ya], [X1 - ib, yb], [X0 + ib, yb]]); woodAt(c, pl, X0, ya, X1 - X0, yb - ya, k + 3, .82); innerShadow(c, pl, 6, 'rgba(0,0,0,.55)', 0, 2); }
  innerShadow(c, lid, 40, 'rgba(0,0,0,.6)', 0, -10); bevel(c, lid, 1);
  { const e = rr(X0 + 12, 74, X1 - X0 - 24, 26, 6); dropShadow(c, e, 10, 'rgba(0,0,0,.7)', 0, 4); woodAt(c, e, X0 + 12, 74, X1 - X0 - 24, 26, 11, 1.15); bevel(c, e, .6); }
  { const g = c.createLinearGradient(0, Y0 - 30, 0, Y0 + 6); g.addColorStop(0, 'rgba(0,0,0,0)'); g.addColorStop(1, 'rgba(0,0,0,.5)'); c.fillStyle = g; c.fillRect(X0 + 6, Y0 - 30, X1 - X0 - 12, 36); }
  [X0 + 78, X1 - 78].forEach(x => ironPlate(c, poly([[x - 18, 100], [x + 18, 100], [x + 20, Y0 + 6], [x - 20, Y0 + 6]]), [[x, 124], [x, 190], [x, 256]]));
  burn(c, 'CRATE', (X0 + X1) / 2, 228, '400 50px "IM Fell English SC",Georgia,serif', { align: 'center', ls: 6 });
  const front = rr(X0 - 4, Y1 - 6, X1 - X0 + 8, 70, [0, 0, 10, 10]); dropShadow(c, front, 30, 'rgba(0,0,0,.85)', 0, 16); woodAt(c, front, X0, Y1, X1 - X0, 70, 9, .62); bevel(c, front, .8);
  ironPlate(c, rr(X0 - 4, Y1 + 24, X1 - X0 + 8, 16, 3), [[X0 + 40, Y1 + 32], [X0 + 260, Y1 + 32], [(X0 + X1) / 2, Y1 + 32], [X1 - 260, Y1 + 32], [X1 - 40, Y1 + 32]]);
  const far = poly([[iX0, iY0], [iX1, iY0], [iX1 - sl, fy], [iX0 + sl, fy]]); woodAt(c, far, iX0, iY0, iX1 - iX0, fy - iY0, 5, .45); innerShadow(c, far, 20, 'rgba(0,0,0,.6)', 0, 8);
  const lw = poly([[iX0, iY0], [iX0 + sl, fy], [iX0, iY1]]), rw = poly([[iX1, iY0], [iX1 - sl, fy], [iX1, iY1]]); woodAt(c, lw, iX0, iY0, sl, iY1 - iY0, 2, .5, true); woodAt(c, rw, iX1 - sl, iY0, sl, iY1 - iY0, 6, .4, true);
  const nP = 6, ph = (iY1 - fy) / nP, lx = y => iX0 + sl * (1 - (y - fy) / (iY1 - fy)), rx = y => iX1 - sl * (1 - (y - fy) / (iY1 - fy));
  for (let k = 0; k < nP; k++) { const ya = fy + k * ph, yb = ya + ph, pl = poly([[lx(ya), ya], [rx(ya), ya], [rx(yb), yb], [lx(yb), yb]]); woodAt(c, pl, iX0, ya, iX1 - iX0, ph, k + 7, .8 + .06 * (k % 2));
    c.save(); c.strokeStyle = 'rgba(20,10,4,.75)'; c.lineWidth = 2.4; c.beginPath(); c.moveTo(lx(yb), yb); c.lineTo(rx(yb), yb); c.stroke(); c.restore();
    [[lx(ya) + 22, ya + ph / 2], [rx(ya) - 22, ya + ph / 2]].forEach(([x, y]) => ironRivet(c, x, y, 3.4)); }
  const g = c.createLinearGradient(0, fy, 0, fy + 90); g.addColorStop(0, 'rgba(0,0,0,.55)'); g.addColorStop(1, 'rgba(0,0,0,0)'); c.fillStyle = g; c.fillRect(iX0, fy, iX1 - iX0, 90);
  const cx0 = iX0 + 14, cx1 = iX1 - 14, cw = (cx1 - cx0) / 5, chh = (iY1 - fy) / 2;
  const divider = (x, y, w, h, v, sd) => { const path = rr(x, y, w, h, 3); dropShadow(c, path, 8, 'rgba(0,0,0,.75)', 4, 6); woodAt(c, path, x, y, w, h, sd, 1.05, v); bevel(c, path, .5); };
  for (let k = 1; k < 5; k++) { const x = cx0 + k * cw; divider(x - 7, fy + 2, 14, iY1 - fy - 4, true, k + 2); }
  for (let k = 1; k < 2; k++) { const y = fy + k * chh; divider(cx0, y - 7, cx1 - cx0, 14, false, k + 5); }
  const rim = new Path2D(); rim.addPath(rr(X0, Y0, X1 - X0, Y1 - Y0, 10)); rim.addPath(rr(iX0, iY0, iX1 - iX0, iY1 - iY0, 3));
  [[X0, Y0, X1 - X0, t, false, 1], [X0, Y1 - t, X1 - X0, t, false, 4], [X0, Y0, t, Y1 - Y0, true, 2], [X1 - t, Y0, t, Y1 - Y0, true, 8]].forEach(([x, y, w, h, v, s]) => { const p = new Path2D(); p.rect(x, y, w, h); c.save(); c.clip(rim, 'evenodd'); woodAt(c, p, x, y, w, h, s, 1.08, v); c.restore(); });
  c.save(); c.clip(rim, 'evenodd'); bevel(c, rr(X0, Y0, X1 - X0, Y1 - Y0, 10), 1); c.restore(); innerShadow(c, rr(iX0, iY0, iX1 - iX0, iY1 - iY0, 3), 18, 'rgba(0,0,0,.7)', 4, 8);
  [[X0, Y0, 1, 1], [X1, Y0, -1, 1], [X0, Y1, 1, -1], [X1, Y1, -1, -1]].forEach(([x, y, sx, sy]) => { const a = 70; ironPlate(c, poly([[x, y], [x + sx * a, y], [x + sx * a, y + sy * t], [x + sx * t, y + sy * t], [x + sx * t, y + sy * a], [x, y + sy * a]]), [[x + sx * 18, y + sy * 18], [x + sx * 52, y + sy * 18], [x + sx * 18, y + sy * 52]]); });
  [X0 + 78, X1 - 78].forEach(x => ironPlate(c, rr(x - 30, Y0 - 14, 60, 40, 5), [[x - 16, Y0 + 6], [x + 16, Y0 + 6]]));
});
/* The round barrel (section 6; the design has no picture of it, so it follows the crate): an open barrel seen from above
   (the ellipse 0.7 of the circle) with its staves and two iron hoops, its head leaning behind it with "BARREL" burned
   in. Inside, on a planked false bottom, a ring divider and radial dividers make 3 compartments in the middle and 7
   around. Floor coordinates (u, v) map to the screen as (1400 + u, 580 + 0.7 v); compartment centres: inner at radius
   91, angles 270 / 30 / 150 degrees, outer at radius 236, angles 270 + k 360/7 (items 88 px) (keep in sync with UI/SaltStoreWidget).
   Frame: 1080p x 950..1850, y 10..1010. */
const BA_X0 = 950, BA_Y0 = 10;
const BAR = { cx: 1400, cy: 560, rx: 340, k: .7, rim: 26, fy: 580, rf: 312, ri: 165, H: 160, bulge: 18, head: [1400, 270, 260, 230] };
part('T_UI_StoreBarrel', 900, 1000, 1, c => {
  c.translate(-BA_X0, -BA_Y0);
  const { cx, cy, rx, k } = BAR, ry = rx * k, irx = rx - BAR.rim, iry = irx * k;
  const ell = (x, y, a, b) => { const p = new Path2D(); p.ellipse(x, y, a, b, 0, 0, TAU); return p; };
  const F = (u, v) => [cx + u, BAR.fy + v * k];
  /* the head, leaning behind: planks across, nails, "BARREL" burned in, the barrel's shadow on its lower part */
  const [hx, hy, hrx, hry] = BAR.head, head = ell(hx, hy, hrx, hry);
  dropShadow(c, head, 30, 'rgba(0,0,0,.75)', 0, 10);
  const seams = [hy - hry, 110, 232, 350, hy + hry];
  c.save(); c.clip(head);
  for (let i = 0; i < seams.length - 1; i++) { const ya = seams[i], yb = seams[i + 1], p = new Path2D(); p.rect(hx - hrx, ya, 2 * hrx, yb - ya); woodAt(c, p, hx - hrx, ya, 2 * hrx, yb - ya, i + 12, [.84, .8, .88, .78][i]); innerShadow(c, p, 6, 'rgba(0,0,0,.5)', 0, 2); }
  c.strokeStyle = 'rgba(20,10,4,.75)'; c.lineWidth = 2.4; seams.slice(1, -1).forEach(y => { c.beginPath(); c.moveTo(hx - hrx, y); c.lineTo(hx + hrx, y); c.stroke(); });
  c.restore();
  innerShadow(c, head, 34, 'rgba(0,0,0,.55)', 0, -8); bevel(c, head, 1);
  c.save(); c.clip(head); c.strokeStyle = 'rgba(255,225,170,.12)'; c.lineWidth = 3; c.stroke(ell(hx, hy, hrx - 16, hry - 16)); c.strokeStyle = 'rgba(20,10,4,.35)'; c.lineWidth = 2; c.stroke(ell(hx, hy, hrx - 18, hry - 18)); c.restore();
  [[75, .7], [171, .95], [291, .98]].forEach(([y, f]) => { const half = hrx * Math.sqrt(Math.max(0, 1 - ((y - hy) / hry) ** 2)) * f - 30; [hx - half, hx + half].forEach(x => ironRivet(c, x, y, 3.6)); });
  burn(c, 'BARREL', hx, 165, '400 50px "IM Fell English SC",Georgia,serif', { align: 'center', ls: 6 });
  c.save(); c.clip(head); const hs = c.createLinearGradient(0, cy - ry - 60, 0, cy - ry + 10); hs.addColorStop(0, 'rgba(0,0,0,0)'); hs.addColorStop(1, 'rgba(0,0,0,.6)'); c.fillStyle = hs; c.fillRect(hx - hrx, cy - ry - 60, 2 * hrx, 200); c.restore();
  /* the body: staves bulging a little, lit from the front-left; its shadow */
  const r = s => rx + BAR.bulge * Math.sin(Math.PI * s), P = (s, a) => [cx + r(s) * Math.cos(a), cy + BAR.H * s + r(s) * k * Math.sin(a)];
  const body = new Path2D(); const S = 12;
  for (let i = 0; i <= S; i++) { const [x, y] = P(i / S, Math.PI); i ? body.lineTo(x, y) : body.moveTo(x, y); }
  for (let i = 0; i <= 48; i++) { const [x, y] = P(1, Math.PI - Math.PI * i / 48); body.lineTo(x, y); }
  for (let i = S; i >= 0; i--) { const [x, y] = P(i / S, 0); body.lineTo(x, y); }
  for (let i = 0; i <= 48; i++) { const a = -Math.PI * i / 48; body.lineTo(cx + rx * Math.cos(a), cy + ry * Math.sin(a)); }
  body.closePath();
  dropShadow(c, body, 30, 'rgba(0,0,0,.85)', 0, 16);
  const N = 16;
  for (let j = 0; j < N; j++) { const a0 = Math.PI * j / N, a1 = Math.PI * (j + 1) / N, st = new Path2D();
    for (let i = 0; i <= S; i++) { const [x, y] = P(i / S, a0); i ? st.lineTo(x, y) : st.moveTo(x, y); }
    for (let i = 0; i <= 6; i++) { const [x, y] = P(1, a0 + (a1 - a0) * i / 6); st.lineTo(x, y); }
    for (let i = S; i >= 0; i--) { const [x, y] = P(i / S, a1); st.lineTo(x, y); }
    for (let i = 0; i <= 6; i++) { const [x, y] = P(0, a1 - (a1 - a0) * i / 6); st.lineTo(x, y); }
    st.closePath();
    const am = (a0 + a1) / 2, lit = .5 + .45 * Math.max(0, Math.cos(am - 2.0)) + .05 * (j % 2), xs = [P(0, a0)[0], P(0, a1)[0], P(.5, a0)[0], P(.5, a1)[0]];
    const x0 = Math.min(...xs) - 2, x1 = Math.max(...xs) + 2; woodAt(c, st, x0, cy - 10, Math.max(8, x1 - x0), BAR.H + ry + 30, j + 20, lit, true); }
  c.save(); c.strokeStyle = 'rgba(20,10,4,.7)'; c.lineWidth = 2;
  for (let j = 1; j < N; j++) { const a = Math.PI * j / N; c.beginPath(); for (let i = 0; i <= S; i++) { const [x, y] = P(i / S, a); i ? c.lineTo(x, y) : c.moveTo(x, y); } c.stroke(); }
  c.restore();
  bevel(c, body, 1);
  /* two iron hoops round the body */
  [[.16, .32], [.6, .76]].forEach(([s0, s1]) => { const hp = new Path2D();
    for (let i = 0; i <= 48; i++) { const [x, y] = P(s0, Math.PI * i / 48); i ? hp.lineTo(x, y) : hp.moveTo(x, y); }
    for (let i = 48; i >= 0; i--) { const [x, y] = P(s1, Math.PI * i / 48); hp.lineTo(x, y); }
    hp.closePath(); ironPlate(c, hp, [.35, .9, 1.57, 2.24, 2.79].map(a => P((s0 + s1) / 2, a))); });
  /* inside: the far wall, the planked false bottom, its shadow, the dividers */
  const opening = ell(cx, cy, irx, iry);
  c.save(); c.clip(opening);
  { const p = new Path2D(); p.rect(cx - irx, cy - iry, 2 * irx, 2 * iry); woodAt(c, p, cx - irx, cy - iry, 2 * irx, 2 * iry, 5, .42, true); }
  c.strokeStyle = 'rgba(20,10,4,.6)'; c.lineWidth = 2;
  for (let j = 0; j < 32; j++) { const a = Math.PI + Math.PI * j / 32; c.beginPath(); c.moveTo(cx + irx * Math.cos(a), cy + iry * Math.sin(a)); c.lineTo(...F(BAR.rf * Math.cos(a), BAR.rf * Math.sin(a))); c.stroke(); }
  const floor = ell(cx, BAR.fy, BAR.rf, BAR.rf * k);
  c.save(); c.clip(floor);
  const fTop = BAR.fy - BAR.rf * k, fBot = BAR.fy + BAR.rf * k, nP = 7, ph = (fBot - fTop) / nP;
  for (let j = 0; j < nP; j++) { const ya = fTop + j * ph, p = new Path2D(); p.rect(cx - BAR.rf, ya, 2 * BAR.rf, ph); woodAt(c, p, cx - BAR.rf, ya, 2 * BAR.rf, ph, j + 30, .8 + .06 * (j % 2));
    c.strokeStyle = 'rgba(20,10,4,.75)'; c.lineWidth = 2.4; c.beginPath(); c.moveTo(cx - BAR.rf, ya + ph); c.lineTo(cx + BAR.rf, ya + ph); c.stroke(); }
  const fs = c.createLinearGradient(0, fTop, 0, fTop + 90); fs.addColorStop(0, 'rgba(0,0,0,.6)'); fs.addColorStop(1, 'rgba(0,0,0,0)'); c.fillStyle = fs; c.fillRect(cx - BAR.rf, fTop, 2 * BAR.rf, 90);
  c.restore();
  innerShadow(c, floor, 14, 'rgba(0,0,0,.55)', 0, 6);
  const D = 6, dividerPath = pts => { const p = new Path2D(); pts.forEach(([u, v], i) => { const [x, y] = F(u, v); i ? p.lineTo(x, y) : p.moveTo(x, y); }); p.closePath(); return p; };
  const divider = (p, sd, v) => { dropShadow(c, p, 8, 'rgba(0,0,0,.75)', 4, 6); woodAt(c, p, cx - BAR.rf, BAR.fy - BAR.rf * k, 2 * BAR.rf, 2 * BAR.rf * k, sd, 1.05, v); bevel(c, p, .5); };
  const radial = (deg, r0, r1, sd) => { const a = deg * Math.PI / 180, ux = Math.cos(a), uy = Math.sin(a), nx = -uy * D, ny = ux * D;
    divider(dividerPath([[ux * r0 + nx, uy * r0 + ny], [ux * r1 + nx, uy * r1 + ny], [ux * r1 - nx, uy * r1 - ny], [ux * r0 - nx, uy * r0 - ny]]), sd, Math.abs(uy) > .7); };
  for (let j = 0; j < 7; j++) radial(270 + (j + .5) * 360 / 7, BAR.ri, BAR.rf + 20, 40 + j);
  [90, 210, 330].forEach((d, j) => radial(d, 0, BAR.ri, 50 + j));
  { const ring = new Path2D(), n = 96; for (const [rad, dir] of [[BAR.ri + D, 1], [BAR.ri - D, -1]]) { for (let i = 0; i <= n; i++) { const a = dir * TAU * i / n, [x, y] = F(rad * Math.cos(a), rad * Math.sin(a)); i ? ring.lineTo(x, y) : ring.moveTo(x, y); } ring.closePath(); }
    dropShadow(c, ring, 8, 'rgba(0,0,0,.75)', 4, 6, 'evenodd'); c.save(); c.clip(ring, 'evenodd'); const p = new Path2D(); p.rect(cx - BAR.ri - 10, BAR.fy - BAR.ri, 2 * BAR.ri + 20, 2 * BAR.ri); woodAt(c, p, cx - BAR.ri - 10, BAR.fy - BAR.ri, 2 * BAR.ri + 20, 2 * BAR.ri, 60, 1.05); c.restore();
    c.save(); c.clip(ring, 'evenodd'); bevel(c, ell(cx, BAR.fy, BAR.ri + D, (BAR.ri + D) * k), .5); c.restore(); }
  c.restore();
  innerShadow(c, opening, 18, 'rgba(0,0,0,.7)', 4, 8);
  /* the staves' tops round the opening (the chime), with their joints */
  const chime = new Path2D(); chime.addPath(ell(cx, cy, rx, ry)); chime.addPath(ell(cx, cy, irx, iry));
  c.save(); c.clip(chime, 'evenodd'); { const p = new Path2D(); p.rect(cx - rx, cy - ry, 2 * rx, 2 * ry); woodAt(c, p, cx - rx, cy - ry, 2 * rx, 2 * ry, 3, 1.08); }
  c.strokeStyle = 'rgba(20,10,4,.7)'; c.lineWidth = 2;
  for (let j = 0; j < 32; j++) { const a = TAU * j / 32; c.beginPath(); c.moveTo(cx + irx * Math.cos(a), cy + iry * Math.sin(a)); c.lineTo(cx + rx * Math.cos(a), cy + ry * Math.sin(a)); c.stroke(); }
  c.restore();
  c.save(); c.clip(chime, 'evenodd'); bevel(c, ell(cx, cy, rx, ry), 1); c.restore();
});
/* The target of a flying item (section 6): a warm dashed outline with a faint fill. Crate compartment: 120 x 145 (radius
   10) in a 160 x 185 frame; round (a satchel dish or a barrel compartment): radius 56 in a 160 x 160 frame. */
function targetOutline(c, p) { c.save(); c.fillStyle = 'rgba(255,200,120,.16)'; c.fill(p); c.setLineDash([10, 8]); c.strokeStyle = 'rgba(255,222,160,.85)'; c.lineWidth = 2.4; c.shadowBlur = 12; c.shadowColor = 'rgba(255,190,90,.8)'; c.stroke(p); c.restore(); }
part('T_UI_StoreTargetRect', 160, 185, 1, c => targetOutline(c, rr(20, 20, 120, 145, 10)));
part('T_UI_StoreTargetRound', 160, 160, 1, c => { const p = new Path2D(); p.arc(80, 80, 56, 0, TAU); targetOutline(c, p); });
/* One dot of the flight's path (white, tinted by the widget; the design's 3 px dashes with 4 px round caps): radius 2.9 in a
   12 x 12 frame. */
part('T_UI_Dot', 12, 12, 2, c => { c.fillStyle = '#fff'; c.beginPath(); c.arc(6, 6, 2.9, 0, TAU); c.fill(); });
/* The store's hints along the bottom (x 560.., baseline 1052). Frame: 1080p x 540..1400, y 1000..1080. */
part('T_UI_StoreHints', 860, 80, 1, c => {
  c.translate(-540, -1000);
  let x = 560; const y = 1052, sh = { shadow: ['rgba(0,0,0,.9)', 6, 0, 2] };
  txt(c, 'Click: move the stack', x, y, '400 27px Alegreya,Georgia,serif', '#f0e2c4', sh); x += 270;
  txt(c, '·', x, y, '400 27px Alegreya', '#f0e2c4', sh); x += 30;
  txt(c, 'Shift-click: move one', x, y, '400 27px Alegreya,Georgia,serif', '#f0e2c4', sh); x += 280;
  txt(c, '·', x, y, '400 27px Alegreya', '#f0e2c4', sh); x += 30;
  x += keycap(c, 'E', x, y - 28, 38) + 12; txt(c, 'close the lid', x, y, '400 27px Alegreya,Georgia,serif', '#f0e2c4', sh);
});

/* ---------- 6. landfall (section 9) ---------- */
/* Each island's banner, drawn by the design's banner() (UI_Prompts.html) and split into layers the widget animates: the
   ribbon (tails, band, rules) and the name (each letter on the arch, ink with bleed). The seal is shared; the heading and
   the subtitle are painted lettering in the widget. Frame: 1080p x 160..1760, y 120..340; the band's top is at y 150,
   centred on x 960, half as wide as max(300, name / 2 + 100). Keep LANDFALL in sync with Core/SaltPlaces.cpp. */
const LANDFALL = [['Spire', 'The Spire'], ['RuinIsle', 'Ruin Isle'], ['Mistholm', 'Mistholm'], ['GullRock', 'Gull Rock'], ['Longshoal', 'Longshoal']];
const LF_X0 = 160, LF_Y0 = 120;
function landfallLayer(c, name, layer) {
  c.translate(-LF_X0, -LF_Y0);
  const cx = 960, size = 66, F = `400 ${size}px ${FSC}`, ls = 5, chars = [...name];
  const cw = chars.map(ch => measure(c, ch, F)), tw = cw.reduce((a, b) => a + b, 0) + ls * (chars.length - 1);
  const halfW = Math.max(300, tw / 2 + 100), h = 90, arch = 18, yTop = 150, xL = cx - halfW, xR = cx + halfW, tailW = 118, ov = 40;
  const T = x => yTop + arch * ((x - cx) / halfW) ** 2, dT = x => 2 * arch * (x - cx) / (halfW * halfW);
  if (layer === 'name') {
    let x = cx - tw / 2;
    chars.forEach((ch, i) => { const xm = x + cw[i] / 2, ym = T(xm) + h / 2 + size * .33, a = Math.atan(dT(xm));
      c.save(); c.translate(xm, ym); c.rotate(a); c.filter = 'blur(.8px)'; txt(c, ch, 0, 0, F, 'rgba(48,30,16,.35)', { align: 'center' }); c.filter = 'none'; txt(c, ch, 0, 0, F, INK, { align: 'center' }); c.restore(); x += cw[i] + ls; });
    return;
  }
  // the tails behind, with a swallowtail cut
  [-1, 1].forEach(side => { const xe = side < 0 ? xL : xR, top = T(xe) + h * .46, bot = top + h * .92, out = xe + side * tailW, inn = xe - side * ov, mid = (top + bot) / 2, dy = 10;
    const tp = poly([[inn, top], [out, top + dy], [out - side * 36, mid + dy], [out, bot + dy], [inn, bot]]);
    dropShadow(c, tp, 16, 'rgba(0,0,0,.55)', 3, 8); c.save(); c.clip(tp); c.filter = 'brightness(.74)'; c.drawImage(PARCH_BIG, side < 0 ? xL - tailW - 10 : xR - 200, top - 20); c.restore();
    const g = c.createLinearGradient(inn, 0, out, 0); g.addColorStop(0, 'rgba(40,20,6,.45)'); g.addColorStop(1, 'rgba(40,20,6,0)'); c.save(); c.clip(tp); c.fillStyle = g; c.fillRect(Math.min(inn, out), top - 5, tailW + ov, bot - top + 30); c.restore();
    innerShadow(c, tp, 10, 'rgba(110,62,20,.6)');
    const fold = poly([[xe, T(xe) + h - 2], [inn, T(xe) + h - 2], [inn, bot]]); const fg = c.createLinearGradient(0, T(xe) + h, 0, bot); fg.addColorStop(0, '#3d2410'); fg.addColorStop(1, '#6b4626'); c.fillStyle = fg; c.fill(fold); });
  // the band along its arch
  const band = new Path2D(); for (let x = xL; x <= xR + .1; x += 8) x === xL ? band.moveTo(x, T(x)) : band.lineTo(x, T(x)); for (let x = xR; x >= xL - .1; x -= 8) band.lineTo(x, T(x) + h); band.closePath();
  dropShadow(c, band, 22, 'rgba(0,0,0,.6)', 4, 10); c.save(); c.clip(band); c.drawImage(PARCH_BIG, xL - 20, yTop - 30); c.restore();
  c.save(); c.clip(band); const sg = c.createLinearGradient(0, yTop, 0, yTop + h + arch); sg.addColorStop(0, 'rgba(255,246,220,.28)'); sg.addColorStop(.35, 'rgba(255,246,220,0)'); sg.addColorStop(1, 'rgba(70,40,14,.32)'); c.fillStyle = sg; c.fillRect(xL, yTop - 5, 2 * halfW, h + arch + 10); c.restore();
  innerShadow(c, band, 14, 'rgba(120,70,24,.6)'); innerShadow(c, band, 2.5, 'rgba(90,52,18,.7)');
  const rule = (off, lw) => { c.save(); c.strokeStyle = 'rgba(48,30,16,.55)'; c.lineWidth = lw; c.beginPath(); for (let x = xL + 14; x <= xR - 14; x += 6) x === xL + 14 ? c.moveTo(x, T(x) + off) : c.lineTo(x, T(x) + off); c.stroke(); c.restore(); };
  [[9, 1.6], [14, .8], [h - 9, 1.6], [h - 14, .8]].forEach(([off, lw]) => rule(off, lw));
}
for (const [id, name] of LANDFALL) {
  part('T_UI_Landfall_' + id + '_Band', 1600, 220, 1, c => landfallLayer(c, name, 'band'));
  part('T_UI_Landfall_' + id + '_Name', 1600, 220, 1, c => landfallLayer(c, name, 'name'));
}
/* The oxblood wax seal (radius 40) with the island-and-gateway glyph, centred in a 120 x 120 frame. */
part('T_UI_LandfallSeal', 120, 120, 2, c => { waxSeal(c, 60, 60, 40, '', 7); sealGlyph(c, 'island', 60, 60, 40); });
