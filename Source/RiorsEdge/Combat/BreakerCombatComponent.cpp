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
    return Result;
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
