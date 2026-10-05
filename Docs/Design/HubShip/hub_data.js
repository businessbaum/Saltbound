/* Saltbound hub ship: the numbers. One source for the build (HubShip_Data.json, written from this by
   Tools/design/render_hub_ship.py --data) and for the layout review (Tools/design/hub_review.py).
   Metres. x forward from the stern (0) to the stem (160 at the main deck), y to starboard (+) from the centreline,
   z up from the design waterline. (In Unreal: +X forward, +Y starboard, +Z up, 1 m = 100 uu; pivot at x 80, y 0, z 0.)

   D64 (owner review, 2026-10-05): boats come in from THE SIDE through hidden doors in the hull that open inwards like a double
   door, into a water hall across the bottom deck; the stern is a galleon's again (round, wide and high at the top, no dock
   mouth); cozy decks 3 m apart, every deck and room built; the hull planked and ported down to the water. */
'use strict';

const R = (x0, x1, y0, y1) => ({ x0, x1, y0, y1 });

const HUB = {
  name: 'Hollowkeel',
  L: 160, B: 40, sprit: 188, keel: -12,
  // The hull's half-width up its height amidships, [z, half-width]: a flat floor, a round bilge, widest at z 6, tumblehome to the
  // main deck's rail and on up the sterncastle (D64: the castle's top is wider than before, 17.4 at the taffrail). Scaled along
  // the ship by hb(x) / 20 and shaped at the ends by `form`.
  section: [[-12, 6], [-6, 16], [0, 19], [6, 20], [17.4, 18.8], [29.4, 17.4]],
  // The floors (their tops), 3 m apart (owner, D64: "cozy, not too big": 2.7 m clear under the beams for the 1.5 m player).
  Z: { deep: -4.5, hold: -1.5, quay: 1.5, lower: 4.5, middle: 7.5, upper: 10.5, gun: 13.5, main: 16.5,
       sc1: 19.5, sc2: 22.5, sc3: 25.5, poop: 28.5, fc1: 19.5, fcTop: 22.5 },
  deckH: 3, slab: 0.3,
  rail: 0.9,                                        // bulwarks and taffrail above their deck: under the 1.4 m player's eyes
  // The hull's form at the ends (D64: "more shape"). run: aft of x1 and below the counter the bottom narrows to the sternpost
  // (its half-width there `post` x the section's), blending to the full section between z blend[0] and the counter;
  // entry: forward of x0 and below z top the bow's bottom is finer than the deck outline (down to `fine` x at the stem).
  // stern: the stern face is round in plan, its corners `round` m forward of its middle; above the counter it overhangs the
  // sternpost (x post_x) and rakes aft 1 m by the taffrail.
  form: { run: { x1: 30, post: 0.06, pow: 1.6, blend: [2.5, 8.5] }, entry: { x0: 118, fine: 0.82, top: 6, pow: 1.4 },
          stern: { round: 2.6, counter: 8.5, post_x: 1.6, rake: 1.0 } },
  // The planking's sheer: the bands, wales and ports rise towards the ends (m at the stern and at the stem); the decks inside
  // stay level. aft/fore: where the rise starts.
  sheer: { stern: 2.2, aft: 44, bow: 2.6, fore: 118 },
  // The water hall (D64): across the ship on the bottom deck, sea water in it at the sea's level (the doors are no seal). Boats
  // come in with their masts up (owner, D64): the hall rises from the water to the gun deck (13.2 m clear; the first boat's
  // mast top is 10.9 m over the water), galleries on the lower, middle and upper decks over both quays. The water runs from side
  // to side between the quays.
  dock: { x0: 26, x1: 50, water: { x0: 30, x1: 46 }, floor: -4, ceiling: 'gun', mast_max: 12 },
  // The hidden doors (D64): a pair in each side, the hull's own planking, hinged at x0 and x1, swinging in 90 degrees into the
  // hall; from below the keel of a boat (z0) to just under the gun deck (z1), tall enough for a mast. Shut, nothing shows but a
  // seam in the planking and the hinge straps. They open for a boat that comes near (the dock watch), or with E at their
  // capstans; ropes from each leaf's head to its capstan.
  doors: { x0: 32, x1: 40, z0: -2.5, z1: 13.0, thick: 0.35, swing: 90, seconds: 10, open_m: 45,
           capstans: [[28, 16.5], [28, -16.5]] },
  ramp: { x0: 46, x1: 60, y0: -5, y1: 5, z0: -3, z1: 1.5 },     // the slipway, out of the water into the Slip's boat hall
  hall: { x0: 50, x1: 68, y0: -10, y1: 10 },        // the Slip's boat hall: the quay deck up to the gun deck (11.7 m clear)
  shop: { x0: 50, x1: 68, y0: 10, y1: 20 },         // the shipwright's shop, starboard of the hall
  stair: { x0: 50.5, x1: 56.0, y0: 16.5, y1: 19.0 }, // the dock stair: in the shop, up to the Tally House (+x); on the 0.5 m grid
  tally: { x0: 50, x1: 68, y0: 10, y1: 20 },        // the Tally House, lower deck, over the shop
  lane: { x0: 68, x1: 73, y0: 6, y1: 12 },          // lower deck, from the Tally House to the Well
  shaft: { x0: 78, x1: 90, y0: -6, y1: 6 },         // the Well: open from the Keelgate floor to the sky
  ring: { x0: 73, x1: 95, y0: -11, y1: 11 },        // its galleries (5 m wide) on the lower, middle, upper and gun decks
  chamber: { x0: 72, x1: 96, y0: -12, y1: 12 },     // the Keelgate chamber at the foot of the Well (quay deck)
  keelgate: { x: 84, y: 0, r: 3.0, ring: 4.0 },     // the ancient hatch in its floor
  hoist: { x0: 78.2, x1: 80.8, y0: 3.2, y1: 5.8 },  // the hoist cage, aft-starboard corner of the shaft
  // The Well's stairs (17 risers of 17.6 cm on 30 cm treads, a short landing at the top), spiralling up round the ring.
  flights: [
    { from: 'quay', to: 'lower', x0: 79, x1: 84.4, y0: 8, y1: 11, up: '-x' },
    { from: 'lower', to: 'middle', x0: 92, x1: 95, y0: -6, y1: -0.6, up: '-y' },
    { from: 'middle', to: 'upper', x0: 79, x1: 84.4, y0: -11, y1: -8, up: '-x' },
    { from: 'upper', to: 'gun', x0: 73, x1: 76, y0: 0.6, y1: 6, up: '+y' },
    { from: 'gun', to: 'main', x0: 84.6, x1: 90, y0: 8, y1: 11, up: '+x' },
  ],
  chandlery: { x0: 98, x1: 114, y0: -17.5, y1: -8, h: 4.2 },
  wing: 10,     // (kept for the room layout: the old wings' line, |y| 10)
  fcX: 134,     // the forecastle starts here
  // A galleon's stern rises in tiers: the quarterdeck over the first storey (x poopX..qdX, its deck at sc2), the poop over three
  // (x 0..poopX, its deck at poop). D64: the castle is a storey taller at 3 m a storey and wider at the top.
  poopX: 24, qdX: 48,
  // Stern galleries (D64): open balconies across the stern face at sc1 and sc2 (out `out` m, balustraded), running round the
  // corners into the quarter galleries, enclosed bays standing out from the sides (x0..x1, out m) over three storeys, domed.
  galleries: { levels: ['sc1', 'sc2'], out: 2.0, quarter: { x0: 2, x1: 13, out: 2.6, z0: 19.5, z1: 28.5 } },
  beakhead: { x0: 158, x1: 171, z: 18.5, halfW: 2.2, rails_to: 22.5, figure: [171.6, 0, 19.2] },
  masts: [
    { id: 'mizzen', name: 'Mizzen', x: 68, h: 54 },
    { id: 'main', name: 'Main', x: 100, h: 76 },
    { id: 'fore', name: 'Fore', x: 140, h: 64 },
  ],
  rig: { thick: [2.0, 0.7], top: { at: 0.38, w: 14 }, cross: { at: 0.68, w: 6.4 },
         yards: { at: [0.27, 0.47, 0.62, 0.8], half: [0.42, 0.35, 0.26, 0.18], scale: { main: 1, fore: 0.92, mizzen: 0.8 } },
         lateen: { mast: 'mizzen', len: 34, rake: 34 }, spritsail: { at: 0.6, half: 7.5 },
         channels: { mizzen: 12, main: 17, fore: 12 }, shrouds: { mizzen: 5, main: 8, fore: 6 }, pennant: { len: 16, hoist: 1.4 } },
  // The great wheel on the quarterdeck, lashed: it never turns. Its ropes run aft to the tiller of the one rudder.
  helm: { x: 38, y: 0, r: 1.8 },
  // One rudder on the sternpost (D64: the dock mouth no longer splits the stern): the blade x0..x1 at the heel, narrowing to
  // `head` at its top under the counter, its tiller in through the counter.
  rudder: { x0: -3.4, x1: 1.6, z0: -11, z1: 8.5, head: 1.4 },
  // She lies still because she is held: two bow cables from hawse holes to anchors ahead (the capstan heaves them), two stern
  // moorings from the quarters. from: on the hull; to: where it meets the water.
  cables: [{ id: 'bow', from: [151, 7, 20], to: [178, 13, 0] }, { id: 'bow', from: [151, -7, 20], to: [178, -13, 0] },
           { id: 'stern', from: [5, 17, 18], to: [-24, 34, 0] }, { id: 'stern', from: [5, -17, 18], to: [-24, -34, 0] }],
  // Where she lies at Saltbound Cove (world metres, north +X, east +Y; the pier is 0, 0): off the lagoon's channel east of the
  // island, her stern to the channel.
  berth: { id: 'SaltboundRoads', centreM: [81, 415], bowDeg: 92 },
  // The rooms of every deck (D64, owner: "build all the decks and rooms, you don't need to design every room"): outside the
  // open spaces each deck is two corridors fore and aft (|y| between spine[0] and spine[1]) and corridors across at `cross`
  // (cross_w m wide), with rooms between them `room` m long (split once more across where they are deeper than `deep` m); every
  // room has a door (door m wide) to a corridor or to the room in front of it. The locked zones below name the rooms' owners.
  rooms: { spine: [6.4, 8.6], cross: [10, 34, 58, 82, 106, 130, 151], cross_w: 2.2, room: [4.5, 6.5], deep: 8.5, door: 1.0,
           // walkways (no rooms): the Water Hall's galleries over both quays, railed on the water side
           walks: [{ levels: ['lower', 'middle', 'upper'], rects: [R(26, 30, -20, 20), R(46, 50, -20, -5), R(46, 50, 5, 20)] }] },
  // How the open spaces meet (D64): 'open' (no wall between them), 'door' (a wall with a doorway), anything else a wall.
  links: [['dock', 'slip', 'open'], ['dock_g', 'tally', 'door'], ['tally', 'lane', 'door'], ['lane', 'well_l', 'open'],
          ['slip', 'keelgate', 'wall']],
};

