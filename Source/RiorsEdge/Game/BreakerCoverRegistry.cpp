#include "Game/BreakerCoverRegistry.h"

#include "Math/RandomStream.h"

namespace
{
    float Distance2D(float AFwd, float ARgt, float BFwd, float BRgt)
    {
        return FMath::Sqrt(FMath::Square(AFwd - BFwd) + FMath::Square(ARgt - BRgt));
    }

    bool InRect(float Fwd, float Rgt, float Near, float Far, float RgtCentre, float HalfWidth, float Grow)
    {
        return Fwd > Near - Grow && Fwd < Far + Grow
            && Rgt > RgtCentre - HalfWidth - Grow && Rgt < RgtCentre + HalfWidth + Grow;
    }

    void AppendChest(TArray<FBreakerCoverPiece>& Out, const FBreakerCoverFieldParams& Params,
        float Fwd, float Rgt, float Yaw, int32 ClusterIndex)
    {
        FBreakerCoverPiece Piece;
        Piece.Forward = Fwd;
        Piece.Right = Rgt;
        Piece.Class = EBreakerCoverClass::ChestHigh;
        Piece.HeightCm = Params.ChestHeightCm;
        Piece.HalfLengthCm = Params.ChestHalfLengthCm;
        Piece.HalfDepthCm = Params.ChestHalfDepthCm;
        Piece.YawDegrees = Yaw;
        Piece.ClusterIndex = ClusterIndex;
        Out.Add(Piece);
    }

    void AppendPillar(TArray<FBreakerCoverPiece>& Out, const FBreakerCoverFieldParams& Params,
        float Fwd, float Rgt, float HeightCm, int32 ClusterIndex)
    {
        FBreakerCoverPiece Piece;
        Piece.Forward = Fwd;
        Piece.Right = Rgt;
        Piece.Class = EBreakerCoverClass::FullHeight;
        Piece.HeightCm = HeightCm;
        Piece.HalfLengthCm = Params.FullHalfExtentCm;
        Piece.HalfDepthCm = Params.FullHalfExtentCm;
        Piece.YawDegrees = 0.0f;
        Piece.ClusterIndex = ClusterIndex;
        Out.Add(Piece);
    }

    // Bearing of the slot a lattice cluster puts its ONE full-height piece in.
    // Needed before the cluster is built, so the separation guard can ask "would
    // this cluster's line break form a sub-dash gate with an existing one".
    float ClusterFullSlotBearing(const FBreakerCoverFieldParams& Params, float BaseYaw)
    {
        const int32 Slots = FMath::Max(Params.ChestPiecesPerCluster, 0) + 1;
        return BaseYaw + 360.0f * (Slots - 1) / Slots;
    }

    // One cluster's worth of pieces on a ring around a centre: N chest-high and
    // one full-height, evenly spaced, the full-height piece last so a caller can
    // find "the line break" without a search.
    void AppendCluster(TArray<FBreakerCoverPiece>& Out, const FBreakerCoverFieldParams& Params,
        float CentreFwd, float CentreRgt, float BaseYaw, int32 ClusterIndex)
    {
        const int32 Slots = FMath::Max(Params.ChestPiecesPerCluster, 0) + 1;
        for (int32 Slot = 0; Slot < Slots; ++Slot)
        {
            const float Bearing = BaseYaw + 360.0f * Slot / Slots;
            const float Radians = FMath::DegreesToRadians(Bearing);
            const float Fwd = CentreFwd + FMath::Cos(Radians) * Params.ClusterRingRadiusCm;
            const float Rgt = CentreRgt + FMath::Sin(Radians) * Params.ClusterRingRadiusCm;
            if (Slot == Slots - 1)
            {
                AppendPillar(Out, Params, Fwd, Rgt, Params.FullHeightCm, ClusterIndex);
            }
            else
            {
                // Tangential: the long face presents to whoever approaches from
                // outside, which is what makes a cluster read as a broken
                // enclosure rather than as four bricks.
                AppendChest(Out, Params, Fwd, Rgt, Bearing + 90.0f, ClusterIndex);
            }
        }
    }

    // Where the arena's two full-height pillars stand, as a radius from the
    // arena centre.
    float ArenaPillarRadius(const FBreakerCoverFieldParams& Params)
    {
        return FMath::Sqrt(FMath::Square(Params.ArenaPillarForwardOffsetCm)
            + FMath::Square(Params.ArenaPillarLateralOffsetCm));
    }

    // How far a lattice or fill cluster's centre must stay from the elite
    // arena's centre. DERIVED: the worst case is a lattice cluster whose own
    // full-height piece lies on the ring's inner edge, collinear with an arena
    // pillar. That pair must still leave a dash lane, so
    //   R >= PillarRadius + DashCorridorWidth + 2 x FullHalfExtent + ClusterRing.
    float ArenaExclusionRadius(const FBreakerCoverFieldParams& Params)
    {
        return ArenaPillarRadius(Params) + Params.DashCorridorWidthCm
            + 2.0f * Params.FullHalfExtentCm + Params.ClusterRingRadiusCm + 100.0f;
    }

    // The arena's cover shells, outside the marker ring. Approaching the arena
    // was a walk across a car park; §5.2 makes the approach a beat, and a beat
    // needs something to move between. TWO shells, because one leaves the
    // annulus between it and the lattice further from cover than G23 allows.
    float ArenaMidShellRadius(const FBreakerCoverFieldParams& Params)
    {
        return Params.CombatPocketRadiusCm + Params.CombatPocketRadiusCm * 0.225f;
    }
    float ArenaOuterShellRadius(const FBreakerCoverFieldParams& Params)
    {
        return ArenaExclusionRadius(Params) - 350.0f;
    }

    bool IsOnAuthoredMovementLane(const FBreakerCoverFieldParams& Params, float Forward, float Right)
    {
        return InRect(Forward, Right, Params.BandNearCm, Params.BandFarCm,
                Params.SniperLaneRightCm, Params.SniperLaneHalfWidthCm, 0.0f)
            || InRect(Forward, Right, Params.BandNearCm, Params.BandFarCm,
                Params.WallLaneRightCm, Params.WallLaneHalfWidthCm, 0.0f);
    }

    // Ground an AUTHORED piece may not stand on. The lattice and the two
    // measured passes go through IsClusterCentreExcluded, which is about where a
    // CLUSTER may be placed; the authored sets — the pocket rings, the arena's
    // §3.3 furniture, the encounter's flank breaks — are placed by position and
    // would otherwise walk straight into the instruments.
    //
    // This is not hypothetical. The elite arena at 17000 and the jump-gap run's
    // third landing genuinely overlap in the shipped field, the second pocket's
    // outer ring reaches into the wall-ride corridor at 6820, and the third
    // pocket's pillar lands inside the sniper lane. All three are pre-existing
    // spatial collisions in the field this pass inherited; the guard is how the
    // cover stops making them worse.
    bool IsAuthoredPieceBlocked(const FBreakerCoverFieldParams& Params, float Forward, float Right, float ExtentCm)
    {
        if (Distance2D(Forward, Right, 0.0f, 0.0f) < Params.SafeZoneRadiusCm + ExtentCm) return true;
        if (InRect(Forward, Right, Params.JumpRunNearCm, Params.JumpRunFarCm, 0.0f,
            Params.JumpRunHalfWidthCm, ExtentCm)) return true;
        if (InRect(Forward, Right, Params.BandNearCm, Params.BandFarCm,
            Params.SniperLaneRightCm, Params.SniperLaneHalfWidthCm, ExtentCm)) return true;
        if (InRect(Forward, Right, Params.BandNearCm, Params.BandFarCm,
            Params.WallLaneRightCm, Params.WallLaneHalfWidthCm, ExtentCm)) return true;
        return false;
    }

