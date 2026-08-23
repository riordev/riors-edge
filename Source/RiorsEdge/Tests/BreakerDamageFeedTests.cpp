#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UI/BreakerDamageFeed.h"
#include "Weapons/BreakerWeaponArchetype.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponDefinition.h"

// ---------------------------------------------------------------------------
// THE DAMAGE-NUMBER FEED (art-and-ui)
// ---------------------------------------------------------------------------
// UI.Damage.Aggregation and UI.Damage.Cap were asserted invariants with no
// tests, and the aggregation they describe was already built — which is exactly
// the state that hides defects. Three of the rule's four clauses were wrong in
// the shipped tuning and none of it was reachable by any instrument: the merge
// lived inside a Canvas draw, and the capture harness cannot pull a trigger.
//
// These assert the rules against the SHIPPED WEAPON TABLE rather than against
// the feed's own constants, because a window is only right or wrong relative to
// the cadences it has to separate. Reading 0.12 back out of the header would
// assert that 0.12 equals 0.12.
namespace BreakerDamageFeedTest
{
    // Distinctively named for the unity build.
    float BreakerDamageFeedShotInterval(EBreakerWeaponArchetype Archetype)
    {
        UBreakerWeaponComponent* Weapon = NewObject<UBreakerWeaponComponent>();
        Weapon->EquipArchetype(Archetype);
        const UBreakerWeaponDefinition* Definition = Weapon->GetActiveDefinition();
        if (!Definition || Definition->RoundsPerMinute <= 0.0f) return 0.0f;
        return 60.0f / Definition->RoundsPerMinute;
    }

