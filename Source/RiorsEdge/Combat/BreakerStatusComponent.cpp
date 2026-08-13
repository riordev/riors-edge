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
            Active.Stacks = FMath::Min(Active.Stacks + FMath::Max(1, Spec.InitialStacks), MaximumStacksPerStatus);
            Active.RemainingDuration = FMath::Max(Active.RemainingDuration, Spec.Duration);
            // Refresh credit to whoever most recently reapplied it.
            if (Instigator) Active.Instigator = Instigator;
            OnStatusApplied.Broadcast(Active);
            return;
        }
    }

    FBreakerActiveStatus Status;
    Status.Spec = Spec;
    Status.DamageFamily = DamageFamily;
    Status.Stacks = FMath::Clamp(Spec.InitialStacks, 1, MaximumStacksPerStatus);
    Status.RemainingDuration = Spec.Duration;
    Status.TimeUntilNextTick = Spec.TickInterval;
    Status.Instigator = Instigator;
    ActiveStatuses.Add(Status);
    OnStatusApplied.Broadcast(Status);
}

void UBreakerStatusComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
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
            FBreakerDamageRequest Tick = UBreakerDamageLibrary::MakeSnapshotDotTick(TickSpec, Status.DamageFamily, Status.TicksDelivered, Status.Instigator.Get());
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