    // Minimum centre-to-centre separation between two line breaks that still
    // leaves a dash lane between them.
    float LineBreakSeparation(const FBreakerCoverFieldParams& Params)
    {
        return Params.DashCorridorWidthCm + 2.0f * Params.FullHalfExtentCm + 100.0f;
    }
}

// TWO GUARDS, TWO PLACEMENT KINDS, AND THEY ARE NOT INTERCHANGEABLE.
// IsAuthoredPieceBlocked (above) refuses an individual authored piece that
// would land inside an instrument. This one refuses a CLUSTER CENTRE, and its
// exclusions carry the cluster's own radius, so it is deliberately the harsher
// of the two. Applying either to the other's callers is not a tightening — it
// pushes the whole lattice off the near half of the field, because a cluster
// radius measured against a per-piece rule excludes ground that has nothing
// wrong with it. That regression has happened once already.
bool UBreakerCoverLayoutLibrary::IsClusterCentreExcluded(const FBreakerCoverFieldParams& Params,
    float Forward, float Right, float ExtentCm)
{
    // Outside the band entirely.
    if (Forward < Params.BandNearCm || Forward > Params.BandFarCm) return true;
    if (FMath::Abs(Right) > Params.BandHalfWidthCm) return true;

    // THE SAFE RING. Enemies never enter it and the session starts in it;
    // geometry inside it is geometry in the one room that has to stay a room.
    if (Distance2D(Forward, Right, 0.0f, 0.0f) < Params.SafeZoneRadiusCm + ExtentCm) return true;

    // THE ELITE ARENA and its marker ring. The arena's own cover is authored to
    // Encounter-Design §3.3 and the lattice must not reach into it.
    if (Distance2D(Forward, Right, Params.ArenaDistanceCm, 0.0f) < ArenaExclusionRadius(Params) + ExtentCm) return true;

    // THE COMBAT POCKETS. Each carries its own authored cluster; the lattice
    // stands off far enough that a lattice line break and a pocket pillar never
    // form a gate narrower than a dash lane.
    for (const FVector2D& Pocket : Params.PocketCentres)
    {
        if (Distance2D(Forward, Right, Pocket.X, Pocket.Y) < Params.PocketExclusionRadiusCm + ExtentCm) return true;
    }

    // THE INSTRUMENT CORRIDOR. The target range's four dummies, the firing line
    // the TTK sampler is read from, and the player's line to the standing
    // encounter on arrival.
    //
    // The standoff is measured from the shoulder line's outer face, and the
    // caller's own extent is added below — adding the cluster ring here as well
    // would double-count it and push the whole lattice off the near half of the
    // field, which is exactly what the first draft of this did.
    const float CorridorStandoff = Params.CorridorShoulderOffsetCm + Params.ChestHalfLengthCm + 200.0f;
    if (Forward > Params.CorridorNearCm - ExtentCm && Forward < Params.CorridorFarCm + ExtentCm
        && FMath::Abs(Right) < CorridorStandoff + ExtentCm)
    {
        return true;
    }

    // THE JUMP-GAP RUN. Three lanes across one trench, sized so the verb each
    // needs is the only verb that clears it. A cover block on a landing platform
    // turns an arithmetic test into a coin flip.
    if (InRect(Forward, Right, Params.JumpRunNearCm, Params.JumpRunFarCm, 0.0f, Params.JumpRunHalfWidthCm, ExtentCm))
    {
        return true;
    }

    // THE MOVEMENT LANES. The sniper sightline and the wall-ride corridor are
    // instruments too, and both are measured in a straight line.
    if (InRect(Forward, Right, Params.BandNearCm, Params.BandFarCm,
        Params.SniperLaneRightCm, Params.SniperLaneHalfWidthCm, ExtentCm))
    {
        return true;
    }
    if (InRect(Forward, Right, Params.BandNearCm, Params.BandFarCm,
        Params.WallLaneRightCm, Params.WallLaneHalfWidthCm, ExtentCm))
    {
        return true;
    }
    return false;
}

bool UBreakerCoverLayoutLibrary::IsPointInClearedGround(const FBreakerCoverFieldParams& Params,
    float Forward, float Right)
{
    // Ground that is open BY DESIGN, and therefore ground whose distance to
    // cover is not a defect. The instrument corridor is deliberately NOT in this
    // list: it is fought over, so it has to be covered, and its chest-high
    // shoulders are what cover it.
    if (Distance2D(Forward, Right, 0.0f, 0.0f) < Params.SafeZoneRadiusCm) return true;
    if (InRect(Forward, Right, Params.JumpRunNearCm, Params.JumpRunFarCm, 0.0f, Params.JumpRunHalfWidthCm, 0.0f)) return true;
    return IsOnAuthoredMovementLane(Params, Forward, Right);
}

