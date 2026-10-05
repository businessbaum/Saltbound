/* Saltbound hub ship: the numbers. One source for every sheet and for the area count in HubShip_Design.md.
   Metres. x forward from the stern face (0) to the stem (160 at the main deck), y to starboard (+) from the centreline,
   z up from the design waterline. (In Unreal: +X forward, +Y starboard, +Z up, 1 m = 100 uu; pivot at x 80, y 0, z 0.) */
'use strict';

const HUB = {
  name: 'Hollowkeel',
  L: 160, B: 40, sprit: 188, keel: -12,
  Z: { deep: -8, quay: 1.5, lower: 5.5, middle: 9.5, upper: 13.5, main: 17.5, sc1: 21.5, sc2: 25.5, sc3: 29.5, poop: 33.5, fc1: 21.5, fcTop: 25.5 },
  rail: 1.2,                                        // bulwark and gallery rails above their floor
  basin: { x0: 0, x1: 44, y0: -10, y1: 10, floor: -5 }, // the Stern Dock: sea water, open to the sky
  ramp: { x0: 30, x1: 46, z0: -3, z1: 1.5 },        // the slipway, out of the water into the boat hall
  hall: { x0: 44, x1: 64, y0: -9, y1: 9 },          // the Slip's boat hall: quay deck to the upper deck (12 m clear)
  shop: { x0: 44, x1: 64, y0: 9, y1: 19 },          // the shipwright's shop, starboard of the hall
  stair: { x0: 30, x1: 44, y0: 14, y1: 17 },        // the dock stair: starboard quay (+1.5) up to the Tally House (+5.5)
  tally: { x0: 44, x1: 64, y0: 9, y1: 20 },         // the Tally House, lower deck, over the shop
  lane: { x0: 64, x1: 73, y0: 6, y1: 12 },          // lower deck, from the Tally House to the Well
  shaft: { x0: 78, x1: 90, y0: -6, y1: 6 },         // the Well: open from the Keelgate floor to the sky
  ring: { x0: 73, x1: 95, y0: -11, y1: 11 },        // its galleries (5 m wide) on the lower, middle and upper decks
  chamber: { x0: 72, x1: 96, y0: -12, y1: 12 },     // the Keelgate chamber at the foot of the Well (quay deck)
  keelgate: { x: 84, y: 0, r: 3.0, ring: 4.0 },     // the ancient hatch in its floor
  hoist: { x0: 78.2, x1: 80.8, y0: 3.2, y1: 5.8 },  // the hoist cage, aft-starboard corner of the shaft
  flights: [                                         // the Well's stairs, spiralling up through the ring (22 risers of 18 cm)
    { from: 'quay', to: 'lower', x0: 79, x1: 86, y0: 8, y1: 11, up: '-x' },
    { from: 'lower', to: 'middle', x0: 92, x1: 95, y0: -7, y1: 0, up: '-y' },
    { from: 'middle', to: 'upper', x0: 79, x1: 86, y0: -11, y1: -8, up: '-x' },
    { from: 'upper', to: 'main', x0: 73, x1: 76, y0: 0, y1: 7, up: '+y' },
  ],
  chandlery: { x0: 98, x1: 114, y0: -19, y1: -8, h: 4.2 },
  arch: { x0: 0, x1: 10, z0: 25.5, z1: 33.5 },
  wing: 10,     // the wings stand outboard of |y| 10, aft of x 44 (the basin between them)
  quayY: 14,    // the quays run |y| 10..14 at +1.5 under the wings (arcades)
  fcX: 134,     // the forecastle starts here
  masts: [
    { id: 'mizzen', name: 'Mizzen', x: 68, h: 54 },
    { id: 'main', name: 'Main', x: 100, h: 76 },
    { id: 'fore', name: 'Fore', x: 140, h: 64 },
  ],
  helm: { x: 6, y: 0, r: 1.8 },  // the great wheel on the poop over the arch: lashed, it never turns
  // Where she lies at Saltbound Cove (world metres, north +X, east +Y; the cove is 0, 0). Stern (the dock mouth) to the cove.
  berth: { id: 'SaltboundRoads', centreM: [230, -555], bowDeg: 292 },
};

