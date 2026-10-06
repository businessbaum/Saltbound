/* Saltbound hub ship: the numbers. One source for the build (HubShip_Data.json, written from this by
   Tools/design/render_hub_ship.py --data) and for the layout review (Tools/design/hub_review.py).
   Metres. x forward from the stern (0) to the stem (160 at the main deck), y to starboard (+) from the centreline,
   z up from the design waterline. (In Unreal: +X forward, +Y starboard, +Z up, 1 m = 100 uu; pivot at x 80, y 0, z 0.)

   D64 (owner review, 2026-10-05): boats come in from THE SIDE through hidden doors in the hull that open inwards like a double
   door, into a water hall across the bottom deck; the stern is a galleon's again (round, wide and high at the top, no dock
   mouth); cozy decks 3 m apart, every deck and room built; the hull planked and ported down to the water.
   D70 (owner, 2026-10-06): "The whole bottom area of the ship will be a small harbor, with 4 doors in total, 2 on each side. The
   water street will go through the whole length of the ship"; the Keelgate is out of the hub for now. */
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
  // stern: above the counter the stern is round in plan: a half ellipse `round` m deep (D69, owner: "the end needs to be more
  // round, it looks like a square"; it was a flat curve 2.6 m deep that met the sides in a corner), meeting the sides without a
  // corner; it overhangs the sternpost (x post_x) and rakes aft 1 m by the taffrail.
  form: { run: { x1: 30, post: 0.06, pow: 1.6, blend: [2.5, 8.5] }, entry: { x0: 118, fine: 0.82, top: 6, pow: 1.4 },
          stern: { round: 9.5, shape: 'ellipse', counter: 8.5, post_x: 1.6, rake: 1.0 } },
  // The planking's sheer: the bands, wales and ports rise towards the ends (m at the stern and at the stem); the decks inside
  // stay level. aft/fore: where the rise starts.
  sheer: { stern: 2.2, aft: 44, bow: 2.6, fore: 118 },
  // The harbour (D70, owner: "The whole bottom area of the ship will be a small harbor, with 4 doors in total, 2 on each side. The
  // water street will go through the whole length of the ship"): a canal down the middle (|y| < half) from x0 to x1, narrower
  // under the Well (narrow: it runs beneath the Well's galleries, no masts there); two basins across the ship at the door
  // stations, water from side to side; quays along both sides of the canal (to |y| quay) and across the basins' ends (edge m
  // wide). Over the canal and the basins the decks are open up to the gun deck (13.2 m over the water: a mast up to 12 m), but
  // under the Well. The sea's own water (z 0), its floor z -4.
  // over: the decks above stand back from the canal to |y| over, so a moored boat's yard (2.6 m either side of her) clears them.
  dock: { canal: { x0: 12, x1: 148, half: 5, over: 6.5, narrow: { x0: 72, x1: 96, half: 3 } },
          basins: [{ x0: 30, x1: 42 }, { x0: 102, x1: 114 }], quay: 10, edge: 3, floor: -4, ceiling: 'gun', mast_max: 12 },
  // The hidden doors (D64, D70: two stations, a pair in each side at each): the hull's own planking, hinged at x0 and x1 of their
  // station, swinging in 90 degrees into its basin; from below the keel of a boat (z0) to just under the gun deck (z1), tall
  // enough for a mast. Shut, nothing shows but a seam in the planking and the hinge straps. They open for a boat that comes near
  // (the dock watch), or with E at their capstans on the basin's aft edge; ropes from each leaf's head to its capstan.
  doors: { stations: [{ x0: 32, x1: 40 }, { x0: 104, x1: 112 }], z0: -2.5, z1: 13.0, thick: 0.35, swing: 90, seconds: 10, open_m: 45,
           capstans: [[28.4, 16.5], [28.4, -16.5], [100.4, 16.5], [100.4, -16.5]] },
  // D70: where the player's boat lies in the harbour: a docking station forward of each basin along the canal's starboard quay
  // (bow forward, the quay to her starboard: reach 2.1 m from her middle to the quay's face). The arrival sails her in through a
  // door of that basin, turns her into the canal and moors her here; the departure takes her back out by the door she came in.
  docking: [{ id: 'aft', basin: 0, x0: 44, x1: 54, heading: 0, side: 1, reach: 2.1 },
            { id: 'fore', basin: 1, x0: 117, x1: 127, heading: 0, side: 1, reach: 2.1 }],
  // D70: the quays along the canal cross the basins on lifting bridges, two leaves each hinged at the basin's edges (raised to
  // `open` degrees for a boat passing: every mast clears between them); fixed footbridges cross the canal where no boat sails
  // (under the Well's galleries, and forward of the fore station).
  bridges: { lift: { w: 3.0, open: 80, at: [[0, 7.75], [0, -7.75], [1, 7.75], [1, -7.75]] },
             fixed: [{ x: 76.5, w: 2.4 }, { x: 91.5, w: 2.4 }, { x: 136, w: 3.0 }] },
  shop: { x0: 50, x1: 68, y0: 10, y1: 20 },         // the shipwright's shop on the starboard quay (D70: no slipway)
  // the dock stair: in the shop along its aft wall, up from its open front to the Tally House (D70: +y; the shop's aft side is a
  // wall now, the old foot stood against it); on the 0.5 m grid
  stair: { x0: 50.5, x1: 53.0, y0: 11.0, y1: 16.5, up: '+y' },
  tally: { x0: 50, x1: 68, y0: 10, y1: 20 },        // the Tally House, lower deck, over the shop
  lane: { x0: 68, x1: 73, y0: 6, y1: 12 },          // lower deck, from the Tally House to the Well
  shaft: { x0: 78, x1: 90, y0: -6, y1: 6 },         // the Well: open from the canal under it to the sky
  ring: { x0: 73, x1: 95, y0: -11, y1: 11 },        // its galleries (5 m wide) on the lower, middle, upper and gun decks
  well_floor: { x0: 72, x1: 96, y0: -12, y1: 12 },  // D70: the Well's foot on the quay deck: quays round the narrow canal (no Keelgate)
  hoist: { x0: 78.2, x1: 80.8, y0: 3.2, y1: 5.8 },  // the hoist cage, aft-starboard corner of the shaft
  // The Well's stairs (17 risers of 17.6 cm on 30 cm treads, a short landing at the top), spiralling up round the ring.
  flights: [
    { from: 'quay', to: 'lower', x0: 79, x1: 84.4, y0: 8, y1: 11, up: '-x' },
    { from: 'lower', to: 'middle', x0: 92, x1: 95, y0: -6, y1: -0.6, up: '-y' },
    { from: 'middle', to: 'upper', x0: 79, x1: 84.4, y0: -11, y1: -8, up: '-x' },
    { from: 'upper', to: 'gun', x0: 73, x1: 76, y0: 0.6, y1: 6, up: '+y' },
    { from: 'gun', to: 'main', x0: 84.6, x1: 90, y0: 8, y1: 11, up: '+x' },
  ],
  // D69 (owner: "implement the Chandler better into the ship, it just stands around ... at the front of the ship"): the shop is
  // built into the forecastle's front at the main deck, two storeys high (the forecastle's storey above is open over it).
  chandlery: { x0: 134, x1: 143, y0: -1, y1: 12, h: 5.7 },
  wing: 10,     // (kept for the room layout: the old wings' line, |y| 10)
  fcX: 134,     // the forecastle starts here
  // A galleon's stern rises in tiers: the quarterdeck over the first storey (x poopX..qdX, its deck at sc2), the poop over three
  // (x 0..poopX, its deck at poop). D64: the castle is a storey taller at 3 m a storey and wider at the top.
  poopX: 24, qdX: 48,
  // Stern galleries (D69, owner: "make them more grand, more space, put them around all 3 sides of the back of the ship"):
  // open balconies at sc1 and sc2 standing `out` m from the hull, round the stern and along both sides forward to x1 (the poop's
  // length), on carved brackets, balustraded, columns up to the roof over the upper one. The closed quarter galleries are gone.
  galleries: { levels: ['sc1', 'sc2'], out: 3.4, x1: 21.5 },
  // The grand stairs (D69, owner: "two grand stairs on each side going up"): a pair at each step of the sterncastle, along the
  // bulwarks, rising aft from `from` to `to` and arriving at the upper deck's forward edge (x); `w` wide, 0.18 m risers on 0.28 m
  // treads. The glazed fronts stay free between them.
  grand: [{ id: 'quarterdeck', from: 'main', to: 'sc2', x: 48, w: 2.6 },
          { id: 'poop', from: 'sc2', to: 'poop', x: 24, w: 2.6 }],
  // The command hall on the poop (D69, owner: "where the command centre is ... big rooms, glass, a bit more modern ... a roof above
  // the back part"): the ship's council (L1). Glass walls in iron frames between oak posts from the poop deck up `h` m over
  // x0..x1 and |y| < half (the poop's own deck a walk round it, wider at its round aft end), a copper roof `roof` m high with a
  // glazed lantern along its ridge.
  // D70 (owner: "still no control room for controlling the ship (the player will unlock it later, the whole ship is not powered
  // by wind (as it seems) its driven by old ancient technology) ... controll room needs panorama view"; "you can use the glass
  // hall if you upgrade it and make two levels"): a second storey on the hall, the bridge, set in `inset` m all round (a railed
  // walk on the hall's roof round it), `h` m high, glass all round from a low sill (`sill` m) between slim iron mullions every
  // `bay` m; the copper roof on it. In its middle the ancient helm (`console`: pale seamless stone, turquoise lines, facing the
  // bow), out of reach behind the hall's barred doors until the player unlocks it.
  command: { x0: 5, x1: 21, half: 11.5, h: 4.4, roof: 2.6, bridge: { inset: 1.2, h: 3.4, sill: 0.5, bay: 1.6 }, console: { x: 14.0, r: 1.5 } },
  beakhead: { x0: 158, x1: 171, z: 18.5, halfW: 2.2, rails_to: 22.5, figure: [171.6, 0, 19.2] },
  masts: [
    { id: 'mizzen', name: 'Mizzen', x: 68, h: 54 },
    { id: 'main', name: 'Main', x: 100, h: 76 },
    { id: 'fore', name: 'Fore', x: 140, h: 64 },
  ],
  rig: { thick: [2.0, 0.7], top: { at: 0.38, w: 14 }, cross: { at: 0.68, w: 6.4 },
         yards: { at: [0.27, 0.47, 0.62, 0.8], half: [0.42, 0.35, 0.26, 0.18], scale: { main: 1, fore: 0.92, mizzen: 0.8 } },
         spritsail: { at: 0.6, half: 7.5 },
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
           // walkways (no rooms): galleries along the canal and round the basins, railed on the water side (D70)
           walks: [{ levels: ['lower', 'middle', 'upper'], rects: [R(12, 72, -9, -6.5), R(12, 72, 6.5, 9), R(96, 148, -9, -6.5), R(96, 148, 6.5, 9),
                                                                    R(27, 30, -20, 20), R(42, 45, -20, 20), R(99, 102, -20, 20), R(114, 117, -20, 20)] }] },
  // How the open spaces meet (D64): 'open' (no wall between them), 'door' (a wall with a doorway), anything else a wall.
  links: [['harbour', 'slip', 'open'], ['tally', 'lane', 'door'], ['lane', 'well_l', 'open'], ['deck', 'chandlery', 'door']],
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
const CN = HUB.dock.canal, NW = CN.narrow;
const CANAL = [R(CN.x0, NW.x0, -CN.half, CN.half), R(NW.x0, NW.x1, -NW.half, NW.half), R(NW.x1, CN.x1, -CN.half, CN.half)];
const BASINS = HUB.dock.basins.map(b => R(b.x0, b.x1, -20, 20));
const WATER = CANAL.concat(BASINS);
// Open up to the gun deck over the water (masts), wider than the canal (its yards), but where the Well's galleries span the
// narrow canal.
const OPEN_OVER = [R(CN.x0, NW.x0, -CN.over, CN.over), R(NW.x1, CN.x1, -CN.over, CN.over)].concat(BASINS);