TArray<FBreakerCoverPiece> UBreakerCoverLayoutLibrary::BuildCoverField(const FBreakerCoverFieldParams& Params)
{
    TArray<FBreakerCoverPiece> Pieces;
    if (Params.ClusterPitchCm <= 1.0f) return Pieces;

    FRandomStream Stream(Params.Seed);
    int32 ClusterIndex = 0;

    // --- 1. THE COMBAT POCKETS ---------------------------------------------
    // Each pocket keeps the cluster the shipped field gave it — four blocks on a
    // half-cover-pitch ring and one pillar off centre — with two changes. The
    // heights are now the two authored classes rather than whatever the spawner
    // happened to scale to, and the pillar moved from bearing 200 to 180: at 200
    // it stood 82 cm from a ring block, which is a gap the SEVERED DRUDGE's
    // 120 cm body cannot pass through. At 180 the same gap is 371 cm.
    //
    // The OUTER RING is new, and it is what makes the pocket exclusion above
    // safe: without it the annulus between the pocket's own cover and the first
    // lattice cluster is ground more than a cover pitch from anything.
    for (const FVector2D& Pocket : Params.PocketCentres)
    {
        const int32 PocketCluster = ClusterIndex++;
        for (int32 Index = 0; Index < 4; ++Index)
        {
            const float Bearing = 90.0f * Index + 45.0f;
            const float Radians = FMath::DegreesToRadians(Bearing);
            const float Fwd = Pocket.X + FMath::Cos(Radians) * Params.PocketInnerRingRadiusCm;
            const float Rgt = Pocket.Y + FMath::Sin(Radians) * Params.PocketInnerRingRadiusCm;
            if (IsAuthoredPieceBlocked(Params, Fwd, Rgt, Params.ChestHalfLengthCm)) continue;
            AppendChest(Pieces, Params, Fwd, Rgt, Bearing + 90.0f, PocketCluster);
        }
        // The pillar wants a bearing MID-WAY between two ring blocks, and it
        // wants one that is not inside an instrument. 180 is the first choice
        // (it faces back down the field); the third pocket sits on the sniper
        // lane and takes 90 instead. Without the fallback that pocket silently
        // loses its only line break, which the line-break assertion catches as a
        // 3529 cm hole and nothing else would.
        static const float PillarBearings[] = { 180.0f, 90.0f, 270.0f, 0.0f };
        for (float Bearing : PillarBearings)
        {
            const float Radians = FMath::DegreesToRadians(Bearing);
            const float Fwd = Pocket.X + FMath::Cos(Radians) * Params.PocketPillarRadiusCm;
            const float Rgt = Pocket.Y + FMath::Sin(Radians) * Params.PocketPillarRadiusCm;
            if (IsAuthoredPieceBlocked(Params, Fwd, Rgt, Params.FullHalfExtentCm)) continue;
            AppendPillar(Pieces, Params, Fwd, Rgt, Params.PocketPillarHeightCm, PocketCluster);
            break;
        }

        const int32 OuterCluster = ClusterIndex++;
        for (int32 Index = 0; Index < 12; ++Index)
        {
            const float Bearing = 30.0f * Index;
            const float Radians = FMath::DegreesToRadians(Bearing);
            const float Fwd = Pocket.X + FMath::Cos(Radians) * Params.PocketOuterRingRadiusCm;
            const float Rgt = Pocket.Y + FMath::Sin(Radians) * Params.PocketOuterRingRadiusCm;
            if (IsAuthoredPieceBlocked(Params, Fwd, Rgt, Params.ChestHalfLengthCm)) continue;
            AppendChest(Pieces, Params, Fwd, Rgt, Bearing + 90.0f, OuterCluster);
        }
    }

    // --- 2. THE ENCOUNTER POCKET'S FLANK BREAKS -----------------------------
    // The standing encounter sits ON the instrument corridor, so it cannot have
    // a pocket cluster: a ring of blocks at 850 cm would stand across the 45 m
    // target dummy and across the player's line in on arrival. What it gets
    // instead is two full-height pieces on its flanks, outside the widest dummy
    // lateral and outside the shoulder line — the Skirmisher's line break and
    // the Warden's "go around me" both need something to go around, and the
    // corridor centre stays clear so the sampler and the arrival read do not.
    for (const FVector2D& Pocket : Params.CorridorPocketCentres)
    {
        for (int32 Side = 0; Side < 2; ++Side)
        {
            const float Rgt = Pocket.Y + (Side == 0 ? -1.0f : 1.0f) * Params.EncounterFlankOffsetCm;
            if (IsAuthoredPieceBlocked(Params, Pocket.X, Rgt, Params.FullHalfExtentCm)) continue;
            AppendPillar(Pieces, Params, Pocket.X, Rgt, Params.FullHeightCm, ClusterIndex++);
        }
    }

    // --- 3. THE LATTICE ----------------------------------------------------
    // Triangular, because a square lattice has a circumradius of P/sqrt(2)
    // (2404 cm at this pitch) against the triangular P/sqrt(3) (1963 cm) for the
    // same clusters per unit area. The extra 440 cm is the difference between
    // passing G23 and failing it.
    const float RowSpacing = Params.ClusterPitchCm * FMath::Sqrt(3.0f) * 0.5f;
    const int32 RowCount = FMath::Max(1, FMath::CeilToInt((Params.BandFarCm - Params.BandNearCm) / RowSpacing));
    const int32 ColumnCount = FMath::Max(1, FMath::CeilToInt((2.0f * Params.BandHalfWidthCm) / Params.ClusterPitchCm));
    const float ClusterExtent = Params.ClusterRingRadiusCm + Params.FullHalfExtentCm;
    const float Separation = LineBreakSeparation(Params);
    for (int32 Row = 0; Row <= RowCount; ++Row)
    {
        const float Fwd = Params.BandNearCm + Row * RowSpacing;
        // Alternate rows offset by half a pitch. That single line is what makes
        // the lattice triangular rather than square.
        const float RowOffset = (Row % 2 == 0) ? 0.0f : Params.ClusterPitchCm * 0.5f;
        for (int32 Column = 0; Column <= ColumnCount; ++Column)
        {
            const float Rgt = -Params.BandHalfWidthCm + RowOffset + Column * Params.ClusterPitchCm;
            // Drawn unconditionally so the seed stream does not depend on which
            // clusters survive: an exclusion moving by a metre must not reshuffle
            // every rotation in the field.
            const float BaseYaw = Stream.FRandRange(0.0f, 360.0f);
            if (IsClusterCentreExcluded(Params, Fwd, Rgt, ClusterExtent)) continue;
            // The cluster's own line break must not form a sub-dash gate with an
            // existing one. Checked at the piece, not at the centre, because the
            // piece is what the player has to walk around.
            const float FullRadians = FMath::DegreesToRadians(ClusterFullSlotBearing(Params, BaseYaw));
            if (DistanceToNearestCoverOfClass(Pieces,
                    Fwd + FMath::Cos(FullRadians) * Params.ClusterRingRadiusCm,
                    Rgt + FMath::Sin(FullRadians) * Params.ClusterRingRadiusCm,
                    EBreakerCoverClass::FullHeight) < Separation)
            {
                continue;
            }
            AppendCluster(Pieces, Params, Fwd, Rgt, BaseYaw, ClusterIndex++);
        }
    }

    // --- 4. THE INSTRUMENT CORRIDOR'S SHOULDERS ----------------------------
    // Chest-high only, outside the widest dummy lateral, left staggered against
    // right so a player crossing the corridor always has one within half a
    // shoulder pitch — worst case sqrt(1100^2 + 425^2) = 1179 cm, comfortably
    // inside G23. This is also the LATTICE archetype's holding ground: a long
    // open sightline with somewhere on each side to hold the 900-1900 band from,
    // and nothing full-height in it to break the sampler's shot.
    if (Params.CorridorShoulderPitchCm > 1.0f)
    {
        const int32 Shoulders = FMath::Max(1,
            FMath::CeilToInt((Params.CorridorFarCm - Params.CorridorNearCm) / Params.CorridorShoulderPitchCm));
        // Both sides share ONE cluster index each. The open-lane measurement is
        // between different clusters, and two shoulders on the same side are one
        // firing step apart on purpose.
        const int32 LeftCluster = ClusterIndex++;
        const int32 RightCluster = ClusterIndex++;
        for (int32 Index = 0; Index <= Shoulders; ++Index)
        {
            const float Fwd = Params.CorridorNearCm + Index * Params.CorridorShoulderPitchCm;
            if (Fwd > Params.CorridorFarCm) break;
            AppendChest(Pieces, Params, Fwd, -Params.CorridorShoulderOffsetCm,
                Stream.FRandRange(-14.0f, 14.0f), LeftCluster);
            AppendChest(Pieces, Params, Fwd + Params.CorridorShoulderPitchCm * 0.5f,
                Params.CorridorShoulderOffsetCm, Stream.FRandRange(-14.0f, 14.0f), RightCluster);
        }
    }

    // --- 5. THE ELITE ARENA (Encounter-Design §3.3, reused by §4.4) --------
    if (Params.bBuildArenaCover)
    {
        const int32 ArenaCluster = ClusterIndex++;
        // "Two pillars: full-height, break Lattice sight lines, and are the only
        // hard cover. Placed off-centre so a single pillar never covers both
        // galleries." Diagonally opposed, so the line between them lies along
        // neither gallery axis.
        for (int32 Side = 0; Side < 2; ++Side)
        {
            const float Sign = Side == 0 ? 1.0f : -1.0f;
            const float Fwd = Params.ArenaDistanceCm + Sign * Params.ArenaPillarForwardOffsetCm;
            const float Rgt = -Sign * Params.ArenaPillarLateralOffsetCm;
            if (IsAuthoredPieceBlocked(Params, Fwd, Rgt, Params.FullHalfExtentCm)) continue;
            // Different cluster indices on purpose: the gate BETWEEN the two
            // pillars is exactly the lane the boss fight is fought through, so
            // it has to be measured rather than excused as intra-cluster.
            AppendPillar(Pieces, Params, Fwd, Rgt, Params.ArenaPillarHeightCm, ClusterIndex++);
        }

        // Four chest-high pieces on the axes, OUTSIDE §3.3's open centre and
        // INSIDE the boss's ±1700 alcoves and ±1900 galleries, so no order the
        // Field Marshal gives can point into geometry. On the axes rather than
        // on a six-piece offset ring: the offset ring put a block 158 cm from a
        // pillar, which the Drudge's 120 cm body barely clears.
        for (int32 Index = 0; Index < 4; ++Index)
        {
            const float Bearing = 90.0f * Index;
            const float Radians = FMath::DegreesToRadians(Bearing);
            const float Fwd = Params.ArenaDistanceCm + FMath::Cos(Radians) * Params.ArenaChestRingRadiusCm;
            const float Rgt = FMath::Sin(Radians) * Params.ArenaChestRingRadiusCm;
            if (IsAuthoredPieceBlocked(Params, Fwd, Rgt, Params.ChestHalfLengthCm)) continue;
            AppendChest(Pieces, Params, Fwd, Rgt, Bearing + 90.0f, ArenaCluster);
        }
        // Two shells outside the marker ring, piece counts rising with radius so
        // the angular gap does not.
        const int32 ShellCluster = ClusterIndex++;
        const float ShellRadii[] = { ArenaMidShellRadius(Params), ArenaOuterShellRadius(Params) };
        const int32 ShellCounts[] = { 10, 12 };
        for (int32 Shell = 0; Shell < 2; ++Shell)
        {
            for (int32 Index = 0; Index < ShellCounts[Shell]; ++Index)
            {
                const float Bearing = 360.0f * Index / ShellCounts[Shell] + Shell * 18.0f;
                const float Radians = FMath::DegreesToRadians(Bearing);
                const float Fwd = Params.ArenaDistanceCm + FMath::Cos(Radians) * ShellRadii[Shell];
                const float Rgt = FMath::Sin(Radians) * ShellRadii[Shell];
                if (IsAuthoredPieceBlocked(Params, Fwd, Rgt, Params.ChestHalfLengthCm)) continue;
                AppendChest(Pieces, Params, Fwd, Rgt, Bearing + 90.0f, ShellCluster);
            }
        }
    }

    // --- 6. THE JUMP-RUN EDGES ---------------------------------------------
    // The run stays clear. Its two long edges get a chest line, so the exclusion
    // does not punch a coverage hole in the ground beside it and a player who
    // drops out of the trench mid-fight is not stepping into 60 m of nothing.
    {
        const int32 EdgeCluster = ClusterIndex++;
        const float EdgeRight = Params.JumpRunHalfWidthCm + 300.0f;
        const float Pitch = FMath::Max(Params.CoverPitchMaxCm, 100.0f);
        for (float Fwd = Params.JumpRunNearCm; Fwd <= Params.JumpRunFarCm; Fwd += Pitch)
        {
            for (int32 Side = 0; Side < 2; ++Side)
            {
                const float Rgt = (Side == 0 ? -1.0f : 1.0f) * EdgeRight;
                if (IsClusterCentreExcluded(Params, Fwd, Rgt, Params.ChestHalfLengthCm)) continue;
                AppendChest(Pieces, Params, Fwd, Rgt, 90.0f + Stream.FRandRange(-10.0f, 10.0f), EdgeCluster);
            }
        }
    }

    // --- 7. THE SNIPER LANE'S HARD COVER -----------------------------------
    // Unchanged in position from the shipped field: one full-height piece with
    // RangedSightlineDepth of clear ground behind it. It is the anchor wave mode
    // finds when the playtest happens to be standing on the lane, which is a
    // real case — the lane is 100 m long and the sampler lives on it.
    if (Params.bBuildLaneCover)
    {
        AppendPillar(Pieces, Params, Params.LaneCoverForwardCm, Params.LaneCoverRightCm,
            FMath::Max(Params.FullHeightCm, 300.0f), ClusterIndex++);
    }

    // The sampling grid the last two passes share. The step and the threshold
    // are tied together on purpose: a general point is at most Step/sqrt(2) from
    // some sample, so a field where every SAMPLE is within (CoverPitchMax -
    // Step/sqrt(2)) of cover has every POINT within CoverPitchMax of cover. Get
    // that relation wrong and the passes leave holes exactly halfway between
    // their own samples, which is how the first draft shipped a 1786 cm gap
    // while believing its threshold was 1615.
    const float FillStep = FMath::Max(Params.CoverPitchMaxCm * 0.30f, 100.0f);
    const float FillThreshold = Params.CoverPitchMaxCm - FillStep * 0.70711f;
    const int32 FillRows = FMath::Max(1, FMath::CeilToInt((Params.BandFarCm - Params.BandNearCm) / FillStep));
    const int32 FillColumns = FMath::Max(1, FMath::CeilToInt((2.0f * Params.BandHalfWidthCm) / FillStep));
    const float FillFwdStep = (Params.BandFarCm - Params.BandNearCm) / FillRows;
    const float FillRgtStep = (2.0f * Params.BandHalfWidthCm) / FillColumns;

    // --- 8. THE LINE-BREAK PASS ---------------------------------------------
    // The fill pass below answers "is there cover here"; this one answers "is
    // there anywhere to BREAK THE LINE here", which is a different question and
    // the one the Skirmisher's whole archetype turns on. A 120 cm block does not
    // stop a LATTICE orb or a Skirmisher's own line — both traces are real — so
    // a field that satisfies the cover pitch entirely in chest-high pieces still
    // has no Skirmisher in it.
    //
    // It runs BEFORE the fill so the fill can see its pillars and not stack a
    // block on one; it keeps a minimum separation from existing line breaks so
    // it cannot close a dash lane; and it keeps clear of every other piece so it
    // cannot land inside one.
    {
        const int32 MaximumLineBreaks = 200;
        const float PieceClearance = Params.FullHalfExtentCm + Params.ChestHalfLengthCm + 200.0f;
        int32 LineBreaks = 0;
        for (int32 Row = 0; Row <= FillRows && LineBreaks < MaximumLineBreaks; ++Row)
        {
            const float Fwd = Params.BandNearCm + Row * FillFwdStep;
            for (int32 Column = 0; Column <= FillColumns && LineBreaks < MaximumLineBreaks; ++Column)
            {
                const float Rgt = -Params.BandHalfWidthCm + Column * FillRgtStep;
                if (IsPointInClearedGround(Params, Fwd, Rgt)) continue;
                const float ToLineBreak =
                    DistanceToNearestCoverOfClass(Pieces, Fwd, Rgt, EBreakerCoverClass::FullHeight);
                if (ToLineBreak <= Params.CoverPitchMaxCm) continue;
                if (ToLineBreak < Separation) continue;
                if (DistanceToNearestCover(Pieces, Fwd, Rgt) < PieceClearance) continue;
                if (IsClusterCentreExcluded(Params, Fwd, Rgt, Params.FullHalfExtentCm)) continue;
                AppendPillar(Pieces, Params, Fwd, Rgt, Params.FullHeightCm, ClusterIndex++);
                ++LineBreaks;
            }
        }
    }

    // --- 9. THE FILL PASS ---------------------------------------------------
    // Everything above is forced geometry, and forced geometry has seams: an
    // exclusion clips a lattice node and the ground beside it loses its cover
    // without anything noticing. In this field that is not a corner case — the
    // instrument corridor, the jump-gap run and the arena between them exclude
    // most of the band's centre, and the raw lattice comes through at only a
    // couple of nodes. So rather than assert in a comment that the seams are
    // fine, the layout MEASURES itself and fills what it finds: a whole cluster
    // where one fits, a lone chest-high piece where only that does.
    //
    // Deterministic — a fixed grid with INCLUSIVE endpoints, walked in a fixed
    // order — and capped, so a misconfigured pitch produces a reported failure
    // rather than ten thousand actors. The endpoints matter: an exclusive grid
    // leaves the band's far corners unsampled and therefore unfilled, which is a
    // 2384 cm hole that the coverage assertion catches and nothing else does.
    {
        const int32 MaximumFills = 800;
        int32 Fills = 0;
        for (int32 Row = 0; Row <= FillRows && Fills < MaximumFills; ++Row)
        {
            const float Fwd = Params.BandNearCm + Row * FillFwdStep;
            for (int32 Column = 0; Column <= FillColumns && Fills < MaximumFills; ++Column)
            {
                const float Rgt = -Params.BandHalfWidthCm + Column * FillRgtStep;
                if (IsPointInClearedGround(Params, Fwd, Rgt)) continue;
                if (DistanceToNearestCover(Pieces, Fwd, Rgt) <= FillThreshold) continue;
                const float BaseYaw = Stream.FRandRange(0.0f, 360.0f);
                const float FullRadians = FMath::DegreesToRadians(ClusterFullSlotBearing(Params, BaseYaw));
                const bool bClusterFits = !IsClusterCentreExcluded(Params, Fwd, Rgt, ClusterExtent)
                    && DistanceToNearestCoverOfClass(Pieces,
                        Fwd + FMath::Cos(FullRadians) * Params.ClusterRingRadiusCm,
                        Rgt + FMath::Sin(FullRadians) * Params.ClusterRingRadiusCm,
                        EBreakerCoverClass::FullHeight) >= Separation;
                if (bClusterFits)
                {
                    AppendCluster(Pieces, Params, Fwd, Rgt, BaseYaw, ClusterIndex++);
                }
                else if (!IsClusterCentreExcluded(Params, Fwd, Rgt, Params.ChestHalfLengthCm))
                {
                    AppendChest(Pieces, Params, Fwd, Rgt, Stream.FRandRange(0.0f, 180.0f), ClusterIndex++);
                }
                else continue;
                ++Fills;
            }
        }
    }

    return Pieces;
}

