#include "Classes/BreakerManaComponent.h"
#include "Classes/BreakerResourceGeneration.h"

#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Abilities/BreakerAbilityTags.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Abilities/BreakerAbility_Cleave.h"
#include "Abilities/BreakerAbility_Rot.h"
#include "Combat/BreakerZoneActor.h"
#include "Data/BreakerDataFile.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerStatusRules.h"
#include "Game/BreakerGameMode.h"
#include "GameFramework/Actor.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponDefinition.h"

UBreakerManaComponent::UBreakerManaComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    SetIsReplicatedByDefault(true);
}

bool UBreakerManaComponent::ParseResourceTuning(const FJsonObject& Object, FBreakerCasterResourceTuning& Out, FString& Error)
{
    FBreakerCasterResourceTuning Candidate;
    struct FField { const TCHAR* Name; float* Value; float Minimum; };
    const FField Fields[] = {
        { TEXT("StatusApplicationMana"), &Candidate.StatusApplicationMana, 0 },
        { TEXT("SeepRankOneMultiplier"), &Candidate.SeepRankOneMultiplier, 1 },
        { TEXT("SeepRankTwoMultiplier"), &Candidate.SeepRankTwoMultiplier, 1 },
        { TEXT("AttritionRankOneRefund"), &Candidate.AttritionRankOneRefund, 0 },
        { TEXT("AttritionRankTwoRefund"), &Candidate.AttritionRankTwoRefund, 0 },
        { TEXT("CloseRankOneRangeCm"), &Candidate.CloseRankOneRangeCm, 0 },
        { TEXT("CloseRankTwoRangeCm"), &Candidate.CloseRankTwoRangeCm, 0 },
        { TEXT("DebtRankOneExtension"), &Candidate.DebtRankOneExtension, 0 },
        { TEXT("DebtRankTwoExtension"), &Candidate.DebtRankTwoExtension, 0 },
        { TEXT("PreparedOvercastFloor"), &Candidate.PreparedOvercastFloor, -MAX_flt },
        { TEXT("OverreachIncomingDamageTaken"), &Candidate.OverreachIncomingDamageTaken, 0 },
        { TEXT("BloodpriceRankOneFraction"), &Candidate.BloodpriceRankOneFraction, 0 },
        { TEXT("BloodpriceRankTwoFraction"), &Candidate.BloodpriceRankTwoFraction, 0 },
        { TEXT("PatienceRankOneDelay"), &Candidate.PatienceRankOneDelay, 0 },
        { TEXT("PatienceRankTwoDelay"), &Candidate.PatienceRankTwoDelay, 0 },
        { TEXT("PatienceBonusRegenPerSecond"), &Candidate.PatienceBonusRegenPerSecond, 0 },
        { TEXT("VarianceRankOneMultiplier"), &Candidate.VarianceRankOneMultiplier, 1 },
        { TEXT("VarianceRankTwoMultiplier"), &Candidate.VarianceRankTwoMultiplier, 1 },
        { TEXT("SequenceWindowSeconds"), &Candidate.SequenceWindowSeconds, UE_KINDA_SMALL_NUMBER },
        { TEXT("SequenceCooldownSeconds"), &Candidate.SequenceCooldownSeconds, UE_KINDA_SMALL_NUMBER },
        { TEXT("SequenceRankOneMana"), &Candidate.SequenceRankOneMana, 0 },
        { TEXT("SequenceRankTwoMana"), &Candidate.SequenceRankTwoMana, 0 }
    };
    if (Object.Values.Num() != UE_ARRAY_COUNT(Fields)) { Error = TEXT("Caster resource fields do not match the supported schema."); return false; }
    for (const FField& Field : Fields)
    {
        double Value = 0;
        if (!Object.TryGetNumberField(Field.Name, Value) || !FMath::IsFinite(Value) || Value < Field.Minimum || Value > MAX_flt)
        { Error = FString::Printf(TEXT("Invalid Caster resource field %s."), Field.Name); return false; }
        *Field.Value = static_cast<float>(Value);
    }
    if (Candidate.BloodpriceRankOneFraction > 1 || Candidate.BloodpriceRankTwoFraction > 1)
    { Error = TEXT("Bloodprice fractions must be between zero and one."); return false; }
    if (Candidate.PreparedOvercastFloor > 0)
    { Error = TEXT("Prepared floor must be non-positive."); return false; }
    Out = Candidate;
    Error.Reset();
    return true;
}