/** Levels (floors) of the ship. width: share of the deck half-beam at that floor where the hull is narrower (the build uses the
 *  hull's form itself). only: where the floor is at all; voids: where it is not (open to the floor below); water: sea in it. */
const LEVELS = [
  { id: 'poop', name: 'Poop (deck)', z: HUB.Z.poop, only: [AFT], deck: true },
  { id: 'sc3', name: 'Sterncastle, third storey', z: HUB.Z.sc3, only: [AFT] },
  { id: 'sc2', name: 'Sterncastle, second storey (quarterdeck)', z: HUB.Z.sc2, only: [AFT, QD] },
  { id: 'sc1', name: 'Sterncastle, first storey', z: HUB.Z.sc1, only: [AFT, QD] },
  { id: 'fcTop', name: 'Forecastle deck', z: HUB.Z.fcTop, only: [R(134, 160, -20, 20)], deck: true },
  { id: 'fc1', name: 'Forecastle, storey', z: HUB.Z.fc1, only: [R(134, 160, -20, 20)], voids: [R(HUB.chandlery.x0, HUB.chandlery.x1, HUB.chandlery.y0, HUB.chandlery.y1)] },
  { id: 'main', name: 'Main deck', z: HUB.Z.main, voids: [HUB.shaft], deck: true },
  { id: 'gun', name: 'Gun deck', z: HUB.Z.gun, voids: [HUB.shaft] },
  { id: 'upper', name: 'Upper deck', z: HUB.Z.upper, voids: OPEN_OVER.concat([HUB.shaft]) },
  { id: 'middle', name: 'Middle deck', z: HUB.Z.middle, voids: OPEN_OVER.concat([HUB.shaft]) },
  { id: 'lower', name: 'Lower deck', z: HUB.Z.lower, voids: OPEN_OVER.concat([HUB.shaft]) },
  // D70: the harbour: the canal and the basins' water on the quay deck.
  { id: 'quay', name: 'Dock deck', z: HUB.Z.quay, water: WATER },
  // Under the water the holds stop at the canal's and the basins' walls (void_edge).
  { id: 'hold', name: 'Hold', z: HUB.Z.hold, voids: WATER, void_edge: 'wall' },
  { id: 'deep', name: 'The Deep', z: HUB.Z.deep, voids: WATER },
];