float UBreakerCoverLayoutLibrary::DistanceToNearestCover(const TArray<FBreakerCoverPiece>& Pieces,
    float Forward, float Right)
{
    float Best = TNumericLimits<float>::Max();
    for (const FBreakerCoverPiece& Piece : Pieces)
    {
        Best = FMath::Min(Best, Distance2D(Forward, Right, Piece.Forward, Piece.Right));
    }
    return Best;
}

float UBreakerCoverLayoutLibrary::DistanceToNearestCoverOfClass(const TArray<FBreakerCoverPiece>& Pieces,
    float Forward, float Right, EBreakerCoverClass Class)
{
    float Best = TNumericLimits<float>::Max();
    for (const FBreakerCoverPiece& Piece : Pieces)
    {
        if (Piece.Class != Class) continue;
        Best = FMath::Min(Best, Distance2D(Forward, Right, Piece.Forward, Piece.Right));
    }
    return Best;
}

float UBreakerCoverLayoutLibrary::LargestUncoveredGap(const TArray<FBreakerCoverPiece>& Pieces,
    const FBreakerCoverFieldParams& Params, int32 SamplesAcross)
{
    if (Pieces.Num() == 0) return TNumericLimits<float>::Max();
    const int32 Samples = FMath::Clamp(SamplesAcross, 4, 512);
    const float FwdStep = FMath::Max((Params.BandFarCm - Params.BandNearCm) / Samples, 1.0f);
    const float RgtStep = FMath::Max((2.0f * Params.BandHalfWidthCm) / Samples, 1.0f);
    float Worst = 0.0f;
    for (int32 FwdIndex = 0; FwdIndex <= Samples; ++FwdIndex)
    {
        const float Fwd = Params.BandNearCm + FwdIndex * FwdStep;
        for (int32 RgtIndex = 0; RgtIndex <= Samples; ++RgtIndex)
        {
            const float Rgt = -Params.BandHalfWidthCm + RgtIndex * RgtStep;
            if (IsPointInClearedGround(Params, Fwd, Rgt)) continue;
            Worst = FMath::Max(Worst, DistanceToNearestCover(Pieces, Fwd, Rgt));
        }
    }
    return Worst;
}

