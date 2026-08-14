#include "Save/BreakerQuestJournal.h"

bool FBreakerQuestFlagSet::HasAll(const TArray<FName>& Required) const
{
    for (const FName& Flag : Required)
    {
        if (!Has(Flag)) return false;
    }
    return true;
}

bool FBreakerQuestFlagSet::HasAny(const TArray<FName>& Any) const
{
    for (const FName& Flag : Any)
    {
        if (Has(Flag)) return true;
    }
    return false;
}

bool FBreakerQuestFlagSet::Add(FName Flag)
{
    // NAME_None is the "no flag authored" value on every dialogue choice, so it
    // arrives here constantly and must never become a stored flag.
    if (Flag == NAME_None || Flags.Contains(Flag)) return false;
    Flags.Add(Flag);
    return true;
}

bool FBreakerQuestFlagSet::RaiseCounter(FName Counter, int32 NewValue)
{
    if (Counter == NAME_None) return false;
    const int32 Current = GetCounter(Counter);
    if (NewValue <= Current) return false;
    Counters.Add(Counter, NewValue);
    return true;
}

bool UBreakerQuestJournal::SetFlag(FName Flag)
{
    if (!State.Add(Flag)) return false;
    OnFlagSet.Broadcast(Flag);
    // Write-through. The failure mode this prevents is the one that made the
    // whole flag system untrustworthy: a story beat that lives only in memory
    // until a clean shutdown that may never come.
    OnPersistRequested.Broadcast();
    return true;
}

bool UBreakerQuestJournal::AddProgress(FName Counter, int32 Delta, int32 Threshold, FName CompletionFlag)
{
    const bool bCounterMoved = State.AddToCounter(Counter, Delta);
    const bool bComplete = Threshold > 0 && State.GetCounter(Counter) >= Threshold;
    if (bComplete && State.Add(CompletionFlag))
    {
        OnFlagSet.Broadcast(CompletionFlag);
        OnPersistRequested.Broadcast();
        return true;
    }
    // A counter tick alone is cheap state, but it is also the only record that
    // partial progress happened. Persist it too — a save write per kill is not
    // free, so the objective threshold is what bounds how often this can fire
    // (the counter stops moving once the flag is set).
    if (bCounterMoved) OnPersistRequested.Broadcast();
    return false;
}

void UBreakerQuestJournal::RestoreFrom(const TArray<FName>& RestoredFlags, const TMap<FName, int32>& RestoredCounters)
{
    State.Flags = RestoredFlags;
    State.Counters = RestoredCounters;
}
