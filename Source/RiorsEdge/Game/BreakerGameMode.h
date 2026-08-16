#pragma once

#include "Containers/Ticker.h"
#include "HAL/IConsoleManager.h"

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Game/BreakerWaveBudget.h"
#include "Game/BreakerCoverRegistry.h"
#include "BreakerGameMode.generated.h"

UCLASS(Blueprintable)
class RIORSEDGE_API ABreakerGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ABreakerGameMode();
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
    // The three shipped maps have no authored PlayerStart (they are runtime-
    // built worlds), and the engine's answer to "no start spot" is a log
    // error and no pawn — no pawn means no field frame means no world, since
    // every builder derives its frame from the possessed pawn. The fallback
    // spawns a transient start at the runtime-build origin instead. An
    // authored PlayerStart in any map (Lvl_FirstPerson's, or a future lit
    // map's) wins untouched.
    virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
    UFUNCTION(BlueprintCallable, Category="Playtest") void ResetPlaytestTargets();

    // Enemies never target players inside the zone and will not enter it.
    UFUNCTION(BlueprintPure, Category="Playtest") bool IsInSafeZone(const FVector& Location) const;
    UFUNCTION(BlueprintPure, Category="Playtest") FVector GetSafeZoneCenter() const { return SafeZoneCenter; }
    UFUNCTION(BlueprintPure, Category="Playtest") float GetSafeZoneRadius() const { return SafeZoneRadius; }

    // O2 PLACEHOLDER. DERIVED: the safe ring is meant to read as "inside the
    // compound", and the compound the player actually stands in is
    // Lvl_FirstPerson's 4000 x 4000 cm template courtyard (half-extent 2000).
    // 1800 fills it to just inside the wall. WAS 900, which suppressed enemies
    // over a quarter of the courtyard and left the rest of a sealed room
    // nominally hostile with nothing able to reach it.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest", meta=(ClampMin="0")) float SafeZoneRadius = 1800.0f;

    // =======================================================================
    //  SPATIAL GRAMMAR  (Docs/Design/Level-Design.md)
    // =======================================================================
    // Every constant in this section is DERIVED from a movement constant on
    // UBreakerCharacterMovementComponent, and the arithmetic is written beside
    // it. They exist because the owner has retuned MOVEMENT three times against
    // a complaint that is about SPACE — "walk speed feels weird but i think its
    // a map scope issue" — which is evidence that the retunes were treating a
    // symptom. Retuning the space now costs the same as retuning movement did.
    //
    // O26: nothing here touches a movement value. If the arithmetic says a
    // movement number is wrong, that is a recommendation in the doc, not an
    // edit to Movement/.
    // O2: every value below is a PLACEHOLDER. None has been playtested.
    //
    // Source constants, read out of the movement component (2026-08-14):
    //   WalkSpeed 700, SprintSpeed 1100, JumpZVelocity 700, GravityScale 1.38,
    //   ApexGravityMultiplier 1.50 / FallGravityMultiplier 1.80 / band 220,
    //   MaxAcceleration 4200, BrakingDecelerationWalking 2400,
    //   DashSpeedFloor 1500 + DashSpeedBonus 200, DashCooldown 4.0,
    //   SlideMaxDuration 1.0, WallRideMaxDuration 0.85,
    //   WallRideJumpAwaySpeed 650, WallRideTraceDistance 85.
    // Everything below follows from those.

    // THE UNIT OF TRAVERSAL. SprintSpeed 1100 x DashCooldown 4.0 = 4400 cm.
    // A space whose long axis is SHORTER than this can be crossed on foot
    // between two dashes, so inside it the dash can only ever be a dodge and
    // never a route choice — the verb is present and structurally unusable.
    // The template courtyard is 4000 cm across, which is why it is.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Field", meta=(ClampMin="100")) float DashRefreshDistance = 4400.0f;   // O2 PLACEHOLDER

    // Minimum width of a corridor a player is expected to SPRINT down. A wall
    // at lateral distance d passing at speed v produces peak peripheral optical
    // flow of v/d rad/s; sustained flow above ~2.2 rad/s (126 deg/s) at the
    // frame edge smears and reads as "too fast for this space" — which is the
    // sentence the owner wrote about walk speed. 1100 / 2.2 = 500 cm of
    // clearance per side, so 1000 cm of corridor; 1100 carries a 10% margin.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Field", meta=(ClampMin="100")) float SprintCorridorWidth = 1100.0f;   // O2 PLACEHOLDER
    // Same rule at dash speed: (DashSpeedFloor 1500 + Bonus 200) / 2.2 = 773
    // per side -> 1546, rounded to 1600. Any lane the player is meant to DASH
    // down rather than sprint down is sized on this one.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Field", meta=(ClampMin="100")) float DashCorridorWidth = 1600.0f;   // O2 PLACEHOLDER

    // A pocket a movement build can actually circle in. Minimum turn radius is
    // v^2 / MaxAcceleration: at sprint 1100^2/4200 = 288 cm, at dash speed
    // 1700^2/4200 = 688 cm. An orbit that is exactly the minimum radius is a
    // rail, not a choice, so the pocket carries twice the dash radius plus
    // ~600 cm of body-and-cover clearance: 2 x 688 + 600 = 1976 -> 2000.
    // Independently corroborated: Encounter-Design 3.3 specifies a 4000 x 4000
    // arena, which is exactly this radius doubled.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Field", meta=(ClampMin="100")) float CombatPocketRadius = 2000.0f;   // O2 PLACEHOLDER

    // Maximum spacing between cover pieces on ground a LATTICE ranged enemy
    // covers. The player's whole warning is WindupSeconds 0.85 plus the
    // projectile's time of flight, and the flight is shortest at the near edge
    // of the band: 900 / ProjectileSpeed 1100 = 0.82 s. 0.85 + 0.82 = 1.67 s,
    // and a sprinting player covers 1100 x 1.67 = 1837 cm in it. Cover spaced
    // wider than that leaves ground where a telegraph cannot be answered by
    // moving, which O1/Encounter-Design 0 forbids (movement is the only active
    // defence). 1700 keeps a margin.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Field", meta=(ClampMin="100")) float CoverPitchMax = 1700.0f;   // O2 PLACEHOLDER
    // Depth of clear ground a ranged enemy needs to use its whole band:
    // MaxEngagementDistance 1900 + MinEngagementDistance 900 = 2800 cm. With
    // less than this the Lattice backs into geometry and its retreat gear —
    // the half of the archetype that makes it not-a-chaser — never fires.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Field", meta=(ClampMin="100")) float RangedSightlineDepth = 2800.0f;   // O2 PLACEHOLDER

    // JUMP GAPS. One flat-to-flat jump at sprint covers SprintSpeed x total
    // airtime. Airtime under the current curve: rise 700 -> 220 at 1.38 x 980
    // (0.355 s) plus the apex band 220 -> 0 at ~1.25 x gravity (0.130 s) =
    // 0.485 s up to an apex of 178 cm; the fall back through the band and then
    // at 1.80 x gravity takes 0.389 s. Airtime 0.874 s -> 1100 x 0.874 = 961 cm.
    // A one-jump gap must therefore sit well INSIDE 961; 700 leaves 27% margin
    // and is still far past a step.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Field", meta=(ClampMin="0")) float OneJumpGap = 700.0f;   // O2 PLACEHOLDER
    // Two jumps (O25 base kit) taken at the first apex: 0.485 + 0.485 rise plus
    // 0.548 s falling from 355 cm = 1.518 s -> 1670 cm. Taken as late as
    // possible it reaches 1923 cm, but that is a timing trick. A gap that
    // DEMANDS both jumps must exceed the 961 single-jump reach and stay inside
    // the 1670 comfortable double: 1400 is the middle of that band.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Field", meta=(ClampMin="0")) float TwoJumpGap = 1400.0f;   // O2 PLACEHOLDER
    // Swift's third jump (O25) adds a third 0.485 s rise and a 0.669 s fall
    // from 532 cm: 2.124 s -> 2336 cm. A Swift-only gap must clear the 1670
    // comfortable double and stay inside 2336. This gap USED TO BE UNCROSSABLE
    // in the gym: SwiftThirdJumpUnlockLevel was 20 and nothing writes
    // CharacterLevel, so no Swift character could ever reach a third jump
    // (owner: "i never could do a 3rd jump"). The gate now defaults to
    // reachable, so this is the piece of geometry that tells a Swift player
    // apart from everyone else — and the piece that will silently become
    // impassable again if that gate is ever raised past a level the game can
    // produce.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Field", meta=(ClampMin="0")) float SwiftThreeJumpGap = 2100.0f;   // O2 PLACEHOLDER
    // Tallest step the mantle accepts (Movement-Design: 35-150 cm). A stair of
    // steps at this height is climbable without jumping at all, which is the
    // "conventional route is never punished" rule (master sheet 5.4).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Field", meta=(ClampMin="35", ClampMax="150")) float MantleStepHeight = 145.0f;   // O2 PLACEHOLDER

    // WALL RIDE. The ride is speed-neutral and capped at WallRideMaxDuration
    // 0.85 s, so one full ride entered at sprint covers 1100 x 0.85 = 935 cm.
    // A wall shorter than that ends the verb before it has read; this is three
    // full rides long so a ride can start anywhere in the first two thirds.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Field", meta=(ClampMin="100")) float WallRideWallLength = 2800.0f;   // O2 PLACEHOLDER
    // Gap between the two walls of a ride-and-chain corridor. A wall jump
    // leaves at WallRideJumpAwaySpeed 650 cm/s and stays high enough to
    // re-contact for roughly 0.85 s, so the crossable gap is 650 x 0.85 = 552.
    // WAS 700 in the shipped field, which needs 1.08 s to cross — the gym's own
    // wall lane could not be chained on its own numbers.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Field", meta=(ClampMin="100")) float WallRideCorridorWidth = 550.0f;   // O2 PLACEHOLDER
    // The entry trace is horizontal from the capsule centre, so a wall must
    // still be there when the player is at the top of a jump: apex 178 + capsule
    // half-height 88 = 266 cm of centre height, plus headroom. 500 is generous
    // on purpose — a wall that runs out vertically drops the ride silently.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Field", meta=(ClampMin="100")) float WallRideWallHeight = 500.0f;   // O2 PLACEHOLDER
    // Recorded, not yet consumed: nothing in the runtime field has a ceiling.
    // Apex 178 + standing height 176 = 354 cm, so any roof under ~360 cm
    // deletes the jump without telling anyone. The first authored interior gets
    // this number instead of a guess.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Field", meta=(ClampMin="0")) float MinCeilingClearance = 400.0f;   // O2 PLACEHOLDER

    // FIELD EXTENTS, in dash-refresh units so the scale statement is legible:
    // 5 x DashRefreshDistance forward, 2.5 each side. A player crossing the
    // long axis at sprint spends 22000 / 1100 = 20 s and must spend five dashes
    // to beat that, which is what makes route choice exist at all.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Field", meta=(ClampMin="1000")) float FieldForwardExtent = 22000.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Field", meta=(ClampMin="1000")) float FieldHalfExtent = 11000.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Field", meta=(ClampMin="0")) float FieldRearExtent = 3000.0f;   // O2 PLACEHOLDER

    // STATIONS along the forward axis, all spaced by at least one
    // DashRefreshDistance so moving between them is a traversal decision.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Field", meta=(ClampMin="0")) float RangeFiringLineDistance = 4600.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Field", meta=(ClampMin="0")) float EncounterPocketDistance = 8500.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Field", meta=(ClampMin="0")) float ArenaDistance = 17000.0f;   // O2 PLACEHOLDER

    // THE BREACH. Lvl_FirstPerson's courtyard is SEALED: a continuous 400 cm
    // parapet on all four sides with no doorway (measured, see LogGymSummary).
    // Two base-kit jumps reach 355 cm, so the player could only leave by
    // climbing the 200 cm inner ledge and then the 200 cm upper wall — a
    // discovery, not an exit. The field spawns a collapsed embankment over the
    // wall instead. Removing the wall properly is editor work; this makes the
    // field reachable in the meantime and reads as overgrown-Earth ruin (O24).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Field") bool bSpawnBreachRamp = true;
    // Crest height. The wall tops out at 400 cm and the upper course is inset,
    // so the ramp surface has to clear 400 with margin across X 1800-2000.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Field", meta=(ClampMin="0")) float BreachCrestHeight = 520.0f;   // O2 PLACEHOLDER

    // GROUND PLANE. Every spawn function used to take the ground as "pawn
    // location minus 88 cm of capsule". That is only the ground if the pawn
    // spawned ON it, and in Lvl_FirstPerson it does not: the PlayerStart sits
    // on the template's 210 cm central plinth, so the ENTIRE runtime field was
    // being built on a phantom plane 212 cm above the real floor. The frame now
    // probes for the floor; this overrides the probe for a map where it is
    // wrong.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Field") bool bUseGroundZOverride = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Field") float GroundZOverride = 0.0f;
    // Probe ring radius: outside the template's 350 cm plinth, inside its
    // 1800 cm wall face.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Field", meta=(ClampMin="100")) float GroundProbeRadius = 1500.0f;

    // COPLANARITY. Owner: "a lot of the textures on the ground were tearing."
    // The apron's top face is at the probed ground plane exactly, and anything
    // else authored with a top face at that same height is COPLANAR with it —
    // the depth test then has no winner and the pair stipples and flickers.
    // This is the separation every slab laid ON the apron must use. It is
    // centimetres on purpose: far under MantleStepHeight and far under the
    // engine's step-up, so nothing trips on it and no gap reads as a ledge.
    // Zero re-creates the bug; that is deliberate, so an A/B is one field edit.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Field", meta=(ClampMin="0")) float GroundOverlayLift = 6.0f;   // O2 PLACEHOLDER (was 0 — coplanar)
    // Tint-patch placements ATTEMPTED. Overlapping placements are rejected —
    // two plates authored at the same height that overlap are coplanar, which
    // was the second and larger half of the same tearing report — so the field
    // settles at rather fewer patches than this. Attempts, not patches.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Field", meta=(ClampMin="0")) int32 FieldPatchAttempts = 420;   // O2 PLACEHOLDER (was a flat 200 placements, unrejected)

    // --- THE FIELD MARSHAL (Encounter-Design §3) ---------------------------
    // Spawns the boss at the elite arena and teleports nothing: the player
    // walks there, which is the §5.2 "approach" beat and is the only part of
    // the pacing curve a gym can actually give you.
    //
    // Reachable three ways, because the obvious one is not available to this
    // lane: the playtest keys live on ABreakerCharacter (Characters/, which
    // this lane does not own), so the binding is installed onto the PLAYER
    // CONTROLLER's input component from HandleStartingNewPlayer instead, and a
    // console command backs it up.
    UFUNCTION(BlueprintCallable, Category="Playtest|Boss") void SpawnBossTest();
    UFUNCTION(BlueprintPure, Category="Playtest|Boss") bool IsBossAlive() const;
    UFUNCTION() void HandleBossDefeated();

    // The boss needs ±1900 cm of clear ground for its gallery offsets and
    // ±1700 for its alcoves — it spawns adds and orders them into those points,
    // and a gallery inside a wall is an order with nowhere to land. Checked
    // against the arena at spawn time rather than asserted in a comment.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Boss", meta=(ClampMin="0")) float BossArenaClearanceCm = 1900.0f;   // O2 PLACEHOLDER

    // Wave mode: the TTK measurement instrument. Escalating non-respawning
    // waves in the elite arena; every third wave carries an elite.
    UFUNCTION(BlueprintCallable, Category="Playtest|Waves") void StartNextWave();
    // Encounter-Design §4.2's budget, costs, cadence and §5.3's caps, in one
    // authored block. The composition is SOLVED from these rather than ramped,
    // so the whole shape of wave mode is retunable in-editor and provable by
    // automation. O2: every value inside is a PLACEHOLDER.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Waves") FBreakerWaveBudgetParams WaveBudget;
    // What wave N would spawn, without spawning it. Exposed so a tuning pass
    // can read the ramp out of the editor rather than by playing to wave 12.
    UFUNCTION(BlueprintPure, Category="Playtest|Waves")
    FBreakerWaveComposition GetWaveComposition(int32 WaveIndex) const;
    UFUNCTION(BlueprintPure, Category="Playtest|Waves") int32 GetCurrentWave() const { return CurrentWave; }
    UFUNCTION(BlueprintPure, Category="Playtest|Waves") int32 GetWaveEnemiesAlive() const;
    UFUNCTION(BlueprintPure, Category="Playtest|Waves") bool IsWaveActive() const { return CurrentWave > 0 && GetWaveEnemiesAlive() > 0; }

    // --- Area level (O27 / Power-Curve.md) ---------------------------------
    // The gym IS a piece of content, so it has an area level, and that level is
    // the only thing deciding how hard its monsters are. Nothing here reads the
    // player's level, gear or build — a playtester walks the curve by turning
    // GymAreaLevel up, not by levelling a character.
    //
    // Both values are EditAnywhere so a playtest can sweep the curve without a
    // recompile. Area level also drives drop item level, which is the mechanism
    // that makes rising item level correspond to gameplay.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Area", meta=(ClampMin="1", ClampMax="100")) int32 GymAreaLevel = 10;   // O2 PLACEHOLDER

    // Logs what the gym built, so a headless smoke run can prove the
    // encounter spawned rather than merely that the process started.
    void LogGymSummary() const;

    // -BreakerScreenshots=N captures N frames, one per ScreenshotIntervalSeconds,
    // then quits. It exists because the standing gap on every UI change in this
    // project is the same sentence -- "nobody has looked at this on a screen."
    // Automation proves arithmetic and cannot see a layout, which is exactly how
    // two bad visual passes shipped and how a whole HUD's worth of text once
    // rendered as nothing while every test stayed green.
    //
    // Dev-only by construction: it is a command-line switch, so a shipped build
    // cannot reach it. Shots land in Saved/Screenshots.
    void ScheduleScreenshots();
    void CaptureScreenshot();
    // A CORE ticker, not a world timer. Opening the front end calls
    // SetPause(true), which stops world timers dead -- so the first attempt at
    // menu capture opened the screen and then photographed nothing, forever.
    // FTSTicker runs on real time and does not care that the game is paused,
    // which is exactly the property a capture harness needs, because the menus
    // are the thing most worth capturing and they are ALWAYS paused.
    FTSTicker::FDelegateHandle ScreenshotTickHandle;
    double NextScreenshotTime = 0.0;
    int32 ScreenshotsRemaining = 0;
    int32 ScreenshotIndex = 0;
    // First shot waits this long so the gym has spawned, the HUD has ticked and
    // the viewport is not still mid-fade. A screenshot of a black frame passes
    // every check a human is not making.
    UPROPERTY(EditAnywhere, Category="Playtest|Capture") float ScreenshotFirstDelaySeconds = 6.0f;
    UPROPERTY(EditAnywhere, Category="Playtest|Capture") float ScreenshotIntervalSeconds = 2.0f;
    // Per-wave escalation, unchanged from the shipping wave mode's
    // "10 + CurrentWave * 2": later waves climb in level so drops and TTK data
    // climb with them. Now it climbs the CHASSIS too, which is the whole point.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Area", meta=(ClampMin="0")) int32 AreaLevelPerWave = 2;   // O2 PLACEHOLDER
    // The area level of wave N. Pure, so a test can walk the escalation.
    UFUNCTION(BlueprintPure, Category="Playtest|Area") int32 GetAreaLevelForWave(int32 WaveIndex) const;

    // --- The modifier layer reaching a player (O27) -------------------------
    // Ten modifiers, a component and a legality system shipped and NOTHING
    // called ConfigureWithModifiers, so O27's "difficulty lives in bosses,
    // elites and monsters with MODIFIERS, not in trash health" was true of the
    // code and false of the game. This is the switch that makes it true of both.
    //
    // Off turns the gym back into the pre-modifier instrument, which matters
    // because the TTK baseline every earlier session recorded was measured
    // without them.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Modifiers") bool bGrantModifiers = true;
    // Deterministic in the seed, so two runs of the same wave meet the same
    // Champions and a screenshot of wave 4 is comparable with the last one.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Modifiers") int32 ModifierSeedBase = 20260814;   // O2 PLACEHOLDER
    // NON-elite modifier carriers in the standing gym encounter (O27's
    // kill-bucket producer — see Playtest/BreakerKillBuckets.h). Elites keep
    // their current behaviour (GrantModifiers restores rank Elite); these
    // plain trash bodies KEEP the promotion to rank ModifierBearing instead,
    // which is the only thing that makes that TTK bucket non-empty. O9 keeps
    // Rank and Modifiers as separate fields, so this is not a contradiction of
    // the elite's own modifier tell.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Modifiers", meta=(ClampMin="0")) int32 GymModifierCarrierCount = 2;   // O2 PLACEHOLDER

    // --- Ammo economy (O2 placeholders) ------------------------------------
    // Third and last resupply channel alongside kill drops and wave-clear:
    // an amber supply crate in the Anchor camp. Standing next to it for
    // SupplyCrateDwellSeconds fully restocks, then goes on cooldown.
    // Chosen over a safe-zone-wide refill because the crate is a visible,
    // walk-to affordance and needs no NPC/interaction plumbing.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Ammo", meta=(ClampMin="0")) float SupplyCrateRadius = 350.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Ammo", meta=(ClampMin="0")) float SupplyCrateDwellSeconds = 2.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Ammo", meta=(ClampMin="0")) float SupplyCrateCooldownSeconds = 8.0f;

    // --- THE COVER FIELD (Game/BreakerCoverRegistry.h) ---------------------
    // Owner: "redesign the playground to make it have proper cover and over
    // [open] space for the wave mechanics for testing as well."
    //
    // The layout itself is pure maths in UBreakerCoverLayoutLibrary and is
    // proved by RiorsEdge.Game.ArenaLayout.*; what lives here is the handful of
    // numbers that are genuinely NEW rather than transported out of the spatial
    // grammar above. Everything the grammar already owns -- CoverPitchMax,
    // CombatPocketRadius, DashCorridorWidth, ArenaDistance, SafeZoneRadius -- is
    // passed through by MakeCoverFieldParams and is NOT re-authored here, so
    // retuning the grammar retunes the cover field with it.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Cover") bool bBuildCoverField = true;
    // CHEST-HIGH. The standing capsule is 176 cm and the crouched capsule is
    // roughly half of it, so 120 hides a crouched body and leaves a standing
    // one's sights clear. Under MantleStepHeight 145 on purpose: this is cover
    // you can take the high ground on rather than cover that fights the kit.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Cover", meta=(ClampMin="0")) float CoverChestHeight = 120.0f;   // O2 PLACEHOLDER
    // FULL-HEIGHT. Standing 176 + jump apex 178 = 354, so anything under that
    // can be seen over at the top of a jump and is chest cover with extra steps.
    // 400 is the same arithmetic MinCeilingClearance is built on.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Cover", meta=(ClampMin="0")) float CoverFullHeight = 400.0f;   // O2 PLACEHOLDER
    // Cluster pitch, 2 x CoverPitchMax. See the derivation in
    // BreakerCoverRegistry.h: it is simultaneously the largest pitch whose
    // lattice circumradius lands inside CoverPitchMax and the smallest that
    // leaves DashCorridorWidth of clear ground between two clusters.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Cover", meta=(ClampMin="500")) float CoverClusterPitch = 3400.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Cover", meta=(ClampMin="100")) float CoverClusterRingRadius = 700.0f;   // O2 PLACEHOLDER
    // The contested band the cover field fills: from the target range's firing
    // line to past the elite arena, stopping short of the sniper lane and the
    // wall-ride corridor so neither movement instrument is touched.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Cover", meta=(ClampMin="0")) float CoverBandHalfWidth = 5800.0f;   // O2 PLACEHOLDER

    // --- THE SEVERED DRUDGE in the gym (O40c reachability) -----------------
    // ABreakerAlteredEnemy shipped, was tested, and was spawned by NOTHING --
    // the same defect O40(c) exists to prevent: content that exists in the
    // codebase and cannot be reached by a player. It is a heavy, slow,
    // low-and-wide melee body (capsule 120 cm across against the standard 90,
    // 150 cm tall against 180, MoveSpeed 250, 2.0x health) whose weak point is a
    // DORSAL ridge at 129 cm rather than a head -- so the whole archetype is
    // "get behind it", and it needs FLANKING ROOM rather than a corridor.
    //
    // ENCOUNTER-DESIGN GATE, stated rather than skirted: the document gates ALL
    // Altered content behind the Act II turn beat. The gym is a playtest
    // instrument and not campaign content, which is why spawning it here is
    // legitimate; nothing in this file feeds campaign progression, and the
    // Drudge must not be added to anything that does until that beat exists.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Drudge") bool bSpawnDrudges = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Drudge", meta=(ClampMin="0")) int32 GymDrudgeCount = 1;   // O2 PLACEHOLDER
    // First wave a Drudge appears on, and how many melee bodies get rendered as
    // one. It is a SUBSTITUTION, not an addition -- see StartNextWave.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Drudge", meta=(ClampMin="1")) int32 DrudgeFromWave = 3;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Drudge", meta=(ClampMin="1")) int32 DrudgeWaveDivisor = 4;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Drudge", meta=(ClampMin="0")) int32 MaximumDrudgesPerWave = 2;   // O2 PLACEHOLDER
    // Clear ground a Drudge needs around it to be flankable. Its own body is
    // 120 cm across and the answer to it is getting behind it, so it is spawned
    // where a full circle of this radius is free of hard cover.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Drudge", meta=(ClampMin="0")) float DrudgeFlankClearanceCm = 900.0f;   // O2 PLACEHOLDER

