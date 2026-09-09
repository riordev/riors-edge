#include "Misc/AutomationTest.h"
#include "Game/BreakerRiftDefinition.h"

#if WITH_DEV_AUTOMATION_TESTS

// ---------------------------------------------------------------------------
// A MAP'S SHAPE IS A FUNCTION OF THE MAP. The cover generator was always
// seeded, but from one authored constant — so every field it built, in every
// rift, in every session, came out identical. Two different rifts were the
// same room twice.
//
// The properties that matter are all here and all arithmetic: reproducible,
// distinct, level-sensitive, and inert where there is no rift.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerRiftLayoutSeedTest,
    "RiorsEdge.Game.RiftLayoutSeed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRiftLayoutSeedTest::RunTest(const FString&)
{
    constexpr int32 Base = 20260814;

    auto Rift = [](const TCHAR* Id, int32 Level)
    {
        FBreakerRiftDefinition R;
        R.EncounterId = FName(Id);
        R.AreaLevel = Level;
        return R;
    };

    // THE GYM MUST NOT MOVE. An unset rift is the ordinary field and the
    // instrument corridor; a layout that shifted under measurement would make
    // every density and cover reading incomparable with the last one.
    const FBreakerRiftDefinition None;
    TestFalse(TEXT("a default rift is unset"), None.IsSet());
    TestEqual(TEXT("no rift leaves the authored base untouched"), None.LayoutSeed(Base), Base);

    // REPRODUCIBLE: the same rift is the same room, so a layout in a bug
    // report can be re-opened rather than described.
    const FBreakerRiftDefinition A = Rift(TEXT("breach.marshalling"), 12);
    TestEqual(TEXT("the same rift seeds identically"), A.LayoutSeed(Base), A.LayoutSeed(Base));
    TestEqual(TEXT("and identically from an equal copy"),
        A.LayoutSeed(Base), Rift(TEXT("breach.marshalling"), 12).LayoutSeed(Base));

    // Fixed algorithm witness, independent of this process's FName allocation
    // order. This is a reproducibility contract, not an O2 gameplay magnitude.
    TestEqual(TEXT("canonical encounter text has a process-independent golden seed"),
        A.LayoutSeed(Base), 70439488);
    TestEqual(TEXT("FName identity remains case insensitive"),
        A.LayoutSeed(Base), Rift(TEXT("BREACH.MARSHALLING"), 12).LayoutSeed(Base));
    TestEqual(TEXT("set rift with empty identity preserves original zero-identity hash"),
        Rift(TEXT(""), 12).LayoutSeed(Base),
        static_cast<int32>(HashCombine(HashCombine(0u, GetTypeHash(12)), static_cast<uint32>(Base)) & 0x7fffffffu));

    // DISTINCT: two rifts are two places.
    const FBreakerRiftDefinition B = Rift(TEXT("fernhall.approach"), 12);
    TestNotEqual(TEXT("a different encounter is a different layout"), A.LayoutSeed(Base), B.LayoutSeed(Base));

    // LEVEL-SENSITIVE: the same place at a harder tier is a new arrangement,
    // not the same one with bigger numbers — the whole point of running a map
    // again at depth.
    TestNotEqual(TEXT("the same place at another level rearranges"),
        A.LayoutSeed(Base), Rift(TEXT("breach.marshalling"), 40).LayoutSeed(Base));

    // Always a legal FRandomStream seed. Negative is legal but reads as a
    // mistake in a log, and zero would silently mean "unseeded" to a reader.
    for (int32 Level = 1; Level <= 100; ++Level)
    {
        for (const TCHAR* Id : {TEXT("a"), TEXT("breach.marshalling"), TEXT("rift.deep.07"), TEXT("")})
        {
            const int32 Seed = Rift(Id, Level).LayoutSeed(Base);
            TestTrue(*FString::Printf(TEXT("seed is non-negative (%s, %d)"), Id, Level), Seed >= 0);
        }
    }

    // The base still participates, so a deliberate reseed of the whole game
    // moves every rift with it rather than leaving them pinned to their ids.
    TestNotEqual(TEXT("the authored base still moves a rift's layout"),
        A.LayoutSeed(Base), A.LayoutSeed(Base + 1));

    return true;
}

#endif
