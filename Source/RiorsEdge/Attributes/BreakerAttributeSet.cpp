#include "Attributes/BreakerAttributeSet.h"

#include "Net/UnrealNetwork.h"

UBreakerAttributeSet::UBreakerAttributeSet()
{
    InitHealth(100.0f);
    InitMaxHealth(100.0f);
    InitShield(0.0f);
    InitMaxShield(0.0f);
    InitArmor(0.0f);
    InitClassResource(0.0f);
    InitMaxClassResource(100.0f);
    InitCriticalChance(0.05f);
    InitCriticalMultiplier(1.5f);
    InitDamageMultiplier(1.0f);
    InitDamageOverTimeMultiplier(1.0f);
    InitMoveSpeed(650.0f);
}

void UBreakerAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
#define BREAKER_REPLICATE(Attribute) DOREPLIFETIME_CONDITION_NOTIFY(UBreakerAttributeSet, Attribute, COND_None, REPNOTIFY_Always)
    BREAKER_REPLICATE(Health);
    BREAKER_REPLICATE(MaxHealth);
    BREAKER_REPLICATE(Shield);
    BREAKER_REPLICATE(MaxShield);
    BREAKER_REPLICATE(Armor);
    BREAKER_REPLICATE(ClassResource);
    BREAKER_REPLICATE(MaxClassResource);
    BREAKER_REPLICATE(CriticalChance);
    BREAKER_REPLICATE(CriticalMultiplier);
    BREAKER_REPLICATE(DamageMultiplier);
    BREAKER_REPLICATE(DamageOverTimeMultiplier);
    BREAKER_REPLICATE(MoveSpeed);
#undef BREAKER_REPLICATE
}

void UBreakerAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    Super::PreAttributeChange(Attribute, NewValue);
    if (Attribute == GetHealthAttribute()) NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
    else if (Attribute == GetMaxHealthAttribute()) NewValue = FMath::Max(1.0f, NewValue);
    else if (Attribute == GetShieldAttribute()) NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxShield());
    else if (Attribute == GetMaxShieldAttribute()) NewValue = FMath::Max(0.0f, NewValue);
    else if (Attribute == GetArmorAttribute()) NewValue = FMath::Max(0.0f, NewValue);
    else if (Attribute == GetClassResourceAttribute()) NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxClassResource());
    else if (Attribute == GetMaxClassResourceAttribute()) NewValue = FMath::Max(0.0f, NewValue);
    else if (Attribute == GetCriticalChanceAttribute()) NewValue = FMath::Clamp(NewValue, 0.0f, 1.0f);
    else if (Attribute == GetCriticalMultiplierAttribute()) NewValue = FMath::Max(1.0f, NewValue);
    else if (Attribute == GetDamageMultiplierAttribute() || Attribute == GetDamageOverTimeMultiplierAttribute()) NewValue = FMath::Max(0.0f, NewValue);
    else if (Attribute == GetMoveSpeedAttribute()) NewValue = FMath::Max(0.0f, NewValue);
}

#define BREAKER_ON_REP(Name) void UBreakerAttributeSet::OnRep_##Name(const FGameplayAttributeData& OldValue) const { GAMEPLAYATTRIBUTE_REPNOTIFY(UBreakerAttributeSet, Name, OldValue); }
BREAKER_ON_REP(Health)
BREAKER_ON_REP(MaxHealth)
BREAKER_ON_REP(Shield)
BREAKER_ON_REP(MaxShield)
BREAKER_ON_REP(Armor)
BREAKER_ON_REP(ClassResource)
BREAKER_ON_REP(MaxClassResource)
BREAKER_ON_REP(CriticalChance)
BREAKER_ON_REP(CriticalMultiplier)
BREAKER_ON_REP(DamageMultiplier)
BREAKER_ON_REP(DamageOverTimeMultiplier)
BREAKER_ON_REP(MoveSpeed)
#undef BREAKER_ON_REP