public:
    // The one frame every spawn function works in. Ground is a point on the
    // REAL floor directly under the spawn; Forward/Right are the spawn facing.
    // Cached once at first spawn, so an F1 reset rebuilds the field where it
    // was authored instead of wherever the player happened to be standing —
    // which is what the old "pass the pawn to every spawner" shape did.
    struct FFieldFrame
    {
        FVector Ground = FVector::ZeroVector;
        FVector Forward = FVector::ForwardVector;
        FVector Right = FVector::RightVector;
        float SpawnZ = 0.0f;
        FVector At(float Fwd, float Rgt, float Up = 0.0f) const
        {
            return Ground + Forward * Fwd + Right * Rgt + FVector(0.0f, 0.0f, Up);
        }
    };

private:
    FFieldFrame Frame;
    bool bFieldFrameSet = false;
    void BuildFieldFrame(const APawn* Pawn);
    // Bound to the hub travel point. Teleports only — see the implementation
    // for why travel must never re-run the gym build.
    void HandleHubTravelSelected(FName DestinationId, APawn* RequestingPawn);
public:
    // Where a session starts. PLAY puts the chosen character here, and the
    // hub's travel point is what reaches the gym from it.
    UFUNCTION(BlueprintCallable, Category="Breaker|Hub") void TeleportPawnToHub(APawn* Pawn);
