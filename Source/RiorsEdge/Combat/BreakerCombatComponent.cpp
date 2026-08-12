#include "Combat/BreakerCombatComponent.h"

#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Combat/BreakerDamageLibrary.h"

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
    if (!Attributes || !GetOwner() || !GetOwner()->HasAuthority()) return;
    TimeSinceStaminaSpend += DeltaTime;
    if (TimeSinceStaminaSpend >= StaminaRegenerationDelay && Attributes->GetStamina() < Attributes->GetMaxStamina())
    {
        Attributes->SetStamina(FMath::Min(Attributes->GetMaxStamina(), Attributes->GetStamina() + StaminaRegenerationPerSecond * DeltaTime));
    }
}

FBreakerDamageResult UBreakerCombatComponent::ReceiveDamage(const FBreakerDamageRequest& Request)
{
    FBreakerDamageResult Result;
    if (!Attributes || !GetOwner() || !GetOwner()->HasAuthority() || IsDead()) return Result;

    FBreakerDefenseState Defense;
    Defense.Health = Attributes->GetHealth();
    Defense.Shield = Attributes->GetShield();
    Defense.Armor = Attributes->GetArmor();
    Result = UBreakerDamageLibrary::ResolveDamage(Request, Defense);
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

bool UBreakerCombatComponent::SpendStamina(float Cost)
{
    if (!Attributes || Cost < 0.0f || Attributes->GetStamina() < Cost) return false;
    Attributes->SetStamina(Attributes->GetStamina() - Cost);
    TimeSinceStaminaSpend = 0.0f;
    return true;
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
    Attributes->SetStamina(Attributes->GetMaxStamina());
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
