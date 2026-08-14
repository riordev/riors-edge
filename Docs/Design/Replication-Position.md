# Replication Position

> **DRAFT — proposed 2026-08-14; O22 keeps this owner-authored; not law until
> the owner signs off.** This page does not rule anything. It exists so O22
> has a concrete recommendation to react to instead of a blank page. Until
> signed off, `Damage-Pipeline.md` §5's pointer note stands, `Design-Overview.md`
> S3 stays OPEN, and every "server-side" claim below is a proposal, not a fact.

**Scope:** slice (see `Vertical-Slice.md`) — this page recommends a topology
for the vertical slice (solo + invited parties) and explicitly defers the
Conquest/dedicated-server case rather than solving it.
**Last reconciled against: O40**

## 1. The recommendation, stated plainly

| Layer | Position |
|---|---|
| **Combat resolution** | **Server-authoritative.** Damage resolution (`Damage-Pipeline.md` §1's nine steps), the O3/O34 More ceiling, the O34 proc-coefficient law, crit and weak-point rolls, and DoT application/snapshot all execute on the server. The client never resolves a hit; it only requests one and renders the result. |
| **Movement** | **Client-predicted, server-reconciled.** Sprint, dash, slide, mantle, wall ride, and the base two-jump kit run predicted on the client for feel, with the server as the final authority and correcting drift. This is the standard shooter-movement split and nothing here asks for a different one. |
| **Recoil / viewmodel presentation** | **Client-predicted, cosmetic-authoritative.** Recoil pattern, ADS transition, muzzle flash, and viewmodel animation are client-owned presentation — they must never be re-derived from a server round trip, or aiming will feel like the game is lying to the player about where their own gun is pointed. |
| **Trace-follows-aim (hit detection)** | **Evaluated server-side.** The server resolves whether a shot's trace actually connects, against server-side positions. Client rewind/lag-compensation tolerance (how far back the server is willing to rewind other players' positions to validate a client's shot) is **flagged as a later problem** — the slice does not need to solve it, because the slice's topology (below) makes it a small-number-of-players problem, not an internet-scale one. |
| **Topology (the slice)** | **Listen-server.** Solo play and invited parties (the slice's only two configurations) run as a listen server hosted by one participant. No dedicated server infrastructure for the slice. |
| **Topology (post-slice / Conquest)** | **Deferred.** Conquest's matchmade, nine-plus-player scale is explicitly a **different topology problem** (likely dedicated servers) and is not solved by this page. Do not build the slice's networking as if listen-server must scale to Conquest — it does not have to, and pretending it does would gold-plate the slice for a mode that is not being built yet. |
| **Saves** | **Unchanged — local, single-player semantics.** `Save-Architecture.md`'s three-tier local save model is not a replication concern for the slice. It stays exactly as designed; this page does not touch it. |

## 2. Why this split, briefly

- **Server-authoritative combat** is the one non-negotiable: it is what makes the O3/O34 More ceiling and the O34 proc-coefficient law actually enforceable rather than advisory. A client that could resolve its own damage could also lie about it, and a loot game where the drop table is downstream of damage dealt cannot afford that.
- **Client-predicted movement and viewmodel** is the one non-negotiable in the other direction: movement is a stated pillar of this game (O26 aside — it dropped in *priority*, not in *quality bar*), and a movement-heavy shooter with server-round-trip-latency-visible aiming or dashing is not a shippable feel, regardless of topology.
- **Listen-server for the slice** is the cheap, correct-for-scope answer: the slice's only two configurations (solo, invited party) never exceed a handful of players, which is exactly the case listen-server topology handles well and dedicated-server infrastructure would be over-engineering for. It also means the slice needs no new infrastructure investment to ship.
- **Deferring Conquest and lag-compensation tuning** follows directly from O4's own discipline (breadth of viable options now, optimization later) applied to engineering rather than balance: solving a nine-plus-player dedicated-server problem before the three-to-five-player slice ships would be solving next year's problem before this month's.

## 3. What each system must do when this ratifies

| System | Required change |
|---|---|
| **Weapons** | Fire requests originate client-side (for responsiveness) but resolve server-side: hit trace, damage, and ammo/heat state are server-authoritative. Muzzle flash, recoil impulse, and tracer VFX remain client-predicted cosmetics that do not wait for a server round trip. |
| **Abilities** | Activation (cooldown check, resource cost, targeting) is server-authoritative, matching the existing GAS pattern already used for cooldowns/resources. Cast-time presentation (animation, camera work, telegraph VFX) is client-predicted. |
| **DoTs** | Application, snapshot (`SourcePower`, crit state, `DamageOverTimeMultiplier`, tick interval — per `Damage-Pipeline.md` §1 step 1 and O10) and every tick resolve server-side only. No DoT math ever runs client-side, including for the owning player's own applied DoTs. |
| **Movement** | Client predicts locally and simulates immediately; the server runs the same movement code as authority and corrects the client on divergence. The base two-jump kit, dash, slide, mantle, and wall ride all follow this pattern uniformly — no verb gets a bespoke exception. |
| **Save** | No change. Saves remain local and single-player-scoped; nothing about combat or movement replication touches the save file format or its authority model. |

## 4. Sign-off checklist

For the owner, when ready to rule on O22:

- [ ] Server-authoritative combat (damage resolution, O3/O34 More ceiling, O34 proc law) — approve / amend / reject
- [ ] Client-predicted movement with server reconciliation — approve / amend / reject
- [ ] Client-predicted recoil/viewmodel as cosmetic-only — approve / amend / reject
- [ ] Trace-follows-aim evaluated server-side; lag-compensation tolerance explicitly deferred — approve / amend / reject
- [ ] Listen-server topology for the slice (solo + invited parties only) — approve / amend / reject
- [ ] Dedicated-server topology explicitly deferred to Conquest-scale matchmaking, not solved now — approve / amend / reject
- [ ] Saves keep local, single-player semantics for the slice — approve / amend / reject

**On sign-off:** this banner is replaced with a ratified date and O-number, `Damage-Pipeline.md` §5's pointer note is replaced with a direct reference to this page, and `Design-Overview.md` S3 is marked RULED against the new O-number — none of which happens until the owner checks the boxes above.