/** Open at the start: about a fifth. svc = the service badge (S J Q E). */
const OPEN = [
  // D70: the harbour: the quays along the canal, across the basins' ends and round the Well's foot.
  { id: 'harbour', name: 'The harbour', sub: 'the water street and its quays', level: 'quay', water: true,
    rects: [R(CN.x0 - 2, CN.x1 + 2, -HUB.dock.quay, HUB.dock.quay)].concat(HUB.dock.basins.map(b => R(b.x0 - HUB.dock.edge, b.x1 + HUB.dock.edge, -20, 20)),
                                                                           [HUB.well_floor]) },
  { id: 'slip', name: 'The Slip', sub: 'the shipwright’s shop', svc: 'S', level: 'quay', rects: [HUB.shop] },
  { id: 'tally', name: 'Tally House', sub: 'jobs and pay', svc: 'J', level: 'lower', rects: [HUB.tally], void_edge: 'wall' },
  { id: 'lane', name: 'Starboard lane', level: 'lower', rects: [HUB.lane] },
  { id: 'well_l', name: 'The Well', sub: 'galleries, stairs, hoist', level: 'lower', rects: [HUB.ring] },
  { id: 'well_m', name: 'The Well', level: 'middle', rects: [HUB.ring] },
  { id: 'well_u', name: 'The Well', level: 'upper', rects: [HUB.ring] },
  { id: 'well_g', name: 'The Well', level: 'gun', rects: [HUB.ring] },
  // D69: the waist runs on to the forecastle's front (the cargo deck is open: its boats and padlocked hatches stand on it).
  { id: 'deck', name: 'Main deck', sub: 'the Waist', level: 'main', rects: [R(HUB.qdX, HUB.fcX, -20, 20)] },
  // D69: the grand stairs make the quarterdeck and the poop open decks (weather: deck under the sky, the bulwark is the edge);
  // the command hall stands on the poop behind its doors (a gate mark), its glass walls round it (a walk of the poop round it).
  { id: 'quarterdeck', name: 'Quarterdeck', sub: 'the great wheel', level: 'sc2', rects: [QD], weather: true },
  { id: 'command', name: 'Command hall', sub: 'the ship\u2019s council (locked)', level: 'poop', rects: [R(5, 21, -11.5, 11.5)] },
  { id: 'poop', name: 'Poop', sub: 'round the command hall', level: 'poop', rects: [AFT], weather: true },
  { id: 'chandlery', name: 'Chandlery', sub: 'in the forecastle', svc: 'E', level: 'main', rects: [R(HUB.chandlery.x0, HUB.chandlery.x1, HUB.chandlery.y0, HUB.chandlery.y1)] },
];

