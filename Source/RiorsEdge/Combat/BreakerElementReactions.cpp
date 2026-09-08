#include "Combat/BreakerElementReactions.h"
#include "Combat/BreakerStatusComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDamageLibrary.h"
#include "UI/BreakerReactionFeedback.h"
#include "Misc/ScopeExit.h"

float BreakerElementReactions::RemainingRotBudget(const FBreakerActiveStatus& Status)
{
    if (!FMath::IsFinite(Status.UnpaidDamageBudget) || Status.UnpaidDamageBudget <= 0
        || !FMath::IsFinite(Status.RemainingDuration) || Status.RemainingDuration <= 0
        || !FMath::IsFinite(Status.TimeUntilNextTick) || !FMath::IsFinite(Status.Spec.TickInterval)
        || Status.Spec.TickInterval <= 0 || !FMath::IsFinite(Status.Spec.BaseDamagePerTick)) return 0;
    const double Remaining = Status.RemainingDuration;
    const double Next = FMath::Max(0.0f, Status.TimeUntilNextTick);
    // Float status clocks can finish a few ulps short of an exact boundary.
    constexpr double BreakerReactionClockTolerance = 1.e-6;
    if (Next > Remaining + BreakerReactionClockTolerance) return 0;
    const double Ticks = 1 + FMath::FloorToDouble(FMath::Max(0.0, Remaining - Next + BreakerReactionClockTolerance) / Status.Spec.TickInterval);
    const double Scheduled = Ticks * FMath::Max(0.0f, Status.Spec.BaseDamagePerTick) * FMath::Max(1, Status.Stacks);
    return static_cast<float>(FMath::Min(static_cast<double>(Status.UnpaidDamageBudget), Scheduled));
}

uint64 UBreakerStatusComponent::PrepareElementReaction(const FBreakerDamageRequest& Request, const FBreakerDamageResult& Result)
{
    AActor* Target = GetOwner();
    const auto* Sink = Target ? Target->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
    if (IsElementTransactionActive() || !Target || !Target->HasAuthority() || Target->IsActorBeingDestroyed()
        || !Sink || Sink->IsDead() || Result.bKilled || Result.bDodged || Result.bParried || IsStatusImmune()
        || Request.bIsDamageOverTime || !Request.bCanApplyElementBuildup
        || !FMath::IsFinite(Request.ElementalFraction) || Request.ElementalFraction <= 0
        || !FMath::IsFinite(Request.ProcCoefficient) || Request.ProcCoefficient <= 0
        || !FMath::IsFinite(Result.HealthDamage) || !FMath::IsFinite(Result.ShieldDamage)
        || Result.HealthDamage + Result.ShieldDamage <= 0) return 0;
    const auto Rot = FGameplayTag::RequestGameplayTag(TEXT("Status.Rot"));
    const auto Erased = FGameplayTag::RequestGameplayTag(TEXT("Status.Erased"));
    FGameplayTag ConsumedTag, ReactionTag;
    if (Request.Element == EBreakerElement::Rift && HasStatus(Rot))
    {
        ConsumedTag = Rot;
        ReactionTag = FGameplayTag::RequestGameplayTag(TEXT("Reaction.Collapse"));
    }
    else if (Request.Element == EBreakerElement::Void && HasStatus(Rot))
    {
        ConsumedTag = Rot;
        ReactionTag = FGameplayTag::RequestGameplayTag(TEXT("Reaction.Wither"));
    }
    else if (Request.Element == EBreakerElement::Rift && HasStatus(Erased))
    {
        ConsumedTag = Erased;
        ReactionTag = FGameplayTag::RequestGameplayTag(TEXT("Reaction.Tear"));
    }
    if (!ConsumedTag.IsValid()) return 0;
    const FBreakerActiveStatus* Candidate = ActiveStatuses.FindByPredicate([ConsumedTag](const auto& Entry)
    { return Entry.Spec.StatusTag == ConsumedTag; });
    if (!Candidate) return 0;
    PendingReactionStatus = *Candidate;
    PendingReactionStatus.UnpaidDamageBudget = ConsumedTag == Rot
        ? BreakerElementReactions::RemainingRotBudget(*Candidate)
        : (FMath::IsFinite(Candidate->UnpaidDamageBudget) ? FMath::Max(0.0f, Candidate->UnpaidDamageBudget) : 0);
    if (Candidate->bHasReactionCreditSnapshot)
    {
        const float Fraction = Candidate->InitialDamageBudget > 0 && FMath::IsFinite(Candidate->InitialDamageBudget)
            ? FMath::Clamp(PendingReactionStatus.UnpaidDamageBudget / Candidate->InitialDamageBudget, 0.0f, 1.0f) : 0;
        PendingReactionStatus.UnpaidDamageBudget = FMath::IsFinite(Candidate->InitialReactionBudget)
            ? FMath::Max(0.0f, Candidate->InitialReactionBudget) * Fraction : 0;
    }
    PendingReactionTag = ReactionTag;
    PendingReactionToken = NextReactionToken++;
    const uint64 Token = PendingReactionToken;
    // Own the token before consume callbacks. A nested hit cannot create
    // another element/reaction; death cancels this budget without dropping
    // the guard before the outer hit has finished dispatching its callbacks.
    bool bFound = false;
    const FBreakerActiveStatus Consumed = ConsumeStatus(ConsumedTag, bFound);
    if (!bFound || Consumed.ApplicationSerial != PendingReactionStatus.ApplicationSerial)
        PendingReactionStatus.UnpaidDamageBudget = 0;
    return Token;
}

