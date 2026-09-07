#pragma once

#include "CoreMinimal.h"
#include "Combat/BreakerStatusComponent.h"

// Pure, world-free: the arithmetic of a status rule, with no actor and no
// library behind it. The weapon and the tests both come through here.
namespace FBreakerStatusRuleMath
{
    // Depth-2 marker. FBreakerStatusApplicationSpec has no "is a spread copy"
    // field (it lives in Progression/, another lane's file), so a copy is
    // marked the way Unmake's Cascade echo already is: ProcCoefficient 0. A
    // status at proc coefficient 0 generates nothing downstream — no Mana, no
    // reaction, no Cascade — and, by the same reading, is never a spread
    // SOURCE. The proc coefficient law's cap at depth 2 falls out of that:
    // original (depth 1) spreads a copy (depth 2); the copy spreads nothing.
    inline bool IsPierceSpreadSource(const FBreakerActiveStatus& Status)
    {
        return Status.Spec.ProcCoefficient > 0.0f;
    }

    // The spread copy of a running status: the same spec, so the tag, the
    // tick interval and the WHOLE application snapshot travel unchanged (a
    // copy of a critical Poison ticks critically), with three deliberate
    // differences —
    //   Duration        = what the source has LEFT, never the authored
    //                     duration: "the original's remaining budget, never a
    //                     fresh full application".
    //   BaseDamagePerTick scaled by PayloadFraction, the normalized payload.
    //   ProcCoefficient = 0, the depth marker above.
    inline FBreakerStatusApplicationSpec MakePierceSpreadSpec(const FBreakerActiveStatus& Source, float PayloadFraction)
    {
        FBreakerStatusApplicationSpec Copy = Source.Spec;
        Copy.Duration = FMath::Max(0.0f, Source.RemainingDuration);
        Copy.BaseDamagePerTick = Source.Spec.BaseDamagePerTick * FMath::Clamp(PayloadFraction, 0.0f, 1.0f);
        Copy.ProcCoefficient = 0.0f;
        return Copy;
    }
}
