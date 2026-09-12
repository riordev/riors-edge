#include "Misc/AutomationTest.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerWardenEnemy.h"
#include "Combat/BreakerSkirmisherEnemy.h"
#include "Combat/BreakerAlteredEnemy.h"
#include "Combat/BreakerHoldfastEnemy.h"
#include "Combat/BreakerModifierSeedMath.h"
#include "Misc/PackageName.h"
#include "Containers/Set.h"

#if WITH_DEV_AUTOMATION_TESTS

// O281: a body at rest idles, a body moving walks, a body struck flinches.
// Every enemy rig ships its idle, walk and hit cycles. Pinned on the SHIPPED
// CONFIGURATION, one CDO at a time, by name rather than by iterating the
// class tree: the ranged Lattice composes primitives and CLEARS its body by
// ruling, so it is excluded here on purpose and the cast pin in
// BreakerEnemyBodyTests keeps its own QuadShell line. The three paths are
// asserted (1) valid, (2) pairwise distinct — a Walk set as the idle was the
// exact defect: the mech stood on the spot, walking — (3) named for what they
// are, (4) under the same rig folder as the body they animate, and (5)
// RESOLVING, so a typo in a path is a red line here and not a T-pose in a
// screenshot. Not gated on the pack directory: the mechs are committed, and a
// missing cycle is the thing this ruling forbids.

namespace
{
    struct FBreakerGaitCastEntry
    {
        const TCHAR* Name;
        const ABreakerEnemy* Defaults;
    };