/** Half-beam of the deck outline at x (D64: the stern is full and round, wider at the top than before). */
HUB.hb = function (x) {
  if (x < 0 || x > 160) return 0;
  if (x < 12) return 18.6 + 1.4 * (1 - Math.pow((12 - x) / 12, 2));
  if (x <= 108) return 20;
  const t = (x - 108) / 52; return 20 * Math.pow(Math.max(0, 1 - t * t), 0.55);
};
/** The deck outline as a polygon (x, y) from the stern round the bow. */
HUB.outline = function (k = 1, n = 80) {
  const s = [], p = [];
  for (let i = 0; i <= n; i++) { const x = i / n * 160; s.push([x, HUB.hb(x) * k]); p.push([x, -HUB.hb(x) * k]); }
  return s.concat(p.reverse());
};
/** The side profile (x, z): taffrail, poop, quarterdeck, waist, forecastle, stem, keel, sternpost, counter. */
HUB.profile = function () {
  const Z = HUB.Z, r = HUB.rail;
  return [[-1, Z.poop + r + 2.0], [HUB.poopX, Z.poop + r], [HUB.poopX + 0.6, Z.sc2 + r], [HUB.qdX, Z.sc2 + r], [HUB.qdX + 0.6, Z.main + r],
    [134, Z.main + r], [134.6, Z.fcTop + r], [161.5, Z.fcTop + r], [161, 18], [159.8, 12], [157.5, 4], [153, -4], [145, -10], [132, -12],
    [12, -12], [4.5, -11.4], [2.2, -9], [1.6, -4], [1.4, 2], [0.8, 6], [0, 8.5]];
};

