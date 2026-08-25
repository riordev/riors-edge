#pragma once

#include "CoreMinimal.h"
#include "Combat/BreakerMonsterChassis.h"
#include "Items/BreakerAffixLibrary.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BreakerRiftDefinition.generated.h"

// ---------------------------------------------------------------------------
// WHAT A RIFT IS, AS DATA.
//
// This is the Local Rift's data model wearing a loading screen — authored
// here and not on the game mode, because GymAreaLevel being an EditAnywhere
// property on ABreakerGameMode is exactly the wall this replaces. An
// instanced rift needs an area name, a player-set area level and everything
// that derives from it; once the rift door exists, it hands one of these
// across the travel and the destination builds to it.
//
// AUTHORED: the name, the line, the level. DERIVED: everything else — the
// item-level range comes from the drop pipeline's own function and the
// monster multipliers from the chassis curves, so this struct can never
// disagree with what the game actually spawns. Do not add authored copies
// of derivable numbers.
// ---------------------------------------------------------------------------
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerRiftDefinition
{
    GENERATED_BODY()

    // One hand-authored name per rift. The first and only authored rift
    // today is the plate's own: FERNHALL SUBSTATION.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rift") FText AreaName;
    // The one-sentence area line under the name. Presentation copy, not a
    // rule.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rift") FText AreaLine;
    // Player-set, clamped 1..100 on every read through EffectiveAreaLevel —
    // the same band GymAreaLevel clamps to. Zero means "not set", which is
    // how a session with no chosen rift stays on the game mode's dev
    // fallback.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rift", meta=(ClampMin="0", ClampMax="100"))
    int32 AreaLevel = 0;

    bool IsSet() const { return AreaLevel > 0; }
    int32 EffectiveAreaLevel() const { return UBreakerMonsterChassisLibrary::ClampAreaLevel(AreaLevel); }
};

UCLASS()
class RIORSEDGE_API UBreakerRiftLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // The drop item-level range this area actually produces: the floor is
    // the pipeline's own GetDropItemLevel (trash), the ceiling adds the
    // elite-or-better bonus, both clamped to the ladder. EliteBonus is the
    // enemy's authored EliteDropItemLevelBonus — passed in, never copied
    // here, so the range cannot drift from what ApplyChassis really hands
    // GrantLoot.
    UFUNCTION(BlueprintPure, Category="Rift")
    static void GetDropItemLevelRange(int32 AreaLevel, int32 EliteBonus, int32& OutMin, int32& OutMax)
    {
        OutMin = UBreakerMonsterChassisLibrary::GetDropItemLevel(AreaLevel);
        OutMax = FMath::Clamp(OutMin + FMath::Max(EliteBonus, 0), 1, UBreakerAffixLibrary::MaxItemLevel);
    }

    // RULED: the multiplier baseline is AREA LEVEL 1 — the only baseline
    // that doesn't move. GetChassisHealth(AL)/GetChassisHealth(1) cancels
    // BaseHealth, so the readout is a property of the CURVE and holds for
    // every chassis sharing the growth constant: (1+g)^(AL-1).
    UFUNCTION(BlueprintPure, Category="Rift")
    static float GetMonsterHealthMultiplier(int32 AreaLevel, const FBreakerMonsterChassisParams& Params)
    {
        return UBreakerMonsterChassisLibrary::GetChassisHealth(AreaLevel, Params)
            / FMath::Max(UBreakerMonsterChassisLibrary::GetChassisHealth(1, Params), KINDA_SMALL_NUMBER);
    }

    UFUNCTION(BlueprintPure, Category="Rift")
    static float GetMonsterDamageMultiplier(int32 AreaLevel, const FBreakerMonsterChassisParams& Params)
    {
        return UBreakerMonsterChassisLibrary::GetChassisDamage(AreaLevel, Params)
            / FMath::Max(UBreakerMonsterChassisLibrary::GetChassisDamage(1, Params), KINDA_SMALL_NUMBER);
    }
};
