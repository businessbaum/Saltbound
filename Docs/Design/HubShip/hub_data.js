/* Saltbound hub ship: the numbers. One source for every sheet and for the area count in HubShip_Design.md.
   Metres. x forward from the stern face (0) to the stem (160 at the main deck), y to starboard (+) from the centreline,
   z up from the design waterline. (In Unreal: +X forward, +Y starboard, +Z up, 1 m = 100 uu; pivot at x 80, y 0, z 0.) */
'use strict';

const HUB = {
  name: 'Hollowkeel',
  L: 160, B: 40, sprit: 188, keel: -12,
  // The hull's half-width up its height amidships, [z, half-width] (outside.hull_section in numbers, for the build): ancient
  // below ancientTop, timber above; aft the tumblehome runs on to the poop rail. Scaled along the ship by hb(x) / 20.
  section: [[-12, 6], [-6, 16], [0, 19], [6, 20], [18.4, 18.8], [34.4, 15.7]], ancientTop: 3,
  Z: { deep: -8, quay: 1.5, lower: 5.5, middle: 9.5, upper: 13.5, main: 17.5, sc1: 21.5, sc2: 25.5, sc3: 29.5, poop: 33.5, fc1: 21.5, fcTop: 25.5 },
  rail: 0.9,                                        // bulwarks and taffrail above their deck: under the 1.4 m player's eyes (1.2 m), so the sea shows
  basin: { x0: 0, x1: 44, y0: -10, y1: 10, floor: -5 }, // the Stern Dock: sea water, open to the sky
  ramp: { x0: 32, x1: 46, z0: -3, z1: 1.5 },        // the slipway, out of the water into the boat hall
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
  chandlery: { x0: 98, x1: 114, y0: -17.5, y1: -8, h: 4.2 },   // back wall clear of the main mast's shrouds (logic pass, D61)
  arch: { x0: 0, x1: 10, z0: 25.5, z1: 33.5 },
  wing: 10,     // the wings stand outboard of |y| 10, aft of x 44 (the basin between them)
  quayY: 14,    // the quays run |y| 10..14 at +1.5 under the wings (arcades)
  fcX: 134,     // the forecastle starts here
  // A galleon's stern rises in tiers (owner, D62: "style like a giant galleon"): the quarterdeck over the first storey
  // (x poopX..44, its deck at sc2), the poop over three (x 0..poopX, its deck at poop).
  poopX: 22,
  // The planking's sheer (D62): the bands and wales rise towards the ends (m at the stern face and at the stem); the decks
  // inside stay level. aft/fore: where the rise starts.
  sheer: { stern: 0.8, aft: 30, bow: 1.4, fore: 128 },
  // The beakhead (D62): a platform out from the stem under the bowsprit, headrails up to the forecastle, the figurehead.
  beakhead: { x0: 158, x1: 171, z: 19.5, halfW: 2.2, rails_to: 25.5, figure: [171.6, 0, 20.2] },
  masts: [
    { id: 'mizzen', name: 'Mizzen', x: 68, h: 54 },
    { id: 'main', name: 'Main', x: 100, h: 76 },
    { id: 'fore', name: 'Fore', x: 140, h: 64 },
  ],
  helm: { x: 6, y: 0, r: 1.8 },  // the great wheel on the poop over the arch: lashed, it never turns
  // Twin rudders on the wings' sternposts (the dock mouth splits the stern); the wheel's ropes run down to their tillers.
  // The blade is x0..x1 at the heel, narrowing to `head` (m) at the top (D62: 1.2 m was a post on a ship this size).
  rudders: { y: 15, x0: -3.2, x1: 0, z0: -6, z1: 6, head: 1.2 },     // y 15: inside the stern face down to its foot (z -6: +/-15.2)
  // She lies still because she is held: two bow cables from hawse holes to anchors ahead (the capstan heaves them), two stern
  // moorings from the wings' quarters, splayed out clear of the dock's approach. from: on the hull; to: where it meets the water.
  cables: [{ id: 'bow', from: [151, 7, 21], to: [178, 13, 0] }, { id: 'bow', from: [151, -7, 21], to: [178, -13, 0] },
           { id: 'stern', from: [2, 18, 20], to: [-24, 34, 0] }, { id: 'stern', from: [2, -18, 20], to: [-24, -34, 0] }],
  // Where she lies at Saltbound Cove (world metres, north +X, east +Y; the pier is 0, 0). Since the home island (D60) the pier
  // is inside its lagoon and the way out is the channel east: she lies off it, her stern (the dock mouth) to the channel, 130 m
  // off its mouth, 345 m from the pier (logic pass, D61; was 230, -555 west-north-west, behind the island from the pier).
  berth: { id: 'SaltboundRoads', centreM: [81, 415], bowDeg: 92 },
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
  // The stern goes down to z -6 at its face: the dock's floor (z -5) and walls stay inside the hull (logic pass, D61).
  return [[0, 34.4], [22, 34.4], [22.6, 26.4], [44, 26.4], [44.6, 18.4], [134, 18.4], [134.6, 26.4], [161.5, 26.4], [161, 20], [159.8, 12], [157.5, 4], [153, -4], [145, -10], [132, -12],
    [14, -12], [6, -11], [2, -8], [0, -6]];
};

