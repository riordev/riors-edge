#include "Abilities/BreakerAbilityStateComponent.h"

#include "GameFramework/Actor.h"
#include "Combat/BreakerCombatComponent.h"

UBreakerAbilityStateComponent::UBreakerAbilityStateComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UBreakerAbilityStateComponent::SetMaintainedBuffRecipients(FName OwnerKey, const TArray<AActor*>& Recipients)
{
    if (OwnerKey.IsNone()) return;
    auto& Held = MaintainedBuffRecipients.FindOrAdd(OwnerKey);
    const auto Previous = Held;
    Held.Reset();
    for (AActor* Recipient : Recipients)
        if (IsValid(Recipient)) Held.AddUnique(Recipient);
    const bool bChanged = Previous != Held;
    if (Held.IsEmpty()) MaintainedBuffRecipients.Remove(OwnerKey);
    if (bChanged) OnMaintainedBuffRecipientsChanged.Broadcast();
}

void UBreakerAbilityStateComponent::ClearMaintainedBuffRecipients(FName OwnerKey)
{
    if (MaintainedBuffRecipients.Remove(OwnerKey) > 0) OnMaintainedBuffRecipientsChanged.Broadcast();
}

int32 UBreakerAbilityStateComponent::GetMaintainedBuffRecipientCount() const
{
    const AActor* Source = GetOwner();
    const auto* SourceCombat = Source ? Source->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
    if (!IsValid(Source) || Source->IsActorBeingDestroyed() || (SourceCombat && SourceCombat->IsDead())) return 0;
    TSet<AActor*> Unique;
    for (const auto& Buff : MaintainedBuffRecipients)
        for (const auto& Held : Buff.Value)
            if (AActor* Recipient = Held.Get(); IsValid(Recipient) && !Recipient->IsActorBeingDestroyed())
                if (const auto* Combat = Recipient->FindComponentByClass<UBreakerCombatComponent>(); Combat && !Combat->IsDead())
                    Unique.Add(Recipient);
    return Unique.Num();
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
    if (Windows.Num() == 0 && OwnedWindows.Num() == 0)
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
    for (auto Group = OwnedWindows.CreateIterator(); Group; ++Group)
    {
        for (auto Owner = Group.Value().CreateIterator(); Owner; ++Owner)
            if (Owner.Value() <= Clock) Owner.RemoveCurrent();
        if (Group.Value().IsEmpty()) { Expired.AddUnique(Group.Key()); Group.RemoveCurrent(); }
    }
    for (const FName Key : Expired)
    {
        if (!IsWindowActive(Key)) OnWindowEnded.Broadcast(Key);
    }
}

void UBreakerAbilityStateComponent::StartWindow(FName Key, float Duration)
{
    StartWindowWithPayload(Key, Duration, 0.0f);
}

void UBreakerAbilityStateComponent::StartOwnedWindow(FName Key, FName OwnerKey, float Duration)
{
    if (!Key.IsNone() && !OwnerKey.IsNone() && FMath::IsFinite(Duration) && Duration > 0)
        OwnedWindows.FindOrAdd(Key).Add(OwnerKey, Clock + Duration);
}

float UBreakerAbilityStateComponent::GetOwnedWindowRemaining(FName Key, FName OwnerKey) const
{
    const auto* Group = OwnedWindows.Find(Key);
    const float* End = Group ? Group->Find(OwnerKey) : nullptr;
    return End ? FMath::Max(0.0f, *End - Clock) : 0.0f;
}

void UBreakerAbilityStateComponent::CloseOwnedWindow(FName Key, FName OwnerKey)
{
    auto* Group = OwnedWindows.Find(Key);
    if (!Group || Group->Remove(OwnerKey) == 0) return;
    if (Group->IsEmpty()) OwnedWindows.Remove(Key);
    if (!IsWindowActive(Key)) OnWindowEnded.Broadcast(Key);
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
    if (auto* Group = OwnedWindows.Find(Key))
        for (auto& Owner : *Group) if (Owner.Value > Clock) Owner.Value += FMath::Max(0.0f, ExtraSeconds);
}

