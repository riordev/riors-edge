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
// ---------------------------------------------------------------------------
// NODE-TAG CONSUMPTION: the report owns this, not a comment.
// ---------------------------------------------------------------------------
// A fallback-tree node that authors no stat effect ships as a GrantedTag and is
// worth nothing until some system reads it. Which ones are read is measured by
// `make status` -- the "Node tags with no consumer" and "Silent nodes" sections.
//
// A HAND-MAINTAINED LEDGER USED TO LIVE HERE AND IT WAS WORSE THAN NOTHING.
// It went stale in both directions -- tags it called inert had gained readers,
// tags it called consumed never had one -- and because the report's consumer
// index was a text scan of production source INCLUDING COMMENTS, every tag this
// block named counted as its own consumer. The ledger made the report agree
// with the ledger. Correcting the scan moved silent nodes 47 -> 54 and dead
// tags 118 -> 143; deleting the ledger moves neither, because the scan no
// longer reads it. It is deleted because a status list in a header is the
// annotation genre the working rules ban, and because what a specific node is
// waiting on belongs at that node's own site, where it already is.
//
// Query path, which is the one line here that was ever instruction rather than
// status:
//   UBreakerProgressionComponent::HasNodeTag(BreakerNodeTags::Node_X.GetTag())
// or, equivalently and just as visible to the report, by the tag's full string
// through an accessor -- RequestGameplayTag("Progression.Node....").
// ---------------------------------------------------------------------------
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
    // One scalar carried by the window itself, so a state that rewrites a rule
    // can also carry the rule's magnitude without a second registry. Unmake is
    // the first user: it opens a window whose payload is the cost scalar every
    // Caster ability multiplies by (0 base, 0.5 under the Long Dark keystone).
    UFUNCTION(BlueprintCallable, Category="Abilities|State") void StartWindowWithPayload(FName Key, float Duration, float Payload);
    UFUNCTION(BlueprintPure, Category="Abilities|State") float GetWindowPayload(FName Key, float DefaultValue = 0.0f) const;
    UFUNCTION(BlueprintCallable, Category="Abilities|State") void ExtendWindow(FName Key, float ExtraSeconds);
    // Closing broadcasts OnWindowEnded exactly like a natural expiry, so
    // listeners have one teardown path.
    UFUNCTION(BlueprintCallable, Category="Abilities|State") void CloseWindow(FName Key);
    UFUNCTION(BlueprintPure, Category="Abilities|State") bool IsWindowActive(FName Key) const;
    UFUNCTION(BlueprintPure, Category="Abilities|State") float GetWindowRemaining(FName Key) const;
    UFUNCTION(BlueprintPure, Category="Abilities|State") int32 GetActiveWindowCount() const;
    // Keys of every window still open, so a cosmetic reader (the HUD) can show
    // "whatever is running" without hard-coding the ability roster.
    UFUNCTION(BlueprintPure, Category="Abilities|State") TArray<FName> GetActiveWindowKeys() const;

    // Marks ---------------------------------------------------------------
    // A single mark, held on the *caster's* state component rather than on the
    // target, so a destroyed target simply reads back as null. This is the
    // minimum surface the HUD needs; UBreakerMarkComponent (spec §4.6) replaces
    // it wholesale when it lands.
    UFUNCTION(BlueprintCallable, Category="Abilities|State") void SetMark(AActor* Target, float Duration);
    UFUNCTION(BlueprintPure, Category="Abilities|State") AActor* GetMarkedTarget() const;
    UFUNCTION(BlueprintPure, Category="Abilities|State") float GetMarkRemaining() const;
    UFUNCTION(BlueprintCallable, Category="Abilities|State") void ClearMark();

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
        float Payload = 0.0f;
    };
    TMap<FName, FWindowState> Windows;

    // Shares the component's Clock, so the mark expires on exactly the same
    // schedule as the window that opened it.
    TWeakObjectPtr<AActor> MarkTarget;
    float MarkEndTime = -1000.0f;

    TWeakObjectPtr<AActor> StreakTarget;
    int32 StreakCount = 0;
    float LastHitTime = -1000.0f;
};
