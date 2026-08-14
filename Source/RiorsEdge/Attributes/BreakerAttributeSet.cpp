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
    // Spec D8: zero for everyone. A class that wants a negative bank opens it
    // explicitly; nothing else in the game can tell the attribute is there.
    InitClassResourceFloor(0.0f);
    InitCriticalChance(0.05f);
    InitCriticalMultiplier(1.5f);
    InitDamageMultiplier(1.0f);
    InitDamageOverTimeMultiplier(1.0f);
    InitMoveSpeed(650.0f);
    // Multiplier-shaped, so 1.0 is "nothing contributed". The movement
    // component divides by DashCooldownReduction, which is why
    // PreAttributeChange gives it a hard floor rather than clamping at zero.
    InitSlideSpeedMultiplier(1.0f);
    InitAirControlMultiplier(1.0f);
    InitDashCooldownReduction(1.0f);
    InitFireRateMultiplier(1.0f);
    InitResourceCostMultiplier(1.0f);
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
    BREAKER_REPLICATE(ClassResourceFloor);
    BREAKER_REPLICATE(CriticalChance);
    BREAKER_REPLICATE(CriticalMultiplier);
    BREAKER_REPLICATE(DamageMultiplier);
    BREAKER_REPLICATE(DamageOverTimeMultiplier);
    BREAKER_REPLICATE(MoveSpeed);
    BREAKER_REPLICATE(SlideSpeedMultiplier);
    BREAKER_REPLICATE(AirControlMultiplier);
    BREAKER_REPLICATE(DashCooldownReduction);
    BREAKER_REPLICATE(FireRateMultiplier);
    BREAKER_REPLICATE(ResourceCostMultiplier);
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
    // Spec D8. The floor is 0 unless a class opened it, so for every class but
    // an Overcasting Caster this is the identical [0, Max] clamp it replaced —
    // and it is the ONE line that makes Overcast reachable, because GAS ability
    // costs are GameplayEffects and every one of them passes through here.
    else if (Attribute == GetClassResourceAttribute()) NewValue = FMath::Clamp(NewValue, FMath::Min(0.0f, GetClassResourceFloor()), GetMaxClassResource());
    else if (Attribute == GetMaxClassResourceAttribute()) NewValue = FMath::Max(0.0f, NewValue);
    // A floor is a debt allowance; a positive one would mean "the bank may
    // never be emptied", which no design asks for and which would strand a
    // spent resource above zero.
    else if (Attribute == GetClassResourceFloorAttribute()) NewValue = FMath::Min(0.0f, NewValue);
    else if (Attribute == GetCriticalChanceAttribute()) NewValue = FMath::Clamp(NewValue, 0.0f, 1.0f);
    else if (Attribute == GetCriticalMultiplierAttribute()) NewValue = FMath::Max(1.0f, NewValue);
    else if (Attribute == GetDamageMultiplierAttribute() || Attribute == GetDamageOverTimeMultiplierAttribute()) NewValue = FMath::Max(0.0f, NewValue);
    else if (Attribute == GetMoveSpeedAttribute()) NewValue = FMath::Max(0.0f, NewValue);
    else if (Attribute == GetSlideSpeedMultiplierAttribute() || Attribute == GetAirControlMultiplierAttribute()) NewValue = FMath::Max(0.0f, NewValue);
    // The movement component DIVIDES the dash cooldown by this. A floor rather
    // than a clamp at zero, because a hostile or badly-rolled -100% would
    // otherwise be a divide by zero rather than a very long cooldown.
    else if (Attribute == GetDashCooldownReductionAttribute()) NewValue = FMath::Max(0.05f, NewValue);
    // Floored well above zero: a fire rate multiplier at or near 0 would turn
    // the fire interval into an infinity and hang the weapon rather than slow it.
    else if (Attribute == GetFireRateMultiplierAttribute()) NewValue = FMath::Max(0.05f, NewValue);
    // Floored at 0.25 rather than at some epsilon: a cost driven to nearly zero
    // is not a strong build, it is the resource loop deleted. Casters have no
    // cooldowns because Mana IS the cooldown, so free casts remove the only
    // limiter the class has.
    else if (Attribute == GetResourceCostMultiplierAttribute()) NewValue = FMath::Clamp(NewValue, 0.25f, 2.0f);
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
    Values[static_cast<int32>(EBreakerAggregatedAttribute::DamageMultiplier)] = GetDamageMultiplier();
    Values[static_cast<int32>(EBreakerAggregatedAttribute::SlideSpeedMultiplier)] = GetSlideSpeedMultiplier();
    Values[static_cast<int32>(EBreakerAggregatedAttribute::AirControlMultiplier)] = GetAirControlMultiplier();
    Values[static_cast<int32>(EBreakerAggregatedAttribute::DashCooldownReduction)] = GetDashCooldownReduction();
    Values[static_cast<int32>(EBreakerAggregatedAttribute::FireRateMultiplier)] = GetFireRateMultiplier();
    Values[static_cast<int32>(EBreakerAggregatedAttribute::ResourceCostMultiplier)] = GetResourceCostMultiplier();
    Values[static_cast<int32>(EBreakerAggregatedAttribute::Armor)] = GetArmor();
    Aggregator.CaptureBases(Values);
}

