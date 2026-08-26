#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BreakerCoverRegistry.generated.h"

// THE COVER FIELD — the gym's combat ground, as pure world-free maths.
//
// WHY THIS EXISTS. Owner: "redesign the playground to make it have proper cover
// and over [open] space for the wave mechanics for testing as well."
//
// What was there before was cover BY ACCRETION. Every piece of hard cover in the
// gym was a side effect of some other spawner: four blocks and a pillar inside
// each combat pocket, one block on the sniper lane, and nothing anywhere else.
// Twenty-one anchors across 250 m of field, all of them inside four discs of
// radius 2000 cm. Wave mode does not spawn in those discs — `StartNextWave`
// builds its arena at `player + forward * DashRefreshDistance`, wherever the
// playtest happens to be standing — so the SKIRMISHER, the one archetype whose
// whole design is breaking line of sight, spawned into open ground on almost
// every wave and logged the warning that says so. The instrument was measuring
// an archetype that was not there.
//
// The other half of the same complaint is that there was no OPEN ground either,
// in the sense that matters: 250 m of featureless flat is not open space, it is
// absence. Open ground is only legible against something to pass.
//
// THE SHAPE. A triangular lattice of cover clusters over the contested band,
// with the derivation written beside every number below. Two properties fall
// straight out of the lattice and are asserted by automation rather than
// believed:
//
//   * MAXIMUM UNCOVERED GAP. The circumradius of a triangular lattice of pitch
//     P is P/sqrt(3). At P = 3400 that is 1963 cm to the nearest cluster CENTRE,
//     and the cluster's pieces sit on a ring of 700, so the nearest PIECE is at
//     worst 1549 cm. Level-Design G23 caps cover pitch at 1700 (hard limit
//     1837) because ground further than that from cover is ground where a
//     LATTICE telegraph cannot be answered by moving, and under O1 movement is
//     the only active defence. 1549 < 1700, by construction.
//
//   * MINIMUM OPEN LANE. Every neighbour in a triangular lattice is exactly P
//     away, so the narrowest gap between two clusters is P minus twice the
//     cluster's own extent: 3400 - 2 x (700 + 150) = 1700 cm. That is wider
//     than DashCorridorWidth (G3, 1600), which is the width at which peripheral
//     optical flow at dash speed stays under the 2.2 rad/s smear threshold. So
//     there is a dashable lane between EVERY pair of adjacent clusters, in six
//     directions from every cluster, everywhere in the band.
//
// Those two sentences together are the "interleave" the brief asks for, stated
// as measurements: cover is never more than one telegraph window away, and open
// ground is never narrower than a dash lane. Neither is a mood.
//
// WHAT THE LATTICE ACTUALLY SURVIVES AS, said plainly because the arithmetic
// above is only half the story. The band this field has to fill is not empty:
// the target range's firing corridor, the jump-gap run and the elite arena
// between them exclude most of its centre, and the raw lattice comes through at
// only a handful of nodes. So the lattice is the GENERATOR, not the whole
// answer — a FILL PASS then measures the band and drops a cluster (or a lone
// chest-high piece where only that fits) at every hole, and a LINE-BREAK PASS
// does the same for full-height cover with a minimum separation that preserves
// the dash lane. Every one of those passes is deterministic and every one of
// them is measured afterwards by the same functions the tests call. The
// measured field, at the shipped parameters:
//
//   106 pieces (79 chest-high, 27 full-height)
//   largest uncovered gap              1645 cm  CEILING 1700   (G23)
//   furthest ground from a line break  2647 cm  CEILING 3400   (2 x G23)
//   narrowest FULL-HEIGHT lane         1700 cm  FLOOR   1600   (G3)
//   nearest piece to corridor centre   (per field)  FLOOR CorridorHalfWidth
//   tightest gap between any two pieces 259 cm  FLOOR    120   (widest body)
//   cover footprint                    3.03%             BAND  0.5%-5.0%
//
// EVERY LINE STATES ITS DIRECTION, in status.py's vocabulary, because two of
// these pass by being UNDER their number and two by being OVER it and the
// figures alone do not say which. A reader who assumes the wrong direction
// reads a passing field as failing, or worse, the reverse. DescribeCoverField
// prints the same words at runtime.
//
// Those five lines are what the automation asserts. If a parameter changes and
// one of them moves the wrong way, the test names which.
//
// O2: every value in FBreakerCoverFieldParams is a PLACEHOLDER. Layout
// dimensions are not balance, but the ones that are not forced by the geometry
// above are marked and are EditAnywhere on ABreakerGameMode.