const ST = { OPEN: 'open', LOCKED: 'locked', WATER: 'water' };
const AFT = R(0, HUB.poopX, -20, 20), QD = R(HUB.poopX, HUB.qdX, -20, 20);
const DOCK = R(HUB.dock.x0, HUB.dock.x1, -20, 20), WATER = R(HUB.dock.water.x0, HUB.dock.water.x1, -20, 20);
const OVER_RAMP = R(46, 50, -5, 5);     // over the slipway's foot: open from the water to the ceiling (masts)

/** Levels (floors) of the ship. width: share of the deck half-beam at that floor where the hull is narrower (the build uses the
 *  hull's form itself). only: where the floor is at all; voids: where it is not (open to the floor below); water: sea in it. */
const LEVELS = [
  { id: 'poop', name: 'Poop (deck)', z: HUB.Z.poop, only: [AFT], deck: true },
  { id: 'sc3', name: 'Sterncastle, third storey', z: HUB.Z.sc3, only: [AFT] },
  { id: 'sc2', name: 'Sterncastle, second storey (quarterdeck)', z: HUB.Z.sc2, only: [AFT, QD] },
  { id: 'sc1', name: 'Sterncastle, first storey', z: HUB.Z.sc1, only: [AFT, QD] },
  { id: 'fcTop', name: 'Forecastle deck', z: HUB.Z.fcTop, only: [R(134, 160, -20, 20)], deck: true },
  { id: 'fc1', name: 'Forecastle, storey', z: HUB.Z.fc1, only: [R(134, 160, -20, 20)] },
  { id: 'main', name: 'Main deck', z: HUB.Z.main, voids: [HUB.shaft], deck: true },
  { id: 'gun', name: 'Gun deck', z: HUB.Z.gun, voids: [HUB.shaft] },
  { id: 'upper', name: 'Upper deck', z: HUB.Z.upper, voids: [WATER, OVER_RAMP, HUB.hall, HUB.shaft] },
  { id: 'middle', name: 'Middle deck', z: HUB.Z.middle, voids: [WATER, OVER_RAMP, HUB.hall, HUB.shaft] },
  { id: 'lower', name: 'Lower deck', z: HUB.Z.lower, voids: [WATER, OVER_RAMP, HUB.hall, HUB.shaft] },
  // The slipway runs from the water through the forward quay (water there) up a slot in the Slip's floor (the ramp is the floor).
  { id: 'quay', name: 'Dock deck', z: HUB.Z.quay, water: [WATER, R(46, 50, -5, 5)], slots: [R(50, 60, -5, 5)] },
  // The slipway runs up through the hold's height under the Slip (x 50..60): walled off from the holds (void_edge).
  { id: 'hold', name: 'Hold', z: HUB.Z.hold, voids: [DOCK, R(50, 60, -5, 5)], void_edge: 'wall' },
  { id: 'deep', name: 'The Deep', z: HUB.Z.deep, voids: [DOCK] },
];

/** Open at the start: about a fifth. svc = the service badge (S J Q E). */
const OPEN = [
  { id: 'dock', name: 'Water Hall', sub: 'water and quays', level: 'quay', rects: [R(26, 30, -20, 20), R(46, 50, -20, -5), R(46, 50, 5, 20)], water: true },
  { id: 'dock_g', name: 'Water Hall galleries', level: 'lower', rects: [R(46, 50, 5, 20)] },
  { id: 'slip', name: 'The Slip', sub: 'boat hall and shop', svc: 'S', level: 'quay', rects: [HUB.hall, HUB.shop] },
  { id: 'keelgate', name: 'The Keelgate', sub: 'chamber at the foot of the Well', svc: 'Q', level: 'quay', rects: [HUB.chamber] },
  { id: 'tally', name: 'Tally House', sub: 'jobs and pay', svc: 'J', level: 'lower', rects: [HUB.tally], void_edge: 'wall' },
  { id: 'lane', name: 'Starboard lane', level: 'lower', rects: [HUB.lane] },
  { id: 'well_l', name: 'The Well', sub: 'galleries, stairs, hoist', level: 'lower', rects: [HUB.ring] },
  { id: 'well_m', name: 'The Well', level: 'middle', rects: [HUB.ring] },
  { id: 'well_u', name: 'The Well', level: 'upper', rects: [HUB.ring] },
  { id: 'well_g', name: 'The Well', level: 'gun', rects: [HUB.ring] },
  { id: 'deck', name: 'Main deck', sub: 'the Waist', level: 'main', rects: [R(HUB.qdX, 116, -20, 20)] },
  { id: 'chandlery', name: 'Chandlery', sub: 'deckhouse', svc: 'E', level: 'main', rects: [R(98, 114, -17.5, -8)], inside: 'deck' },
];

/** Planned, locked, not designed: working names only. gate = how its corridors meet the open spaces (see GATES).
 *  Every room of these is built (D64), behind its gates; on the levels in `air` the zone is open deck under the sky (no rooms). */
