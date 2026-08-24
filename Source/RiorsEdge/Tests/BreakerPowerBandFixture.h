#pragma once

#if WITH_DEV_AUTOMATION_TESTS

// ---------------------------------------------------------------------------
// THE AT-CAP BAND, AS A FUNCTION RATHER THAN AS A NUMBER SOMEBODY TYPED.
//
// RiorsEdge.Combat.PowerCurve.BossOptimized needs the at-cap build-variance
// band to derive an optimized boss kill from a baseline one, and it used to get
// it like this:
//
//     constexpr float MeasuredAtCapBand = 6.53f;   // emitted by PowerBand.AtCap
//
// with a comment beside it promising "if the band moves, this moves with it".
// It does not. It is a transcription, and the comment is the kind of claim that
// makes a stale number look maintained — the band moved to 6.54 and this stayed
// at 6.53, so the project had two instruments reporting one quantity and no way
// to notice they disagreed. A second copy of a measured number is a second
// source of truth for the one thing whose entire value is having one.
//
// So the band is computed, once, by the fixtures that define it. PowerBand.AtCap
// emits what this returns and BossOptimized derives from what this returns, and
// neither holds a copy. There is nothing left to transcribe.
// ---------------------------------------------------------------------------
namespace BreakerPowerBandTest
{
    // Optimized total over baseline total at the level cap, in the measurement
    // condition state. Deterministic over the shipped fixtures: same inputs,
    // same answer, every call.
    float AtCapBand();
}

#endif // WITH_DEV_AUTOMATION_TESTS