const ST = { OPEN: 'open', LOCKED: 'locked', WATER: 'water' };
const R = (x0, x1, y0, y1) => ({ x0, x1, y0, y1 });
const WINGS = [R(0, 44, 10, 20), R(0, 44, -20, -10)];
const WINGS_AFT = [R(0, 22, 10, 20), R(0, 22, -20, -10)];     // under the poop (HUB.poopX): the upper storeys (D62)

/** Levels (floors) of the ship. width: share of the deck half-beam at that floor (the hull is narrower low down). */
const LEVELS = [
  { id: 'sc3', name: 'Sterncastle, third storey', z: 29.5, only: [...WINGS_AFT, R(0, 10, -10, 10)] },
  { id: 'sc2', name: 'Sterncastle, second storey', z: 25.5, only: [...WINGS_AFT, R(0, 10, -10, 10)] },
  { id: 'sc1', name: 'Sterncastle, first storey', z: 21.5, only: WINGS },
  { id: 'fc1', name: 'Forecastle, upper storey', z: 21.5, only: [R(134, 160, -20, 20)] },
  { id: 'main', name: 'Main deck', z: 17.5, voids: [HUB.basin, HUB.shaft] },
  { id: 'upper', name: 'Upper deck', z: 13.5, voids: [HUB.basin, HUB.shaft] },
  { id: 'middle', name: 'Middle deck', z: 9.5, voids: [HUB.basin, HUB.hall, HUB.shaft] },
  { id: 'lower', name: 'Lower deck', z: 5.5, voids: [HUB.basin, HUB.hall, HUB.shaft] },
  { id: 'quay', name: 'Quay deck', z: 1.5, width: 0.95, water: [HUB.basin] },
  { id: 'deep', name: 'The Deep', z: -8, width: 0.8, only: [R(46, 150, -20, 20)] },
];

/** Open at the start: the 20 %. svc = the service badge (S J Q E). */
const OPEN = [
  { id: 'dock', name: 'Stern Dock', sub: 'basin and quays', level: 'quay', rects: [R(0, 44, 10, 14), R(0, 44, -14, -10), R(44, 48, -14, -9)], water: true },
  { id: 'slip', name: 'The Slip', sub: 'boat hall and shop', svc: 'S', level: 'quay', rects: [HUB.hall, HUB.shop] },
  { id: 'stair', name: 'Dock stair', level: 'quay', rects: [HUB.stair] },
  { id: 'keelgate', name: 'The Keelgate', sub: 'chamber at the foot of the Well', svc: 'Q', level: 'quay', rects: [HUB.chamber] },
  { id: 'tally', name: 'Tally House', sub: 'jobs and pay', svc: 'J', level: 'lower', rects: [HUB.tally, R(42, 44, 14, 17)] },
  { id: 'lane', name: 'Starboard lane', level: 'lower', rects: [HUB.lane] },
  { id: 'well_l', name: 'The Well', sub: 'galleries, stairs, hoist', level: 'lower', rects: [HUB.ring] },
  { id: 'well_m', name: 'The Well', level: 'middle', rects: [HUB.ring] },
  { id: 'well_u', name: 'The Well', level: 'upper', rects: [HUB.ring] },
  { id: 'deck', name: 'Main deck', sub: 'the Waist', level: 'main', rects: [R(44, 116, -20, 20)] },
  { id: 'chandlery', name: 'Chandlery', sub: 'deckhouse', svc: 'E', level: 'main', rects: [R(98, 114, -17.5, -8)], inside: 'deck' },
];