const LOCKED = [
  { id: 'L1', name: 'Sterncastle', levels: { main: [AFT, QD], sc1: [AFT, QD], sc2: [AFT, QD], sc3: [AFT], poop: [AFT] }, air: { sc2: [QD], poop: [AFT] }, gate: 'G1', sees: 'Tall doors at the aft end of the main deck, barred from inside. The stern windows glow at night; the great wheel on the quarterdeck is lashed and never turns.' },
  { id: 'L2', name: 'Stern quarters', levels: { lower: [R(0, 30, -20, 20)], middle: [R(0, 26, -20, 20), R(26, 30, -20, 20)] }, gate: 'G1', sees: 'Doors off the Water Hall’s aft quay, and the aft galleries over the water with their shutters.' },
  { id: 'L3', name: 'Stern stores', levels: { quay: [R(0, 26, -20, 20)] }, gate: 'G4', sees: 'Shuttered bays along the Water Hall’s aft quay.' },
  { id: 'L4', name: 'Timber store', levels: { quay: [R(50, 68, -20, -10)] }, gate: 'G6', sees: 'Iron grilles on the port side of the Slip; stacked timber behind them.' },
  { id: 'L5', name: 'Keel Walk', levels: { quay: [R(68, 72, -20, 20)] }, gate: 'G1', sees: 'A heavy door in the Slip’s forward wall and another in the Keelgate chamber: the first shortcut to open.' },
  { id: 'L6', name: 'The Holds', levels: { quay: [R(96, 160, -20, 20), R(72, 96, 12, 20), R(72, 96, -20, -12)], hold: [R(0, 160, -20, 20)] }, gate: 'G6', sees: 'Iron grilles in the Keelgate chamber’s forward wall; padlocked cargo hatches on the main deck.' },
  { id: 'L7', name: 'Purser’s stores', levels: { lower: [R(68, 73, -11, 6), R(68, 73, 12, 20)] }, gate: 'G1', sees: 'A door in the Tally House’s forward wall, by the pay booth.' },
  { id: 'L8', name: 'The Ropewalk', levels: { lower: [R(30, 150, -20, -9)] }, gate: 'G1', sees: 'Double doors off the Well’s port gallery; the creak of a rope being laid.' },
  { id: 'L9', name: 'Mess and galley', levels: { lower: [R(95, 160, -9, 20), R(73, 95, 11, 20)] }, gate: 'G1', sees: 'A door off the Well’s forward gallery; the galley chimney smokes on the main deck.' },
  { id: 'L10', name: 'Guild rooms', levels: { middle: [R(46, 73, -20, 20)], lower: [R(46, 50, -20, 5)] }, gate: 'G1', sees: 'Doors with painted crests off the Well’s aft gallery; their galleries over the Water Hall’s forward quay.' },
  { id: 'L11', name: 'Lantern Market', levels: { middle: [R(95, 160, -20, 20), R(73, 95, 11, 20), R(73, 95, -20, -11)] }, gate: 'G2', sees: 'A rope across the forward gallery with a board: “Market closed”; light and voices behind shutters.' },
  { id: 'L12', name: 'Archive', levels: { upper: [R(0, 73, -20, 20)] }, gate: 'G1', sees: 'A door with a brass plate off the Well’s aft gallery; its galleries high over the Water Hall.' },
  { id: 'L13', name: 'Inn and bunkrooms', levels: { upper: [R(95, 160, -20, 20), R(73, 95, 11, 20), R(73, 95, -20, -11)] }, gate: 'G1', sees: 'A door off the forward gallery; laughter behind it in the evening.' },
  { id: 'L14', name: 'Forecastle and bow', levels: { main: [R(134, 160, -20, 20)], fc1: [R(134, 160, -20, 20)], fcTop: [R(134, 160, -20, 20)] }, air: { fcTop: [R(134, 160, -20, 20)] }, gate: 'G3', sees: 'Chained ladders and closed doors at the forward end of the main deck; the bowsprit beyond.' },
  { id: 'L15', name: 'Cargo deck', levels: { main: [R(116, 134, -20, 20)] }, air: { main: [R(116, 134, -20, 20)] }, gate: 'G2', sees: 'The forward end of the main deck behind a rope and a board (“Cargo work, keep clear”): lashed cargo under nets, the padlocked hatches, the ship’s boats.' },
  { id: 'L16', name: 'Masts and tops', levels: {}, gate: 'G3', sees: 'Shrouds whose ratlines start out of reach; the tops’ lanterns at night.' },
  { id: 'L17', name: 'The Deep', levels: { deep: [R(0, 160, -20, 20)] }, gate: 'G5', sees: 'The Keelgate: the ancient seal in the floor at the foot of the Well, faintly lit from inside.' },
  { id: 'L18', name: 'Sail lofts and workshops', levels: { gun: [R(0, 160, -20, 20)] }, gate: 'G1', sees: 'Doors off the Well’s gun-deck gallery; canvas and tar.' },
];

const GATES = {
  G1: { name: 'Barred door', why: 'Somebody’s room or business, not open to you yet.', how: 'A timber door with an iron bar and a padlock. E: a short line of why.', prompt: 'Try the door', line: 'Barred from inside.' },
  G2: { name: 'Rope and board', why: 'Work going on, or closed for now.', how: 'A rope across the way on stanchions (or between the door posts), a hand-painted board on it.', prompt: 'Read the board', line: 'Closed for now.' },
  G3: { name: 'Chained ladder or hatch', why: 'Up or down: masts, forecastle, cargo hatches.', how: 'A chain through the rungs or the grating, padlocked.', prompt: 'Try the chain', line: 'Chained and padlocked.' },
  G4: { name: 'Shutters', why: 'Windows and bays of locked rooms.', how: 'Closed shutters: nothing inside to see, nothing inside to build.', prompt: 'Try the shutters', line: 'Shuttered from inside.' },
  G5: { name: 'Ancient seal', why: 'The lost civilisation: the main quest.', how: 'Pale seamless stone, turquoise lines, no handle, no lock.' },
  G6: { name: 'Iron grille', why: 'A glimpse: stores and holds.', how: 'Bars you see through into a shallow set, 2 to 3 m deep, and no further.', prompt: 'Look through the grille', line: 'Stacked timber, and no way in.' },
};

/* ---------- the spaces in detail: NPC spots, interaction points, furniture (for the build) ----------
   Plan positions in metres (ship frame). z and h are metres above that space's floor. face: the way an NPC looks, in
   degrees in plan: 0 = towards the bow (+x), 90 = to starboard (+y) (= Unreal yaw in the ship's frame).
   Boxes: x0..x1, y0..y1, height h. Wall items: wall 'x=64' (a plane at x 64) or 'y=19', with their extent and z0..z1. */
