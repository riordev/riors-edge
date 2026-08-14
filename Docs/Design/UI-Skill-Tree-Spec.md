# FIELDPLATE — Skill matrix spec

Owner-authored design canvas, transcribed from `Skill Tree v2.dc.html` on
2026-08-13 (`Skill Tree.dc.html` is the superseded first pass). Reads on top of
`UI-Style-Guide-Fieldplate.md`.

## Zones (1920×1080)

- **Header** 1920×88 at `bg/raised`: title `Skill matrix` (h1), meta line
  `BREAKER · SWIFT · LV 42`, the two-tab switch (`Class · Swift` / `Core`),
  both point counters as separate railed chips — class points cyan rail, core
  points gold rail, each showing unspent large and `/ N spent` beneath — and
  respec at the far right in the discard style.
- **Board** 1500×936 fills the rest, with a 60px branch header strip above the
  path field.
- **Detail rail** is a fixed 420px column so node cards never reflow when it
  populates.
- **Footer** 1920×56 carries the input legend (`LMB BUY 1 RANK · HOVER FULL
  DETAIL · SHIFT+LMB BUY TO MAX · TAB SWITCH TREE`) and the live purchasable
  count in gold.
- Screen margin 40px, panel gutters 24px.

## Node states

Every card carries the effect as a number in mono on line two, so the tree is
readable without hovering anything.

| State | Treatment |
| --- | --- |
| Owned | cyan rail, cyan pips, `MAXED` rather than `5/5` alone when full |
| Purchasable | gold 2px border and a gold action line (`1 PT → RANK 4`). Gold is the only border colour that means "spend now", which is what makes scanning work. |
| Available, unowned | neutral 1px `#2A3E58` |
| Locked | dashed border, muted text, and the reason stated literally — tier gate with a progress bar (`TIER GATE 6 / 8`), or the prerequisite by name (`NEEDS WALL CARRY`) |

Keystones and sealed clusters also read dashed.

## Class ↔ Core

One tab pair, not a mode toggle: the board swaps, the header and detail rail
persist.

**Class** is three branches drawn as paths: one 2px trunk per branch running
the full height, cyan where the route is already owned and `#1F3047` where it
is not, with 2px diagonals dropping to each node. Tier gates are dashed
hairlines across the column, labelled once in a dedicated 76px gutter at the
left edge of the field — two short lines, tier then gate cost — so a label can
never land on node copy. Minor and Notable nodes label beneath the marker in a
180px block; Convergence and Keystone label to the right of the marker so the
trunk never runs through their text. Nodes are the markers themselves — 48px
square for multi-rank Minors with the rank inside, 44px diamond for Notables,
64px square for Convergences, 60px diamond for Keystones — and their name,
number, and state sit as plain text beneath, not inside a card.

**Core** is spatial: five clusters positioned around Kinesis as the hub, linked
by convergence lines, each cluster showing its node grid at a glance with a
purchasable count. Elements sits sealed below centre in suppression teal
(`#08B8A8`), the one place teal is legal here because it is a rift object, not
chrome.

Respec is per-tree and its button label states which tree it will clear.

## Branch selection and the build summary (2026-08-13, playtest response)

Two additions to the canvas above, both from the owner's playtest ("you can't
really see the numerical significance of your points… there should be a button
to select your subclass and swap between to the others").

**Branch strip.** The class board draws ONE branch at a time. A chip row above
the board carries every branch of the class — name plus `n / m INVESTED` — and
a `COMPARE ALL` chip that restores the side-by-side view. Focusing one branch
is what buys the board the width it needs: three full-detail columns inside a
1200px panel is what made it feel cramped.

This is **browsing, not commitment**, and the strip says so. See "Subclass
commitment" under Not landed.

**Build summary.** The detail rail is now two zones: a pinned BUILD TOTALS
plate above, and the hover node card below it in its own scroll. Totals are
READ from the character's live aggregation, never recomputed beside it — the
five composed rows (damage, crit chance, crit damage, max health, DoT) come
from the `FBreakerAttributeAggregator` the attribute set owns, and the
tree-layer rows (move/slide/air, dodge, block) come from the same
`FBreakerNodeStats` the movement and combat components consume. The damage row
splits its additive Increased bucket by layer (`TREE +x% · GEAR +y%`), which is
the one-bucket rule made visible.