const FBreakerCasterResourceTuning& UBreakerManaComponent::GetResourceTuning()
{
    static const FBreakerCasterResourceTuning Tuning = []
    {
        FBreakerCasterResourceTuning Values;
        BreakerDataFile::FBreakerDataErrors Errors;
        const TSharedPtr<FJsonObject> Object = BreakerDataFile::Load(TEXT("Data/caster-resource.json"), Errors);
        FString Error;
        if (!Object || !ParseResourceTuning(*Object, Values, Error))
            UE_LOG(LogTemp, Error, TEXT("Caster resource tuning refused; compiled O2 defaults used: %s %s"), *Errors.Join(), *Error);
        return Values;
    }();
    return Tuning;
}

void UBreakerManaComponent::BeginPlay()
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
            Progression->OnProgressionChanged.AddUniqueDynamic(this, &UBreakerManaComponent::HandleProgressionChanged);
        }
    }
    BindOwnerEvents();
    // Resolves the class, publishes the floor, and puts the Overcast state (and
    // its damage penalty) in agreement with a bank that may have been restored
    // from a save mid-debt.
    HandleProgressionChanged();
}

void UBreakerManaComponent::BindAttributes(UBreakerAttributeSet* InAttributes)
{
    Attributes = InAttributes;
    // A component wired up outside a world never gets a BeginPlay, so this is
    // where it picks up the shot and reset hooks. Idempotent.
    BindOwnerEvents();
    HandleProgressionChanged();
}

void UBreakerManaComponent::BindOwnerEvents()
{
    AActor* Owner = GetOwner();
    if (!Owner) return;
    if (UBreakerProgressionComponent* Progression = Owner->FindComponentByClass<UBreakerProgressionComponent>())
    {
        CachedProgression = Progression;
        Progression->OnProgressionChanged.AddUniqueDynamic(this, &UBreakerManaComponent::HandleProgressionChanged);
    }

    // Weapon hits ACCELERATE recovery (owner ruling 2026-08-14); passive
    // regeneration is the primary path and needs no hook. Melee/kill events
    // bind below; accepted status applications notify their applier directly.
    if (UBreakerWeaponComponent* Weapon = Owner->FindComponentByClass<UBreakerWeaponComponent>())
    {
        if (!Weapon->OnShot.IsAlreadyBound(this, &UBreakerManaComponent::HandleShot))
        {
            Weapon->OnShot.AddDynamic(this, &UBreakerManaComponent::HandleShot);
        }
    }
    // The playtest reset and respawn path. "Start full" has to survive F1, or a
    // reset would leave a Caster standing in the safe ring waiting out a refill
    // that spawn already gave them once.
    if (UBreakerCombatComponent* Combat = Owner->FindComponentByClass<UBreakerCombatComponent>())
    {
        if (!Combat->OnHitDealt.IsAlreadyBound(this, &UBreakerManaComponent::HandleMeleeHit))
            Combat->OnHitDealt.AddDynamic(this, &UBreakerManaComponent::HandleMeleeHit);
        if (!Combat->OnKillDealt.IsAlreadyBound(this, &UBreakerManaComponent::HandleCasterKill))
            Combat->OnKillDealt.AddDynamic(this, &UBreakerManaComponent::HandleCasterKill);
        if (!Combat->OnVitalsRestored.IsAlreadyBound(this, &UBreakerManaComponent::HandleVitalsRestored))
        {
            Combat->OnVitalsRestored.AddDynamic(this, &UBreakerManaComponent::HandleVitalsRestored);
        }
    }
}

float UBreakerManaComponent::HitGeneration(bool bWeakPoint, int32 LandedPellets, int32 PelletsPerShot, float WeaponHitGain, float WeakPointGain, float ProcCoefficient)
{
    const int32 Pellets = FMath::Max(1, PelletsPerShot);
    const int32 Landed = FMath::Clamp(LandedPellets, 0, Pellets);
    if (Landed <= 0 || ProcCoefficient <= 0.0f) return 0.0f;

    // One weak-point pellet is paid at the weak-point rate; the rest bank at
    // the weapon-hit rate. Weak point replaces, it never stacks.
    const float WeakPointPellets = bWeakPoint ? 1.0f : 0.0f;
    const float PlainPellets = static_cast<float>(Landed) - WeakPointPellets;
    const float Total = WeakPointPellets * FMath::Max(0.0f, WeakPointGain) + FMath::Max(0.0f, PlainPellets) * FMath::Max(0.0f, WeaponHitGain);
    return (Total / static_cast<float>(Pellets)) * ProcCoefficient;
}

float UBreakerManaComponent::ClampGeneration(float RequestedAmount, float GlobalCap)
{
    return FMath::Clamp(RequestedAmount, 0.0f, FMath::Max(0.0f, GlobalCap));
}