    BreakerDamageFeed::FMergeKey BreakerDamageFeedKey(const void* Target, bool bDoT = false,
        bool bCrit = false, bool bWeak = false)
    {
        BreakerDamageFeed::FMergeKey Key;
        Key.Target = Target;
        Key.bFromDoT = bDoT;
        Key.bCritical = bCrit;
        Key.bWeakPoint = bWeak;
        return Key;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerDamageAggregationTest,
    "RiorsEdge.UI.Damage.Aggregation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerDamageAggregationTest::RunTest(const FString& Parameters)
{
    using namespace BreakerDamageFeedTest;
    // Two distinct non-null targets. Only their identity matters here.
    const int32 TargetA = 0, TargetB = 0;
    const void* A = &TargetA;
    const void* B = &TargetB;

    // --- SIMULTANEITY MERGES, which is the clause the rule exists for -------
    // "multishot, pierce and pellets otherwise produce a wall of digits".
    TestTrue(TEXT("Two pellets of one shotgun blast are one number"),
        BreakerDamageFeed::ShouldMerge(BreakerDamageFeedKey(A), 100.0, BreakerDamageFeedKey(A), 100.0));

    const float BurstInterval = BreakerDamageFeedShotInterval(EBreakerWeaponArchetype::BurstRifle);
    TestTrue(TEXT("The burst rifle has a readable in-burst cadence"), BurstInterval > 0.0f);
    TestTrue(*FString::Printf(TEXT("Two rounds inside one burst (%.4fs apart) are one number"), BurstInterval),
        BreakerDamageFeed::ShouldMerge(BreakerDamageFeedKey(A), 100.0,
            BreakerDamageFeedKey(A), 100.0 + BurstInterval));

    // --- AND DELIBERATE FIRE DOES NOT -------------------------------------
    // The old shared window was 0.18s against a Sidearm firing every 0.143s
    // semi-automatically, so two aimed pulls became one number while the
    // constant's own comment promised they would not.
    const float SidearmInterval = BreakerDamageFeedShotInterval(EBreakerWeaponArchetype::Sidearm);
    TestTrue(TEXT("The sidearm has a readable cadence"), SidearmInterval > 0.0f);
    TestFalse(*FString::Printf(
        TEXT("Two deliberate semi-automatic shots (%.4fs apart) read as two numbers"), SidearmInterval),
        BreakerDamageFeed::ShouldMerge(BreakerDamageFeedKey(A), 100.0,
            BreakerDamageFeedKey(A), 100.0 + SidearmInterval));

    // --- THE WINDOW IS MEASURED FROM BIRTH --------------------------------
    // It used to refresh on every merge, so a held SMG trigger produced ONE
    // number that swallowed a whole magazine and never expired while the
    // trigger was down. Simulated the way the bug actually happened: repeated
    // hits one SMG interval apart, each inside the previous gap.
    const float SmgInterval = BreakerDamageFeedShotInterval(EBreakerWeaponArchetype::SMG);
    const double Birth = 100.0;
    double Now = Birth;
    int32 Merged = 0;
    for (int32 Round = 0; Round < 40; ++Round)
    {
        Now += SmgInterval;
        if (BreakerDamageFeed::ShouldMerge(BreakerDamageFeedKey(A), Birth, BreakerDamageFeedKey(A), Now)) ++Merged;
    }
    TestTrue(*FString::Printf(
        TEXT("A held trigger stops merging into the same number (%d of 40 rounds merged)"), Merged),
        Merged < 40);
    AddInfo(FString::Printf(
        TEXT("DAMAGE FEED  sidearm %.4fs, burst %.4fs, SMG %.4fs; direct window %.3fs, DoT window %.3fs"),
        SidearmInterval, BurstInterval, SmgInterval,
        BreakerDamageFeed::DirectMergeWindow, BreakerDamageFeed::DoTMergeWindow));

    // --- DAMAGE OVER TIME GETS THE LONGER WINDOW --------------------------
    // "Damage-over-time ticks aggregate on a LONGER window." One shared 0.18s
    // against a 0.5s Bleed tick meant a DoT could never merge with itself.
    TestTrue(TEXT("The DoT window is longer than the direct window"),
        BreakerDamageFeed::DoTMergeWindow > BreakerDamageFeed::DirectMergeWindow);

    const UBreakerWeaponDefinition* Smg = [&]
    {
        UBreakerWeaponComponent* Weapon = NewObject<UBreakerWeaponComponent>();
        Weapon->EquipArchetype(EBreakerWeaponArchetype::SMG);
        return Weapon->GetActiveDefinition();
    }();
    if (TestNotNull(TEXT("The SMG definition resolves"), Smg))
    {
        TestTrue(TEXT("The shipped Bleed actually ticks"), Smg->BleedTickInterval > 0.0f);
        TestTrue(*FString::Printf(
            TEXT("Two consecutive Bleed ticks (%.3fs apart) merge into one number"), Smg->BleedTickInterval),
            BreakerDamageFeed::ShouldMerge(BreakerDamageFeedKey(A, /*bDoT*/ true), 100.0,
                BreakerDamageFeedKey(A, true), 100.0 + Smg->BleedTickInterval));
        // The second half of the same bug: a number already dead when the next
        // tick lands cannot be merged into, whatever the window says.
        TestTrue(TEXT("A DoT number outlives its own merge window"),
            BreakerDamageFeed::MinimumDoTLifetime >= BreakerDamageFeed::DoTMergeWindow);
    }

    // --- THE READS THAT MUST NEVER BE AVERAGED AWAY -----------------------
    TestFalse(TEXT("A crit never merges into a body hit"),
        BreakerDamageFeed::ShouldMerge(BreakerDamageFeedKey(A), 100.0,
            BreakerDamageFeedKey(A, false, true, false), 100.0));
    TestFalse(TEXT("A weak point never merges into a body hit"),
        BreakerDamageFeed::ShouldMerge(BreakerDamageFeedKey(A), 100.0,
            BreakerDamageFeedKey(A, false, false, true), 100.0));
    TestFalse(TEXT("A DoT tick never merges into a direct hit"),
        BreakerDamageFeed::ShouldMerge(BreakerDamageFeedKey(A), 100.0,
            BreakerDamageFeedKey(A, true), 100.0));
    TestFalse(TEXT("Two targets are two numbers"),
        BreakerDamageFeed::ShouldMerge(BreakerDamageFeedKey(A), 100.0, BreakerDamageFeedKey(B), 100.0));
    // A destroyed actor leaves a null weak pointer, and two dead targets are
    // not the same target.
    TestFalse(TEXT("A number whose target has gone absorbs nothing"),
        BreakerDamageFeed::ShouldMerge(BreakerDamageFeedKey(nullptr), 100.0,
            BreakerDamageFeedKey(nullptr), 100.0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerDamageCapTest,
    "RiorsEdge.UI.Damage.Cap",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerDamageCapTest::RunTest(const FString& Parameters)
{
    // "A hard simultaneous cap culls OLDEST-FIRST." The ring evicted by write
    // cursor, which is insertion order, and merges refreshed numbers in place
    // without moving their slot.
    const TArray<double> Births = {100.0, 101.0, 99.0, 103.0};
    const TArray<double> LiveDeaths = {100.6, 101.6, 99.6, 103.6};

    // At t=103.7 everything is expired, so the oldest expired one goes.
    TestEqual(TEXT("Among expired entries the oldest is evicted"),
        BreakerDamageFeed::IndexToEvict(Births, LiveDeaths, 103.7), 2);

    // Nothing expired yet: still the genuinely oldest, not the first written.
    const TArray<double> FarDeaths = {200.0, 200.0, 200.0, 200.0};
    TestEqual(TEXT("With nothing expired the oldest live entry is evicted"),
        BreakerDamageFeed::IndexToEvict(Births, FarDeaths, 104.0), 2);

    // AN EXPIRED SLOT IS TAKEN BEFORE ANY LIVE ONE, whatever the birth order.
    // The old cursor could not do this at all: entries were never reclaimed, so
    // the buffer pinned at its cap forever after the first full pass and a lull
    // in the fight never cleared it.
    const TArray<double> MixedDeaths = {200.0, 101.5, 200.0, 200.0};
    TestEqual(TEXT("An expired slot is reclaimed before a live one is dropped"),
        BreakerDamageFeed::IndexToEvict(Births, MixedDeaths, 104.0), 1);

    TestEqual(TEXT("An empty feed evicts nothing"),
        BreakerDamageFeed::IndexToEvict({}, {}, 0.0), static_cast<int32>(INDEX_NONE));
    return true;
}

#endif