**Before/after on every node.** A node card leads with what the purchase would
produce — `DAMAGE 1.06x -> 1.10x +4%` — and, for multi-rank nodes, the same
line for buying to max. The per-rank authoring values still print beneath it.
The projection composes through a COPY of the character's real aggregator with
only the progression contribution swapped, so it cannot drift from the live
numbers; `RiorsEdge.UI.SkillProjection.ProjectionMatchesRealPurchase` buys the
node for real and asserts the attribute lands exactly where the arrow pointed.

## Implementation status (2026-08-13, layout re-zoning pass)

Landed in `SBreakerMenu::BuildSkillTreesScreen`:

- Fieldplate palette and the type scale throughout.
- The four node states as the spec's colour language: owned cyan, purchasable
  gold 2px border with a gold action line, available neutral, locked muted
  with the failure reason printed literally. `MAXED` in place of `5/5`.
- **The header zone.** An 88-tall band at `bg/raised` (via
  `SBreakerMenu::BuildZonedFrame`): title `SKILL MATRIX`, the
  `BREAKER · <CLASS> · LV n` meta line, the two-tab `CLASS · <class>` / `CORE`
  switch, both point counters as separate railed chips (class cyan, core gold,
  unspent large with `/ N SPENT` beneath), and respec at the far right in the
  discard style, labelled with the pool it clears.
- **The class board as PATHS.** Drawn on an `SCanvas` at fixed pixel
  positions: a 60px branch header strip above the field, one 2px trunk per
  branch running the full height (cyan where the route is owned, `#1F3047`
  where it is not), 2px diagonals dropping from the trunk to each node,
  tier-gate dashed hairlines across the field labelled once in a dedicated
  76px left gutter as two short lines (tier, then gate cost), and the markers
  themselves — 48px square for multi-rank Minors with the rank inside, 44px
  diamond for Notables, 64px square for Convergences, 60px diamond for
  Keystones. Name, effect number and state sit as plain text near the marker,
  not inside a card; Convergence and Keystone label to the RIGHT so the trunk
  never runs through their text.
- **The Core board as the constellation map.** Kinesis is the hub at centre
  with Precision, Volley, Affliction and Bulwark positioned around it,
  convergence lines drawn between hub and cluster (cyan only when both ends
  are owned), each cluster showing its node grid as compact markers plus a
  purchasable count, and Elements sealed below centre in suppression teal
  `#08B8A8` — the one legal teal here, because a rift is a world object.
- **The fixed 420px hover-detail rail.** Populated through `SetContent` from
  `SButton::OnHovered`, which is event-driven; the column never changes width,
  so the board cannot reflow when it fills. Markers are deliberately never
  disabled, because a disabled `SButton` fires no hover events and a locked
  node most needs to explain itself.
- **Footer**, 56 tall: the input legend and the live purchasable count in gold.
- `SHIFT+LMB` buys to max. The modifier state is read once, inside the click
  handler — never polled from a per-frame attribute.

Not landed / known compromises:

- **Node kind is derived, not authored.** `UBreakerProgressionNode` carries no
  kind field, so `ClassifyNode` infers it: `bCornerstone` → Keystone,
  `MaxRank > 1` → Minor, single-rank costing 3+ → Convergence (the O21
  promotion tier), otherwise Notable. Replace with a direct read when a Kind
  enum lands on the node asset.
- The Elements cluster renders as a sealed placeholder because no Elements
  nodes exist in the fallback content; an `UNMAPPED` cluster catches any node
  authored outside the five known `Core.<Constellation>.` prefixes so nothing
  silently vanishes.
