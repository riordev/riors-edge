#include "Attributes/BreakerAttributeSet.h"

#include "AbilitySystemGlobals.h"
#include "GameFramework/Actor.h"
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

void UBreakerAttributeSet::CaptureAttributeBases()
{
    // Read straight off the attribute data: whichever component asks first
    // gets the authored values, before any contribution has been applied.
    float Values[FBreakerAttributeAggregator::AttributeCount] = {};
    Values[static_cast<int32>(EBreakerAggregatedAttribute::MaxHealth)] = GetMaxHealth();
    Values[static_cast<int32>(EBreakerAggregatedAttribute::MaxClassResource)] = GetMaxClassResource();
    Values[static_cast<int32>(EBreakerAggregatedAttribute::CriticalChance)] = GetCriticalChance();
    Values[static_cast<int32>(EBreakerAggregatedAttribute::CriticalMultiplier)] = GetCriticalMultiplier();
    Values[static_cast<int32>(EBreakerAggregatedAttribute::MoveSpeed)] = GetMoveSpeed();
    Values[static_cast<int32>(EBreakerAggregatedAttribute::DamageOverTimeMultiplier)] = GetDamageOverTimeMultiplier();
    Aggregator.CaptureBases(Values);
}

void UBreakerAttributeSet::ApplyAttributeContribution(EBreakerAttributeContributor Contributor, const FBreakerAttributeContribution& Contribution)
{
    // A contributor that submits before anything captured is itself the
    // trigger: capture first so its own bonus is never folded into the base.
    CaptureAttributeBases();
    Aggregator.SetContribution(Contributor, Contribution);
    RecomputeAggregatedAttributes();
}

void UBreakerAttributeSet::ClearAttributeContribution(EBreakerAttributeContributor Contributor)
{
    Aggregator.ClearContribution(Contributor);
    if (Aggregator.HasCapturedBases()) RecomputeAggregatedAttributes();
}

void UBreakerAttributeSet::RecomputeAggregatedAttributes()
{
    // Health and class resource ride their maximum by fraction/clamp so a
    // recalculation never silently heals or drains the character.
    const float PreviousMaxHealth = GetMaxHealth();
    const float HealthFraction = PreviousMaxHealth > 0.0f ? GetHealth() / PreviousMaxHealth : 1.0f;

    WriteAggregatedAttribute(GetMaxHealthAttribute(), MaxHealth, Aggregator.Compose(EBreakerAggregatedAttribute::MaxHealth));
    WriteAggregatedAttribute(GetHealthAttribute(), Health, GetMaxHealth() * HealthFraction);

    WriteAggregatedAttribute(GetMaxClassResourceAttribute(), MaxClassResource, Aggregator.Compose(EBreakerAggregatedAttribute::MaxClassResource));
    WriteAggregatedAttribute(GetClassResourceAttribute(), ClassResource, FMath::Min(GetClassResource(), GetMaxClassResource()));

    WriteAggregatedAttribute(GetCriticalChanceAttribute(), CriticalChance, Aggregator.Compose(EBreakerAggregatedAttribute::CriticalChance));
    WriteAggregatedAttribute(GetCriticalMultiplierAttribute(), CriticalMultiplier, Aggregator.Compose(EBreakerAggregatedAttribute::CriticalMultiplier));
    WriteAggregatedAttribute(GetMoveSpeedAttribute(), MoveSpeed, Aggregator.Compose(EBreakerAggregatedAttribute::MoveSpeed));
    WriteAggregatedAttribute(GetDamageOverTimeMultiplierAttribute(), DamageOverTimeMultiplier, Aggregator.Compose(EBreakerAggregatedAttribute::DamageOverTimeMultiplier));
}

UAbilitySystemComponent* UBreakerAttributeSet::FindOwningAbilitySystemSafe() const
{
    // UAttributeSet::GetOwningAbilitySystemComponent() CastChecked's the outer
    // to an AActor, which is fatal for a standalone attribute set (a test, a
    // tool). Resolve it defensively instead; in game the outer is the actor.
    if (const AActor* OwningActor = Cast<AActor>(GetOuter()))
    {
        return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwningActor);
    }
    return nullptr;
}

void UBreakerAttributeSet::WriteAggregatedAttribute(const FGameplayAttribute& Attribute, FGameplayAttributeData& Data, float NewValue)
{
    if (UAbilitySystemComponent* OwningAbilitySystem = FindOwningAbilitySystemSafe())
    {
        OwningAbilitySystem->SetNumericAttributeBase(Attribute, NewValue);
        return;
    }
    // No ability system (a standalone attribute set in a test): the generated
    // setters would ensure, so write the data with the same clamp policy the
    // ability system path goes through.
    float ClampedValue = NewValue;
    PreAttributeChange(Attribute, ClampedValue);
    Data.SetBaseValue(ClampedValue);
    Data.SetCurrentValue(ClampedValue);
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
