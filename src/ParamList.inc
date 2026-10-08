// THE parameter list. Index order = Resolume display order: groups contiguous, Debug last.
// X( id, name, type, min, max, default, group, options "A|B|C", flags )
//   type: FILE TEXT OPT EVENT BOOL STD HUE SAT BRI ALPHA BUFFER    flags: 0 or HIDDEN
// Names are <=16 chars (Arena truncates) and unique (Resolume stores values by NAME: never rename one).
// Included with X defined; Params.h builds the enum, LoadEhrPlugin.cpp the definitions.
//
// BUNDLES (W26): Resolume shows at most 512 FFGL params. MEHR_REGION (set per block below) says which bundles carry a
// param: 0 = every bundle, MEHR_WOW = LoadEhr Wow / Wow FX only, MEHR_CLASSIC = LoadEhr / LoadEhr FX only (MaskEhr's carrier
// logic; LoadEhr W1 moved Face to the Wow side). The enum keeps
// every id in every build (the code is shared); a param a bundle does not carry keeps its default there (= off).
#ifndef MEHR_WOW
#define MEHR_WOW 2
#define MEHR_CLASSIC 4
#endif
// COLOURS: every colour is ONE Arena colour picker = four contiguous params HUE, SAT, BRI, ALPHA (W11: Arena
// 7.28 turns exactly this quartet into its picker; RED/GREEN/BLUE(/ALPHA) stay plain sliders). The ALPHA always
// does something: it is the group's mix / opacity, or else a strength multiplier on that colour.
// A lone HUE (Hue Shift) is shown by Arena as a plain 0..1 slider and is used as a SHIFT, not a colour.
#define MEHR_COLOR( id, base, group, h, s, b, a, alphaName )  \
	X( id##_H, base " Color", HUE, 0, 1, h, group, "", 0 )     \
	X( id##_S, base " Sat", SAT, 0, 1, s, group, "", 0 )       \
	X( id##_V, base " Bri", BRI, 0, 1, b, group, "", 0 )       \
	X( id##_A, alphaName, ALPHA, 0, 1, a, group, "", 0 )

#undef MEHR_REGION
#define MEHR_REGION 0
// MODEL
// LoadEhr has NO library: a model comes only from a file the user points a Model slot at. Nothing loaded = transparent.
// W21: what slot A shows: its file, typed text or a logo file, each extruded into the same pipeline
X( P_MODEL_SOURCE, "Model Source", OPT, 0, 2, 0, "Model", "Model File|Text|Logo", 0 )
// FOUR MODEL SLOTS (LoadEhr W1): each a FILE + its own read-only Status line + Reload (re-read the file from disk). Every filled
// slot loads and uploads in the background ahead of use; Transition walks the chain of filled slots A -> B -> C -> D.
// SLOT LAYOUT: P_MODEL_A + 3 x slot + { 0 file, 1 status, 2 reload } (SlotParam in LoadEhrPlugin.cpp)
X( P_MODEL_A, "Model A", FILE, 0, 0, 0, "Model", "", 0 ) // MaskEhr's "Model File"
X( P_STATUS_A, "Status A", TEXT, 0, 0, 0, "Model", "", 0 ) // read-only report: what was loaded / built, or why not
X( P_RELOAD_A, "Reload A", EVENT, 0, 1, 0, "Model", "", 0 )
X( P_MODEL_B, "Model B", FILE, 0, 0, 0, "Model", "", 0 )
X( P_STATUS_B, "Status B", TEXT, 0, 0, 0, "Model", "", 0 )
X( P_RELOAD_B, "Reload B", EVENT, 0, 1, 0, "Model", "", 0 )
X( P_MODEL_C, "Model C", FILE, 0, 0, 0, "Model", "", 0 )
X( P_STATUS_C, "Status C", TEXT, 0, 0, 0, "Model", "", 0 )
X( P_RELOAD_C, "Reload C", EVENT, 0, 1, 0, "Model", "", 0 )
X( P_MODEL_D, "Model D", FILE, 0, 0, 0, "Model", "", 0 )
X( P_STATUS_D, "Status D", TEXT, 0, 0, 0, "Model", "", 0 )
X( P_RELOAD_D, "Reload D", EVENT, 0, 1, 0, "Model", "", 0 )
X( P_AUTO_FIT, "Auto Fit", BOOL, 0, 1, 1, "Model", "", 0 )
// W18: which parts Auto Fit sizes the frame by. Visible Parts = hiding a stand / prop re-fits (a 0.5 s glide).
X( P_FIT_TO, "Fit To", OPT, 0, 1, 1, "Model", "All Parts|Visible Parts", 0 )
X( P_NORMAL_FLIP_Y, "Normal Flip Y", BOOL, 0, 1, 0, "Model", "", 0 )
// W17: textures larger than this are downsampled at load (the file is never touched). Full = as the file has them.
X( P_MAX_TEX, "Max Texture Size", OPT, 0, 2, 0, "Model", "Full|4K|2K", 0 )
// MORPH CHAIN (generalises MaskEhr's Model B + Transition): Transition 0..1 walks every filled slot in order, an equal share per
// hop (integer points = a pure model). Chain Loop: the last filled slot morphs back into the first (one more hop).
X( P_TRANSITION, "Transition", STD, 0, 1, 0, "Model", "", 0 )
X( P_CHAIN_LOOP, "Chain Loop", BOOL, 0, 1, 0, "Model", "", 0 )
// W6 Stage 2 (Amrit: "how many transitions types between models can be added?"): the pack appended (0..4 keep their numbers, so saved
// clips keep their style). Swarm, Burn and Lava need the Wow bundles' dials: in LoadEhr / FX they dissolve.
X( P_TRANSITION_STYLE, "Transition Style", OPT, 0, 22, 0, "Model",
   "Dissolve|Shatter|Dust|Slice|Liquid|Points|Shrinkwrap|Wire|Voxel|Low Poly|Pieces|Implode|Twist|Melt|Scan|Glitch|Ripple|Swarm|Smoke|Burn|Lava|Spin Swap|Fold", 0 ) // W26: + Liquid
// the style per hop: Fixed = Transition Style every hop; Cycle = Transition Style, then the next style each hop; Random = a
// style per hop from a hash of the hop (stable while scrubbing)
X( P_STYLE_ORDER, "Style Order", OPT, 0, 2, 0, "Model", "Fixed|Cycle|Random", 0 )
// W28: the chain plays by itself (the Transition slider is ignored while it does): Once On Trigger = through the chain once each
// time the clip is triggered (and when Play is switched on), then holds the last model; Ping-Pong = to the end and back, round
// and round; Loop = to the end, then again from the first (Chain Loop on: seamless, the last hop morphs back into the first)
X( P_TRANS_PLAY, "Transition Play", OPT, 0, 3, 0, "Model", "Off|Once On Trigger|Ping-Pong|Loop", 0 )
X( P_TRANS_TIME, "Transition Time", STD, 0.5f, 30, 6, "Model", "", 0 ) // seconds PER HOP
X( P_HOLD_TIME, "Hold Time", STD, 0, 60, 0, "Model", "", 0 )         // seconds resting on each model between hops
// W16: Shatter / Dust timing, per hop. Ease 0 = the linear, all-at-once flight (the old hard onset); Stagger spreads the pieces'
// departures and arrivals over the hop; Spread = flight distance (1 = the old Shatter flight). Dissolve / Slice ignore them.
X( P_TRANS_EASE, "Trans Ease", STD, 0, 1, 1, "Model", "", 0 )
X( P_TRANS_STAGGER, "Trans Stagger", STD, 0, 1, 0.85, "Model", "", 0 )
X( P_TRANS_SPREAD, "Trans Spread", STD, 0, 4, 0.6, "Model", "", 0 )
#undef MEHR_REGION
#define MEHR_REGION MEHR_WOW
// W26 Stage 4: Transition Style Liquid = A melts into B as one liquid surface (the two models' SDFs blended and raymarched)
X( P_LIQUID_GOO, "Liquid Goo", STD, 0, 1, 0.6f, "Model", "", 0 )       // 0 = a straight shape morph, 1 = blobs joined by a bridge
X( P_LIQUID_LOOK, "Liquid Look", OPT, 0, 1, 0, "Model", "Chrome|Paint", 0 ) // Paint = the two models' own colours flow along
MEHR_COLOR( P_LIQUID_COL, "Liquid", "Model", 0.08f, 0.6f, 1, 0, "Liquid Tint Mix" ) // alpha = how much the tint colours it
X( P_LIQUID_WOBBLE, "Liquid Wobble", STD, 0, 1, 0.4f, "Model", "", 0 )
X( P_LIQUID_SPEED, "Liquid Speed", STD, 0, 2, 0.25f, "Model", "", 0 ) // W28: the wobble's pace (1 = the W26 pace, 2 = crazy)
#undef MEHR_REGION
#define MEHR_REGION 0
// TEXT (W21): typed text, extruded. '|' starts a new row (Arena's FFGL text field is one line); real line breaks too.
X( P_TEXT, "Text", TEXT, 0, 0, 0, "Text", "", 0 )
X( P_FONT, "Font", OPT, 0, 0, 0, "Text", "", 0 )          // dynamic: installed families; a pick writes Font Name
X( P_FONT_NAME, "Font Name", TEXT, 0, 0, 0, "Text", "", 0 )
X( P_FONT_STYLE, "Font Style", OPT, 0, 0, 0, "Text", "", 0 ) // dynamic: the faces of Font Name
X( P_FONT_FILE, "Font File", FILE, 0, 0, 0, "Text", "", 0 )  // .ttf / .otf / .ttc, overrides Font Name
X( P_ALIGN, "Align", OPT, 0, 2, 1, "Text", "Left|Center|Right", 0 )
X( P_LINE_SPACING, "Line Spacing", STD, 0.5f, 3, 1, "Text", "", 0 )
X( P_LETTER_SPACING, "Letter Spacing", STD, -0.3f, 2, 0, "Text", "", 0 )
X( P_CURVE_DETAIL, "Curve Detail", STD, 0, 1, 0.5f, "Text", "", 0 ) // text and logos: curve flattening
#undef MEHR_REGION
#define MEHR_REGION MEHR_CLASSIC
// LOGO (W21): svg, pdf, ai (PDF-compatible), png, jpg. EPS is answered in Status (no PostScript on macOS).
X( P_LOGO_FILE, "Logo File", FILE, 0, 0, 0, "Logo", "", 0 )
X( P_LOGO_COLOURS, "Logo Colours", OPT, 0, 1, 0, "Logo", "File Colours|Parts", 0 )
X( P_TRACE_THRESHOLD, "Trace Threshold", STD, 0, 1, 0.5f, "Logo", "", 0 )
X( P_TRACE_SMOOTH, "Trace Smooth", STD, 0, 1, 0.5f, "Logo", "", 0 )
X( P_TRACE_INVERT, "Trace Invert", BOOL, 0, 1, 0, "Logo", "", 0 )
// EXTRUDE (W21): text and logos. Sizes in letter heights (text) / the logo's height.
X( P_EXTRUDE_DEPTH, "Extrude Depth", STD, 0.01f, 2, 0.25f, "Extrude", "", 0 )
X( P_BEVEL_SIZE, "Bevel Size", STD, 0, 0.2f, 0.02f, "Extrude", "", 0 )
X( P_BEVEL_SEGMENTS, "Bevel Segments", STD, 1, 8, 3, "Extrude", "", 0 )
X( P_BEVEL_SHAPE, "Bevel Shape", OPT, 0, 3, 1, "Extrude", "Flat|Round|Chamfer|Inset", 0 )
X( P_BACK_CAP, "Back Cap", BOOL, 0, 1, 1, "Extrude", "", 0 )
X( P_OUTLINE_ONLY, "Outline Only", BOOL, 0, 1, 0, "Extrude", "", 0 )
X( P_WALL, "Wall Thickness", STD, 0.005f, 0.3f, 0.04f, "Extrude", "", 0 )
// LETTERS (W21): per glyph / per logo shape
X( P_TYPE_ON, "Type On", STD, 0, 1, 1, "Letters", "", 0 )
X( P_TYPE_SPEED, "Type Speed", STD, 0, 30, 0, "Letters", "", 0 ) // letters / s; above 0 the text types itself on, holds 2 s, repeats
X( P_LETTER_STAGGER, "Letter Stagger", STD, 0, 1, 0.5f, "Letters", "", 0 )
X( P_LETTER_WAVE, "Letter Wave", STD, 0, 2, 0, "Letters", "", 0 )
X( P_WAVE_SPEED, "Wave Speed", STD, 0, 5, 1, "Letters", "", 0 ) // cycles / s
X( P_LETTER_SPIN, "Letter Spin", STD, -30, 30, 0, "Letters", "", 0 ) // degrees / s
X( P_SPIN_MODE, "Spin Mode", OPT, 0, 1, 0, "Letters", "Together|Separate", 0 )
X( P_LETTER_HOP, "Letter Hop", OPT, 0, 4, 0, "Letters", "Off|Every Beat|Every 2|Every 4|Every 8", 0 )
X( P_HOP_HEIGHT, "Hop Height", STD, 0, 2, 0.5f, "Letters", "", 0 )
#undef MEHR_REGION
#define MEHR_REGION 0
// TRANSFORM
X( P_ROT_X, "Rotate X", STD, -180, 180, 0, "Transform", "", 0 )
X( P_ROT_Y, "Rotate Y", STD, -180, 180, 0, "Transform", "", 0 )
X( P_ROT_Z, "Rotate Z", STD, -180, 180, 0, "Transform", "", 0 )
X( P_SPIN_X, "Spin X", STD, -30, 30, 0, "Transform", "", 0 )
X( P_SPIN_Y, "Spin Y", STD, -30, 30, 0, "Transform", "", 0 )
X( P_SPIN_Z, "Spin Z", STD, -30, 30, 0, "Transform", "", 0 )
X( P_RESET_SPIN, "Reset Spin", EVENT, 0, 1, 0, "Transform", "", 0 )
X( P_POS_X, "Pos X", STD, -3, 3, 0, "Transform", "", 0 )
X( P_POS_Y, "Pos Y", STD, -3, 3, 0, "Transform", "", 0 )
X( P_POS_Z, "Pos Z", STD, -10, 10, 0, "Transform", "", 0 )
X( P_SCALE, "Scale", STD, 0, 6, 1, "Transform", "", 0 )
// CAMERA
X( P_FOV, "FOV", STD, 5, 150, 35, "Camera", "", 0 )
X( P_DISTANCE, "Distance", STD, 0.3f, 20, 4, "Camera", "", 0 )
X( P_ORBIT_YAW, "Orbit Yaw", STD, -180, 180, 0, "Camera", "", 0 )
X( P_ORBIT_PITCH, "Orbit Pitch", STD, -89, 89, 0, "Camera", "", 0 )
#undef MEHR_REGION
#define MEHR_REGION MEHR_CLASSIC
// CAMERA LIVE (Stage 6): beat cuts, stored poses (kept in the hidden Pose Data text), shake, vertigo
X( P_CUT_EVERY, "Cut Every", OPT, 0, 4, 0, "Camera Live", "Off|1 Beat|2 Beats|4 Beats|8 Beats", 0 )
X( P_CUT_ANGLES, "Cut Angles", STD, 2, 8, 6, "Camera Live", "", 0 )
X( P_ANGLE_SPREAD, "Angle Spread", STD, 0, 180, 50, "Camera Live", "", 0 )
X( P_CUT_ORDER, "Cut Order", OPT, 0, 1, 0, "Camera Live", "Random|Sequence", 0 )
X( P_BEAT_SHAKE, "Beat Shake", STD, 0, 4, 0, "Camera Live", "", 0 )
X( P_SHAKE_DECAY, "Shake Decay", STD, 0.02f, 2, 0.25f, "Camera Live", "", 0 )
X( P_VERTIGO, "Vertigo", STD, -1, 1, 0, "Camera Live", "", 0 )
X( P_POSE_GLIDE, "Pose Glide", STD, 0, 10, 1.5f, "Camera Live", "", 0 )
X( P_STORE_POSE1, "Store Pose 1", EVENT, 0, 1, 0, "Camera Live", "", 0 )
X( P_STORE_POSE2, "Store Pose 2", EVENT, 0, 1, 0, "Camera Live", "", 0 )
X( P_STORE_POSE3, "Store Pose 3", EVENT, 0, 1, 0, "Camera Live", "", 0 )
X( P_STORE_POSE4, "Store Pose 4", EVENT, 0, 1, 0, "Camera Live", "", 0 )
X( P_GO_POSE1, "Go Pose 1", EVENT, 0, 1, 0, "Camera Live", "", 0 )
X( P_GO_POSE2, "Go Pose 2", EVENT, 0, 1, 0, "Camera Live", "", 0 )
X( P_GO_POSE3, "Go Pose 3", EVENT, 0, 1, 0, "Camera Live", "", 0 )
X( P_GO_POSE4, "Go Pose 4", EVENT, 0, 1, 0, "Camera Live", "", 0 )
X( P_POSE_DATA, "Pose Data", TEXT, 0, 0, 0, "Camera Live", "", HIDDEN )
#undef MEHR_REGION
#define MEHR_REGION 0
// MOTION
X( P_WOBBLE_AMOUNT, "Wobble Amount", STD, 0, 2, 0, "Motion", "", 0 )
X( P_WOBBLE_SPEED, "Wobble Speed", STD, 0, 5, 0.5f, "Motion", "", 0 )
X( P_BEAT_BOUNCE, "Beat Bounce", STD, 0, 2, 0, "Motion", "", 0 )
X( P_BEAT_NOD, "Beat Nod", STD, 0, 2, 0, "Motion", "", 0 )
X( P_AUDIO_GAIN, "Audio Gain", STD, 0, 10, 1, "Motion", "", 0 )
X( P_AUDIO_SCALE, "Audio > Scale", STD, 0, 3, 0, "Motion", "", 0 )
X( P_AUDIO_GLOW, "Audio > Glow", STD, 0, 10, 0, "Motion", "", 0 )
X( P_AUDIO_EXPLODE, "Audio > Explode", STD, 0, 3, 0, "Motion", "", 0 )
// MATERIAL
MEHR_COLOR( P_TINT, "Tint", "Material", 0, 0, 1, 0, "Tint Mix" ) // alpha = the tint mix
X( P_HUE_SHIFT, "Hue Shift", HUE, 0, 1, 0, "Material", "", 0 )
X( P_SATURATION, "Saturation", STD, 0, 4, 1, "Material", "", 0 )
X( P_BRIGHTNESS, "Brightness", STD, 0, 6, 1, "Material", "", 0 )
X( P_METAL_OVERRIDE, "Metal Override", BOOL, 0, 1, 0, "Material", "", 0 )
X( P_METAL_VALUE, "Metallic Value", STD, 0, 1, 1, "Material", "", 0 )
X( P_ROUGH_SCALE, "Roughness Scale", STD, 0, 4, 1, "Material", "", 0 )
X( P_NORMAL_STRENGTH, "Normal Strength", STD, 0, 6, 1, "Material", "", 0 )
X( P_HEIGHT_DISPLACE, "Height Displace", STD, -0.5f, 0.5f, 0, "Material", "", 0 )
X( P_EMISSIVE_GLOW, "Emissive Glow", STD, 0, 10, 1, "Material", "", 0 )
X( P_RIM_GLOW, "Rim Glow", STD, 0, 10, 0, "Material", "", 0 )
MEHR_COLOR( P_RIM, "Rim", "Material", 0.1041667f, 0.4f, 1, 1, "Rim Alpha" ) // alpha x Rim Glow
#undef MEHR_REGION
#define MEHR_REGION MEHR_CLASSIC
// SURFACE: oil film, glitter, liquid chrome / glass, hologram, heat glow, crawling patterns.
// Each is scaled per part by "Part N Surface".
X( P_FILM, "Oil Film", STD, 0, 2, 0, "Surface", "", 0 )
X( P_FILM_THICK, "Film Thickness", STD, 0.1f, 3, 1, "Surface", "", 0 )
X( P_FILM_SHIFT, "Film Angle Shift", STD, 0, 2, 1, "Surface", "", 0 )
X( P_FILM_SWIRL, "Film Swirl", STD, 0, 2, 1, "Surface", "", 0 )
X( P_GLITTER, "Glitter", STD, 0, 10, 0, "Surface", "", 0 )
X( P_GLITTER_DENSITY, "Glitter Density", STD, 0, 1, 0.35f, "Surface", "", 0 )
X( P_GLITTER_SIZE, "Glitter Size", STD, 0.1f, 6, 1, "Surface", "", 0 )
X( P_GLITTER_TWINKLE, "Glitter Twinkle", STD, 0, 1, 0.2f, "Surface", "", 0 ) // twinkles per second per flake
X( P_GLITTER_METAL, "Glitter On Metal", STD, 0, 1, 0.8f, "Surface", "", 0 )
X( P_CHROME, "Liquid Chrome", STD, 0, 1, 0, "Surface", "", 0 )
X( P_CHROME_RIPPLE, "Chrome Ripple", STD, 0, 3, 0.6f, "Surface", "", 0 )
#undef MEHR_REGION
#define MEHR_REGION 0
X( P_GLASS, "Glass", STD, 0, 1, 0, "Surface", "", 0 )
X( P_GLASS_IOR, "Glass IOR", STD, 1, 2.5f, 1.45f, "Surface", "", 0 )
X( P_GLASS_DISP, "Glass Dispersion", STD, 0, 2, 0.4f, "Surface", "", 0 )
MEHR_COLOR( P_GLASS_TINT, "Glass", "Surface", 0.5625f, 0.08f, 1, 1, "Glass Alpha" ) // alpha = tint strength
#undef MEHR_REGION
#define MEHR_REGION MEHR_CLASSIC
X( P_HOLO, "Hologram", STD, 0, 1, 0, "Surface", "", 0 )
X( P_HOLO_SCAN, "Holo Scanlines", STD, 0, 4, 1, "Surface", "", 0 )
X( P_HOLO_FLICKER, "Holo Flicker", STD, 0, 2, 0.5f, "Surface", "", 0 )
X( P_HOLO_SPLIT, "Holo Split", STD, 0, 4, 1, "Surface", "", 0 )
X( P_HOLO_GLITCH, "Holo Glitch", STD, 0, 4, 0, "Surface", "", 0 )
MEHR_COLOR( P_HOLO_COL, "Holo", "Surface", 0.5284603f, 1, 0.7627587f, 1, "Holo Alpha" ) // alpha x hologram colour
X( P_HEAT, "Heat Glow", STD, 0, 10, 0, "Surface", "", 0 )
X( P_HEAT_TEMP, "Heat Temp", STD, 600, 12000, 1900, "Surface", "", 0 )
X( P_HEAT_SPREAD, "Heat Spread", STD, 0, 1, 0.7f, "Surface", "", 0 )
X( P_PATTERN, "Pattern", OPT, 0, 3, 0, "Surface", "Off|Stripes|Cells|Noise", 0 )
X( P_PAT_AMOUNT, "Pattern Amount", STD, 0, 10, 2, "Surface", "", 0 )
X( P_PAT_SCALE, "Pattern Scale", STD, 0.2f, 40, 5, "Surface", "", 0 )
X( P_PAT_SPEED, "Pattern Speed", STD, -4, 4, 0.5f, "Surface", "", 0 )
MEHR_COLOR( P_PAT_COL, "Pattern", "Surface", 0.5208333f, 0.8f, 1, 1, "Pattern Alpha" ) // alpha x Pattern Amount
X( P_PAT_BEAT, "Pattern Beat", STD, 0, 4, 0, "Surface", "", 0 )
X( P_PAT_AUDIO, "Pattern Audio", STD, 0, 4, 0, "Surface", "", 0 )
#undef MEHR_REGION
#define MEHR_REGION 0
// PARTS: 8 slots; renamed "<material> <field>" at load, unused slots hidden
#define MEHR_PART( n )                                                                \
	X( P_PART##n##_VISIBLE, "Part " #n " Visible", BOOL, 0, 1, 1, "Parts", "", 0 )     \
	X( P_PART##n##_HUE, "Part " #n " Hue Shift", STD, 0, 1, 0, "Parts", "", 0 )        \
	X( P_PART##n##_GLOW, "Part " #n " Glow", STD, 0, 5, 0, "Parts", "", 0 )            \
	X( P_PART##n##_SKIN, "Part " #n " Skin", BOOL, 0, 1, 0, "Parts", "", 0 )            \
	X( P_PART##n##_SURFACE, "Part " #n " Surface", STD, 0, 1, 1, "Parts", "", 0 )
MEHR_PART( 1 )
MEHR_PART( 2 )
MEHR_PART( 3 )
MEHR_PART( 4 )
MEHR_PART( 5 )
MEHR_PART( 6 )
MEHR_PART( 7 )
MEHR_PART( 8 )
#undef MEHR_PART
// SHAPE: the triangle-soup vertex stage (fitted object space). Shatter moves whole shards.
#undef MEHR_REGION
#define MEHR_REGION 0
X( P_SHATTER, "Shatter", EVENT, 0, 1, 0, "Shape", "", 0 )
X( P_REFORM, "Reform", EVENT, 0, 1, 0, "Shape", "", 0 )
X( P_SHATTER_BEAT, "Shatter On Beat", OPT, 0, 4, 0, "Shape", "Off|Every Beat|Every 2|Every 4|Every 8", 0 )
X( P_SHARD_COUNT, "Shard Count", STD, 8, 512, 64, "Shape", "", 0 )
// W2 text / logos: Letters = whole letters fly (W21); Pieces = each letter breaks into solid chunks (Shard Count shared over the
// letters by area, cut faces = the Core part); Pieces + Letters = the chunks fly, each letter's chunks with one common kick.
// Loaded models: shards as before (any mode).
X( P_SHATTER_MODE, "Shatter Mode", OPT, 0, 2, 1, "Shape", "Letters|Pieces|Pieces + Letters", 0 )
X( P_SHATTER_FORCE, "Shatter Force", STD, 0, 10, 1.5f, "Shape", "", 0 )
X( P_SHATTER_GRAVITY, "Shatter Gravity", STD, 0, 20, 2, "Shape", "", 0 )
X( P_SHARD_SPIN, "Shard Spin", STD, 0, 20, 3, "Shape", "", 0 )
X( P_REFORM_TIME, "Reform Time", STD, 0.05f, 10, 1.2f, "Shape", "", 0 )
// W26 Stage 9: Bounce = the shards fall, bounce on the floor (and the wall when the catcher is a wall), slide and tumble; Reform
// flies them back along the same path. LoadEhr W2: every bundle (a logo's pieces bounce in LoadEhr too); Physics Speed stays Wow
X( P_SHATTER_PHYSICS, "Shatter Physics", OPT, 0, 1, 0, "Shape", "Analytic|Bounce", 0 )
X( P_BOUNCINESS, "Bounciness", STD, 0, 1, 0.5f, "Shape", "", 0 )
X( P_FLOOR_FRICTION, "Floor Friction", STD, 0, 1, 0.3f, "Shape", "", 0 )
#undef MEHR_REGION
#define MEHR_REGION MEHR_WOW
X( P_PHYSICS_SPEED, "Physics Speed", STD, 0, 2, 0.35f, "Shape", "", 0 ) // W28: shard flight / bounce / reform pace (1 = W26)
#undef MEHR_REGION
#define MEHR_REGION MEHR_CLASSIC
X( P_SLICE_COUNT, "Slice Count", STD, 2, 64, 12, "Shape", "", 0 )
X( P_SLICE_GAP, "Slice Gap", STD, 0, 2, 0, "Shape", "", 0 )
X( P_SLICE_AXIS, "Slice Axis", OPT, 0, 2, 1, "Shape", "X|Y|Z", 0 )
X( P_SLICE_WAVE, "Slice Wave", STD, 0, 4, 0, "Shape", "", 0 )
X( P_VOXEL, "Voxel", STD, 0, 1, 0, "Shape", "", 0 )
X( P_VOXEL_SIZE, "Voxel Size", STD, 0.005f, 0.5f, 0.06f, "Shape", "", 0 )
X( P_LOW_POLY, "Low Poly", STD, 0, 1, 0, "Shape", "", 0 )
X( P_LOW_POLY_SIZE, "Low Poly Size", STD, 0.01f, 0.8f, 0.15f, "Shape", "", 0 )
// the low-poly grid turns in mask space (static angle + integrated spin, degrees and degrees per second)
X( P_LP_ROT_X, "LowPoly Rot X", STD, -180, 180, 0, "Shape", "", 0 )
X( P_LP_ROT_Y, "LowPoly Rot Y", STD, -180, 180, 0, "Shape", "", 0 )
X( P_LP_ROT_Z, "LowPoly Rot Z", STD, -180, 180, 0, "Shape", "", 0 )
X( P_LP_SPIN_X, "LowPoly Spin X", STD, -30, 30, 0, "Shape", "", 0 )
X( P_LP_SPIN_Y, "LowPoly Spin Y", STD, -30, 30, 0, "Shape", "", 0 )
X( P_LP_SPIN_Z, "LowPoly Spin Z", STD, -30, 30, 0, "Shape", "", 0 )
// facets (one per cell of the rotated grid) tumble rigidly about their own centres, in mask space
X( P_TUMBLE, "Facet Tumble", OPT, 0, 2, 0, "Shape", "Off|Together|Separate", 0 )
X( P_TUMBLE_X, "Tumble X", STD, -30, 30, 0, "Shape", "", 0 )
X( P_TUMBLE_Y, "Tumble Y", STD, -30, 30, 0, "Shape", "", 0 )
X( P_TUMBLE_Z, "Tumble Z", STD, -30, 30, 0, "Shape", "", 0 )
X( P_TUMBLE_RANDOM, "Tumble Random", STD, 0, 1, 0.5f, "Shape", "", 0 )
X( P_TUMBLE_SWAY, "Tumble Sway", STD, 0, 1, 0, "Shape", "", 0 )
X( P_TUMBLE_BEAT, "Tumble On Beat", OPT, 0, 4, 0, "Shape", "Off|Every Beat|Every 2|Every 4|Every 8", 0 )
X( P_TUMBLE_FLIP, "Tumble Beat Flip", OPT, 0, 1, 0, "Shape", "90|180", 0 )
X( P_FACET_GAP, "Facet Gap", STD, 0, 1, 0, "Shape", "", 0 )
X( P_TUMBLE_RESET, "Tumble Reset", EVENT, 0, 1, 0, "Shape", "", 0 )
X( P_BLOW_DUST, "Blow To Dust", EVENT, 0, 1, 0, "Shape", "", 0 )
X( P_DUST_REFORM, "Dust Reform", EVENT, 0, 1, 0, "Shape", "", 0 )
X( P_DUST_BEAT, "Dust On Beat", OPT, 0, 4, 0, "Shape", "Off|Every Beat|Every 2|Every 4|Every 8", 0 )
X( P_DUST_HOLD, "Dust Hold", STD, 0, 30, 0, "Shape", "", 0 ) // seconds out before it reforms by itself; 0 = until Reform
X( P_DUST, "Dust", STD, 0, 1, 0, "Shape", "", 0 )
X( P_DUST_COUNT, "Dust Count", OPT, 0, 4, 2, "Shape", "25k|50k|100k|200k|400k", 0 )
X( P_DUST_WIND, "Dust Wind", STD, 0, 6, 1, "Shape", "", 0 )
X( P_DUST_SIZE, "Dust Size", STD, 0.3f, 8, 1.5f, "Shape", "", 0 )
X( P_CONTOURS, "Contours", STD, 0, 10, 0, "Shape", "", 0 )
X( P_CONTOUR_MODE, "Contour Mode", OPT, 0, 4, 1, "Shape", "X|Y|Z|Height Map|Radial", 0 )
X( P_CONTOUR_SPACING, "Contour Spacing", STD, 0.005f, 0.5f, 0.06f, "Shape", "", 0 )
X( P_CONTOUR_WIDTH, "Contour Width", STD, 0.01f, 1, 0.15f, "Shape", "", 0 )
X( P_CONTOUR_SPEED, "Contour Speed", STD, -4, 4, 0.3f, "Shape", "", 0 )
MEHR_COLOR( P_CONTOUR_COL, "Contour", "Shape", 0.07843137f, 0.85f, 1, 1, "Contour Alpha" ) // alpha x Contours
// DEFORM (W2, every bundle): bends the model, text and logos (any slot) in a fixed order: Taper, Twist, Bend, Squash, Shear,
// Bulge, Spherize, Noise Warp, Ripple, Melt / Drip, Glitch, Mirror Fold, Jelly; shards / pieces fly from the deformed shape.
// Drawn by the DEFORM program twins (every deform at 0 = the plain programs). Not deformed: the sim obstacle, the eye finder, the
// liquid SDF, the dust points (they stay the rigid model). Deform Axis .. Drip moved here from Shape (names and meaning as before).
#undef MEHR_REGION
#define MEHR_REGION 0
X( P_DEFORM_AMOUNT, "Deform Amount", STD, 0, 2, 1, "Deform", "", 0 )     // master on every deform (> 1 is too much on purpose)
X( P_DEFORM_AXIS, "Deform Axis", OPT, 0, 2, 1, "Deform", "X|Y|Z", 0 )
X( P_TAPER, "Taper", STD, -2, 2, 0, "Deform", "", 0 )                    // across the axis: grows (+) / shrinks (-) along it
X( P_TWIST, "Twist", STD, -8, 8, 0, "Deform", "", 0 )
X( P_BEND, "Bend", STD, -4, 4, 0, "Deform", "", 0 )
X( P_SQUASH, "Squash", STD, -1, 1, 0, "Deform", "", 0 )                  // + shorter along the axis and wider (volume kept), - stretched
X( P_SQUASH_BEAT, "Squash Beat", STD, 0, 2, 0, "Deform", "", 0 )         // a squash kick on each beat (the Beat Bounce envelope)
X( P_SHEAR, "Shear", STD, -2, 2, 0, "Deform", "", 0 )                    // slides along the axis' first perpendicular
X( P_BULGE, "Bulge", STD, -1, 2, 0, "Deform", "", 0 )                    // rounds the middle out (negative: pinches it in)
X( P_SPHERIZE, "Spherize", STD, 0, 1, 0, "Deform", "", 0 )               // toward a sphere of the fitted radius
X( P_NOISE_WARP, "Noise Warp", STD, 0, 2, 0, "Deform", "", 0 )           // smooth 3D noise displacement
X( P_WARP_SCALE, "Warp Scale", STD, 0.2f, 10, 2, "Deform", "", 0 )       // noise cells per fitted unit
X( P_WARP_SPEED, "Warp Speed", STD, 0, 4, 0.3f, "Deform", "", 0 )        // noise cells per second
X( P_RIPPLE, "Ripple", STD, 0, 0.5f, 0, "Deform", "", 0 )
X( P_RIPPLE_FREQ, "Ripple Freq", STD, 1, 80, 14, "Deform", "", 0 )
X( P_RIPPLE_SPEED, "Ripple Speed", STD, -20, 20, 4, "Deform", "", 0 )
X( P_RIPPLE_BEAT, "Ripple Beat", STD, 0, 4, 0, "Deform", "", 0 )
X( P_MELT, "Melt", STD, 0, 1, 0, "Deform", "", 0 )
X( P_DRIP, "Drip", STD, 0, 3, 1, "Deform", "", 0 )
X( P_GLITCH_SHIFT, "Glitch Shift", STD, 0, 1, 0, "Deform", "", 0 )       // bands across the axis jump sideways
X( P_GLITCH_BANDS, "Glitch Bands", STD, 2, 64, 12, "Deform", "", 0 )
X( P_GLITCH_RATE, "Glitch Rate", STD, 0, 30, 6, "Deform", "", 0 )        // jumps (steps) per second
X( P_MIRROR_FOLD, "Mirror Fold", OPT, 0, 3, 0, "Deform", "Off|X|X+Y|X+Y+Z", 0 ) // kaleidoscope object: folded across the centre planes
X( P_JELLY, "Jelly", STD, 0, 2, 0, "Deform", "", 0 )                     // lags behind its own spin / move / bounce, springs back
X( P_AUDIO_DEFORM, "Audio > Deform", STD, 0, 4, 0, "Deform", "", 0 )     // the audio level adds to Deform Amount
// WIREFRAME (W5, every bundle; Wireframe Mix / Wire Thickness moved here from the classic FX group, names kept). Over Surface with
// Color Mix / Glow / Bloom / Fade 0, Back Hidden, Edges All = the W1 look on the plain programs; anything else draws through the WIRE
// program twins (lines from the triangle-soup barycentrics; Feature Edges from per-triangle edge flags built off-thread).
// LoadEhr W6 (Amrit: "At full."): Wire Mode defaults to Off and Wireframe Mix to 1, so a fresh clip shows no wire and any mode picked
// draws at full strength.
// W6 Stage 1 (Amrit: "Can we have points only also?"): Points Only / Points + Wire draw round sprites (Wire Thickness = their size) in the
// lines' look (Wire Color, Glow, Bloom, Fade, Back) through the POINTS twins; every point rides its triangle
X( P_WIRE_MODE, "Wire Mode", OPT, 0, 5, 0, "Wireframe", "Off|Over Surface|Wire Only|Wire + Dark Fill|Points Only|Points + Wire", 0 )
X( P_WIRE_MIX, "Wireframe Mix", STD, 0, 1, 1, "Wireframe", "", 0 )      // the wire's strength (W1: also fades the fill)
X( P_WIRE_THICKNESS, "Wire Thickness", STD, 0.2f, 8, 1, "Wireframe", "", 0 ) // px, constant on screen
X( P_WIRE_BACK, "Wire Back", OPT, 0, 1, 0, "Wireframe", "Hidden|X-Ray", 0 ) // Hidden: the solid's own back lines are hidden
X( P_WIRE_EDGES, "Wire Edges", OPT, 0, 1, 0, "Wireframe", "All Triangles|Feature Edges", 0 ) // Feature: creases > 20 deg + open edges
MEHR_COLOR( P_WIRE_COL, "Wire", "Wireframe", 0.5f, 1, 1, 0, "Wire Color Mix" ) // alpha: the picked colour vs the surface colour
X( P_WIRE_GLOW, "Wire Glow", STD, 0, 10, 0, "Wireframe", "", 0 )        // the lines' own light (added at alpha 0, past white)
X( P_WIRE_BLOOM, "Wire Bloom", STD, 0, 10, 0, "Wireframe", "", 0 )      // a halo round the lines only
X( P_WIRE_BLOOM_SIZE, "Wire Bloom Size", STD, 0, 1, 0.5f, "Wireframe", "", 0 )
X( P_WIRE_FADE, "Wire Fade", STD, 0, 1, 0, "Wireframe", "", 0 )         // lines fade with distance from the camera (across the model)
// W6: where the points sit: every vertex (welded), the vertices on the feature edges, or evenly over the area (sparse models still read)
X( P_POINT_SOURCE, "Point Source", OPT, 0, 4, 3, "Wireframe", "Vertices|Feature Vertices|Surface 25k|Surface 100k|Surface 400k", 0 )
X( P_POINT_TWINKLE, "Point Twinkle", STD, 0, 1, 0, "Wireframe", "", 0 ) // each point flickers on its own (~1 twinkle / s, this deep)
#undef MEHR_REGION
#define MEHR_REGION MEHR_CLASSIC
// COPIES: instanced clones (LODs by screen size), echo trails, ghost poses
X( P_CLONE_LAYOUT, "Clone Layout", OPT, 0, 4, 0, "Copies", "Off|Ring|Grid|Tunnel|Carousel", 0 )
X( P_CLONE_COUNT, "Clone Count", STD, 1, 64, 8, "Copies", "", 0 )
X( P_CLONE_SPACING, "Clone Spacing", STD, 0.2f, 12, 2.4f, "Copies", "", 0 )
X( P_CLONE_SPIN_STEP, "Clone Spin Step", STD, -180, 180, 0, "Copies", "", 0 )
X( P_CLONE_HUE, "Clone Hue Spread", STD, 0, 1, 0, "Copies", "", 0 )
X( P_CLONE_ORBIT, "Clone Orbit", STD, -30, 30, 0, "Copies", "", 0 ) // degrees per second, the same scale as Spin
X( P_CLONE_SCALE, "Clone Scale", STD, 0.05f, 3, 0.6f, "Copies", "", 0 )
X( P_TUNNEL_FADE, "Tunnel Fade", STD, 0, 1, 0.25f, "Copies", "", 0 ) // share of the tunnel the rings fade in (back) + out (front)
X( P_KEEP_ORIGINAL, "Keep Original", BOOL, 0, 1, 1, "Copies", "", 0 )
X( P_LOD_BIAS, "LOD Bias", STD, 0.1f, 4, 1, "Copies", "", 0 )
X( P_TRAIL_ECHOES, "Trail Echoes", STD, 0, 7, 0, "Copies", "", 0 )
X( P_TRAIL_DELAY, "Trail Delay", STD, 1, 30, 4, "Copies", "", 0 )
X( P_TRAIL_DECAY, "Trail Decay", STD, 0, 1, 0.7f, "Copies", "", 0 )
X( P_TRAIL_OFFSET_X, "Trail Offset X", STD, -0.1f, 0.1f, 0, "Copies", "", 0 )
X( P_TRAIL_OFFSET_Y, "Trail Offset Y", STD, -0.1f, 0.1f, 0, "Copies", "", 0 )
X( P_GHOSTS, "Ghosts", STD, 0, 8, 0, "Copies", "", 0 )
X( P_GHOST_SPACING, "Ghost Spacing", STD, 1, 30, 5, "Copies", "", 0 )
X( P_GHOST_FADE, "Ghost Fade", STD, 0, 1, 0.6f, "Copies", "", 0 )
// W15: how the trail copies (Trail Echoes and Ghosts) look. Smear fills the gaps, Blur softens with age (echoes),
// Dust breaks them into drifting grain; Fade Curve shapes the fade (Decay / Ghost Fade stay the master).
X( P_TRAIL_SMEAR, "Trail Smear", STD, 0, 1, 0, "Copies", "", 0 )
X( P_TRAIL_BLUR, "Trail Blur", STD, 0, 2, 0, "Copies", "", 0 )
X( P_TRAIL_DUST, "Trail Dust", STD, 0, 1, 0, "Copies", "", 0 )
X( P_TRAIL_GRAIN, "Trail Grain", STD, 1, 12, 2.5f, "Copies", "", 0 )         // grain size, pixels at 1080p
X( P_TRAIL_DRIFT, "Trail Drift", STD, 0, 2, 0.15f, "Copies", "", 0 )         // screen heights per second
X( P_TRAIL_DRIFT_DIR, "Trail Drift Dir", STD, -180, 180, 90, "Copies", "", 0 ) // degrees on screen, 0 = right, 90 = up
X( P_TRAIL_CURVE, "Trail Fade Curve", OPT, 0, 2, 0, "Copies", "Linear|Soft|Long Tail", 0 )
X( P_TRAIL_GLOW, "Trail Glow", STD, 0, 10, 0, "Copies", "", 0 )
MEHR_COLOR( P_TRAIL_COL, "Trail", "Copies", 0.55f, 0.6f, 1, 1, "Trail Alpha" ) // alpha x Glow and x Color Mix
X( P_TRAIL_COL_MIX, "Trail Color Mix", STD, 0, 1, 0, "Copies", "", 0 )
#undef MEHR_REGION
#define MEHR_REGION 0
// LIGHTING
X( P_KEY_AZIMUTH, "Key Azimuth", STD, -180, 180, 35, "Lighting", "", 0 )
X( P_KEY_ELEVATION, "Key Elevation", STD, -90, 90, 35, "Lighting", "", 0 )
X( P_KEY_INTENSITY, "Key Intensity", STD, 0, 20, 3, "Lighting", "", 0 )
MEHR_COLOR( P_KEY_COL, "Key", "Lighting", 0.1041667f, 0.08f, 1, 1, "Key Alpha" ) // alpha x Key Intensity
X( P_AMBIENT, "Ambient", STD, 0, 1, 0.03f, "Lighting", "", 0 )
X( P_ENV_INTENSITY, "Env Intensity", STD, 0, 10, 1, "Lighting", "", 0 )
X( P_ENV_SOURCE, "Env Source", OPT, 0, 2, 0, "Lighting", "Studio|Input (FX)|Off", 0 )
X( P_ENV_ROTATE, "Env Rotate", STD, -180, 180, 0, "Lighting", "", 0 )
X( P_EXPOSURE, "Exposure", STD, -5, 5, 0, "Lighting", "", 0 )
X( P_TONEMAP, "Tonemap", OPT, 0, 2, 0, "Lighting", "ACES|Filmic|None", 0 )
// VOLUME LIGHT: a smoky medium lit by the volume lights; shared medium first, then Light A, B and C
X( P_VOL_METHOD, "Vol Method", OPT, 0, 1, 0, "Volume", "Raymarch|Screen Rays", 0 )
X( P_HAZE, "Haze", STD, 0, 2, 0.12f, "Volume", "", 0 )
X( P_SMOKE_AMOUNT, "Smoke Amount", STD, 0, 4, 1, "Volume", "", 0 )
X( P_SMOKE_SCALE, "Smoke Scale", STD, 0.1f, 10, 1.5f, "Volume", "", 0 )
X( P_SMOKE_SPEED, "Smoke Speed", STD, 0, 4, 0.3f, "Volume", "", 0 )
X( P_SMOKE_TURB, "Smoke Turbulence", STD, 0, 4, 1, "Volume", "", 0 )
X( P_SMOKE_RISE, "Smoke Rise", STD, -2, 2, 0.15f, "Volume", "", 0 )
X( P_GLOW_CLOUD, "Glow Cloud", STD, 0, 10, 1, "Volume", "", 0 )
X( P_SMOKE_AMBIENT, "Smoke Ambient", STD, 0, 4, 0, "Volume", "", 0 ) // W23: the smoke scatters the key's colour everywhere (no beam needed)
X( P_SMOKE_KEY, "Smoke Key Light", STD, 0, 4, 0, "Volume", "", 0 )  // W23: the key light lights the smoke, with the mask's shadow
X( P_VOLUME_SIZE, "Volume Size", STD, 0.5f, 12, 3.5f, "Volume", "", 0 )
X( P_SCATTER_G, "Scatter G", STD, -0.9f, 0.99f, 0.6f, "Volume", "", 0 )
X( P_SHAFT_LENGTH, "Shaft Length", STD, 0.2f, 20, 6, "Volume", "", 0 )
X( P_MOTES, "Motes", STD, 0, 4, 0, "Volume", "", 0 )
X( P_VOL_QUALITY, "Vol Quality", OPT, 0, 3, 1, "Volume", "16|32|64|128", 0 )
X( P_VOL_RES, "Vol Res", OPT, 0, 2, 1, "Volume", "Full|Half|Quarter", 0 )
X( P_FOG_BLEND, "Fog Blend", OPT, 0, 1, 0, "Volume", "Add|Over", 0 )
X( P_DRIFT_RETURN, "Drift Return", STD, 0, 3, 0.4f, "Volume", "", 0 ) // s: a light's Hue Drift at 0 glides home (A, B, C, spots)
// SMOKE SIM (W20): a real 3D fluid (velocity, pressure, obstacle = the mask) in a grid around the mask. Smoke Engine Noise =
// the W3 noise smoke, untouched. Visible, like every medium, only while a volume light is on. Rates: smoke / s, heat / s.
X( P_SMOKE_ENGINE, "Smoke Engine", OPT, 0, 2, 0, "Smoke Sim", "Noise|Sim|Sim + Detail", 0 )
X( P_SIM_RES, "Sim Res", OPT, 0, 3, 1, "Smoke Sim", "48|64|96|128", 0 )
X( P_SIM_BOX, "Sim Box", STD, 1, 6, 2.5f, "Smoke Sim", "", 0 ) // grid edge in mask sizes
X( P_SIM_ITER, "Sim Iterations", STD, 10, 40, 20, "Smoke Sim", "", 0 )
X( P_BUOYANCY, "Buoyancy", STD, 0, 10, 2, "Smoke Sim", "", 0 )
X( P_HEAT_DECAY, "Heat Decay", STD, 0, 5, 0.5f, "Smoke Sim", "", 0 )
X( P_VORTICITY, "Vorticity", STD, 0, 10, 2, "Smoke Sim", "", 0 )
X( P_SMOKE_FADE, "Smoke Fade", STD, 0, 3, 0.6f, "Smoke Sim", "", 0 )
X( P_WIND_X, "Wind X", STD, -3, 3, 0, "Smoke Sim", "", 0 )
X( P_WIND_Y, "Wind Y", STD, -3, 3, 0, "Smoke Sim", "", 0 )
X( P_WIND_Z, "Wind Z", STD, -3, 3, 0, "Smoke Sim", "", 0 )
X( P_SIM_TURB, "Sim Turbulence", STD, 0, 2, 0.5f, "Smoke Sim", "", 0 ) // W24: max 2 (was 5)
X( P_EMIT_EYES, "Emit Eyes", STD, 0, 10, 0, "Smoke Sim", "", 0 )
X( P_EYES_HEAT, "Eyes Heat", STD, 0, 5, 1, "Smoke Sim", "", 0 )
X( P_EMIT_BEHIND, "Emit Behind", STD, 0, 10, 0, "Smoke Sim", "", 0 )
X( P_BEHIND_HEAT, "Behind Heat", STD, 0, 5, 1, "Smoke Sim", "", 0 )
X( P_EMIT_RIM, "Emit Rim", STD, 0, 10, 0, "Smoke Sim", "", 0 )
X( P_RIM_HEAT, "Rim Heat", STD, 0, 5, 1, "Smoke Sim", "", 0 )
X( P_EMIT_INSIDE, "Emit Inside", STD, 0, 10, 0, "Smoke Sim", "", 0 )
X( P_INSIDE_HEAT, "Inside Heat", STD, 0, 5, 1, "Smoke Sim", "", 0 )
X( P_EMIT_OFFSET, "Emit Offset", STD, 0, 0.5f, 0, "Smoke Sim", "", 0 ) // W23: mask radii; Rim and Eyes emit this far out of the surface
X( P_BEAT_PUFF, "Beat Puff", OPT, 0, 4, 0, "Smoke Sim", "Off|Every Beat|Every 2|Every 4|Every 8", 0 )
X( P_PUFF_AMOUNT, "Puff Amount", STD, 0, 10, 3, "Smoke Sim", "", 0 )
X( P_AUDIO_EMIT, "Audio > Emit", STD, 0, 4, 0, "Smoke Sim", "", 0 )
X( P_SIM_RESET, "Sim Reset", EVENT, 0, 1, 0, "Smoke Sim", "", 0 )
MEHR_COLOR( P_SMOKE_COL, "Smoke", "Smoke Sim", 0, 0, 1, 1, "Smoke Alpha" ) // alpha = the sim smoke's density
X( P_SMOKE_CONFINE, "Smoke Confine", STD, 0, 1, 0, "Smoke Sim", "", 0 ) // 0 = free; up: a soft wall ~1.3 mask radii out
#if LOADEHR_FX
#undef MEHR_REGION
#define MEHR_REGION MEHR_CLASSIC
X( P_SIM_PROJECT, "Project Input", STD, 0, 1, 0, "Smoke Sim", "", 0 ) // the incoming clip projected onto the smoke (W28: LoadEhr FX only, 512 cap)
#undef MEHR_REGION
#define MEHR_REGION 0
#endif
// LoadEhr W2: Trip moved to the Wow bundles (W2's Deform group went into every bundle; both FX carriers can only fit it if one
// shared group goes to one side: 2 x shared + extras <= 2 x 512)
#undef MEHR_REGION
#define MEHR_REGION MEHR_WOW
// TRIP (W24): psychedelic looks on top of the smoke (Sim, and the Noise engine where it makes sense). Everything defaults
// off. Trip Master scales every look (0 = all off; the top end is too much on purpose). Medium looks = Raymarch only.
X( P_TRIP_MASTER, "Trip Master", STD, 0, 2, 1, "Trip", "", 0 )
X( P_COLOUR_BY, "Colour By", OPT, 0, 6, 0, "Trip", "Off|Density|Heat|Speed|Swirl|Age|Height", 0 ) // Noise: Height, else its density
X( P_TRIP_PALETTE, "Trip Palette", OPT, 0, 5, 0, "Trip", "Rainbow|Acid|Neon|Fire|Ice|Picker Pair", 0 )
X( P_PALETTE_SHIFT, "Palette Shift", STD, -2, 2, 0, "Trip", "", 0 )   // turns per second (the palette cycles)
X( P_PALETTE_SPREAD, "Palette Spread", STD, 0, 4, 1, "Trip", "", 0 ) // how many times the palette repeats over the field
MEHR_COLOR( P_TRIP_A, "Trip A", "Trip", 0.83, 1, 1, 1, "Trip A Alpha" ) // Picker Pair: A -> B -> A; alpha = that colour's strength
MEHR_COLOR( P_TRIP_B, "Trip B", "Trip", 0.45, 1, 1, 1, "Trip B Alpha" )
X( P_THIN_FILM, "Thin Film", STD, 0, 2, 0, "Trip", "", 0 )                // oil-slick iridescence (view angle x density)
X( P_THIN_FILM_THICK, "Thin Film Thick", STD, 0.1, 3, 1, "Trip", "", 0 )
X( P_PRISM, "Prism", STD, 0, 2, 0, "Trip", "", 0 )                        // chromatic dispersion: rainbow fringes
X( P_PRISM_ANGLE, "Prism Angle", STD, -180, 180, 0, "Trip", "", 0 )       // 0 = out from the mask, 90 = round it
X( P_KALEIDO, "Kaleido Smoke", STD, 1, 16, 1, "Trip", "", 0 )             // N-way mirror fold of the volume (1 = off)
X( P_KALEIDO_TWIST, "Kaleido Twist", STD, -360, 360, 0, "Trip", "", 0 )   // degrees per mask radius out from the axis
X( P_KALEIDO_SPIN, "Kaleido Spin", STD, -30, 30, 0, "Trip", "", 0 )       // degrees per second
X( P_FRACTAL_FOG, "Fractal Fog", STD, 0, 2, 0, "Trip", "", 0 )            // a folded 3D fractal carved into the smoke
X( P_FRACTAL_SCALE, "Fractal Scale", STD, 0.2, 4, 1, "Trip", "", 0 )      // mask radii
X( P_FRACTAL_ITER, "Fractal Iter", STD, 1, 6, 3, "Trip", "", 0 )
X( P_FRACTAL_MORPH, "Fractal Morph", STD, -30, 30, 6, "Trip", "", 0 )     // degrees per second (the folds turn)
X( P_NEON_VEINS, "Neon Veins", STD, 0, 10, 0, "Trip", "", 0 )             // glowing filaments at the curls' edges
X( P_VEIN_WIDTH, "Vein Width", STD, 0, 1, 0.4f, "Trip", "", 0 )
MEHR_COLOR( P_VEIN_COL, "Vein", "Trip", 0.5, 1, 1, 1, "Vein Alpha" )
X( P_SMOKE_CONTOURS, "Smoke Contours", STD, 0, 10, 0, "Trip", "", 0 )     // glowing iso-density rings (palette colours)
X( P_CONTOUR_BANDS, "Contour Bands", STD, 2, 32, 8, "Trip", "", 0 )
X( P_BAND_SHARP, "Band Sharpness", STD, 0, 1, 0.6f, "Trip", "", 0 )
X( P_BAND_SCROLL, "Band Scroll", STD, -4, 4, 0.3f, "Trip", "", 0 )       // bands per second
// W25: Lines = anti-aliased iso-lines of the smoke's optical depth drawn on the fog (screen-space fwidth, crisp at any res);
// Bands = the W24 glowing iso-density bands in the volume. Line Width in output pixels; Line Glow = the soft halo round them
X( P_CONTOUR_STYLE, "Contour Style", OPT, 0, 1, 0, "Trip", "Lines|Bands", 0 )
X( P_LINE_WIDTH, "Line Width", STD, 0.5, 6, 1.5f, "Trip", "", 0 )
X( P_LINE_GLOW, "Line Glow", STD, 0, 4, 1, "Trip", "", 0 )
X( P_BLACKLIGHT, "Blacklight", STD, 0, 10, 0, "Trip", "", 0 )             // the smoke glows by itself (no light needed)
X( P_BLACKLIGHT_BY, "Blacklight By", OPT, 0, 1, 0, "Trip", "Density|Heat", 0 )
MEHR_COLOR( P_BL_COL, "Blacklight", "Trip", 0.76, 0.85, 1, 1, "Blacklight Alpha" )
X( P_FIRE_RING, "Fire Ring", EVENT, 0, 1, 0, "Trip", "", 0 )              // Sim: a toroidal vortex ring
X( P_RING_ON_BEAT, "Ring On Beat", OPT, 0, 4, 0, "Trip", "Off|Every Beat|Every 2|Every 4|Every 8", 0 )
X( P_RING_FROM, "Ring From", OPT, 0, 2, 0, "Trip", "Eyes|Mouth|Behind", 0 )
X( P_RING_SIZE, "Ring Size", STD, 0.05f, 1, 0.3f, "Trip", "", 0 )         // mask radii
X( P_RING_SPEED, "Ring Speed", STD, 0, 6, 2, "Trip", "", 0 )              // mask radii per second
X( P_RING_SPIN, "Ring Spin", STD, -10, 10, 0, "Trip", "", 0 )             // corkscrew round the travel axis
X( P_RING_SMOKE, "Ring Smoke", STD, 0, 10, 4, "Trip", "", 0 )
X( P_SMOKE_GHOSTS, "Smoke Ghosts", STD, 0, 4, 0, "Trip", "", 0 )          // Sim: smoke copies of the mask per second
X( P_GHOST_OFFSET, "Ghost Offset", STD, 0, 3, 1, "Trip", "", 0 )          // mask radii the copy peels off to
X( P_GHOST_SCALE, "Ghost Scale", STD, 0.3f, 3, 1.3f, "Trip", "", 0 )      // the copy's size when it lets go
X( P_GHOST_LIFE, "Ghost Fade Time", STD, 0.1f, 5, 1.5f, "Trip", "", 0 )   // seconds the copy is painted while it peels off
X( P_WHIRLPOOL, "Whirlpool", STD, -5, 5, 0, "Trip", "", 0 )               // Sim: swirl round the mask's up axis
X( P_HYPERSPACE, "Hyperspace", STD, -5, 5, 0, "Trip", "", 0 )             // Sim: out from (+) / into (-) the camera axis
X( P_HUE_ECHO, "Hue Echo", STD, 0, 1, 0, "Trip", "", 0 )                  // rainbow afterimages: how long they last
X( P_ECHO_HUE_STEP, "Echo Hue Step", STD, -0.1f, 0.1f, 0.02f, "Trip", "", 0 ) // turns per 60 fps frame
X( P_TIME_WARP, "Time Warp", STD, 0, 2, 1, "Trip", "", 0 )                // smoke speed (0 = frozen; not scaled by Trip Master)
X( P_FREEZE, "Freeze", EVENT, 0, 1, 0, "Trip", "", 0 )                    // hold: the smoke stands still
#undef MEHR_REGION
#define MEHR_REGION MEHR_WOW
// FIRE (W26 Stage 3): combustion in the smoke sim (it switches the sim on). The sim's emitters (Emit Eyes / Behind / Rim /
// Inside, with their Heat) give FUEL; it burns above the ignition heat into hot gas (blackbody light, Flame Temp) and soot
// (the smoke), cools and rises. Fire Light = a flickering light on the mask from the flames.
X( P_FIRE, "Fire", STD, 0, 1, 0, "Fire", "", 0 )                          // 0 off; 1 = the emitters give only fuel
X( P_FIRE_SPEED, "Fire Speed", STD, 0, 2, 0.5f, "Fire", "", 0 )           // W28: the fire's pace (1 = the W26 pace, 2 = frantic)
X( P_BURN_RATE, "Burn Rate", STD, 0.2f, 10, 2, "Fire", "", 0 )            // per second, once the fuel is hot enough (W28: default 3 -> 2)
X( P_FLAME_TEMP, "Flame Temp", STD, 1000, 6000, 2000, "Fire", "", 0 )      // kelvin of the hottest gas: red -> yellow -> white
X( P_FLAME_HEIGHT, "Flame Height", STD, 0, 4, 1, "Fire", "", 0 )           // slower cooling + more lift = taller flames
X( P_FLAME_GLOW, "Flame Glow", STD, 0, 10, 2, "Fire", "", 0 )
X( P_FIRE_SMOKE, "Fire Smoke", STD, 0, 2, 0.4f, "Fire", "", 0 )            // soot per burnt fuel
MEHR_COLOR( P_FLAME_TINT, "Flame Tint", "Fire", 0.6f, 0.8f, 1, 0, "Flame Tint Mix" ) // alpha = how far from the blackbody colour
X( P_FIRE_BURST, "Fire Burst", OPT, 0, 4, 0, "Fire", "Off|Every Beat|Every 2|Every 4|Every 8", 0 )
X( P_BURST_AMOUNT, "Burst Amount", STD, 0, 10, 2, "Fire", "", 0 ) // W28: default 4 -> 2
X( P_FIRE_LIGHT, "Fire Light", STD, 0, 10, 2, "Fire", "", 0 )              // the flames light the mask (flickering)
// LIGHTNING (W26 Stage 5): branching bolts, a new one per strike (button, beat or a rate), crawling over the mask's surface or
// jumping between two points. Each strike flashes onto the mask and into the smoke.
X( P_STRIKE, "Lightning Strike", EVENT, 0, 1, 0, "Lightning", "", 0 )
X( P_STRIKE_BEAT, "Strike On Beat", OPT, 0, 4, 0, "Lightning", "Off|Every Beat|Every 2|Every 4|Every 8", 0 )
X( P_STRIKE_RATE, "Strike Rate", STD, 0, 10, 0, "Lightning", "", 0 )      // strikes per second by themselves (crackle)
X( P_BOLT_PATH, "Bolt Path", OPT, 0, 3, 0, "Lightning", "Crawl|Eye To Eye|To Floor|To Clones", 0 )
X( P_BOLT_COUNT, "Bolt Count", STD, 1, 8, 2, "Lightning", "", 0 )         // bolts per strike
X( P_BRANCHING, "Branching", STD, 0, 1, 0.5f, "Lightning", "", 0 )
X( P_JAGGEDNESS, "Jaggedness", STD, 0, 1, 0.5f, "Lightning", "", 0 )
X( P_BOLT_LIFE, "Bolt Life", STD, 0.05f, 2, 0.4f, "Lightning", "", 0 )     // seconds a strike flickers
X( P_BOLT_WIDTH, "Bolt Width", STD, 0.5f, 6, 1.5f, "Lightning", "", 0 )    // core, pixels at 1080p
X( P_BOLT_GLOW, "Bolt Glow", STD, 0, 10, 3, "Lightning", "", 0 )
MEHR_COLOR( P_BOLT_COL, "Bolt", "Lightning", 0.64f, 0.55f, 1, 1, "Bolt Alpha" ) // alpha x the bolts' brightness
X( P_BOLT_FLASH, "Bolt Flash", STD, 0, 10, 2, "Lightning", "", 0 )         // the strike's light on the mask and in the smoke
// SWARM (W26 Stage 6): thousands of fireflies that flock (alignment, cohesion, separation from a grid round the mask, + curl
// noise) and are drawn to points on the mask's surface, so the swarm forms it; Swarm Release (or the beat) scatters them, they
// re-form after Release Time. Mask Visible off = the swarm IS the mask.
X( P_SWARM_COUNT, "Swarm Count", OPT, 0, 5, 0, "Swarm", "Off|2k|5k|10k|20k|40k", 0 )
X( P_SWARM_RELEASE, "Swarm Release", EVENT, 0, 1, 0, "Swarm", "", 0 )
X( P_RELEASE_BEAT, "Release On Beat", OPT, 0, 4, 0, "Swarm", "Off|Every Beat|Every 2|Every 4|Every 8", 0 )
X( P_RELEASE_TIME, "Release Time", STD, 0.2f, 10, 2, "Swarm", "", 0 )    // seconds scattered before they re-form
X( P_ATTRACTION, "Attraction", STD, 0, 10, 3, "Swarm", "", 0 )          // the pull to their places on the mask
X( P_FLOCK, "Flock", STD, 0, 2, 1, "Swarm", "", 0 )
X( P_SWARM_SPEED, "Swarm Speed", STD, 0, 4, 1, "Swarm", "", 0 )         // curl-noise wander
X( P_SWARM_OFFSET, "Swarm Offset", STD, 0, 1, 0.03f, "Swarm", "", 0 )   // mask units out from the surface (a shell round it)
X( P_FIREFLY_SIZE, "Firefly Size", STD, 0.5f, 8, 2, "Swarm", "", 0 )    // core, pixels at 1080p
X( P_FIREFLY_GLOW, "Firefly Glow", STD, 0, 10, 3, "Swarm", "", 0 )
X( P_TWINKLE, "Twinkle", STD, 0, 1, 0.5f, "Swarm", "", 0 )
MEHR_COLOR( P_FIREFLY_COL, "Firefly", "Swarm", 0.14f, 0.75f, 1, 1, "Firefly Alpha" ) // alpha x Firefly Glow
X( P_MASK_VISIBLE, "Mask Visible", BOOL, 0, 1, 1, "Swarm", "", 0 )
// LAVA (W26 Stage 7): a crack network (3D Voronoi in mask space) spreads over the mask from a seed point, glowing hot (blackbody,
// as Fire). Lava Split opens the mask along the cracks; Lava Light = a light inside that pours out through the gaps (a volume
// point light with its shadow: shafts in the haze). Heat Haze shimmers the picture round the cracks.
X( P_LAVA, "Lava Cracks", STD, 0, 1, 0, "Lava", "", 0 )                 // 0 off
X( P_CRACK_SPREAD, "Crack Spread", STD, 0, 1, 1, "Lava", "", 0 )        // by hand (until the next Crack Start / Reverse)
X( P_CRACK_START, "Crack Start", EVENT, 0, 1, 0, "Lava", "", 0 )        // the cracks run over the mask in Crack Time
X( P_CRACK_REVERSE, "Crack Reverse", EVENT, 0, 1, 0, "Lava", "", 0 )    // ... and heal back
X( P_CRACK_BEAT, "Crack On Beat", OPT, 0, 4, 0, "Lava", "Off|Every Beat|Every 2|Every 4|Every 8", 0 )
X( P_CRACK_TIME, "Crack Time", STD, 0.2f, 10, 2, "Lava", "", 0 )
X( P_CRACK_SCALE, "Crack Scale", STD, 0.5f, 8, 2.5f, "Lava", "", 0 )    // cells per mask unit
X( P_CRACK_WIDTH, "Crack Width", STD, 0, 1, 0.3f, "Lava", "", 0 )
X( P_LAVA_TEMP, "Lava Temp", STD, 800, 4000, 1600, "Lava", "", 0 )      // kelvin
X( P_LAVA_GLOW, "Lava Glow", STD, 0, 10, 3, "Lava", "", 0 )
MEHR_COLOR( P_LAVA_TINT, "Lava Tint", "Lava", 0.55f, 0.8f, 1, 0, "Lava Tint Mix" ) // alpha = how far from the blackbody colour
X( P_HEAT_HAZE, "Heat Haze", STD, 0, 2, 0.5f, "Lava", "", 0 )
X( P_LAVA_SPLIT, "Lava Split", STD, 0, 1, 0, "Lava", "", 0 )
X( P_LAVA_LIGHT, "Lava Light", STD, 0, 40, 10, "Lava", "", 0 )
// CAUSTICS (W26 Stage 8): light through glass (the W19 glass parts, or the whole mask as a lens) focused into moving caustic
// patterns on the catcher plane, the smoke and the mask itself (caustics mapping from a light along the key's direction).
X( P_CAUSTICS, "Caustics", STD, 0, 4, 0, "Caustics", "", 0 )
X( P_CAUSTIC_SOURCE, "Caustic Source", OPT, 0, 1, 0, "Caustics", "Glass Parts|Whole Mask", 0 )
X( P_CAUSTIC_IOR, "Caustic IOR", STD, 1, 2.5f, 1.5f, "Caustics", "", 0 )
X( P_CAUSTIC_RAINBOW, "Caustic Rainbow", STD, 0, 2, 0.5f, "Caustics", "", 0 ) // dispersion: rainbow edges
X( P_CAUSTIC_SHARP, "Caustic Sharp", STD, 0, 1, 0.6f, "Caustics", "", 0 )
X( P_CAUSTIC_RIPPLE, "Caustic Ripple", STD, 0, 1, 0.3f, "Caustics", "", 0 )   // a moving ripple on the glass
X( P_CAUSTIC_FLOOR, "Caustic Floor", STD, 0, 2, 1, "Caustics", "", 0 )        // on the catcher plane (Floor group)
X( P_CAUSTIC_SMOKE, "Caustic Smoke", STD, 0, 2, 0.5f, "Caustics", "", 0 )
X( P_CAUSTIC_SURFACE, "Caustic Surface", STD, 0, 2, 1, "Caustics", "", 0 )    // on the mask behind the glass
MEHR_COLOR( P_CAUSTIC_COL, "Caustic", "Caustics", 0.12f, 0.15f, 1, 1, "Caustic Alpha" ) // alpha x Caustics
#undef MEHR_REGION
#define MEHR_REGION 0
#define MEHR_VLIGHT( L, S, px, py, pz, anchor, aim, h, s, v )                                          \
	X( P_##L##_ENABLE, S " Enable", BOOL, 0, 1, 0, "Light " S, "", 0 )                             \
	X( P_##L##_TYPE, S " Type", OPT, 0, 1, 0, "Light " S, "Spot|Point", 0 )                        \
	X( P_##L##_ANCHOR, S " Anchor To Mask", BOOL, 0, 1, anchor, "Light " S, "", 0 )                \
	X( P_##L##_POS_X, S " Pos X", STD, -6, 6, px, "Light " S, "", 0 )                              \
	X( P_##L##_POS_Y, S " Pos Y", STD, -6, 6, py, "Light " S, "", 0 )                              \
	X( P_##L##_POS_Z, S " Pos Z", STD, -6, 6, pz, "Light " S, "", 0 )                              \
	X( P_##L##_AIM_MODE, S " Aim Mode", OPT, 0, 2, aim, "Light " S, "At Camera|At Mask|Free", 0 ) \
	X( P_##L##_AIM_YAW, S " Aim Yaw", STD, -180, 180, 0, "Light " S, "", 0 )                       \
	X( P_##L##_AIM_PITCH, S " Aim Pitch", STD, -90, 90, 0, "Light " S, "", 0 )                     \
	X( P_##L##_CONE, S " Cone Angle", STD, 1, 170, 45, "Light " S, "", 0 )                         \
	X( P_##L##_CONE_SOFT, S " Cone Soft", STD, 0, 1, 0.5f, "Light " S, "", 0 )                     \
	X( P_##L##_INTENSITY, S " Intensity", STD, 0, 100, 12, "Light " S, "", 0 )                     \
	MEHR_COLOR( P_##L##_COL, S, "Light " S, h, s, v, 1, S " Alpha" ) /* alpha x intensity */          \
	X( P_##L##_HUE_DRIFT, S " Hue Drift", STD, -0.7f, 0.7f, 0, "Light " S, "", 0 )                 \
	X( P_##L##_BEAT, S " Beat Pulse", STD, 0, 4, 0, "Light " S, "", 0 )                            \
	X( P_##L##_AUDIO, S " Audio > Light", STD, 0, 4, 0, "Light " S, "", 0 )
MEHR_VLIGHT( LA, "A", 0, 0, -1.5f, 1, 0, 0.1052632f, 0.38f, 1 )
MEHR_VLIGHT( LB, "B", 2.5f, 0.6f, 1, 0, 1, 0.5925926f, 0.45f, 1 )
#undef MEHR_VLIGHT
// LIGHT C: "eye lights", a pair of spots (left = -X in mask space, right = +X), same light model as A.
// Each Pos is the EYE OPENING; the light sits Behind Depth behind it along the aim. Place At Eyes finds the
// eye holes (EyeFinder) and writes Pos / aim / cone; C Placed For (hidden) records the model it was placed
// for, so the auto-place on Enable runs once per model and never overwrites a saved placement.
X( P_LC_ENABLE, "C Enable", BOOL, 0, 1, 0, "Light C", "", 0 )
X( P_LC_PLACE, "C Place At Eyes", EVENT, 0, 1, 0, "Light C", "", 0 )
X( P_LC_FIT_CONE, "C Fit Cone", EVENT, 0, 1, 0, "Light C", "", 0 )
X( P_LC_FRONT_ONLY, "C Front Only", BOOL, 0, 1, 1, "Light C", "", 0 )
X( P_LC_BEHIND, "C Behind Depth", STD, 0.02f, 2, 0.3f, "Light C", "", 0 )
X( P_LC_ANCHOR, "C Anchor To Mask", BOOL, 0, 1, 1, "Light C", "", 0 )
X( P_LC_MIRROR, "C Mirror X", BOOL, 0, 1, 1, "Light C", "", 0 )
X( P_LC_L_POS_X, "C Left Pos X", STD, -6, 6, -0.3f, "Light C", "", 0 )
X( P_LC_L_POS_Y, "C Left Pos Y", STD, -6, 6, 0.2f, "Light C", "", 0 )
X( P_LC_L_POS_Z, "C Left Pos Z", STD, -6, 6, 0.2f, "Light C", "", 0 )
X( P_LC_R_POS_X, "C Right Pos X", STD, -6, 6, 0.3f, "Light C", "", 0 )
X( P_LC_R_POS_Y, "C Right Pos Y", STD, -6, 6, 0.2f, "Light C", "", 0 )
X( P_LC_R_POS_Z, "C Right Pos Z", STD, -6, 6, 0.2f, "Light C", "", 0 )
X( P_LC_AIM_MODE, "C Aim Mode", OPT, 0, 2, 0, "Light C", "Through Hole|At Camera|Free", 0 )
X( P_LC_AIM_YAW, "C Aim Yaw", STD, -180, 180, 0, "Light C", "", 0 )
X( P_LC_AIM_PITCH, "C Aim Pitch", STD, -90, 90, 0, "Light C", "", 0 )
X( P_LC_CONE, "C Cone Angle", STD, 1, 170, 20, "Light C", "", 0 )
X( P_LC_CONE_SOFT, "C Cone Soft", STD, 0, 1, 0.5f, "Light C", "", 0 )
X( P_LC_INTENSITY, "C Intensity", STD, 0, 100, 6, "Light C", "", 0 )
MEHR_COLOR( P_LC_LCOL, "C Left", "Light C", 0.1052632f, 0.38f, 1, 1, "C Left Alpha" ) // alpha x intensity
X( P_LC_LINK, "C Link Colors", BOOL, 0, 1, 1, "Light C", "", 0 )
MEHR_COLOR( P_LC_RCOL, "C Right", "Light C", 0.1052632f, 0.38f, 1, 1, "C Right Alpha" )
X( P_LC_HUE_DRIFT, "C Hue Drift", STD, -0.7f, 0.7f, 0, "Light C", "", 0 )
X( P_LC_BEAT, "C Beat Pulse", STD, 0, 4, 0, "Light C", "", 0 )
X( P_LC_AUDIO, "C Audio > Light", STD, 0, 4, 0, "Light C", "", 0 )
X( P_LC_PLACED, "C Placed For", TEXT, 0, 0, 0, "Light C", "", HIDDEN )
#undef MEHR_REGION
#define MEHR_REGION MEHR_CLASSIC
// SPOTS: club-style moving spotlights (LoadEhr W2: moved to LoadEhr / FX, making room in Wow FX) across the mask (their beams show in the Volume medium)
X( P_SPOT_COUNT, "Spot Count", OPT, 0, 4, 0, "Spots", "Off|1|2|3|4", 0 )
X( P_SPOT_INTENSITY, "Spot Intensity", STD, 0, 100, 25, "Spots", "", 0 )
X( P_SPOT_CONE, "Spot Cone", STD, 2, 90, 14, "Spots", "", 0 )
X( P_SPOT_SPEED, "Spot Speed", STD, 0, 15, 3.75f, "Spots", "", 0 ) // sweeps per minute (one sweep = there and back)
X( P_SPOT_PATTERN, "Spot Pattern", OPT, 0, 4, 0, "Spots", "Pan|Circle|Figure 8|Cross|Random", 0 )
X( P_SPOT_WIDTH, "Spot Width", STD, 0, 4, 1.2f, "Spots", "", 0 )
X( P_SPOT_HEIGHT, "Spot Rig Height", STD, -4, 6, 2.5f, "Spots", "", 0 )
X( P_SPOT_DIST, "Spot Rig Dist", STD, 0.5f, 10, 3, "Spots", "", 0 )
MEHR_COLOR( P_SPOT_COL, "Spot", "Spots", 0, 0, 1, 1, "Spot Alpha" ) // alpha x Spot Intensity
X( P_SPOT_COLOR_MODE, "Spot Color Mode", OPT, 0, 2, 0, "Spots", "Single|Rainbow|Pairs", 0 )
X( P_SPOT_STROBE, "Spot Beat Strobe", STD, 0, 1, 0, "Spots", "", 0 )
X( P_SPOT_BEAMS, "Spot Beams", STD, 0, 4, 1, "Spots", "", 0 )
#undef MEHR_REGION
#define MEHR_REGION MEHR_WOW
// PROJECTOR (W26 Stage 2): a spot whose light is a picture (FX: the incoming clip or a file; source: a file). It lights the medium
// (the picture shows as a beam in the haze / smoke) and lands on the mask, shadowed by it. Placed like Light A.
// W2: Proj File = an image (png / jpg) or a VIDEO (mov / mp4 / m4v: H.264, HEVC, ProRes incl. 4444 alpha, HAP / HAP Alpha / HAP Q)
X( P_PROJ_ENABLE, "Proj Enable", BOOL, 0, 1, 0, "Projector", "", 0 )
#if LOADEHR_FX
X( P_PROJ_SOURCE, "Proj Source", OPT, 0, 1, 0, "Projector", "Input|File", 0 )
#endif
X( P_PROJ_FILE, "Proj File", FILE, 0, 0, 0, "Projector", "", 0 )
X( P_VIDEO_SPEED, "Video Speed", STD, -4, 4, 1, "Projector", "", 0 )       // x real time (W4: < 0 plays backwards)
// W4: Scrub = no clock, the frame shown is Video Position (drag it, map a fader, or give it an Arena timeline animation = a transport)
X( P_VIDEO_LOOP, "Video Loop", OPT, 0, 3, 0, "Projector", "Loop|Ping-Pong|Once|Scrub", 0 )
// W4: the clip position 0..1, frame-quantised. Scrub: the frame shown. Loop / Ping-Pong / Once: a user change CUES (the clock jumps
// there and plays on); while playing the plugin pushes its own position here ~15 / s (the playhead; a pushed value is no cue)
X( P_VIDEO_POS, "Video Position", STD, 0, 1, 0, "Projector", "", 0 )
X( P_VIDEO_TIME, "Video Time", TEXT, 0, 0, 0, "Projector", "", 0 ) // read-only: "mm:ss.f / mm:ss.f  frame n / N  codec"
X( P_VIDEO_RESTART, "Video Restart", EVENT, 0, 1, 0, "Projector", "", 0 )  // back to the first frame (a clip trigger does it too)
// W3: how a video's alpha is read (the file does not say; Arena asks per clip, Premultiplied by default). Straight = colour x alpha in
// the copy pass (W2's behaviour for every codec); Premultiplied = the colour as it is. Every codec with alpha.
X( P_VIDEO_ALPHA, "Video Alpha", OPT, 0, 1, 0, "Projector", "Premultiplied|Straight", 0 )
X( P_PROJ_ANCHOR, "Proj Anchor", BOOL, 0, 1, 0, "Projector", "", 0 )
X( P_PROJ_POS_X, "Proj Pos X", STD, -6, 6, 0.8f, "Projector", "", 0 )
X( P_PROJ_POS_Y, "Proj Pos Y", STD, -6, 6, 1.2f, "Projector", "", 0 )
X( P_PROJ_POS_Z, "Proj Pos Z", STD, -6, 6, 4, "Projector", "", 0 )
X( P_PROJ_AIM_MODE, "Proj Aim Mode", OPT, 0, 1, 0, "Projector", "At Mask|Free", 0 )
X( P_PROJ_AIM_YAW, "Proj Aim Yaw", STD, -180, 180, 0, "Projector", "", 0 )
X( P_PROJ_AIM_PITCH, "Proj Aim Pitch", STD, -90, 90, 0, "Projector", "", 0 )
X( P_PROJ_THROW, "Proj Throw", STD, 5, 120, 30, "Projector", "", 0 )      // degrees across the picture
X( P_PROJ_FOCUS, "Proj Focus", STD, 0, 1, 0, "Projector", "", 0 )         // 0 sharp, up: out of focus
X( P_PROJ_INTENSITY, "Proj Intensity", STD, 0, 100, 25, "Projector", "", 0 )
MEHR_COLOR( P_PROJ_COL, "Proj", "Projector", 0, 0, 1, 1, "Proj Alpha" )  // alpha x intensity (white = the picture's own colours)
X( P_PROJ_BEAM, "Proj Beam", STD, 0, 4, 1, "Projector", "", 0 )           // the picture in the haze / smoke
X( P_PROJ_SURFACE, "Proj Surface", STD, 0, 4, 1, "Projector", "", 0 )     // the picture on the mask
#undef MEHR_REGION
#define MEHR_REGION 0
// POST: bloom + lens flares
X( P_BLOOM, "Bloom", STD, 0, 10, 0, "Post", "", 0 )
X( P_BLOOM_THRESHOLD, "Bloom Threshold", STD, 0, 1, 0.65f, "Post", "", 0 )
X( P_BLOOM_RADIUS, "Bloom Radius", STD, 0, 1, 0.6f, "Post", "", 0 )
X( P_FLARE_GHOSTS, "Flare Ghosts", STD, 0, 10, 0, "Post", "", 0 )
X( P_FLARE_STREAK, "Flare Streak", STD, 0, 10, 0, "Post", "", 0 )
// FLOOR: shadow catcher (alpha-only) + mirror floor, on invisible planes
X( P_SHADOW_CATCHER, "Shadow Catcher", STD, 0, 1, 0, "Floor", "", 0 )
X( P_CATCHER_PLANE, "Catcher Plane", OPT, 0, 1, 0, "Floor", "Floor|Wall", 0 )
X( P_FLOOR_HEIGHT, "Floor Height", STD, -4, 2, -1.02f, "Floor", "", 0 ) // just under an auto-fitted mask (bottom at -1)
X( P_WALL_DIST, "Wall Distance", STD, 0, 8, 1.5f, "Floor", "", 0 )
X( P_SHADOW_SOFT, "Shadow Soft", STD, 0, 1, 0.4f, "Floor", "", 0 )
X( P_MIRROR, "Mirror Floor", STD, 0, 1, 0, "Floor", "", 0 )
X( P_MIRROR_BLUR, "Mirror Blur", STD, 0, 1, 0.2f, "Floor", "", 0 )
X( P_MIRROR_FADE, "Mirror Fade", STD, 0, 1, 0.2f, "Floor", "", 0 )
X( P_FLOOR_LIGHT, "Floor Light", STD, 0, 1, 0.5f, "Floor", "", 0 )   // key-lit pool on the catcher, so the shadow reads over black
X( P_FLOOR_CAMERA, "Floor Camera", STD, 0, 1, 1, "Floor", "", 0 )  // tilts the camera down onto the floor while the floor is on
#undef MEHR_REGION
#define MEHR_REGION MEHR_CLASSIC // W5: moved Wow -> classic (room for Wireframe in Wow FX)
// SILHOUETTE: black fill with a glowing outline; Sil Fill reveals the lit mask
X( P_SILHOUETTE, "Silhouette", STD, 0, 1, 0, "Silhouette", "", 0 )
X( P_SIL_FILL, "Sil Fill", STD, 0, 1, 0, "Silhouette", "", 0 )
X( P_SIL_GLOW, "Sil Glow", STD, 0, 10, 2, "Silhouette", "", 0 )
X( P_SIL_WIDTH, "Sil Width", STD, 0.5f, 60, 8, "Silhouette", "", 0 )
MEHR_COLOR( P_SIL_COL, "Sil", "Silhouette", 0.5238095f, 0.7f, 1, 1, "Sil Alpha" ) // alpha x Sil Glow
#undef MEHR_REGION
#define MEHR_REGION MEHR_CLASSIC
// FX
X( P_EXPLODE, "Explode", STD, 0, 3, 0, "FX", "", 0 )
X( P_EXPLODE_RANDOM, "Explode Random", STD, 0, 1, 0.5f, "FX", "", 0 )
X( P_DISSOLVE, "Dissolve", STD, 0, 1, 0, "FX", "", 0 )
X( P_DISSOLVE_EDGE, "Dissolve Glow", STD, 0, 10, 1, "FX", "", 0 )
X( P_TOON_STEPS, "Toon Steps", STD, 0, 8, 0, "FX", "", 0 )
X( P_OUTLINE, "Outline", STD, 0, 1, 0, "FX", "", 0 )
X( P_DEPTH_FOG, "Depth Fog", STD, 0, 3, 0, "FX", "", 0 )
#undef MEHR_REGION
#define MEHR_REGION 0
// OUTPUT
MEHR_COLOR( P_BG, "Background", "Output", 0, 0, 0, 0, "Background Alpha" ) // alpha = background opacity
X( P_MSAA, "MSAA", OPT, 0, 2, 1, "Output", "1x|4x|8x", 0 )
#if LOADEHR_FX
// INPUT (FX only)
X( P_INPUT_USE, "Input Use", OPT, 0, 3, 0, "Input", "Off|Background|Skin on flagged parts|Skin all", 0 )
X( P_INPUT_PROJECTION, "Input Projection", OPT, 0, 2, 0, "Input", "Front planar|Model UV|Spherical", 0 )
X( P_INPUT_MIX, "Input Mix", STD, 0, 1, 1, "Input", "", 0 )
#undef MEHR_REGION
#define MEHR_REGION MEHR_WOW
// FACE (FX only, Stage 7): Apple Vision on the incoming layer places the mask on up to 4 faces. LoadEhr W1: carried by Wow FX
// (the four model slots pushed LoadEhr FX to 521 params; Face's 10 brought it to 511, Wow FX to 507)
X( P_FACE_TRACK, "Face Track", BOOL, 0, 1, 0, "Face", "", 0 )
X( P_MAX_FACES, "Max Faces", STD, 1, 4, 4, "Face", "", 0 )
X( P_FACE_SCALE, "Face Scale", STD, 0.1f, 5, 1.2f, "Face", "", 0 )
X( P_FACE_OFFSET_X, "Face Offset X", STD, -1, 1, 0, "Face", "", 0 )
X( P_FACE_OFFSET_Y, "Face Offset Y", STD, -1, 1, 0.3f, "Face", "", 0 )
X( P_FOLLOW_ROLL, "Follow Roll", STD, 0, 1, 1, "Face", "", 0 )
X( P_FOLLOW_YAW, "Follow Yaw", STD, 0, 1, 1, "Face", "", 0 )
X( P_FOLLOW_PITCH, "Follow Pitch", STD, 0, 1, 0.6f, "Face", "", 0 )
X( P_FACE_SMOOTH, "Face Smooth", STD, 0, 1, 0.5f, "Face", "", 0 )
X( P_HIDE_NO_FACE, "Hide No Face", BOOL, 0, 1, 1, "Face", "", 0 )
#endif
#undef MEHR_REGION
#define MEHR_REGION 0
// AUDIO
X( P_FFT, "Audio FFT", BUFFER, 0, 0, 0, "Audio", "", 0 )
// DEBUG
#undef MEHR_REGION
#define MEHR_REGION MEHR_CLASSIC
X( P_DEBUG_VIEW, "Debug View", OPT, 0, 6, 0, "Debug", "Final|Albedo|Normal|Roughness|Metallic|UV|Part IDs", 0 ) // W28: classic only (512 cap)
#undef MEHR_REGION
#define MEHR_REGION 0
X( P_DEBUG_LOG, "Debug Log", BOOL, 0, 1, 0, "Debug", "", 0 ) // every param change + per-drag rebuilds into LoadEhr.log

#undef MEHR_COLOR
#undef MEHR_REGION
#define MEHR_REGION 0
