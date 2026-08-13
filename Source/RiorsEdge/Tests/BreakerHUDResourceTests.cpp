#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UI/BreakerHUDResourceRow.h"
#include "Engine/Font.h"
#include "Fonts/SlateFontInfo.h"

// The drawing cannot be tested — there is no viewport in the automation suite
// and no way to assert that a mark reads. The RESOLUTION can: label, signed
// fraction, state word, state colour and track treatment are pure functions of
// the class resource's numbers, and that is the part a wrong class would break.
//
// Moved here from UI/ during integration to match the project's source layout;
// discovery is by macro, not by directory.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerHUDResourceRowTest,
    "RiorsEdge.UI.ClassResourceRow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerHUDResourceRowTest::RunTest(const FString& Parameters)
{
    using namespace BreakerHUD;

    // --- No class resource ------------------------------------------------
    {
        const FResourceRow Row = ResolveEmptyResourceRow();
        TestFalse(TEXT("An unwired class is inactive"), Row.bActive);
        TestEqual(TEXT("Inactive state word"), Row.StateWord, FString(TEXT("NO RESOURCE")));
        TestEqual(TEXT("Inactive fraction is zero"), Row.Fraction, 0.0f);
        TestTrue(TEXT("Inactive track is empty"), Row.Track == EResourceTrack::Empty);
    }

    // --- Swift: Momentum, the three states --------------------------------
    {
        const FResourceRow Settled = ResolveMomentumRow(0.20f, EBreakerMomentumState::Settled);
        TestTrue(TEXT("Momentum is active"), Settled.bActive);
        TestEqual(TEXT("Swift label"), Settled.Label, FString(TEXT("MOMENTUM")));
        TestEqual(TEXT("Settled state word"), Settled.StateWord, FString(TEXT("SETTLED")));
        TestEqual(TEXT("Settled fraction"), Settled.Fraction, 0.20f);
        TestTrue(TEXT("Settled is one continuous bar"), Settled.Track == EResourceTrack::Continuous);
        TestTrue(TEXT("Settled is cyan"), Settled.StateColor.Equals(BreakerUI::Cyan));
        TestEqual(TEXT("Settled keeps the 1px track border"), Settled.BorderPixels, 1.0f);

        const FResourceRow Running = ResolveMomentumRow(0.50f, EBreakerMomentumState::Running);
        TestEqual(TEXT("Running state word"), Running.StateWord, FString(TEXT("RUNNING")));
        TestTrue(TEXT("Running splits into 8px blocks"), Running.Track == EResourceTrack::Blocks);
        TestTrue(TEXT("Running is gold"), Running.StateColor.Equals(BreakerUI::Gold));

        const FResourceRow Redline = ResolveMomentumRow(0.90f, EBreakerMomentumState::Redline);
        TestEqual(TEXT("Redline state word"), Redline.StateWord, FString(TEXT("REDLINE")));
        TestTrue(TEXT("Redline widens the blocks"), Redline.Track == EResourceTrack::WideBlocks);
        TestTrue(TEXT("Redline is orange"), Redline.StateColor.Equals(BreakerUI::Orange));
        TestTrue(TEXT("Redline takes the orange track border"), Redline.BorderColor.Equals(BreakerUI::Orange));
        TestEqual(TEXT("Redline widens the track border to 2px"), Redline.BorderPixels, 2.0f);

        // Momentum has no debt half, whatever it is handed.
        TestEqual(TEXT("Momentum clamps above 1"), ResolveMomentumRow(1.8f, EBreakerMomentumState::Redline).Fraction, 1.0f);
        TestEqual(TEXT("Momentum never goes negative"), ResolveMomentumRow(-0.5f, EBreakerMomentumState::Settled).Fraction, 0.0f);
    }

    // --- Caster in credit -------------------------------------------------
    {
        const FResourceRow Row = ResolveManaRow(60.0f, 100.0f, -20.0f);
        TestTrue(TEXT("Mana is active"), Row.bActive);
        TestEqual(TEXT("Caster label"), Row.Label, FString(TEXT("MANA")));
        TestEqual(TEXT("Credit state word"), Row.StateWord, FString(TEXT("BANKED")));
        TestEqual(TEXT("Credit divides by the maximum"), Row.Fraction, 0.60f);
        TestTrue(TEXT("Credit is positive"), Row.Fraction > 0.0f);
        TestTrue(TEXT("Mana uses the signed track"), Row.Track == EResourceTrack::Signed);
        TestTrue(TEXT("Credit is the Caster accent"), Row.StateColor.Equals(BreakerUI::Cyan));
        TestEqual(TEXT("Credit keeps the 1px track border"), Row.BorderPixels, 1.0f);

        const FResourceRow Full = ResolveManaRow(140.0f, 100.0f, -20.0f);
        TestEqual(TEXT("Credit clamps at the maximum"), Full.Fraction, 1.0f);

        const FResourceRow NoPool = ResolveManaRow(0.0f, 0.0f, -20.0f);
        TestEqual(TEXT("An unauthored maximum reads empty, not infinite"), NoPool.Fraction, 0.0f);
        TestEqual(TEXT("Zero is still credit, not debt"), NoPool.StateWord, FString(TEXT("BANKED")));
    }

    // --- Caster in debt (Overcast) ----------------------------------------
    {
        const FResourceRow Row = ResolveManaRow(-10.0f, 100.0f, -20.0f);
        TestTrue(TEXT("Overcast is active"), Row.bActive);
        TestEqual(TEXT("Debt keeps the same label"), Row.Label, FString(TEXT("MANA")));
        TestEqual(TEXT("Debt state word"), Row.StateWord, FString(TEXT("OVERCAST")));
        // Debt is measured against the floor, not the maximum: half the rope
        // spent is half a bar, whatever the pool size is.
        TestEqual(TEXT("Debt divides by the Overcast floor"), Row.Fraction, -0.50f);
        TestTrue(TEXT("Debt reads as a negative fraction"), Row.Fraction < 0.0f);
        TestTrue(TEXT("Debt still uses the signed track"), Row.Track == EResourceTrack::Signed);
        TestTrue(TEXT("Debt is the harm colour"), Row.StateColor.Equals(BreakerUI::Harm));
        TestTrue(TEXT("Debt takes the harm track border"), Row.BorderColor.Equals(BreakerUI::Harm));
        TestEqual(TEXT("Debt widens the track border to 2px"), Row.BorderPixels, 2.0f);

        // The pool size must not move the debt reading (SB4 deepens the floor;
        // gear deepens the pool — only the first may change this bar).
        TestEqual(TEXT("Debt ignores the maximum"), ResolveManaRow(-10.0f, 400.0f, -20.0f).Fraction, -0.50f);
        TestEqual(TEXT("A deeper floor makes the same debt read shallower"),
            ResolveManaRow(-10.0f, 100.0f, -35.0f).Fraction, -10.0f / 35.0f);
        TestEqual(TEXT("Full debt is a full bar"), ResolveManaRow(-20.0f, 100.0f, -20.0f).Fraction, -1.0f);
        TestEqual(TEXT("Debt past the floor still clamps at one bar"),
            ResolveManaRow(-50.0f, 100.0f, -20.0f).Fraction, -1.0f);
        TestEqual(TEXT("A class with no floor reads full debt rather than dividing by zero"),
            ResolveManaRow(-5.0f, 100.0f, 0.0f).Fraction, -1.0f);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerHUDFontContractTest,
    "RiorsEdge.UI.HUDFontContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Regression, and a sharp one: the whole HUD's text vanished at once.
//
// FCanvasTextItem::HasValidText() is literally `Font != nullptr`, and the
// FSlateFontInfo constructor sets that member with Cast<UFont>(FontObject).
// An FSlateFontInfo from FCoreStyle carries a raw FCompositeFont and no
// UObject at all, so Font came back null, Draw() returned immediately, and
// every string on the HUD silently disappeared while the plates, bars and
// glyphs kept drawing.
//
// So the HUD's font has two hard requirements, both asserted here rather than
// discovered in a screenshot: the info must resolve to a real UFont, and that
// UFont must be Runtime-cached, because GetFontCacheType() picks the draw path
// from it and the Offline path ignores the size in the font info and goes back
// to magnifying a bitmap — which is the fuzziness this replaced.
bool FBreakerHUDFontContractTest::RunTest(const FString& Parameters)
{
    const UFont* Font = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"));
    if (!TestNotNull(TEXT("The HUD's font asset resolves"), Font))
    {
        return false;
    }
    TestEqual(TEXT("It is Runtime-cached, so it rasterises at the size asked for"),
        Font->FontCacheType, EFontCacheType::Runtime);

    // The exact expression FCanvasTextItem uses to decide whether to draw.
    for (const TCHAR* Typeface : {TEXT("Regular"), TEXT("Bold")})
    {
        const FSlateFontInfo Info(Font, 24, Typeface);
        TestNotNull(*FString::Printf(TEXT("%s resolves to a UFont, so canvas text draws"), Typeface),
            Cast<const UFont>(Info.FontObject));
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