void UBreakerAbilityStateComponent::CloseWindow(FName Key)
{
    const bool bRemoved = Windows.Remove(Key) > 0;
    const bool bOwnedRemoved = OwnedWindows.Remove(Key) > 0;
    if (bRemoved || bOwnedRemoved)
    {
        OnWindowEnded.Broadcast(Key);
    }
}

bool UBreakerAbilityStateComponent::IsWindowActive(FName Key) const
{
    return GetWindowRemaining(Key) > 0;
}

float UBreakerAbilityStateComponent::GetWindowRemaining(FName Key) const
{
    const FWindowState* State = Windows.Find(Key);
    float Remaining = State ? FMath::Max(State->EndTime - Clock, 0.0f) : 0.0f;
    if (const auto* Group = OwnedWindows.Find(Key))
        for (const auto& Owner : *Group) Remaining = FMath::Max(Remaining, Owner.Value - Clock);
    return Remaining;
}

int32 UBreakerAbilityStateComponent::GetActiveWindowCount() const
{
    return GetActiveWindowKeys().Num();
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
    for (const auto& Group : OwnedWindows)
        if (IsWindowActive(Group.Key)) Keys.AddUnique(Group.Key);
    // order would make the rows swap places between frames.
    Keys.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });
    return Keys;
}

void UBreakerAbilityStateComponent::SetMark(AActor* Target, float Duration)
{
    ClearMark();
    AddMark(Target, Duration, 1);
}

void UBreakerAbilityStateComponent::AddMark(AActor* Target, float Duration, int32 Capacity)
{
    Marks.RemoveAll([this, Target](const FMarkState& Mark)
    {
        const UBreakerCombatComponent* Combat = Mark.Target.IsValid() ? Mark.Target->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
        return !Mark.Target.IsValid() || Mark.EndTime <= Clock || Mark.Target.Get() == Target || (Combat && Combat->IsDead());
    });
    if (!Target || Duration <= 0) return;
    const int32 Limit = FMath::Clamp(Capacity, 1, 2);
    while (Marks.Num() >= Limit) Marks.RemoveAt(0);
    FMarkState Mark;
    Mark.Target = Target; Mark.EndTime = Clock + Duration;
    Marks.Add(Mark);
}

TArray<AActor*> UBreakerAbilityStateComponent::GetMarkedTargets() const
{
    TArray<AActor*> Targets;
    for (const FMarkState& Mark : Marks)
        if (Mark.EndTime > Clock && Mark.Target.IsValid())
        {
            const UBreakerCombatComponent* Combat = Mark.Target->FindComponentByClass<UBreakerCombatComponent>();
            if (!Combat || !Combat->IsDead()) Targets.Add(Mark.Target.Get());
        }
    return Targets;
}

AActor* UBreakerAbilityStateComponent::GetMarkedTarget() const
{
    const TArray<AActor*> Targets = GetMarkedTargets();
    return Targets.IsEmpty() ? nullptr : Targets.Last();
}

float UBreakerAbilityStateComponent::GetMarkRemainingFor(const AActor* Target) const
{
    for (const FMarkState& Mark : Marks)
        if (Mark.Target.IsValid() && Mark.Target.Get() == Target) return FMath::Max(0.0f, Mark.EndTime - Clock);
    return 0;
}

bool UBreakerAbilityStateComponent::IsMarked(const AActor* Target) const
{
    const UBreakerCombatComponent* Combat = Target ? Target->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
    return Target && (!Combat || !Combat->IsDead()) && GetMarkRemainingFor(Target) > 0;
}
float UBreakerAbilityStateComponent::GetMarkRemaining() const { return GetMarkRemainingFor(GetMarkedTarget()); }
void UBreakerAbilityStateComponent::ClearMark() { Marks.Reset(); }

bool UBreakerAbilityStateComponent::ConsumeMarkRefund(const AActor* Target)
{
    for (FMarkState& Mark : Marks)
        if (Mark.Target.Get() == Target && Mark.EndTime > Clock && !Mark.bRefunded)
        { Mark.bRefunded = true; return true; }
    return false;
}

void UBreakerAbilityStateComponent::TransferMark(const AActor* From, AActor* To)
{
    if (!To || IsMarked(To)) return;
    for (FMarkState& Mark : Marks)
        if (Mark.Target.Get() == From && Mark.EndTime > Clock)
        { Mark.Target = To; return; }
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
