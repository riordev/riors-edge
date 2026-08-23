#include "Classes/BreakerScrapComponent.h"

#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerGunsmithAbilities.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Game/BreakerGameMode.h"
#include "GameFramework/Actor.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"

UBreakerScrapComponent::UBreakerScrapComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    SetIsReplicatedByDefault(true);
}

void UBreakerScrapComponent::BeginPlay()
{
    Super::BeginPlay();
    if (const IAbilitySystemInterface* AbilityOwner = Cast<IAbilitySystemInterface>(GetOwner()))
    {
        if (UAbilitySystemComponent* ASC = AbilityOwner->GetAbilitySystemComponent())
        {
            Attributes = const_cast<UBreakerAttributeSet*>(ASC->GetSet<UBreakerAttributeSet>());
        }
    }
    if (AActor* Owner = GetOwner())
    {
        if (UBreakerProgressionComponent* Progression = Owner->FindComponentByClass<UBreakerProgressionComponent>())
        {
            CachedProgression = Progression;
            Progression->OnProgressionChanged.AddDynamic(this, &UBreakerScrapComponent::HandleProgressionChanged);
        }
    }
    // DELIBERATELY NO WEAPON BINDING. The Momentum and Mana components subscribe
    // to UBreakerWeaponComponent::OnShot because their loops read shots. Scrap's
    // weapon-side sources are RELOAD COMPLETED and MAGAZINE EMPTIED, and neither
    // has an event on the weapon component today (Class-Kits-Gunsmith §9 ranks
    // `OnMagazineEmptied` as a missing hook). Subscribing to OnShot and inferring
    // "the magazine is now empty" from a shot result would put the anti-farm rule
    // — the magazine must have been FULL at the start of the cycle — in the wrong
    // component and get it wrong. The Notify* entry points below are the contract
    // instead; Weapons/ calls them when the hook lands.
    HandleProgressionChanged();
    CachedState = StateForFraction(GetScrapFraction(), StockedFraction, SurplusFraction);
}

void UBreakerScrapComponent::BindAttributes(UBreakerAttributeSet* InAttributes)
{
    Attributes = InAttributes;
    HandleProgressionChanged();
    CachedState = StateForFraction(GetScrapFraction(), StockedFraction, SurplusFraction);
}

EBreakerScrapState UBreakerScrapComponent::StateForFraction(float Fraction, float StockedAt, float SurplusAt)
{
    // Class-Kits-Gunsmith §1.4: Dry 0-24, Stocked 25-59, Surplus 60-100. Read as
    // FRACTIONS of the maximum rather than as absolutes, so a Maximum Resource
    // roll moves the bands with the bar instead of quietly making Surplus
    // cheaper to hold — the rule Tank states explicitly for IRONCLAD and that
    // every banded resource obeys.
    if (Fraction >= SurplusAt) return EBreakerScrapState::Surplus;
    if (Fraction >= StockedAt) return EBreakerScrapState::Stocked;
    return EBreakerScrapState::Dry;
}

EBreakerScrapState UBreakerScrapComponent::StateForFraction(float Fraction)
{
    return StateForFraction(Fraction, 0.25f, 0.60f);   // O2 PLACEHOLDER seeds
}

float UBreakerScrapComponent::ComposeGenerationMultipliers(const TArray<float>& Multipliers)
{
    float Composed = 1.0f;
    for (const float Multiplier : Multipliers)
    {
        if (Multiplier > 0.0f) Composed *= Multiplier;
    }
    return Composed;
}

bool UBreakerScrapComponent::IsLoopOverrideExpired(double ExpiryTime, double Now)
{
    return ExpiryTime >= 0.0 && ExpiryTime <= Now;
}

float UBreakerScrapComponent::ClampGeneration(float RequestedRate, float GlobalCap)
{
    return FMath::Clamp(RequestedRate, 0.0f, FMath::Max(0.0f, GlobalCap));
}

float UBreakerScrapComponent::ClampToBank(float Value, float MaxScrap)
{
    // Floor is a hard 0: Scrap has no Overcast analogue and no node grants one,
    // so a Gunsmith can be broke but never in debt. Ceiling discards overflow
    // rather than banking it (§1.2).
    return FMath::Clamp(Value, 0.0f, FMath::Max(0.0f, MaxScrap));
}