/** Planned, locked, not designed: working names only. gate = the gate type (see GATES). */
const LOCKED = [
  { id: 'L1', name: 'Sterncastle and Stern Arch', levels: { main: WINGS, sc1: WINGS, sc2: [...WINGS_AFT, R(0, 10, -10, 10)], sc3: [...WINGS_AFT, R(0, 10, -10, 10)] }, gate: 'G1', sees: 'Tall doors at the aft end of the main deck, barred from inside. The great windows of the arch glow at night; the great wheel on the poop is lashed and never turns.' },
  { id: 'L2', name: 'Wing quarters', levels: { lower: WINGS, upper: WINGS }, gate: 'G4', sees: 'Rows of shuttered windows and washing lines on the wings over the dock.' },
  { id: 'L3', name: 'Quay stores', levels: { quay: [R(0, 44, 14, 20), R(0, 44, -20, -14)] }, gate: 'G4', sees: 'Shuttered bays along both arcades of the dock.' },
  { id: 'L4', name: 'Timber store', levels: { quay: [R(48, 64, -20, -9), R(44, 48, -20, -14)] }, gate: 'G6', sees: 'Iron grilles on the port side of the Slip and at the end of the port quay; stacked timber behind them, 2 m deep.' },
  { id: 'L5', name: 'Keel Walk', levels: { quay: [R(64, 72, -20, 20)] }, gate: 'G1', sees: 'A heavy door in the Slip’s forward wall and another in the Keelgate chamber: the first shortcut to open.' },
  { id: 'L6', name: 'The Holds', levels: { quay: [R(96, 160, -20, 20), R(72, 96, 12, 20), R(72, 96, -20, -12)] }, gate: 'G6', sees: 'Iron grilles in the Keelgate chamber’s forward wall; padlocked cargo hatches on the main deck.' },
  { id: 'L7', name: 'Purser’s stores', levels: { lower: [R(64, 73, -11, 6), R(64, 73, 12, 20)] }, gate: 'G1', sees: 'A door in the Tally House’s forward wall, by the pay booth.' },
  { id: 'L8', name: 'The Ropewalk', levels: { lower: [R(44, 150, -20, -9)] }, gate: 'G1', sees: 'Double doors off the Well’s port gallery; the creak of a rope being laid.' },
  { id: 'L9', name: 'Mess and galley', levels: { lower: [R(95, 160, -9, 20), R(73, 95, 11, 20)] }, gate: 'G1', sees: 'A door off the Well’s forward gallery; the galley chimney smokes on the main deck.' },
  { id: 'L10', name: 'Guild rooms', levels: { middle: [R(0, 73, -20, 20)] }, gate: 'G1', sees: 'Doors with painted crests off the Well’s aft gallery.' },
  { id: 'L11', name: 'Lantern Market', levels: { middle: [R(95, 160, -20, 20), R(73, 95, 11, 20), R(73, 95, -20, -11)] }, gate: 'G2', sees: 'A rope across the forward gallery with a board: “Market closed”; light and voices behind shutters.' },
  { id: 'L12', name: 'Archive', levels: { upper: [R(44, 73, -20, 20)] }, gate: 'G1', sees: 'A door with a brass plate off the Well’s aft gallery.' },
  { id: 'L13', name: 'Inn and bunkrooms', levels: { upper: [R(95, 160, -20, 20), R(73, 95, 11, 20), R(73, 95, -20, -11)] }, gate: 'G1', sees: 'A door off the forward gallery; laughter behind it in the evening.' },
  { id: 'L14', name: 'Forecastle and bow', levels: { main: [R(134, 160, -20, 20)], fc1: [R(134, 160, -20, 20)] }, gate: 'G3', sees: 'Chained ladders and closed doors at the forward end of the main deck; the bowsprit beyond.' },
  { id: 'L15', name: 'Cargo deck', levels: { main: [R(116, 134, -20, 20)] }, gate: 'G2', sees: 'The forward end of the main deck behind a rope and a board (“Cargo work, keep clear”): lashed cargo under nets, the padlocked hatches, the ship’s boats.' },
  { id: 'L16', name: 'Masts and tops', levels: {}, gate: 'G3', sees: 'Shrouds whose ratlines start out of reach; the tops’ lanterns at night.' },
  { id: 'L17', name: 'The Deep', levels: { deep: [R(46, 150, -20, 20)] }, gate: 'G5', sees: 'The Keelgate: the ancient seal in the floor at the foot of the Well, faintly lit from inside.' },
];

const GATES = {
  G1: { name: 'Barred door', why: 'Somebody’s room or business, not open to you yet.', how: 'A timber door with an iron bar and a padlock. E: a short line of why.' },
  G2: { name: 'Rope and board', why: 'Work going on, or closed for now.', how: 'A rope across the way on stanchions (or between the door posts), a hand-painted board on it.' },
  G3: { name: 'Chained ladder or hatch', why: 'Up or down: masts, forecastle, cargo hatches.', how: 'A chain through the rungs or the grating, padlocked.' },
  G4: { name: 'Shutters', why: 'Windows and bays of locked rooms.', how: 'Closed shutters: nothing inside to see, nothing inside to build.' },
  G5: { name: 'Ancient seal', why: 'The lost civilisation: the main quest.', how: 'Pale seamless stone, turquoise lines, no handle, no lock.' },
  G6: { name: 'Iron grille', why: 'A glimpse: stores and holds.', how: 'Bars you see through into a shallow set, 2 to 3 m deep, and no further.' },
};

/* ---------- the spaces in detail: NPC spots, interaction points, furniture (for the build) ----------
   Plan positions in metres (ship frame). z and h are metres above that space's floor. face: the way an NPC looks, in
   degrees in plan: 0 = towards the bow (+x), 90 = to starboard (+y) (= Unreal yaw in the ship's frame).
   Boxes: x0..x1, y0..y1, height h. Wall items: wall 'x=64' (a plane at x 64) or 'y=19', with their extent and z0..z1. */