/** Half-beam of the deck outline at x. */
HUB.hb = function (x) {
  if (x < 0 || x > 160) return 0;
  if (x < 10) return 19 + x / 10;
  if (x <= 108) return 20;
  const t = (x - 108) / 52; return 20 * Math.pow(Math.max(0, 1 - t * t), 0.55);
};
/** The deck outline as a polygon (x, y) from the stern round the bow. */
HUB.outline = function (k = 1, n = 80) {
  const s = [], p = [];
  for (let i = 0; i <= n; i++) { const x = i / n * 160; s.push([x, HUB.hb(x) * k]); p.push([x, -HUB.hb(x) * k]); }
  return s.concat(p.reverse());
};
/** The side profile (x, z): stern face, sterncastle, waist, forecastle, stem, keel. */
HUB.profile = function () {
  return [[0, 34.7], [44, 34.7], [44.6, 18.7], [134, 18.7], [134.6, 26.7], [161.5, 26.7], [161, 20], [159.8, 12], [157.5, 4], [153, -4], [145, -10], [132, -12],
    [14, -12], [6, -10], [2, -6], [0, -2]];
};

const ST = { OPEN: 'open', LOCKED: 'locked', WATER: 'water' };
const R = (x0, x1, y0, y1) => ({ x0, x1, y0, y1 });
const WINGS = [R(0, 44, 10, 20), R(0, 44, -20, -10)];

/** Levels (floors) of the ship. width: share of the deck half-beam at that floor (the hull is narrower low down). */
const LEVELS = [
  { id: 'sc3', name: 'Sterncastle, third storey', z: 29.5, only: [...WINGS, R(0, 10, -10, 10)] },
  { id: 'sc2', name: 'Sterncastle, second storey', z: 25.5, only: [...WINGS, R(0, 10, -10, 10)] },
  { id: 'sc1', name: 'Sterncastle, first storey', z: 21.5, only: WINGS },
  { id: 'fc1', name: 'Forecastle, upper storey', z: 21.5, only: [R(134, 160, -20, 20)] },
  { id: 'main', name: 'Main deck', z: 17.5, voids: [HUB.basin, HUB.shaft] },
  { id: 'upper', name: 'Upper deck', z: 13.5, voids: [HUB.basin, HUB.shaft] },
  { id: 'middle', name: 'Middle deck', z: 9.5, voids: [HUB.basin, HUB.hall, HUB.shaft] },
  { id: 'lower', name: 'Lower deck', z: 5.5, voids: [HUB.basin, HUB.hall, HUB.shaft] },
  { id: 'quay', name: 'Quay deck', z: 1.5, width: 0.95, water: [HUB.basin] },
  { id: 'deep', name: 'The Deep', z: -8, width: 0.8, only: [R(30, 150, -20, 20)] },
];

/** Open at the start: the 20 %. svc = the service badge (S J Q E). */
const OPEN = [
  { id: 'dock', name: 'Stern Dock', sub: 'basin and quays', level: 'quay', rects: [R(0, 44, 10, 14), R(0, 44, -14, -10)], water: true },
  { id: 'slip', name: 'The Slip', sub: 'boat hall and shop', svc: 'S', level: 'quay', rects: [HUB.hall, HUB.shop] },
  { id: 'stair', name: 'Dock stair', level: 'quay', rects: [HUB.stair] },
  { id: 'keelgate', name: 'The Keelgate', sub: 'chamber at the foot of the Well', svc: 'Q', level: 'quay', rects: [HUB.chamber] },
  { id: 'tally', name: 'Tally House', sub: 'jobs and pay', svc: 'J', level: 'lower', rects: [HUB.tally, R(42, 44, 14, 17)] },
  { id: 'lane', name: 'Starboard lane', level: 'lower', rects: [HUB.lane] },
  { id: 'well_l', name: 'The Well', sub: 'galleries, stairs, hoist', level: 'lower', rects: [HUB.ring] },
  { id: 'well_m', name: 'The Well', level: 'middle', rects: [HUB.ring] },
  { id: 'well_u', name: 'The Well', level: 'upper', rects: [HUB.ring] },
  { id: 'deck', name: 'Main deck', sub: 'the Waist', level: 'main', rects: [R(44, 116, -20, 20)] },
  { id: 'chandlery', name: 'Chandlery', sub: 'deckhouse', svc: 'E', level: 'main', rects: [R(98, 114, -19, -8)], inside: 'deck' },
];

