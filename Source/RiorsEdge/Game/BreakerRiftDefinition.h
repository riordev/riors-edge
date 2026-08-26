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
// Which death rule a rift runs under (O82, amended). Campaign: unlimited
// respawn from the tileset start, boss deaths reset the encounter. Endgame:
// the death budget — two for a solo character — PARKED behind O122's other
// half: no decrement may be wired until endgame rifts are consumable,
// because a limit on a free instance kicks the player out of a door they
// immediately walk back through.
UENUM(BlueprintType)
enum class EBreakerRiftTier : uint8
{
    Campaign,
    Endgame,
};

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

    // Which death rule this rift runs under. Every rift today is Campaign;
    // Endgame arrives with O122's consumable entry.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rift")
    EBreakerRiftTier Tier = EBreakerRiftTier::Campaign;

    bool IsSet() const { return AreaLevel > 0; }
    int32 EffectiveAreaLevel() const { return UBreakerMonsterChassisLibrary::ClampAreaLevel(AreaLevel); }
};

// THE COMPLETION EVENT (O168). FIELD's terminator raises that it died; GROUND
// consumes and broadcasts THIS; LEDGER binds it and pays. Declared here rather
// than on the game mode so a consumer needs the rift's own header and not the
// 500-line game-mode header to know the signature.
//
// It carries the WHOLE definition because that is already the thing that
// travels, and a struct grows. It fires ONLY on completion — abandonment has no
// representation on it, deliberately, because a bool for "abandoned" invites
// paying a reduced amount for walking out, and leaving by the door you came in
// is simply the absence of this.
DECLARE_MULTICAST_DELEGATE_TwoParams(FBreakerRiftCompleted,
    const FBreakerRiftDefinition& /*Rift*/, APawn* /*Player*/);

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

    // O82: what an endgame instance grants a solo character. The party
    // scaling and the decrement are both parked behind O122's consumable
    // entry; this constant exists so the readout below and the future
    // budget spend read one number.
    static constexpr int32 SoloEndgameDeathBudget = 2;

    // CAN THIS RUN BE COMPLETED? The whole rule, world-free, so the latch is
    // provable without standing in a rift (the precedent is every other rule in
    // this project that matters: extract the arithmetic, make the actor a thin
    // caller). Two conditions and both are refusals worth having:
    //
    //   * A rift that is NOT SET cannot be completed. You cannot finish a run
    //     you are not in, and without this a stray console call in the gym or
    //     the hub would broadcast a completion carrying a nameless rift — which
    //     LEDGER would pay out on.
    //   * A run already completed cannot complete again. This is the ONE-WAY
    //     LATCH O168 names: one completion, one broadcast, so LEDGER binds
    //     directly instead of settling grants against a counter.
    //
    // Re-ENTERING a door is not caught here and must not be: that is a new
    // world with fresh state and honestly a new run.
    UFUNCTION(BlueprintPure, Category="Rift")
    static bool CanCompleteRiftRun(bool bAlreadyCompleted, const FBreakerRiftDefinition& Rift)
    {
        return Rift.IsSet() && !bAlreadyCompleted;
    }

    // O123: the death-allowance field is ALWAYS present and reads its mode —
    // campaign prints UNLIMITED, endgame prints the count remaining. Never
    // hide it and never lay it out conditionally; only the value moves.
    UFUNCTION(BlueprintPure, Category="Rift")
    static FString GetDeathAllowanceReadout(EBreakerRiftTier Tier, int32 EndgameDeathsRemaining)
    {
        return Tier == EBreakerRiftTier::Campaign
            ? FString(TEXT("UNLIMITED"))
            : FString::Printf(TEXT("%d REMAINING"), FMath::Max(EndgameDeathsRemaining, 0));
    }
};