void UBreakerAttributeSet::SetAggregatedAttributeBase(EBreakerAggregatedAttribute Attribute, float Value)
{
    // Capture first so a caller that arrives before equipment or progression
    // does not leave every OTHER base uncaptured — SetBase flips the captured
    // flag, and a later CaptureAttributeBases would then be a silent no-op
    // against attributes that were never snapshotted.
    CaptureAttributeBases();
    Aggregator.SetBase(Attribute, Value);
    RecomputeAggregatedAttributes();
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

void UBreakerAttributeSet::ApplyClassResourceFloor(float NewFloor)
{
    const float Floor = FMath::Min(0.0f, NewFloor);
    WriteAttributeValue(GetClassResourceFloorAttribute(), ClassResourceFloor, Floor);
    // Raising the floor — a class change away from Caster, a respec that gives
    // back the node that deepened it — must not leave the bank stranded below
    // it, unable to climb out and permanently flagged as in debt.
    if (GetClassResource() < Floor)
    {
        WriteAttributeValue(GetClassResourceAttribute(), ClassResource, Floor);
    }
}

void UBreakerAttributeSet::ApplyClassResource(float NewValue)
{
    WriteAttributeValue(GetClassResourceAttribute(), ClassResource, NewValue);
}

void UBreakerAttributeSet::ApplyHealth(float NewValue)
{
    WriteAttributeValue(GetHealthAttribute(), Health, NewValue);
}

void UBreakerAttributeSet::ApplyShield(float NewValue)
{
    WriteAttributeValue(GetShieldAttribute(), Shield, NewValue);
}

void UBreakerAttributeSet::RecomputeAggregatedAttributes()
{
    // Health and class resource ride their maximum by fraction/clamp so a
    // recalculation never silently heals or drains the character.
    const float PreviousMaxHealth = GetMaxHealth();
    const float HealthFraction = PreviousMaxHealth > 0.0f ? GetHealth() / PreviousMaxHealth : 1.0f;

    WriteAttributeValue(GetMaxHealthAttribute(), MaxHealth, Aggregator.Compose(EBreakerAggregatedAttribute::MaxHealth));
    WriteAttributeValue(GetHealthAttribute(), Health, GetMaxHealth() * HealthFraction);

    WriteAttributeValue(GetMaxClassResourceAttribute(), MaxClassResource, Aggregator.Compose(EBreakerAggregatedAttribute::MaxClassResource));
    WriteAttributeValue(GetClassResourceAttribute(), ClassResource, FMath::Min(GetClassResource(), GetMaxClassResource()));

    WriteAttributeValue(GetCriticalChanceAttribute(), CriticalChance, Aggregator.Compose(EBreakerAggregatedAttribute::CriticalChance));
    WriteAttributeValue(GetCriticalMultiplierAttribute(), CriticalMultiplier, Aggregator.Compose(EBreakerAggregatedAttribute::CriticalMultiplier));
    WriteAttributeValue(GetMoveSpeedAttribute(), MoveSpeed, Aggregator.Compose(EBreakerAggregatedAttribute::MoveSpeed));
    WriteAttributeValue(GetDamageOverTimeMultiplierAttribute(), DamageOverTimeMultiplier, Aggregator.Compose(EBreakerAggregatedAttribute::DamageOverTimeMultiplier));
    // The one number every damage path reads. It was permanently 1.0 until
    // this line existed: nothing wrote it, which is why no amount of gear or
    // skill-point spending changed how hard a weapon hit.
    WriteAttributeValue(GetDamageMultiplierAttribute(), DamageMultiplier, Aggregator.Compose(EBreakerAggregatedAttribute::DamageMultiplier));

    // The three movement multipliers the movement component now reads instead
    // of composing gear and tree together itself. Same reason as
    // DamageMultiplier above: two layers multiplied is not the locked rule.
    WriteAttributeValue(GetSlideSpeedMultiplierAttribute(), SlideSpeedMultiplier, Aggregator.Compose(EBreakerAggregatedAttribute::SlideSpeedMultiplier));
    WriteAttributeValue(GetAirControlMultiplierAttribute(), AirControlMultiplier, Aggregator.Compose(EBreakerAggregatedAttribute::AirControlMultiplier));
    WriteAttributeValue(GetDashCooldownReductionAttribute(), DashCooldownReduction, Aggregator.Compose(EBreakerAggregatedAttribute::DashCooldownReduction));

    // Flat mitigation from gear. Reaches gameplay through
    // UBreakerCombatComponent::GetEffectiveArmor() -> FBreakerDefenseState
    // ::Armor -> the mitigation formula, which is the same route the flat
    // armour strippers already use, so a stripped point of gear armour and a
    // stripped point of authored armour are the same point.
    WriteAttributeValue(GetArmorAttribute(), Armor, Aggregator.Compose(EBreakerAggregatedAttribute::Armor));
    WriteAttributeValue(GetFireRateMultiplierAttribute(), FireRateMultiplier, Aggregator.Compose(EBreakerAggregatedAttribute::FireRateMultiplier));
    WriteAttributeValue(GetResourceCostMultiplierAttribute(), ResourceCostMultiplier, Aggregator.Compose(EBreakerAggregatedAttribute::ResourceCostMultiplier));
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

void UBreakerAttributeSet::WriteAttributeValue(const FGameplayAttribute& Attribute, FGameplayAttributeData& Data, float NewValue)
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
BREAKER_ON_REP(ClassResourceFloor)
BREAKER_ON_REP(CriticalChance)
BREAKER_ON_REP(CriticalMultiplier)
BREAKER_ON_REP(DamageMultiplier)
BREAKER_ON_REP(DamageOverTimeMultiplier)
BREAKER_ON_REP(MoveSpeed)
BREAKER_ON_REP(SlideSpeedMultiplier)
BREAKER_ON_REP(AirControlMultiplier)
BREAKER_ON_REP(DashCooldownReduction)
BREAKER_ON_REP(FireRateMultiplier)
BREAKER_ON_REP(ResourceCostMultiplier)
#undef BREAKER_ON_REP
