#pragma once
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
class AActor;
namespace BreakerReactionFeedback
{
    RIORSEDGE_API void Play(AActor* Target, AActor* Applier, FGameplayTag ReactionTag);
}