float UBreakerScrapComponent::KillGeneration(float KillGrant, float ProcCoefficient)
{
    return FMath::Max(0.0f, KillGrant) * FMath::Max(0.0f, ProcCoefficient);
}

float UBreakerScrapComponent::ReloadGeneration(bool bAnyRoundFired, float ReloadGrant)
{
    return bAnyRoundFired ? FMath::Max(0.0f, ReloadGrant) : 0.0f;
}

float UBreakerScrapComponent::MagazineDumpGeneration(bool bStartedFull, float DumpGrant)
{
    return bStartedFull ? FMath::Max(0.0f, DumpGrant) : 0.0f;
}

float UBreakerScrapComponent::DestructionRefund(float DeployableScrapCost, float RefundFraction)
{
    // Hard ceiling below 100%: a refund that returned the whole cost would make
    // placement free and the density cap the only constraint, which is the
    // "class farms itself" inversion §1.3's acceptance invariant forbids.
    return FMath::Max(0.0f, DeployableScrapCost) * FMath::Clamp(RefundFraction, 0.0f, 1.0f);
}

float UBreakerScrapComponent::DeployableDamageGeneration(float DamageAppliedToHealth, float DamagePerScrap)
{
    if (DamagePerScrap <= 0.0f) return 0.0f;
    // DamageAppliedToHealth is post-mitigation and overkill-trimmed by the
    // caller, per §1.1's anti-farm clause. This function cannot verify that and
    // does not try — it is stated here so the caller's obligation is recorded at
    // the only place the arithmetic happens.
    return FMath::Max(0.0f, DamageAppliedToHealth) / DamagePerScrap;
}

bool UBreakerScrapComponent::CanSpendFrom(float Scrap, float Cost)
{
    if (Cost <= 0.0f) return true;
    return Scrap >= Cost;
}

void UBreakerScrapComponent::PushLoopOverride(FName Key, float GenerationMultiplier, float Duration)
{
    if (Key.IsNone()) return;
    FLoopOverrideEntry Entry;
    // Zero is a legal multiplier and must not be silently promoted to 1.0 — the
    // exact bug UBreakerMomentumComponent::PushLoopOverride carries a comment
    // about, where a caller asking for "generation stops entirely" was handed
    // full normal generation. Only a NEGATIVE multiplier is meaningless.
    if (GenerationMultiplier < 0.0f)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("PushLoopOverride('%s'): negative generation multiplier %.3f is meaningless; falling back to 1.0."),
            *Key.ToString(), GenerationMultiplier);
    }
    Entry.GenerationMultiplier = GenerationMultiplier >= 0.0f ? GenerationMultiplier : 1.0f;
    const UWorld* World = GetWorld();
    Entry.ExpiryTime = (Duration > 0.0f && World) ? World->GetTimeSeconds() + Duration : -1.0;
    // Re-pushing the same key replaces rather than stacks: a re-cast refreshes.
    LoopOverrides.Add(Key, Entry);
}

void UBreakerScrapComponent::PopLoopOverride(FName Key)
{
    LoopOverrides.Remove(Key);
}

void UBreakerScrapComponent::PruneLoopOverrides() const
{
    const UWorld* World = GetWorld();
    if (!World || LoopOverrides.Num() == 0) return;
    const double Now = World->GetTimeSeconds();
    for (auto It = LoopOverrides.CreateIterator(); It; ++It)
    {
        if (IsLoopOverrideExpired(It.Value().ExpiryTime, Now)) It.RemoveCurrent();
    }
}

float UBreakerScrapComponent::GetGenerationMultiplier() const
{
    PruneLoopOverrides();
    TArray<float> Active;
    Active.Reserve(LoopOverrides.Num());
    for (const TPair<FName, FLoopOverrideEntry>& Pair : LoopOverrides)
    {
        Active.Add(Pair.Value.GenerationMultiplier);
    }
    return ComposeGenerationMultipliers(Active);
}

int32 UBreakerScrapComponent::GetActiveLoopOverrideCount() const
{
    PruneLoopOverrides();
    return LoopOverrides.Num();
}

void UBreakerScrapComponent::HandleProgressionChanged()
{
    if (!CachedProgression.IsValid() && GetOwner())
    {
        CachedProgression = GetOwner()->FindComponentByClass<UBreakerProgressionComponent>();
    }
    const UBreakerProgressionComponent* Progression = CachedProgression.Get();
    bIsGunsmith = Progression && Progression->GetProgressionState().PermanentClass == EBreakerClassId::Gunsmith;
    // Queued income is dropped when the loop stops being ours, exactly as the
    // Momentum loop drops it: a class swap must not pay out a frame later.
    if (!bIsGunsmith) PendingGrants = 0.0f;
}

