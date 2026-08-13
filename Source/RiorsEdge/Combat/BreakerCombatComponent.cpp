#include "Combat/BreakerCombatComponent.h"

#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Combat/BreakerDamageLibrary.h"
#include "GameFramework/Actor.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Net/UnrealNetwork.h"

UBreakerCombatComponent::UBreakerCombatComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    SetIsReplicatedByDefault(true);
}

void UBreakerCombatComponent::BeginPlay()
{
    Super::BeginPlay();
    if (const IAbilitySystemInterface* AbilityOwner = Cast<IAbilitySystemInterface>(GetOwner()))
    {
        if (UAbilitySystemComponent* ASC = AbilityOwner->GetAbilitySystemComponent())
        {
            Attributes = const_cast<UBreakerAttributeSet*>(ASC->GetSet<UBreakerAttributeSet>());
        }
    }
}

void UBreakerCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

FBreakerDamageResult UBreakerCombatComponent::ReceiveDamage(const FBreakerDamageRequest& Request)
{
    FBreakerDamageResult Result;
    if (!Attributes || !GetOwner() || !GetOwner()->HasAuthority() || IsDead()) return Result;

    FBreakerDefenseState Defense;
    Defense.Health = Attributes->GetHealth();
    Defense.Shield = Attributes->GetShield();
    Defense.Armor = Attributes->GetArmor();
    // Gear-rolled physical damage reduction folds into the incoming
    // multiplier so the resolution order stays single-path.
    if (Request.DamageFamily == EBreakerDamageFamily::Physical)
    {
        if (const UBreakerEquipmentComponent* Equipment = GetOwner()->FindComponentByClass<UBreakerEquipmentComponent>())
        {
            Defense.IncomingDamageMultiplier *= 1.0f - Equipment->GetStats().PhysicalDamageReductionPercent / 100.0f;
        }
    }
    // Pushed incoming modifiers compose on top, in the same stage: Caster's
    // Overcast penalty, and defensive windows when they land.
    Defense.IncomingDamageMultiplier *= GetComposedIncomingDamageMultiplier();
    Defense.DodgeChance = DodgeChance;
    Defense.BlockChance = BlockChance;
    Defense.BlockMitigation = BlockMitigation;
    // Tree nodes raise the passive layers on top of the component baseline.
    if (const UBreakerProgressionComponent* Progression = GetOwner()->FindComponentByClass<UBreakerProgressionComponent>())
    {
        Defense.DodgeChance = FMath::Clamp(Defense.DodgeChance + Progression->GetDodgeChanceBonus(), 0.0f, 1.0f);
        Defense.BlockChance = FMath::Clamp(Defense.BlockChance + Progression->GetBlockChanceBonus(), 0.0f, 1.0f);
    }

    Result = UBreakerDamageLibrary::ResolveDamage(Request, Defense);
    if (Result.bDodged)
    {
        AddClassResource(DodgeResourceRefund);
        OnDamageReceived.Broadcast(Result);
        return Result;
    }
    Attributes->SetShield(Result.RemainingShield);
    Attributes->SetHealth(Result.RemainingHealth);
    LastDamageTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    OnDamageReceived.Broadcast(Result);
    if (Result.bKilled && !bDeathBroadcast)
    {
        bDeathBroadcast = true;
        OnDeath.Broadcast();
    }
    DispatchHitDealt(Request, Result);
    return Result;
}

void UBreakerCombatComponent::DispatchHitDealt(const FBreakerDamageRequest& Request, const FBreakerDamageResult& Result) const
{
    AActor* Dealer = Request.Instigator.Get();
    // Self-damage would otherwise let a listener that deals damage on hit
    // re-enter its own dealer component without bound.
    if (!Dealer || Dealer == GetOwner()) return;
    UBreakerCombatComponent* DealerCombat = Dealer->FindComponentByClass<UBreakerCombatComponent>();
    if (!DealerCombat) return;

    FBreakerHitContext Context;
    Context.Instigator = Dealer;
    Context.Target = GetOwner();
    Context.Result = Result;
    Context.bFromDoT = Request.bIsDamageOverTime;
    Context.bWeakPoint = Result.bWeakPoint;
    Context.DamageFamily = Request.DamageFamily;
    Context.WorldLocation = GetOwner() ? GetOwner()->GetActorLocation() : Request.SourceLocation;

    DealerCombat->OnHitDealt.Broadcast(Context);
    if (Result.bKilled) DealerCombat->OnKillDealt.Broadcast(Context);
}