bool UBreakerManaComponent::IsOvercastValue(float Mana)
{
    return Mana < 0.0f;
}

float UBreakerManaComponent::GenerationMultiplierForMana(float Mana, float OvercastMultiplier)
{
    return IsOvercastValue(Mana) ? FMath::Max(1.0f, OvercastMultiplier) : 1.0f;
}

float UBreakerManaComponent::ClampToBank(float Value, float Floor, float MaxMana)
{
    return FMath::Clamp(Value, FMath::Min(0.0f, Floor), FMath::Max(0.0f, MaxMana));
}

bool UBreakerManaComponent::CanSpendFrom(float Mana, float Cost, float Floor)
{
    // Overcast is a debt, not a spiral: a cast may drive the bank to the floor,
    // but nothing may be cast while already below zero.
    if (Cost <= 0.0f) return true;
    if (IsOvercastValue(Mana)) return false;
    return Mana - Cost >= FMath::Min(0.0f, Floor) - KINDA_SMALL_NUMBER;
}

void UBreakerManaComponent::HandleProgressionChanged()
{
    RefreshClassOwnership();
    const UBreakerProgressionComponent* Progression = CachedProgression.Get();
    const int32 Rank = bIsCaster && Progression ? Progression->GetNodeRank(TEXT("Caster.Multispell.Sequence"), EBreakerPointCurrency::DoctrinePoints) : 0;
    if (Rank != ObservedSequenceRank) ClearSequenceApplications();
    ObservedSequenceRank = Rank;
    // Ownership may change while the bank remains negative.
    SyncOvercastDamagePenalty();
}

void UBreakerManaComponent::HandleVitalsRestored()
{
    SequenceTargets.Reset();
    FillToMaximum();
}

void UBreakerManaComponent::RefreshClassOwnership()
{
    if (!CachedProgression.IsValid() && GetOwner())
    {
        CachedProgression = GetOwner()->FindComponentByClass<UBreakerProgressionComponent>();
    }
    const UBreakerProgressionComponent* Progression = CachedProgression.Get();
    ObservedClass = Progression ? Progression->GetProgressionState().PermanentClass : EBreakerClassId::None;
    const bool bWasCaster = bIsCaster;
    bIsCaster = ObservedClass == EBreakerClassId::Caster;
    if (!bIsCaster) { PendingGrants = 0.0f; ClearSequenceApplications(); }
    // Order matters: close the floor first (which lifts a stranded negative
    // bank back to zero), then re-evaluate Overcast, so the incoming-damage
    // penalty can never outlive the class that justified it.
    SyncClassResourceFloor();
    // Owner ruling 2026-08-14: a Caster starts FULL. Only on the transition
    // INTO Caster, never on every refresh — this function is also the tick's
    // safety poll and the respec handler, and refilling from either would hand
    // a player a free bar mid-fight for doing nothing.
    if (bIsCaster && !bWasCaster) FillToMaximum();
    RefreshOvercastState();
}

void UBreakerManaComponent::FillToMaximum()
{
    if (!Attributes || !IsActiveForOwner()) return;
    if (GetOwner() && !GetOwner()->HasAuthority()) return;
    // Queued conditional income is discarded, not banked: it would otherwise
    // arrive a frame after a refill and read as the bar overfilling.
    PendingGrants = 0.0f;
    Attributes->ApplyClassResource(Attributes->GetMaxClassResource());
    RefreshOvercastState();
}

void UBreakerManaComponent::SetOvercastFloor(float Floor)
{
    if (GetOwner() && !GetOwner()->HasAuthority()) return;
    OvercastFloor = FMath::Min(0.0f, Floor);
    SyncClassResourceFloor();
    RefreshOvercastState();
}

float UBreakerManaComponent::GetOvercastFloor() const
{
    const UBreakerProgressionComponent* Progression = CachedProgression.Get();
    const int32 Rank = IsActiveForOwner() && Progression ? Progression->GetNodeRank(TEXT("Caster.Spellblade.Debt"), EBreakerPointCurrency::DoctrinePoints) : 0;
    const FBreakerCasterResourceTuning& Tuning = GetResourceTuning();
    const float ExistingFloor = FMath::Min(0.0f, OvercastFloor) - (Rank >= 2 ? Tuning.DebtRankTwoExtension : Rank == 1 ? Tuning.DebtRankOneExtension : 0.0f);
    const bool bPrepared = IsActiveForOwner() && Progression && Progression->HasNodeTag(BreakerNodeTags::Node_MS_Prepared.GetTag());
    return bPrepared ? FMath::Min(ExistingFloor, Tuning.PreparedOvercastFloor) : ExistingFloor;
}

