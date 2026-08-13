#include "Combat/BreakerCombatComponent.h"

#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Combat/BreakerDamageLibrary.h"
#include "GameFramework/Actor.h"
#include "Items/BreakerEquipmentComponent.h"
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
    Defense.bDodgeInvulnerable = IsDodgeInvulnerable();
    Defense.BlockMitigation = BlockMitigation;
    // Blocking only counts when the stance is up, stamina can pay for the
    // hit, and the hit comes from the front half-space.
    Defense.bBlockingStance = bBlocking && Attributes->GetStamina() >= BlockStaminaCostPerHit;
    Defense.BlockChance = BlockChance;
    if (Request.bHasSourceLocation && GetOwner())
    {
        const FVector ToSource = (Request.SourceLocation - GetOwner()->GetActorLocation()).GetSafeNormal2D();
        Defense.bAttackFromFront = FVector::DotProduct(GetOwner()->GetActorForwardVector().GetSafeNormal2D(), ToSource) > 0.0f;
    }

    Result = UBreakerDamageLibrary::ResolveDamage(Request, Defense);
    if (Result.bDodged)
    {
        AddClassResource(DodgeResourceRefund);
        OnDamageReceived.Broadcast(Result);
        return Result;
    }
    if (Result.bBlocked) SpendStamina(BlockStaminaCostPerHit);
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

void UBreakerCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UBreakerCombatComponent, bBlocking);
}

void UBreakerCombatComponent::SetBlocking(bool bNewBlocking)
{
    if (GetOwner() && !GetOwner()->HasAuthority())
    {
        ServerSetBlocking(bNewBlocking);
        return;
    }
    bBlocking = bNewBlocking;
}

void UBreakerCombatComponent::ServerSetBlocking_Implementation(bool bNewBlocking)
{
    SetBlocking(bNewBlocking);
}

bool UBreakerCombatComponent::TryDodge()
{
    if (GetOwner() && !GetOwner()->HasAuthority())
    {
        ServerTryDodge();
        return false;
    }
    if (IsDodgeInvulnerable() || !SpendStamina(DodgeStaminaCost)) return false;
    DodgeWindowEndTime = GetWorld() ? GetWorld()->GetTimeSeconds() + DodgeWindowSeconds : 0.0;
    return true;
}

void UBreakerCombatComponent::ServerTryDodge_Implementation()
{
    TryDodge();
}

bool UBreakerCombatComponent::IsDodgeInvulnerable() const
{
    return GetWorld() && GetWorld()->GetTimeSeconds() < DodgeWindowEndTime;
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
