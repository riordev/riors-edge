#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UI/BreakerBannerQueue.h"

// The banners' drawing cannot be tested — no viewport. Their ORDER, their
// stagger, their holds and their rectangles are pure functions in
// UI/BreakerBannerQueue.h (04-death-banners, O202), and this pins each by
// value so a retune that disagrees with the sheet fails here rather than in
// a screenshot.

namespace
{
    constexpr double BreakerBannerQueueTolerance = 0.001;

    FBreakerBanner BreakerBannerQueueMake(EBreakerBannerKind Kind)
    {
        FBreakerBanner Banner;
        Banner.Kind = Kind;
        return Banner;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerBannerQueueTest,
    "RiorsEdge.UI.BannerQueue",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerBannerQueueTest::RunTest(const FString& Parameters)
{
    using namespace BreakerBannerQueue;

    // --- Three enqueued the same frame, in the wrong order, arrive by priority
    {
        const double Now = 100.0;
        TArray<FBreakerBanner> Pending;
        Pending.Add(BreakerBannerQueueMake(EBreakerBannerKind::WaveClear));
        Pending.Add(BreakerBannerQueueMake(EBreakerBannerKind::LevelUp));
        Pending.Add(BreakerBannerQueueMake(EBreakerBannerKind::RiftComplete));
        const double Latest = ScheduleArrival(Pending, Now);

        double RiftAt = -1.0, LevelAt = -1.0, WaveAt = -1.0;
        for (const FBreakerBanner& Banner : Pending)
        {
            switch (Banner.Kind)
            {
            case EBreakerBannerKind::RiftComplete: RiftAt = Banner.ArriveAt; break;
            case EBreakerBannerKind::LevelUp:      LevelAt = Banner.ArriveAt; break;
            default:                               WaveAt = Banner.ArriveAt; break;
            }
        }
        TestEqual(TEXT("Rift complete arrives now"), RiftAt, Now, BreakerBannerQueueTolerance);
        TestEqual(TEXT("Level up arrives one stagger later"), LevelAt, Now + 0.3, BreakerBannerQueueTolerance);
        TestEqual(TEXT("Wave clear arrives two staggers later"), WaveAt, Now + 0.6, BreakerBannerQueueTolerance);
        TestEqual(TEXT("The latest arrival is the wave's"), Latest, WaveAt, BreakerBannerQueueTolerance);
        TestEqual(TEXT("Stagger 300 ms"), StaggerSeconds, 0.3f);

        // A second call schedules nothing new and moves nothing.
        ScheduleArrival(Pending, Now + 0.05);
        for (const FBreakerBanner& Banner : Pending)
        {
            TestTrue(TEXT("A scheduled arrival never moves"),
                Banner.ArriveAt == RiftAt || Banner.ArriveAt == LevelAt || Banner.ArriveAt == WaveAt);
        }

        // A fourth, enqueued later, lands after the last scheduled slot.
        Pending.Add(BreakerBannerQueueMake(EBreakerBannerKind::LevelUp));
        ScheduleArrival(Pending, Now + 0.1);
        TestEqual(TEXT("A later banner queues behind the last"), Pending.Last().ArriveAt, WaveAt + 0.3, BreakerBannerQueueTolerance);

        // One enqueued long after the queue has drained arrives at once.
        TArray<FBreakerBanner> Late;
        Late.Add(BreakerBannerQueueMake(EBreakerBannerKind::WaveClear));
        ScheduleArrival(Late, 500.0);
        TestEqual(TEXT("An empty queue arrives now"), Late[0].ArriveAt, 500.0, BreakerBannerQueueTolerance);

        // Pruning drops a banner once its own out has finished, and only
        // then. The wave (Now + 0.6, 1.88 total) is the first to finish; the
        // rift (Now, 2.68 total) and the first level (Now + 0.3, 2.28 total)
        // go together at Now + 2.68; the late level (Now + 0.9) outlives all.
        const double WaveDone = WaveAt + TotalSecondsFor(EBreakerBannerKind::WaveClear);
        Prune(Pending, WaveDone - 0.001);
        TestEqual(TEXT("Nothing pruned before the first out ends"), Pending.Num(), 4);
        Prune(Pending, WaveDone);
        TestEqual(TEXT("The wave banner is pruned on its last frame"), Pending.Num(), 3);
        Prune(Pending, RiftAt + TotalSecondsFor(EBreakerBannerKind::RiftComplete));
        TestEqual(TEXT("The rift and the first level are pruned together"), Pending.Num(), 1);
        TestTrue(TEXT("The late level outlives them"), Pending[0].Kind == EBreakerBannerKind::LevelUp);
    }

    // --- Holds, in, out
    TestEqual(TEXT("Rift complete holds 2.4"), HoldSecondsFor(EBreakerBannerKind::RiftComplete), 2.4f);
    TestEqual(TEXT("Level up holds 2.0"), HoldSecondsFor(EBreakerBannerKind::LevelUp), 2.0f);
    TestEqual(TEXT("Wave clear holds 1.6"), HoldSecondsFor(EBreakerBannerKind::WaveClear), 1.6f);
    TestEqual(TEXT("Wave clear slides in over 160 ms"), InSecondsFor(EBreakerBannerKind::WaveClear), 0.16f);
    TestEqual(TEXT("Out 120 ms"), OutSeconds, 0.12f);
    TestEqual(TEXT("Wave clear slides 16"), SlidePixels, 16.0f);

    // --- The out is a slide: displacement is full on frame zero, zero through
    // the hold, full again on the last frame. Never an alpha.
    {
        const EBreakerBannerKind Kind = EBreakerBannerKind::WaveClear;
        const float In = InSecondsFor(Kind);
        const float Hold = HoldSecondsFor(Kind);
        TestEqual(TEXT("Frame zero sits a full slide behind"), SlideDisplacementFor(Kind, 0.0f), SlidePixels, 0.001f);
        TestEqual(TEXT("Half in is half a slide"), SlideDisplacementFor(Kind, In * 0.5f), SlidePixels * 0.5f, 0.001f);
        TestEqual(TEXT("At rest through the hold"), SlideDisplacementFor(Kind, In + Hold * 0.5f), 0.0f, 0.001f);
        TestEqual(TEXT("Half out is half a slide back"), SlideDisplacementFor(Kind, In + Hold + OutSeconds * 0.5f), SlidePixels * 0.5f, 0.001f);
        TestFalse(TEXT("Gone after the out"), IsShowing(Kind, TotalSecondsFor(Kind)));
        TestTrue(TEXT("Showing on the last out frame"), IsShowing(Kind, TotalSecondsFor(Kind) - 0.001f));
        TestFalse(TEXT("Not showing before arrival"), IsShowing(Kind, -0.001f));
    }

    // --- The three rectangles are the sheet's and pairwise disjoint
    {
        const FRect Rift = RectFor(EBreakerBannerKind::RiftComplete);
        const FRect Level = RectFor(EBreakerBannerKind::LevelUp);
        const FRect Wave = RectFor(EBreakerBannerKind::WaveClear);
        TestEqual(TEXT("Wave clear at 760"), Wave.X, 760.0f);
        TestEqual(TEXT("Wave clear at 96"), Wave.Y, 96.0f);
        TestEqual(TEXT("Wave clear 400 wide"), Wave.W, 400.0f);
        TestEqual(TEXT("Wave clear 64 tall"), Wave.H, 64.0f);
        TestEqual(TEXT("Rift band at 0"), Rift.X, 0.0f);
        TestEqual(TEXT("Rift band at 440"), Rift.Y, 440.0f);
        TestEqual(TEXT("Rift band full width"), Rift.W, 1920.0f);
        TestEqual(TEXT("Rift band 200 tall"), Rift.H, 200.0f);
        TestEqual(TEXT("Level up at 1480"), Level.X, 1480.0f);
        TestEqual(TEXT("Level up at 200"), Level.Y, 200.0f);
        TestEqual(TEXT("Level up 400 wide"), Level.W, 400.0f);
        TestEqual(TEXT("Level up 88 tall"), Level.H, 88.0f);
        TestTrue(TEXT("Rift and level are disjoint"), RectsDisjoint(Rift, Level));
        TestTrue(TEXT("Rift and wave are disjoint"), RectsDisjoint(Rift, Wave));
        TestTrue(TEXT("Level and wave are disjoint"), RectsDisjoint(Level, Wave));
        TestFalse(TEXT("A rectangle is not disjoint from itself"), RectsDisjoint(Wave, Wave));
    }

    // --- Slide axes and the rail
    TestTrue(TEXT("Wave clear slides down"), SlideAxisFor(EBreakerBannerKind::WaveClear).Equals(FVector2D(0.0f, 1.0f)));
    TestTrue(TEXT("Rift complete slides from the left"), SlideAxisFor(EBreakerBannerKind::RiftComplete).Equals(FVector2D(1.0f, 0.0f)));
    TestTrue(TEXT("Level up slides from the right"), SlideAxisFor(EBreakerBannerKind::LevelUp).Equals(FVector2D(-1.0f, 0.0f)));
    TestTrue(TEXT("Only the level is gold"), RailIsGold(EBreakerBannerKind::LevelUp));
    TestFalse(TEXT("The rift is not gold"), RailIsGold(EBreakerBannerKind::RiftComplete));
    TestFalse(TEXT("The wave is not gold"), RailIsGold(EBreakerBannerKind::WaveClear));

    // --- Priority is enum order
    TestTrue(TEXT("Rift outranks level"), PriorityFor(EBreakerBannerKind::RiftComplete) < PriorityFor(EBreakerBannerKind::LevelUp));
    TestTrue(TEXT("Level outranks wave"), PriorityFor(EBreakerBannerKind::LevelUp) < PriorityFor(EBreakerBannerKind::WaveClear));

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