void UBreakerStatusComponent::FlushElementReaction(uint64 Token)
{
    if (Token == 0 || Token != PendingReactionToken || bFlushingReaction) return;
    TGuardValue<bool> Flushing(bFlushingReaction, true);
    ON_SCOPE_EXIT
    {
        if (PendingReactionToken == Token)
        {
            PendingReactionToken = 0;
            PendingReactionTag = FGameplayTag();
            PendingReactionStatus = FBreakerActiveStatus();
        }
    };
    AActor* Target = GetOwner();
    if (!Target || !Target->HasAuthority() || Target->IsActorBeingDestroyed()) return;
    if (!Combat) Combat = Target->FindComponentByClass<UBreakerCombatComponent>();
    if (!Combat || Combat->IsDead()) return;
    const FBreakerActiveStatus Consumed = PendingReactionStatus;
    const float Budget = Consumed.UnpaidDamageBudget;
    if (!FMath::IsFinite(Budget) || Budget <= 0) return;
    PendingReactionStatus.UnpaidDamageBudget = 0;
    FBreakerStatusApplicationSpec Spec = Consumed.Spec;
    Spec.BaseDamagePerTick = Budget;
    Spec.InitialStacks = 1;
    Spec.ProcCoefficient = 0;
    Spec.Snapshot.SourcePower = 1;
    Spec.Snapshot.CriticalChance = 0;
    Spec.Snapshot.bRolledCritical = false;
    FBreakerDamageRequest Payment = UBreakerDamageLibrary::MakeSnapshotDotTick(Spec,
        EBreakerDamageFamily::Elemental, 1, Consumed.Instigator.Get(),
        Consumed.SourceLocationSnapshot, Consumed.bHasSourceLocationSnapshot);
    Payment.DamageTypeTag = PendingReactionTag;
    Consumed.CopyThreatTo(Payment);
    Payment.bCanCritical = false;
    Payment.bCanApplyElementBuildup = false;
    Payment.bBypassShield = false;
    Payment.Element = EBreakerElement::None;
    Payment.ElementalFraction = 0;
    BreakerReactionFeedback::Play(Target, Consumed.Instigator.Get(), PendingReactionTag);
    Combat->ReceiveDamage(Payment);
}

