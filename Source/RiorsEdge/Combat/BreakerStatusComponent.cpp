#include "Combat/BreakerStatusComponent.h"

#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDamageLibrary.h"

UBreakerStatusComponent::UBreakerStatusComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UBreakerStatusComponent::BeginPlay()
{
    Super::BeginPlay();
    Combat = GetOwner() ? GetOwner()->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
}

void UBreakerStatusComponent::ApplyStatus(const FBreakerStatusApplicationSpec& Spec, EBreakerDamageFamily DamageFamily, AActor* Instigator)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || Spec.Duration <= 0.0f || Spec.TickInterval <= 0.0f) return;

    for (FBreakerActiveStatus& Active : ActiveStatuses)
    {
        if (Active.Spec.StatusTag == Spec.StatusTag)
        {
            Active.Stacks = FMath::Min(Active.Stacks + FMath::Max(1, Spec.InitialStacks), GetEffectiveStackCap());
            Active.RemainingDuration = FMath::Max(Active.RemainingDuration, Spec.Duration);
            // Refresh credit to whoever most recently reapplied it — and the
            // facing snapshot with it, because credit and angle belong to the
            // same application.
            if (Instigator)
            {
                Active.Instigator = Instigator;
                Active.SourceLocationSnapshot = Instigator->GetActorLocation();
                Active.bHasSourceLocationSnapshot = true;
            }
            OnStatusApplied.Broadcast(Active);
            return;
        }
    }

    FBreakerActiveStatus Status;
    Status.Spec = Spec;
    Status.DamageFamily = DamageFamily;
    Status.Stacks = FMath::Clamp(Spec.InitialStacks, 1, GetEffectiveStackCap());
    Status.RemainingDuration = Spec.Duration;
    Status.TimeUntilNextTick = Spec.TickInterval;
    Status.Instigator = Instigator;
    // Application-time facing snapshot. Taken from the applier's position NOW,
    // not per tick: the DoT contract snapshots at application, and a tick that
    // re-read the applier's live position would let a shooter flank AFTER the
    // wound to retroactively strip armour off every remaining tick.
    if (Instigator)
    {
        Status.SourceLocationSnapshot = Instigator->GetActorLocation();
        Status.bHasSourceLocationSnapshot = true;
    }
    ActiveStatuses.Add(Status);
    OnStatusApplied.Broadcast(Status);
}

void UBreakerStatusComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    AdvanceStatuses(DeltaTime);
}

void UBreakerStatusComponent::AdvanceStatuses(float DeltaTime)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !Combat || ActiveStatuses.IsEmpty()) return;

    for (int32 Index = ActiveStatuses.Num() - 1; Index >= 0; --Index)
    {
        FBreakerActiveStatus& Status = ActiveStatuses[Index];
        Status.RemainingDuration -= DeltaTime;
        Status.TimeUntilNextTick -= DeltaTime;

        if (Status.TimeUntilNextTick <= 0.0f)
        {
            Status.TimeUntilNextTick += Status.Spec.TickInterval;
            ++Status.TicksDelivered;

            FBreakerStatusApplicationSpec TickSpec = Status.Spec;
            TickSpec.InitialStacks = Status.Stacks;
            FBreakerDamageRequest Tick = UBreakerDamageLibrary::MakeSnapshotDotTick(TickSpec, Status.DamageFamily, Status.TicksDelivered, Status.Instigator.Get(),
                Status.SourceLocationSnapshot, Status.bHasSourceLocationSnapshot);
            // Physical DoTs — Bleed, Poison — ignore shields and take half
            // armour mitigation via the damage library's global status rule.
            Tick.bBypassShield = Status.DamageFamily == EBreakerDamageFamily::Physical;
            Combat->ReceiveDamage(Tick);
        }

        if (Status.RemainingDuration <= 0.0f)
        {
            OnStatusExpired.Broadcast(Status);
            ActiveStatuses.RemoveAt(Index);
        }
    }
}