const SPACES = {
  dock: {
    level: 'quay', floor: 1.5,
    props: [
      { id: 'basin', x0: 0, x1: 44, y0: -10, y1: 10, note: 'sea water, open to the sky; floor of ancient stone at z -5; walls of ancient stone up to the quays' },
      { id: 'quays', note: 'x 0..44 at |y| 10..14, timber decking on the ancient walls, under the wings (arcades)' },
      { id: 'arcade_posts', every: 6, from: 3, to: 44, y: 10.2, size: 0.6, h: 4, note: 'both sides; carry the wing over the quay from z 5.5' },
      { id: 'bollards', every: 6, from: 6, to: 44, y: 10.6, r: 0.3, h: 0.6, note: 'both sides' },
      { id: 'rung_ladders', at: [1.5, 20], y: 10, note: 'both sides, from the water onto the quay (the D54 ladder)' },
      { id: 'slipway', x0: 32, x1: 46, y0: -5, y1: 5, z_at_x0: -3, z_at_x1: 1.5, note: 'absolute z (waterline 0); a beach the boat runs up on; enters the hall floor at x 46' },
      { id: 'fitting_berth', x0: 32, x1: 42, y: 7.9, note: 'starboard quay under the wall crane: the player boat moors here, bow forward (centreline 0.4 m clear of the wall for the 3.4 m beam: blockout, D61)' },
      { id: 'fleet_berths', berths: [{ x0: 8, x1: 18, y: -7.9, boat: '10 m' }, { x0: 22, x1: 36, y: -7.5, boat: '14 m' }], note: 'port quay; boats the player owns (later)' },
      { id: 'dock_stair', x0: 30, x1: 44, y0: 14, y1: 17, rises: '+x', from_floor: 1.5, to_floor: 5.5, note: 'inside the starboard wing, through an opening in the arcade wall' },
      { id: 'dock_head_rail', note: 'main deck, along x 44 for |y| < 10: rail 1.0 m over the basin' },
      { id: 'port_landing', x0: 44, x1: 48, y0: -14, y1: -9, note: 'the port quay’s end, open to the boat hall: the fleet berths are reached on foot (logic pass, D61)' },
      { id: 'lanterns', at: [[9, 10.6, 3.0], [21, 10.6, 3.0], [33, 10.6, 3.0], [9, -10.6, 3.0], [21, -10.6, 3.0], [33, -10.6, 3.0]], hook_z: 3.6, note: 'on iron brackets on the arcade posts' },
    ],
  },
  slip: {
    svc: 'S', level: 'quay', floor: 1.5, name: 'The Slip (shipwright)',
    npc: { x: 51.4, y: 13.2, face: 180, note: 'behind the counter across the shop, facing aft to the door from the quay where customers come in; the workshop behind' },
    e: [
      { id: 'counter', x: 48.6, y: 13.2, prompt: 'Talk' },
      { id: 'plans', x: 45.4, y: 16.6, prompt: 'Look at the plans' },
      { id: 'display_boat', x: 57.5, y: -3.2, prompt: 'Look her over' },
      { id: 'fitting_berth', x: 37, y: 11.2, prompt: 'Read the berth board', note: 'a board at the quay’s edge; the boat moors here from its own windlass' },
    ],
    props: [
      { id: 'hall', x0: 44, x1: 64, y0: -9, y1: 9, h: 12, note: 'open to the basin along x 44; ceiling is the upper deck' },
      { id: 'shop', x0: 44, x1: 64, y0: 9, y1: 19, h: 4, note: 'open to the hall along y 9; the Tally House is above it' },
      { id: 'counter', x0: 49.4, x1: 50.4, y0: 10.4, y1: 16.6, h: 0.95, note: 'across the shop facing aft; a flap at its hall end (y 9..10.4) for the shipwright' },
      { id: 'plans_board', wall: 'x=44', y0: 14.6, y1: 18.6, z0: 0.9, z1: 2.5, note: 'pinned plans of the upgrades, by the door from the quay' },
      { id: 'half_models', wall: 'x=64', y0: 11, y1: 17, z0: 2.5, z1: 3.1, note: 'three half-hull models: the boats for sale' },
      { id: 'workbench', x0: 55, x1: 59, y0: 16.8, y1: 18.4, h: 0.9 },
      { id: 'timber_racks', wall: 'x=64', y0: -8, y1: 4, z0: 0.6, z1: 3.5, note: 'four rows of planks on iron brackets' },
      { id: 'display_boat', x0: 47, x1: 61, y: 0, beam: 4.4, keel_z: 0.6, note: 'the 14 m cutter (D51), unrigged, bow forward, on cradles at x 50, 54, 58' },
      { id: 'mast_on_trestles', x0: 46, x1: 62, y: -5.5, z: 1.1, note: 'her mast lying on two trestles' },
      { id: 'crane', x: 39, y: 10, bracket_z: 11, jib_to: [39, 8.6, 11], winch: [39, 12.6], note: 'the quay is roofed by the wing, so the crane is bracketed to the wing’s face over the arcade post at x 39: its jib 11 m above the quay (clear of a 10.5 m mast), the hook over the fitting berth’s side deck, the rope down to a winch under the arcade (was a 9 m derrick standing under the roof)' },
      { id: 'hall_windows', wall: 'y=19', note: 'two windows in the shop, x 47..48.5 and x 60.5..62, z 1.6..3.2 (sills above the ancient hull, which reaches z +3)' },
      { id: 'lanterns', at: [[58, 4, 8], [58, -4, 8], [50, 0, 8]], hook_z: 12 },
      { id: 'sign', x: 44, y: 0, z: 13, text: 'SHIPWRIGHT', note: 'over the hall mouth, facing aft into the dock' },
    ],
    gates: ['L5 door in the forward wall (x 64, y 5..7.5)', 'L4 grilles on the port side (y -9, x 48..60) and at the port landing (x 48)'],
  },
  tally: {
    svc: 'J', level: 'lower', floor: 5.5, name: 'Tally House (side jobs)',
    npc: { x: 53, y: 18.8, face: 270, note: 'inside the pay booth, behind the hatch, facing into the room' },
    e: [
      { id: 'pay_hatch', x: 53, y: 16.6, prompt: 'Talk' },
      { id: 'job_board', x: 62.4, y: 15.6, prompt: 'Read the board' },
      { id: 'weighing_bench', x: 59.5, y: 17.6, prompt: 'Deliver goods' },
    ],
    props: [
      { id: 'room', x0: 44, x1: 64, y0: 9, y1: 20, h: 4 },
      { id: 'stair_door', wall: 'x=44', y0: 14, y1: 17, note: 'the dock stair arrives here' },
      { id: 'lane_door', wall: 'x=64', y0: 9.2, y1: 12, note: 'to the lane and the Well' },
      { id: 'job_board', wall: 'x=64', y0: 12.8, y1: 18.2, z0: 0.9, z1: 3.3, note: 'six slips; header board TALLY (y 13.6..17.4, z 3.35..3.85); a chalk slate under it' },
      { id: 'pay_booth', x0: 50, x1: 56, y0: 17.4, y1: 20, h: 2.6, door: [18.0, 19.0], note: 'timber booth on the hull wall, roof at 2.8; grilled hatch x 51.5..54.5, z 1.0..2.0; ledge y 17.0..17.4 at z 1.0; sign PAY; a door in its forward side (x 56, y 18.0..19.0): the purser comes in from the stores' },
      { id: 'weighing_bench', x0: 57, x1: 62, y0: 18.6, y1: 20, h: 0.9, note: 'a big balance on it; sacks and crates beside' },
      { id: 'salt_cakes', x0: 48.6, x1: 49.4, y0: 18.8, y1: 19.6, h: 0.9, note: 'a stack of salt cakes by the booth' },
      { id: 'table', x0: 49, x1: 58, y0: 12.6, y1: 13.8, h: 0.8, note: 'benches either side at y 14.2..14.6 and y 11.8..12.2, h 0.45' },
      { id: 'hull_ports', wall: 'y=20', spans: [[46.4, 48], [59.4, 61]], z0: 0.8, z1: 2.0, note: 'open shutters; sills under the 1.4 m player’s eyes' },
      { id: 'hall_windows', wall: 'y=9', spans: [[47, 50], [51, 54], [55, 58], [59, 62]], z0: 0.8, z1: 2.6, note: 'look down into the Slip' },
      { id: 'lanterns', at: [[60, 15, 3], [53.5, 13.2, 3], [47, 15, 3]], hook_z: 4 },
    ],
    gates: ['L7 door in the forward wall (x 64, y 18.6..19.8)'],
  },
  well: {
    level: 'lower..main', name: 'The Well',
    props: [
      { id: 'shaft', x0: 78, x1: 90, y0: -6, y1: 6, note: 'open from the Keelgate floor (+1.5) to the sky; rails 1.0 m round it on every gallery' },
      { id: 'galleries', x0: 73, x1: 95, y0: -11, y1: 11, note: 'lower, middle, upper deck; walls on their outer edges with the gates' },
      { id: 'hoist', x0: 78.2, x1: 80.8, y0: 3.2, y1: 5.8, stops: ['quay', 'lower', 'main'], door: 'aft (x 78.2): it opens on to the galleries’ rail gates and the chamber’s aft side', note: 'cage on ropes from the gantry, running between two guide posts at its aft corners (braced to each gallery’s edge); gates in the rails at its stops' },
      { id: 'counterweight', x0: 78.2, x1: 79.0, y0: 1.6, y1: 2.8, h: 1.5, note: 'its rope over the gantry beam’s second sheave; at the bottom of its run it sits in a railed bay on the chamber floor (no one under it)' },
      { id: 'hoist_brake', note: 'a lever in the cage on a hand line to the brake drum at the gantry’s head: the rider works it' },
      { id: 'gantry', note: 'A-frame over the opening on the main deck, x 76.5..91.5 at y 0, top 7.5 m above the deck, a beam out to the hoist with two sheaves (the cage’s ropes, the counterweight’s)' },
      { id: 'coaming', h: 0.3, note: 'a raised edge round the opening on the main deck, under the rail: rain and spray run off the deck, not down the Well' },
      { id: 'lanterns', note: 'one on each gallery corner, a ring lantern in the shaft at the middle deck on a chain from the gantry’s apex' },
    ],
  },
  keelgate: {
    svc: 'Q', level: 'quay', floor: 1.5, name: 'The Keelgate (main quest)',
    npc: { x: 75.8, y: -9.4, face: 48, note: 'behind the reading desk by the port wall, the desk between them and the gate they face' },
    e: [
      { id: 'talk', x: 76.2, y: -5.6, prompt: 'Talk' },
      { id: 'gate', x: 81, y: 1.7, prompt: 'Look at the gate' },
      { id: 'hoist', x: 77.4, y: 4.5, prompt: 'Ride the hoist' },
    ],
    props: [
      { id: 'chamber', x0: 72, x1: 96, y0: -12, y1: 12, h: 4, note: 'the middle 12 x 12 m (the shaft) open 16 m up to the sky' },
      { id: 'keelgate', x: 84, y: 0, ring_r: 4.0, disc_r: 3.0, raised: 0.12, note: 'pale seamless ancient stone; turquoise lines: circles r 1.0 and 2.1, seven radial lines r 1.0..2.8; seven empty sockets at r 3.5 (an idea)' },
      { id: 'desk', x0: 74, x1: 77.5, y0: -8.4, y1: -6.6, h: 0.85, chair: [75.4, 76.2, -10.4, -9.6], note: 'books, a brass lamp, rubbings; the chair behind it, on the wall side' },
      { id: 'shelves', wall: 'y=-12', x0: 79, x1: 94, depth: 0.6, z0: 0, z1: 3.2, note: 'salvaged ancient pieces in jars; about a quarter faintly glowing (MI_AncientGlow, low)' },
      { id: 'rubbings', wall: 'x=96', y0: -2.8, y1: 3, z0: 1.4, z1: 2.9, note: 'paper rubbings of the gate’s lines' },
      { id: 'lanterns', at: [[94, 0, 3], [84, -10, 3]], hook_z: 4 },
      { id: 'well_stair', x0: 79, x1: 86, y0: 8, y1: 11, note: 'down from the lower deck: top at x 79, foot at x 86' },
    ],
    gates: ['L5 door in the aft wall (x 72, y 5..7.5)', 'L6 grilles in the forward wall (x 96, y -7..-4 and 4..7)', 'L17 the Keelgate itself (G5)'],
  },
  chandlery: {
    svc: 'E', level: 'main', floor: 17.5, name: 'Chandlery (equipment)',
    npc: { x: 105, y: -9.6, face: 90, note: 'behind the counter, facing out to the open deck and the main mast' },
    e: [
      { id: 'counter', x: 105, y: -6.6, prompt: 'Talk' },
      { id: 'try_spyglass', x: 106, y: -3.6, prompt: 'Try the spyglass' },
      { id: 'try_compass', x: 110.4, y: -3.6, prompt: 'Try the compass' },
    ],
    props: [
      { id: 'deckhouse', x0: 98, x1: 114, y0: -17.5, y1: -8, h: 4.2, ridge: 5.4, note: 'pitched roof, ridge along x at y -12.75; its back 1.5 m in from the bulwark, clear of the main mast’s port shrouds' },
      { id: 'counter_front', x0: 99.5, x1: 110, y0: -8.4, y1: -8, h: 0.9, top: 0.95, note: 'the front wall is the counter; top board at 0.95 (under the 1.4 m player’s eyes); posts at x 98..99.5, 110..111, 113..114' },
      { id: 'door', wall: 'y=-8', x0: 111, x1: 113 },
      { id: 'awning', x0: 99, x1: 110.5, from: [-8, 3.3], to: [-5.8, 2.75], note: 'canvas in muted oxblood and cream stripes on two posts at y -5.8' },
      { id: 'sign', wall: 'y=-8', x0: 101, x1: 109, z0: 3.5, z1: 4.4, text: 'CHANDLER' },
      { id: 'shelves', runs: [[98.5, 113.5, -17.2, -16.6], [98.3, 98.9, -16.6, -9.2]], h: 3.6, note: 'along the back wall and the aft wall, depth 0.6; tins, jars, coils, boxes; a ladder on a rail' },
      { id: 'display_table', x0: 103, x1: 108, y0: -14.5, y1: -12.5, h: 0.85, note: 'the tools of the wheel: compass, spyglass, lantern; room for more' },
      { id: 'window', wall: 'x=114', y0: -15, y1: -12.5, z0: 0.9, z1: 2.3 },
      { id: 'beams', at_x: [102, 106, 110], z: 3.9, note: 'stock hangs from them: lanterns, coils, a net' },
      { id: 'tripod_spyglass', x: 106, y: -4.5, h: 1.15, note: 'eyepiece at the 1.4 m player’s eye' },
      { id: 'binnacle', x: 110, y: -4.5, h: 1.0, note: 'the bowl at waist height, looked down into' },
      { id: 'fife_rail', x0: 98, x1: 102, y0: -2, y1: 2, h: 0.9, note: 'round the main mast (x 100, r 1.0), with belaying pins' },
      { id: 'barrels_crates', x0: 112, x1: 114, y0: -7.6, y1: -6 },
    ],
    gates: [],
  },
  main_deck: {
    level: 'main', floor: 17.5,
    props: [
      { id: 'bulwark', note: 'round the hull edge from x 44, 0.9 m with a cap rail (the 1.4 m player sees over it); inner face at 0.965 x the half-beam' },
      { id: 'stair_hatch', x0: 73, x1: 76, y0: 0, y1: 7, note: 'the last flight from the upper deck arrives here (aft of the Well), a hooded companion with a rail' },
      { id: 'masts', at: [[68, 0], [100, 0], [140, 0]], r: 1.0, note: 'fife rails 4 x 4 m round each; channels on the hull sides for the shrouds' },
      { id: 'cargo_rope', x: 116, every: 4, note: 'G2: a rope across the deck on stanchions every 4 m, a board at y 0 (Cargo work, keep clear)' },
      { id: 'cargo_hatches', boxes: [[118, 122, -3, 3], [123, 127, -3, 3]], note: 'gratings, padlocked (G3)' },
      { id: 'ships_boats', boxes: [[119, 129, 12, 15], [119, 129, -15, -12]], note: 'on chocks, under the cargo rope (locked zone L15)' },
      { id: 'capstan', x: 131.5, y: 0, r: 1.1, note: 'heaves the bow cables, led aft from the hawse holes under the forecastle' },
      { id: 'galley_chimney', x: 120.8, y: 8, h: 4.5, note: 'smokes: the mess below (L9)' },
      { id: 'bowsprit', from: [154, 0, 24.5], to: [188, 0, 31.5], note: 'absolute z' },
      { id: 'bobstay', from: [186, 0, 31], to: [158.5, 0, 6], note: 'holds the bowsprit down against the forestays: from its end to the stem' },
      { id: 'lanterns', at: [[72.4, 7.6, 2.2], [99, -5.6, 2.6], [110.5, -5.6, 2.6], [100, 2.3, 2.0]], hook_z: 2.8, note: 'at the companion, on the awning’s posts, on the main fife rail' },
    ],
  },
};