// Hard cover comes in exactly two heights because a 176 cm character can only
// be in two relationships with a wall: behind it, or behind it and blind.
UENUM(BlueprintType)
enum class EBreakerCoverClass : uint8
{
    // CHEST-HIGH. Crouch behind it, shoot over it. The standing capsule is
    // 176 cm (half-height 88) and the crouched capsule is roughly half that, so
    // 120 cm hides a crouched body completely and leaves a standing body's
    // shoulders and sights clear. It is also under MantleStepHeight (145), so
    // it is climbable — cover you can take the high ground on rather than cover
    // that fights the movement kit.
    ChestHigh,
    // FULL-HEIGHT. Breaks the line outright. 400 cm, because standing height
    // 176 plus jump apex 178 is 354: anything shorter than that can be seen
    // over at the top of a jump, which makes it chest-high cover with extra
    // steps. This is the only class that stops a LATTICE orb — the orb is a
    // real replicated actor and the trace is real, so height is not cosmetic.
    FullHeight
};

// One piece of hard cover, in FIELD coordinates: forward and right offsets from
// the spawn frame, exactly the units FFieldFrame::At consumes. Nothing here is
// world space, which is the whole reason it can be tested.
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerCoverPiece
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Cover") float Forward = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category="Cover") float Right = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category="Cover") float HeightCm = 0.0f;
    // Footprint half-extents before rotation. Kept per piece so the open-lane
    // and cover-ratio measurements are exact rather than assumed from a class.
    UPROPERTY(BlueprintReadOnly, Category="Cover") float HalfLengthCm = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category="Cover") float HalfDepthCm = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category="Cover") float YawDegrees = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category="Cover") EBreakerCoverClass Class = EBreakerCoverClass::ChestHigh;
    // Which cluster this piece belongs to. -1 for the standalone pieces (the
    // sniper lane's hard cover, the corridor shoulders, the arena). The open-lane
    // measurement is between DIFFERENT clusters: inside a cluster the pieces are
    // deliberately close, because that is what makes it a cluster and not a
    // scatter.
    UPROPERTY(BlueprintReadOnly, Category="Cover") int32 ClusterIndex = -1;

    // Conservative circular extent, used by every clearance test.
    float ExtentCm() const { return FMath::Max(HalfLengthCm, HalfDepthCm); }
    float FootprintAreaCm2() const { return 4.0f * HalfLengthCm * HalfDepthCm; }
};