const SPACES = {
  dock: {
    level: 'quay', floor: 1.5, name: 'The Water Hall',
    props: [
      { id: 'water', x0: 30, x1: 46, note: 'sea water from door to door, its floor z -4; the quays’ faces timber on piles; open up to the gun deck (13.2 m over the water)' },
      { id: 'quays', note: 'x 26..30 (aft) and 46..50 (forward) at +1.5, from side to side; the forward quay is cut by the slipway (y -5..5)' },
      { id: 'doors', note: 'hub.doors: a pair in each side at x 32..40, 15.5 m tall; shut, a seam in the planking; open, the leaves stand in the water along x 32 and x 40, between the galleries' },
      { id: 'bollards', every: 5, along: [[30, -17, 17], [46, -17, -6], [46, 6, 17]], r: 0.3, h: 0.6, note: 'on the quays’ edges (x, y from, y to)' },
      { id: 'rung_ladders', at: [[30, 14], [30, -15], [46, -8], [46, 17]], note: 'from the water onto the quays (the D54 ladder); [x of the edge, y]' },
      { id: 'fitting_berth', x: 43.9, y0: 6, y1: 16, heading: -90, note: 'along the forward quay (its edge x 46), bow to port: a boat that came in by the starboard doors slides alongside; the quay to her starboard (reach 2.1 m)' },
      { id: 'fleet_berths', berths: [{ x: 43.9, y0: -16, y1: -6, boat: '10 m', reach: 2.1 }, { x: 32.3, y0: -7, y1: 7, boat: '14 m', reach: -2.3 }], heading: -90, note: 'bow to port: the 10 m along the forward quay’s port side (the quay to her starboard; the fore leaves never swing past x 41.4), the 14 m along the middle of the aft quay (the quay to her port side): the aft leaves stand open at y 12.3..16.3 and sweep x 30.6..36.5 beyond |y| 12' },
      { id: 'crane', beam: [50, 44.2, 11, 12.6], hook_rope_x: 44.6, hand_chain: [45.8, 11], note: 'a runway beam under the ceiling from the forward wall out over the fitting berth (above the galleries and a mast), a trolley with a block; its hand chain hangs just off the quay’s edge' },
      { id: 'pillars', at: [[29.8, -10.5], [29.8, -4.5], [29.8, 4.5], [29.8, 10.5], [46.2, -15], [46.2, -10], [46.2, 10], [46.2, 15]], size: 0.36, note: 'posts under the galleries’ edges, from the quays to the gun deck; clear of the bollards, ladders, capstans and the berth board' },
      { id: 'galleries', levels: ['lower', 'middle', 'upper'], aft: [26, 30], fore: [46, 50], note: 'balconies over both quays on three decks, balustraded on the water side; the lower forward-starboard one (y 5..20) is open, with a door into the Tally House; the others belong to the rooms behind them' },
      { id: 'lanterns', at: [[28, 10, 2.6], [28, -10, 2.6], [48, 12, 2.6], [48, -12, 2.6], [29.6, 0, 5.6], [46.4, 12, 5.6], [29.6, 0, 8.6], [46.4, -12, 8.6]], hook_z: 3.0, note: 'under the galleries over the quays, and from the galleries’ edges; none hang over the water or the slipway (masts)' },
    ],
    gates: ['L3 shuttered bays along the aft quay (x 26)', 'L2 doors at the aft quay’s ends'],
  },
  slip: {
    svc: 'S', level: 'quay', floor: 1.5, name: 'The Slip (shipwright)',
    npc: { x: 57.4, y: 13.2, face: 180, note: 'behind the counter across the shop, facing aft to the door from the quay' },
    e: [
      { id: 'counter', x: 54.6, y: 13.2, prompt: 'Talk' },
      { id: 'plans', x: 50.6, y: 12, prompt: 'Look at the plans' },
      { id: 'display_boat', x: 61.5, y: 4.7, prompt: 'Look her over', note: 'beside her, at the head of the slipway' },
      { id: 'fitting_berth', x: 47, y: 13.5, prompt: 'Read the berth board', note: 'a board at the quay’s edge by the fitting berth, between two bollards' },
    ],
    props: [
      { id: 'hall', x0: 50, x1: 68, y0: -10, y1: 10, h: 11.7, note: 'open to the Water Hall’s forward quay along x 50; its ceiling is the gun deck (a boat on the slipway keeps her mast)' },
      { id: 'shop', x0: 50, x1: 68, y0: 10, y1: 20, h: 3, note: 'open to the hall along y 10; the Tally House above it; the dock stair in its aft-starboard corner' },
      { id: 'counter', x0: 55.4, x1: 56.4, y0: 11.2, y1: 15.6, h: 0.95, note: 'across the shop facing aft' },
      { id: 'plans_board', wall: 'x=50', y0: 10.6, y1: 13.6, z0: 0.9, z1: 2.4, note: 'pinned plans of the upgrades, by the door from the quay' },
      { id: 'half_models', wall: 'x=68', y0: 12, y1: 18, z0: 1.9, z1: 2.5, note: 'three half-hull models: the boats for sale' },
      { id: 'workbench', x0: 60, x1: 64, y0: 17.6, y1: 19, h: 0.9 },
      { id: 'timber_racks', wall: 'x=68', y0: -5.4, y1: 5.4, z0: 0.6, z1: 3.5, note: 'four rows of planks on iron brackets, either side of the mizzen’s foot (the mast comes down through the forward wall to its step), clear of the Keel Walk’s doors' },
      { id: 'display_boat', x0: 52, x1: 66, y: 7.5, beam: 4.4, keel_z: 0.6, note: 'the 14 m cutter (D51), unrigged, bow forward, on cradles at x 55, 59, 63 on the starboard side of the hall: the slipway’s way (y -5..5) stays clear' },
      { id: 'mast_on_trestles', x0: 51.5, x1: 64.5, y: -7, z: 1.1, note: 'her mast lying on two trestles, port side (the grille to the timber store behind it stays in reach)' },
      { id: 'lanterns', at: [[62, 7.5, 7], [62, -5, 7], [56, -7.5, 7], [62, 15, 2.2]], hook_z: 11.7, note: 'none over the slipway (masts)' },
      { id: 'sign', x: 50, y: 0, z: 10.9, text: 'SHIPWRIGHT', note: 'over the hall mouth under the gun deck, facing aft over the Water Hall (above a mast on the slipway)' },
    ],
    gates: ['L5 door in the forward wall (x 68)', 'L4 grilles on the port side (y -10)'],
  },
  tally: {
    svc: 'J', level: 'lower', floor: 4.5, name: 'Tally House (side jobs)',
    npc: { x: 60, y: 18.8, face: 270, note: 'inside the pay booth, behind the hatch, facing into the room' },
    e: [
      { id: 'pay_hatch', x: 60, y: 16.6, prompt: 'Talk' },
      { id: 'job_board', x: 66.4, y: 14.6, prompt: 'Read the board' },
      { id: 'weighing_bench', x: 64.5, y: 17.6, prompt: 'Deliver goods' },
    ],
    props: [
      { id: 'room', x0: 50, x1: 68, y0: 10, y1: 20, h: 3 },
      { id: 'stair_hole', x0: 50.5, x1: 56.0, y0: 16.5, y1: 19.0, note: 'the dock stair arrives here, a rail round it' },
      { id: 'gallery_door', wall: 'x=50', y0: 11, y1: 12.2, note: 'out onto the Water Hall gallery' },
      { id: 'lane_door', wall: 'x=68', y0: 10.2, y1: 12, note: 'to the lane and the Well' },
      { id: 'job_board', wall: 'x=68', y0: 12.8, y1: 17.2, z0: 0.6, z1: 1.9, note: 'six slips; header board TALLY over it (under the beams); a chalk slate under it' },
      { id: 'pay_booth', x0: 57.6, x1: 62.6, y0: 17.4, y1: 20, h: 2.4, door: [18.0, 19.0], note: 'timber booth on the hull wall, 1.8 m forward of where the dock stair arrives; grilled hatch x 58.5..61.5, z 1.0..1.8; ledge at z 1.0; sign PAY; its door in the forward side' },
      { id: 'weighing_bench', x0: 63.4, x1: 67, y0: 18.6, y1: 20, h: 0.9, note: 'a big balance on it; sacks and crates beside' },
      { id: 'salt_cakes', x0: 51.2, x1: 52, y0: 10.6, y1: 11.4, h: 0.9, note: 'stacked by the windows over the Slip, out of the stair’s way' },
      { id: 'table', x0: 56, x1: 64, y0: 12.6, y1: 13.8, h: 0.8, note: 'benches either side at y 14.2..14.6 and y 11.8..12.2, h 0.45' },
      { id: 'hull_ports', wall: 'y=20', spans: [[62.4, 64], [65.2, 66.8]], z0: 0.8, z1: 1.8 },
      { id: 'hall_windows', wall: 'y=10', spans: [[54, 57], [58, 61], [62, 65]], z0: 0.8, z1: 2.2, note: 'look down into the Slip' },
      { id: 'lanterns', at: [[64, 15, 2.2], [59.5, 13.2, 2.2], [53, 13, 2.2], [70.5, 9, 2.2]], hook_z: 2.7 },
    ],
    gates: ['L7 door in the forward wall (x 68, y 18.6..19.8)'],
  },
  well: {
    level: 'lower..main', name: 'The Well',
    props: [
      { id: 'shaft', x0: 78, x1: 90, y0: -6, y1: 6, note: 'open from the Keelgate floor (+1.5) to the sky; rails 1.0 m round it on every gallery' },
      { id: 'galleries', x0: 73, x1: 95, y0: -11, y1: 11, note: 'lower, middle, upper and gun deck; walls on their outer edges with the gates' },
      { id: 'hoist', x0: 78.2, x1: 80.8, y0: 3.2, y1: 5.8, stops: ['quay', 'lower', 'main'], door: 'aft (x 78.2)', note: 'cage on ropes from the gantry, between two guide posts at its aft corners; gates in the rails at its stops' },
      { id: 'counterweight', x0: 78.2, x1: 79.0, y0: 1.6, y1: 2.8, h: 1.5, note: 'its rope over the gantry beam’s second sheave; at the bottom of its run it sits in a railed bay on the chamber floor' },
      { id: 'gantry', note: 'A-frame over the opening on the main deck, x 76.5..91.5 at y 0, top 7.5 m above the deck, a beam out to the hoist with two sheaves' },
      { id: 'coaming', h: 0.3, note: 'a raised edge round the opening on the main deck, under the rail' },
      { id: 'lanterns', note: 'one on each gallery corner, a ring lantern in the shaft at the middle deck on a chain from the gantry’s apex' },
    ],
  },
  keelgate: {
    svc: 'Q', level: 'quay', floor: 1.5, name: 'The Keelgate (main quest)',
    npc: { x: 75.8, y: -9.4, face: 48, note: 'behind the reading desk by the port wall' },
    e: [
      { id: 'talk', x: 76.2, y: -5.6, prompt: 'Talk' },
      { id: 'gate', x: 81, y: 1.7, prompt: 'Look at the gate' },
      { id: 'hoist', x: 77.4, y: 4.5, prompt: 'Ride the hoist' },
    ],
    props: [
      { id: 'chamber', x0: 72, x1: 96, y0: -12, y1: 12, h: 3, note: 'the middle 12 x 12 m (the shaft) open 15 m up to the sky' },
      { id: 'keelgate', x: 84, y: 0, ring_r: 4.0, disc_r: 3.0, raised: 0.12, note: 'pale seamless ancient stone; turquoise lines: circles r 1.0 and 2.1, seven radial lines r 1.0..2.8; seven empty sockets at r 3.5' },
      { id: 'desk', x0: 74, x1: 77.5, y0: -8.4, y1: -6.6, h: 0.85, chair: [75.4, 76.2, -10.4, -9.6], note: 'books, a brass lamp, rubbings' },
      { id: 'shelves', wall: 'y=-12', x0: 83.8, x1: 94, depth: 0.6, z0: 0, z1: 2.6, note: 'salvaged ancient pieces in jars; about a quarter faintly glowing' },
      { id: 'rubbings', wall: 'x=96', y0: -2.8, y1: 3, z0: 1.2, z1: 2.4, note: 'paper rubbings of the gate’s lines' },
      { id: 'lanterns', at: [[94, 0, 2.2], [84, -10, 2.2]], hook_z: 2.7 },
    ],
    gates: ['L5 door in the aft wall (x 72)', 'L6 grilles in the forward wall (x 96)', 'L17 the Keelgate itself (G5)'],
  },
  chandlery: {
    svc: 'E', level: 'main', floor: 16.5, name: 'Chandlery (equipment)',
    npc: { x: 105, y: -9.6, face: 90, note: 'behind the counter, facing out to the open deck and the main mast' },
    e: [
      { id: 'counter', x: 105, y: -6.6, prompt: 'Talk' },
      { id: 'try_spyglass', x: 106, y: -3.6, prompt: 'Try the spyglass' },
      { id: 'try_compass', x: 110.4, y: -3.6, prompt: 'Try the compass' },
    ],
    props: [
      { id: 'deckhouse', x0: 98, x1: 114, y0: -17.5, y1: -8, h: 4.2, ridge: 5.4, note: 'pitched roof, ridge along x at y -12.75' },
      { id: 'counter_front', x0: 99.5, x1: 110, y0: -8.4, y1: -8, h: 0.9, top: 0.95, note: 'the front wall is the counter; posts at x 98..99.5, 110..111, 113..114' },
      { id: 'door', wall: 'y=-8', x0: 111, x1: 113 },
      { id: 'awning', x0: 99, x1: 110.5, from: [-8, 3.3], to: [-5.8, 2.75], note: 'canvas in muted oxblood and cream stripes on two posts at y -5.8' },
      { id: 'sign', wall: 'y=-8', x0: 101, x1: 109, z0: 3.5, z1: 4.4, text: 'CHANDLER' },
      { id: 'shelves', runs: [[98.5, 113.5, -17.2, -16.6], [98.3, 98.9, -16.6, -9.2]], h: 3.6, note: 'along the back wall and the aft wall; tins, jars, coils, boxes; a ladder on a rail' },
      { id: 'display_table', x0: 103, x1: 108, y0: -14.5, y1: -12.5, h: 0.85, note: 'compass, spyglass, lantern; room for more' },
      { id: 'window', wall: 'x=114', y0: -15, y1: -12.5, z0: 0.9, z1: 2.3 },
      { id: 'beams', at_x: [102, 106, 110], z: 3.9, note: 'stock hangs from them: lanterns, coils, a net' },
      { id: 'tripod_spyglass', x: 106, y: -4.5, h: 1.15 },
      { id: 'binnacle', x: 110, y: -4.5, h: 1.0 },
      { id: 'fife_rail', x0: 98, x1: 102, y0: -2, y1: 2, h: 0.9, note: 'round the main mast (x 100, r 1.0), with belaying pins' },
      { id: 'barrels_crates', x0: 112, x1: 114, y0: -7.6, y1: -6 },
    ],
    gates: [],
  },
  main_deck: {
    level: 'main', floor: 16.5,
    props: [
      { id: 'bulwark', note: 'round the hull edge from x 48, 0.9 m with a cap rail; inner face at 0.965 x the half-beam' },
      { id: 'stair_hatch', x0: 84.6, x1: 90, y0: 8, y1: 11, note: 'the last flight from the gun deck arrives here (starboard of the Well), a hooded companion with a rail' },
      { id: 'masts', at: [[68, 0], [100, 0], [140, 0]], r: 1.0, note: 'fife rails 4 x 4 m round each; channels on the hull sides for the shrouds' },
      { id: 'cargo_rope', x: 116, every: 4, note: 'G2: a rope across the deck on stanchions every 4 m, a board at y 0 (Cargo work, keep clear)' },
      { id: 'cargo_hatches', boxes: [[118, 122, -3, 3], [123, 127, -3, 3]], note: 'gratings, padlocked (G3)' },
      { id: 'ships_boats', boxes: [[119, 129, 12, 15], [119, 129, -15, -12]], note: 'on chocks, under the cargo rope (locked zone L15)' },
      { id: 'capstan', x: 131.5, y: 0, r: 1.1, note: 'heaves the bow cables, led aft from the hawse holes under the forecastle' },
      { id: 'galley_chimney', x: 120.8, y: 8, h: 4.5, note: 'smokes: the mess below (L9)' },
      { id: 'bowsprit', from: [154, 0, 23.5], to: [188, 0, 30.5], note: 'absolute z' },
      { id: 'bobstay', from: [186, 0, 30], to: [158.5, 0, 5], note: 'holds the bowsprit down against the forestays: from its end to the stem' },
      { id: 'lanterns', at: [[86, 7.6, 2.2], [99, -5.6, 2.6], [110.5, -5.6, 2.6], [100, 2.3, 2.0]], hook_z: 2.8 },
    ],
  },
};