/* Where the gates stand (plan positions, metres). zone = the locked zone behind it. */
const GATE_MARKS = [
  { level: 'quay', x: 48, y: -11.5, g: 'G6', zone: 'L4' }, { level: 'quay', x: 54, y: -9, g: 'G6', zone: 'L4' },
  { level: 'quay', x: 64, y: 6.2, g: 'G1', zone: 'L5' }, { level: 'quay', x: 72, y: 6.2, g: 'G1', zone: 'L5' },
  { level: 'quay', x: 96, y: -5.5, g: 'G6', zone: 'L6' }, { level: 'quay', x: 96, y: 5.5, g: 'G6', zone: 'L6' },
  { level: 'quay', x: 8, y: 14, g: 'G4', zone: 'L3' }, { level: 'quay', x: 22, y: 14, g: 'G4', zone: 'L3' }, { level: 'quay', x: 8, y: -14, g: 'G4', zone: 'L3' }, { level: 'quay', x: 26, y: -14, g: 'G4', zone: 'L3' },
  { level: 'quay', x: 84, y: 0, g: 'G5', zone: 'L17' },
  { level: 'lower', x: 64, y: 19.2, g: 'G1', zone: 'L7' }, { level: 'lower', x: 73, y: -2.5, g: 'G1', zone: 'L7' },
  { level: 'lower', x: 84, y: -11, g: 'G1', zone: 'L8' }, { level: 'lower', x: 95, y: 3.5, g: 'G1', zone: 'L9' }, { level: 'lower', x: 89.5, y: 11, g: 'G1', zone: 'L9' },
  { level: 'middle', x: 73, y: 0, g: 'G1', zone: 'L10' }, { level: 'middle', x: 95, y: 3.5, g: 'G2', zone: 'L11' }, { level: 'middle', x: 88, y: -11, g: 'G4', zone: 'L11' }, { level: 'middle', x: 88, y: 11, g: 'G4', zone: 'L11' },
  { level: 'upper', x: 73, y: -4, g: 'G1', zone: 'L12' }, { level: 'upper', x: 95, y: 0, g: 'G1', zone: 'L13' }, { level: 'upper', x: 88.5, y: -11, g: 'G1', zone: 'L13' }, { level: 'upper', x: 84, y: 11, g: 'G1', zone: 'L13' },
  { level: 'main', x: 44, y: 14.5, g: 'G1', zone: 'L1' }, { level: 'main', x: 44, y: -14.5, g: 'G1', zone: 'L1' },
  { level: 'main', x: 116, y: 0, g: 'G2', zone: 'L15' }, { level: 'main', x: 122, y: 0, g: 'G3', zone: 'L6' },
  { level: 'main', x: 134, y: 8, g: 'G1', zone: 'L14' }, { level: 'main', x: 134, y: -8, g: 'G1', zone: 'L14' }, { level: 'main', x: 134, y: 15, g: 'G3', zone: 'L14' }, { level: 'main', x: 134, y: -15, g: 'G3', zone: 'L14' },
  { level: 'main', x: 68, y: 17.6, g: 'G3', zone: 'L16' }, { level: 'main', x: 100, y: 17.6, g: 'G3', zone: 'L16' },
];
const WHERE = {
  L1: 'Main deck aft and three storeys above it; the arch over the dock; the poop with the great wheel',
  L2: 'Both wings over the dock, lower and upper deck', L3: 'Quay deck, behind both arcades', L4: 'Quay deck, port of the Slip',
  L5: 'Quay deck, between the Slip and the Keelgate', L6: 'Quay deck forward; round the Keelgate chamber', L7: 'Lower deck, between the Tally House and the Well',
  L8: 'Lower deck, port side, 100 m long', L9: 'Lower deck forward', L10: 'Middle deck aft (over the Tally House, the Slip and the wings)',
  L11: 'Middle deck forward', L12: 'Upper deck over the Slip', L13: 'Upper deck forward and round the Well', L14: 'Bow: two storeys and the bowsprit',
  L15: 'Main deck between the Waist and the forecastle', L16: 'Three masts, their tops and yards', L17: 'Under the waterline, behind the Keelgate',
};