bool UBreakerScrapComponent::IsActiveForOwner() const
{
    return bIsGunsmith;
}

float UBreakerScrapComponent::GetScrap() const
{
    return Attributes ? Attributes->GetClassResource() : 0.0f;
}

float UBreakerScrapComponent::GetScrapFraction() const
{
    if (!Attributes) return 0.0f;
    const float Max = Attributes->GetMaxClassResource();
    return Max > 0.0f ? FMath::Clamp(Attributes->GetClassResource() / Max, 0.0f, 1.0f) : 0.0f;
}

bool UBreakerScrapComponent::IsInSafeZone() const
{
    const AActor* Owner = GetOwner();
    if (!Owner || !GetWorld()) return false;
    const ABreakerGameMode* GameMode = GetWorld()->GetAuthGameMode<ABreakerGameMode>();
    return GameMode && GameMode->IsInSafeZone(Owner->GetActorLocation());
}

void UBreakerScrapComponent::ApplyScrapDelta(float Delta)
{
    if (!Attributes || FMath::IsNearlyZero(Delta)) return;
    // ApplyClassResource, not the generated SetClassResource, for the reason the
    // Mana and Momentum loops both record: the generated setter ensure()s with
    // no owning AbilitySystemComponent, which would make the whole loop
    // unexercisable in automation.
    Attributes->ApplyClassResource(ClampToBank(Attributes->GetClassResource() + Delta, Attributes->GetMaxClassResource()));
}

void UBreakerScrapComponent::RefreshState()
{
    const EBreakerScrapState NewState = StateForFraction(GetScrapFraction(), StockedFraction, SurplusFraction);
    if (NewState != CachedState)
    {
        CachedState = NewState;
        OnScrapStateChanged.Broadcast(NewState);
    }
}

void UBreakerScrapComponent::QueueGrant(float Amount)
{
    // Every event source funnels through here so the global 15/s budget is the
    // single place generation is metered, and so the safe-zone and class gates
    // are stated once rather than at seven call sites.
    if (Amount <= 0.0f || !GetOwner() || !GetOwner()->HasAuthority() || !IsActiveForOwner() || IsInSafeZone()) return;
    PendingGrants += Amount;
}

void UBreakerScrapComponent::NotifyKill(float ProcCoefficient)
{
    QueueGrant(KillGeneration(KillGrant, ProcCoefficient));
}

void UBreakerScrapComponent::NotifyReloadCompleted(bool bAnyRoundFired)
{
    // AR6 Cold Barrel: a reload completing while the emptied flag is up is a
    // reload from an empty magazine (rounds only leave by firing, and nothing
    // refills a magazine but a reload). Once per reload by construction — the
    // flag is consumed here. The shave itself is the Sidearm Rig's rule.
    const bool bFromEmpty = bMagazineEmptiedSinceReload;
    bMagazineEmptiedSinceReload = false;
    if (bFromEmpty)
    {
        const int32 ColdBarrelRank = GetGunsmithNodeRank(TEXT("Gunsmith.Armory.ColdBarrel"));
        if (ColdBarrelRank > 0)
        {
            UBreakerAbility_SidearmRig::ShaveCooldownForEmptyReload(GetOwner(), ColdBarrelRank);
        }
    }

    // AR11 No Reserve: reload and magazine Scrap pay double. NOTE, recorded:
    // the reserve-halving half of the node lives on the weapon's reserve cap
    // (Weapons/ territory) and is still waiting; the doubling is real either
    // way and reads as the node's upside until the downside lands.
    const float Multiplier = HasOwnedNodeTag(BreakerNodeTags::Node_AR_NoReserve.GetTag()) ? 2.0f : 1.0f;
    QueueGrant(ReloadGeneration(bAnyRoundFired, ReloadGrant) * Multiplier);
}