- `TAB` is not bound to the board switch; the legend says click instead.
- Motion is still unimplemented (panel transition, purchase-confirm snap).
- **Subclass commitment does not exist in the data model.**
  `FBreakerProgressionState` has no chosen-branch field and
  `UBreakerClassDefinition::BranchTrees` is a flat list with no selected
  member, so the branch strip is a VIEW and the screen says so rather than
  inventing a persistence rule the owner has not ruled on. A real commitment
  needs: a branch id on `UBreakerProgressionTree` or a selected index on
  `FBreakerProgressionState`; a one-way setter beside
  `ChoosePermanentClassById` carrying the same permanence rule (or an explicit
  Forge-respec rule); save-version handling in `UBreakerSaveGame`; and a ruling
  on whether unselected branches become unpurchasable — a balance decision that
  collides with O15 (branch nodes freely mixed, no mutually exclusive tiers).
- **Only two of Swift's three branches exist as content.** Class-Kits §1.3–1.5
  names them FRENZY, KINETIC and MARKSMAN (not "Survivor");
  `UBreakerProgressionLibrary` authors Kinetic and Marksman only, so the strip
  shows two chips. Frenzy is unauthored content, not a UI gap.
- **Nothing here was visually verified** as of this pass. SUPERSEDED by the
  2026-08-14 looking pass at the end of this file, which photographed all three
  boards and lists what looking found.

## Implementation status (2026-08-14, the first LOOKING pass)

**This screen has now been photographed at 1920x1080 and iterated against the
pictures.** All three of its boards were captured: the Class path board, the
Core constellation map, and COMPARE ALL. The Core map and COMPARE ALL had never
been seen by anyone before this pass — the capture harness only reached the
default board, so `-BreakerCaptureBoard=CORE|COMPARE|BRANCH<n>` was added
(parsed in `SBreakerMenu::ShowScreenForCapture`, dev-only, command-line gated).

What the pictures showed, and what changed:

- **The keystone rendered as a solid black hole.** Bloodrhythm is Swift/Frenzy's
  branch keystone and drew as an unmarked black square — the least distinct
  marker on the board. Two causes, both fixed. (1) `ClassifyNode` keyed only on
  `bCornerstone`, which the fallback content never sets, so Bloodrhythm fell
  through to "single-rank costing 3+" and was classified as a Convergence. It
  now ALSO reads the node's granted tags: a node that hands its owner a
  `Keystone.*` tag IS a keystone, which is the same thing that makes it a
  keystone at runtime (`Keystone.Swift.Bloodrhythm` is a real row in Overdrive's
  variant table). (2) Every kind except a multi-rank Minor put an `SSpacer`
  inside the marker, so Notables, Convergences and Keystones were all empty
  boxes; `MakeMarkerCore` now gives each a filled centre in its state colour,
  and the keystone a concentric ring-gap-core no other marker has. Keystones
  also always carry rail-weight (3px) rings, and locked markers moved from
  `border/rest` to `border/emphasis` — rest is one value step off the plate
  face and was simply invisible at 44px.
- **"GATE" meant two things on one screen.** The tier gutter read `GATE 2` (the
  tier's entry requirement) while every locked marker read `GATE 0/2` (that
  node's progress toward it), stacked vertically. The gutter now reads
  `TIER n` / `OPENS AT g` in gold, or `OPEN` in cyan once met, and prints no
  second line at all when the gate is 0. The gutter widened 76 -> 104 to hold
  the sentence.
- **Locked nodes no longer repeat the tier gate.** A node held ONLY by its
  tier's entry requirement prints no state line — the gutter states that
  requirement once for the whole row, and the muted marker already says "not
  yet". Node-SPECIFIC reasons (a prerequisite by name, points you cannot
  afford) still print, because those are about that node.
  `SkillNodeIsPurchasable` gained an `OutTierGated` flag to tell the two apart.
- **The footer count disagreed with the board.** It read `8 PURCHASABLE` over a
  branch showing three gold nodes, because it counted every class tree while
  the board draws one branch. The visible-tree list is now resolved once and
  used by both the board and the count, and the count states its scope:
  `3 PURCHASABLE ON THIS BRANCH` / `9 PURCHASABLE ON THIS BOARD`.
- **The point counters inverted their own hierarchy.** `CLASS POINTS · 108 ·
  / 32 SPENT` read as "108 of 32". The big number now carries the word
  `UNSPENT` on its own baseline and `32 SPENT` is a separate labelled line.
