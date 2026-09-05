#include "Game/BreakerWaveBudget.h"

FBreakerWaveBudgetParams UBreakerWaveBudgetLibrary::MakeRiftWaveBudget(int32 BossWave)
{
    FBreakerWaveBudgetParams Params;

    // A RUN IS THREE WAVES, so everything the run is supposed to contain has to
    // arrive inside three. The gym's schedule introduces the Skirmisher on wave
    // 4 and the first promotion on wave 4, which in a rift means never.
    Params.BossWaveInterval = FMath::Max(BossWave, 1);
    // The rest beat is 4.3's endurance pacing and a run is not an endurance
    // test. Parked beyond the boss rather than deleted: a longer rift tier may
    // want it back, and a rest wave inside a three-wave run would spend a third
    // of the run on a breather.
    Params.RestWaveInterval = FMath::Max(BossWave, 1) * 100;

    // Everything arrives by wave 2, so the last wave before the boss is the
    // one that has all of it.
    Params.LatticeFromWave = 1;   // O2 PLACEHOLDER
    Params.WardenFromWave = 2;   // O2 PLACEHOLDER
    Params.SkirmisherFromWave = 2;   // O2 PLACEHOLDER
    Params.ModifierCarrierFromWave = 2;   // O2 PLACEHOLDER
    // floor(wave/WavesPerElite), so 2 puts the first elite on wave 2 rather
    // than on a wave the run never reaches.
    Params.WavesPerElite = 2;   // O2 PLACEHOLDER
    Params.VarietyEnforcedFromWave = 2;   // O2 PLACEHOLDER

    // AND THE BUDGET HAS TO FUND WHAT THE SCHEDULE INTRODUCES. Compressing the
    // introduction waves alone does not work, and the first solve said so: at
    // the gym's curve (6 + 4n) wave 2 has 14 points, spends 6 on the Warden, 3
    // on the Skirmisher and 3 on the Lattice, and has 2 left against an elite
    // promotion costing 5. So the elite was introduced on a wave that could not
    // buy one, and a run contained NO elite at all — which puts the 0.75 elite
    // drop rate out of reach for the same reason the 0.10 trash rate was.
    //
    // A THREE-WAVE RUN ESCALATES FASTER BECAUSE IT HAS FEWER WAVES TO DO IT IN.
    // The steeper curve is the compression expressed in points rather than in
    // wave numbers; introducing early and funding late is half a change.
    // O2 PLACEHOLDER, and the solved composition is in the lane's report.
    Params.BudgetPerWave = 9;   // O2 PLACEHOLDER

    // EVERY WAVE OF A RUN DROPS. The gym's rest-and-boss-only rule is its
    // measurement discipline, not the player's economy.
    Params.bRiftInstance = true;
    return Params;
}

int32 UBreakerWaveBudgetLibrary::GetWaveBudget(int32 Wave, const FBreakerWaveBudgetParams& Params)
{
    if (Wave <= 0) return 0;
    const int32 Raw = Params.BudgetBase + Params.BudgetPerWave * Wave;
    const int32 Capped = FMath::Min(Raw, FMath::Max(Params.BudgetCap, 1));
    if (GetWaveKind(Wave, Params) == EBreakerWaveKind::Rest)
    {
        return FMath::Max(FMath::FloorToInt32(Capped * FMath::Clamp(Params.RestBudgetFraction, 0.0f, 1.0f)), 1);
    }
    return Capped;
}

EBreakerWaveKind UBreakerWaveBudgetLibrary::GetWaveKind(int32 Wave, const FBreakerWaveBudgetParams& Params)
{
    if (Wave <= 0) return EBreakerWaveKind::Standard;
    // Boss is checked FIRST because 12 is a multiple of 6: §4.2's table makes
    // wave 6 a rest wave and wave 12 the boss, so the boss interval wins where
    // they coincide. Reversing these two lines deletes the boss wave entirely,
    // which is exactly the kind of silent loss a cadence rule invites.
    const int32 BossInterval = FMath::Max(Params.BossWaveInterval, 1);
    const int32 RestInterval = FMath::Max(Params.RestWaveInterval, 1);
    if (Wave % BossInterval == 0) return EBreakerWaveKind::Boss;
    if (Wave % RestInterval == 0) return EBreakerWaveKind::Rest;
    return EBreakerWaveKind::Standard;
}