bool UBreakerStatusComponent::HasStatus(FGameplayTag StatusTag) const
{
    return ActiveStatuses.ContainsByPredicate([StatusTag](const FBreakerActiveStatus& Status) { return Status.Spec.StatusTag == StatusTag; });
}

int32 UBreakerStatusComponent::GetEffectiveStackCap() const
{
    // Never below one: a cap of zero would make ApplyStatus create a status
    // with zero stacks that ticks for nothing and never expires early.
    return FMath::Max(1, MaximumStacksPerStatus + StackCapDelta);
}

int32 UBreakerStatusComponent::GetDistinctStatusTypeCount() const
{
    // ApplyStatus already guarantees one entry per tag, so the list length IS
    // the distinct count. Counted through a set anyway: the invariant is worth
    // one allocation on a list that is never longer than a handful, and if the
    // invariant is ever broken this keeps Resonance honest instead of paying
    // out twice for the same status.
    TSet<FGameplayTag> Distinct;
    for (const FBreakerActiveStatus& Status : ActiveStatuses)
    {
        if (Status.Spec.StatusTag.IsValid()) Distinct.Add(Status.Spec.StatusTag);
    }
    return Distinct.Num();
}

TArray<FBreakerActiveStatus> UBreakerStatusComponent::ConsumeAllStatuses()
{
    TArray<FBreakerActiveStatus> Consumed;
    if (!GetOwner() || !GetOwner()->HasAuthority()) return Consumed;

    // Move first, broadcast second. A listener that reapplies a status (Cascade
    // does exactly that) must land in an EMPTY list, not into the array being
    // iterated — otherwise the reapplication is consumed by its own detonation.
    Consumed = MoveTemp(ActiveStatuses);
    ActiveStatuses.Reset();
    for (const FBreakerActiveStatus& Status : Consumed) OnStatusConsumed.Broadcast(Status);
    return Consumed;
}

FBreakerActiveStatus UBreakerStatusComponent::ConsumeStatus(FGameplayTag StatusTag, bool& bOutFound)
{
    bOutFound = false;
    FBreakerActiveStatus Consumed;
    if (!GetOwner() || !GetOwner()->HasAuthority()) return Consumed;

    for (int32 Index = 0; Index < ActiveStatuses.Num(); ++Index)
    {
        if (ActiveStatuses[Index].Spec.StatusTag != StatusTag) continue;
        Consumed = ActiveStatuses[Index];
        ActiveStatuses.RemoveAt(Index);
        bOutFound = true;
        OnStatusConsumed.Broadcast(Consumed);
        return Consumed;
    }
    // Consuming a status that is not present is a legal no-op. Callers branch
    // on bOutFound; returning a default-constructed status with a zero
    // magnitude means a caller that ignores the flag still detonates for zero
    // rather than for garbage.
    return Consumed;
}

void UBreakerStatusComponent::ScaleRemainingDurations(float Scalar)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    const float Clamped = FMath::Max(0.0f, Scalar);
    for (int32 Index = ActiveStatuses.Num() - 1; Index >= 0; --Index)
    {
        ActiveStatuses[Index].RemainingDuration *= Clamped;
        // A scalar of zero means gone, and gone is gone by exactly one route:
        // it broadcasts as a consumption, because a caller that shortened a
        // duration to nothing did consume it.
        if (ActiveStatuses[Index].RemainingDuration <= 0.0f)
        {
            const FBreakerActiveStatus Removed = ActiveStatuses[Index];
            ActiveStatuses.RemoveAt(Index);
            OnStatusConsumed.Broadcast(Removed);
        }
    }
}

void UBreakerStatusComponent::SetStackCapDelta(int32 Delta)
{
    StackCapDelta = Delta;
    const int32 Cap = GetEffectiveStackCap();
    // Lowering the cap trims immediately. A live status carrying more stacks
    // than the cap allows would keep ticking at the old magnitude and read as
    // a broken cap.
    for (FBreakerActiveStatus& Status : ActiveStatuses) Status.Stacks = FMath::Min(Status.Stacks, Cap);
}
