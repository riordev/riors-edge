#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BreakerPartyPolicy.generated.h"

UCLASS(BlueprintType)
class RIORSEDGE_API UBreakerPartyPolicy : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Party", meta=(ClampMin="1", ClampMax="5")) int32 MaximumPartySize = 5;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Scaling", meta=(ClampMin="0")) float HealthPerAdditionalPlayer = 0.60f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Scaling", meta=(ClampMin="0")) float DamagePerAdditionalPlayer = 0.08f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Scaling", meta=(ClampMin="0")) float EliteFrequencyPerAdditionalPlayer = 0.12f;

    UFUNCTION(BlueprintPure, Category="Scaling") float GetEnemyHealthMultiplier(int32 PartySize) const
    {
        return 1.0f + FMath::Max(0, FMath::Clamp(PartySize, 1, MaximumPartySize) - 1) * HealthPerAdditionalPlayer;
    }

    UFUNCTION(BlueprintPure, Category="Scaling") float GetEnemyDamageMultiplier(int32 PartySize) const
    {
        return 1.0f + FMath::Max(0, FMath::Clamp(PartySize, 1, MaximumPartySize) - 1) * DamagePerAdditionalPlayer;
    }
};