void UBreakerManaComponent::SyncClassResourceFloor()
{
    // Server authority only: the floor is a replicated attribute, so a client
    // that wrote its own would fight the replicated value.
    if (!Attributes || (GetOwner() && !GetOwner()->HasAuthority())) return;
    Attributes->ApplyClassResourceFloor(GetPublishedFloor());
}

bool UBreakerManaComponent::IsActiveForOwner() const
{
    return bIsCaster;
}

float UBreakerManaComponent::GetMana() const
{
    return Attributes ? Attributes->GetClassResource() : 0.0f;
}

float UBreakerManaComponent::GetManaFraction() const
{
    if (!Attributes) return 0.0f;
    const float Max = Attributes->GetMaxClassResource();
    return Max > 0.0f ? FMath::Clamp(Attributes->GetClassResource() / Max, 0.0f, 1.0f) : 0.0f;
}

bool UBreakerManaComponent::IsOvercast() const
{
    return IsActiveForOwner() && IsOvercastValue(GetMana());
}

bool UBreakerManaComponent::IsOverreachActive() const
{
    const auto* Progression = CachedProgression.Get();
    return IsOvercast() && Progression && Progression->HasNodeTag(BreakerNodeTags::Node_SB_Overreach.GetTag());
}

float UBreakerManaComponent::GetOvercastIncomingDamageTaken() const
{
    if (!IsOvercast()) return 0.0f;
    return FMath::Max(0.0f, IsOverreachActive() ? GetResourceTuning().OverreachIncomingDamageTaken : OvercastIncomingDamageTaken);
}

bool UBreakerManaComponent::CanAffordSpend(float Cost) const
{
    return IsActiveForOwner() && CanSpendFrom(GetMana(), Cost, GetPublishedFloor());
}

bool UBreakerManaComponent::TrySpendMana(float Cost)
{
    if (!Attributes || !GetOwner() || !GetOwner()->HasAuthority()) return false;
    if (!CanAffordSpend(Cost)) return false;
    ApplyManaDelta(-Cost);
    RefreshOvercastState();
    return true;
}

bool UBreakerManaComponent::IsInSafeZone() const
{
    const AActor* Owner = GetOwner();
    if (!Owner || !GetWorld()) return false;
    const ABreakerGameMode* GameMode = GetWorld()->GetAuthGameMode<ABreakerGameMode>();
    return GameMode && GameMode->IsInSafeZone(Owner->GetActorLocation());
}

void UBreakerManaComponent::ApplyManaDelta(float Delta)
{
    if (!Attributes || FMath::IsNearlyZero(Delta)) return;
    // ApplyClassResource, not the generated setter: identical in play (both go
    // through the ability system's base-value write and the same clamp), but it
    // does not fatally assert on an attribute set with no ability system, which
    // is what makes the whole loop exercisable in an automation test.
    Attributes->ApplyClassResource(ClampToBank(Attributes->GetClassResource() + Delta, GetPublishedFloor(), Attributes->GetMaxClassResource()));
}

void UBreakerManaComponent::RefreshOvercastState()
{
    const bool bNowOvercast = IsOvercast();
    if (bNowOvercast != bOvercast)
    {
        bOvercast = bNowOvercast;
        SyncOvercastDamagePenalty();
        OnOvercastChanged.Broadcast(bNowOvercast);
    }
}

FName UBreakerManaComponent::OvercastDamageModifierKey()
{
    return TEXT("Caster.Overcast");
}

float UBreakerManaComponent::OvercastIncomingMultiplier(bool bOvercastNow, float PenaltyFraction)
{
    return bOvercastNow ? 1.0f + FMath::Max(0.0f, PenaltyFraction) : 1.0f;
}

void UBreakerManaComponent::SyncOvercastDamagePenalty()
{
    AActor* Owner = GetOwner();
    UBreakerCombatComponent* Combat = Owner ? Owner->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
    if (!Combat) return;

    if (bOvercast)
    {
        Combat->PushIncomingDamageModifier(OvercastDamageModifierKey(), OvercastIncomingMultiplier(true, GetOvercastIncomingDamageTaken()));
    }
    else
    {
        Combat->RemoveIncomingDamageModifier(OvercastDamageModifierKey());
    }
}

void UBreakerManaComponent::GrantMana(float Amount, bool bIgnoreGlobalCap)
{
    if (Amount <= 0.0f || !GetOwner() || !GetOwner()->HasAuthority() || !IsActiveForOwner()) return;
    if (IsGenerationSuspended()) return;

    if (bIgnoreGlobalCap)
    {
        ApplyManaDelta(Amount * GenerationMultiplierForMana(GetMana(), OvercastGenerationMultiplier));
        RefreshOvercastState();
        return;
    }
    PendingGrants += Amount;
}

