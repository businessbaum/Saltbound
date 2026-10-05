/* Saltbound menu parts (D66): the start, pause, settings, voyages, loading, first-start and keys screens. Each part is drawn
   alone on a transparent canvas with the approved designs' own code: the toolkit and the prompt functions are pulled out of
   Docs/Design/UI/*.html by gen_ui_parts.py, the rope / knot / anchor drawing is the one on the owner-approved menu page
   (Docs/Design/Menu_Ideas/Menus.html). Every part draws at 1080p pixel units into a canvas scaled by its `scale`. */

const PARTS = [];
function part(name, w, h, scale, draw) { PARTS.push({ name, w, h, scale, draw }); }

function setupShared() {
  PARCH = parchment(1000, 900, 6);
  SPECK = tex(1400, 80, (x, y) => { const n = N3(x / 1.7, y / 1.7), m = N1(x / 12, y / 12); return [95, 62, 30, clamp(sstep(.28, .62, n) * .9 + sstep(.35, .75, m) * .45) * 255]; });
}

/* The big rope, exactly as the intro and the menu page draw it: two edges, the twist, salt-white paint with a dark rim. */
function ropeDraw(c, pts, t, lw, blur) {
  const S = []; walk(pts, Math.max(2.2, t * .3), (x, y, a) => S.push([x, y, a])); if (S.length < 2) return;
  const side = sg => S.map(([x, y, a]) => [x - Math.sin(a) * t / 2 * sg, y + Math.cos(a) * t / 2 * sg]), A = side(1), B = side(-1);
  const pass = (col, w, bl) => {
    c.save(); c.lineCap = 'round'; c.lineJoin = 'round'; c.strokeStyle = col; c.lineWidth = w; if (bl) { c.shadowBlur = bl; c.shadowColor = 'rgba(0,0,0,.85)'; }
    [A, B].forEach(P => { c.beginPath(); P.forEach(([x, y], i) => i ? c.lineTo(x, y) : c.moveTo(x, y)); c.stroke(); });
    c.beginPath(); S.forEach(([x, y, a], i) => { if (i === S.length - 1) return; const nx = -Math.sin(a), ny = Math.cos(a), tx = Math.cos(a), ty = Math.sin(a); c.moveTo(x + nx * t / 2, y + ny * t / 2); c.lineTo(x + tx * t * .42 - nx * t / 2, y + ty * t * .42 - ny * t / 2); }); c.stroke(); c.restore();
  };
  pass('rgba(18,10,4,.78)', lw + 2.6, blur); pass('#f2e2bf', lw, 0);
}

/* The rope as a seamless horizontal tile: 420 x 30, the rope 14 px thick in the middle (the twist repeats every 4.2 px, 100 times
   across the tile). Widgets tile it along x and scale it to their height. */
part('T_UI_MenuRope', 420, 30, 2, c => ropeDraw(c, [[-42, 15], [462, 15]], 14, 2.4, 5));

/* A knot tied in the same rope (slider handles): 120 x 60, the rope runs in at the left and out at the right at y 36, the knot's
   loop above it; the widget centres it on the value. */
part('T_UI_MenuKnot', 120, 60, 2, c => {
  const cp = [[-70, 36], [-14, 36], [-2, 34], [8, 26], [10, 15], [3, 6], [-7, 6], [-13, 15], [-9, 27], [0, 35], [14, 37], [70, 36]].map(([x, y]) => [x + 60, y]);
  const cr = []; for (let i = 0; i < cp.length - 1; i++) { const p0 = cp[Math.max(0, i - 1)], p1 = cp[i], p2 = cp[i + 1], p3 = cp[Math.min(cp.length - 1, i + 2)];
    for (let s = 0; s < 8; s++) { const u = s / 8, u2 = u * u, u3 = u2 * u; cr.push([0, 1].map(j => .5 * (2 * p1[j] + (-p0[j] + p2[j]) * u + (2 * p0[j] - 5 * p1[j] + 4 * p2[j] - p3[j]) * u2 + (-p0[j] + 3 * p1[j] - 3 * p2[j] + p3[j]) * u3))); } }
  cr.push(cp[cp.length - 1]); ropeDraw(c, cr, 11.5, 2.4, 5);
});

/* A small painted anchor (the "< value >" arrows): 48 x 48, crown down; the widget turns it 90 degrees each way. */
part('T_UI_MenuAnchor', 48, 48, 2, c => {
  const anchor = () => { c.beginPath(); c.arc(24, 8, 4, 0, TAU); c.moveTo(24, 12); c.lineTo(24, 39); c.moveTo(14, 15); c.lineTo(34, 15); c.moveTo(10, 28); c.quadraticCurveTo(12, 41, 24, 40); c.quadraticCurveTo(36, 41, 38, 28);
    c.moveTo(10, 28); c.lineTo(7, 33); c.moveTo(10, 28); c.lineTo(15, 31); c.moveTo(38, 28); c.lineTo(41, 33); c.moveTo(38, 28); c.lineTo(33, 31); };
  c.lineCap = 'round'; c.lineJoin = 'round'; c.save(); c.strokeStyle = 'rgba(18,10,4,.8)'; c.lineWidth = 6.6; c.shadowBlur = 5; c.shadowColor = 'rgba(0,0,0,.85)'; anchor(); c.stroke(); c.restore();
  c.strokeStyle = '#f2e2bf'; c.lineWidth = 3.6; anchor(); c.stroke();
});

/* Parchment cards with torn edges and a burnt rim (the prompts' scrap), each in a frame 30 px larger all round for its shadow:
   the pause menu's "where you are" card, a voyage page, the wide scrap of a question, and a wide key scrap for words. */
function card(name, w, h, seed, tear) { part(name, w + 60, h + 60, 1, c => scrap(c, 30, 30, w, h, { seed, rot: 0, tear })); }
card('T_UI_MenuCard', 560, 770, 21, 5);
card('T_UI_MenuSlot', 460, 600, 33, 5);
card('T_UI_MenuScrap', 845, 460, 41, 6);
/* The key scrap for words (Esc, Tab, Shift...): 116 x 58, in a 180 x 120 frame like T_UI_KeyScrap's 120 x 120. */
part('T_UI_MenuKeyWide', 180, 120, 2, c => scrap(c, 32, 31, 116, 58, { seed: 12, rot: 0, tear: 4 }));

/* The X key for "hold to scrap this voyage" (the ink-soak material needs the letter and its light twin, as the prompts' keys):
   same frame, face and baseline as ui_parts.js's key letters. */
part('T_UI_KeyLetter_X', 120, 120, 2, c => inkText(c, 'X', 60, 71, 32, { align: 'center', sc: true }));
part('T_UI_KeyLetterLight_X', 120, 120, 2, c => txt(c, 'X', 60, 71, `400 32px ${FSC}`, '#efdfb8', { align: 'center' }));