void UBreakerScrapComponent::NotifyMagazineEmptied(bool bStartedFull)
{
    bMagazineEmptiedSinceReload = true;
    // AR1 Field Stripping R2: the full-at-cycle-start requirement on the dump
    // source is removed. (R1's clause — reload credit paying on a partial
    // reload provided a round was fired — is ALREADY the shipped base rule:
    // ReloadGeneration keys on bAnyRoundFired and nothing else, exactly the
    // doc's own anti-farm wording. Rank 1 therefore changes nothing here and
    // rank 2 is the rank that moves this event.)
    const bool bTreatAsFull = bStartedFull
        || GetGunsmithNodeRank(TEXT("Gunsmith.Armory.FieldStripping")) >= 2;
    const float Multiplier = HasOwnedNodeTag(BreakerNodeTags::Node_AR_NoReserve.GetTag()) ? 2.0f : 1.0f;
    QueueGrant(MagazineDumpGeneration(bTreatAsFull, MagazineDumpGrant) * Multiplier);
}

void UBreakerScrapComponent::NotifyDeployableDestroyed(float DeployableScrapCost)
{
    QueueGrant(DestructionRefund(DeployableScrapCost, GetEffectiveDestructionRefundFraction()));
}

void UBreakerScrapComponent::NotifyDeployableDamageDealt(float DamageAppliedToHealth)
{
    const float Generated = DeployableDamageGeneration(DamageAppliedToHealth, DeployableDamagePerScrap);
    // FT4 Tithe: while Surplus, deployable-damage Scrap ignores the per-second
    // cap — paid directly instead of queued through the metered budget. The
    // band gate keeps it accelerating the top of the bar only. (R2's shorter
    // per-deployable ICD still waits: the ICD itself is recorded-unenforced,
    // see DeployableDamageInterval.)
    if (HasOwnedNodeTag(BreakerNodeTags::Node_FT_Tithe.GetTag())
        && GetScrapState() == EBreakerScrapState::Surplus)
    {
        GrantScrap(Generated);
        return;
    }
    QueueGrant(Generated);
}

void UBreakerScrapComponent::NotifyAmmoPickupOverflow(float OverflowRounds)
{
    const int32 Rank = GetGunsmithNodeRank(TEXT("Gunsmith.Armory.DeepPockets"));
    if (Rank <= 0 || OverflowRounds <= 0.0f) return;
    // Metered (the node row: "respects the 15/s global cap"), doubled at R2.
    QueueGrant(OverflowRounds * OverflowScrapPerRound * (Rank >= 2 ? 2.0f : 1.0f));
}

void UBreakerScrapComponent::NotifyAmmoReturnedOnKill(int32 RoundsReturned)
{
    if (!HasOwnedNodeTag(BreakerNodeTags::Node_AR_Reciprocal.GetTag()) || RoundsReturned <= 0) return;
    // OUTSIDE the per-second cap, per the node row — the direct-credit path.
    GrantScrap(RoundsReturned * ReciprocalScrapPerReturn);
}

void UBreakerScrapComponent::NotifyDisruptorFieldKill()
{
    const float Refund = AttritionFieldRefund(GetGunsmithNodeRank(TEXT("Gunsmith.Tinkerer.AttritionField")));
    if (Refund <= 0.0f) return;
    // "ignoring the global cap" — direct credit, like the node refund pattern.
    GrantScrap(Refund);
}

int32 UBreakerScrapComponent::GetReloadTierShift() const
{
    return ReloadTierShiftFor(GetGunsmithNodeRank(TEXT("Gunsmith.Armory.WorkingStock")), GetScrapState());
}

float UBreakerScrapComponent::GetEffectiveDestructionRefundFraction() const
{
    return SalvageRefundFraction(GetGunsmithNodeRank(TEXT("Gunsmith.FieldTech.Salvage")), DestructionRefundFraction);
}

float UBreakerScrapComponent::SalvageRefundFraction(int32 SalvageRank, float BaseFraction)
{
    // FT1: 65% at rank 1, 80% at rank 2 — "the hard ceiling", transcribed.
    if (SalvageRank >= 2) return 0.80f;
    if (SalvageRank == 1) return 0.65f;
    return FMath::Clamp(BaseFraction, 0.0f, 1.0f);
}

float UBreakerScrapComponent::AttritionFieldRefund(int32 Rank)
{
    // TK5: 8 Scrap at rank 1, 14 at rank 2, transcribed.
    if (Rank >= 2) return 14.0f;
    if (Rank == 1) return 8.0f;
    return 0.0f;
}