void UBreakerManaComponent::PushGenerationSuspension(FName Key)
{
    if (Key.IsNone()) return;
    GenerationSuspensions.Add(Key);
    ClearSequenceApplications();
    // Queued credits are dropped, not banked: a bar that leaps the instant the
    // window closes would read as the suspension never having happened.
    PendingGrants = 0.0f;
}

void UBreakerManaComponent::PopGenerationSuspension(FName Key)
{
    GenerationSuspensions.Remove(Key);
}

void UBreakerManaComponent::HandleMeleeHit(const FBreakerHitContext& Hit)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !IsActiveForOwner() || IsInSafeZone()) return;
    if (Hit.Instigator != GetOwner() || Hit.bFromDoT || Hit.Result.bDodged
        || Hit.Result.HealthDamage + Hit.Result.ShieldDamage <= 0.0f
        || !Hit.SourceTags.HasTagExact(BreakerAbilityTags::Damage_Melee.GetTag())) return;
    const UBreakerProgressionComponent* Progression = CachedProgression.Get();
    const bool bContactCharge = Progression && Progression->HasNodeTag(BreakerNodeTags::Node_SB_ContactCharge.GetTag());
    // SB1 replaces the baseline hit gain with the existing weak-point rate.
    // Ordinary weapon hits stay on OnShot's normalized volley path, so this
    // listener never multiplies their income by pellet count.
    const int32 BloodRank = Progression ? Progression->GetNodeRank(TEXT("Caster.Spellblade.Bloodprice"), EBreakerPointCurrency::DoctrinePoints) : 0;
    if (BloodRank > 0 && GetMana() < 0 && Hit.ProcCoefficient > 0)
    {
        if (UBreakerCombatComponent* Combat = GetOwner()->FindComponentByClass<UBreakerCombatComponent>(); Combat && !Combat->IsDead())
        {
            const FBreakerCasterResourceTuning& Tuning = GetResourceTuning();
            FBreakerHealRequest Heal;
            Heal.Amount = (Hit.Result.HealthDamage + Hit.Result.ShieldDamage)
                * (BloodRank >= 2 ? Tuning.BloodpriceRankTwoFraction : Tuning.BloodpriceRankOneFraction)
                * FMath::Clamp(Hit.ProcCoefficient, 0.0f, 1.0f);
            Heal.SourceTag = BreakerAbilityTags::Damage_Melee.GetTag();
            Heal.SetHealer(GetOwner());
            Combat->ApplyHealing(Heal);
        }
    }
    if (Hit.ProcCoefficient > 0) GrantMana((bContactCharge ? WeakPointGain : WeaponHitGain) * FMath::Clamp(Hit.ProcCoefficient, 0.0f, 1.0f), false);
}

void UBreakerManaComponent::NotifyStatusApplication(const FBreakerStatusApplicationSpec& Spec, bool bAlreadyPresent, AActor* Target)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !IsActiveForOwner() || IsInSafeZone()
        || IsGenerationSuspended() || !FMath::IsFinite(Spec.ProcCoefficient) || Spec.ProcCoefficient <= 0) return;
    const UBreakerCombatComponent* Combat = GetOwner()->FindComponentByClass<UBreakerCombatComponent>();
    if (Combat && Combat->IsDead()) { ClearSequenceApplications(); return; }
    const UBreakerProgressionComponent* Progression = CachedProgression.Get();
    RecordSequenceApplication(Spec, Target);
    const bool bFollowThrough = Progression && Progression->HasNodeTag(BreakerNodeTags::Node_SB_FollowThrough.GetTag());
    const bool bCleaveBleed = Spec.StatusTag == FGameplayTag::RequestGameplayTag(TEXT("Status.Bleed"))
        && Spec.Snapshot.SourceTags.HasTagExact(BreakerAbilityTags::Ability_Class_Caster_Cleave.GetTag());
    if (bAlreadyPresent && !(bFollowThrough && bCleaveBleed)) return;
    const FBreakerCasterResourceTuning& Tuning = GetResourceTuning();
    const int32 Rank = Progression ? Progression->GetNodeRank(TEXT("Caster.VoidWhisperer.Seep"), EBreakerPointCurrency::DoctrinePoints) : 0;
    float Multiplier = Rank >= 2 ? Tuning.SeepRankTwoMultiplier : Rank == 1 ? Tuning.SeepRankOneMultiplier : 1.0f;
    const int32 VarianceRank = !bAlreadyPresent && Progression ? Progression->GetNodeRank(TEXT("Caster.Multispell.Variance"), EBreakerPointCurrency::DoctrinePoints) : 0;
    Multiplier += VarianceRank >= 2 ? Tuning.VarianceRankTwoMultiplier - 1.0f : VarianceRank == 1 ? Tuning.VarianceRankOneMultiplier - 1.0f : 0.0f;
    GrantMana(Tuning.StatusApplicationMana * Multiplier * FMath::Clamp(Spec.ProcCoefficient, 0.0f, 1.0f), false);
}

