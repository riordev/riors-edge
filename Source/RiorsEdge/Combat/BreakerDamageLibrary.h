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

    // The healing half of the contract, pure and world-free for exactly the
    // reason ResolveDamage is: the arithmetic of "how much of this heal lands,
    // how much is overheal, how much of the overheal becomes shield" is
    // testable with no actor, and every healing source in the game must agree
    // on it. Health first, then overheal — never shield first, or a target at
    // full health with a broken shield would consume the heal on the shield and
    // report zero healing to Support's Charge loop.
    UFUNCTION(BlueprintPure, Category="Combat|Healing")
    static FBreakerHealResult ResolveHealing(const FBreakerHealRequest& Request, const FBreakerVitalsState& Vitals);
};