/** Planned, locked, not designed: working names only. gate = how its corridors meet the open spaces (see GATES).
 *  Every room of these is built (D64), behind its gates; on the levels in `air` the zone is open deck under the sky (no rooms). */
const LOCKED = [
  { id: 'L1', name: 'Sterncastle', levels: { main: [AFT, QD], sc1: [AFT, QD], sc2: [AFT], sc3: [AFT] }, gate: 'G1', sees: 'Tall doors at the aft end of the main deck, barred from inside. The stern windows glow at night; the great wheel on the quarterdeck is lashed and never turns.' },
  { id: 'L2', name: 'Stern quarters', levels: { lower: [R(0, 30, -20, 20)], middle: [R(0, 26, -20, 20), R(26, 30, -20, 20)] }, gate: 'G1', sees: 'Doors off the harbour’s aft quays, and the galleries over the water with their shutters.' },
  { id: 'L3', name: 'Stern stores', levels: { quay: [R(0, 27, -20, 20)] }, gate: 'G4', sees: 'Shuttered bays along the harbour’s aft quays.' },
  { id: 'L4', name: 'Timber store', levels: { quay: [R(50, 68, -20, -10)] }, gate: 'G6', sees: 'Iron grilles on the port side of the Slip; stacked timber behind them.' },
  { id: 'L5', name: 'Warehouses', levels: { quay: [R(45, 99, 10, 20), R(45, 99, -20, -10)] }, gate: 'G1', sees: 'Barred doors along the harbour’s quays amidships.' },
  { id: 'L6', name: 'The Holds', levels: { quay: [R(117, 160, -20, 20)], hold: [R(0, 160, -20, 20)] }, gate: 'G6', sees: 'Iron grilles along the forward quays; padlocked cargo hatches on the main deck.' },
  { id: 'L7', name: 'Purser’s stores', levels: { lower: [R(68, 73, -11, 6), R(68, 73, 12, 20)] }, gate: 'G1', sees: 'A door in the Tally House’s forward wall, by the pay booth.' },
  { id: 'L8', name: 'The Ropewalk', levels: { lower: [R(30, 150, -20, -9)] }, gate: 'G1', sees: 'Double doors off the Well’s port gallery; the creak of a rope being laid.' },
  { id: 'L9', name: 'Mess and galley', levels: { lower: [R(95, 160, -9, 20), R(73, 95, 11, 20)] }, gate: 'G1', sees: 'A door off the Well’s forward gallery; the galley chimney smokes on the main deck.' },
  { id: 'L10', name: 'Guild rooms', levels: { middle: [R(42, 73, -20, 20)], lower: [R(42, 68, -20, 20)] }, gate: 'G1', sees: 'Doors with painted crests off the Well’s aft gallery; their galleries over the harbour.' },
  { id: 'L11', name: 'Lantern Market', levels: { middle: [R(95, 160, -20, 20), R(73, 95, 11, 20), R(73, 95, -20, -11)] }, gate: 'G2', sees: 'A rope across the forward gallery with a board: “Market closed”; light and voices behind shutters.' },
  { id: 'L12', name: 'Archive', levels: { upper: [R(0, 73, -20, 20)] }, gate: 'G1', sees: 'A door with a brass plate off the Well’s aft gallery; its galleries high over the harbour.' },
  { id: 'L13', name: 'Inn and bunkrooms', levels: { upper: [R(95, 160, -20, 20), R(73, 95, 11, 20), R(73, 95, -20, -11)] }, gate: 'G1', sees: 'A door off the forward gallery; laughter behind it in the evening.' },
  { id: 'L14', name: 'Forecastle', levels: { main: [R(134, 160, -20, 20)], fc1: [R(134, 160, -20, 20)] }, gate: 'G1', sees: 'A barred door in the forecastle’s front beside the Chandlery, and one in the shop’s back wall; a gallery high over the shop.' },
  { id: 'L15', name: 'Forecastle deck and the head', levels: { fcTop: [R(134, 160, -20, 20)] }, air: { fcTop: [R(134, 160, -20, 20)] }, gate: 'G3', sees: 'Chained ladders up the forecastle’s front at both ends; the bowsprit and the beakhead beyond.' },
  { id: 'L16', name: 'Masts and tops', levels: {}, gate: 'G3', sees: 'Shrouds whose ratlines start out of reach; the tops’ lanterns at night.' },
  { id: 'L17', name: 'The Deep', levels: { deep: [R(0, 160, -20, 20)] }, gate: 'G5', sees: 'Under the waterline; no way down yet (D70: the Keelgate is out of the hub for now).' },
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
  harbour: {
    level: 'quay', floor: 1.5, name: 'The harbour',
    props: [
      { id: 'water', note: 'hub.dock: the canal (|y| < 5, 3 under the Well) from x 12 to 148 and the two basins across the ship; sea water, its floor z -4; open up to the gun deck (13.2 m over the water) but under the Well’s galleries' },
      { id: 'quays', note: 'at +1.5: along the canal to |y| 10, across the basins’ ends (3 m), round the Well’s foot; faces of timber on piles with fenders; the decks above stand back to |y| 6.5' },
      { id: 'doors', note: 'hub.doors: a pair in each side at both basins, 15.5 m tall; shut, a seam in the planking; open, the leaves stand in the basin along its edges' },
      // Bollards every `every` m along the quays' edges (x0, y0, x1, y1: the edge's line), 0.45 m in from the face, none within
      // 0.8 m of a pillar or a bridge's landing.
      { id: 'bollards', every: 5, r: 0.3, h: 0.6, lines: [
        [12.5, 5, 29.5, 5], [45, 5, 71.5, 5], [72.5, 3, 77.5, 3], [83, 3, 95.5, 3], [96.5, 5, 101.5, 5], [117.5, 5, 147, 5],
        [12.5, -5, 29.5, -5], [45, -5, 71.5, -5], [72.5, -3, 95.5, -3], [96.5, -5, 101.5, -5], [117.5, -5, 147, -5],
        [30, 10.6, 30, 18], [30, -18, 30, -10.6], [42, 10.6, 42, 18], [42, -18, 42, -10.6],
        [102, 10.6, 102, 18], [102, -18, 102, -10.6], [114, 10.6, 114, 18], [114, -18, 114, -10.6]] },
      // Rung ladders from the water onto the quays (the D54 ladder): [x, y on the quay's face, yaw from the water to the quay].
      { id: 'rung_ladders', at: [[20, 5, 90], [20, -5, -90], [62, -5, -90], [98.5, -5, -90], [140, 5, 90], [140, -5, -90],
                                 [30, 14.5, 180], [42, -14.5, 0], [102, 14.5, 180], [114, -14.5, 0]] },
      // Other boats' berths (the fleet, D64): along the canal's quays, bow forward; side +1 the quay to her starboard.
      { id: 'fleet_berths', berths: [{ x0: 44, x1: 54, side: -1, boat: '10 m', reach: 2.1 }, { x0: 57, x1: 71, side: 1, boat: '14 m', reach: 2.3 }],
        heading: 0, note: 'a 10 m across the canal from the player’s boat at the aft station; the 14 m cutter at the Slip’s quay, forward of it' },
      // Posts under the galleries’ edges, from the quays to the gun deck.
      { id: 'pillars', size: 0.36, at: [16, 25, 47.5, 57, 66.5, 120.5, 130, 139.5].flatMap(x => [[x, 6.75], [x, -6.75]])
          .concat([[29.75, 10.5], [29.75, -10.5], [42.25, 10.5], [42.25, -10.5], [101.75, 10.5], [101.75, -10.5], [114.25, 10.5], [114.25, -10.5]]) },
      { id: 'galleries', levels: ['lower', 'middle', 'upper'], note: 'walkways along the canal (|y| 6.5..9) and across the basins’ ends on three decks, balustraded on the water side; they belong to the rooms behind them' },
      // Lanterns [x, y, z above the quay]: under the decks over the quays and from the galleries’ edges; none over the water.
      { id: 'lanterns', hook_z: 3.0, at: [].concat(
          [15, 22, 48, 56, 64, 121, 129, 137, 143].flatMap(x => [[x, 8.5, 2.6], [x, -8.5, 2.6]]),
          [28.5, 43.5, 100.5, 115.5].flatMap(x => [[x, 13, 2.6], [x, -13, 2.6]]),
          [[75, 8, 2.6], [75, -8, 2.6], [93, 8, 2.6], [93, -8, 2.6]],
          [[20, 6.9, 5.6], [26, -6.9, 8.6], [60, -6.9, 5.6], [66, 6.9, 8.6], [124, 6.9, 5.6], [132, -6.9, 8.6], [140, -6.9, 5.6]]) },
      // The docking stations (hub.docking): bollards fore and aft of the berth, a gangway landing with a lantern post, a board.
      { id: 'stations', note: 'at each: the gangway’s landing (a gap in the bollards, a lantern on a post), the station’s board with its name' },
    ],
    gates: ['L3 shuttered bays along the aft quays', 'L5 barred doors along the quays amidships', 'L6 grilles along the forward quays'],
  },
  slip: {
    svc: 'S', level: 'quay', floor: 1.5, name: 'The Slip (shipwright)',
    npc: { x: 57.4, y: 13.2, face: 180, note: 'behind the counter across the shop, facing aft to the door from the quay' },
    e: [
      { id: 'counter', x: 54.6, y: 13.2, prompt: 'Talk' },
      { id: 'plans', x: 51.2, y: 18.0, prompt: 'Look at the plans' },
      { id: 'display_boat', x: 64, y: 7.4, prompt: 'Look her over', note: 'on the quay by the cutter’s gangway' },
      { id: 'fitting_berth', x: 53.0, y: 6.6, prompt: 'Read the berth board', note: 'a board on a post at the quay’s edge by the bow of the aft docking station, between two bollards' },
    ],
    props: [
      { id: 'shop', x0: 50, x1: 68, y0: 10, y1: 20, h: 3, note: 'open to the starboard quay along y 10; the Tally House above it; the dock stair up along its aft wall' },
      { id: 'counter', x0: 55.4, x1: 56.4, y0: 11.2, y1: 15.6, h: 0.95, note: 'across the shop facing aft' },
      { id: 'plans_board', wall: 'x=50', y0: 17.0, y1: 19.0, z0: 0.9, z1: 2.4, note: 'pinned plans of the upgrades on the aft wall, past the stair' },
      { id: 'half_models', wall: 'x=68', y0: 12, y1: 18, z0: 1.9, z1: 2.5, note: 'three half-hull models: the boats for sale' },
      { id: 'workbench', x0: 60, x1: 64, y0: 17.6, y1: 19, h: 0.9 },
      { id: 'timber_racks', wall: 'x=68', y0: 12.5, y1: 17.5, z0: 0.4, z1: 1.7, note: 'planks on iron brackets on the forward wall, under the half-models' },
      { id: 'quay_work', x0: 58, x1: 68, y0: 8.6, y1: 9.9, note: 'the shipwright’s work on the quay along the shop’s front: a spar on trestles, coils, a grindstone, the steam box (the covered quay in front of it stays free to walk)' },
      { id: 'lanterns', at: [[62, 15, 2.2], [54, 15, 2.2]], hook_z: 2.7 },
      { id: 'sign', x: 59, y: 10, z: 2.2, text: 'SHIPWRIGHT', note: 'over the shop’s open front, facing the quay and the canal' },
    ],
    gates: ['L5 door in the forward wall (x 68)'],
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
      { id: 'stair_hole', x0: 50.5, x1: 53.0, y0: 11.0, y1: 16.5, note: 'the dock stair arrives here along the aft wall, a rail round it' },
      { id: 'lane_door', wall: 'x=68', y0: 10.2, y1: 12, note: 'to the lane and the Well' },
      { id: 'job_board', wall: 'x=68', y0: 12.8, y1: 17.2, z0: 0.6, z1: 1.9, note: 'six slips; header board TALLY over it (under the beams); a chalk slate under it' },
      { id: 'pay_booth', x0: 57.6, x1: 62.6, y0: 17.4, y1: 20, h: 2.4, door: [18.0, 19.0], note: 'timber booth on the hull wall, 1.8 m forward of where the dock stair arrives; grilled hatch x 58.5..61.5, z 1.0..1.8; ledge at z 1.0; sign PAY; its door in the forward side' },
      { id: 'weighing_bench', x0: 63.4, x1: 67, y0: 18.6, y1: 20, h: 0.9, note: 'a big balance on it; sacks and crates beside' },
      { id: 'salt_cakes', x0: 64.6, x1: 65.4, y0: 10.6, y1: 11.4, h: 0.9, note: 'stacked by the lane door, out of the way to it' },
      { id: 'table', x0: 56, x1: 64, y0: 12.6, y1: 13.8, h: 0.8, note: 'benches either side at y 14.2..14.6 and y 11.8..12.2, h 0.45' },
      { id: 'hull_ports', wall: 'y=20', spans: [[62.4, 64], [65.2, 66.8]], z0: 0.8, z1: 1.8 },
      { id: 'lanterns', at: [[64, 15, 2.2], [59.5, 13.2, 2.2], [53, 13, 2.2], [70.5, 9, 2.2]], hook_z: 2.7 },
    ],
    gates: ['L7 door in the forward wall (x 68, y 18.6..19.8)'],
  },
  well: {
    level: 'lower..main', name: 'The Well',
    props: [
      { id: 'shaft', x0: 78, x1: 90, y0: -6, y1: 6, note: 'open from the canal and the quays round it (+1.5) to the sky; rails 1.0 m round it on every gallery' },
      { id: 'galleries', x0: 73, x1: 95, y0: -11, y1: 11, note: 'lower, middle, upper and gun deck; walls on their outer edges with the gates' },
      { id: 'hoist', x0: 78.2, x1: 80.8, y0: 3.2, y1: 5.8, stops: ['quay', 'lower', 'main'], door: 'aft (x 78.2)', note: 'cage on ropes from the gantry, between two guide posts at its aft corners; gates in the rails at its stops' },
      { id: 'counterweight', x0: 81.2, x1: 82.0, y0: 3.4, y1: 4.6, h: 1.5, note: 'its rope over the gantry beam’s second sheave; at the bottom of its run it sits in a railed bay on the quay beside the cage (D70: the canal runs where it stood)' },
      { id: 'gantry', note: 'A-frame over the opening on the main deck, x 76.5..91.5 at y 0, top 7.5 m above the deck, a beam out to the hoist with two sheaves' },
      { id: 'coaming', h: 0.3, note: 'a raised edge round the opening on the main deck, under the rail' },
      { id: 'lanterns', note: 'one on each gallery corner, a ring lantern in the shaft at the middle deck on a chain from the gantry’s apex' },
    ],
  },
  chandlery: {
    svc: 'E', level: 'main', floor: 16.5, name: 'Chandlery (equipment)',
    npc: { x: 140.6, y: 5.5, face: 180, note: 'behind the counter, facing the door and the deck beyond it' },
    e: [
      { id: 'counter', x: 138.7, y: 5.5, prompt: 'Talk' },
      { id: 'try_spyglass', x: 136.3, y: 9.3, prompt: 'Try the spyglass' },
      { id: 'try_compass', x: 136.3, y: 2.2, prompt: 'Try the compass' },
    ],
    props: [
      { id: 'shop', x0: 134, x1: 143, y0: -1, y1: 12, h: 5.7, note: 'in the forecastle at the main deck, two storeys high; its front is the forecastle’s, its door onto the deck (links)' },
      { id: 'front', wall: 'x=134', spans: [[-0.4, 3.8], [7.6, 11.4]], z0: 0.7, z1: 3.0, note: 'two shop windows of small panes either side of the door, lit inside (cut through the wall: spans)' },
      { id: 'sign', wall: 'x=134', y0: 0.0, y1: 11.0, z0: 3.6, z1: 4.5, text: 'CHANDLER' },
      { id: 'awning', y0: -1.2, y1: 12.2, from: [134, 3.35], to: [131.8, 2.75], note: 'striped oxblood and cream canvas on two iron arms' },
      { id: 'counter', x0: 139.2, x1: 140.0, y0: 1.4, y1: 9.6, h: 1.0, top: 1.05, note: 'across the shop facing the door' },
      { id: 'shelves', runs: [[142.35, 142.95, -0.7, 11.7], [134.6, 142.3, 11.1, 11.7], [134.6, 142.3, -0.7, -0.1]], h: 5.3, note: 'up to the beams: tins, jars, coils, boxes; a ladder on a brass rail along the back; open where a gate stands' },
      { id: 'display_table', x0: 135.6, x1: 137.0, y0: 1.3, y1: 3.1, h: 0.85, note: 'the compass to try, a chart, dividers' },
      { id: 'spyglass_stand', x: 136.3, y: 9.9, h: 1.15, note: 'a spyglass on its tripod' },
      { id: 'beams', at_x: [136.3, 139.6], z: 5.0, note: 'stock hangs from them: lanterns, coils, a net' },
      { id: 'goods', boxes: [[131.5, 133.7, 9.8, 11.9], [131.7, 133.7, -1.1, 0.7]], note: 'outside the front, either side of the windows: barrels, crates, coils, an anchor, oars' },
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
      { id: 'lanterns', at: [[86, 7.6, 2.2], [100, 2.3, 2.0]], hook_z: 2.8 },
    ],
  },
};

