#include "Classes/BreakerManaComponent.h"

#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
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
    HandleProgressionChanged();
    bOvercast = IsOvercastValue(GetMana());
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
    if (!CachedProgression.IsValid() && GetOwner())
    {
        CachedProgression = GetOwner()->FindComponentByClass<UBreakerProgressionComponent>();
    }
    const UBreakerProgressionComponent* Progression = CachedProgression.Get();
    bIsCaster = Progression && Progression->GetProgressionState().PermanentClass == EBreakerClassId::Caster;
    if (!bIsCaster) PendingGrants = 0.0f;
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
    return IsActiveForOwner() && CanSpendFrom(GetMana(), Cost, GetOvercastFloor());
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
    Attributes->SetClassResource(ClampToBank(Attributes->GetClassResource() + Delta, GetOvercastFloor(), Attributes->GetMaxClassResource()));
}

void UBreakerManaComponent::RefreshOvercastState()
{
    const bool bNowOvercast = IsOvercast();
    if (bNowOvercast != bOvercast)
    {
        bOvercast = bNowOvercast;
        OnOvercastChanged.Broadcast(bNowOvercast);
    }
}

void UBreakerManaComponent::HandleShot(const FBreakerShotResult& Shot)
{
    // Landed hits only: a fired-and-missed shot banks nothing, and DoT ticks
    // never arrive here at all (they carry proc coefficient 0 by rule).
    if (!Shot.bFired || !Shot.bHit || !GetOwner() || !GetOwner()->HasAuthority() || !IsActiveForOwner() || IsInSafeZone()) return;

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