bool UBreakerWaveBudgetLibrary::GetAutoAdvanceDelay(int32 ClearedWave, const FBreakerWaveBudgetParams& Params, float& OutDelaySeconds)
{
    OutDelaySeconds = 0.0f;
    // The gym never advances itself: it is the instrument, and its pacing is
    // the tester's key.
    if (!Params.bRiftInstance || ClearedWave <= 0) return false;
    switch (GetWaveKind(ClearedWave, Params))
    {
    case EBreakerWaveKind::Boss:
        // Nothing follows the boss. The run ends when the terminator dies
        // (O168), and a wave after it would be a run that cannot end.
        return false;
    case EBreakerWaveKind::Rest:
        OutDelaySeconds = FMath::Max(Params.RestBreatherSeconds, 0.0f);
        return true;
    default:
        OutDelaySeconds = FMath::Max(Params.WaveClearBreatherSeconds, 0.0f);
        return true;
    }
}

int32 UBreakerWaveBudgetLibrary::GetMaximumLiveEnemies(int32 PartySize, const FBreakerWaveBudgetParams& Params)
{
    return FMath::Max(PartySize, 1) * FMath::Max(Params.MaximumLiveEnemiesPerPlayer, 1);
}

int32 UBreakerWaveBudgetLibrary::GetMaximumElites(int32 PartySize, const FBreakerWaveBudgetParams& Params)
{
    // §5.3 gives the two ends of the range (solo 1, five-player 3) and nothing
    // in between, so the middle is interpolated rather than invented.
    const int32 Clamped = FMath::Clamp(PartySize, 1, 5);
    const float Alpha = (Clamped - 1) / 4.0f;
    return FMath::RoundToInt32(FMath::Lerp(
        static_cast<float>(Params.MaximumElitesSolo), static_cast<float>(Params.MaximumElitesFullParty), Alpha));
}