/** Planned, locked, not designed: working names only. gate = the gate type (see GATES). */
const LOCKED = [
  { id: 'L1', name: 'Sterncastle and Stern Arch', levels: { main: WINGS, sc1: WINGS, sc2: [...WINGS, R(0, 10, -10, 10)], sc3: [...WINGS, R(0, 10, -10, 10)] }, gate: 'G1', sees: 'Tall doors at the aft end of the main deck, barred from inside. The great windows of the arch glow at night; the great wheel on the poop is lashed and never turns.' },
  { id: 'L2', name: 'Wing quarters', levels: { lower: WINGS, upper: WINGS }, gate: 'G4', sees: 'Rows of shuttered windows and washing lines on the wings over the dock.' },
  { id: 'L3', name: 'Quay stores', levels: { quay: [R(0, 44, 14, 20), R(0, 44, -20, -14)] }, gate: 'G4', sees: 'Shuttered bays along both arcades of the dock.' },
  { id: 'L4', name: 'Timber store', levels: { quay: [R(44, 64, -20, -9)] }, gate: 'G6', sees: 'An iron grille on the port side of the Slip; stacked timber behind it, 2 m deep.' },
  { id: 'L5', name: 'Keel Walk', levels: { quay: [R(64, 72, -20, 20)] }, gate: 'G1', sees: 'A heavy door in the Slip’s forward wall and another in the Keelgate chamber: the first shortcut to open.' },
  { id: 'L6', name: 'The Holds', levels: { quay: [R(96, 160, -20, 20), R(72, 96, 12, 20), R(72, 96, -20, -12)] }, gate: 'G6', sees: 'Iron grilles in the Keelgate chamber’s forward wall; padlocked cargo hatches on the main deck.' },
  { id: 'L7', name: 'Purser’s stores', levels: { lower: [R(64, 73, -11, 6), R(64, 73, 12, 20)] }, gate: 'G1', sees: 'A door behind the pay hatch.' },
  { id: 'L8', name: 'The Ropewalk', levels: { lower: [R(44, 150, -20, -9)] }, gate: 'G1', sees: 'Double doors off the Well’s port gallery; the creak of a rope being laid.' },
  { id: 'L9', name: 'Mess and galley', levels: { lower: [R(95, 160, -9, 20), R(73, 95, 11, 20)] }, gate: 'G1', sees: 'A door off the Well’s forward gallery; the galley chimney smokes on the main deck.' },
  { id: 'L10', name: 'Guild rooms', levels: { middle: [R(0, 73, -20, 20)] }, gate: 'G1', sees: 'Doors with painted crests off the Well’s aft gallery.' },
  { id: 'L11', name: 'Lantern Market', levels: { middle: [R(95, 160, -20, 20), R(73, 95, 11, 20), R(73, 95, -20, -11)] }, gate: 'G2', sees: 'A rope across the forward gallery with a board: “Market closed”; light and voices behind shutters.' },
  { id: 'L12', name: 'Archive', levels: { upper: [R(44, 73, -20, 20)] }, gate: 'G1', sees: 'A door with a brass plate off the Well’s aft gallery.' },
  { id: 'L13', name: 'Inn and bunkrooms', levels: { upper: [R(95, 160, -20, 20), R(73, 95, 11, 20), R(73, 95, -20, -11)] }, gate: 'G1', sees: 'A door off the forward gallery; laughter behind it in the evening.' },
  { id: 'L14', name: 'Forecastle and bow', levels: { main: [R(134, 160, -20, 20)], fc1: [R(134, 160, -20, 20)] }, gate: 'G3', sees: 'Chained ladders and closed doors at the forward end of the main deck; the bowsprit beyond.' },
  { id: 'L15', name: 'Cargo deck', levels: { main: [R(116, 134, -20, 20)] }, gate: 'G2', sees: 'The forward end of the main deck behind a rope and a board (“Cargo work, keep clear”): lashed cargo under nets, the padlocked hatches, the ship’s boats.' },
  { id: 'L16', name: 'Masts and tops', levels: {}, gate: 'G3', sees: 'Shrouds whose ratlines start out of reach; the tops’ lanterns at night.' },
  { id: 'L17', name: 'The Deep', levels: { deep: [R(30, 150, -20, 20)] }, gate: 'G5', sees: 'The Keelgate: the ancient seal in the floor at the foot of the Well, faintly lit from inside.' },
];