    FString BreakerRigFolderOf(const FSoftObjectPath& Path)
    {
        return FPackageName::GetLongPackagePath(Path.GetLongPackageName());
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerEnemyShippedGaitSetTest,
    "RiorsEdge.Enemy.Body.ShippedGaitSet",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerEnemyShippedGaitSetTest::RunTest(const FString& Parameters)
{
    const FBreakerGaitCastEntry Cast[] = {
        { TEXT("ABreakerEnemy"),           GetDefault<ABreakerEnemy>() },
        { TEXT("ABreakerWardenEnemy"),     GetDefault<ABreakerWardenEnemy>() },
        { TEXT("ABreakerSkirmisherEnemy"), GetDefault<ABreakerSkirmisherEnemy>() },
        { TEXT("ABreakerAlteredEnemy"),    GetDefault<ABreakerAlteredEnemy>() },
        { TEXT("ABreakerHoldfastEnemy"),   GetDefault<ABreakerHoldfastEnemy>() },
    };

    for (const FBreakerGaitCastEntry& Entry : Cast)
    {
        const FString Name(Entry.Name);
        TestNotNull(*FString::Printf(TEXT("%s has a CDO"), *Name), Entry.Defaults);
        if (!Entry.Defaults) continue;
        const ABreakerEnemy& D = *Entry.Defaults;

        // (1) All four paths are set.
        TestTrue(*FString::Printf(TEXT("%s ships a named body"), *Name), D.BodyMeshAsset.IsValid());
        TestTrue(*FString::Printf(TEXT("%s ships an idle"), *Name), D.BodyIdleAnimation.IsValid());
        TestTrue(*FString::Printf(TEXT("%s ships a walk"), *Name), D.BodyRunAnimation.IsValid());
        TestTrue(*FString::Printf(TEXT("%s ships a hit"), *Name), D.BodyHitAnimation.IsValid());
        if (!D.BodyMeshAsset.IsValid() || !D.BodyIdleAnimation.IsValid()
            || !D.BodyRunAnimation.IsValid() || !D.BodyHitAnimation.IsValid())
        {
            continue;
        }

        // (2) Pairwise distinct: the same sequence in two slots is one cycle
        // wearing two names, which is the defect this ruling closes.
        TestNotEqual(*FString::Printf(TEXT("%s's idle is not its walk"), *Name),
            D.BodyIdleAnimation.ToString(), D.BodyRunAnimation.ToString());
        TestNotEqual(*FString::Printf(TEXT("%s's idle is not its hit"), *Name),
            D.BodyIdleAnimation.ToString(), D.BodyHitAnimation.ToString());
        TestNotEqual(*FString::Printf(TEXT("%s's walk is not its hit"), *Name),
            D.BodyRunAnimation.ToString(), D.BodyHitAnimation.ToString());

        // (3) Named for what they are: the pack's own suffixes.
        const FString IdleName = D.BodyIdleAnimation.GetAssetName();
        const FString RunName = D.BodyRunAnimation.GetAssetName();
        const FString HitName = D.BodyHitAnimation.GetAssetName();
        TestTrue(*FString::Printf(TEXT("%s's idle ends in _Idle (%s)"), *Name, *IdleName),
            IdleName.EndsWith(TEXT("_Idle"), ESearchCase::CaseSensitive));
        TestTrue(*FString::Printf(TEXT("%s's gait is a _Walk or a _Run (%s)"), *Name, *RunName),
            RunName.Contains(TEXT("_Walk"), ESearchCase::CaseSensitive)
            || RunName.Contains(TEXT("_Run"), ESearchCase::CaseSensitive));
        TestTrue(*FString::Printf(TEXT("%s's hit is a HitRecieve (%s)"), *Name, *HitName),
            HitName.Contains(TEXT("HitRecieve"), ESearchCase::CaseSensitive));

        // (4) All three cycles live beside the body they animate: a Stan
        // cycle on a George skeleton is a rig mismatch, not a gait.
        const FString RigFolder = BreakerRigFolderOf(D.BodyMeshAsset);
        TestFalse(*FString::Printf(TEXT("%s's body has a rig folder"), *Name), RigFolder.IsEmpty());
        TestEqual(*FString::Printf(TEXT("%s's idle lives in its rig folder"), *Name),
            BreakerRigFolderOf(D.BodyIdleAnimation), RigFolder);
        TestEqual(*FString::Printf(TEXT("%s's walk lives in its rig folder"), *Name),
            BreakerRigFolderOf(D.BodyRunAnimation), RigFolder);
        TestEqual(*FString::Printf(TEXT("%s's hit lives in its rig folder"), *Name),
            BreakerRigFolderOf(D.BodyHitAnimation), RigFolder);

        // (5) Every path resolves. A typo is red here.
        TestNotNull(*FString::Printf(TEXT("%s's body resolves: %s"), *Name, *D.BodyMeshAsset.ToString()),
            D.BodyMeshAsset.TryLoad());
        TestNotNull(*FString::Printf(TEXT("%s's idle resolves: %s"), *Name, *D.BodyIdleAnimation.ToString()),
            D.BodyIdleAnimation.TryLoad());
        TestNotNull(*FString::Printf(TEXT("%s's walk resolves: %s"), *Name, *D.BodyRunAnimation.ToString()),
            D.BodyRunAnimation.TryLoad());
        TestNotNull(*FString::Printf(TEXT("%s's hit resolves: %s"), *Name, *D.BodyHitAnimation.ToString()),
            D.BodyHitAnimation.TryLoad());
    }
    return true;
}

// O281's other half: modifier rolls mix the session into the seed so the same
// body never wears the same set two runs running. BreakerModifierSeed::Mix is
// pure (BreakerModifierSeedMath.h), so the rule is proved without a world:
// the salt moves the seed, the site moves the seed, the mix is a function of
// its inputs, and a small site table under one salt has no collisions — two
// spawners in the same wave sharing a seed would share a modifier set, which
// is the "same set" complaint wearing a different coat.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerEnemyModifierSeedVariesBySessionTest,
    "RiorsEdge.Enemy.Modifiers.SeedVariesBySession",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerEnemyModifierSeedVariesBySessionTest::RunTest(const FString& Parameters)
{
    using BreakerModifierSeed::Mix;

    constexpr int32 Base = 12345; // O2 PLACEHOLDER — any spawn-table seed
    constexpr int32 Site = 7;     // O2 PLACEHOLDER — any site index

    // The session moves the seed.
    TestNotEqual(TEXT("salt 0 and salt 1 differ"), Mix(Base, 0, Site), Mix(Base, 1, Site));
    // The site moves the seed.
    TestNotEqual(TEXT("site 0 and site 1 differ"), Mix(Base, 0, 0), Mix(Base, 0, 1));
    // The mix is deterministic.
    TestEqual(TEXT("the same inputs mix to the same seed"), Mix(Base, 3, Site), Mix(Base, 3, Site));
    TestEqual(TEXT("zero inputs mix the same twice"), Mix(0, 0, 0), Mix(0, 0, 0));

    // 64 sites under one salt: no two spawners in a small table share a seed.
    {
        constexpr int32 SiteCount = 64; // O2 PLACEHOLDER — a small site table
        constexpr int32 Salt = 99;      // O2 PLACEHOLDER — any session
        TSet<int32> Seen;
        int32 Collisions = 0;
        for (int32 S = 0; S < SiteCount; ++S)
        {
            bool bAlready = false;
            Seen.Add(Mix(Base, Salt, S), &bAlready);
            if (bAlready) ++Collisions;
        }
        TestEqual(TEXT("64 sites under one salt yield 64 distinct seeds"), Collisions, 0);
        TestEqual(TEXT("the site set holds every seed"), Seen.Num(), SiteCount);
    }
    return true;
}

#endif
