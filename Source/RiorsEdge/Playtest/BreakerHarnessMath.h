#pragma once

#include "CoreMinimal.h"
#include "Misc/Parse.h"

// IS THIS A HARNESS RUN, as pure world-free maths over a command line. The
// precedent is Playtest/BreakerKillBuckets.h: this is a rule, and a rule that
// reads the global command line inside an actor is a rule nobody can test.
//
// WHY IT EXISTS. A harness run must never write a save. The character's save
// guard used to refuse only under -BreakerCaptureMenu=; a -BreakerAutoPlay=Gym
// capture travelled to a real map, played, and on EndPlay wrote
// BreakerSave0.sav and BreakerAccount.sav into the OWNER'S save directory —
// overwriting the account's Riftglass with whatever the capture pawn was
// seeded with. The refusal has to be keyed on the whole harness vocabulary,
// and the vocabulary has to live in one place so it cannot drift.
//
// The predicate takes the command line as an argument rather than reading
// FCommandLine::Get(), so the suite can hand it strings.
namespace BreakerHarness
{
    // THE HARNESS'S OWN SWITCH LIST, from CLAUDE.md's capture-harness block
    // plus the gym instruments (-BreakerCrowdProbe, -BreakerCrowdLoad,
    // -BreakerEffectProbe) and the movement trace (-BreakerMoveTrace).
    // Names carry no dash and no '='; each is recognised bare (FParse::Param,
    // exact: dash before, whitespace or end after) and as '<Name>=<value>'
    // (FParse::Value), so a switch handed in the wrong form still refuses.
    //
    // A NEW SWITCH IS ADDED HERE OR ITS RUN WRITES THE OWNER'S SAVES.
    inline constexpr const TCHAR* const HarnessSwitches[] =
    {
        TEXT("BreakerAutoPlay"),
        TEXT("BreakerScreenshots"),
        TEXT("BreakerScreenshotFirst"),
        TEXT("BreakerScreenshotInterval"),
        TEXT("BreakerCaptureMenu"),
        TEXT("BreakerCaptureBoard"),
        TEXT("BreakerCaptureTour"),
        TEXT("BreakerCaptureHUD"),
        TEXT("BreakerCycleWeapons"),
        TEXT("BreakerBossOnStart"),
        TEXT("BreakerAbilityProbe"),
        TEXT("BreakerCaptureBlast"),
        TEXT("BreakerCapturePocketRift"),
        TEXT("BreakerCaptureChest"),
        TEXT("BreakerCaptureNpc"),
        TEXT("BreakerCaptureWeakPoint"),
        TEXT("BreakerCrowdProbe"),
        TEXT("BreakerCrowdLoad"),
        TEXT("BreakerEffectProbe"),
        TEXT("BreakerMoveTrace"),
    };
    inline constexpr int32 HarnessSwitchCount = UE_ARRAY_COUNT(HarnessSwitches);

    // True when CmdLine carries any harness switch, bare or with a value.
    // FParse::Param is exact, so -BreakerAutoPlayX is not -BreakerAutoPlay;
    // FParse::Value is a substring find, so a switch name embedded inside a
    // longer '=' token (-XBreakerAutoPlay=1) still reads as harness. That
    // errs toward refusing a save, which is the safe direction here.
    //
    // A run whose saves are ISOLATED is not refused: -UserDir= points the
    // whole Saved/ tree somewhere else, so the loop probe
    // (Scripts/ue-loop-probe.ps1, -BreakerAutoPlay -BreakerLoopProbe -UserDir)
    // can carry XP and Riftglass across its map travels through the file,
    // which is the thing it exists to prove, and the owner's saves are never
    // the file it writes.
    inline bool IsHarnessCommandLine(const TCHAR* CmdLine)
    {
        if (!CmdLine || !*CmdLine) return false;
        FString UserDir;
        if (FParse::Value(CmdLine, TEXT("UserDir="), UserDir) && !UserDir.IsEmpty()) return false;
        for (const TCHAR* Switch : HarnessSwitches)
        {
            if (FParse::Param(CmdLine, Switch)) return true;
            FString ValueScratch;
            if (FParse::Value(CmdLine, *FString::Printf(TEXT("%s="), Switch), ValueScratch)) return true;
        }
        return false;
    }
}