void UBreakerManaComponent::ClearSequenceApplications()
{
    // Preserve per-target cooldowns through respec/suspension. Otherwise
    // changing a build could repeatedly cash the same target's payout.
    for (auto& Pair : SequenceTargets) Pair.Value.Applications.Reset();
}

void UBreakerManaComponent::RecordSequenceApplication(const FBreakerStatusApplicationSpec& Spec, AActor* Target)
{
    if (!Target || Target->IsActorBeingDestroyed() || Target == GetOwner() || !GetWorld() || !BreakerStatusRules::FindRule(Spec.StatusTag)) return;
    const UBreakerProgressionComponent* Progression = CachedProgression.Get();
    const int32 Rank = Progression ? Progression->GetNodeRank(TEXT("Caster.Multispell.Sequence"), EBreakerPointCurrency::DoctrinePoints) : 0;
    if (Rank <= 0) { ClearSequenceApplications(); return; }
    const UBreakerCombatComponent* TargetCombat = Target->FindComponentByClass<UBreakerCombatComponent>();
    if (!TargetCombat || TargetCombat->IsDead()) return;
    const FBreakerCasterResourceTuning& Tuning = GetResourceTuning();
    const double Now = GetWorld()->GetTimeSeconds();
    for (auto It = SequenceTargets.CreateIterator(); It; ++It)
    {
        AActor* Tracked = It.Key().Get();
        const UBreakerCombatComponent* TrackedCombat = Tracked ? Tracked->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
        if (!TrackedCombat || Tracked->IsActorBeingDestroyed() || TrackedCombat->IsDead()) { It.RemoveCurrent(); continue; }
        for (auto Entry = It.Value().Applications.CreateIterator(); Entry; ++Entry)
            if (Now - Entry.Value().Time > Tuning.SequenceWindowSeconds) Entry.RemoveCurrent();
        if (It.Value().Applications.IsEmpty() && Now >= It.Value().NextAllowedTime) It.RemoveCurrent();
    }
    FSequenceTarget& State = SequenceTargets.FindOrAdd(Target);
    if (Now < State.NextAllowedTime) return;
    State.Applications.Add(Spec.StatusTag, FSequenceApplication{Now, FMath::Clamp(Spec.ProcCoefficient, 0.0f, 1.0f)});
    if (State.Applications.Num() < 3) return;
    float Proc = 1.0f;
    for (const auto& Pair : State.Applications) Proc = FMath::Min(Proc, Pair.Value.ProcCoefficient);
    State.Applications.Reset();
    State.NextAllowedTime = Now + Tuning.SequenceCooldownSeconds;
    // Authored lump sum has its own per-target cooldown; the ordinary hit
    // income meter must not discard it. Suspension and bank limits still apply.
    GrantMana((Rank >= 2 ? Tuning.SequenceRankTwoMana : Tuning.SequenceRankOneMana) * Proc, true);
}

void UBreakerManaComponent::NotifyAfflictedVictimDeath()
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !IsActiveForOwner() || IsInSafeZone()) return;
    const UBreakerCombatComponent* Combat = GetOwner()->FindComponentByClass<UBreakerCombatComponent>();
    if (Combat && Combat->IsDead()) return;
    const UBreakerProgressionComponent* Progression = CachedProgression.Get();
    const int32 Rank = Progression ? Progression->GetNodeRank(TEXT("Caster.VoidWhisperer.Attrition"), EBreakerPointCurrency::DoctrinePoints) : 0;
    const FBreakerCasterResourceTuning& Tuning = GetResourceTuning();
    if (Rank > 0) GrantMana(Rank >= 2 ? Tuning.AttritionRankTwoRefund : Tuning.AttritionRankOneRefund, true);
}

