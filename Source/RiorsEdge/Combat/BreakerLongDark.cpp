#include "Combat/BreakerStatusComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Progression/BreakerProgressionComponent.h"

void UBreakerStatusComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    RevokeLongDarkLease();
    for (const auto& Status : ActiveStatuses) ReleaseLongDarkLink(Status);
    Super::EndPlay(EndPlayReason);
}

bool UBreakerStatusComponent::CanMaintainLongDark() const
{
    const AActor* Owner = GetOwner();
    if (!IsValid(Owner) || !Owner->HasAuthority() || Owner->IsActorBeingDestroyed()) return false;
    const auto* Progression = Owner->FindComponentByClass<UBreakerProgressionComponent>();
    const auto* Sink = Owner->FindComponentByClass<UBreakerCombatComponent>();
    return Progression && Progression->GetNodeStats().bLongDark && Sink && !Sink->IsDead();
}

bool UBreakerStatusComponent::ReserveLongDarkLease(UBreakerStatusComponent* Target, uint64 Serial)
{
    const auto TargetAccepts = [Target]()
    {
        AActor* Actor = IsValid(Target) ? Target->GetOwner() : nullptr;
        const auto* Sink = IsValid(Actor) ? Actor->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
        return IsValid(Actor) && Actor->HasAuthority() && !Actor->IsActorBeingDestroyed()
            && Sink && !Sink->IsDead() && !Target->HasStatus(FGameplayTag::RequestGameplayTag(TEXT("Status.Rot")));
    };
    if (bChangingLongDarkLease || !CanMaintainLongDark() || Serial == 0 || !TargetAccepts()) return false;
    TGuardValue<bool> Changing(bChangingLongDarkLease, true);
    const TWeakObjectPtr<UBreakerStatusComponent> Previous = LongDarkTarget;
    const uint64 PreviousSerial = LongDarkSerial;
    // Publish the new reservation before old-status callbacks. Serial-specific
    // cleanup cannot clear it, and reentrant application cannot claim a second.
    LongDarkTarget = Target;
    LongDarkSerial = Serial;
    if (auto* OwnerCombat = GetOwner()->FindComponentByClass<UBreakerCombatComponent>())
        OwnerCombat->OnDeath.AddUniqueDynamic(this, &ThisClass::HandleLongDarkOwnerStateChanged);
    if (auto* Progression = GetOwner()->FindComponentByClass<UBreakerProgressionComponent>())
        Progression->OnProgressionChanged.AddUniqueDynamic(this, &ThisClass::HandleLongDarkOwnerStateChanged);
    GetOwner()->OnDestroyed.AddUniqueDynamic(this, &ThisClass::HandleLongDarkOwnerDestroyed);
    if (Previous.IsValid()) Previous->RemoveLongDarkApplication(PreviousSerial);
    if (LongDarkTarget == Target && LongDarkSerial == Serial && CanMaintainLongDark() && TargetAccepts()) return true;
    if (LongDarkTarget == Target && LongDarkSerial == Serial) { LongDarkTarget.Reset(); LongDarkSerial = 0; }
    return false;
}

bool UBreakerStatusComponent::IsLongDarkLeaseValid(const FBreakerActiveStatus& Status) const
{
    const auto* Source = Status.LongDarkSource.Get();
    return Status.bPersistentRot && Source && Source->CanMaintainLongDark()
        && Source->LongDarkTarget.Get() == this && Source->LongDarkSerial == Status.ApplicationSerial;
}

void UBreakerStatusComponent::ReleaseLongDarkLink(const FBreakerActiveStatus& Status)
{
    auto* Source = Status.LongDarkSource.Get();
    if (Source && Source->LongDarkTarget.Get() == this && Source->LongDarkSerial == Status.ApplicationSerial)
    {
        Source->LongDarkTarget.Reset();
        Source->LongDarkSerial = 0;
    }
}

void UBreakerStatusComponent::RemoveLongDarkApplication(uint64 Serial)
{
    const int32 Index = ActiveStatuses.IndexOfByPredicate([Serial](const FBreakerActiveStatus& Entry)
    { return Entry.ApplicationSerial == Serial && Entry.bPersistentRot; });
    if (Index == INDEX_NONE) return;
    const FBreakerActiveStatus Removed = ActiveStatuses[Index];
    ActiveStatuses.RemoveAt(Index);
    ReleaseLongDarkLink(Removed);
    OnStatusConsumed.Broadcast(Removed);
}

void UBreakerStatusComponent::RevokeLongDarkLease()
{
    const TWeakObjectPtr<UBreakerStatusComponent> Previous = LongDarkTarget;
    const uint64 PreviousSerial = LongDarkSerial;
    LongDarkTarget.Reset();
    LongDarkSerial = 0;
    TGuardValue<bool> Changing(bChangingLongDarkLease, true);
    if (Previous.IsValid()) Previous->RemoveLongDarkApplication(PreviousSerial);
}

void UBreakerStatusComponent::HandleLongDarkOwnerStateChanged()
{
    if (!CanMaintainLongDark()) RevokeLongDarkLease();
}

void UBreakerStatusComponent::HandleLongDarkOwnerDestroyed(AActor* DestroyedActor)
{
    RevokeLongDarkLease();
    for (const auto& Status : ActiveStatuses) ReleaseLongDarkLink(Status);
}