/* Gates the room layout cannot place itself (plan positions, metres): the cargo rope, the cargo hatch, the forecastle's ladders,
   the shrouds, the Keelgate. The rest stand where a locked zone's corridor meets an open space (hub_layout.gate_marks). */
const GATE_MARKS = [
  { level: 'quay', x: 84, y: 0, g: 'G5', zone: 'L17' },
  { level: 'main', x: 116, y: 0, g: 'G2', zone: 'L15', line: 'Cargo work, keep clear.' }, { level: 'main', x: 122, y: 0, g: 'G3', zone: 'L6' },
  { level: 'main', x: 134, y: 15, g: 'G3', zone: 'L14' }, { level: 'main', x: 134, y: -15, g: 'G3', zone: 'L14' },
  { level: 'main', x: 68, y: 17.6, g: 'G3', zone: 'L16' }, { level: 'main', x: 100, y: 17.6, g: 'G3', zone: 'L16' },
];
const WHERE = {
  L1: 'Main deck aft and the sterncastle over it: the great cabin, the stern galleries, the quarterdeck, the poop',
  L2: 'Lower and middle deck aft, over the Water Hall’s aft quay and the stern', L3: 'Dock deck aft of the Water Hall', L4: 'Dock deck, port of the Slip',
  L5: 'Dock deck, between the Slip and the Keelgate', L6: 'Dock deck forward and round the Keelgate chamber; the hold under everything', L7: 'Lower deck, between the Tally House and the Well',
  L8: 'Lower deck, port side, 120 m long', L9: 'Lower deck forward', L10: 'Middle deck aft (over the Water Hall and the Slip)',
  L11: 'Middle deck forward', L12: 'Upper deck aft', L13: 'Upper deck forward and round the Well', L14: 'Bow: the forecastle and the bowsprit',
  L15: 'Main deck between the Waist and the forecastle', L16: 'Three masts, their tops and yards', L17: 'The Deep, under the waterline, behind the Keelgate',
  L18: 'Gun deck, all of it but the Well’s gallery',
};