- **The footer legend and the count overprinted each other** once the count got
  longer. An `SHorizontalBox` does not shrink an oversized child to its slot —
  it draws it at full width straight through its neighbour, which is the same
  bug class as the loadout's chip row. The legend is now clipped to its slot
  and cut back to bindings a player cannot guess.
- **Core cluster chips clipped their own rank digits** at 30px (a `0` read as a
  bracket) and, being single-rank, were empty black boxes. Chips are 36px and
  carry the same centre marks as the path board; `MakeMarkerCore` scales off
  the marker's edge length so both boards speak one language.

Still not verified or not fixed:

- **Hover has never been photographed.** The detail rail, the before/after
  projection and the equip-limit outline are all hover-driven and the harness
  cannot move a mouse. Everything above is the resting state only.
- **The Core map's UNMAPPED cluster holds six nodes.** That is the catch-all
  working as designed, but it means six authored Core nodes sit outside the
  five known `Core.<Constellation>.` prefixes and are therefore on the wrong
  part of the map. That is content naming in `Progression/`, not a UI fix.
- The constellation canvas is 1060 wide inside a ~1300 board viewport, so the
  right third of the Core board is empty. Cosmetic; left alone.
- In COMPARE ALL the node pitch hits its 168px floor and labels sit tight
  against the next column. It scrolls horizontally and nothing actually
  overlaps, so it was left as-is.
- Motion (panel transition, purchase-confirm snap) is still unimplemented.

## Layout fixes, 2026-08-13 (the "numbers clip / clunky" pass)

The screen was authored at a hard 1760x1000 and drawn on an `SCanvas` at fixed
1920x1080-canvas pixels, with no relationship to the window it renders in.
What was fixed, and why:

- **The panel now sizes from the viewport**, read once per rebuild via
  `GEngine->GameViewport->GetViewportSize` and clamped to the authored 1760x1000
  ceiling. Previously a maximised PIE window (about 1920x1000 of client area)
  gave the vertically centred `MaxDesiredHeight(1000)` plate 920px to live in,
  so the header counters and the footer count were sliced off by the viewport,
  and any window narrower than 1840 cut the right-hand column outright. This is
  NOT the banned allotted-size pattern: nothing measures its own arrangement,
  so there is no layout feedback loop — it is the rule `ABreakerPlaytestHUD`
  already follows.
- **The board scrolls in both axes** (a horizontal `SScrollBox` nested inside
  the vertical one), retiring the known compromise above. The branch header
  strip rides inside the horizontal scroll with the columns it labels.
- **Node pitch, label width and label height are derived** from the measured
  board width instead of being frozen at 176/168/86. The label block was the
  literal overflow: `+18 CRIT DAMAGE / RANK (+1 MORE)` and a lock reason like
  `REQUIRES 2 INVESTED (0)` wrapped to five or six lines inside an 86px box on
  a 190px tier pitch, so the copy ran down through the tier hairline beneath
  it. The board line is now compact (`+18 CRIT DMG · +3% DMG`, `GATE 0/2`) with
  the full sentence on the rail, the box is 96px on a 216px tier, and the font
  stays at the 11px caption floor.
- **`EBreakerNodeStatTarget::Damage` was missing from `StatTargetLabel`**, so
  every damage node on the board printed `+4% STAT` — the one stat the owner
  most wanted to read was the one with no name.
- **The right-label margin is only reserved when a node actually labels
  right**, instead of unconditionally widening the board past the panel.
- **Core cluster plates size to their chip rows** (six per row, arithmetic on a
  known count — deliberately not an `SWrapBox`), instead of a flat 156px that
  assumed one row and let a wide cluster's chips run out through the plate.
- **The header compacts below 1500px** rather than pushing BACK off the band.
- `ProgressionGatherTrees` now calls `UBreakerProgressionComponent::GetAvailableTrees`
  instead of walking `ClassDefinition->BranchTrees` alone, so a character with
  no class Data Asset sees the fallback content instead of an empty screen.