const GATES = {
  G1: { name: 'Barred door', why: 'Somebody’s room or business, not open to you yet.', how: 'A timber door with an iron bar and a padlock. E: a short line of why.' },
  G2: { name: 'Rope and board', why: 'Work going on, or closed for now.', how: 'A rope across the way, a hand-painted board on it.' },
  G3: { name: 'Chained ladder or hatch', why: 'Up or down: masts, forecastle, cargo hatches.', how: 'A chain through the rungs or the grating, padlocked.' },
  G4: { name: 'Shutters', why: 'Windows and bays of locked rooms.', how: 'Closed shutters: nothing inside to see, nothing inside to build.' },
  G5: { name: 'Ancient seal', why: 'The lost civilisation: the main quest.', how: 'Pale seamless stone, turquoise lines, no handle, no lock.' },
  G6: { name: 'Iron grille', why: 'A glimpse: stores and holds.', how: 'Bars you see through into a shallow set, 2 to 3 m deep, and no further.' },
};

/* ---------- the area count (0.5 m cells) ---------- */
const inR = (r, x, y) => x >= r.x0 && x < r.x1 && y >= r.y0 && y < r.y1;
function countAreas(step = 0.5) {
  const res = { levels: {}, open: {}, locked: {}, total: 0, openTotal: 0 };
  const cell = step * step;
  for (const L of LEVELS) {
    let tot = 0, op = 0;
    const openHere = OPEN.filter(o => o.level === L.id);
    const lockedHere = LOCKED.filter(z => z.levels[L.id]);
    for (let x = step / 2; x < 160; x += step) {
      const hb = HUB.hb(x) * (L.width || 1);
      for (let y = -20 + step / 2; y < 20; y += step) {
        if (Math.abs(y) >= hb) continue;
        if (L.only && !L.only.some(r => inR(r, x, y))) continue;
        if (L.voids && L.voids.some(r => inR(r, x, y))) continue;
        tot += cell;
        const water = L.water && L.water.some(r => inR(r, x, y));
        const o = water ? { id: 'dock' } : openHere.find(o => !o.inside && o.rects.some(r => inR(r, x, y)));
        if (o) { op += cell; res.open[o.id] = (res.open[o.id] || 0) + cell; continue; }
        const z = lockedHere.find(z => z.levels[L.id].some(r => inR(r, x, y)));
        const id = z ? z.id : 'rest_' + L.id; res.locked[id] = (res.locked[id] || 0) + cell;
      }
    }
    // A deckhouse counts with the deck it stands on; its own area is reported for the register.
    for (const o of openHere.filter(o => o.inside)) res.open[o.id] = o.rects.reduce((a, r) => a + (r.x1 - r.x0) * (r.y1 - r.y0), 0);
    res.levels[L.id] = { total: tot, open: op };
    res.total += tot; res.openTotal += op;
  }
  return res;
}