/* The outside, for the build (metres, absolute z). */
const OUTSIDE = {
  hull_section: 'Planked all the way down (D64: no pale ancient band outside; the ancient hull shows only inside, at the Keelgate and the Deep): flat bottom +/-6 m at the keel (z -12), +/-16 m at z -6, +/-19 m at the waterline, widest +/-20 m at z +6, tumblehome to +/-18.8 m at the main deck rail and +/-17.4 m at the taffrail. Fine at the ends below the water (hub.form): the run aft narrows to the sternpost, the entry forward is finer than the deck outline.',
  stern: 'A galleon’s stern (D64): the sterncastle rises in two tiers, the quarterdeck over x 24..48 (deck 22.5) and the poop over x 0..24 (three storeys, deck 28.5). Below the counter (z 8.5) the hull narrows to the sternpost and the one rudder; above it the stern is full width, round in plan (its corners 2.6 m forward of its middle) and leans aft a little. On its face: rows of tall cabin windows on each storey, open stern galleries with balustrades at sc1 and sc2 running round the corners into the quarter galleries (x 2..13, standing out 2.6 m, three storeys, domed), the name in carved brass letters under the taffrail, three great lanterns on the taffrail. The tiller comes in through the counter; the lashed great wheel is on the quarterdeck.',
  sides: 'Dark tarred planking in strakes about 0.55 m wide with a lively sheer (hub.sheer: up 2.2 m at the stern, 2.6 m at the stem); heavy wales in a belt at the waterline and at the deck lines, standing out like mouldings; ochre bands; lidded square ports (1 m) on every deck below the main deck down to the dock deck, cream-framed windows above it; most shut, some lit at night. Channels with deadeyes at each mast. The hidden doors at x 32..40 on both sides, from under the water to 13 m: shut, the planking, wales and painted port lids run on across them; only a seam and the iron hinge straps show.',
  masts: 'x 68, 100, 140 (hub.rig in numbers); 54, 76, 64 m above the main deck; the mizzen’s lowest yard a lateen yard; every sail furled on its yard; shrouds to the channels, stays forward, a long oxblood-and-cream pennant at the main truck.',
  bow: 'Forecastle x 134..160, a storey over the main deck with a rail; a plain stem with two hawse holes and their cables to the anchors ahead; the beakhead out from the stem under the bowsprit (a grated platform, headrails curving up to the forecastle, a carved figurehead); the bowsprit from (154, 23.5) to (188, 30.5) with its spritsail yard, held down by its bobstay.',
  colours: 'Hull planking dark tarred brown; ochre bands, muted; window frames cream; carvings and the name brass, muted; decks teak and its light and dark variants; canvas; iron.',
};