/* The outside, for the build (from the sketches; metres, absolute z). */
const OUTSIDE = {
  hull_section: 'Below +3 m the ancient hull: flat bottom +/-6 m at the keel (z -12), +/-16 m at z -6, +/-19 m at the waterline. Above it timber: widest +/-20 m at about z +6, tumblehome to +/-18.8 m at the main deck rail amidships (z 18.4) and +/-15.7 m at the poop rail aft (z 34.4). The deck outline is HUB.hb(x).',
  stern: 'A galleon’s stern (D62): the sterncastle rises in two tiers, the quarterdeck over x 22..44 (rail 26.4) and the poop over x 0..22 (rail 34.4). The stern face (x 0) goes down to z -6. The dock mouth is +/-10 m wide from the basin floor (z -5) up, +/-14 m between the quay decks (z 1.5) and z 5.5 (the quays show; under them the quays are solid stone), its top an arch from z 21.5 at the sides to 25.5 in the middle. The Stern Arch (x 0..10) fills z 25.5..33.5: seven tall great windows on its aft face (z 26.2..31.5, 1.8 m wide, 2.8 m apart, pointed tops), a brass frieze with the name at z 32..33.3. The taffrail rises 1.0 m in the middle (z 34.4 to 35.4). Quarter galleries on both wings’ outer sides, x 3..13, z 21.5..32, standing out 2.4 m, two tiers of windows, a brass drop below and a small dome on top. Stern lanterns on the wing corners at z 37.6. The great wheel (r 1.8 m) on the arch’s top, lashed with a rope cross, shows above the taffrail; its ropes run down the wings to the tillers of twin rudders hung on the wings’ sternposts (hub.rudders). Stern moorings from both wings’ quarters, splayed out clear of the approach (hub.cables).',
  sides: 'Dark tarred planking in strakes about 0.55 m wide with a gentle sheer (hub.sheer: the bands rise towards both ends) and heavy wales at the bands; ochre bands at z 5.5, 17.5, 33.5 (0.9 m) and thinner ones at the other deck lines; lidded square ports (1 m) below the main deck, cream-framed windows (1.3 x 2 m) above it; most shut, some lit at night. Channels with deadeyes at each mast. The ancient band (pale, two turquoise lines) from z -1.5 to +3 all round.',
  masts: 'x 68, 100, 140; 54, 76, 64 m above the main deck; 2.0 m thick at the deck tapering to 0.7; tops at 0.38 h (14 m wide) and crosstrees at 0.68 h (6.4 m); yards at 0.27, 0.47, 0.62, 0.80 h with half-lengths 0.42, 0.35, 0.26, 0.18 x 40 m (main; fore x 0.92, mizzen x 0.8); every sail furled on its yard with gaskets; shrouds to the channels, stays forward, a long oxblood-and-cream pennant at the main truck.',
  bow: 'Forecastle x 134..160 up to z 25.5 with a rail; a plain stem with two hawse holes and their cables down to the anchors ahead (hub.cables); a galleon’s beakhead out from the stem under the bowsprit (hub.beakhead: a grated platform at z 19.5, headrails curving up to the forecastle, a carved figurehead at its tip); the bowsprit from (154, 24.5) to (188, 31.5) with furled jibs, held down by its bobstay; the stem foot is ancient stone (the pale band rises a little at the bow).',
  colours: 'Hull planking dark tarred brown (new MI); ochre bands (new MI, muted); window frames MI_Paint_Cream; carvings and frieze MI_Metal_Brass (muted); decks MI_Wood_Teak and its light/dark variants; canvas MI_Canvas; iron MI_Metal_Iron; the ancient hull a new pale stone MI with MI_AncientGlow lines (sparingly).',
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

/** Everything above as plain data (the build's scripts read HubShip_Data.json, written from this by render_hub_ship.py). */
function exportData() {
  const hub = JSON.parse(JSON.stringify(HUB));
  hub.outline_deck = HUB.outline(1, 80).map(([x, y]) => [+x.toFixed(2), +y.toFixed(3)]);
  hub.profile = HUB.profile();
  const a = countAreas(), r = o => Object.fromEntries(Object.entries(o).map(([k, v]) => [k, Math.round(v)]));
  return {
    _comment: 'Saltbound hub ship, design data (generated from Docs/Design/HubShip/hub_data.js; do not edit, change hub_data.js and run Tools/design/render_hub_ship.py --data).',
    frame: { units: 'metres', x: 'forward from the stern face (0) to the stem (160)', y: 'to starboard', z: 'up from the design waterline',
      unreal: 'cm; +X forward, +Y starboard, +Z up; the ship actor pivot at x 80, y 0, z 0: X_cm = (x - 80) * 100, Y_cm = y * 100, Z_cm = z * 100',
      face: 'degrees in plan: 0 = towards the bow (+x), 90 = to starboard (+y); equals Unreal yaw in the ship frame' },
    hub, levels: LEVELS, open: OPEN, locked: LOCKED, gates: GATES, gate_marks: GATE_MARKS, where: WHERE, spaces: SPACES, outside: OUTSIDE,
    areas: { total: Math.round(a.total), open: Math.round(a.openTotal), levels: Object.fromEntries(Object.entries(a.levels).map(([k, v]) => [k, r(v)])), open_by_zone: r(a.open), locked_by_zone: r(a.locked) },
  };
}
