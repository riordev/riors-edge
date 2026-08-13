#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Combat/BreakerCombatTypes.h"
#include "BreakerDamageLibrary.generated.h"

UCLASS()
class RIORSEDGE_API UBreakerDamageLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category="Combat|Damage")
    static FBreakerDamageResult ResolveDamage(const FBreakerDamageRequest& Request, const FBreakerDefenseState& Defense);

    UFUNCTION(BlueprintPure, Category="Combat|Damage")
    static float CalculateArmorMitigation(float Armor, float ArmorPenetration);

    UFUNCTION(BlueprintPure, Category="Combat|Status")
    // Instigator is the actor that applied the status; it is carried weakly on
    // the tick request so attacker-side hit events fire for DoT damage too.
    static FBreakerDamageRequest MakeSnapshotDotTick(const FBreakerStatusApplicationSpec& StatusSpec, EBreakerDamageFamily DamageFamily, int32 TickIndex, AActor* Instigator);
};