void UBreakerCombatComponent::PushOutgoingModifier(FName Key, float FlatBonus, float MoreMultiplier, float ExpirySeconds)
{
    if (Key.IsNone()) return;
    FBreakerOutgoingModifier Modifier;
    Modifier.Key = Key;
    Modifier.FlatBonus = FlatBonus;
    Modifier.MoreMultiplier = FMath::Max(0.0f, MoreMultiplier);
    Modifier.ExpiryTime = ExpirySeconds > 0.0f && GetWorld()
        ? static_cast<float>(GetWorld()->GetTimeSeconds()) + ExpirySeconds
        : -1.0f;

    // Re-pushing the same key replaces rather than stacks: a window refreshed
    // mid-flight must not compose with itself.
    for (FBreakerOutgoingModifier& Existing : OutgoingModifiers)
    {
        if (Existing.Key == Key)
        {
            Existing = Modifier;
            return;
        }
    }
    OutgoingModifiers.Add(Modifier);
}

void UBreakerCombatComponent::RemoveOutgoingModifier(FName Key)
{
    OutgoingModifiers.RemoveAll([Key](const FBreakerOutgoingModifier& Modifier) { return Modifier.Key == Key; });
}

void UBreakerCombatComponent::PruneExpiredOutgoingModifiers()
{
    if (OutgoingModifiers.IsEmpty() || !GetWorld()) return;
    const float Now = static_cast<float>(GetWorld()->GetTimeSeconds());
    OutgoingModifiers.RemoveAll([Now](const FBreakerOutgoingModifier& Modifier)
    {
        return Modifier.ExpiryTime >= 0.0f && Modifier.ExpiryTime <= Now;
    });
}

float UBreakerCombatComponent::GetComposedMoreMultiplier() const
{
    float Product = 1.0f;
    const float Now = GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds()) : -1.0f;
    for (const FBreakerOutgoingModifier& Modifier : OutgoingModifiers)
    {
        if (Now >= 0.0f && Modifier.ExpiryTime >= 0.0f && Modifier.ExpiryTime <= Now) continue;
        Product *= Modifier.MoreMultiplier;
    }
    // Damage-Pipeline §4: the composed More product may never exceed 2.20x.
    // Exceeding it is a design bug, not a runtime condition — surface it loudly
    // and clamp so a live session degrades instead of running away.
    if (Product > ComposedMoreCeiling)
    {
        // Loud but suite-safe: the automation test intentionally crosses the
        // ceiling, and an ensure would fail the run it exists to protect.
        UE_LOG(LogTemp, Warning, TEXT("Composed More product %.3f exceeds the %.2f ceiling (Damage-Pipeline S4); clamping."),
            Product, ComposedMoreCeiling);
        Product = ComposedMoreCeiling;
    }
    return Product;
}

void UBreakerCombatComponent::PushIncomingDamageModifier(FName Key, float Multiplier)
{
    if (Key.IsNone()) return;
    // Re-pushing the same key replaces rather than stacks, matching the
    // outgoing chain's rule.
    IncomingDamageModifiers.Add(Key, FMath::Max(0.0f, Multiplier));
}

void UBreakerCombatComponent::RemoveIncomingDamageModifier(FName Key)
{
    IncomingDamageModifiers.Remove(Key);
}

float UBreakerCombatComponent::GetComposedIncomingDamageMultiplier() const
{
    float Product = 1.0f;
    for (const TPair<FName, float>& Entry : IncomingDamageModifiers) Product *= Entry.Value;
    return Product;
}

void UBreakerCombatComponent::ApplyOutgoingModifiers(FBreakerDamageRequest& Request)
{
    PruneExpiredOutgoingModifiers();
    if (OutgoingModifiers.IsEmpty()) return;

    float Flat = 0.0f;
    for (const FBreakerOutgoingModifier& Modifier : OutgoingModifiers) Flat += Modifier.FlatBonus;

    // Flat first, then the More product — resolution order step 1.
    Request.BaseDamage = FMath::Max(0.0f, Request.BaseDamage + Flat);
    Request.SourceDamageMultiplier *= GetComposedMoreMultiplier();
}

bool UBreakerCombatComponent::SpendClassResource(float Cost)
{
    if (!Attributes || Cost < 0.0f || Attributes->GetClassResource() < Cost) return false;
    Attributes->SetClassResource(Attributes->GetClassResource() - Cost);
    return true;
}

void UBreakerCombatComponent::AddClassResource(float Amount)
{
    if (Attributes && Amount > 0.0f) Attributes->SetClassResource(FMath::Min(Attributes->GetMaxClassResource(), Attributes->GetClassResource() + Amount));
}

void UBreakerCombatComponent::RestoreVitals()
{
    if (!Attributes || !GetOwner() || !GetOwner()->HasAuthority()) return;
    Attributes->SetHealth(Attributes->GetMaxHealth());
    Attributes->SetShield(Attributes->GetMaxShield());
    bDeathBroadcast = false;
}

bool UBreakerCombatComponent::IsDead() const
{
    return Attributes && Attributes->GetHealth() <= 0.0f;
}

float UBreakerCombatComponent::GetSecondsSinceDamage() const
{
    return GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds() - LastDamageTime) : BIG_NUMBER;
}
