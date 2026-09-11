#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Progression/BreakerProgressionTypes.h"
#include "BreakerRotAimRuntimeObserver.generated.h"

// Counts UBreakerAbilityComponent::OnAbilityActivated — the broadcast the HUD
// plays the cast cue off. A dynamic delegate needs a UFUNCTION to land on.
UCLASS()
class UBreakerRotAimRuntimeObserver : public UObject
{
    GENERATED_BODY()
public:
    int32 Count = 0;
    UFUNCTION() void OnActivated(EBreakerAbilitySlot Slot) { ++Count; }
};
