#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BreakerAbilityStateComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBreakerWindowEnded, FName, WindowKey);

// SI-9 (Ability-Implementation-Spec §2.5). Eleven abilities and a dozen tree
// nodes need "a named state that lasts N seconds" or "a counter per target that
// resets on a miss or a swap". Building it once is the difference between a
// clean ability layer and eleven bespoke timers.
//
// Server-side only for now: nothing here replicates. The spec asks for the
// window array to replicate eventually so simulated proxies can drive
// cosmetics; that is deliberately deferred until a cosmetic actually reads it.
//
// DEVIATION FROM SPEC: the spec keys windows and streaks by FGameplayTag. This
// implementation keys by FName because the ability definitions already key
// their fallback registry by FName and no tag vocabulary exists yet for the
// window names. The signatures are otherwise the spec's; swapping the key type
// later is a mechanical change confined to this file and its callers.
UCLASS(ClassGroup=Abilities, BlueprintType, meta=(BlueprintSpawnableComponent))
class RIORSEDGE_API UBreakerAbilityStateComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UBreakerAbilityStateComponent();
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // Windows ------------------------------------------------------------
    // Starting a window that is already open replaces its remaining time
    // rather than stacking: a re-cast refreshes, it does not accumulate.
    UFUNCTION(BlueprintCallable, Category="Abilities|State") void StartWindow(FName Key, float Duration);
    UFUNCTION(BlueprintCallable, Category="Abilities|State") void ExtendWindow(FName Key, float ExtraSeconds);
    // Closing broadcasts OnWindowEnded exactly like a natural expiry, so
    // listeners have one teardown path.
    UFUNCTION(BlueprintCallable, Category="Abilities|State") void CloseWindow(FName Key);
    UFUNCTION(BlueprintPure, Category="Abilities|State") bool IsWindowActive(FName Key) const;
    UFUNCTION(BlueprintPure, Category="Abilities|State") float GetWindowRemaining(FName Key) const;
    UFUNCTION(BlueprintPure, Category="Abilities|State") int32 GetActiveWindowCount() const;

    // Per-target hit streaks ---------------------------------------------
    // Returns the streak count after recording the hit. The streak resets to 1
    // when the target changes or when StreakGapSeconds have passed since the
    // previous hit (Class-Kits S2 Cadence Break: "resets on miss or target
    // swap"; the gap covers the miss case until SI-8's real miss event exists).
    UFUNCTION(BlueprintCallable, Category="Abilities|State") int32 RecordHit(AActor* Target);
    UFUNCTION(BlueprintPure, Category="Abilities|State") int32 GetStreak(AActor* Target) const;
    UFUNCTION(BlueprintCallable, Category="Abilities|State") void ResetStreak();

    // Drives both clocks. Called from TickComponent, and directly by tests so
    // the whole component is exercisable without a world.
    UFUNCTION(BlueprintCallable, Category="Abilities|State") void AdvanceTime(float DeltaSeconds);

    UPROPERTY(BlueprintAssignable, Category="Abilities|State") FBreakerWindowEnded OnWindowEnded;

    // Class-Kits S2/F9 use 1.0s; 3.0s is this component's default gap, quoted
    // from the task brief rather than from a design doc. O2 PLACEHOLDER.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Abilities|State", meta=(ClampMin="0")) float StreakGapSeconds = 3.0f;

    // Pure rule, exposed for tests: a streak survives only a same-target hit
    // inside the gap.
    static bool ShouldContinueStreak(bool bSameTarget, float SecondsSinceLastHit, float GapSeconds);

    // Finds the state component on Owner, adding one if it is absent. Abilities
    // use this so no character class has to be edited to gain the component.
    static UBreakerAbilityStateComponent* FindOrAdd(AActor* Owner);

private:
    // The component keeps its own clock rather than reading world time, so the
    // rules are testable without a world and behave identically under pause.
    float Clock = 0.0f;

    struct FWindowState
    {
        float EndTime = 0.0f;
    };
    TMap<FName, FWindowState> Windows;

    TWeakObjectPtr<AActor> StreakTarget;
    int32 StreakCount = 0;
    float LastHitTime = -1000.0f;
};