/* Gates the room layout cannot place itself (plan positions, metres): the cargo hatch, the forecastle's ladders, the shrouds,
   the command hall's doors. The rest stand where a locked zone's corridor meets an open space (hub_layout.gate_marks). */
const GATE_MARKS = [
  { level: 'main', x: 122, y: 0, g: 'G3', zone: 'L6' },
  { level: 'main', x: 134, y: 15, g: 'G3', zone: 'L15' }, { level: 'main', x: 134, y: -15, g: 'G3', zone: 'L15' },
  { level: 'main', x: 68, y: 17.6, g: 'G3', zone: 'L16' }, { level: 'main', x: 100, y: 17.6, g: 'G3', zone: 'L16' },
  // D69: the command hall's glass doors, at its front on the poop.
  { level: 'poop', x: 21, y: 0, g: 'G1', zone: 'L1', line: 'Barred from inside. The council meets here.' },
];
const WHERE = {
  L1: 'Main deck aft and the sterncastle over it: the great cabin, the stern galleries, the quarterdeck, the poop',
  L2: 'Lower and middle deck aft, over the harbour’s aft quays and the stern', L3: 'Dock deck aft, round the canal’s end', L4: 'Dock deck, across the canal from the Slip',
  L5: 'Dock deck amidships, along both quays', L6: 'Dock deck forward of the fore basin; the hold under everything', L7: 'Lower deck, between the Tally House and the Well',
  L8: 'Lower deck, port side, 120 m long', L9: 'Lower deck forward', L10: 'Middle deck aft (over the harbour and the Slip)',
  L11: 'Middle deck forward', L12: 'Upper deck aft', L13: 'Upper deck forward and round the Well', L14: 'The forecastle’s two storeys, round the Chandlery',
  L15: 'The forecastle deck, the beakhead and the bowsprit', L16: 'Three masts, their tops and yards', L17: 'The Deep, under the waterline',
  L18: 'Gun deck, all of it but the Well’s gallery',
};

