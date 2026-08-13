#include "Classes/BreakerManaComponent.h"

#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Combat/BreakerCombatComponent.h"
#include "Game/BreakerGameMode.h"
#include "GameFramework/Actor.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponDefinition.h"

UBreakerManaComponent::UBreakerManaComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    SetIsReplicatedByDefault(true);
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
            Progression->OnProgressionChanged.AddDynamic(this, &UBreakerManaComponent::HandleProgressionChanged);
        }
        // Weapon hits are the Caster's primary bank. Statuses, kills, and
        // reloads (Class-Kits 2.1) wait on hooks the combat and status layers
        // do not publish attacker-side yet.
        if (UBreakerWeaponComponent* Weapon = Owner->FindComponentByClass<UBreakerWeaponComponent>())
        {
            Weapon->OnShot.AddDynamic(this, &UBreakerManaComponent::HandleShot);
        }
    }
    // Resolves the class, publishes the floor, and puts the Overcast state (and
    // its damage penalty) in agreement with a bank that may have been restored
    // from a save mid-debt.
    HandleProgressionChanged();
}

void UBreakerManaComponent::BindAttributes(UBreakerAttributeSet* InAttributes)
{
    Attributes = InAttributes;
    RefreshClassOwnership();
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
}

void UBreakerManaComponent::RefreshClassOwnership()
{
    if (!CachedProgression.IsValid() && GetOwner())
    {
        CachedProgression = GetOwner()->FindComponentByClass<UBreakerProgressionComponent>();
    }
    const UBreakerProgressionComponent* Progression = CachedProgression.Get();
    ObservedClass = Progression ? Progression->GetProgressionState().PermanentClass : EBreakerClassId::None;
    bIsCaster = ObservedClass == EBreakerClassId::Caster;
    if (!bIsCaster) PendingGrants = 0.0f;
    // Order matters: close the floor first (which lifts a stranded negative
    // bank back to zero), then re-evaluate Overcast, so the incoming-damage
    // penalty can never outlive the class that justified it.
    SyncClassResourceFloor();
    RefreshOvercastState();
}

void UBreakerManaComponent::SetOvercastFloor(float Floor)
{
    if (GetOwner() && !GetOwner()->HasAuthority()) return;
    OvercastFloor = FMath::Min(0.0f, Floor);
    SyncClassResourceFloor();
    RefreshOvercastState();
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

float UBreakerManaComponent::GetOvercastIncomingDamageTaken() const
{
    return IsOvercast() ? FMath::Max(0.0f, OvercastIncomingDamageTaken) : 0.0f;
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
        Combat->PushIncomingDamageModifier(OvercastDamageModifierKey(), OvercastIncomingMultiplier(true, OvercastIncomingDamageTaken));
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
    // Queued credits are dropped, not banked: a bar that leaps the instant the
    // window closes would read as the suspension never having happened.
    PendingGrants = 0.0f;
}

void UBreakerManaComponent::PopGenerationSuspension(FName Key)
{
    GenerationSuspensions.Remove(Key);
}

void UBreakerManaComponent::HandleShot(const FBreakerShotResult& Shot)
{
    // Landed hits only: a fired-and-missed shot banks nothing, and DoT ticks
    // never arrive here at all (they carry proc coefficient 0 by rule).
    if (!Shot.bFired || !Shot.bHit || !GetOwner() || !GetOwner()->HasAuthority() || !IsActiveForOwner() || IsInSafeZone()) return;
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
    PendingGrants += HitGeneration(Shot.bWeakPoint, PelletsPerShot, PelletsPerShot, WeaponHitGain, WeakPointGain, 1.0f);
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

    // Class changes that do not broadcast (DevForceClass, and any future path
    // that mutates the progression state directly) would otherwise leave this
    // component publishing a Caster floor for a non-Caster. Cheap comparison,
    // and it is the safety net that guarantees the floor cannot be stranded.
    const UBreakerProgressionComponent* Progression = CachedProgression.Get();
    const EBreakerClassId LiveClass = Progression ? Progression->GetProgressionState().PermanentClass : EBreakerClassId::None;
    if (LiveClass != ObservedClass) RefreshClassOwnership();

    if (!IsActiveForOwner())
    {
        PendingGrants = 0.0f;
        return;
    }
    if (IsInSafeZone())
    {
        PendingGrants = 0.0f;
        RefreshOvercastState();
        return;
    }

    if (IsGenerationSuspended())
    {
        PendingGrants = 0.0f;
        RefreshOvercastState();
        return;
    }

    // The bank never decays and never trickles: with nothing queued there is
    // nothing to do but keep the Overcast state honest.
    if (PendingGrants > 0.0f)
    {
        const float Budget = ClampGeneration(GlobalGenerationCap, GlobalGenerationCap) * DeltaTime;
        const float Drawn = FMath::Min(PendingGrants, Budget);
        PendingGrants -= Drawn;
        // Overcast doubling is evaluated against the bank as it stands when the
        // credit is paid, so it stops the instant the debt is cleared.
        ApplyManaDelta(Drawn * GenerationMultiplierForMana(GetMana(), OvercastGenerationMultiplier));
    }

    RefreshOvercastState();
}
