#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Game/BreakerPocketRift.h"
#include "Game/BreakerPocketRiftMath.h"

// ---------------------------------------------------------------------------
// THE TEAR'S SHAPE AND ITS CLOCK.
//
// What is provable here is the silhouette and the pulse; what is NOT provable
// here is whether a tear reads as a rift, which is the owner's eye and the
// capture harness's job. So the assertions are about the properties that make
// the shape the shape — pointed at both ends, widest through the middle,
// symmetric — rather than about any particular width.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerPocketRiftTest,
    "RiorsEdge.Zone.PocketRift.Shape",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerPocketRiftTest::RunTest(const FString& Parameters)
{
    using namespace BreakerPocketRift;
    constexpr float Width = 150.0f;
    constexpr float Height = 280.0f;

    // POINTED AT BOTH ENDS. This is the whole difference between a tear and a
    // capsule: a shape with caps reads as an object, one that closes to a point
    // reads as a split in something.
    TestEqual(TEXT("the bottom is a point"), TearHalfWidth(0.0f, Width), 0.0f);
    TestEqual(TEXT("so is the top"), TearHalfWidth(1.0f, Width), 0.0f);
    // WIDEST THROUGH THE MIDDLE, and exactly half the authored width there, so
    // the dial means what it says.
    TestEqual(TEXT("the middle is the full width"), TearHalfWidth(0.5f, Width), Width * 0.5f, 0.01f);

    // SYMMETRIC and RISING to the middle. Asserted by sweep rather than at
    // three sample points: an exponent typo produces a shape that is still
    // zero at the ends and still half-width in the middle.
    float Previous = -1.0f;
    for (int32 Step = 0; Step <= 50; ++Step)
    {
        const float T = static_cast<float>(Step) / 100.0f;   // 0.00 .. 0.50
        const float Here = TearHalfWidth(T, Width);
        TestTrue(*FString::Printf(TEXT("the tear widens toward the middle at t=%.2f"), T), Here >= Previous);
        TestEqual(*FString::Printf(TEXT("and is symmetric about it at t=%.2f"), T),
            TearHalfWidth(1.0f - T, Width), Here, 0.01f);
        Previous = Here;
    }

    // A ZERO OR NEGATIVE WIDTH IS A FLAT TEAR, not a mirrored one: a negative
    // half-width would fold the two edges through each other.
    TestEqual(TEXT("a zero width has no shape"), TearHalfWidth(0.5f, 0.0f), 0.0f);
    TestEqual(TEXT("and a negative one does not invert"), TearHalfWidth(0.5f, -80.0f), 0.0f);

    // THE EDGE POINTS. Flat in X, because the actor's own rotation is what aims
    // the tear — a shape with thickness of its own could not be turned to face
    // the walk without the two disagreeing.
    const FVector Left = TearEdgePoint(0.5f, Width, Height, false);
    const FVector Right = TearEdgePoint(0.5f, Width, Height, true);
    TestEqual(TEXT("the tear is flat"), static_cast<float>(Left.X), 0.0f);
    TestEqual(TEXT("the two edges are opposite"),
        static_cast<float>(Left.Y), static_cast<float>(-Right.Y), 0.01f);
    TestEqual(TEXT("and level with each other"),
        static_cast<float>(Left.Z), static_cast<float>(Right.Z), 0.01f);
    // Centred on its own origin, so the actor drops on the floor and the
    // component lifts by half a height — one offset, in one place.
    TestEqual(TEXT("the bottom point is half a height below centre"),
        static_cast<float>(TearEdgePoint(0.0f, Width, Height, true).Z), -Height * 0.5f, 0.01f);
    TestEqual(TEXT("and the top point half a height above"),
        static_cast<float>(TearEdgePoint(1.0f, Width, Height, true).Z), Height * 0.5f, 0.01f);

    // ---- THE RAGGED EDGE -------------------------------------------------
    // DETERMINISTIC, which is what makes a tear photographable: the same yard
    // must draw the same tear in every process or no capture can be compared
    // with the one before it.
    for (int32 Key = -40; Key < 40; ++Key)
    {
        const float Once = EdgeNoise(Key);
        TestEqual(*FString::Printf(TEXT("the wander repeats for key %d"), Key), EdgeNoise(Key), Once);
        TestTrue(*FString::Printf(TEXT("and stays in the unit range at key %d"), Key),
            Once >= 0.0f && Once <= 1.0f);
    }
    // It must actually WANDER. A hash that returned a constant would pass every
    // assertion above and draw the clean lens this is here to avoid.
    {
        float Low = 1.0f, High = 0.0f;
        for (int32 Key = 0; Key < 64; ++Key)
        {
            Low = FMath::Min(Low, EdgeNoise(Key));
            High = FMath::Max(High, EdgeNoise(Key));
        }
        TestTrue(TEXT("the wander covers a real spread"), High - Low > 0.6f);
    }

    // THE TIPS STAY TIPS. The wander is a fraction of the local half-width, and
    // the profile is zero at both ends — so a tear cannot be blunted open at
    // the top or bottom no matter how large the fraction is.
    constexpr int32 Steps = 12;
    for (const bool bRight : { false, true })
    {
        for (const float Fraction : { 0.0f, 0.3f, 4.0f })
        {
            TestEqual(TEXT("the bottom tip does not wander"),
                static_cast<float>(RaggedEdgePoint(0, Steps, Width, Height, bRight, Fraction).Y), 0.0f);
            TestEqual(TEXT("nor the top tip"),
                static_cast<float>(RaggedEdgePoint(Steps, Steps, Width, Height, bRight, Fraction).Y), 0.0f);
        }
    }
    // BOUNDED BY ITS OWN FRACTION, so a tear can never turn inside out or grow
    // wider than the dial that governs it.
    for (int32 Step = 0; Step <= Steps; ++Step)
    {
        const float T = static_cast<float>(Step) / Steps;
        const float Room = TearHalfWidth(T, Width) * (1.0f + ABreakerPocketRift::EdgeWander);
        for (const bool bRight : { false, true })
        {
            const float Y = static_cast<float>(
                RaggedEdgePoint(Step, Steps, Width, Height, bRight, ABreakerPocketRift::EdgeWander).Y);
            TestTrue(*FString::Printf(TEXT("step %d stays inside its own wander"), Step),
                FMath::Abs(Y) <= Room + 0.01f);
            TestTrue(*FString::Printf(TEXT("and stays on its own side at step %d"), Step),
                bRight ? Y >= -0.01f : Y <= 0.01f);
        }
        // A zero fraction is the clean lens, exactly — the dial has to be able
        // to turn the effect off, or nobody can tell what it contributes.
        TestEqual(*FString::Printf(TEXT("a zero wander is the bare profile at step %d"), Step),
            static_cast<float>(RaggedEdgePoint(Step, Steps, Width, Height, true, 0.0f).Y),
            TearHalfWidth(T, Width), 0.01f);
    }
    // THE TWO EDGES DISAGREE. Mirrored wander would draw a leaf; the whole
    // point is that the two sides of the split do not match.
    {
        int32 Differing = 0;
        for (int32 Step = 1; Step < Steps; ++Step)
        {
            const float LeftY = static_cast<float>(
                RaggedEdgePoint(Step, Steps, Width, Height, false, ABreakerPocketRift::EdgeWander).Y);
            const float RightY = static_cast<float>(
                RaggedEdgePoint(Step, Steps, Width, Height, true, ABreakerPocketRift::EdgeWander).Y);
            if (FMath::Abs(LeftY + RightY) > 0.5f) ++Differing;
        }
        TestTrue(TEXT("the two edges wander independently"), Differing >= Steps / 2);
    }

    // THE RESTING BREATH stays a breath. A scale that reaches zero would blink
    // the tear out of existence once a cycle, and one that goes negative would
    // turn it inside out.
    for (int32 Step = 0; Step < 64; ++Step)
    {
        const float Seconds = Step * 0.13f;
        const float Scale = IdleScale(Seconds, ABreakerPocketRift::IdleHz, ABreakerPocketRift::IdleAmplitude);
        TestTrue(*FString::Printf(TEXT("the idle breath stays positive at %.2fs"), Seconds), Scale > 0.5f);
        TestTrue(*FString::Printf(TEXT("and stays small at %.2fs"), Seconds),
            FMath::Abs(Scale - 1.0f) <= ABreakerPocketRift::IdleAmplitude + KINDA_SMALL_NUMBER);
    }

    // THE ARRIVAL. Full at the instant it fires, gone when the clock runs out,
    // and never rising in between — a flare that came back would read as two
    // bodies coming through.
    constexpr float Total = ABreakerPocketRift::FlareSeconds;
    TestEqual(TEXT("the flare is full at the instant it fires"), FlareBoost(Total, Total), 1.0f, 0.001f);
    TestEqual(TEXT("and gone when the clock runs out"), FlareBoost(0.0f, Total), 0.0f);
    float Last = FlareBoost(Total, Total);
    for (int32 Step = 1; Step <= 32; ++Step)
    {
        const float Remaining = Total * (1.0f - static_cast<float>(Step) / 32.0f);
        const float Here = FlareBoost(Remaining, Total);
        TestTrue(*FString::Printf(TEXT("the flare only falls, at %.2fs left"), Remaining), Here <= Last);
        TestTrue(TEXT("and never leaves the unit range"), Here >= 0.0f && Here <= 1.0f);
        Last = Here;
    }
    // A zeroed dial silences the flare rather than making it permanent, which
    // is the failure a designer wants from a zeroed dial.
    TestEqual(TEXT("a zero flare length draws nothing"), FlareBoost(5.0f, 0.0f), 0.0f);

    // ---- THE OPENING AND THE CLOSING (O274) ------------------------------
    // A tear is an event: nothing before it opens, the whole tear once it has,
    // and only ever growing in between. A dip on the way open would read as a
    // tear that changed its mind.
    constexpr float Appear = ABreakerPocketRift::AppearSeconds;
    constexpr float Shut = ABreakerPocketRift::CloseSeconds;
    TestEqual(TEXT("a tear that has just opened is nothing"), AppearScale(0.0f, Appear), 0.0f);
    TestEqual(TEXT("and before that is still nothing"), AppearScale(-1.0f, Appear), 0.0f);
    TestEqual(TEXT("and is whole once its clock has run"), AppearScale(Appear, Appear), 1.0f);
    TestEqual(TEXT("and stays whole after"), AppearScale(Appear * 3.0f, Appear), 1.0f);
    {
        float Before = AppearScale(0.0f, Appear);
        for (int32 Step = 1; Step <= 40; ++Step)
        {
            const float Age = Appear * static_cast<float>(Step) / 40.0f;
            const float Here = AppearScale(Age, Appear);
            TestTrue(*FString::Printf(TEXT("the tear only grows while opening, at %.3fs"), Age), Here >= Before);
            TestTrue(*FString::Printf(TEXT("and stays in the unit range at %.3fs"), Age), Here >= 0.0f && Here <= 1.0f);
            Before = Here;
        }
    }
    // EASE-OUT, so the split is readable before the body is due: more than
    // half the size by half the clock.
    TestTrue(TEXT("the tear opens fast and settles"), AppearScale(Appear * 0.5f, Appear) > 0.5f);
    // A ZEROED DIAL IS INSTANT, not permanent-closed: a designer who turns the
    // opening off gets the O268 tear standing at once, not a tear that never
    // draws.
    TestEqual(TEXT("a zero appear dial is instantly whole"), AppearScale(0.0f, 0.0f), 1.0f);
    TestEqual(TEXT("and so is a negative one"), AppearScale(0.0f, -1.0f), 1.0f);

    // THE MIRROR. Whole at the moment it is told to close, nothing once the
    // clock has run, only ever shrinking in between — and EXACTLY one minus
    // the open curve, so tuning one cannot leave the other behind.
    TestEqual(TEXT("a tear that has just been closed is still whole"), CloseScale(0.0f, Shut), 1.0f);
    TestEqual(TEXT("and is nothing once its clock has run"), CloseScale(Shut, Shut), 0.0f);
    TestEqual(TEXT("and stays nothing after"), CloseScale(Shut * 3.0f, Shut), 0.0f);
    {
        float Before = CloseScale(0.0f, Shut);
        for (int32 Step = 1; Step <= 40; ++Step)
        {
            const float Since = Shut * static_cast<float>(Step) / 40.0f;
            const float Here = CloseScale(Since, Shut);
            TestTrue(*FString::Printf(TEXT("the tear only shrinks while closing, at %.3fs"), Since), Here <= Before);
            TestEqual(*FString::Printf(TEXT("and mirrors the opening at %.3fs"), Since),
                Here, 1.0f - AppearScale(Since, Shut), 0.0001f);
            Before = Here;
        }
    }
    TestEqual(TEXT("a zero close dial shuts at once"), CloseScale(0.0f, 0.0f), 0.0f);

    // THE INVERSE, which is what lets a tear interrupted mid-motion resume
    // from where it is: the age the inverse returns must land the curve back
    // on the scale it was given.
    for (int32 Step = 0; Step <= 10; ++Step)
    {
        const float Scale = static_cast<float>(Step) / 10.0f;
        const float Age = AppearAgeForScale(Scale, Appear);
        TestTrue(*FString::Printf(TEXT("the inverse stays on the clock for scale %.1f"), Scale),
            Age >= 0.0f && Age <= Appear);
        TestEqual(*FString::Printf(TEXT("and lands back on the curve for scale %.1f"), Scale),
            AppearScale(Age, Appear), Scale, 0.001f);
    }
    TestEqual(TEXT("the inverse of a zero dial is now"), AppearAgeForScale(0.5f, 0.0f), 0.0f);

    // ---- THE SHIPPED CONFIGURATION ---------------------------------------
    // A tear a body cannot walk out of is not an arrival point. An enemy
    // capsule stands about 176 cm; the authored height must clear that, and the
    // width must clear a capsule's shoulders at the height it walks through.
    TestTrue(TEXT("the tear is taller than what comes out of it"),
        ABreakerPocketRift::TearHeightCm > 200.0f);
    TestTrue(TEXT("and wide enough at shoulder height to walk through"),
        TearHalfWidth(0.35f, ABreakerPocketRift::TearWidthCm) * 2.0f > 60.0f);
    // The flare must outlast the emergence window, or the protection a body
    // arrives with would still be running after the thing that announced it has
    // gone quiet.
    TestTrue(TEXT("the flare outlasts the emergence window"),
        ABreakerPocketRift::FlareSeconds > 0.8f);
    // And it must be brighter than rest by a margin an eye turning toward it
    // can find, rather than by a percent.
    TestTrue(TEXT("an arrival is far brighter than a resting tear"),
        ABreakerPocketRift::FlareGlow > ABreakerPocketRift::IdleGlow * 2.0f);
    // THE OPENING IS A DELAY ON THE BODY (O274): the repopulation clock spawns
    // the patrol AppearSeconds after it opens the tear, and
    // RiorsEdge.Zone.Fernhall.OutdoorEncounterRuntime reads pocket 0's body
    // at 1.10 s after a claim at 0.10 s. The dial's real ceiling is that
    // second, asserted here so it cannot be turned past the test that
    // depends on it without this line saying why.
    TestTrue(TEXT("the opening lands before the runtime test reads the body"),
        ABreakerPocketRift::AppearSeconds < 1.0f);
    // Both clocks must actually run: a zero would make the tear appear or
    // vanish in one frame, which is the pop-in this actor exists to remove.
    TestTrue(TEXT("the tear takes time to open"), ABreakerPocketRift::AppearSeconds > 0.0f);
    TestTrue(TEXT("and time to close"), ABreakerPocketRift::CloseSeconds > 0.0f);
    return true;
}

#endif   // WITH_DEV_AUTOMATION_TESTS
