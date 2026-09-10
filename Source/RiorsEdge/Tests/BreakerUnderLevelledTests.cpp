#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/BreakerMonsterChassis.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Game/BreakerZoneBuilder.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponMath.h"

// ---------------------------------------------------------------------------
// HOW MUCH HARDER IS CONTENT YOU ARRIVED AT UNDER-GEARED.
//
// Owner: "i cant make it to the end of the rift even with all those extra
// points because my base character is just so weak". Measured rather than
// tuned on that sentence, because the sentence has two readings and they want
// opposite fixes: either the character is weak everywhere, or he walked into
// content far above what he was carrying.
//
// RiorsEdge.Combat.PowerCurve.Composition already proves the first reading is
// wrong: monster health and weapon damage grow at the same rate, so a baseline
// character's time-to-kill against ON-LEVEL trash does not drift across the
// whole levelling game. Its own comment states the assumption that makes that
// true — "the player is carrying what this content drops" — and says a player
// who skips content SHOULD find it harder.
//
// So this measures the OTHER reading: the penalty for being under-levelled,
// printed as a multiple. It is a diagnostic, and it asserts only the shape it
// is confident of; the numbers it prints are for the owner to rule on.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerUnderLevelledTest,
    "RiorsEdge.Combat.PowerCurve.UnderLevelled",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
    // The same rifle base the composition test uses, so the two measurements
    // cannot disagree about what a baseline weapon is.
    constexpr float BreakerUnderLevelledArchetypeBase = 13.0f;

    float BreakerUnderLevelledShotsToKill(int32 ItemLevel, int32 AreaLevel,
        const FBreakerMonsterChassisParams& Params)
    {
        const float Health = UBreakerMonsterChassisLibrary::GetMonsterHealth(
            AreaLevel, EBreakerMonsterRank::Trash, Params);
        const float Damage = FBreakerWeaponMath::WeaponBaseDamage(
            BreakerUnderLevelledArchetypeBase, ItemLevel,
            GetDefault<UBreakerWeaponComponent>()->ItemLevelDamageGrowth);
        return Damage > UE_SMALL_NUMBER ? Health / Damage : 0.0f;
    }
}

bool FBreakerUnderLevelledTest::RunTest(const FString& Parameters)
{
    const FBreakerMonsterChassisParams Params;

    // THE FOUR PLACES THE CAMPAIGN ACTUALLY SENDS YOU, read from the zone
    // builder rather than restated, so a retune of any yard's level moves this
    // measurement with it.
    struct FStop
    {
        const TCHAR* Name;
        int32 AreaLevel;
    };
    const FStop Stops[] = {
        { TEXT("Fernhall entry"),   UBreakerZoneBuilder::FernhallYardAreaLevel(NAME_None) },
        { TEXT("Substation yard"),  UBreakerZoneBuilder::FernhallYardAreaLevel(FName(TEXT("substation"))) },
        { TEXT("Depot yard"),       UBreakerZoneBuilder::FernhallYardAreaLevel(FName(TEXT("depot"))) },
        { TEXT("Breach marshalling"), UBreakerZoneBuilder::FernhallYardAreaLevel(FName(TEXT("breach"))) },
    };

    // The gear a player is actually carrying when they first reach the entry
    // yard: the starter rifle itself, READ FROM THE SHIPPED ITEM rather than
    // restated. A diagnostic that keeps printing a number the game no longer
    // issues is worse than no diagnostic — this one said "starter i1" for one
    // run after the owner ruled the starter up, which is exactly the drift it
    // exists to catch in everything else.
    const int32 StarterItemLevel = UBreakerEquipmentComponent::MakeStarterRifle().ItemLevel;
    for (const FStop& Stop : Stops)
    {
        const int32 OnLevel = UBreakerMonsterChassisLibrary::GetDropItemLevel(Stop.AreaLevel);
        const float Geared = BreakerUnderLevelledShotsToKill(OnLevel, Stop.AreaLevel, Params);
        const float Starter = BreakerUnderLevelledShotsToKill(StarterItemLevel, Stop.AreaLevel, Params);
        const float Penalty = Geared > UE_SMALL_NUMBER ? Starter / Geared : 0.0f;
        AddInfo(FString::Printf(
            TEXT("UNDER-LEVELLED  %-20s area %3d | on-level gear i%-3d %5.1f shots | starter i%-2d %6.1f shots | %5.1fx"),
            Stop.Name, Stop.AreaLevel, OnLevel, Geared, StarterItemLevel, Starter, Penalty));

        // ON-LEVEL GEAR HOLDS ITS TIME-TO-KILL. This is the composition test's
        // claim, re-asserted here against the campaign's OWN stops rather than
        // against an abstract ladder — if a yard's authored level ever drifts
        // off the curve, this is where it shows.
        const float EntryGeared = BreakerUnderLevelledShotsToKill(
            UBreakerMonsterChassisLibrary::GetDropItemLevel(Stops[0].AreaLevel), Stops[0].AreaLevel, Params);
        TestTrue(*FString::Printf(TEXT("%s costs about the same in on-level gear"), Stop.Name),
            FMath::Abs(Geared - EntryGeared) < EntryGeared * 0.25f);

        // AND UNDER-GEARED IS STRICTLY WORSE, MONOTONICALLY. The penalty must
        // never fall as the content gets deeper, or arriving early at the
        // hardest place in the game would be the cheapest mistake in it.
        TestTrue(*FString::Printf(TEXT("%s punishes under-levelled gear"), Stop.Name), Penalty >= 1.0f);
    }

    // THE SHAPE THE OWNER RAN INTO. The deepest stop against starter gear,
    // relative to the shallowest: this single number is what "my base
    // character is just so weak" measures out to when the character is where
    // he was rather than where the content assumes.
    {
        const float ShallowStarter = BreakerUnderLevelledShotsToKill(StarterItemLevel, Stops[0].AreaLevel, Params);
        const float DeepStarter = BreakerUnderLevelledShotsToKill(
            StarterItemLevel, Stops[UE_ARRAY_COUNT(Stops) - 1].AreaLevel, Params);
        const float Ratio = ShallowStarter > UE_SMALL_NUMBER ? DeepStarter / ShallowStarter : 0.0f;
        AddInfo(FString::Printf(
            TEXT("UNDER-LEVELLED  starter gear costs %.1fx as many shots in the Breach as in the entry yard"),
            Ratio));
        TestTrue(TEXT("the deepest content is harder in starter gear than the shallowest"), Ratio > 1.0f);

        // AND THE STARTER IS ON-CURVE FOR THE YARD IT IS ISSUED IN. Owner-ruled
        // ("lets just raise the damage of the starter rifle"): the first fight
        // in the game used to cost 23.9 shots against the 16.9 that on-level
        // gear pays everywhere else. If the entry yard's level is ever retuned
        // and the starter is not, this is where it says so.
        const int32 EntryOnLevel = UBreakerMonsterChassisLibrary::GetDropItemLevel(Stops[0].AreaLevel);
        TestEqual(TEXT("the starter rifle is on-level for the yard it is issued in"),
            StarterItemLevel, EntryOnLevel);
    }
    return true;
}

#endif   // WITH_DEV_AUTOMATION_TESTS