// Every dimension the layout is built from. Filled by ABreakerGameMode out of
// the spatial grammar it already owns (Level-Design §3), so there is exactly one
// source of truth for CoverPitchMax, CombatPocketRadius and the rest — this
// struct transports them, it does not re-author them.
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerCoverFieldParams
{
    GENERATED_BODY()

    // --- Transported from the grammar (Level-Design §3) --------------------
    // G23. Ground further than this from cover cannot answer a LATTICE
    // telegraph by moving. Hard limit 1837; 1700 is the authored margin.
    UPROPERTY(BlueprintReadOnly, Category="Cover|Grammar") float CoverPitchMaxCm = 1700.0f;
    // G3. The narrowest lane a player is expected to cross at dash speed.
    UPROPERTY(BlueprintReadOnly, Category="Cover|Grammar") float DashCorridorWidthCm = 1600.0f;
    // G6.
    UPROPERTY(BlueprintReadOnly, Category="Cover|Grammar") float CombatPocketRadiusCm = 2000.0f;
    // G22. The clear depth a LATTICE needs to work its whole 900-1900 band.
    UPROPERTY(BlueprintReadOnly, Category="Cover|Grammar") float RangedSightlineDepthCm = 2800.0f;
    UPROPERTY(BlueprintReadOnly, Category="Cover|Grammar") float SafeZoneRadiusCm = 1800.0f;
    UPROPERTY(BlueprintReadOnly, Category="Cover|Grammar") float ArenaDistanceCm = 17000.0f;

    // --- The lattice --------------------------------------------------------
    // Cluster pitch. DERIVED, not chosen: it is the largest pitch at which the
    // lattice's own circumradius still lands inside CoverPitchMax once the
    // cluster ring is subtracted, and simultaneously the smallest pitch that
    // leaves DashCorridorWidth of clear ground between two adjacent clusters.
    // 2 x CoverPitchMax is the round number in the middle of that window.
    UPROPERTY(BlueprintReadOnly, Category="Cover|Lattice") float ClusterPitchCm = 3400.0f;   // O2 PLACEHOLDER
    // Radius of the ring the cluster's pieces stand on. Big enough that a player
    // can be BETWEEN two pieces of one cluster (the capsule is 70 cm across and
    // the chord between neighbours at 90 degrees is 990 cm), small enough that
    // the cluster still reads as one object from across the field.
    UPROPERTY(BlueprintReadOnly, Category="Cover|Lattice") float ClusterRingRadiusCm = 700.0f;   // O2 PLACEHOLDER
    // Three chest-high and one full-height per cluster. The ratio is the design:
    // most cover is cover you can shoot over, and there is exactly ONE line
    // break per cluster — which is what a Skirmisher needs to exist and what
    // stops the field becoming a maze of blind corners.
    UPROPERTY(BlueprintReadOnly, Category="Cover|Lattice") int32 ChestPiecesPerCluster = 3;   // O2 PLACEHOLDER

    // --- Piece dimensions ---------------------------------------------------
    UPROPERTY(BlueprintReadOnly, Category="Cover|Piece") float ChestHeightCm = 120.0f;   // O2 PLACEHOLDER
    UPROPERTY(BlueprintReadOnly, Category="Cover|Piece") float FullHeightCm = 400.0f;   // O2 PLACEHOLDER
    UPROPERTY(BlueprintReadOnly, Category="Cover|Piece") float ChestHalfLengthCm = 150.0f;   // O2 PLACEHOLDER
    UPROPERTY(BlueprintReadOnly, Category="Cover|Piece") float ChestHalfDepthCm = 60.0f;   // O2 PLACEHOLDER
    UPROPERTY(BlueprintReadOnly, Category="Cover|Piece") float FullHalfExtentCm = 150.0f;   // O2 PLACEHOLDER

    // --- The contested band -------------------------------------------------
    // The stretch of field the lattice fills. Near edge is the target range's
    // firing line, far edge is past the elite arena; laterally it stops short of
    // the sniper lane (-6820) and the wall-ride corridor (+6820) so neither
    // movement lane is touched.
    UPROPERTY(BlueprintReadOnly, Category="Cover|Band") float BandNearCm = 4000.0f;   // O2 PLACEHOLDER
    UPROPERTY(BlueprintReadOnly, Category="Cover|Band") float BandFarCm = 19000.0f;   // O2 PLACEHOLDER
    UPROPERTY(BlueprintReadOnly, Category="Cover|Band") float BandHalfWidthCm = 5800.0f;   // O2 PLACEHOLDER

    // --- THE INSTRUMENT CORRIDOR -------------------------------------------
    // The one piece of ground that is NOT allowed to acquire cover clusters,
    // because the gym measures itself down it: the target range's firing line
    // and its four dummies (laterals -850 .. +350), and the player's line to the
    // standing encounter on arrival. A cluster centre inside it would put a
    // 400 cm slab across the TTK sampler's shot.
    UPROPERTY(BlueprintReadOnly, Category="Cover|Corridor") float CorridorNearCm = 4000.0f;   // O2 PLACEHOLDER
    UPROPERTY(BlueprintReadOnly, Category="Cover|Corridor") float CorridorFarCm = 10500.0f;   // O2 PLACEHOLDER
    UPROPERTY(BlueprintReadOnly, Category="Cover|Corridor") float CorridorHalfWidthCm = 900.0f;   // O2 PLACEHOLDER
    // Chest-high SHOULDERS flanking the corridor. They are what stops the
    // corridor being an exposed crossing (Level-Design §4) without ever standing
    // in the sampler's line: they sit outside the widest dummy lateral, they are
    // 120 cm, and there is no full-height cover anywhere in the corridor. This
    // is also the LATTICE's holding ground — a long open sightline with cover on
    // both shoulders is exactly the geometry §2.2 asks for.
    UPROPERTY(BlueprintReadOnly, Category="Cover|Corridor") float CorridorShoulderOffsetCm = 1100.0f;   // O2 PLACEHOLDER
    UPROPERTY(BlueprintReadOnly, Category="Cover|Corridor") float CorridorShoulderPitchCm = 1700.0f;   // O2 PLACEHOLDER

    // --- The combat pockets -------------------------------------------------
    // Centres, in field coordinates, of the pockets that get their own authored
    // cluster: four chest-high on an inner ring, one full-height pillar off
    // centre, and a twelve-piece chest ring further out. The outer ring is what
    // makes the exclusion below safe -- without it the annulus between a
    // pocket's own cover and the first lattice cluster is ground more than a
    // cover pitch from anything.
    UPROPERTY(BlueprintReadOnly, Category="Cover|Pocket") TArray<FVector2D> PocketCentres;
    // Pockets that sit ON the instrument corridor and therefore CANNOT have a
    // cluster: a ring of blocks at 850 cm would stand across the 45 m target
    // dummy and across the player's line in on arrival. They get two flanking
    // full-height pieces instead, outside the widest dummy lateral.
    UPROPERTY(BlueprintReadOnly, Category="Cover|Pocket") TArray<FVector2D> CorridorPocketCentres;
    // DERIVED: a lattice cluster's own line break, at ClusterRingRadius inside
    // this circle, must still leave a dash lane to the pocket's pillar.
    //   R >= PocketPillarRadius + DashCorridorWidth + 2 x FullHalf + ClusterRing
    //     =  900 + 1600 + 300 + 700 = 3500, rounded to 3600.
    UPROPERTY(BlueprintReadOnly, Category="Cover|Pocket") float PocketExclusionRadiusCm = 3600.0f;   // O2 PLACEHOLDER
    // Half a cover pitch: a player crossing the pocket always has the next piece
    // inside one telegraph-plus-flight window, and so does the enemy.
    UPROPERTY(BlueprintReadOnly, Category="Cover|Pocket") float PocketInnerRingRadiusCm = 850.0f;   // O2 PLACEHOLDER
    UPROPERTY(BlueprintReadOnly, Category="Cover|Pocket") float PocketOuterRingRadiusCm = 2600.0f;   // O2 PLACEHOLDER
    // The pillar sits on the -Forward bearing, mid-way between two ring blocks.
    // It used to sit at bearing 200, which put it 82 cm from one of them -- a
    // gap the SEVERED DRUDGE's 120 cm body cannot pass through. At 180 the same
    // gap is 371 cm.
    UPROPERTY(BlueprintReadOnly, Category="Cover|Pocket") float PocketPillarRadiusCm = 900.0f;   // O2 PLACEHOLDER
    UPROPERTY(BlueprintReadOnly, Category="Cover|Pocket") float PocketPillarHeightCm = 500.0f;   // O2 PLACEHOLDER
    // Lateral standoff of the encounter pocket's two flank line breaks. Outside
    // the shoulder line (1100) and inside the cover pitch (1700), so the
    // Skirmisher at the standing encounter has a real line to break and the
    // Warden has something a player can decide to go around.
    UPROPERTY(BlueprintReadOnly, Category="Cover|Pocket") float EncounterFlankOffsetCm = 1600.0f;   // O2 PLACEHOLDER

    // The widest enemy body in the project: the SEVERED DRUDGE's overridden
    // capsule is 120 cm across against the standard 90. Any gap narrower than
    // this is a gap it cannot path through, which is a layout bug that only
    // shows up in play.
    UPROPERTY(BlueprintReadOnly, Category="Cover|Clearance") float WidestEnemyBodyCm = 120.0f;

    // --- The elite arena (Encounter-Design §3.3, reused by §4.4) ------------
    // "Two pillars: full-height, break Lattice sight lines, and are the only
    // hard cover. Placed off-centre so a single pillar never covers both
    // galleries." The centre stays open, which is §3.3's "where the boss wants
    // you". The chest ring is inside the boss's ±1700 alcoves and ±1900
    // galleries, so no order can be given into geometry.
    UPROPERTY(BlueprintReadOnly, Category="Cover|Arena") bool bBuildArenaCover = true;
    UPROPERTY(BlueprintReadOnly, Category="Cover|Arena") float ArenaPillarForwardOffsetCm = 800.0f;   // O2 PLACEHOLDER
    UPROPERTY(BlueprintReadOnly, Category="Cover|Arena") float ArenaPillarLateralOffsetCm = 900.0f;   // O2 PLACEHOLDER
    UPROPERTY(BlueprintReadOnly, Category="Cover|Arena") float ArenaPillarHeightCm = 500.0f;   // O2 PLACEHOLDER
    // Four pieces, on the axes. An offset six-piece ring put a block 158 cm from
    // a pillar, which the Drudge's 120 cm body barely clears.
    UPROPERTY(BlueprintReadOnly, Category="Cover|Arena") float ArenaChestRingRadiusCm = 1400.0f;   // O2 PLACEHOLDER

    // --- Exclusions ---------------------------------------------------------
    // The jump-gap run. Three lanes across one trench; a cover block on a
    // landing platform turns an arithmetic test into a coin flip.
    UPROPERTY(BlueprintReadOnly, Category="Cover|Exclusion") float JumpRunNearCm = 10700.0f;
    UPROPERTY(BlueprintReadOnly, Category="Cover|Exclusion") float JumpRunFarCm = 16000.0f;
    UPROPERTY(BlueprintReadOnly, Category="Cover|Exclusion") float JumpRunHalfWidthCm = 4160.0f;
    // The sniper lane and the wall-ride corridor, both movement instruments.
    UPROPERTY(BlueprintReadOnly, Category="Cover|Exclusion") float SniperLaneRightCm = -6820.0f;
    UPROPERTY(BlueprintReadOnly, Category="Cover|Exclusion") float SniperLaneHalfWidthCm = 800.0f;
    UPROPERTY(BlueprintReadOnly, Category="Cover|Exclusion") float WallLaneRightCm = 6820.0f;
    UPROPERTY(BlueprintReadOnly, Category="Cover|Exclusion") float WallLaneHalfWidthCm = 400.0f;

    // --- The sniper lane's own hard cover ----------------------------------
    // Kept exactly where it was: one full-height piece with RangedSightlineDepth
    // of clear ground behind it, which is the minimum a LATTICE needs to use its
    // whole band instead of backing into a kerb. It is the reason wave mode
    // finds an anchor when the playtest is standing on the lane.
    UPROPERTY(BlueprintReadOnly, Category="Cover|Lane") bool bBuildLaneCover = true;
    UPROPERTY(BlueprintReadOnly, Category="Cover|Lane") float LaneCoverForwardCm = 5900.0f;
    UPROPERTY(BlueprintReadOnly, Category="Cover|Lane") float LaneCoverRightCm = -6320.0f;

    // Yaw jitter only. The lattice positions are deterministic maths; the
    // rotations are seeded so the field does not read as a printed grid.
    UPROPERTY(BlueprintReadOnly, Category="Cover|Seed") int32 Seed = 20260814;

    // The band the cover-to-open ratio is allowed to sit in, as a fraction of
    // the band's ground area occupied by cover footprint. O2 PLACEHOLDER: the
    // load-bearing checks are the gap and the lane width, both of which are
    // forced by the geometry; this is a coarse sanity rail that catches "someone
    // doubled the piece size and closed every lane" and "someone deleted the
    // lattice and left a shooting gallery".
    UPROPERTY(BlueprintReadOnly, Category="Cover|Ratio") float MinimumCoverFraction = 0.005f;   // O2 PLACEHOLDER
    UPROPERTY(BlueprintReadOnly, Category="Cover|Ratio") float MaximumCoverFraction = 0.050f;   // O2 PLACEHOLDER

    // How far a point may be from a LINE BREAK — a full-height piece. This is
    // deliberately looser than CoverPitchMax and the looseness is the design:
    // chest-high cover is answered by crouching, full-height cover is answered
    // by choosing a different route, and a field with a route decision every
    // 17 m is a maze. Encounter-Design §3.3 gives an entire 4000 x 4000 boss
    // arena exactly TWO full-height pieces. Two cover pitches is the bound the
    // built field actually meets, measured rather than hoped for.
    UPROPERTY(BlueprintReadOnly, Category="Cover|Ratio") float MaximumLineBreakGapCm = 3400.0f;   // O2 PLACEHOLDER

    float BandAreaCm2() const { return FMath::Max(BandFarCm - BandNearCm, 0.0f) * FMath::Max(2.0f * BandHalfWidthCm, 0.0f); }
    // The circumradius of a triangular lattice of this pitch: the furthest a
    // point in the band can be from the NEAREST cluster centre.
    float LatticeCircumradiusCm() const { return ClusterPitchCm / FMath::Sqrt(3.0f); }
};