FBreakerWaveComposition UBreakerWaveBudgetLibrary::SolveWave(int32 Wave, int32 PartySize, const FBreakerWaveBudgetParams& Params)
{
    FBreakerWaveComposition Out;
    if (Wave <= 0) return Out;

    const int32 Players = FMath::Max(PartySize, 1);
    Out.Wave = Wave;
    Out.Kind = GetWaveKind(Wave, Params);
    Out.Budget = GetWaveBudget(Wave, Params);
    // A RIFT RUN IS THE PLAYER'S LOOP AND EVERY WAVE IN IT DROPS; the gym is an
    // instrument and 4.3's rest-and-boss-only rule keeps its drop-rate data
    // clean. Same solver, same struct — the caller says which loop this is.
    Out.bDropsLoot = Params.bRiftInstance || Out.Kind != EBreakerWaveKind::Standard;

    if (Out.Kind == EBreakerWaveKind::Boss)
    {
        // Nothing else. The Field Marshal deploys its own adds and respawns its
        // own gallery Lattices, and §5.3's density ceiling is enforced at that
        // SOURCE — a wave budget spent alongside it would be counting the same
        // pressure twice and blowing the cap from two directions at once.
        Out.bBoss = true;
        Out.SpentBudget = Out.Budget;
        return Out;
    }

    int32 Remaining = Out.Budget;
    int32 Bodies = 0;
    const int32 MaximumBodies = GetMaximumLiveEnemies(Players, Params);

    // SPEND ORDER IS DELIBERATE: the capped, expensive archetypes first, and
    // the cheap uncapped one last. Filling with Skitters first would leave no
    // budget for a Warden and the wave would silently be one archetype — which
    // is the exact failure §4.3's variety rule exists to prevent.

    // 1. Warden. §5.3: one per player, "frontal-armour anchors overlapping
    //    create unsolvable geometry".
    if (Wave >= Params.WardenFromWave)
    {
        const int32 Affordable = Params.WardenCost > 0 ? Remaining / Params.WardenCost : 0;
        Out.Wardens = FMath::Min3(Affordable, Params.MaximumWardensPerPlayer * Players, MaximumBodies - Bodies);
        Out.Wardens = FMath::Max(Out.Wardens, 0);
        Remaining -= Out.Wardens * Params.WardenCost;
        Bodies += Out.Wardens;
    }

    // 2. Ranged sources, hard-capped at 3 TOTAL regardless of party size —
    //    §5.3 calls this "the single most dangerous scaling knob", because four
    //    converging projectile sources removes all safe ground.
    const int32 RangedAllowance = FMath::Max(Params.MaximumRangedSources, 0);
    if (Wave >= Params.SkirmisherFromWave)
    {
        // The Skirmisher takes its share FIRST, because it is the archetype
        // that teaches the fourth answer (push, take an angle) and a Lattice
        // that crowded it out would leave the wave asking the same question
        // twice at the same range.
        const int32 Wanted = FMath::Clamp(1 + (Wave - Params.SkirmisherFromWave) / 4, 1, RangedAllowance);
        const int32 Affordable = Params.SkirmisherCost > 0 ? Remaining / Params.SkirmisherCost : 0;
        Out.Skirmishers = FMath::Max(FMath::Min3(Wanted, Affordable, MaximumBodies - Bodies), 0);
        Remaining -= Out.Skirmishers * Params.SkirmisherCost;
        Bodies += Out.Skirmishers;
    }
    if (Wave >= Params.LatticeFromWave)
    {
        const int32 Wanted = FMath::Clamp(Wave / 2, 0, RangedAllowance - Out.Skirmishers);
        const int32 Affordable = Params.LatticeCost > 0 ? Remaining / Params.LatticeCost : 0;
        Out.Lattices = FMath::Max(FMath::Min3(Wanted, Affordable, MaximumBodies - Bodies), 0);
        Remaining -= Out.Lattices * Params.LatticeCost;
        Bodies += Out.Lattices;
    }

    // 3. Elite promotions. §4.2: floor(wave/4), and §5.3 caps live elites at 1
    //    solo — "two elites means two modifier sets to read simultaneously".
    //    An elite is a PROMOTED Skitter, so it costs a body plus its modifiers
    //    and does not add to the body count on its own.
    //    A REST wave takes none, which §4.2's table states outright (wave 6:
    //    "0 elites"): the beat exists so the player stops reading, and a
    //    modifier set is the most reading the game asks for.
    const int32 WantedElites = Out.Kind == EBreakerWaveKind::Rest
        ? 0
        : FMath::Min(Wave / FMath::Max(Params.WavesPerElite, 1), GetMaximumElites(Players, Params));
    if (WantedElites > 0)
    {
        Out.ModifiersPerElite = FMath::Clamp(
            1 + Wave / FMath::Max(Params.WavesPerExtraEliteModifier, 1), 1, 3);
        const int32 CostEach = Params.SkitterCost + Out.ModifiersPerElite * Params.EliteModifierCost;
        const int32 Affordable = CostEach > 0 ? Remaining / CostEach : 0;
        Out.Elites = FMath::Max(FMath::Min3(WantedElites, Affordable, MaximumBodies - Bodies), 0);
        if (Out.Elites == 0) Out.ModifiersPerElite = 0;
        Remaining -= Out.Elites * CostEach;
        // The promoted bodies ARE Skitters, so they are counted here and the
        // plain-Skitter fill below adds on top of them.
        Out.Skitters += Out.Elites;
        Bodies += Out.Elites;
    }

    // 3b. Non-elite modifier carriers (O27's kill-bucket producer — see the
    //    header note and Playtest/BreakerKillBuckets.h). Exactly the elite
    //    promotion pattern above: a promoted Skitter body, counted in
    //    Skitters and not added to the body count on its own. Zero on a REST
    //    wave for the same reason elites are zero there — the beat exists so
    //    the player stops reading, and a modifier set is more reading.
    const int32 WantedCarriers = (Out.Kind != EBreakerWaveKind::Rest && Wave >= Params.ModifierCarrierFromWave)
        ? FMath::Clamp(Wave / FMath::Max(Params.ModifierCarrierWaveDivisor, 1), 0, FMath::Max(Params.MaximumModifierCarriers, 0))
        : 0;
    if (WantedCarriers > 0)
    {
        // Reuses EliteModifierCost: both an elite promotion and a carrier
        // promotion are buying the same thing from the budget's point of
        // view — one modifier's worth of reading — so a carrier's single
        // modifier costs exactly what an elite's first one does.
        const int32 CarrierCostEach = Params.SkitterCost + Params.EliteModifierCost;
        const int32 Affordable = CarrierCostEach > 0 ? Remaining / CarrierCostEach : 0;
        Out.ModifierCarriers = FMath::Max(FMath::Min3(WantedCarriers, Affordable, MaximumBodies - Bodies), 0);
        Remaining -= Out.ModifierCarriers * CarrierCostEach;
        Out.Skitters += Out.ModifierCarriers;
        Bodies += Out.ModifierCarriers;
    }

    // 4. Fill with Skitters, against BOTH ceilings.
    if (Params.SkitterCost > 0)
    {
        int32 Fill = FMath::Min(Remaining / Params.SkitterCost, MaximumBodies - Bodies);

        // §4.3's variety rule: no archetype above 70% of the budget after wave
        // 3. Measured in BUDGET, not in head count, which is what the document
        // says and is the only reading that means anything — ten Skitters are
        // ten points and one Warden is six, so a head-count reading would call
        // a legal wave illegal.
        if (Wave >= Params.VarietyEnforcedFromWave && Out.Budget > 0)
        {
            const int32 SkitterBudgetCeiling = FMath::FloorToInt32(
                Out.Budget * FMath::Clamp(Params.MaximumSingleArchetypeShare, 0.0f, 1.0f));
            // Elite promotions carry their bodies' cost, so they count toward
            // the Skitter share: they are Skitters wearing modifiers.
            const int32 AlreadySpentOnSkitters = Out.Skitters * Params.SkitterCost;
            const int32 Headroom = FMath::Max(SkitterBudgetCeiling - AlreadySpentOnSkitters, 0);
            Fill = FMath::Min(Fill, Headroom / Params.SkitterCost);
        }

        Fill = FMath::Max(Fill, 0);
        Out.Skitters += Fill;
        Remaining -= Fill * Params.SkitterCost;
        Bodies += Fill;
    }

    // A wave with nothing in it is not a measurement. Only reachable if every
    // cost has been retuned past the budget, but silence would be worse.
    if (Bodies == 0)
    {
        Out.Skitters = 1;
        Remaining = FMath::Max(Remaining - Params.SkitterCost, 0);
    }

    Out.UnspentBudget = FMath::Max(Remaining, 0);
    Out.SpentBudget = Out.Budget - Out.UnspentBudget;
    return Out;
}