void UBreakerManaComponent::HandleCasterKill(const FBreakerHitContext& Hit)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !IsActiveForOwner() || IsInSafeZone()
        || Hit.bFromDoT || Hit.ProcCoefficient <= 0 || Hit.Instigator != GetOwner()
        || !Hit.SourceTags.HasTagExact(BreakerAbilityTags::Ability_Class_Caster_Cleave.GetTag())) return;
    const UBreakerCombatComponent* Combat = GetOwner()->FindComponentByClass<UBreakerCombatComponent>();
    if (Combat && Combat->IsDead()) return;
    const UBreakerProgressionComponent* Progression = CachedProgression.Get();
    const int32 Rank = Progression ? Progression->GetNodeRank(TEXT("Caster.Spellblade.FollowThrough"), EBreakerPointCurrency::DoctrinePoints) : 0;
    if (Rank <= 0) return;
    UBreakerAbilityDefinition::FindFallback(TEXT("Caster.Cleave"));
    const UBreakerAbility_Cleave* Cleave = GetDefault<UBreakerAbility_Cleave>();
    GrantMana(Rank >= 2 ? Cleave->FollowThroughRankTwoKillRefund : Cleave->FollowThroughRankOneKillRefund, true);
}

void UBreakerManaComponent::HandleShot(const FBreakerShotResult& Shot)
{
    if (Shot.bFired && GetOwner() && GetOwner()->HasAuthority()) SecondsSinceWeaponFire = 0.0f;
    // Landed hits only: a fired-and-missed shot banks nothing, and DoT ticks
    // never arrive here at all (they carry proc coefficient 0 by rule).
    if (!Shot.bFired || !Shot.bHit || Shot.DamageResult.bDodged
        || Shot.DamageResult.HealthDamage + Shot.DamageResult.ShieldDamage <= 0.0f
        || !GetOwner() || !GetOwner()->HasAuthority() || !IsActiveForOwner() || IsInSafeZone()) return;
    // bHit includes world geometry. Use the volley's actual damage, not its
    // last HitActor: a later pellet may touch a wall after another paid damage.
    // Unmake suspends generation outright (Class-Kits §2.2).
    if (IsGenerationSuspended()) return;

    int32 PelletsPerShot = 1;
    if (const UBreakerWeaponComponent* Weapon = GetOwner()->FindComponentByClass<UBreakerWeaponComponent>())
    {
        if (const UBreakerWeaponDefinition* Definition = Weapon->GetActiveDefinition())
        {
            PelletsPerShot = FMath::Max(1, Definition->PelletsPerShot);
        }
    }
    // FBreakerShotResult aggregates a multishot volley into one event and does
    // not carry a landed-pellet count, so a landed volley is credited in full
    // (n/n) — which is exactly the "shotgun banks like a rifle" outcome the
    // anti-Multishot clause wants. AWAITING WEAPONS: when the attacker-side
    // per-hit event (Ability-Implementation-Spec SI-8) lands, pass the real
    // landed count here and partial volleys will pay 1/n per pellet.
    float Gain = HitGeneration(Shot.bWeakPoint, PelletsPerShot, PelletsPerShot, WeaponHitGain, WeakPointGain, 1.0f);
    const UBreakerProgressionComponent* Progression = CachedProgression.Get();
    const int32 CloseRank = Progression ? Progression->GetNodeRank(TEXT("Caster.Spellblade.Close"), EBreakerPointCurrency::DoctrinePoints) : 0;
    if (CloseRank > 0 && Shot.HitActor && Shot.HitActor->FindComponentByClass<UBreakerCombatComponent>()
        && !Shot.DamageResult.bDodged && Shot.DamageResult.HealthDamage + Shot.DamageResult.ShieldDamage > 0)
    {
        const FBreakerCasterResourceTuning& Tuning = GetResourceTuning();
        const float Range = CloseRank >= 2 ? Tuning.CloseRankTwoRangeCm : Tuning.CloseRankOneRangeCm;
        if (FVector::Dist(Shot.TraceStart, Shot.ImpactPoint) <= Range) Gain *= 2.0f;
    }
    PendingGrants += Gain;
}

void UBreakerManaComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    AdvanceLoop(DeltaTime);
}

