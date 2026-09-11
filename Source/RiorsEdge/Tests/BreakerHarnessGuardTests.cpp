#include "Misc/AutomationTest.h"
#include "Playtest/BreakerHarnessMath.h"

#if WITH_DEV_AUTOMATION_TESTS

// A harness run never writes a save. The character's guard keys on
// BreakerHarness::IsHarnessCommandLine; this proves the predicate over the
// strings a harness actually passes, and pins the switch list so a switch
// that leaves it is a red here before it is an overwritten account file.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerHarnessGuardTest,
    "RiorsEdge.Playtest.HarnessGuard",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerHarnessGuardTest::RunTest(const FString& Parameters)
{
    // --- Shipped configuration: the list itself ---------------------------
    TestTrue(TEXT("The harness switch list carries at least the 17 switches the finding named"),
        BreakerHarness::HarnessSwitchCount >= 17);
    bool bHasAutoPlay = false;
    bool bHasCaptureMenu = false;
    for (const TCHAR* Switch : BreakerHarness::HarnessSwitches)
    {
        if (FCString::Stricmp(Switch, TEXT("BreakerAutoPlay")) == 0) bHasAutoPlay = true;
        if (FCString::Stricmp(Switch, TEXT("BreakerCaptureMenu")) == 0) bHasCaptureMenu = true;
        TestTrue(FString::Printf(TEXT("Switch '%s' is listed without a dash"), Switch), Switch[0] != TEXT('-'));
        TestTrue(FString::Printf(TEXT("Switch '%s' is listed without an '='"), Switch), FCString::Strchr(Switch, TEXT('=')) == nullptr);
    }
    TestTrue(TEXT("The list names BreakerAutoPlay (the switch whose run overwrote the owner's account)"), bHasAutoPlay);
    TestTrue(TEXT("The list names BreakerCaptureMenu (the switch the old guard knew)"), bHasCaptureMenu);

    // --- Each listed switch alone, bare and with a value --------------------
    for (const TCHAR* Switch : BreakerHarness::HarnessSwitches)
    {
        const FString Bare = FString::Printf(TEXT("-%s"), Switch);
        const FString Valued = FString::Printf(TEXT("-%s=1"), Switch);
        const FString InAFullLine = FString::Printf(TEXT("-game -windowed -ResX=1920 -ResY=1080 -%s -log"), Switch);
        TestTrue(FString::Printf(TEXT("'%s' alone is a harness run"), *Bare), BreakerHarness::IsHarnessCommandLine(*Bare));
        TestTrue(FString::Printf(TEXT("'%s' alone is a harness run"), *Valued), BreakerHarness::IsHarnessCommandLine(*Valued));
        TestTrue(FString::Printf(TEXT("'%s' is a harness run"), *InAFullLine), BreakerHarness::IsHarnessCommandLine(*InAFullLine));
    }

    // --- The exact lines that wrote the owner's saves ----------------------
    TestTrue(TEXT("-BreakerAutoPlay=Gym (today's capture line) is a harness run"),
        BreakerHarness::IsHarnessCommandLine(TEXT("-game -windowed -ResX=1920 -ResY=1080 -BreakerAutoPlay=Gym -BreakerScreenshots=3")));
    TestTrue(TEXT("-BreakerAutoPlay=Fernhall is a harness run"),
        BreakerHarness::IsHarnessCommandLine(TEXT("-BreakerAutoPlay=Fernhall")));
    TestTrue(TEXT("Bare -BreakerAutoPlay is a harness run"),
        BreakerHarness::IsHarnessCommandLine(TEXT("-BreakerAutoPlay")));
    TestTrue(TEXT("-BreakerCaptureMenu=RIFTDEBRIEF is a harness run"),
        BreakerHarness::IsHarnessCommandLine(TEXT("-BreakerCaptureMenu=RIFTDEBRIEF")));
    TestTrue(TEXT("-BreakerAbilityProbe=BreakerSwift:Dash (the old probe guard's shape) is a harness run"),
        BreakerHarness::IsHarnessCommandLine(TEXT("-BreakerAbilityProbe=BreakerSwift:Dash")));

    // --- Lines the owner plays on are not harness runs ----------------------
    TestFalse(TEXT("An empty command line is not a harness run"),
        BreakerHarness::IsHarnessCommandLine(TEXT("")));
    TestFalse(TEXT("A null command line is not a harness run"),
        BreakerHarness::IsHarnessCommandLine(nullptr));
    TestFalse(TEXT("A plain play line is not a harness run"),
        BreakerHarness::IsHarnessCommandLine(TEXT("-game -windowed -ResX=1920")));
    TestFalse(TEXT("The suite's own line is not a harness run"),
        BreakerHarness::IsHarnessCommandLine(TEXT("-ExecCmds=\"Automation RunTests RiorsEdge; SoftQuit\" -unattended -nop4 -nosplash -nullrhi")));

    // --- Prefix collision: FParse::Param is exact ---------------------------
    TestFalse(TEXT("-BreakerAutoPlayX is not -BreakerAutoPlay (the bare parse is exact)"),
        BreakerHarness::IsHarnessCommandLine(TEXT("-BreakerAutoPlayX")));
    TestFalse(TEXT("-BreakerCaptureMenuX is not -BreakerCaptureMenu"),
        BreakerHarness::IsHarnessCommandLine(TEXT("-game -BreakerCaptureMenuX")));
    // Isolated saves are not the owner's saves: the loop probe's own line.
    TestFalse(TEXT("A -UserDir= run keeps its saves, harness switches or not"),
        BreakerHarness::IsHarnessCommandLine(TEXT("-game -BreakerAutoPlay=Anchor -BreakerLoopProbe -UserDir=C:/tmp/probe")));
    TestTrue(TEXT("An empty -UserDir= is no isolation"),
        BreakerHarness::IsHarnessCommandLine(TEXT("-game -BreakerAutoPlay=Anchor -UserDir=")));

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
