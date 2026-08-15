# Asset prompts — copy/paste

Extracted from `Player-Weapon-Gear-Asset-Brief.md`. The full brief has
the reasoning, the citations and the failure-mode section; this file is
just the prompts, in order, so they can be pasted one after another.

**Each prompt is self-contained** — every one carries its own scale, up/forward
axis, origin, material rules and triangle budget, so they can be pasted one at
a time in any order with nothing prepended.

Order matters only for consistency: **do the RIFLE first.** It is the reference
the other seven weapons are described against ("wider than the rifle", "shorter
than the rifle"), so generating it first gives you something to judge the rest
against.

Priority if you only do a few: **1 (Rifle), 9 (Arms), 4 (Shotgun), 8 (Sidearm),
then 16 (icons)**. Rifle is the default equipped weapon, the arms are behind all
eight guns and are 100% of screen time, and rifle-vs-shotgun is the first real
variation test. Gear is deliberately last — nothing in the game can render it yet.


## 1. Rifle — the reference. Model this one first.

```
Basic first-person videogame weapon model: a standard-issue automatic assault
rifle, built for Unreal Engine import as static meshes (no rig, no bones, no
skeleton). Scale: modeled in real-world centimeters, approximately 80 cm
overall length, Z up, barrel pointing along +X. Origin of the model at the
TRIGGER GROUP / firing hand — the stock extends behind the origin into negative
X, the barrel extends in front into positive X.

Silhouette: long, straight, balanced, nothing exaggerated. Three shapes and no
more: one long horizontal receiver mass (about 34 cm long, 5 cm wide, 7 cm
tall), ONE break in the top line — a low boxy top-mounted optic sitting about
8 cm above the trigger group — and ONE break in the bottom line, a straight
30-round box magazine raked slightly forward. A fixed straight stock behind the
grip. A plain cylindrical barrel about 26 cm long running forward from the
receiver, ending in a slightly wider muzzle device.

Character: THE ONE GUN THAT LOOKS ISSUED. Uniform matte factory finish, zero
personalization, no tape, no paint, no hand-marking, no wear beyond even use.
It is the weapon every other gun in this armory will be compared against, so
it must be the most boring and the most correct.

Materials: one to three flat matte materials only, no textures, no decals.
Very dark desaturated gunmetal for the receiver and barrel, a slightly
different dark olive-toned polymer for the grip, stock and magazine. Nothing
saturated. Nothing teal or cyan anywhere — that color is reserved.

Deliver as FIVE SEPARATE MESHES in one file, not merged: body/receiver, barrel
with muzzle device, optic/sight, magazine, and the charging handle mechanism.
Low poly, blockout-to-first-pass quality, about 1,500-3,000 triangles total.
No environment, no hands, no arms, no base, single object, neutral orientation.
```

## 2. SMG — 900 RPM, 35 rounds, and no stock

```
Basic first-person videogame weapon model: a cheap, mass-produced submachine
gun, built for Unreal Engine import as static meshes (no rig, no bones).
Scale: real-world centimeters, approximately 40 cm overall — the SHORTEST long
gun in the set, half the length of a rifle. Z up, barrel along +X, origin at
the trigger group.

Silhouette: short, stubby, front-heavy. The single defining feature is that it
has NO STOCK AT ALL — nothing extends behind the grip. The second defining
feature is that the magazine is the LONGEST thing on the weapon: a 35-round
stick, raked slightly forward, that looks disproportionately large for a gun
this small. That contradiction is the whole read — this thing fires 900 rounds
per minute and it is carrying the ammunition to do it in a body that is barely
there.

Character: cheap and replaceable. Stamped sheet metal and visibly injection-
molded polymer, mold seams and sprue marks acceptable, a slightly LIGHTER and
more washed-out plastic than any other weapon in the set. This is the gun you
do not care about losing.

Materials: two flat matte materials, no textures. Pale-ish worn polymer for the
body, shell and magazine; very dark gunmetal for the short barrel and bolt.
Nothing saturated, nothing teal or cyan.

Deliver as FIVE SEPARATE MESHES: body, barrel, sight, magazine, bolt. Low poly,
about 1,200-2,000 triangles. No environment, no hands, no base.
```

## 3. Sniper — 8 rounds, 150 RPM, and the most personal object in the game

```
Basic first-person videogame weapon model: a long-range bolt-action sniper
rifle, built for Unreal Engine import as static meshes (no rig, no bones).
Scale: real-world centimeters, approximately 96 cm overall — the LONGEST
weapon in the set by about 20% over a standard rifle. Z up, barrel along +X,
origin at the trigger group.

Silhouette: extremely long and notably THIN — long WITHOUT being bulky. A
narrow receiver (about 30 cm long but only 4.5 cm wide) and a very long thin
barrel (about 44 cm) that is thinner than a rifle's despite the gun being
longer. The single dominant feature, which no other weapon in this set is
allowed to have: an OVERSIZED CYLINDRICAL OPTIC that visibly breaks the top
line, roughly 26 cm long and 7 cm in diameter, sitting about 12 cm above the
trigger group — a scope that looks too big for the gun it is on.

Character: PERSONALLY MAINTAINED, the most individual object in the armory.
Add a small flat off-white taped card on the top of the stock at the cheek
rest — a hand-marked range card, roughly 7 x 5 cm, plainly stuck on by the
owner. Bedding compound at the action, tape at the barrel band, a stock that
has been shaved to fit somebody's face. Nothing factory about it.

Materials: three flat matte materials, no textures. Dark gunmetal receiver and
optic, darker still barrel, dark olive polymer stock and grip, and ONE small
off-white patch for the taped card — that patch is the only light value on the
whole weapon and it must read at a glance.

Deliver as FIVE SEPARATE MESHES: body, barrel, scope, magazine, bolt handle.
Low poly, about 1,500-2,500 triangles. No environment, no hands, no base.
```

## 4. Shotgun — 8 pellets per trigger pull, and a mechanism you can watch

```
Basic first-person videogame weapon model: a pump-action combat shotgun, built
for Unreal Engine import as static meshes (no rig, no bones). Scale: real-world
centimeters, approximately 76 cm overall. Z up, barrel along +X, origin at the
trigger group.

Silhouette: the WIDEST and THICKEST weapon in the set — blunt, heavy, closer to
a tool than to a firearm. A thick receiver about 26 cm long, 8 cm wide, 9 cm
tall. A fat barrel about 5 cm in diameter.

TWO dominant features, and no other weapon may borrow either:
1. A TUBE MAGAZINE slung UNDERNEATH the barrel, running most of the barrel's
   length, clearly a separate parallel cylinder — the read that says "this
   holds shells, not a box magazine."
2. A PUMP on that tube, and it must be the BRIGHTEST VALUE on the whole
   weapon: a bare bright-steel sleeve, visibly a moving part, positioned where
   the support hand goes. The player watches this thing move when they reload,
   so it has to be obviously a mechanism and obviously separate from the body.

Character: industrial, blunt, unglamorous, honestly worn. Not tactical, not
sleek. Something taken out of a workshop.

Materials: three flat matte materials, no textures. Dark gunmetal receiver and
barrel, dark olive polymer stock and grip, and BRIGHT BARE STEEL for the pump
and the front bead sight — that value contrast is doing all the work. Nothing
saturated, nothing teal or cyan.

Deliver as FIVE SEPARATE MESHES: body, barrel, sight, tube magazine, pump. Low
poly, about 1,500-2,500 triangles. No environment, no hands, no base.
```

## 5. Rocket — the one that breaks the shoulder line, and the only hazard amber in the game

```
Basic first-person videogame weapon model: a shoulder-fired, field-fabricated
rocket launcher, built for Unreal Engine import as static meshes (no rig, no
bones). Scale: real-world centimeters, approximately 83 cm overall, and it is
the LARGEST SINGLE PART of any weapon in the set — one tube about 66 cm long
and 11 cm in diameter. Z up, tube along +X, origin at the firing grip.

Silhouette: a TUBE, mounted HIGH — about 6 cm above the grip line, high enough
that it visibly breaks the shoulder line rather than sitting in front of the
chest. It is the only weapon in this set that does that. Deliberately
ASYMMETRIC: a small simple box sight offset to the LEFT SIDE of the tube, not
on the centre line. A forward flare (a short cone widening toward the muzzle)
and a REAR BACK-BLAST CONE widening backward behind the grip. Two grips: a
firing grip at the origin and a forward grip about 27 cm out.

Character: FIELD-FABRICATED. Welded rather than machined, uneven seams,
hand-cut plate, and hand-painted markings. It should look like it might not be
entirely safe to fire. This is the only weapon in the game permitted a bright
color, and it gets exactly one: a HAZARD AMBER / warning-orange band wrapping
the tube roughly a third of the way along, plainly hand-painted rather than
printed.

Materials: three flat matte materials, no textures. Very dark gunmetal for the
tube and cones, dark olive polymer for both grips, and ONE hazard amber band.
No other saturated color. Nothing teal or cyan anywhere.

Deliver as FIVE SEPARATE MESHES: body/tube, forward muzzle flare, offset sight,
grip, rear vent cone. Low poly, about 1,200-2,000 triangles. No environment, no
hands, no base.
```

## 6. Burst Rifle — three rounds, a gap you cannot shorten, and a ladder you memorise

```
Basic first-person videogame weapon model: a precision burst-fire marksman
rifle, built for Unreal Engine import as static meshes (no rig, no bones).
Scale: real-world centimeters, approximately 89 cm overall — longer than a
standard rifle but visibly LIGHTER than one. Z up, barrel along +X, origin at
the trigger group.

Silhouette: rifle-shaped but built for discipline rather than volume. Three
differences from a standard assault rifle, and they must all be visible at a
glance:
1. A LONGER, THINNER barrel — about 34 cm and noticeably narrower than a
   standard rifle's, so the weapon reads as reaching further.
2. The dominant feature: a TALL DEDICATED OPTIC sitting on a VISIBLE RISER
   BLOCK, about 12.5 cm above the trigger group — clearly a separate mount
   lifting the scope well clear of the receiver, unlike a rifle's flat boxy
   optic sitting low. The riser is as much of the read as the scope is.
3. A STRAIGHT IN-LINE STOCK running level with the barrel line, no drop, no
   comb — the stock of something you shoot from a supported position.

Character: this weapon fires a fixed three-round burst and then WAITS, and it
cannot be hurried. Whoever carries it pre-aims. It should look considered and
slightly precious next to the standard rifle — better made, more deliberate,
less issued. No personalization or hand-marking (that belongs to the sniper);
this is quality from the factory, not care from the owner.

Materials: two to three flat matte materials, no textures. Dark gunmetal
receiver, riser and optic; darker thin barrel; dark olive polymer for the grip
and in-line stock. Nothing saturated, nothing teal or cyan.

Deliver as FIVE SEPARATE MESHES: body, barrel, optic (with its riser as part of
the optic mesh), magazine, charging mechanism. Low poly, about 1,500-2,500
triangles. No environment, no hands, no base.
```

## 7. Machinegun — 120 rounds, a 4.2 second reload, and "only while planted"

```
Basic first-person videogame weapon model: a belt-or-drum-fed light machine
gun, built for Unreal Engine import as static meshes (no rig, no bones).
Scale: real-world centimeters, approximately 83 cm overall, but it must read as
the HEAVIEST OBJECT IN THE GAME — more than twice the visual mass of a standard
rifle at similar length. Z up, barrel along +X, origin at the trigger group.

Silhouette: thick everywhere. A heavy receiver about 34 cm long, 8 cm wide,
10 cm tall. A SHROUDED barrel — a fat perforated or ribbed sleeve about 7 cm in
diameter, not a bare tube.

TWO dominant features, and they are the whole archetype:
1. A DRUM MAGAZINE, and it must be THE BIGGEST SINGLE PART ON ANY WEAPON IN
   THE SET — a cylinder roughly 17 cm across lying ON ITS SIDE (its axis
   pointing left-right, across the weapon, not along it), slung under the
   receiver. It holds 120 rounds and it has to look like it. Nobody should have
   to be told what it is.
2. A BIPOD, folded down and deployed, two thin legs splaying forward and
   outward from under the barrel about 14 cm long. This weapon's recoil is
   designed to become unmanageable if you keep firing while moving, so the gun
   itself must say "planted" before the player has fired a shot.

Character: heavy, agricultural, unsubtle. Something two people were meant to
carry.

Materials: three flat matte materials, no textures. Dark gunmetal receiver and
drum, darker shrouded barrel, dark olive polymer stock and grip, and BRIGHT
BARE STEEL for the bipod legs only — so the legs read as separate hardware
against the dark mass above them. Nothing saturated, nothing teal or cyan.

Deliver as FIVE SEPARATE MESHES: body, barrel shroud, sight, drum, bipod. Low
poly, about 2,000-3,000 triangles given the drum and bipod. No environment, no
hands, no base.
```

## 8. Sidearm — 0.18 seconds to bring up, and it must read as *nothing*

```
Basic first-person videogame weapon model: a compact semi-automatic pistol,
built for Unreal Engine import as static meshes (no rig, no bones). Scale:
real-world centimeters, approximately 22 cm overall — the SMALLEST weapon in
the set by a factor of three or more against any long gun, and roughly one
fourteenth the visual mass of the machine gun. Z up, barrel along +X, origin at
the trigger group.

Silhouette: almost nothing. A slide about 17 cm long, 3.4 cm wide, 5.4 cm tall,
sitting on a grip. NO STOCK. NO SEPARATE MAGAZINE PROTRUDING — the rounds live
inside the grip, so the bottom line of this weapon is unbroken, which is the
opposite of every other weapon in the set. A very short barrel, barely
protruding. Two tiny sights, front and rear, and they are the only fine detail
on the object.

Character: the whole point of this weapon is that it comes up in under a fifth
of a second. The read the instant it appears must be "there is almost nothing
in my hands." Do not add a rail, a light, a compensator, a threaded barrel, or
any accessory. Every gram you add to this silhouette destroys the archetype.

Materials: two flat matte materials, no textures. Dark gunmetal slide and
barrel, dark olive polymer grip, and BRIGHT BARE STEEL on the two tiny sights
only — small bright points on an otherwise dark object. Nothing saturated,
nothing teal or cyan.

Deliver as FIVE SEPARATE MESHES: body/frame, barrel, sights, magazine
(internal, still its own mesh for the reload), slide. Low poly, about 800-1,500
triangles. No environment, no hands, no base.
```

## 9. 5.2 The prompt

```
Basic first-person videogame ARMS model: a matched pair of forearms and gloved
hands for a first-person shooter viewmodel, seen from the wearer's own eyes.
Built for Unreal Engine. Scale: modeled in real-world centimeters — a forearm
is about 6 cm across at the wrist and a gloved hand is about a 9.5 cm cube.
Z up, arms extending along +X away from the viewer. The arms are cut off at the
ELBOW or above, open-ended, NOT capped or finished at the shoulder end — that
end is clipped off screen and must never be seen, so do not model a shoulder,
a torso, a neck, or a head. Nothing above the elbow exists.

Pose: bind pose / T-pose forearms with the hands relaxed but shaped to grip —
one hand shaped to wrap a pistol grip, one shaped to wrap a horizontal
handguard. Do not model them holding a specific weapon; the weapon is a
separate object placed between them.

Character: MILITIA, NOT MILITARY. Equipped by an organization that has money
for the things that keep people alive and nothing left over. The read is
competent, funded unevenly, personally maintained. Layer grammar from the skin
outward: a plain dark bodysuit sleeve, soft armor at the forearm, one hard
plate strapped over the outside of the forearm, and a webbing strap or two.
Every hard edge is scuffed and every recess is clean — wear happens where a
body touches things. Add exactly ONE personal item: a wrist wrap, a taped ring,
or a small charm, plainly added by the wearer rather than issued. It is the
cheapest humanizing detail available and it is the only decorative element
permitted.

CRITICAL NEGATIVES: no hook, no grapple mount, no hardpoint, no tether, no
launcher, no wrist-mounted device of any kind — the game has no such verb and
this model must not promise one. No exposed skin above the glove. No unit
insignia large enough to read.

Composition: THE FOREARM IS THE LARGEST SHAPE ON SCREEN AND MUST BE THE
QUIETEST. Value it DARKER than the weapon it holds and give it less surface
detail than the weapon, because the weapon carries the information and the arm
must not compete with it.

Materials: two flat matte materials, no textures needed. Very dark olive for
the gloves, a slightly lighter dark slate for the sleeve and forearm armor.
Nothing saturated. NOTHING TEAL OR CYAN anywhere — that color is reserved for
rift objects and the player may never carry it.

Deliver GLOVES AND FOREARMS AS SEPARATE MESHES (gloves are a real equipment
slot and will be swapped independently), plus the personal item as its own
small mesh. Rig to a standard UE5-Manny-compatible arm hierarchy if rigging at
all, and ALSO export the bind pose as a static mesh — the game currently
consumes static meshes only. About 4,000-6,000 triangles for the pair. No
environment, no weapon, no base, black background.
```

## 10. 6.3 Helmet ×2

```
Basic videogame armor model: TWO variants of a militia combat HELMET for a
first-person shooter's third-person character, built for Unreal Engine. Scale:
real-world centimeters, sized to a 176 cm humanoid — an adult head covering.
Z up, face along +X.

Both variants MUST LEAVE THE WEARER'S FACE VISIBLE. This is a hard constraint:
the game has a social hub where players see each other, and a helmet that hides
the face makes that space dead. Cover the skull, the temples and the jaw line;
leave the eyes, nose and mouth open. No full face plate, no mirrored visor, no
respirator covering the mouth.

VARIANT A — ISSUED: a clean factory-made shell, uniform matte, even edges, one
mounting rail on the left temple with nothing on it, a simple chin strap with
one buckle. Impersonal and correct.

VARIANT B — SALVAGED: the same shell profile achieved from cut and welded plate
— a visible seam where two pieces were joined, a mismatched replacement strap, a
patch of a different material riveted over what was presumably a hole. Same
silhouette, obviously repaired.

Shared grammar both variants must use: one strap width, one buckle type, one
plate-edge profile. Every hard edge scuffed, every recess clean.

Materials: two flat matte materials, no textures. Desaturated olive-slate shell,
darker webbing and straps. Nothing saturated. NOTHING TEAL OR CYAN — that color
is reserved and gear may not carry it.

Deliver the two variants as separate meshes in one file. About 800-1,500
triangles each. No environment, no head, no body, no base.
```

## 11. 6.4 Body Armour ×2

```
Basic videogame armor model: TWO variants of a militia BODY ARMOUR torso shell
for a first-person shooter's third-person character, built for Unreal Engine.
Scale: real-world centimeters, sized to a 176 cm humanoid. Z up, chest along
+X.

BINDING SILHOUETTE RULE: armored at the CHEST and STOMACH, and deliberately
LIGHT AND MOBILE at the SHOULDERS AND HIPS. This character wall-rides, slides
and double-jumps, and cannot look like a heavy infantryman. No pauldrons, no
skirt plates, no bulk at the joints.

Layer grammar, visible where the edges meet: a plain bodysuit underneath, a soft
armor vest over it, ONE hard plate across the chest, then webbing and pouches on
top of that. The eye should be able to count those four layers at the vest's
hem and collar.

VARIANT A — ISSUED: a factory vest, even stitching, matched pouches in a
regular row, one clean rectangular hard plate, uniform color across every
component.

VARIANT B — SALVAGED: the same vest carrying a hard plate that was plainly cut
from something else — different material, different edge finish, held on with
straps rather than seated in a pocket. Pouches of three different origins. One
seam repaired by hand.

Shared grammar: one strap width, one buckle type, one plate-edge profile, one
webbing weave. Scuffed at every hard edge, clean in every recess.

Materials: two to three flat matte materials, no textures. Desaturated
olive-slate vest, darker webbing, a slightly different value for the hard
plate. Nothing saturated. NOTHING TEAL OR CYAN.

Deliver the two variants as separate meshes in one file. About 1,500-2,000
triangles each. No environment, no body, no head, no arms, no base.
```

## 12. 6.5 Gloves ×2

```
Basic videogame armor model: TWO variants of militia GLOVES, built for Unreal
Engine. Scale: real-world centimeters — a gloved hand is roughly a 9.5 cm cube,
sized to a 176 cm humanoid. Z up, fingers along +X.

IMPORTANT: these gloves are also seen in FIRST PERSON, filling the lower half of
the screen for the entire game. They get more scrutiny per triangle than any
other armor piece, and they must be authored SEPARATELY from the forearm so a
glove can be swapped without re-authoring the arm. Include a clean cut at the
wrist and a socket point at the wrist and at the knuckles.

VARIANT A — ISSUED: a clean tactical glove, uniform material, a simple knuckle
guard, one wrist strap with one buckle.

VARIANT B — SALVAGED: a work glove pressed into service — heavier, a mismatched
reinforcement patch stitched across the palm, the knuckle guard obviously a
separate piece added later and lashed on rather than seated.

Shared grammar with the rest of the kit: one strap width, one buckle type, one
plate-edge profile. Scuffed at the knuckles and fingertips, clean in the palm
creases — wear is where a hand touches things.

CRITICAL NEGATIVE: no wrist-mounted device, no hook, no grapple, no launcher,
no hardpoint of any kind. The game has no such verb.

Materials: two flat matte materials, no textures. Very dark olive glove body, a
slightly lighter dark slate for the knuckle guard and strap. Nothing saturated.
NOTHING TEAL OR CYAN.

Deliver the two variants as separate meshes in one file. About 600-1,200
triangles each. No environment, no arms, no body, no base.
```

## 13. 6.6 Waist ×2

```
Basic videogame armor model: TWO variants of a militia WAIST belt-and-pouch rig,
built for Unreal Engine. Scale: real-world centimeters, sized to a 176 cm
humanoid. Z up, front along +X.

This slot is cheap geometry with a high visual return, so it should carry more
readable detail per triangle than any other armor piece — the belt line is
roughly at eye level in a third-person view and it is where a character's
personality lives. It must stay LIGHT AT THE HIPS: no hanging skirt plates, no
thigh rigs that would foul a slide.

VARIANT A — ISSUED: a plain webbing belt with four matched pouches evenly
spaced, one buckle, one closed magazine carrier.

VARIANT B — SALVAGED: the same belt carrying five pouches of four different
origins — one canvas, one hard case, one plainly a repurposed tool pouch, one
lashed on with cord instead of clipped. Nothing matches and it clearly works.

Shared grammar: one strap width, one buckle type, one webbing weave. Scuffed at
every buckle and clasp, clean inside every pouch fold.

Materials: two flat matte materials, no textures. Desaturated olive-slate
webbing, darker pouch bodies. Nothing saturated. NOTHING TEAL OR CYAN.

Deliver the two variants as separate meshes in one file. About 800-1,500
triangles each. No environment, no body, no base.
```

## 14. 6.7 Boots ×2

```
Basic videogame armor model: TWO variants of militia BOOTS with shin protection,
built for Unreal Engine. Scale: real-world centimeters, sized to a 176 cm
humanoid — a boot and the shin above it. Z up, toe along +X.

This character's entire identity is movement — sliding, wall-riding,
double-jumping — and the boots are where that reads. They must look like they
are FOR that: a sole with visible edge grip, a reinforced toe, an ankle that is
plainly articulated rather than encased. HARD PLATE ON THE SHIN, nothing bulky
at the ankle or the hip end.

VARIANT A — ISSUED: a clean laced combat boot with a factory shin plate seated
in a fitted pocket, matched buckles, even sole.

VARIANT B — SALVAGED: the same boot with a shin plate strapped over the top
rather than seated, cut from a different material with a different edge, and a
sole that has plainly been re-glued at the toe.

Shared grammar: one strap width, one buckle type, one plate-edge profile.
Scuffed hard at the toe, the outer edge and the shin plate; clean in the lace
channel and behind the ankle.

Materials: two flat matte materials, no textures. Very dark boot body,
desaturated olive-slate shin plate. Nothing saturated. NOTHING TEAL OR CYAN.

Deliver the two variants as separate meshes in one file (model one boot each;
the pair mirrors). About 800-1,500 triangles each. No environment, no legs, no
body, no base.
```

## 15. 6.8 Necklace ×2

```
Basic videogame accessory model: TWO variants of a small hanging NECK item worn
over a militia armor vest, built for Unreal Engine. Scale: real-world
centimeters, sized to a 176 cm humanoid — the hanging element is roughly 4-6 cm
across on a cord or chain about 45 cm long. Z up, front along +X.

This is the smallest and lowest-priority piece in the kit and it must not
pretend otherwise. It hangs at the sternum, over the vest, and its only job is
to be a legible small shape against a large flat one. Do not make it jewelry —
this is militia, not fantasy.

VARIANT A — ISSUED: a plain flat identification tag on a beaded chain, stamped
and unremarkable, exactly the thing an organization hands out.

VARIANT B — PERSONAL: something the wearer chose — a small worn object on a
leather cord, plainly not issued, plainly kept. A keepsake, not a trinket.

Shared grammar: the same cord and clasp hardware as the rest of the kit, at the
same scale.

Materials: one to two flat matte materials, no textures. Dull metal for the tag,
dark cord. Nothing saturated. NOTHING TEAL OR CYAN — that color is reserved for
rift objects and Anomalous-rarity items only, and this piece is neither.

Deliver the two variants as separate meshes in one file. About 200-500 triangles
each. No environment, no body, no base.
```

## 16. 7.3 The prompt pattern

```
A single item icon for a dark sci-fi looter-shooter inventory screen, in the
style of a technical equipment catalogue rather than an illustration.

SUBJECT: <<< one line, from the list below >>>

Composition: the object is shown in ORTHOGRAPHIC THREE-QUARTER VIEW from
slightly above and slightly to the left — the same camera for every icon in the
set, never straight-on and never dramatic. The object is centred and fills a
square optical box occupying about 70% of the frame, with clear empty space on
all four sides. It is a product shot, not a scene.

Rendering: flat, matte, near-unlit. Value contrast carries the entire read.
Three tones only — a dark body, one mid tone for the secondary material, and
one bright edge or highlight marking the object's single most identifying
feature. No gradients, no gloss, no rim lighting, no bloom, no lens effects, no
drop shadow, no ground shadow, no reflection.

Palette: desaturated militia hardware only — dark gunmetal, dark olive polymer,
one bare-steel bright. NOTHING SATURATED. Absolutely NO TEAL, CYAN, OR
TURQUOISE anywhere in the image; that color band is reserved and will be
applied separately by the interface.

Background: FULLY TRANSPARENT. No plate, no tile, no frame, no border, no
vignette, no backdrop of any kind — the interface draws its own plate behind
this and a baked background will destroy it.

Output: 256 x 256 pixels, square, 32-bit PNG with alpha. It must remain
identifiable when scaled down to 64 x 64 pixels, so no detail smaller than
about 4 pixels at that size, and no text.
```