void UBreakerManaComponent::AdvanceLoop(float DeltaTime)
{
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority() || !Attributes || DeltaTime <= 0.0f) return;

    // Defensive backstop, not a workaround for a broadcast that never fires:
    // DevForceClass DOES call OnProgressionChanged.Broadcast(). This poll
    // covers what the broadcast alone cannot — a component bound via
    // BindAttributes with no BeginPlay never registers for the delegate at
    // all, and no future mutator of PermanentClass is guaranteed to
    // broadcast — so it is what would otherwise leave this component
    // publishing a Caster floor for a non-Caster. Cheap comparison, and it is
    // the safety net that guarantees the floor cannot be stranded.
    const UBreakerProgressionComponent* Progression = CachedProgression.Get();
    const EBreakerClassId LiveClass = Progression ? Progression->GetProgressionState().PermanentClass : EBreakerClassId::None;
    if (LiveClass != ObservedClass) RefreshClassOwnership();
    SyncClassResourceFloor();
    const float PreviousFireAge = SecondsSinceWeaponFire;
    SecondsSinceWeaponFire = FMath::Min(SecondsSinceWeaponFire + DeltaTime, 1000000.0f);

    if (!IsActiveForOwner())
    {
        PendingGrants = 0.0f;
        ClearSequenceApplications();
        return;
    }

    // Suspension is checked BEFORE regeneration, because Unmake suspends
    // "Mana generation" (Class-Kits §2.2) and regeneration is now the bulk of
    // it. If regeneration ran through the free window, Unmake would hand back
    // most of its own 80-Mana price while it was being spent.
    if (IsGenerationSuspended())
    {
        PendingGrants = 0.0f;
        ClearSequenceApplications();
        RefreshOvercastState();
        return;
    }

    // Passive regeneration, the primary recovery path under the 2026-08-14
    // owner ruling. Deliberately ABOVE the safe-zone gate and outside the
    // per-second generation budget:
    //  * the safe-zone gate is an anti-FARM rule aimed at target-dependent
    //    income, and a Caster who cannot refill in camp would have to leave it
    //    to become able to fight;
    //  * the budget is the ceiling on that same conditional income. Metering
    //    the baseline through it would make every accelerator a no-op whenever
    //    regeneration alone already filled the frame's allowance.
    // Overcast doubling still applies, because clearing a debt faster is
    // exactly what the doubling is for.
    const float CoreFlatRegen = BreakerResourceGeneration::FlatRate(Owner);
    if (PassiveRegenPerSecond > 0.0f || CoreFlatRegen > 0.0f)
    {
        const int32 Rank = Progression ? Progression->GetNodeRank(TEXT("Caster.VoidWhisperer.Patience"), EBreakerPointCurrency::DoctrinePoints) : 0;
        const FBreakerCasterResourceTuning& Tuning = GetResourceTuning();
        const float Delay = Rank >= 2 ? Tuning.PatienceRankTwoDelay : Tuning.PatienceRankOneDelay;
        const float BonusSeconds = Rank > 0 ? FMath::Clamp(DeltaTime - FMath::Max(0.0f, Delay - PreviousFireAge), 0.0f, DeltaTime) : 0.0f;
        ApplyManaDelta(((PassiveRegenPerSecond + CoreFlatRegen) * DeltaTime + Tuning.PatienceBonusRegenPerSecond * BonusSeconds)
            * GenerationMultiplierForMana(GetMana(), OvercastGenerationMultiplier)
            * BreakerResourceGeneration::Multiplier(GetOwner()));
    }

    if (IsInSafeZone())
    {
        PendingGrants = 0.0f;
        ClearSequenceApplications();
        RefreshOvercastState();
        return;
    }

    // Standing Water is one owner income stream, independent of bodies or
    // overlapping puddles. It shares the same budget as other accelerators.
    const UBreakerCombatComponent* OwnerCombat = GetOwner()->FindComponentByClass<UBreakerCombatComponent>();
    const int32 StandingRank = Progression ? Progression->GetNodeRank(TEXT("Caster.VoidWhisperer.StandingWater"), EBreakerPointCurrency::DoctrinePoints) : 0;
    if (StandingRank > 0 && OwnerCombat && !OwnerCombat->IsDead())
    {
        UBreakerAbilityDefinition::FindFallback(TEXT("Caster.Rot"));
        const UBreakerAbility_Rot* Rot = GetDefault<UBreakerAbility_Rot>();
        const float Rate = StandingRank >= 2 ? Rot->StandingWaterRankTwoManaPerSecond : Rot->StandingWaterRankOneManaPerSecond;
        GrantMana(Rate * ABreakerZoneActor::OwnedOccupiedSeconds(GetOwner(), BreakerAbilityTags::Zone_Caster_Rot.GetTag(), DeltaTime), false);
    }
    // Conditional income on top: accelerators, metered against the budget.
    if (PendingGrants > 0.0f)
    {
        const float Budget = ClampGeneration(GlobalGenerationCap, GlobalGenerationCap) * DeltaTime;
        const float Drawn = FMath::Min(PendingGrants, Budget);
        PendingGrants -= Drawn;
        // Overcast doubling is evaluated against the bank as it stands when the
        // credit is paid, so it stops the instant the debt is cleared.
        ApplyManaDelta(Drawn * GenerationMultiplierForMana(GetMana(), OvercastGenerationMultiplier)
            * BreakerResourceGeneration::Multiplier(GetOwner()));
    }

    RefreshOvercastState();
}