/* ---------- the area count (0.5 m cells, on the deck outline; the build uses the hull's form) ---------- */
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
        if (L.water && L.water.some(r => inR(r, x, y))) continue;
        tot += cell;
        const o = openHere.find(o => !o.inside && o.rects.some(r => inR(r, x, y)));
        if (o) { op += cell; res.open[o.id] = (res.open[o.id] || 0) + cell; continue; }
        const z = lockedHere.find(z => z.levels[L.id].some(r => inR(r, x, y)));
        const id = z ? z.id : 'rest_' + L.id; res.locked[id] = (res.locked[id] || 0) + cell;
      }
    }
    for (const o of openHere.filter(o => o.inside)) res.open[o.id] = o.rects.reduce((a, r) => a + (r.x1 - r.x0) * (r.y1 - r.y0), 0);
    res.levels[L.id] = { total: tot, open: op };
    res.total += tot; res.openTotal += op;
  }
  return res;
}

/** Everything above as plain data (the build's scripts read HubShip_Data.json, written from this by render_hub_ship.py). */
function exportData() {
  const hub = JSON.parse(JSON.stringify(HUB));
  hub.outline_deck = HUB.outline(1, 80).map(([x, y]) => [+x.toFixed(2), +y.toFixed(3)]);
  hub.profile = HUB.profile();
  const a = countAreas(), r = o => Object.fromEntries(Object.entries(o).map(([k, v]) => [k, Math.round(v)]));
  return {
    _comment: 'Saltbound hub ship, design data (generated from Docs/Design/HubShip/hub_data.js; do not edit, change hub_data.js and run Tools/design/render_hub_ship.py --data).',
    frame: { units: 'metres', x: 'forward from the stern (0) to the stem (160)', y: 'to starboard', z: 'up from the design waterline',
      unreal: 'cm; +X forward, +Y starboard, +Z up; the ship actor pivot at x 80, y 0, z 0: X_cm = (x - 80) * 100, Y_cm = y * 100, Z_cm = z * 100',
      face: 'degrees in plan: 0 = towards the bow (+x), 90 = to starboard (+y); equals Unreal yaw in the ship frame' },
    hub, levels: LEVELS, open: OPEN, locked: LOCKED, gates: GATES, gate_marks: GATE_MARKS, where: WHERE, spaces: SPACES, outside: OUTSIDE,
    areas: { total: Math.round(a.total), open: Math.round(a.openTotal), levels: Object.fromEntries(Object.entries(a.levels).map(([k, v]) => [k, r(v)])), open_by_zone: r(a.open), locked_by_zone: r(a.locked) },
  };
}