private:
    FVector HubOrigin = FVector::ZeroVector;
    bool bHubBuilt = false;
    float ResolveGroundZ(const APawn* Pawn) const;

    // Vantage cameras for -BreakerCaptureTour. A screenshot of the spawn view
    // says nothing about a LAYOUT, and a layout is exactly what this pass
    // changes, so the harness can be pointed at the field from above and from
    // each station instead.
    UPROPERTY() TArray<TObjectPtr<class ACameraActor>> TourCameras;
    void BuildCaptureTour();

    bool bPlaytestTargetsSpawned = false;
    // Every piece of hard cover the field built, in world space, WITH ITS CLASS
    // AND HEIGHT. Populated during SpawnExpandedField, so it survives an F1
    // reset (which rebuilds the enemies but not the geometry).
    //
    // It was a bare TArray<FVector>, which recorded position and nothing else --
    // so the placement code could not tell a 500 cm pillar from a 110 cm block,
    // and the difference between those two is the difference between breaking
    // line of sight and standing behind a footstool.
    FBreakerCoverRegistry CoverRegistry;
    int32 CurrentWave = 0;
    UPROPERTY() TArray<TObjectPtr<class ABreakerEnemy>> WaveEnemies;
    UPROPERTY() TObjectPtr<class ABreakerBossEnemy> ActiveBoss;
    // Console fallback for the boss key, registered once. Held so it is
    // unregistered on EndPlay — a stale IConsoleCommand outliving its game mode
    // is a dangling this pointer the next PIE session walks straight into.
    IConsoleCommand* BossConsoleCommand = nullptr;
    FVector SafeZoneCenter = FVector::ZeroVector;
    bool bSafeZoneSet = false;
    FVector SupplyCrateLocation = FVector::ZeroVector;
    bool bSupplyCrateSet = false;
    float SupplyCrateDwell = 0.0f;
    double LastSupplyCrateUseTime = -1000.0;
    void TickSupplyCrate(float DeltaSeconds);
    // Refills the first player's weapon to full (both slots).
    void RefillPlayerAmmo();
    // The field, laid out against the derived grammar above.
    void SpawnExpandedField();
    void SpawnBreach();
    void SpawnJumpGapRun();
    // bRimRuins false leaves out the broken-wall arc. The elite arena uses it:
    // the arc sits at 1800-2200 cm from centre and the Field Marshal's gallery
    // offsets are +/-1900 and its alcoves +/-1700, so in the ARENA the arc is
    // geometry the boss gives orders into. SpawnBossTest has warned about
    // exactly this since it was written; this is the fix rather than the warning.
    void SpawnCombatPocket(float Fwd, float Rgt, struct FRandomStream& Stream, bool bRimRuins = true);
    // THE COVER FIELD. Builds UBreakerCoverLayoutLibrary's answer in the world
    // and registers every piece. Called from SpawnExpandedField.
    void SpawnCoverField();
    FBreakerCoverFieldParams MakeCoverFieldParams() const;
    // A Drudge wants a clear circle to be flanked in. Returns a point near
    // Around whose DrudgeFlankClearanceCm neighbourhood carries no hard cover,
    // or Around itself when the field has none to avoid.
    FVector FindFlankableGround(const FVector& Around, float SearchRadius) const;
    class ABreakerAlteredEnemy* SpawnDrudge(const FVector& Around, float PatrolPhase, int32 AreaLevel);
    void SpawnPlaytestTargets();
    void SpawnMovementCourse();
    void SpawnCombatEncounter();

    // --- The cover registry ------------------------------------------------
    // ABreakerSkirmisherEnemy has a PLACEMENT REQUIREMENT, not a preference:
    // it is the only enemy in the project that breaks line of sight, and its
    // own class note says so plainly — "in an empty field it correctly finds no
    // cover and degrades to an open-ground shooter, which is honest but is NOT
    // the fight." Its cover search is a ring of SearchRadiusCm (1400) around
    // ITSELF, filtered to candidates whose line from the threat is blocked, so
    // whether the archetype exists at all is decided by where it is spawned.
    //
    // The field used to build its hard cover as a side effect of other
    // spawners — pocket blocks, pocket pillars, one piece on the sniper lane —
    // and record only WHERE, not WHAT. So the placement code could not tell a
    // 500 cm pillar from a 110 cm block, and there was no cover at all outside
    // four discs in 250 m of field. SpawnCoverField now builds the whole thing
    // from UBreakerCoverLayoutLibrary and records every piece with its class and
    // height, which is what lets the Skirmisher ask for a LINE BREAK rather than
    // for the nearest lump of geometry.
    void RegisterCoverAnchor(const FVector& WorldLocation, EBreakerCoverClass Class, float HeightCm);
    // Nearest recorded cover to Around, within MaxDistance. False means "there
    // is no cover here", which is a real answer. Kept as the class-agnostic
    // query for callers that only need a position; SpawnSkirmisherNearCover uses
    // the registry's class-filtered form directly.
    bool FindCoverAnchorNear(const FVector& Around, float MaxDistance, FVector& OutAnchor) const;
    // Spawns a Skirmisher AT a cover anchor near Around, standing off from the
    // threat side so its first act is to duck rather than to walk into view.
    class ABreakerSkirmisherEnemy* SpawnSkirmisherNearCover(const FVector& Around,
        const FVector& ThreatLocation, float PatrolPhase);

    // Rolls a legal modifier set onto an enemy and then RESTORES the rank it
    // was authored with.
    //
    // ABreakerEnemy::ConfigureWithModifiers promotes the rank to
    // ModifierBearing unconditionally, which is correct for trash and wrong for
    // an elite: ModifierBearing is x2.5/x1.25 against Elite's x3.0/x1.5, so
    // calling it on the arena elite would quietly DEMOTE it. Restoring the rank
    // afterwards rebuilds the chassis, which invalidates the Warded ward
    // (sized off max health), so the set is re-published last — the same
    // re-derive-after-the-chassis order ConfigureWithModifiers itself uses, and
    // for the same reason.
    void GrantModifiers(class ABreakerEnemy* Enemy, int32 Seed) const;
    // Grants modifiers to a NON-elite enemy that must KEEP rank
    // ModifierBearing at kill time. Unlike GrantModifiers, this does NOT
    // capture-and-restore an authored rank: ConfigureWithModifiers's
    // unconditional promotion to ModifierBearing IS the desired outcome for a
    // carrier. See Playtest/BreakerKillBuckets.h for why this bucket existed
    // and was never reachable before this pair of functions.
    void GrantModifierCarrier(class ABreakerEnemy* Enemy, int32 Seed) const;
    // §4.3's "loot only on rest and boss waves". Reaches a protected property
    // by reflection because adding a setter would mean editing Combat/ — see
    // the note at the implementation.
    void SetEnemyDropsLoot(class ABreakerEnemy* Enemy, bool bDrops) const;
    void SpawnSafeZone();
    void SpawnAnchorCamp();
    // Overgrown-Earth dressing (O24): vegetation, ruins, and scattered tech.
    // Cosmetic only — no gameplay values, spawn points, or radii change.
    void SpawnWorldDressing();
    void SpawnOvergrowth(struct FRandomStream& Stream);
    void SpawnRuins(struct FRandomStream& Stream);
    void SpawnScatteredTech(struct FRandomStream& Stream);
};