float UBreakerCoverLayoutLibrary::LargestGapToLineBreak(const TArray<FBreakerCoverPiece>& Pieces,
    const FBreakerCoverFieldParams& Params, int32 SamplesAcross)
{
    if (CountOfClass(Pieces, EBreakerCoverClass::FullHeight) == 0) return TNumericLimits<float>::Max();
    const int32 Samples = FMath::Clamp(SamplesAcross, 4, 512);
    const float FwdStep = FMath::Max((Params.BandFarCm - Params.BandNearCm) / Samples, 1.0f);
    const float RgtStep = FMath::Max((2.0f * Params.BandHalfWidthCm) / Samples, 1.0f);
    // The instrument corridor is skipped: it carries no full-height cover on
    // purpose, so measuring it here would report a design decision as a defect.
    const float CorridorInner = Params.CorridorShoulderOffsetCm + Params.ChestHalfLengthCm;
    float Worst = 0.0f;
    for (int32 FwdIndex = 0; FwdIndex <= Samples; ++FwdIndex)
    {
        const float Fwd = Params.BandNearCm + FwdIndex * FwdStep;
        const bool bCorridorSpan = Fwd >= Params.CorridorNearCm - 1.0f && Fwd <= Params.CorridorFarCm + 1.0f;
        for (int32 RgtIndex = 0; RgtIndex <= Samples; ++RgtIndex)
        {
            const float Rgt = -Params.BandHalfWidthCm + RgtIndex * RgtStep;
            if (bCorridorSpan && FMath::Abs(Rgt) < CorridorInner) continue;
            if (IsPointInClearedGround(Params, Fwd, Rgt)) continue;
            Worst = FMath::Max(Worst,
                DistanceToNearestCoverOfClass(Pieces, Fwd, Rgt, EBreakerCoverClass::FullHeight));
        }
    }
    return Worst;
}