int32 UBreakerScrapComponent::ReloadTierShiftFor(int32 WorkingStockRank, EBreakerScrapState State)
{
    // AR2: one tier faster while Dry; R2 extends the band to Stocked. Never a
    // percentage — a band-and-tier rewrite only.
    if (WorkingStockRank >= 2) return State != EBreakerScrapState::Surplus ? 1 : 0;
    if (WorkingStockRank == 1) return State == EBreakerScrapState::Dry ? 1 : 0;
    return 0;
}

int32 UBreakerScrapComponent::GetGunsmithNodeRank(FName NodeId) const
{
    if (!CachedProgression.IsValid() && GetOwner())
    {
        const_cast<UBreakerScrapComponent*>(this)->CachedProgression =
            GetOwner()->FindComponentByClass<UBreakerProgressionComponent>();
    }
    const UBreakerProgressionComponent* Progression = CachedProgression.Get();
    return Progression ? Progression->GetNodeRank(NodeId, EBreakerPointCurrency::DoctrinePoints) : 0;
}

bool UBreakerScrapComponent::HasOwnedNodeTag(const FGameplayTag& Tag) const
{
    if (!CachedProgression.IsValid() && GetOwner())
    {
        const_cast<UBreakerScrapComponent*>(this)->CachedProgression =
            GetOwner()->FindComponentByClass<UBreakerProgressionComponent>();
    }
    const UBreakerProgressionComponent* Progression = CachedProgression.Get();
    return Progression && Progression->HasNodeTag(Tag);
}

void UBreakerScrapComponent::GrantScrap(float Amount)
{
    if (Amount <= 0.0f || !GetOwner() || !GetOwner()->HasAuthority() || !IsActiveForOwner()) return;
    ApplyScrapDelta(Amount);
    RefreshState();
}

bool UBreakerScrapComponent::CanAffordSpend(float Cost) const
{
    return IsActiveForOwner() && CanSpendFrom(GetScrap(), Cost);
}

bool UBreakerScrapComponent::TrySpendScrap(float Cost)
{
    if (!Attributes || !GetOwner() || !GetOwner()->HasAuthority()) return false;
    if (!CanAffordSpend(Cost)) return false;
    ApplyScrapDelta(-Cost);
    RefreshState();
    return true;
}

void UBreakerScrapComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    AdvanceLoop(DeltaTime);
}

void UBreakerScrapComponent::AdvanceLoop(float DeltaTime)
{
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority() || !Attributes || DeltaTime <= 0.0f) return;
    if (!IsActiveForOwner())
    {
        PendingGrants = 0.0f;
        return;
    }

    if (IsInSafeZone())
    {
        PendingGrants = 0.0f;
        RefreshState();
        return;
    }

    // THIS IS THE WHOLE LOOP, AND ITS SHORTNESS IS THE DESIGN. There is no
    // regeneration term because Scrap has no idle income, and no decay term
    // because Scrap never loses value — §1.2's "no decay, ever, in or out of
    // combat" is expressed by there being nothing here to express it. Both
    // absences are load-bearing: idle income on a class whose spend persists in
    // the world is free permanent power, and decay on a class with no idle
    // income is a bar that can only fall.
    if (PendingGrants <= 0.0f)
    {
        RefreshState();
        return;
    }

    // Scrap has NO rate sources at all — every row of §1.1 is an event — so an
    // active override has exactly one thing to act on: the per-second budget
    // those events drain through. Multiplying the CAP rather than the queued
    // amount is what makes that a single, unambiguous application; multiplying
    // both would pay a queued kill twice, once as a bigger credit and once as
    // more room to pay it in. This is the same conclusion the Mana loop reaches
    // by a different route (its GrantMana metering scales the credit and leaves
    // the cap alone, because Mana's cap bounds a RATE it also generates).
    const float EffectiveCap = FMath::Max(0.0f, GlobalGenerationCap * GetGenerationMultiplier());
    const float Budget = ClampGeneration(EffectiveCap, EffectiveCap) * DeltaTime;
    // Grants QUEUE rather than being discarded when the budget is already spent,
    // matching Momentum: a kill landed during a capped frame is paid next frame,
    // not lost. Losing it would make the anti-farm cap read as a bug in a dense
    // wave, which is precisely the situation the cap exists for.
    const float Drawn = FMath::Min(PendingGrants, Budget);
    PendingGrants -= Drawn;
    ApplyScrapDelta(Drawn);
    RefreshState();
}
