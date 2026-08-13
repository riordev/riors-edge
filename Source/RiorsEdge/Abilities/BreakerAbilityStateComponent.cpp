#include "Abilities/BreakerAbilityStateComponent.h"

#include "GameFramework/Actor.h"

UBreakerAbilityStateComponent::UBreakerAbilityStateComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

UBreakerAbilityStateComponent* UBreakerAbilityStateComponent::FindOrAdd(AActor* Owner)
{
    if (!Owner)
    {
        return nullptr;
    }
    if (UBreakerAbilityStateComponent* Existing = Owner->FindComponentByClass<UBreakerAbilityStateComponent>())
    {
        return Existing;
    }
    UBreakerAbilityStateComponent* Created = NewObject<UBreakerAbilityStateComponent>(Owner);
    Created->RegisterComponent();
    return Created;
}

void UBreakerAbilityStateComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    AdvanceTime(DeltaTime);
}

void UBreakerAbilityStateComponent::AdvanceTime(float DeltaSeconds)
{
    Clock += FMath::Max(DeltaSeconds, 0.0f);
    if (Windows.Num() == 0)
    {
        return;
    }

    // Collect first, broadcast after: a listener is allowed to open a new
    // window from inside OnWindowEnded, and iterating the map while it mutates
    // is a crash.
    TArray<FName, TInlineAllocator<4>> Expired;
    for (const TPair<FName, FWindowState>& Pair : Windows)
    {
        if (Pair.Value.EndTime <= Clock)
        {
            Expired.Add(Pair.Key);
        }
    }
    for (const FName Key : Expired)
    {
        Windows.Remove(Key);
    }
    for (const FName Key : Expired)
    {
        OnWindowEnded.Broadcast(Key);
    }
}

void UBreakerAbilityStateComponent::StartWindow(FName Key, float Duration)
{
    StartWindowWithPayload(Key, Duration, 0.0f);
}

void UBreakerAbilityStateComponent::StartWindowWithPayload(FName Key, float Duration, float Payload)
{
    if (Key.IsNone() || Duration <= 0.0f)
    {
        return;
    }
    FWindowState State;
    State.EndTime = Clock + Duration;
    State.Payload = Payload;
    Windows.Add(Key, State);
}

float UBreakerAbilityStateComponent::GetWindowPayload(FName Key, float DefaultValue) const
{
    const FWindowState* State = Windows.Find(Key);
    return (State && State->EndTime > Clock) ? State->Payload : DefaultValue;
}

void UBreakerAbilityStateComponent::ExtendWindow(FName Key, float ExtraSeconds)
{
    if (FWindowState* State = Windows.Find(Key))
    {
        State->EndTime += FMath::Max(ExtraSeconds, 0.0f);
    }
}

void UBreakerAbilityStateComponent::CloseWindow(FName Key)
{
    if (Windows.Remove(Key) > 0)
    {
        OnWindowEnded.Broadcast(Key);
    }
}

bool UBreakerAbilityStateComponent::IsWindowActive(FName Key) const
{
    const FWindowState* State = Windows.Find(Key);
    return State && State->EndTime > Clock;
}

float UBreakerAbilityStateComponent::GetWindowRemaining(FName Key) const
{
    const FWindowState* State = Windows.Find(Key);
    return State ? FMath::Max(State->EndTime - Clock, 0.0f) : 0.0f;
}

int32 UBreakerAbilityStateComponent::GetActiveWindowCount() const
{
    int32 Count = 0;
    for (const TPair<FName, FWindowState>& Pair : Windows)
    {
        if (Pair.Value.EndTime > Clock)
        {
            ++Count;
        }
    }
    return Count;
}

TArray<FName> UBreakerAbilityStateComponent::GetActiveWindowKeys() const
{
    TArray<FName> Keys;
    Keys.Reserve(Windows.Num());
    for (const TPair<FName, FWindowState>& Pair : Windows)
    {
        if (Pair.Value.EndTime > Clock)
        {
            Keys.Add(Pair.Key);
        }
    }
    // Stable order: the HUD stacks these vertically, and a TMap's iteration
    // order would make the rows swap places between frames.
    Keys.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });
    return Keys;
}

void UBreakerAbilityStateComponent::SetMark(AActor* Target, float Duration)
{
    if (!Target || Duration <= 0.0f)
    {
        ClearMark();
        return;
    }
    MarkTarget = Target;
    MarkEndTime = Clock + Duration;
}

AActor* UBreakerAbilityStateComponent::GetMarkedTarget() const
{
    // Expiry is evaluated on read, so no tick work is needed to retire a mark.
    return MarkEndTime > Clock ? MarkTarget.Get() : nullptr;
}

float UBreakerAbilityStateComponent::GetMarkRemaining() const
{
    return MarkTarget.IsValid() ? FMath::Max(MarkEndTime - Clock, 0.0f) : 0.0f;
}

void UBreakerAbilityStateComponent::ClearMark()
{
    MarkTarget = nullptr;
    MarkEndTime = -1000.0f;
}

bool UBreakerAbilityStateComponent::ShouldContinueStreak(bool bSameTarget, float SecondsSinceLastHit, float GapSeconds)
{
    return bSameTarget && SecondsSinceLastHit <= GapSeconds;
}

int32 UBreakerAbilityStateComponent::RecordHit(AActor* Target)
{
    if (!Target)
    {
        return StreakCount;
    }
    const bool bSameTarget = StreakTarget.Get() == Target;
    const float SinceLastHit = Clock - LastHitTime;
    StreakCount = ShouldContinueStreak(bSameTarget, SinceLastHit, StreakGapSeconds) ? StreakCount + 1 : 1;
    StreakTarget = Target;
    LastHitTime = Clock;
    return StreakCount;
}

int32 UBreakerAbilityStateComponent::GetStreak(AActor* Target) const
{
    if (!Target || StreakTarget.Get() != Target)
    {
        return 0;
    }
    // A stale streak reads as zero without needing a tick to clear it.
    return ShouldContinueStreak(true, Clock - LastHitTime, StreakGapSeconds) ? StreakCount : 0;
}

void UBreakerAbilityStateComponent::ResetStreak()
{
    StreakTarget = nullptr;
    StreakCount = 0;
    LastHitTime = -1000.0f;
}
