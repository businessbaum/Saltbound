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
