#pragma once
#include "CoreMinimal.h"
class AActor;
namespace BreakerVoidFeedback
{
    RIORSEDGE_API void PlayActivation(AActor* Target, AActor* Applier);
    RIORSEDGE_API void PlayBurst(AActor* Target, AActor* Applier);
}
