#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BreakerQuestJournal.generated.h"

// The campaign's durable record of story progress.
//
// Campaign-And-Story.md 6.4 fixes three properties and this container enforces
// them rather than trusting call sites: flags are PRESENCE-ONLY (a flag carries
// no payload), MONOTONIC (there is deliberately no Remove — a flag that can
// un-fire is a state machine hiding in a set), and NAMESPACED by act/mission.
//
// It is a USTRUCT with UPROPERTY members, where the shipped version was a bare
// `TArray<FName>` on the character with no reflection at all. That cost more
// than tidiness: without UPROPERTY the array is invisible to the property
// editor, unreachable from Blueprint, and cannot ever replicate. Save-Architecture
// 7 moves authority to a server, and an unreflected member is the one shape that
// cannot make that move.
//
// Counters exist because Campaign-And-Story.md 6.4 is explicit that a progress
// count ("kill 8") does not belong in the flag array. They are INTERMEDIATE
// state only: a counter that reaches its threshold sets a flag, and quest state
// is derived from flags alone. That keeps the save format from forking — the
// promise the campaign doc extracts in return for a quest object existing.
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerQuestFlagSet
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest") TArray<FName> Flags;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest") TMap<FName, int32> Counters;

    bool Has(FName Flag) const { return Flag != NAME_None && Flags.Contains(Flag); }
    bool HasAll(const TArray<FName>& Required) const;
    bool HasAny(const TArray<FName>& Any) const;

    // True only when the set actually changed. Every persistence decision in
    // this file hangs off that return value, so a repeated set costs no disk.
    bool Add(FName Flag);

    int32 GetCounter(FName Counter) const { const int32* Found = Counters.Find(Counter); return Found ? *Found : 0; }
    // Monotonic like the flags: a counter never decreases. Returns true when
    // the stored value changed.
    bool RaiseCounter(FName Counter, int32 NewValue);
    bool AddToCounter(FName Counter, int32 Delta) { return RaiseCounter(Counter, GetCounter(Counter) + FMath::Max(0, Delta)); }
};

// Raised after the flag set changed, once per change.
DECLARE_MULTICAST_DELEGATE_OneParam(FBreakerQuestFlagChanged, FName /*Flag*/);
// Raised when the journal believes its state must reach disk NOW. Whoever owns
// the save slot binds this; the journal deliberately does not know how a save
// is written, so it stays world-free and unit-testable.
DECLARE_MULTICAST_DELEGATE(FBreakerQuestPersistRequested);

// Runtime owner of quest state.
//
// THE BUG THIS CLASS EXISTS TO CLOSE: flags used to be set straight onto a
// plain array on the character, and the only calls to SaveGameState() were
// EndPlay and a handful of menu commit points. A flag earned in conversation
// reached disk only if the session later ended CLEANLY. A crash, a power loss,
// or a killed editor after a story beat silently lost the beat — with the
// campaign spine hanging act progress off these flags, that is data loss, not a
// design gap (Campaign-And-Story.md 6.3 #5, ranked as "Trust").
//
// The fix is write-through: a flag that changes the set requests a save in the
// same call. It is requested rather than performed so that the policy (which
// slot, what else goes in it) stays with the save owner.
UCLASS(BlueprintType)
class RIORSEDGE_API UBreakerQuestJournal : public UObject
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category="Quest") bool HasFlag(FName Flag) const { return State.Has(Flag); }
    UFUNCTION(BlueprintPure, Category="Quest") const TArray<FName>& GetFlags() const { return State.Flags; }
    UFUNCTION(BlueprintPure, Category="Quest") int32 GetCounter(FName Counter) const { return State.GetCounter(Counter); }

    // Returns true when the flag was new. Persists on change, never otherwise.
    UFUNCTION(BlueprintCallable, Category="Quest") bool SetFlag(FName Flag);
    // Raises a progress counter and, when it reaches Threshold, sets
    // CompletionFlag. One call so no caller can advance progress and forget to
    // close the objective.
    UFUNCTION(BlueprintCallable, Category="Quest") bool AddProgress(FName Counter, int32 Delta, int32 Threshold, FName CompletionFlag);

    const FBreakerQuestFlagSet& GetState() const { return State; }
    void RestoreFrom(const FBreakerQuestFlagSet& Restored) { State = Restored; }
    void RestoreFrom(const TArray<FName>& RestoredFlags, const TMap<FName, int32>& RestoredCounters);

    FBreakerQuestFlagChanged OnFlagSet;
    FBreakerQuestPersistRequested OnPersistRequested;

private:
    UPROPERTY() FBreakerQuestFlagSet State;
};