// One recorded anchor in WORLD space. The runtime half: the layout above says
// where cover goes, this says where it went.
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerCoverAnchor
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Cover") FVector Location = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly, Category="Cover") float HeightCm = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category="Cover") EBreakerCoverClass Class = EBreakerCoverClass::ChestHigh;
};

// THE COVER REGISTRY. Every piece of hard cover the field built, with its class
// and its height, so the placement code can AIM at cover instead of guessing.
//
// It replaces a bare TArray<FVector> that recorded position and nothing else,
// which meant `SpawnSkirmisherNearCover` could not tell a 500 cm pillar from a
// 110 cm block — and the difference between those two is the difference between
// breaking line of sight and standing behind a footstool.
//
// Deliberately world-free: it holds FVectors and answers questions about them.
// Nothing in here needs a UWorld, so all of it is testable.
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerCoverRegistry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Cover") TArray<FBreakerCoverAnchor> Anchors;

    void Reset() { Anchors.Reset(); }
    int32 Num() const { return Anchors.Num(); }

    void Add(const FVector& Location, EBreakerCoverClass Class, float HeightCm)
    {
        FBreakerCoverAnchor Anchor;
        Anchor.Location = Location;
        Anchor.Class = Class;
        Anchor.HeightCm = HeightCm;
        Anchors.Add(Anchor);
    }

    int32 CountOfClass(EBreakerCoverClass Class) const
    {
        int32 Count = 0;
        for (const FBreakerCoverAnchor& Anchor : Anchors) { if (Anchor.Class == Class) ++Count; }
        return Count;
    }

    // Nearest anchor to Around within MaxDistance. Measured in 2D on purpose: a
    // full-height piece is registered at its mid-height, and comparing that
    // against a chest block registered at 60 in 3D would cost the better piece
    // 200 cm of spurious distance and make the field prefer the worse cover.
    bool FindNearest(const FVector& Around, float MaxDistance, FBreakerCoverAnchor& OutAnchor) const
    {
        float BestSquared = MaxDistance * MaxDistance;
        bool bFound = false;
        for (const FBreakerCoverAnchor& Anchor : Anchors)
        {
            const float DistanceSquared = FVector::DistSquared2D(Anchor.Location, Around);
            if (DistanceSquared > BestSquared) continue;
            BestSquared = DistanceSquared;
            OutAnchor = Anchor;
            bFound = true;
        }
        return bFound;
    }

    // Nearest anchor of a REQUIRED class. The Skirmisher wants a line break and
    // will settle for a parapet; a Lattice holding a band wants the parapet.
    bool FindNearestOfClass(const FVector& Around, float MaxDistance, EBreakerCoverClass Class,
        FBreakerCoverAnchor& OutAnchor) const
    {
        float BestSquared = MaxDistance * MaxDistance;
        bool bFound = false;
        for (const FBreakerCoverAnchor& Anchor : Anchors)
        {
            if (Anchor.Class != Class) continue;
            const float DistanceSquared = FVector::DistSquared2D(Anchor.Location, Around);
            if (DistanceSquared > BestSquared) continue;
            BestSquared = DistanceSquared;
            OutAnchor = Anchor;
            bFound = true;
        }
        return bFound;
    }
};

