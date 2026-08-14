#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/BreakerEnemyFamily.h"
#include "Combat/BreakerEnemyModifiers.h"
#include "Combat/BreakerMonsterChassis.h"

// The modifier layer's DECISIONS are pure maths and are all tested here: the
// composition rules, the selection sweep, the family scoping, the chassis
// composition, and the O27 invariant that no modifier can read the player.
//
// What no test can prove is whether a halo actually reads from 20 m in an
// untextured graybox, which is Encounter-Design §1.2's first acceptance test.

using EMods = UBreakerEnemyModifierLibrary;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerModifierCompositionTest,
    "RiorsEdge.Combat.Modifiers.Composition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerModifierCompositionTest::RunTest(const FString& Parameters)
{
    FString Reason;

    // The empty set is legal: an unmodified trash mob is the common case and
    // must not be an error.
    TestTrue(TEXT("No modifiers is a legal set"), EMods::IsLegalModifierSet({}, Reason));

    // Every single modifier is legal on its own.
    for (const EBreakerEnemyModifier Modifier : EMods::GetAllModifiers())
    {
        TestTrue(FString::Printf(TEXT("%s alone is legal"), *EMods::GetModifierName(Modifier)),
            EMods::IsLegalModifierSet({ Modifier }, Reason));
        // ...and every one has a name, a counterplay line and a pressure kind.
        // A modifier with no name cannot be announced, and an unannounced
        // modifier is an unfair death (§1.2).
        TestTrue(FString::Printf(TEXT("Modifier %d has a display name"), static_cast<int32>(Modifier)),
            !EMods::GetModifierName(Modifier).IsEmpty());
        TestTrue(FString::Printf(TEXT("%s tells the player what to do"), *EMods::GetModifierName(Modifier)),
            !EMods::GetModifierCounterplay(Modifier).IsEmpty());
    }

    // §1.3's forbidden pairs, both orders.
    const TPair<EBreakerEnemyModifier, EBreakerEnemyModifier> Forbidden[] =
    {
        { EBreakerEnemyModifier::Warded,    EBreakerEnemyModifier::Reflective },
        { EBreakerEnemyModifier::Splitting, EBreakerEnemyModifier::Volatile   },
        { EBreakerEnemyModifier::Phasing,   EBreakerEnemyModifier::Cascading  },
        { EBreakerEnemyModifier::Wakeful,   EBreakerEnemyModifier::Splitting  },
    };
    for (const auto& Pair : Forbidden)
    {
        TestTrue(TEXT("Forbidden pair is symmetric"),
            EMods::AreModifiersForbiddenTogether(Pair.Key, Pair.Value)
            && EMods::AreModifiersForbiddenTogether(Pair.Value, Pair.Key));
        TestFalse(FString::Printf(TEXT("%s + %s is rejected"),
            *EMods::GetModifierName(Pair.Key), *EMods::GetModifierName(Pair.Value)),
            EMods::IsLegalModifierSet({ Pair.Key, Pair.Value }, Reason));
        TestTrue(TEXT("A rejection names the rule it broke"), !Reason.IsEmpty());
    }

    // A modifier with itself.
    TestFalse(TEXT("A duplicate modifier is rejected"),
        EMods::IsLegalModifierSet({ EBreakerEnemyModifier::Fleetfoot, EBreakerEnemyModifier::Fleetfoot }, Reason));

    // The count cap.
    TestFalse(TEXT("Four modifiers is rejected"),
        EMods::IsLegalModifierSet({ EBreakerEnemyModifier::Fleetfoot, EBreakerEnemyModifier::Volatile,
            EBreakerEnemyModifier::Warded, EBreakerEnemyModifier::Cascading }, Reason));

    // Required diversity: two modifiers from one pressure kind is one problem
    // stated twice.
    TestTrue(TEXT("Warded and Warding Aura are both Durability"),
        EMods::GetModifierPressure(EBreakerEnemyModifier::Warded) == EBreakerModifierPressure::Durability
        && EMods::GetModifierPressure(EBreakerEnemyModifier::WardingAura) == EBreakerModifierPressure::Durability);
    TestFalse(TEXT("Two Durability modifiers alone is rejected"),
        EMods::IsLegalModifierSet({ EBreakerEnemyModifier::Warded, EBreakerEnemyModifier::WardingAura }, Reason));
    TestTrue(TEXT("Durability plus Mobility is accepted"),
        EMods::IsLegalModifierSet({ EBreakerEnemyModifier::Warded, EBreakerEnemyModifier::Fleetfoot }, Reason));

    // ...and the anti-sponge rule: no 3-set may hold two Durability entries,
    // even when the third supplies diversity.
    TestFalse(TEXT("A 3-set with two Durability entries is rejected"),
        EMods::IsLegalModifierSet({ EBreakerEnemyModifier::Warded, EBreakerEnemyModifier::WardingAura,
            EBreakerEnemyModifier::Fleetfoot }, Reason));
    TestTrue(TEXT("A 3-set across three pressure kinds is accepted"),
        EMods::IsLegalModifierSet({ EBreakerEnemyModifier::Warded, EBreakerEnemyModifier::Fleetfoot,
            EBreakerEnemyModifier::Volatile }, Reason));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerModifierSelectionSweepTest,
    "RiorsEdge.Combat.Modifiers.SelectionSweep",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerModifierSelectionSweepTest::RunTest(const FString& Parameters)
{
    // Encounter-Design §1.5's acceptance criterion, verbatim: "Forbidden pairs
    // never generate. Automated test asserts this over 10,000 rolls."
    const FBreakerEnemyModifierParams Params;
    int32 CountHistogram[4] = { 0, 0, 0, 0 };
    int32 SeenModifiers = 0;
    TSet<EBreakerEnemyModifier> Seen;

    // Swept across BOTH families, because family scoping (Story-Source §1.5)
    // means neither family alone can reach every modifier: a Vestige can never
    // roll Anchored or Warding Aura, and an Altered can never roll Splitting,
    // Phasing or Reflective. Sweeping one family and asserting reachability
    // would fail for a correct reason, and sweeping only its own eight would
    // hide a genuinely unreachable entry in the other pool.
    for (const EBreakerEnemyFamily Family : { EBreakerEnemyFamily::Vestige, EBreakerEnemyFamily::Altered })
    {
        for (int32 Seed = 0; Seed < 10000; ++Seed)
        {
            const TArray<EBreakerEnemyModifier> Rolled = EMods::RollModifiers(Seed, Params, Family);
            FString Reason;
            if (!EMods::IsLegalModifierSetForFamily(Rolled, Family, Reason))
            {
                AddError(FString::Printf(TEXT("Family %s seed %d produced an illegal set: %s"),
                    *UBreakerEnemyFamilyLibrary::GetFamilyName(Family), Seed, *Reason));
                return false;
            }
            if (Family == EBreakerEnemyFamily::Vestige)
            {
                CountHistogram[FMath::Clamp(Rolled.Num(), 0, 3)] += 1;
                SeenModifiers += Rolled.Num();
            }
            for (const EBreakerEnemyModifier Modifier : Rolled) Seen.Add(Modifier);
        }
    }

    // Every roll produces at least one modifier: the caller has already decided
    // this enemy is modifier-bearing, and a zero-modifier result would silently
    // downgrade it to trash.
    TestEqual(TEXT("No roll produces an empty set"), CountHistogram[0], 0);
    TestTrue(TEXT("Single-modifier enemies are the common case"),
        CountHistogram[1] > CountHistogram[2] && CountHistogram[2] > CountHistogram[3]);
    TestTrue(TEXT("Three-modifier enemies exist but are rare"),
        CountHistogram[3] > 0 && CountHistogram[3] < CountHistogram[1] / 4);
    // Every authored modifier must be reachable. A modifier that can never roll
    // is content nobody will ever see, and the weight table is exactly where
    // that goes wrong silently.
    TestEqual(TEXT("Every modifier is reachable on some family"),
        Seen.Num(), EMods::GetAllModifiers().Num());
    TestTrue(TEXT("The sweep actually granted modifiers"), SeenModifiers > 10000);

    // Determinism: the same seed always produces the same set. The sweep above
    // is only meaningful if a failure is reproducible.
    const TArray<EBreakerEnemyModifier> First = EMods::RollModifiers(1234, Params);
    const TArray<EBreakerEnemyModifier> Second = EMods::RollModifiers(1234, Params);
    TestTrue(TEXT("The same seed rolls the same set"), First == Second);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerModifierFamilyScopeTest,
    "RiorsEdge.Combat.Modifiers.FamilyScope",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerModifierFamilyScopeTest::RunTest(const FString& Parameters)
{
    // Story-Source §1.5: a Vestige has "no tactics that resemble a military",
    // so a modifier that reads as tactical discipline must never appear on one.
    TestFalse(TEXT("A Vestige never plants its stance"),
        EMods::IsModifierAllowedOnFamily(EBreakerEnemyModifier::Anchored, EBreakerEnemyFamily::Vestige));
    TestFalse(TEXT("A Vestige never protects the squad"),
        EMods::IsModifierAllowedOnFamily(EBreakerEnemyModifier::WardingAura, EBreakerEnemyFamily::Vestige));
    TestTrue(TEXT("An Altered may plant its stance"),
        EMods::IsModifierAllowedOnFamily(EBreakerEnemyModifier::Anchored, EBreakerEnemyFamily::Altered));

    // ...and the reverse: alien-body rules do not belong on something that was
    // a person.
    TestFalse(TEXT("An Altered does not segment into copies"),
        EMods::IsModifierAllowedOnFamily(EBreakerEnemyModifier::Splitting, EBreakerEnemyFamily::Altered));
    TestFalse(TEXT("An Altered does not phase out of the world"),
        EMods::IsModifierAllowedOnFamily(EBreakerEnemyModifier::Phasing, EBreakerEnemyFamily::Altered));

    // Things a body does are legal on both.
    for (const EBreakerEnemyModifier Modifier : { EBreakerEnemyModifier::Warded, EBreakerEnemyModifier::Volatile,
        EBreakerEnemyModifier::Fleetfoot, EBreakerEnemyModifier::Cascading, EBreakerEnemyModifier::Wakeful })
    {
        TestTrue(FString::Printf(TEXT("%s is legal on either family"), *EMods::GetModifierName(Modifier)),
            EMods::IsModifierAllowedOnFamily(Modifier, EBreakerEnemyFamily::Vestige)
            && EMods::IsModifierAllowedOnFamily(Modifier, EBreakerEnemyFamily::Altered));
    }

    // Both families keep enough of a pool to roll a full 3-set, or the scoping
    // would have quietly deleted the Champion tier for one of them.
    const FBreakerEnemyModifierParams Params;
    for (const EBreakerEnemyFamily Family : { EBreakerEnemyFamily::Vestige, EBreakerEnemyFamily::Altered })
    {
        bool bSawThree = false;
        for (int32 Seed = 0; Seed < 4000; ++Seed)
        {
            const TArray<EBreakerEnemyModifier> Rolled = EMods::RollModifierSet(Seed, 3, Params, Family);
            FString Reason;
            if (!EMods::IsLegalModifierSetForFamily(Rolled, Family, Reason))
            {
                AddError(FString::Printf(TEXT("Family %s seed %d produced an illegal set: %s"),
                    *UBreakerEnemyFamilyLibrary::GetFamilyName(Family), Seed, *Reason));
                return false;
            }
            if (Rolled.Num() == 3) bSawThree = true;
        }
        TestTrue(FString::Printf(TEXT("%s can still reach a 3-modifier set"),
            *UBreakerEnemyFamilyLibrary::GetFamilyName(Family)), bSawThree);
    }

    // The behavioural contracts of the severance spectrum.
    TestFalse(TEXT("A Vestige never takes cover, whatever stage is set on it"),
        UBreakerEnemyFamilyLibrary::StageUsesCover(EBreakerEnemyFamily::Vestige, EBreakerSeveranceStage::Early));
    TestFalse(TEXT("A Vestige never flinches"),
        UBreakerEnemyFamilyLibrary::StageFlinches(EBreakerEnemyFamily::Vestige, EBreakerSeveranceStage::Early));
    TestTrue(TEXT("An early-severance Altered takes cover and flinches"),
        UBreakerEnemyFamilyLibrary::StageUsesCover(EBreakerEnemyFamily::Altered, EBreakerSeveranceStage::Early)
        && UBreakerEnemyFamilyLibrary::StageFlinches(EBreakerEnemyFamily::Altered, EBreakerSeveranceStage::Early));
    TestFalse(TEXT("A late-severance Altered does neither"),
        UBreakerEnemyFamilyLibrary::StageUsesCover(EBreakerEnemyFamily::Altered, EBreakerSeveranceStage::Late));
    // A Vestige's readout never claims a severance stage: it was never a
    // person, so it never degraded from anything.
    TestEqual(TEXT("A Vestige banner carries no stage"),
        UBreakerEnemyFamilyLibrary::GetFamilyBanner(EBreakerEnemyFamily::Vestige, EBreakerSeveranceStage::Early),
        FString(TEXT("VESTIGE")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerModifierStackingBoundsTest,
    "RiorsEdge.Combat.Modifiers.StackingBounds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerModifierStackingBoundsTest::RunTest(const FString& Parameters)
{
    FBreakerEnemyModifierParams Params;

    // §1.1's health step: 2.0x base, +0.35x per modifier beyond the first.
    TestEqual(TEXT("Zero modifiers costs nothing"), EMods::GetModifierCountHealthMultiplier(0, Params), 1.0f);
    TestEqual(TEXT("One modifier costs nothing extra"), EMods::GetModifierCountHealthMultiplier(1, Params), 1.0f);
    TestEqual(TEXT("Two modifiers is +0.35x"), EMods::GetModifierCountHealthMultiplier(2, Params), 1.35f, 0.0001f);
    TestEqual(TEXT("Three modifiers is +0.70x"), EMods::GetModifierCountHealthMultiplier(3, Params), 1.70f, 0.0001f);

    // The step is ADDITIVE in count, never geometric. A geometric step would
    // make a 3-modifier Champion a sponge, which is exactly what O27 forbids
    // and what §1.3's Durability rule exists to prevent from the other side.
    const float Two = EMods::GetModifierCountHealthMultiplier(2, Params) - 1.0f;
    const float Three = EMods::GetModifierCountHealthMultiplier(3, Params) - 1.0f;
    TestEqual(TEXT("The health step is linear in modifier count"), Three, Two * 2.0f, 0.0001f);

    // A 3-modifier Champion must stay well under the BOSS chassis row, or the
    // ranks stop meaning anything.
    FBreakerMonsterChassisParams Chassis;
    const float Champion = UBreakerMonsterChassisLibrary::GetMonsterHealth(
        20, EBreakerMonsterRank::ModifierBearing, Chassis, EMods::GetModifierCountHealthMultiplier(3, Params));
    const float Boss = UBreakerMonsterChassisLibrary::GetMonsterHealth(20, EBreakerMonsterRank::Boss, Chassis);
    const float Trash = UBreakerMonsterChassisLibrary::GetMonsterHealth(20, EBreakerMonsterRank::Trash, Chassis);
    TestTrue(TEXT("A 3-modifier Champion is nowhere near boss health"), Champion < Boss * 0.4f);
    TestTrue(TEXT("A 3-modifier Champion is still meaningfully above trash"), Champion > Trash * 3.0f);
    // O27: difficulty lives in the modifiers, not in the health bar. The bound
    // that actually holds against the authored values is that stacking three
    // modifiers never DOUBLES the health of a one-modifier enemy — a 3-modifier
    // Champion is a harder puzzle, not a second health bar.
    //
    // Recorded because it is not what a first reading of §1.1 suggests: at the
    // shipped rank row (2.5x) and step (+0.35), the count step contributes MORE
    // health than the rank promotion itself does (+1.75x trash against +1.5x).
    // That is a property of the authored numbers, not of this code, and O2
    // freezes both — flagged in the as-built section of Encounter-Design rather
    // than silently retuned here.
    const float ModifierRank = UBreakerMonsterChassisLibrary::GetMonsterHealth(
        20, EBreakerMonsterRank::ModifierBearing, Chassis);
    TestTrue(TEXT("Three modifiers never doubles a one-modifier enemy's health"),
        Champion < ModifierRank * 2.0f);

    // Rank derivation (O9 mapped onto the code enum).
    TestTrue(TEXT("Zero modifiers is Trash"),
        EMods::GetRankForModifierCount(0) == EBreakerMonsterRank::Trash);
    TestTrue(TEXT("Any modifier promotes to ModifierBearing"),
        EMods::GetRankForModifierCount(1) == EBreakerMonsterRank::ModifierBearing
        && EMods::GetRankForModifierCount(3) == EBreakerMonsterRank::ModifierBearing);

    // A zeroed count-weight block still yields a modifier-bearing enemy rather
    // than silently producing trash.
    Params.WeightOneModifier = 0.0f;
    Params.WeightTwoModifiers = 0.0f;
    Params.WeightThreeModifiers = 0.0f;
    TestEqual(TEXT("A zeroed weight table still grants one modifier"),
        EMods::RollModifierCount(7, Params), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerModifierCannotReadPlayerTest,
    "RiorsEdge.Combat.Modifiers.CannotReadPlayer",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerModifierCannotReadPlayerTest::RunTest(const FString& Parameters)
{
    // THE O27 INVARIANT. "Nothing you write may read the player's level, gear
    // or build to decide difficulty."
    //
    // The structural half is the header: every function in
    // UBreakerEnemyModifierLibrary takes only a modifier, the authored params,
    // and ENEMY-side numbers, so there is no parameter a player value could
    // arrive through. This test covers the behavioural half — that the two
    // magnitudes which ARE numbers derive from the monster and from nothing
    // else, and that everything else is a constant of the params block.
    const FBreakerEnemyModifierParams Params;

    // (1) Volatile. Encounter-Design §1.2 authors it as "45% of PLAYER max
    // health", which is a player read. It is re-expressed as a multiple of the
    // MONSTER's chassis damage, so it must scale with the monster's attack and
    // with nothing else.
    const float WeakMonster = EMods::GetVolatileDetonationDamage(10.0f, Params);
    const float StrongMonster = EMods::GetVolatileDetonationDamage(40.0f, Params);
    TestEqual(TEXT("Volatile scales linearly with the MONSTER's own attack"),
        StrongMonster, WeakMonster * 4.0f, 0.001f);
    TestEqual(TEXT("A monster that deals no damage detonates for nothing"),
        EMods::GetVolatileDetonationDamage(0.0f, Params), 0.0f);

    // (2) Reflective. §1.2 caps it at "6% of PLAYER max health"; it is capped
    // against the MONSTER's max health instead. The cap is the load-bearing
    // part: uncapped, it is a tax on high-DPS builds for existing.
    const float Cap = 500.0f * Params.ReflectCapFractionOfMonsterHealth;
    TestEqual(TEXT("A small hit reflects its authored fraction"),
        EMods::GetReflectDamage(100.0f, 500.0f, Params), 100.0f * Params.ReflectFraction, 0.001f);
    TestEqual(TEXT("A huge hit is capped by MONSTER health, not by the hit"),
        EMods::GetReflectDamage(1000000.0f, 500.0f, Params), Cap, 0.001f);
    TestTrue(TEXT("A tougher monster reflects a higher ceiling"),
        EMods::GetReflectDamage(1000000.0f, 5000.0f, Params) > EMods::GetReflectDamage(1000000.0f, 500.0f, Params));

    // (3) Everything else is a RULE and therefore a constant. These are the
    // magnitudes a player-scaling bug would have to hide in, and none of them
    // has an input a player value could travel through: the ward is a fraction
    // of the monster's own health, the blink is a fixed distance, the hazard is
    // a fraction of the monster's own attack.
    TestEqual(TEXT("The ward is a fraction of the MONSTER's health"),
        EMods::GetWardShieldAmount(1000.0f, Params), 1000.0f * Params.WardShieldFractionOfHealth, 0.001f);
    TestEqual(TEXT("The hazard tick is a fraction of the MONSTER's attack"),
        EMods::GetHazardTickDamage(20.0f, Params), 20.0f * Params.HazardTickDamageAsAttackFraction, 0.001f);

    // (4) The blink geometry: distance only, and it can never end inside the
    // standoff. A blink that arrives in melee range is a teleport-and-hit, and
    // O1's passive defence has no answer to one.
    const FVector Target(0.0f, 0.0f, 0.0f);
    const FVector Far(3000.0f, 0.0f, 0.0f);
    const FVector AfterFar = EMods::GetPhaseDestination(Far, Target, Params);
    TestEqual(TEXT("A far blink covers exactly the authored distance"),
        static_cast<float>(FVector::Dist2D(Far, AfterFar)), Params.PhaseDistanceCm, 0.01f);

    const FVector Near(Params.PhaseMinimumStandoffCm + 100.0f, 0.0f, 0.0f);
    const FVector AfterNear = EMods::GetPhaseDestination(Near, Target, Params);
    TestTrue(TEXT("A blink never ends inside the standoff"),
        FVector::Dist2D(AfterNear, Target) >= Params.PhaseMinimumStandoffCm - 0.01f);
    const FVector Inside(Params.PhaseMinimumStandoffCm * 0.5f, 0.0f, 0.0f);
    TestTrue(TEXT("A blink from inside the standoff does not move at all"),
        EMods::GetPhaseDestination(Inside, Target, Params).Equals(Inside, 0.01f));
    TestTrue(TEXT("A blink onto its own position is not a divide by zero"),
        !EMods::GetPhaseDestination(Target, Target, Params).ContainsNaN());

    // (5) Volatile falloff: full inside, zero outside, monotonic between. A
    // detonation that ticks for a sliver at 30 m is a detonation nobody can
    // learn the radius of.
    TestEqual(TEXT("Full damage at the epicentre"), EMods::GetVolatileFalloff(0.0f, Params), 1.0f);
    TestEqual(TEXT("Full damage at the inner radius"),
        EMods::GetVolatileFalloff(Params.VolatileInnerRadiusCm, Params), 1.0f);
    TestEqual(TEXT("Exactly zero at the outer radius"),
        EMods::GetVolatileFalloff(Params.VolatileOuterRadiusCm, Params), 0.0f);
    TestEqual(TEXT("Exactly zero well beyond it"), EMods::GetVolatileFalloff(100000.0f, Params), 0.0f);
    float Previous = 2.0f;
    for (int32 Step = 0; Step <= 20; ++Step)
    {
        const float Falloff = EMods::GetVolatileFalloff(Params.VolatileOuterRadiusCm * Step / 20.0f, Params);
        TestTrue(TEXT("Falloff never rises with distance"), Falloff <= Previous + KINDA_SMALL_NUMBER);
        Previous = Falloff;
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerModifierAnnouncementTest,
    "RiorsEdge.Combat.Modifiers.Announcement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerModifierAnnouncementTest::RunTest(const FString& Parameters)
{
    // Encounter-Design §1.2 test 1 and §1.5's first acceptance criterion: a
    // modifier must be identifiable before it matters. Automation cannot check
    // that a halo reads on a screen, but it CAN check that an enemy carrying
    // modifiers always has something to say and that an unmodified one is
    // byte-identical to before the system existed.
    TestTrue(TEXT("An unmodified enemy announces nothing at all"),
        EMods::GetModifierBanner({}).IsEmpty());

    for (const EBreakerEnemyModifier Modifier : EMods::GetAllModifiers())
    {
        const FString Banner = EMods::GetModifierBanner({ Modifier });
        TestFalse(FString::Printf(TEXT("%s produces a banner"), *EMods::GetModifierName(Modifier)),
            Banner.IsEmpty());
        TestEqual(TEXT("A single modifier's banner is its name"), Banner, EMods::GetModifierName(Modifier));

        // Every modifier has a distinct tell colour, and none of them is
        // saturated teal (O19 reserves that for rift objects and suppression
        // hardware, and a modifier halo is neither).
        const FLinearColor Color = EMods::GetModifierColor(Modifier);
        const bool bSaturatedTeal = Color.G > 0.7f && Color.B > 0.7f && Color.R < 0.2f;
        TestFalse(FString::Printf(TEXT("%s does not use reserved saturated teal"),
            *EMods::GetModifierName(Modifier)), bSaturatedTeal);
    }

    // A multi-modifier banner names every one of them: reading two of three is
    // how a player dies to the one they did not see.
    const FString Three = EMods::GetModifierBanner(
        { EBreakerEnemyModifier::Warded, EBreakerEnemyModifier::Fleetfoot, EBreakerEnemyModifier::Volatile });
    TestTrue(TEXT("A 3-modifier banner names all three"),
        Three.Contains(TEXT("WARDED")) && Three.Contains(TEXT("FLEETFOOT")) && Three.Contains(TEXT("VOLATILE")));
    return true;
}

#endif