/* The outside, for the build (metres, absolute z). */
const OUTSIDE = {
  hull_section: 'Planked all the way down (D64: no pale ancient band outside; the ancient hull shows only inside, in the Deep): flat bottom +/-6 m at the keel (z -12), +/-16 m at z -6, +/-19 m at the waterline, widest +/-20 m at z +6, tumblehome to +/-18.8 m at the main deck rail and +/-17.4 m at the taffrail. Fine at the ends below the water (hub.form): the run aft narrows to the sternpost, the entry forward is finer than the deck outline.',
  stern: 'A galleon’s stern (D64, D69): the sterncastle rises in two tiers, the quarterdeck over x 24..48 (deck 22.5) and the poop over x 0..24 (three storeys, deck 28.5). Below the counter (z 8.5) the hull narrows to the sternpost and the one rudder; above it the stern is full width and round in plan, a half ellipse 9.5 m deep that runs into the sides without a corner, leaning aft a little. On its face: rows of tall cabin windows on each storey; grand open galleries at sc1 and sc2, 3.4 m deep, round the stern and along both sides to x 21.5, on carved brackets with balustrades and columns, the upper one roofed; glazed doors from the cabins onto them. Her name in carved brass letters under the taffrail, three great lanterns on the taffrail. The castles’ fronts are glazed in iron frames, with grand stairs along the bulwarks up to the quarterdeck and the poop; on the poop the command hall in glass under a copper roof. The tiller comes in through the counter; the lashed great wheel is on the quarterdeck.',
  sides: 'Dark tarred planking in strakes about 0.55 m wide with a lively sheer (hub.sheer: up 2.2 m at the stern, 2.6 m at the stem); heavy wales in a belt at the waterline and at the deck lines, standing out like mouldings; ochre bands; lidded square ports (1 m) on every deck below the main deck down to the dock deck, cream-framed windows above it; most shut, some lit at night. Channels with deadeyes at each mast. The hidden doors (D70: four) at x 32..40 and 104..112 on both sides, from under the water to 13 m: shut, the planking, wales and painted port lids run on across them; only a seam and the iron hinge straps show.',
  masts: 'x 68, 100, 140 (hub.rig in numbers); 54, 76, 64 m above the main deck; square yards on all three (D69: the mizzen’s lateen yard is gone); every sail furled on its yard, gathered canvas bound by gaskets; shrouds to the channels, stays forward, a long oxblood-and-cream pennant at the main truck.',
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