void UBreakerStatusComponent::AdvanceRotStatus(uint64 ApplicationSerial, float DeltaSeconds)
{
    const auto Find = [this, ApplicationSerial]()
    { return ActiveStatuses.IndexOfByPredicate([ApplicationSerial](const auto& Entry)
        { return Entry.ApplicationSerial == ApplicationSerial; }); };
    double RemainingFrame = DeltaSeconds;
    constexpr double BreakerRotClockTolerance = 1.e-6;
    while (RemainingFrame > 0)
    {
        const int32 Index = Find();
        if (Index == INDEX_NONE || !Combat || Combat->IsDead()) return;
        FBreakerActiveStatus& Active = ActiveStatuses[Index];
        if (Active.bPersistentRot && !IsLongDarkLeaseValid(Active)) { RemoveLongDarkApplication(ApplicationSerial); return; }
        if (!Active.bPersistentRot && Active.RemainingDuration <= 0) break;
        const double ActiveFrame = Active.bPersistentRot ? RemainingFrame : FMath::Min(RemainingFrame, static_cast<double>(Active.RemainingDuration));
        const double Step = FMath::Min(ActiveFrame, static_cast<double>(FMath::Max(0.0f, Active.TimeUntilNextTick)));
        Active.RemainingDuration = FMath::Max(0.0f, Active.RemainingDuration - static_cast<float>(Step));
        Active.TimeUntilNextTick = FMath::Max(0.0f, Active.TimeUntilNextTick - static_cast<float>(Step));
        RemainingFrame = FMath::Max(0.0, RemainingFrame - Step);
        if (Active.TimeUntilNextTick <= BreakerRotClockTolerance)
        {
            Active.TimeUntilNextTick = Active.Spec.TickInterval;
            const float SnapshotTick = FMath::Max(0.0f, Active.Spec.BaseDamagePerTick) * FMath::Max(1, Active.Stacks);
            const float TickBudget = Active.bPersistentRot ? SnapshotTick : FMath::Min(Active.UnpaidDamageBudget, SnapshotTick);
            if (TickBudget > 0)
            {
                // Claim this one boundary before its callback. Remaining
                // lifetime/cadence now describe only the unpaid tail, even
                // during a large frame containing several later boundaries.
                Active.UnpaidDamageBudget = FMath::Max(0.0f, Active.UnpaidDamageBudget - TickBudget);
                ++Active.TicksDelivered;
                FBreakerStatusApplicationSpec TickSpec = Active.Spec;
                TickSpec.BaseDamagePerTick = TickBudget;
                TickSpec.InitialStacks = 1;
                TickSpec.Snapshot.SourcePower = 1;
                TickSpec.Snapshot.CriticalChance = 0;
                TickSpec.Snapshot.bRolledCritical = false;
                FBreakerDamageRequest Tick = UBreakerDamageLibrary::MakeSnapshotDotTick(TickSpec,
                    EBreakerDamageFamily::Elemental, Active.TicksDelivered, Active.Instigator.Get(),
                    Active.SourceLocationSnapshot, Active.bHasSourceLocationSnapshot);
                Tick.Element = EBreakerElement::Entropy;
                Active.CopyThreatTo(Tick);
                Tick.ElementalFraction = 1;
                Tick.bCanApplyElementBuildup = false;
                Tick.bCanCritical = false;
                Tick.bBypassShield = false;
                TGuardValue<FGameplayTag> Delivering(DeliveringTickTag, Active.Spec.StatusTag);
                Combat->ReceiveDamage(Tick);
            }
        }
        else if (Step <= 0) break;
    }
    const int32 Index = Find();
    if (Index != INDEX_NONE && !ActiveStatuses[Index].bPersistentRot && ActiveStatuses[Index].RemainingDuration <= BreakerRotClockTolerance)
    {
        const FBreakerActiveStatus Expired = ActiveStatuses[Index];
        ActiveStatuses.RemoveAt(Index);
        OnStatusExpired.Broadcast(Expired);
    }
}
