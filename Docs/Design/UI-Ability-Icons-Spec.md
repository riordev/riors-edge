# FIELDPLATE — Ability icon system

Owner-authored design canvas, transcribed from `Ability Icons.dc.html` on
2026-08-13. Reads on top of `UI-Style-Guide-Fieldplate.md`. This is an art
commission brief: the icons themselves do not exist yet.

Silhouette-driven marks stamped on squircle plates. One accent per icon, no
second hue, no fill gradients — an icon is a shape cut out of metal and
backlit by exactly one colour.

## Construction

- **Tile** — 52×52 plate, 4px radius, fill `#18263A`, 1px `#2A3E58` border.
  The glyph occupies a 36×36 optical box centred, 8px clear on all sides so the
  mark survives a 2px accent border in the ready state.
- **Stroke** — 2px at 52px, butt caps, mitre joins. One weight only: no
  tapering, no hairline detail. Anything that needs a third line to read is the
  wrong idea for an icon.
- **Perspective** — every mark is side-on and reads left-to-right, with motion
  rising 8° toward the upper right. Momentum in this game goes forward and up;
  the icon set states that nine times.
- **Colour** — Swift `#4FD8F5`. Caster `#4FD8F5` unless the ability is
  elemental: Void `#B866FF`, Entropy `#FF8A3D`, Rift `#A8FBFF`. Ultimates carry
  violet. Teal is never used on an ability icon.

## Swift — Kinetic / Marksman

- **Skim** (`#4FD8F5`) — a flat velocity line running left along the floor,
  then snapping hard onto a new upward vector with an arrowhead at the break.
  Two short speed ticks trail behind the elbow to say the momentum was already
  there: Skim redirects it, it does not create it. *Min size: the elbow.* The
  angle change must be the loudest feature at 52px; ticks may drop to two at
  32px.
- **Lead** (`#4FD8F5`) — a dashed sightline climbing from lower-left to a
  tagged diamond in the upper right, the dashes lengthening with distance. The
  diamond is drawn in perspective, not flat, and carries two stub ticks on
  opposing corners — a tag clamped onto something, not a reticle floating over
  it. *Min size: the diamond.* Dash count drops to three at 32px; the corner
  ticks go before the dashes do.
- **Overdrive** (`#B866FF`, ultimate) — a meter whose fill has broken past its
  own end cap and continues as two detached blocks off the right edge, with a
  rising chevron lifting out of the bar. The container is complete and the
  contents are not: decay suspended, flow doubled, the housing plainly overrun.
  *Min size: the gap.* The break between cap and detached blocks must stay ≥2px
  at 52; drop the third block before narrowing it.

## Caster — Spellblade / Void

- **Spellblade strike** (`#4FD8F5`) — a narrow blade angled up to the right,
  its cutting edge doubled by a second parallel line: the mana edge sitting a
  hair off the steel. One clean arc crosses the lower half as the swing path,
  cut off before it closes so it reads as a slash rather than a ring. *Min
  size: blade angle + arc.* The doubled edge may merge below 40px; the arc must
  never merge into the blade.
- **Void lash** (`#B866FF`) — a single S-curve whipping from the lower left
  corner up to a two-pronged barb at the far upper right, drawn at full stroke
  the whole way. Two small dots fall off the underside of the curve — the tail
  coming apart as it travels, the only Void tell the set needs. *Min size:
  reach.* The curve must touch two opposite tile corners at 52px; the dots drop
  first, the barb never.
- **Overcast** (`#FF8A3D`, cost) — a muted baseline across the middle with the
  bar's outline continuing below it: the same channel, half above zero and half
  beneath. A small cross sits under the dipped section as the debt mark. The
  bar is one continuous path, because the cost is the same resource, not a
  second one. *Min size: the baseline crossing.* Keep the grey baseline at 1px
  minimum; without it the icon is just a step, not a deficit.

## Generic state overlays

Applied over any ability tile. **Never redraw the glyph for a state.**

- **Ultimate ready** — the tile border thickens to 2px violet and four corner
  brackets appear inside the plate edge, with the glyph switching from outline
  to solid fill. Filled-versus-hollow is the readable difference at a glance;
  the brackets are the confirmation. Brackets may drop below 40px.
- **On cooldown** — the glyph desaturates to `#3E4C5E` and a flat dark wedge
  sweeps clockwise from 12 o'clock, uncovering the plate as it empties.
  Remaining seconds sit centred in mono over the top, one decimal below 3s. The
  wedge is a hard-edged fill with no feather, so the boundary reads as a moving
  edge rather than a shadow. Drop the timer text below 44px; never shrink the
  number below 11px.
- **Unaffordable** — glyph drops to `#3E4C5E`, tile border goes deep red
  (`#C22A2A`), and the hex cost glyph sits struck through in the lower-centre
  on its own opaque plate so it never tangles with the mark behind it. Distinct
  from cooldown by having no sweep — nothing is filling, so waiting will not
  fix it. Below 40px the hex loses its inner slash and becomes a solid red hex
  chip in the same position.

## Handoff notes

Deliver each icon as a single-path-per-element SVG on a 52×52 artboard with no
tile background baked in — the plate, border, and state overlays are drawn by
UI code so that one glyph serves all four states. Export a 32px variant per
icon with the noted simplifications applied by hand rather than by scaling. Do
not add a second accent colour, a drop shadow, or a fill tint to any glyph: the
set's whole legibility argument is one weight, one hue, one silhouette.

## Implementation status (2026-08-13)

The **plate and the four state overlays are implemented** in
`ABreakerPlaytestHUD::DrawAbilitySlot`: `#18263A` face at 4px radius,
`#2A3E58` rest border, 2px accent ready border, window-active corner ticks on a
`#1F3047` face, the clockwise cooldown wedge with the mono timer, and the deep
red border plus struck cost chip when unaffordable. Key hints inherit the state
colour.

**No glyphs exist.** Each square falls back to the ability's short name in the
state colour where the 36×36 optical box would go. Dropping authored SVG/texture
glyphs in later replaces that fallback without touching any state code, which
is what the handoff note asks for.