float UBreakerCoverLayoutLibrary::MinimumOpenLaneWidth(const TArray<FBreakerCoverPiece>& Pieces,
    const FBreakerCoverFieldParams& Params)
{
    // FULL-HEIGHT ONLY, and that is a design statement rather than a shortcut.
    // Chest-high cover is 120 cm, which is under MantleStepHeight (145): a
    // player does not go around it, they go over it, so it does not close a
    // lane. Only a piece that cannot be mantled or seen over narrows the ground.
    //
    // Pieces standing ON an authored movement lane are excluded, because the
    // width of that ground is already set by the lane's own kerbs — the sniper
    // lane is G3 wide by construction and its hard-cover piece is furniture on
    // it, not a gate across the open field.
    float Narrowest = TNumericLimits<float>::Max();
    for (int32 A = 0; A < Pieces.Num(); ++A)
    {
        if (Pieces[A].Class != EBreakerCoverClass::FullHeight) continue;
        if (IsOnAuthoredMovementLane(Params, Pieces[A].Forward, Pieces[A].Right)) continue;
        for (int32 B = A + 1; B < Pieces.Num(); ++B)
        {
            if (Pieces[B].Class != EBreakerCoverClass::FullHeight) continue;
            if (IsOnAuthoredMovementLane(Params, Pieces[B].Forward, Pieces[B].Right)) continue;
            // Within one cluster the pieces are deliberately close — that
            // closeness is what makes it a cluster rather than a scatter.
            if (Pieces[A].ClusterIndex == Pieces[B].ClusterIndex && Pieces[A].ClusterIndex >= 0) continue;
            const float Clear = Distance2D(Pieces[A].Forward, Pieces[A].Right, Pieces[B].Forward, Pieces[B].Right)
                - Pieces[A].ExtentCm() - Pieces[B].ExtentCm();
            Narrowest = FMath::Min(Narrowest, Clear);
        }
    }
    return Narrowest;
}

float UBreakerCoverLayoutLibrary::NearestPieceToCorridorCentre(const TArray<FBreakerCoverPiece>& Pieces,
    const FBreakerCoverFieldParams& Params)
{
    // THE CORRIDOR RULE, REPORTED IN THE RULE'S OWN UNITS. IsLayoutLegal
    // already rejects any piece standing within CorridorHalfWidth of the
    // centreline over the corridor's forward span; this returns how much room
    // the closest piece actually left, so the margin is visible while it is
    // healthy instead of only when it is gone.
    //
    // IT IS AN OFFSET, NOT A WIDTH, and that is deliberate. A width has to
    // pick a convention — centre-to-centre or face-to-face — and the two differ
    // by a piece's depth, so printing one against a floor written in the other
    // is how a passing field reads as failing. The placement rule is written
    // about CENTRES, so this reports centres and compares against the same
    // number the rule uses. A zone author who wants a face-to-face figure
    // subtracts their own piece's half-depth and owns the arithmetic.
    float Nearest = TNumericLimits<float>::Max();
    for (const FBreakerCoverPiece& Piece : Pieces)
    {
        if (Piece.Forward <= Params.CorridorNearCm || Piece.Forward >= Params.CorridorFarCm) continue;
        Nearest = FMath::Min(Nearest, FMath::Abs(Piece.Right));
    }
    return Nearest;
}

float UBreakerCoverLayoutLibrary::MinimumPieceClearance(const TArray<FBreakerCoverPiece>& Pieces)
{
    // The narrowest gap between ANY two pieces, chest-high included. Two
    // separate things ride on this number and both are easy to break silently:
    // a negative value is interpenetrating geometry, and a value under an
    // enemy's body diameter is a gap that enemy cannot path through. The widest
    // body in the project is the SEVERED DRUDGE at 120 cm.
    float Narrowest = TNumericLimits<float>::Max();
    for (int32 A = 0; A < Pieces.Num(); ++A)
    {
        for (int32 B = A + 1; B < Pieces.Num(); ++B)
        {
            const float Clear = Distance2D(Pieces[A].Forward, Pieces[A].Right, Pieces[B].Forward, Pieces[B].Right)
                - Pieces[A].ExtentCm() - Pieces[B].ExtentCm();
            Narrowest = FMath::Min(Narrowest, Clear);
        }
    }
    return Narrowest;
}

float UBreakerCoverLayoutLibrary::CoverAreaFraction(const TArray<FBreakerCoverPiece>& Pieces,
    const FBreakerCoverFieldParams& Params)
{
    const float BandArea = Params.BandAreaCm2();
    if (BandArea <= 0.0f) return 0.0f;
    float Footprint = 0.0f;
    for (const FBreakerCoverPiece& Piece : Pieces) Footprint += Piece.FootprintAreaCm2();
    return Footprint / BandArea;
}

int32 UBreakerCoverLayoutLibrary::CountOfClass(const TArray<FBreakerCoverPiece>& Pieces, EBreakerCoverClass Class)
{
    int32 Count = 0;
    for (const FBreakerCoverPiece& Piece : Pieces) { if (Piece.Class == Class) ++Count; }
    return Count;
}

