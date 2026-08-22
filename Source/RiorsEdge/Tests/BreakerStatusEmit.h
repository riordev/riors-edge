#pragma once

// ---------------------------------------------------------------------------
// STATUS EMISSION — the machine-readable half of `make status`
// ---------------------------------------------------------------------------
// A handful of numbers this project cares about most — the build variance
// bands, rewrite impact, the loot rate — are computed inside the suite, by code
// no external script can reach without reimplementing the aggregator or the
// drop pipeline. A second implementation of either would be a second source of
// truth for numbers whose entire value is that there is one.
//
// So the suite emits them, in a fixed shape, and Scripts/status.py reads them
// out of the log. Before this existed the numbers were in the log as prose —
// "AT-CAP BAND ... => COMPOSED 8.08x" — which a script can only read by
// regexing an English sentence somebody will eventually reword.
//
// The shape, one per line, nothing else on the line:
//
//     [BreakerStatus] key=power-band-atcap value=6.5300
//
// The key must match a section key in Scripts/status.py. An unmatched key is
// reported rather than dropped: a status line nobody reads is the same silent
// nothing as a pin that names no section.
//
// This is for numbers a HUMAN tunes against. It is not a logging channel for
// test diagnostics — AddInfo already does that, and it stays prose because a
// person reads it.
// ---------------------------------------------------------------------------

#include "CoreMinimal.h"

namespace BreakerStatus
{
    inline void Emit(const TCHAR* Key, float Value)
    {
        UE_LOG(LogTemp, Display, TEXT("[BreakerStatus] key=%s value=%.4f"), Key, Value);
    }
}