bool UBreakerWaveBudgetLibrary::IsCompositionLegal(const FBreakerWaveComposition& Composition, int32 PartySize,
    const FBreakerWaveBudgetParams& Params, FString& OutReason)
{
    const int32 Players = FMath::Max(PartySize, 1);

    if (Composition.TotalEnemies() > GetMaximumLiveEnemies(Players, Params))
    {
        OutReason = FString::Printf(TEXT("%d live enemies exceeds the density ceiling of %d (5.3)"),
            Composition.TotalEnemies(), GetMaximumLiveEnemies(Players, Params));
        return false;
    }
    if (Composition.RangedSources() > Params.MaximumRangedSources)
    {
        OutReason = FString::Printf(TEXT("%d ranged sources exceeds the cap of %d, which 5.3 holds at any party size"),
            Composition.RangedSources(), Params.MaximumRangedSources);
        return false;
    }
    if (Composition.Wardens + (Composition.bBoss ? 1 : 0) > Params.MaximumWardensPerPlayer * Players)
    {
        OutReason = FString::Printf(TEXT("%d Warden-class anchors exceeds %d per player (5.3)"),
            Composition.Wardens + (Composition.bBoss ? 1 : 0), Params.MaximumWardensPerPlayer * Players);
        return false;
    }
    if (Composition.Elites > GetMaximumElites(Players, Params))
    {
        OutReason = FString::Printf(TEXT("%d elites exceeds %d (5.3)"),
            Composition.Elites, GetMaximumElites(Players, Params));
        return false;
    }
    if (Composition.SpentBudget > Composition.Budget)
    {
        OutReason = FString::Printf(TEXT("spent %d of a %d budget"), Composition.SpentBudget, Composition.Budget);
        return false;
    }
    if (Composition.Elites + Composition.ModifierCarriers > Composition.Skitters)
    {
        OutReason = TEXT("more elite/modifier-carrier promotions than bodies to promote");
        return false;
    }
    if (Composition.Wave >= Params.VarietyEnforcedFromWave && Composition.Kind == EBreakerWaveKind::Standard
        && Composition.Budget > 0)
    {
        const int32 SkitterBudget = Composition.Skitters * Params.SkitterCost;
        const int32 Ceiling = FMath::FloorToInt32(
            Composition.Budget * FMath::Clamp(Params.MaximumSingleArchetypeShare, 0.0f, 1.0f));
        if (SkitterBudget > Ceiling)
        {
            OutReason = FString::Printf(TEXT("Skitters take %d of a %d budget, past the %d ceiling (4.3 variety)"),
                SkitterBudget, Composition.Budget, Ceiling);
            return false;
        }
    }

    OutReason.Reset();
    return true;
}

FString UBreakerWaveBudgetLibrary::DescribeComposition(const FBreakerWaveComposition& Composition)
{
    const TCHAR* KindName =
        Composition.Kind == EBreakerWaveKind::Boss ? TEXT("BOSS") :
        Composition.Kind == EBreakerWaveKind::Rest ? TEXT("REST") : TEXT("standard");
    return FString::Printf(
        TEXT("wave %d [%s] budget %d (spent %d, unspent %d) | skitter %d (elite %d x %d mods, carrier %d) | lattice %d | skirmisher %d | warden %d | boss %d | loot %s"),
        Composition.Wave, KindName, Composition.Budget, Composition.SpentBudget, Composition.UnspentBudget,
        Composition.Skitters, Composition.Elites, Composition.ModifiersPerElite, Composition.ModifierCarriers,
        Composition.Lattices, Composition.Skirmishers, Composition.Wardens, Composition.bBoss ? 1 : 0,
        Composition.bDropsLoot ? TEXT("yes") : TEXT("no"));
}