bool UBreakerCoverLayoutLibrary::IsLayoutLegal(const TArray<FBreakerCoverPiece>& Pieces,
    const FBreakerCoverFieldParams& Params, FString& OutReason)
{
    OutReason.Reset();
    if (Pieces.Num() == 0)
    {
        OutReason = TEXT("the field has no hard cover at all");
        return false;
    }
    if (CountOfClass(Pieces, EBreakerCoverClass::FullHeight) == 0)
    {
        OutReason = TEXT("no FULL-HEIGHT cover: nothing in the field breaks a LATTICE sight line, so the Skirmisher degrades to a slower Lattice");
        return false;
    }
    if (CountOfClass(Pieces, EBreakerCoverClass::ChestHigh) == 0)
    {
        OutReason = TEXT("no CHEST-HIGH cover: every piece is a line break, which is a maze rather than a fight");
        return false;
    }

    for (const FBreakerCoverPiece& Piece : Pieces)
    {
        if (Distance2D(Piece.Forward, Piece.Right, 0.0f, 0.0f) < Params.SafeZoneRadiusCm + Piece.ExtentCm())
        {
            OutReason = FString::Printf(TEXT("a cover piece at (%.0f, %.0f) stands inside the safe ring"),
                Piece.Forward, Piece.Right);
            return false;
        }
        // The marker ring is a RING, not a disc: §3.3 authors two pillars and a
        // cover ring inside the arena, so what is forbidden is standing ON the
        // markers.
        const float ArenaDistance = Distance2D(Piece.Forward, Piece.Right, Params.ArenaDistanceCm, 0.0f);
        if (FMath::Abs(ArenaDistance - Params.CombatPocketRadiusCm) < Piece.ExtentCm() + 100.0f)
        {
            OutReason = FString::Printf(TEXT("a cover piece at (%.0f, %.0f) stands on the arena marker ring"),
                Piece.Forward, Piece.Right);
            return false;
        }
        if (InRect(Piece.Forward, Piece.Right, Params.JumpRunNearCm, Params.JumpRunFarCm, 0.0f,
            Params.JumpRunHalfWidthCm, Piece.ExtentCm()))
        {
            OutReason = FString::Printf(TEXT("a cover piece at (%.0f, %.0f) stands in the jump-gap run"),
                Piece.Forward, Piece.Right);
            return false;
        }
        const bool bInCorridorSpan = Piece.Forward > Params.CorridorNearCm && Piece.Forward < Params.CorridorFarCm;
        // The instrument corridor may carry chest-high shoulders and nothing
        // else: a full-height slab anywhere near its centre would stand across
        // the TTK sampler's shot and across the player's line in on arrival.
        if (bInCorridorSpan && Piece.Class == EBreakerCoverClass::FullHeight
            && FMath::Abs(Piece.Right) < Params.CorridorShoulderOffsetCm + Params.ChestHalfLengthCm)
        {
            OutReason = FString::Printf(TEXT("FULL-HEIGHT cover at (%.0f, %.0f) blocks the instrument corridor"),
                Piece.Forward, Piece.Right);
            return false;
        }
        if (bInCorridorSpan && FMath::Abs(Piece.Right) < Params.CorridorHalfWidthCm)
        {
            OutReason = FString::Printf(TEXT("cover at (%.0f, %.0f) stands in the firing line's own corridor"),
                Piece.Forward, Piece.Right);
            return false;
        }
    }

    // G23: no exposed crossing.
    const float Gap = LargestUncoveredGap(Pieces, Params);
    if (Gap > Params.CoverPitchMaxCm)
    {
        OutReason = FString::Printf(
            TEXT("largest uncovered gap is %.0f cm against a cover pitch of %.0f: that ground cannot answer a LATTICE telegraph by moving"),
            Gap, Params.CoverPitchMaxCm);
        return false;
    }

    // Somewhere to break the line, everywhere a fight can happen.
    const float LineBreakGap = LargestGapToLineBreak(Pieces, Params);
    if (LineBreakGap > Params.MaximumLineBreakGapCm)
    {
        OutReason = FString::Printf(
            TEXT("furthest ground from a FULL-HEIGHT piece is %.0f cm against a limit of %.0f: a Skirmisher spawned there has nothing to break line of sight behind"),
            LineBreakGap, Params.MaximumLineBreakGapCm);
        return false;
    }

    // G3: the field must not close into a maze.
    const float Lane = MinimumOpenLaneWidth(Pieces, Params);
    if (Lane < Params.DashCorridorWidthCm)
    {
        OutReason = FString::Printf(
            TEXT("narrowest FULL-HEIGHT lane is %.0f cm against a dash corridor FLOOR of %.0f"),
            Lane, Params.DashCorridorWidthCm);
        return false;
    }

    // Nothing may interpenetrate, and no gap may be narrower than the widest
    // body in the project — a gap a SEVERED DRUDGE cannot fit through is a
    // layout bug that only shows up in play.
    const float Clearance = MinimumPieceClearance(Pieces);
    if (Clearance < Params.WidestEnemyBodyCm)
    {
        OutReason = FString::Printf(
            TEXT("narrowest gap between two pieces is %.0f cm against the widest enemy body of %.0f"),
            Clearance, Params.WidestEnemyBodyCm);
        return false;
    }

    const float Fraction = CoverAreaFraction(Pieces, Params);
    if (Fraction < Params.MinimumCoverFraction || Fraction > Params.MaximumCoverFraction)
    {
        OutReason = FString::Printf(TEXT("cover occupies %.2f%% of the band, outside the %.2f%%-%.2f%% band"),
            Fraction * 100.0f, Params.MinimumCoverFraction * 100.0f, Params.MaximumCoverFraction * 100.0f);
        return false;
    }
    return true;
}

float UBreakerCoverLayoutLibrary::DistanceAffordedInField(const FBreakerCoverFieldParams& Params,
    float FromForward, float FromRight, float DirForward, float DirRight, float PackRadiusCm)
{
    float Enter = 0.0f;
    float Exit = 0.0f;
    if (!SolveFieldRayInterval(Params, FromForward, FromRight, DirForward, DirRight, PackRadiusCm, Enter, Exit))
    {
        return 0.0f;
    }
    return FMath::Max(Exit, 0.0f);
}

bool UBreakerCoverLayoutLibrary::SolveFieldRayInterval(const FBreakerCoverFieldParams& Params,
    float FromForward, float FromRight, float DirForward, float DirRight, float PackRadiusCm,
    float& OutEnter, float& OutExit)
{
    // The band, shrunk by the pack's own radius: a centre inside THIS keeps the
    // whole pack inside the yard, which is the difference between "the spawn
    // point is in the yard" and "the pack is in the yard".
    const float Radius = FMath::Max(PackRadiusCm, 0.0f);
    const float NearEdge = Params.BandNearCm + Radius;
    const float FarEdge = Params.BandFarCm - Radius;
    const float SideEdge = Params.BandHalfWidthCm - Radius;
    OutEnter = 0.0f;
    OutExit = 0.0f;
    if (NearEdge > FarEdge || SideEdge <= 0.0f) return false;

    // BOTH ENDS OF THE SLAB, and the ENTRY half is the one that matters.
    // Clipping only the exit is wrong whenever the ray starts outside the
    // shrunk band, which happens constantly: a player at the yard's edge is
    // inside the wall and outside the pack's margin. The first version of this
    // did exactly that and returned a centre short of the near edge; the
    // containment sweep caught it, and a single sample from mid-yard would not
    // have.
    float Enter = 0.0f;
    float Exit = TNumericLimits<float>::Max();
    auto ClipAxis = [&Enter, &Exit](float From, float Dir, float MinEdge, float MaxEdge) -> bool
    {
        if (FMath::Abs(Dir) < KINDA_SMALL_NUMBER)
        {
            // Parallel to this slab: either always inside it, or never.
            return From >= MinEdge && From <= MaxEdge;
        }
        float T0 = (MinEdge - From) / Dir;
        float T1 = (MaxEdge - From) / Dir;
        if (T0 > T1) { const float Swap0 = T0; T0 = T1; T1 = Swap0; }
        Enter = FMath::Max(Enter, T0);
        Exit = FMath::Min(Exit, T1);
        return Exit >= Enter;
    };
    if (!ClipAxis(FromForward, DirForward, NearEdge, FarEdge)) return false;
    if (!ClipAxis(FromRight, DirRight, -SideEdge, SideEdge)) return false;
    if (Exit < Enter || Exit <= 0.0f) return false;

    OutEnter = FMath::Max(Enter, 0.0f);
    OutExit = Exit;
    return OutExit >= OutEnter;
}