// ONE YARD'S MEASURED FIELD: its name, the grammar it answers to, and its
// pieces IN ITS OWN FRAME. The frame is what makes this per-yard rather than
// per-zone — FBreakerCoverPiece is field-space by construction ("nothing here
// is world space, which is the whole reason it can be tested"), so two yards
// are two coordinate systems and not two regions of one.
//
// WHO BUILDS A NON-ENTRY YARD'S FRAME IS NOT DECIDED. The entry yard's frame
// is derived from the player start and the rift marker; a zone has exactly one
// player start, so a second yard has no anchor by that rule. That is a design
// question in the lane's report, not something to invent here — which is why
// this struct takes pieces already in a frame rather than deriving one.
struct FBreakerZoneField
{
    // NAME_None is the entry yard, matching the marker contract.
    FName Yard = NAME_None;
    FBreakerCoverFieldParams Params;
    TArray<FBreakerCoverPiece> Pieces;
};

UCLASS()
class RIORSEDGE_API UBreakerCoverLayoutLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // THE WHOLE LAYOUT. Deterministic: same params, same field, every time —
    // which is what makes an F1 reset and a screenshot from two sessions ago
    // describe the same ground.
    UFUNCTION(BlueprintPure, Category="Cover")
    static TArray<FBreakerCoverPiece> BuildCoverField(const FBreakerCoverFieldParams& Params);

    // True if a cluster CENTRE may not be placed here. Takes the cluster's own
    // extent, so the test is about where its PIECES would land rather than about
    // its centre point — an exclusion that only rejects centres lets a 400 cm
    // slab hang over the thing it was protecting.
    UFUNCTION(BlueprintPure, Category="Cover")
    static bool IsClusterCentreExcluded(const FBreakerCoverFieldParams& Params, float Forward, float Right, float ExtentCm);

    // True if a POINT is inside ground the layout deliberately keeps clear —
    // the jump-gap run, the movement lanes, the arena marker ring, the safe
    // ring. Coverage is not measured inside these: they are open on purpose.
    UFUNCTION(BlueprintPure, Category="Cover")
    static bool IsPointInClearedGround(const FBreakerCoverFieldParams& Params, float Forward, float Right);

    // Distance from a field point to the nearest piece, or a large sentinel if
    // there is none. The Skirmisher's whole existence is this number being under
    // CoverPitchMax wherever it is spawned.
    UFUNCTION(BlueprintPure, Category="Cover")
    static float DistanceToNearestCover(const TArray<FBreakerCoverPiece>& Pieces, float Forward, float Right);

    // Same, restricted to a class.
    UFUNCTION(BlueprintPure, Category="Cover")
    static float DistanceToNearestCoverOfClass(const TArray<FBreakerCoverPiece>& Pieces, float Forward, float Right,
        EBreakerCoverClass Class);

    // THE EXPOSED-CROSSING MEASUREMENT. Samples the band on a grid and returns
    // the worst distance to cover over every sampled point that is not
    // deliberately cleared ground. Level-Design §4 calls anything over 1837 cm
    // an exposed crossing and forbids it; CoverPitchMax 1700 is the authored
    // margin. Sampled rather than solved because the band has holes in it, and a
    // sampled worst case that is checked every build beats an exact one that is
    // asserted in a comment.
    UFUNCTION(BlueprintPure, Category="Cover")
    static float LargestUncoveredGap(const TArray<FBreakerCoverPiece>& Pieces, const FBreakerCoverFieldParams& Params,
        int32 SamplesAcross = 96);

    // THE LINE-BREAK MEASUREMENT. Same sweep, restricted to FULL-HEIGHT pieces
    // and skipping the instrument corridor — which carries no full-height cover
    // BY DESIGN, because a 400 cm slab in it would stand across the TTK
    // sampler's shot and across the player's line to the encounter on arrival.
    UFUNCTION(BlueprintPure, Category="Cover")
    static float LargestGapToLineBreak(const TArray<FBreakerCoverPiece>& Pieces, const FBreakerCoverFieldParams& Params,
        int32 SamplesAcross = 96);

    // THE MAZE MEASUREMENT, AND IT IS FULL-HEIGHT ONLY. Narrowest clear gap
    // between two FULL-HEIGHT pieces belonging to DIFFERENT clusters. Within a
    // cluster the pieces are close on purpose; it is the ground BETWEEN
    // clusters that has to stay dashable, and this is the number that says
    // whether it does.
    //
    // THE CLASS RESTRICTION IS THE WHOLE RULE, and saying "lane" without
    // saying "full-height" has already misled one reader. Chest-high cover is
    // 120 cm, under MantleStepHeight 145: a player goes OVER it, so it does
    // not close a lane and does not appear here.
    //
    // THIS IS NOT THE ONLY LANE RULE, which is the half that got missed. The
    // ground a player dashes down is held by the CORRIDOR rejection in
    // IsLayoutLegal — no piece of any class may stand within
    // CorridorHalfWidth of the centreline over the corridor span — and that
    // is what keeps chest-flanked ground open (O132). Two rules, two
    // measurements, and the readout now prints both rather than one figure
    // under a word that fits either.
    UFUNCTION(BlueprintPure, Category="Cover")
    static float MinimumOpenLaneWidth(const TArray<FBreakerCoverPiece>& Pieces, const FBreakerCoverFieldParams& Params);

    // THE CORRIDOR MARGIN. Smallest |Right| among pieces standing in the
    // corridor's forward span — how much room the closest piece left the
    // centreline. IsLayoutLegal already REJECTS anything inside
    // CorridorHalfWidth; this reports the margin so it is visible while it is
    // healthy, not only in the failure message. AN OFFSET, NOT A WIDTH: the
    // placement rule is written about centres, so this is measured about
    // centres and compared against that same number.
    UFUNCTION(BlueprintPure, Category="Cover")
    static float NearestPieceToCorridorCentre(const TArray<FBreakerCoverPiece>& Pieces, const FBreakerCoverFieldParams& Params);

    // Narrowest gap between ANY two pieces, chest-high included. Negative means
    // interpenetrating geometry; under WidestEnemyBodyCm means a gap the widest
    // enemy in the project cannot path through.
    UFUNCTION(BlueprintPure, Category="Cover")
    static float MinimumPieceClearance(const TArray<FBreakerCoverPiece>& Pieces);

    // Cover footprint as a fraction of the band's ground area.
    UFUNCTION(BlueprintPure, Category="Cover")
    static float CoverAreaFraction(const TArray<FBreakerCoverPiece>& Pieces, const FBreakerCoverFieldParams& Params);

    // Every layout rule in one place, with the broken rule named. The precedent
    // is UBreakerWaveBudgetLibrary::IsCompositionLegal and the reason is the
    // same: a rule that is only in a comment is a rule that has already been
    // broken once.
    UFUNCTION(BlueprintPure, Category="Cover")
    static bool IsLayoutLegal(const TArray<FBreakerCoverPiece>& Pieces, const FBreakerCoverFieldParams& Params,
        FString& OutReason);

    // THE ZONE RULE (Q3): a zone is legal when EVERY YARD is legal in its own
    // frame. IsLayoutLegal validates one open combat field — one band, one
    // corridor, one safe circle — and every rule in it is correct about that
    // and has no concept of a room boundary. Handed a rectangle containing
    // several yards it measures the dead ground between them, finds no cover
    // there, and reports an exposed crossing over ground no player can stand
    // on. So the fix is not a wider band; it is one field per yard.
    //
    // AT ONE YARD THIS IS EXACTLY IsLayoutLegal, and that is deliberate: the
    // shipped yard's verdict does not move, and what changes is that the zone
    // now has a level above the field for a second yard to be added to.
    //
    // CONNECTIONS ARE NOT CHECKED HERE. Q3 ruled them a distinct kind of
    // space with their own rule, asking about length and sightline rather than
    // cover pitch, and their magnitudes are not authored yet — so this
    // function deliberately says nothing about the ground BETWEEN yards rather
    // than pretending the field rules cover it. A zone becomes fully legal
    // when this passes AND the connection rule passes; only the first half
    // exists.
    static bool IsZoneLegal(const TArray<struct FBreakerZoneField>& Yards, FString& OutReason);

    UFUNCTION(BlueprintPure, Category="Cover")
    static FString DescribeCoverField(const TArray<FBreakerCoverPiece>& Pieces, const FBreakerCoverFieldParams& Params);

    UFUNCTION(BlueprintPure, Category="Cover")
    static int32 CountOfClass(const TArray<FBreakerCoverPiece>& Pieces, EBreakerCoverClass Class);
};
