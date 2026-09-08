#include "Combat/BreakerElementReactions.h"
#include "Combat/BreakerStatusComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Combat/BreakerElementSourceMath.h"
#include "Combat/BreakerEnemy.h"
#include "Classes/BreakerManaComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "EngineUtils.h"
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

uint64 UBreakerStatusComponent::PrepareElementReaction(const FBreakerDamageRequest& Request, const FBreakerDamageResult& Result, uint64 RequiredHitToken)
{
    return PrepareElementReactionBatch({Request}, Result, RequiredHitToken);
}

uint64 UBreakerStatusComponent::PrepareElementReactionBatch(const TArray<FBreakerDamageRequest>& Requests,
    const FBreakerDamageResult& Result, uint64 RequiredHitToken)
{
    AActor* Target = GetOwner();
    const auto* Sink = Target ? Target->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
    if (IsElementTransactionActive() || !Target || !Target->HasAuthority() || Target->IsActorBeingDestroyed()
        || !Sink || Sink->IsDead() || Result.bKilled || Result.bDodged || Result.bParried || IsStatusImmune()
        || !FMath::IsFinite(Result.HealthDamage) || !FMath::IsFinite(Result.ShieldDamage)
        || Result.HealthDamage + Result.ShieldDamage <= 0) return 0;
    const auto Rot = FGameplayTag::RequestGameplayTag(TEXT("Status.Rot"));
    const auto Erased = FGameplayTag::RequestGameplayTag(TEXT("Status.Erased"));
    struct FCandidate { FBreakerActiveStatus Status; FGameplayTag Reaction; };
    TArray<FCandidate> Candidates;
    AActor* ExpandingCreditor = nullptr;
    bool bExpand = false;
    for (const auto& Request : Requests)
    {
        if (Request.bIsDamageOverTime || !Request.bCanApplyElementBuildup
            || !FMath::IsFinite(Request.ElementalFraction) || Request.ElementalFraction <= 0
            || !FMath::IsFinite(Request.ProcCoefficient) || Request.ProcCoefficient <= 0) continue;
        for (const auto Tag : {Rot, Erased})
        {
            FGameplayTag Reaction;
            if (Tag == Rot && Request.Element == EBreakerElement::Rift) Reaction = FGameplayTag::RequestGameplayTag(TEXT("Reaction.Collapse"));
            else if (Tag == Rot && Request.Element == EBreakerElement::Void) Reaction = FGameplayTag::RequestGameplayTag(TEXT("Reaction.Wither"));
            else if (Tag == Erased && Request.Element == EBreakerElement::Rift) Reaction = FGameplayTag::RequestGameplayTag(TEXT("Reaction.Tear"));
            if (!Reaction.IsValid()) continue;
            const auto* Candidate = ActiveStatuses.FindByPredicate([Tag](const FBreakerActiveStatus& Entry) { return Entry.Spec.StatusTag == Tag; });
            if (!Candidate || (RequiredHitToken != 0 && (Candidate->OriginElementHitToken != RequiredHitToken
                || Candidate->Instigator != Request.Instigator))) continue;
            if (Candidates.ContainsByPredicate([&](const FCandidate& Existing) { return Existing.Status.ApplicationSerial == Candidate->ApplicationSerial; })) continue;
            if (Candidates.IsEmpty())
            {
                ExpandingCreditor = Candidate->Instigator.Get();
                const auto* Progression = ExpandingCreditor ? ExpandingCreditor->FindComponentByClass<UBreakerProgressionComponent>() : nullptr;
                bExpand = Progression && Progression->HasNodeTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Reaction.Sympathetic")));
            }
            else if (!bExpand || Candidate->Instigator.Get() != ExpandingCreditor) continue;
            Candidates.Add({*Candidate, Reaction});
            if (!bExpand) break;
        }
        if (!Candidates.IsEmpty() && !bExpand) break;
    }
    if (Candidates.IsEmpty()) return 0;
    // Original creditor owns both the throttle and its capped compensation.
    // Reserve successful timestamps before any callback. Sympathetic's second
    // candidate is another reaction, so Conductor can refuse it independently.
    ConductorReservations.RemoveAll([](const auto& Entry) { return !Entry.Source.IsValid(); });
    TArray<TWeakObjectPtr<AActor>> ConductorRefunds;
    TArray<FCandidate> ThrottledCandidates;
    for (const auto& Candidate : Candidates)
    {
        AActor* Source = Candidate.Status.Instigator.Get();
        const auto* Progression = Source ? Source->FindComponentByClass<UBreakerProgressionComponent>() : nullptr;
        const float Funded = Candidate.Status.Spec.StatusTag == Rot ? BreakerElementReactions::RemainingRotBudget(Candidate.Status) : Candidate.Status.UnpaidDamageBudget;
        if (!FMath::IsFinite(Funded) || Funded <= 0 || !Progression
            || !Progression->HasNodeTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Caster.Multispell.ConductorRule"))))
        { ThrottledCandidates.Add(Candidate); continue; }
        auto* Reservation = ConductorReservations.FindByPredicate([Source](const auto& Entry) { return Entry.Source.Get() == Source; });
        // Match the element clock boundary tolerance: fifty float .01s slices
        // accumulate just below .5 when promoted into the double clock.
        if (Reservation && Reservation->ReadyTime - ElementClock > 1.e-6)
        { ConductorRefunds.Add(Source); continue; }
        if (!Reservation) { Reservation = &ConductorReservations.AddDefaulted_GetRef(); Reservation->Source = Source; }
        if (auto* SourceStatus = Source->FindComponentByClass<UBreakerStatusComponent>())
            if (auto* SourceCombat = Source->FindComponentByClass<UBreakerCombatComponent>())
                SourceCombat->OnDeath.AddUniqueDynamic(SourceStatus, &UBreakerStatusComponent::HandleAfflictedOwnerDeath);
        Reservation->ReadyTime = ElementClock + .5; // O2 PLACEHOLDER, authored MS11 half-second gate.
        ThrottledCandidates.Add(Candidate);
    }
    Candidates = MoveTemp(ThrottledCandidates);
    auto PayConductorRefusals = [&]()
    {
        TGuardValue<bool> Refunding(bConductorCallback, true);
        for (const auto& Source : ConductorRefunds)
            if (Source.IsValid())
                if (auto* Mana = Source->FindComponentByClass<UBreakerManaComponent>())
                    Mana->GrantMana(10.f, false); // O2 PLACEHOLDER, authored capped refusal income.
    };
    if (Candidates.IsEmpty())
    {
        // Eligible but throttled is still this hit's reaction decision. A
        // claimed empty transaction prevents falling through into buildup or
        // asking Second Order for the same compensation a second time.
        PendingReactionToken = NextReactionToken++;
        PendingElementReactions.Reset(); bReactionCanceled = false;
        const uint64 Token = PendingReactionToken;
        PayConductorRefusals();
        return Token;
    }
    // There are exactly two consumable status identities in the existing pair
    // table. Unstable has no invented reaction and no recyclable paid burst.
    check(Candidates.Num() <= 2);
    PendingElementReactions.Reset();
    TArray<float> Residues, ReactionBudgets;
    for (const auto& Candidate : Candidates)
    {
        FPendingElementReaction Pending;
        Pending.Status = Candidate.Status;
        Pending.Tag = Candidate.Reaction;
        Pending.bSympathetic = bExpand;
        AActor* Creditor = Candidate.Status.Instigator.Get();
        const auto* Progression = Creditor ? Creditor->FindComponentByClass<UBreakerProgressionComponent>() : nullptr;
        if (Progression && Progression->HasNodeTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Reaction.Chain"))))
        {
            double BestDistance = FMath::Square(500.0); // O2 PLACEHOLDER, authored 5m.
            for (TActorIterator<ABreakerEnemy> It(GetWorld()); It; ++It)
            {
                auto* Enemy = *It; const auto* EnemyCombat = Enemy->FindComponentByClass<UBreakerCombatComponent>();
                if (Enemy == Target || Enemy == Creditor || !IsValid(Enemy) || Enemy->IsActorBeingDestroyed() || !EnemyCombat || EnemyCombat->IsDead()) continue;
                const double Distance = FVector::DistSquared(Target->GetActorLocation(), Enemy->GetActorLocation());
                if (Distance > BestDistance || (Distance == BestDistance && Pending.ChainTarget.IsValid()
                    && Enemy->GetUniqueID() >= Pending.ChainTarget->GetUniqueID())) continue;
                Pending.ChainTarget = Enemy; BestDistance = Distance;
            }
        }
        const float Normal = Candidate.Status.Spec.StatusTag == Rot ? BreakerElementReactions::RemainingRotBudget(Candidate.Status)
            : (FMath::IsFinite(Candidate.Status.UnpaidDamageBudget) ? FMath::Max(0.0f, Candidate.Status.UnpaidDamageBudget) : 0.f);
        float Reaction = Normal;
        if (Candidate.Status.bHasReactionCreditSnapshot)
        {
            const float Fraction = FMath::IsFinite(Candidate.Status.InitialDamageBudget) && Candidate.Status.InitialDamageBudget > 0
                ? FMath::Clamp(Normal / Candidate.Status.InitialDamageBudget, 0.f, 1.f) : 0.f;
            Reaction = FMath::IsFinite(Candidate.Status.InitialReactionBudget) ? FMath::Max(0.f, Candidate.Status.InitialReactionBudget) * Fraction : 0.f;
        }
        const float Authored = Progression ? Progression->GetNodeStats().ReactionResiduePercent : 0.f;
        const float Residue = FMath::IsFinite(Authored) ? FMath::Clamp(Authored / 100.f, 0.f, 1.f) : 0.f;
        Pending.Status.UnpaidDamageBudget = Reaction * (1.f - Residue);
        PendingElementReactions.Add(Pending); Residues.Add(Residue); ReactionBudgets.Add(Reaction);
    }
    PendingReactionToken = NextReactionToken++;
    bReactionCanceled = false;
    const uint64 Token = PendingReactionToken;
    TArray<FBreakerActiveStatus> ConsumedNotifications;
    for (int32 Index = 0; Index < Candidates.Num(); ++Index)
    {
        bool bFound = false;
        const auto Consumed = ConsumeReactionStatus(Candidates[Index].Status.Spec.StatusTag, Residues[Index], ReactionBudgets[Index], bFound, false);
        if (!bFound || Consumed.ApplicationSerial != Candidates[Index].Status.ApplicationSerial)
            PendingElementReactions[Index].Status.UnpaidDamageBudget = 0;
        else ConsumedNotifications.Add(Consumed);
    }
    // Every original has been consumed and every residual installed before
    // the first external callback. Nested hits see the transaction guard.
    for (const auto& Consumed : ConsumedNotifications) OnStatusConsumed.Broadcast(Consumed);
    PayConductorRefusals();
    return Token;
}

void UBreakerStatusComponent::FlushElementReaction(uint64 Token)
{
    if (Token == 0 || Token != PendingReactionToken || bFlushingReaction) return;
    TGuardValue<bool> Flushing(bFlushingReaction, true);
    ON_SCOPE_EXIT
    {
        if (PendingReactionToken == Token)
        { PendingReactionToken = 0; PendingElementReactions.Reset(); bReactionCanceled = false; }
    };
    AActor* Target = GetOwner();
    if (!Target || !Target->HasAuthority() || Target->IsActorBeingDestroyed()) return;
    if (!Combat) Combat = Target->FindComponentByClass<UBreakerCombatComponent>();
    if (!Combat || Combat->IsDead() || bReactionCanceled) return;
    const TArray<FPendingElementReaction> Claimed = PendingElementReactions;
    for (auto& Pending : PendingElementReactions) { Pending.Status.UnpaidDamageBudget = 0; Pending.ChainTarget.Reset(); }
    // Both primary and child allocations are now claimed for the entire batch.
    for (const auto& Pending : Claimed)
    {
        if (bReactionCanceled || Combat->IsDead() || Target->IsActorBeingDestroyed()) break;
        const auto& Consumed = Pending.Status;
        const float Budget = Consumed.UnpaidDamageBudget;
        if (!FMath::IsFinite(Budget) || Budget <= 0) continue;
        AActor* Source = Consumed.Instigator.Get();
        if (Pending.bSympathetic)
        {
            BeginSympatheticLockout(Source);
            AActor* ReservedChild = Pending.ChainTarget.Get();
            const auto* ChildSink = IsValid(ReservedChild) ? ReservedChild->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
            if (ChildSink && !ChildSink->IsDead())
                if (auto* ChildStatus = ReservedChild->FindComponentByClass<UBreakerStatusComponent>()) ChildStatus->BeginSympatheticLockout(Source);
        }
        FBreakerStatusApplicationSpec Spec = Consumed.Spec;
        Spec.BaseDamagePerTick = Budget; Spec.InitialStacks = 1; Spec.ProcCoefficient = 0;
        Spec.Snapshot.SourcePower = 1; Spec.Snapshot.CriticalChance = 0; Spec.Snapshot.bRolledCritical = false;
        auto Payment = UBreakerDamageLibrary::MakeSnapshotDotTick(Spec, EBreakerDamageFamily::Elemental, 1,
            Source, Consumed.SourceLocationSnapshot, Consumed.bHasSourceLocationSnapshot);
        Payment.DamageTypeTag = Pending.Tag; Consumed.CopyThreatTo(Payment);
        Payment.bCanCritical = false; Payment.bCanApplyElementBuildup = false; Payment.bBypassShield = false;
        Payment.Element = EBreakerElement::None; Payment.ElementalFraction = 0;
        BreakerReactionFeedback::Play(Target, Source, Pending.Tag);
        Combat->ReceiveDamage(Payment);
        AActor* Child = Pending.ChainTarget.Get();
        const auto* SourceCombat = IsValid(Source) ? Source->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
        if (!IsValid(Child) || Child->IsActorBeingDestroyed() || !IsValid(Source) || Source->IsActorBeingDestroyed()
            || !SourceCombat || SourceCombat->IsDead()) continue;
        auto* ChildCombat = Child->FindComponentByClass<UBreakerCombatComponent>();
        if (!ChildCombat || ChildCombat->IsDead()) continue;
        // Child lockout was reserved before primary callbacks, with its payout.
        auto ChildPayment = Payment;
        ChildPayment.BaseDamage *= .5f; // O2 PLACEHOLDER, authored half-strength child.
        ChildPayment.ImpactLocation = Child->GetActorLocation(); ChildPayment.bHasImpactLocation = true;
        ChildCombat->ReceiveDamage(ChildPayment);
    }
}

void UBreakerStatusComponent::BeginSympatheticLockout(AActor* Source)
{
    // Enemy-owned component is the target-side key; an ally's application is
    // keyed by another source and never sees this owner's lockout.
    if (!Cast<ABreakerEnemy>(GetOwner()) || !IsValid(Source) || Source == GetOwner()) return;
    auto* Lock = SympatheticLockouts.FindByPredicate([Source](const auto& Entry) { return Entry.Source.Get() == Source; });
    if (!Lock) { Lock = &SympatheticLockouts.AddDefaulted_GetRef(); Lock->Source = Source; }
    Lock->UnlockTime = ElementClock + 3.0; // O2 PLACEHOLDER, authored O231 three seconds.
    if (auto* SourceStatus = Source->FindComponentByClass<UBreakerStatusComponent>())
        if (auto* SourceCombat = Source->FindComponentByClass<UBreakerCombatComponent>())
            SourceCombat->OnDeath.AddUniqueDynamic(SourceStatus, &UBreakerStatusComponent::HandleAfflictedOwnerDeath);
}

float UBreakerStatusComponent::ClaimedElementBuildup(const FBreakerDamageRequest& Request, float OrdinaryAmount)
{
    if (DeferredReplayRequest == &Request) return DeferredReplayAmount;
    const int32 Index = Request.Element == EBreakerElement::Entropy ? 0 : Request.Element == EBreakerElement::Void ? 1
        : Request.Element == EBreakerElement::Rift ? 2 : INDEX_NONE;
    if (Index == INDEX_NONE) return OrdinaryAmount;
    auto* Lock = SympatheticLockouts.FindByPredicate([&](const auto& Entry)
        { return Entry.Source == Request.Instigator && Entry.UnlockTime <= ElementClock + 1.e-6; });
    if (!Lock) return OrdinaryAmount;
    // A subthreshold bank keeps its original decay until a later accepted
    // same-source hit funds the crossing. It never gains a fresh timeout just
    // because the lock ended and never donates its credit to an ally.
    auto& Bank = Lock->Banks[Index];
    const float Carried = Bank.Decay.Amount;
    Bank.Decay.Amount = 0; Bank.bHasSnapshot = false;
    return OrdinaryAmount + Carried;
}

bool UBreakerStatusComponent::DeferSympatheticElement(const FBreakerDamageRequest& Request,
    const FBreakerDamageResult& Result, float Amount, float GraceSeconds)
{
    if (DeferredReplayRequest == &Request) return false;
    auto* Lock = SympatheticLockouts.FindByPredicate([&](const auto& Entry)
        { return Entry.Source == Request.Instigator && Entry.UnlockTime > ElementClock + 1.e-6; });
    if (!Lock) return false;
    const int32 Index = Request.Element == EBreakerElement::Entropy ? 0 : Request.Element == EBreakerElement::Void ? 1
        : Request.Element == EBreakerElement::Rift ? 2 : INDEX_NONE;
    if (Index == INDEX_NONE) return false;
    if (!FMath::IsFinite(Amount) || Amount <= 0) return true;
    auto& Bank = Lock->Banks[Index];
    Bank.Decay = BreakerBuildup::Advance(Bank.Decay, static_cast<float>(ElementBuildupClock - Bank.AdvancedBuildupClock));
    Bank.AdvancedBuildupClock = ElementBuildupClock;
    Bank.Decay.Amount += Amount;
    Bank.Decay.GraceRemaining = GraceSeconds;
    Bank.Decay.FadeRemaining = FMath::IsFinite(Request.ElementBuildupFadeSeconds) ? FMath::Max(0.f, Request.ElementBuildupFadeSeconds) : 0.f;
    Bank.Request = Request;
    Bank.Result = Result;
    Bank.bHasSnapshot = true;
    return true;
}

float UBreakerStatusComponent::DeferredElementBuildup(EBreakerElement Element) const
{
    const int32 Index = Element == EBreakerElement::Entropy ? 0 : Element == EBreakerElement::Void ? 1
        : Element == EBreakerElement::Rift ? 2 : INDEX_NONE;
    if (Index == INDEX_NONE) return 0;
    float Total = 0;
    for (const auto& Lock : SympatheticLockouts) Total += Lock.Banks[Index].Decay.Amount;
    return Total;
}

void UBreakerStatusComponent::AdvanceSympatheticLockouts()
{
    struct FReplay { FBreakerDamageRequest Request; FBreakerDamageResult Result; float Amount; };
    TArray<FReplay> Ready;
    const auto* Sink = GetOwner() ? GetOwner()->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
    if (!Sink || Sink->IsDead() || !GetOwner()->HasAuthority()) { SympatheticLockouts.Reset(); return; }
    for (auto& Lock : SympatheticLockouts)
    {
        auto* Source = Lock.Source.Get();
        const auto* SourceCombat = IsValid(Source) ? Source->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
        if (!IsValid(Source) || Source->IsActorBeingDestroyed() || (SourceCombat && SourceCombat->IsDead()))
        { Lock.Source.Reset(); continue; }
        for (int32 Index = 0; Index < 3; ++Index)
        {
            auto& Bank = Lock.Banks[Index];
            Bank.Decay = BreakerBuildup::Advance(Bank.Decay, static_cast<float>(ElementBuildupClock - Bank.AdvancedBuildupClock));
            Bank.AdvancedBuildupClock = ElementBuildupClock;
            if (Lock.UnlockTime > ElementClock + 1.e-6 || Bank.Decay.Amount <= 0 || !Bank.bHasSnapshot) continue;
            const auto Tag = FGameplayTag::RequestGameplayTag(Index == 0 ? TEXT("Status.Rot") : Index == 1 ? TEXT("Status.Erased") : TEXT("Status.Unstable"));
            if (HasStatus(Tag) || IsStatusImmune() || IsElementTransactionActive()) continue;
            const float BaseThreshold = Index == 0 ? GetEntropyThreshold() : Index == 1 ? GetVoidThreshold() : GetRiftThreshold();
            const float Total = Index == 0 ? GetEntropyBuildup() : Index == 1 ? GetVoidBuildup() : GetRiftBuildup();
            const float Ordinary = Total - DeferredElementBuildup(Bank.Request.Element);
            if (Ordinary + Bank.Decay.Amount < BreakerElementSource::Threshold(Bank.Request, BaseThreshold)) continue;
            Ready.Add({Bank.Request, Bank.Result, Bank.Decay.Amount});
        }
    }
    SympatheticLockouts.RemoveAll([this](const auto& Lock)
    {
        return !Lock.Source.IsValid() || (Lock.UnlockTime <= ElementClock + 1.e-6
            && Lock.Banks[0].Decay.Amount <= 0 && Lock.Banks[1].Decay.Amount <= 0 && Lock.Banks[2].Decay.Amount <= 0);
    });
    // Revalidate and claim one bank immediately before its replay. Earlier
    // callbacks or another source may have occupied the same status slot.
    for (const auto& Replay : Ready)
    {
        if (!IsValid(GetOwner()) || GetOwner()->IsActorBeingDestroyed() || Sink->IsDead()) break;
        auto* Source = Replay.Request.Instigator.Get();
        const auto* SourceCombat = IsValid(Source) ? Source->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
        if (!IsValid(Source) || Source->IsActorBeingDestroyed() || (SourceCombat && SourceCombat->IsDead())) continue;
        const int32 Index = Replay.Request.Element == EBreakerElement::Entropy ? 0 : Replay.Request.Element == EBreakerElement::Void ? 1 : 2;
        const auto Tag = FGameplayTag::RequestGameplayTag(Index == 0 ? TEXT("Status.Rot") : Index == 1 ? TEXT("Status.Erased") : TEXT("Status.Unstable"));
        if (HasStatus(Tag) || IsStatusImmune() || IsElementTransactionActive()) continue;
        auto* Lock = SympatheticLockouts.FindByPredicate([Source](const auto& Entry) { return Entry.Source.Get() == Source; });
        if (!Lock || Lock->UnlockTime > ElementClock + 1.e-6) continue;
        auto& Bank = Lock->Banks[Index];
        if (!Bank.bHasSnapshot || Bank.Decay.Amount <= 0) continue;
        const float BaseThreshold = Index == 0 ? GetEntropyThreshold() : Index == 1 ? GetVoidThreshold() : GetRiftThreshold();
        const float Total = Index == 0 ? GetEntropyBuildup() : Index == 1 ? GetVoidBuildup() : GetRiftBuildup();
        if (Total - DeferredElementBuildup(Replay.Request.Element) + Bank.Decay.Amount
            < BreakerElementSource::Threshold(Bank.Request, BaseThreshold)) continue;
        // Copy before callbacks; never retain an array reference through application.
        const FReplay Claimed{Bank.Request, Bank.Result, Bank.Decay.Amount};
        Bank.Decay.Amount = 0; Bank.bHasSnapshot = false;
        TGuardValue<const FBreakerDamageRequest*> Replaying(DeferredReplayRequest, &Claimed.Request);
        TGuardValue<float> Amount(DeferredReplayAmount, Claimed.Amount);
        ApplyEntropyHit(Claimed.Request, Claimed.Result);
        ApplyVoidHit(Claimed.Request, Claimed.Result);
        const uint64 Rift = ApplyRiftHit(Claimed.Request, Claimed.Result);
        if (Rift != 0) FlushRiftActivation(Rift);
    }
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
        SpreadExpiredRot(Expired);
        OnStatusExpired.Broadcast(Expired);
    }
}