bool UBreakerCoverLayoutLibrary::SolveContainedSpawnCentre(const FBreakerCoverFieldParams& Params,
    float PlayerForward, float PlayerRight, float FacingForward, float FacingRight,
    float BandMinCm, float BandMaxCm, float PackRadiusCm,
    float& OutForward, float& OutRight, float& OutAffordedCm)
{
    const float Low = FMath::Min(BandMinCm, BandMaxCm);
    const float High = FMath::Max(BandMinCm, BandMaxCm);

    float FacingLength = FMath::Sqrt(FacingForward * FacingForward + FacingRight * FacingRight);
    if (FacingLength < KINDA_SMALL_NUMBER) { FacingForward = 1.0f; FacingRight = 0.0f; FacingLength = 1.0f; }
    const float BaseF = FacingForward / FacingLength;
    const float BaseR = FacingRight / FacingLength;

    // THE LAST-RESORT CENTRE is the band's own middle, inside by construction.
    // Set before the search so every path out of this function is contained,
    // including the one where the player stands somewhere no heading fits.
    OutForward = 0.5f * (Params.BandNearCm + Params.BandFarCm);
    OutRight = 0.0f;
    OutAffordedCm = 0.0f;

    // ROTATE AWAY FROM THE FACING ONLY AS FAR AS CONTAINMENT REQUIRES, and try
    // both ways at each step so the pack does not consistently drift to one
    // side. A spawner that always rotated clockwise would put every fight in a
    // walled yard on the same flank, which is a tell nobody would trace back to
    // here. 24 steps is 15 degrees apart: fine enough that the honoured facing
    // is visibly the player's, coarse enough to stay cheap.
    const int32 Steps = 24;
    float BestRoom = -1.0f;
    float BestF = 0.0f;
    float BestR = 0.0f;
    float BestDistance = 0.0f;
    bool bBestFound = false;

    for (int32 Step = 0; Step <= Steps / 2; ++Step)
    {
        const float Angle = FMath::DegreesToRadians(360.0f * Step / Steps);
        for (int32 Sign = 0; Sign < (Step == 0 ? 1 : 2); ++Sign)
        {
            const float Theta = Sign == 0 ? Angle : -Angle;
            const float CosT = FMath::Cos(Theta);
            const float SinT = FMath::Sin(Theta);
            const float DirF = BaseF * CosT - BaseR * SinT;
            const float DirR = BaseF * SinT + BaseR * CosT;

            float Enter = 0.0f;
            float Exit = 0.0f;
            if (!SolveFieldRayInterval(Params, PlayerForward, PlayerRight, DirF, DirR, PackRadiusCm, Enter, Exit))
            {
                continue;
            }

            // The containable stretch of the authored band along this heading.
            const float DLow = FMath::Max(Low, Enter);
            const float DHigh = FMath::Min(High, Exit);
            if (DLow <= DHigh)
            {
                // FURTHEST ALLOWED, up to the authored distance: the band's top
                // is "one dash-cooldown of ground away" and is kept whenever
                // the yard has room for it. The first heading that fits wins,
                // because this search exists to honour the facing rather than
                // to find the roomiest corner of the yard.
                OutForward = PlayerForward + DirF * DHigh;
                OutRight = PlayerRight + DirR * DHigh;
                OutAffordedCm = DHigh;
                return true;
            }

            // This heading cannot hold the band. Remember the roomiest one so
            // the fallback is the best the yard has rather than the first thing
            // tried.
            const float Room = Exit - Enter;
            if (Room > BestRoom)
            {
                BestRoom = Room;
                BestF = DirF;
                BestR = DirR;
                BestDistance = 0.5f * (Enter + Exit);
                bBestFound = true;
            }
        }
    }

    // NOTHING AFFORDED THE BAND. Place at the middle of the roomiest heading
    // rather than refusing: a rift run that cannot spawn is worse than one that
    // spawns close, and the caller is handed the figure so it can say so out
    // loud. Whatever happens, the centre is inside the field.
    if (bBestFound)
    {
        OutForward = PlayerForward + BestF * BestDistance;
        OutRight = PlayerRight + BestR * BestDistance;
        OutAffordedCm = BestDistance;
    }
    return false;
}

bool UBreakerCoverLayoutLibrary::IsZoneLegal(const TArray<FBreakerZoneField>& Yards, FString& OutReason)
{
    // A zone with no yards is not an empty zone, it is a build that produced
    // nothing — the same reading CollectZonePieces takes of an empty mesh
    // folder. Passing it would report the strongest possible verdict on the
    // least possible evidence.
    if (Yards.Num() == 0)
    {
        OutReason = TEXT("a zone with no yards in it; the build produced nothing to measure");
        return false;
    }

    // No two yards may share a name. Yard names key the marker contract, so a
    // duplicate would make "the north yard's door" ambiguous, and the first
    // match would silently win.
    for (int32 A = 0; A < Yards.Num(); ++A)
    {
        for (int32 B = A + 1; B < Yards.Num(); ++B)
        {
            if (Yards[A].Yard == Yards[B].Yard)
            {
                OutReason = FString::Printf(TEXT("two yards named '%s'"),
                    Yards[A].Yard.IsNone() ? TEXT("<entry>") : *Yards[A].Yard.ToString());
                return false;
            }
        }
    }

    // EVERY yard, and the failing one is NAMED. A zone-level "illegal" with no
    // yard on it would send the reader to search a whole world for a number
    // that belongs to one room.
    for (const FBreakerZoneField& Yard : Yards)
    {
        FString Reason;
        if (!IsLayoutLegal(Yard.Pieces, Yard.Params, Reason))
        {
            OutReason = FString::Printf(TEXT("yard '%s' is grammar-illegal: %s"),
                Yard.Yard.IsNone() ? TEXT("<entry>") : *Yard.Yard.ToString(), *Reason);
            return false;
        }
    }

    OutReason.Reset();
    return true;
}

FString UBreakerCoverLayoutLibrary::DescribeCoverField(const TArray<FBreakerCoverPiece>& Pieces,
    const FBreakerCoverFieldParams& Params)
{
    // EVERY BAND STATES ITS DIRECTION, in status.py's vocabulary (CEILING falls
    // and never rises, FLOOR rises and never falls, BAND stays inside). Two of
    // these pass by being UNDER their limit and two by being OVER it, and the
    // bare "%.0f (limit %.0f)" this line used to print did not say which — so a
    // reader assuming the wrong direction reads a passing field as failing, or
    // worse, the reverse.
    //
    // The lane figure NAMES ITS CLASS. It used to print as "narrowest lane",
    // which reads as the ground the player dashes down and is not: it is
    // full-height only, because chest cover at 120 cm is under MantleStepHeight
    // and is gone over rather than around. THE GROUND THE PLAYER DASHES DOWN IS
    // GUARDED BY A DIFFERENT RULE — the per-piece corridor rejection — and that
    // rule now prints its own margin beside the lane instead of being the only
    // constraint here with no number attached (O132).
    return FString::Printf(
        TEXT("cover field: %d pieces (%d chest-high @ %.0f cm, %d full-height @ %.0f cm)")
        TEXT(" | largest uncovered gap %.0f CEILING %.0f")
        TEXT(" | furthest from a line break %.0f CEILING %.0f")
        TEXT(" | narrowest FULL-HEIGHT lane %.0f FLOOR %.0f")
        TEXT(" | nearest piece to the corridor centreline %.0f FLOOR %.0f")
        TEXT(" | tightest gap %.0f FLOOR %.0f (widest body)")
        TEXT(" | cover footprint %.2f%% BAND %.2f%%-%.2f%%"),
        Pieces.Num(),
        CountOfClass(Pieces, EBreakerCoverClass::ChestHigh), Params.ChestHeightCm,
        CountOfClass(Pieces, EBreakerCoverClass::FullHeight), Params.FullHeightCm,
        LargestUncoveredGap(Pieces, Params), Params.CoverPitchMaxCm,
        LargestGapToLineBreak(Pieces, Params), Params.MaximumLineBreakGapCm,
        MinimumOpenLaneWidth(Pieces, Params), Params.DashCorridorWidthCm,
        NearestPieceToCorridorCentre(Pieces, Params), Params.CorridorHalfWidthCm,
        MinimumPieceClearance(Pieces), Params.WidestEnemyBodyCm,
        CoverAreaFraction(Pieces, Params) * 100.0f,
        Params.MinimumCoverFraction * 100.0f, Params.MaximumCoverFraction * 100.0f);
}
