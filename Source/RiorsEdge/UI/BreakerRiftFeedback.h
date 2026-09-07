#pragma once
#include "CoreMinimal.h"
class AActor;
namespace BreakerRiftFeedback
{
    // O19: Rift damage is hotter, whiter cyan; never object teal.
    inline const FLinearColor Color(.72f, .94f, 1.0f);
    RIORSEDGE_API void PlayActivation(AActor* Target, AActor* Applier);
}
