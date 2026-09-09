#include "Abilities/BreakerGameplayAbility.h"

#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbilityComponent.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Abilities/BreakerAbilityTags.h"
#include "Abilities/BreakerSkillLevelMath.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Weapons/BreakerWeaponComponent.h"

UBreakerAbilityCostEffect::UBreakerAbilityCostEffect()
{
    DurationPolicy = EGameplayEffectDurationType::Instant;

    FGameplayModifierInfo Modifier;
    Modifier.Attribute = UBreakerAttributeSet::GetClassResourceAttribute();
    Modifier.ModifierOp = EGameplayModOp::Additive;
    FSetByCallerFloat CostMagnitude;
    CostMagnitude.DataTag = BreakerAbilityTags::Data_AbilityCost.GetTag();
    Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(CostMagnitude);
    Modifiers.Add(Modifier);
}

UBreakerAbilityCooldownEffect::UBreakerAbilityCooldownEffect()
{
    DurationPolicy = EGameplayEffectDurationType::HasDuration;
    FSetByCallerFloat DurationSetByCaller;
    DurationSetByCaller.DataTag = BreakerAbilityTags::Data_AbilityCooldown.GetTag();
    DurationMagnitude = FGameplayEffectModifierMagnitude(DurationSetByCaller);
}

UBreakerGameplayAbility::UBreakerGameplayAbility()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    // Spec D5: window/impulse abilities predict locally; anything that spawns
    // an actor or mutates another pawn overrides this to ServerOnly.
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
    CostGameplayEffectClass = UBreakerAbilityCostEffect::StaticClass();
    CooldownGameplayEffectClass = UBreakerAbilityCooldownEffect::StaticClass();
}

bool UBreakerGameplayAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
    const AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
    const UBreakerCombatComponent* Combat = Avatar ? Avatar->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
    if (Combat && Combat->IsStaggered()) return false;
    const auto* Character = ActorInfo ? Cast<ABreakerCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
    if (Character && Character->GetAbilities()->IsAbilityCommitInProgress()) return false;
    return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

const UBreakerAbilityDefinition* UBreakerGameplayAbility::GetAbilityDefinition() const
{
    if (AbilityDefinition)
    {
        return AbilityDefinition;
    }
    return UBreakerAbilityDefinition::FindFallback(FallbackAbilityId);
}

float UBreakerGameplayAbility::GetResourceCost() const
{
    if (bCostSnapshotActive) return CommitCostSnapshot;
    const auto* Character = GetBreakerCharacter();
    const auto* Abilities = Character ? Character->GetAbilities() : nullptr;
    return GetUnmodifiedResourceCost() * (GetAbilityDefinition() && Abilities ? Abilities->GetConductionCostMultiplier() : 1.0f);
}

bool UBreakerGameplayAbility::CommitAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, FGameplayTagContainer* OptionalRelevantTags)
{
    auto* Character = GetBreakerCharacter();
    auto* Abilities = Character ? Character->GetAbilities() : nullptr;
    if (Abilities && !Abilities->BeginAbilityCommit()) return false;
    CommitCostSnapshot = GetResourceCost();
    bCostSnapshotActive = true;
    const bool Committed = Super::CommitAbility(Handle, ActorInfo, ActivationInfo, OptionalRelevantTags);
    bCostSnapshotActive = false;
    if (Abilities) Abilities->EndAbilityCommit();
    return Committed;
}

void UBreakerGameplayAbility::CommitExecute(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo)
{
    LastPaidResourceCost = GetResourceCost();
    Super::CommitExecute(Handle, ActorInfo, ActivationInfo);
    if (GetAbilityDefinition())
        if (auto* Character = GetBreakerCharacter()) Character->GetAbilities()->RecordConductionCast();
}

bool UBreakerGameplayAbility::CheckCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    FGameplayTagContainer* OptionalRelevantTags) const
{
    const auto* Character = GetBreakerCharacter();
    return (GetAbilityDefinition() && Character && Character->GetAbilities()->IsConductionActive())
        || Super::CheckCooldown(Handle, ActorInfo, OptionalRelevantTags);
}
float UBreakerGameplayAbility::GetAuthoredResourceCost() const
{
    const UBreakerAbilityDefinition* Definition = GetAbilityDefinition();
    return Definition ? Definition->ResourceCost : 0.0f;
}

float UBreakerGameplayAbility::GetUnmodifiedResourceCost() const
{
    const float Authored = GetAuthoredResourceCost();
    if (Authored <= 0.0f) return 0.0f;
    const UBreakerAttributeSet* Attributes = GetBreakerAttributes();
    const float Multiplier = Attributes ? Attributes->GetResourceCostMultiplier() : 1.0f;
    // Live gear/node efficiency precedes class-specific windows and price rewrites.
    // Attributes enforce the ordinary .25 floor; retain Caster's defensive .10 floor.
    return Authored * (FMath::IsFinite(Multiplier) ? FMath::Max(0.10f, Multiplier) : 1.0f);
}

float UBreakerGameplayAbility::GetCooldownSeconds() const
{
    const UBreakerAbilityDefinition* Definition = GetAbilityDefinition();
    if (!Definition) return 0.0f;
    const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
    const AActor* Avatar = Info ? Info->AvatarActor.Get() : nullptr;
    const auto* Character = Cast<ABreakerCharacter>(Avatar);
    if (Character && Character->GetAbilities()->IsConductionActive()) return 0.0f;
    return ScaledCooldownSeconds(Definition->CooldownSeconds, AbilityCooldownReductionFor(Avatar));
}

float UBreakerGameplayAbility::ScaledCooldownSeconds(float AuthoredSeconds, float ReductionDivisor)
{
    // A non-positive authored cooldown means "no cooldown at all" (Caster's
    // whole class, T8) and stays exactly that — dividing it would be inventing
    // a mechanic the definition deliberately does not author.
    if (AuthoredSeconds <= 0.0f) return AuthoredSeconds;
    return AuthoredSeconds / FMath::Max(0.01f, ReductionDivisor);
}

float UBreakerGameplayAbility::AbilityCastRateMultiplierFor(const AActor* OwnerActor)
{
    const auto* Progression = OwnerActor ? OwnerActor->FindComponentByClass<UBreakerProgressionComponent>() : nullptr;
    float Rate = Progression ? Progression->GetNodeStats().AbilityCastRateMultiplier : 1.0f;
    if (Progression && Progression->GetNodeStats().bCooldownRecoveryAffectsTempo)
    {
        const float Recovery = Progression->GetNodeStats().AbilityCooldownReduction;
        if (FMath::IsFinite(Recovery)) Rate += 0.5f * FMath::Max(0.0f, Recovery - 1.0f);
    }
    return FMath::IsFinite(Rate) ? FMath::Max(1.0f, Rate) : 1.0f;
}

float UBreakerGameplayAbility::AbilityChannelRateMultiplierFor(const AActor* OwnerActor)
{
    const auto* Progression = OwnerActor ? OwnerActor->FindComponentByClass<UBreakerProgressionComponent>() : nullptr;
    float Rate = Progression ? Progression->GetNodeStats().AbilityChannelRateMultiplier : 1.0f;
    if (Progression && Progression->GetNodeStats().bCooldownRecoveryAffectsTempo)
    {
        const float Recovery = Progression->GetNodeStats().AbilityCooldownReduction;
        if (FMath::IsFinite(Recovery)) Rate += 0.5f * FMath::Max(0.0f, Recovery - 1.0f);
    }
    return FMath::IsFinite(Rate) ? FMath::Max(1.0f, Rate) : 1.0f;
}
float UBreakerGameplayAbility::AbilityAreaMultiplierFor(const AActor* OwnerActor)
{
    const UBreakerProgressionComponent* Progression = OwnerActor ? OwnerActor->FindComponentByClass<UBreakerProgressionComponent>() : nullptr;
    return Progression ? Progression->GetNodeStats().AbilityAreaMultiplier : 1.0f;
}

float UBreakerGameplayAbility::AbilityDurationMultiplierFor(const AActor* OwnerActor, EBreakerAbilityDurationKind Kind)
{
    const UBreakerProgressionComponent* Progression = OwnerActor ? OwnerActor->FindComponentByClass<UBreakerProgressionComponent>() : nullptr;
    if (!Progression) return 1.0f;
    return ComposeAbilityDurationMultiplier(Progression->GetNodeStats(), Kind);
}

float UBreakerGameplayAbility::ComposeAbilityDurationMultiplier(const FBreakerNodeStats& Stats, EBreakerAbilityDurationKind Kind)
{
    float Percent = Stats.AbilityDurationPercent;
    if (Kind == EBreakerAbilityDurationKind::Zone || Kind == EBreakerAbilityDurationKind::Window)
        Percent += Stats.ZoneAndWindowDurationPercent;
    if (Kind == EBreakerAbilityDurationKind::Buff || Kind == EBreakerAbilityDurationKind::Window)
        Percent += Stats.BuffAndWindowDurationPercent;
    return FMath::Max(0.0f, 1.0f + Percent * .01f);
}

int32 UBreakerGameplayAbility::SkillLevelFor(const AActor* OwnerActor)
{
    const auto* Progression = OwnerActor ? OwnerActor->FindComponentByClass<UBreakerProgressionComponent>() : nullptr;
    if (!Progression) return BreakerSkillLevel::MinLevel;
    const int32 Earned = BreakerSkillLevel::ForCharacterLevel(
        Progression->GetCharacterLevel(), UBreakerExperienceLibrary::MaxCharacterLevel);
    // O253: gear adds ABOVE the earned ceiling. Summed as a whole number the
    // equipment layer already floored, and clamped to the composed ceiling
    // rather than the earned one — clamping to 15 here is exactly what would
    // make the affix's endgame tiers, where every character is already at 15,
    // pay nothing at all.
    const auto* Equipment = OwnerActor->FindComponentByClass<UBreakerEquipmentComponent>();
    const int32 FromGear = Equipment ? FMath::Max(0, Equipment->GetStats().BonusSkillLevels) : 0;
    return FMath::Clamp(Earned + FromGear, BreakerSkillLevel::MinLevel, BreakerSkillLevel::MaxComposedLevel);
}

float UBreakerGameplayAbility::AbilityBaseDamageFor(const AActor* OwnerActor, float ScaledAuthoredBase)
{
    if (!FMath::IsFinite(ScaledAuthoredBase) || ScaledAuthoredBase <= 0.0f) return 0.0f;
    const auto* Progression = OwnerActor ? OwnerActor->FindComponentByClass<UBreakerProgressionComponent>() : nullptr;
    const float Added = Progression ? Progression->GetNodeStats().AddedAbilityPower : 0.0f;
    // O252: the skill level scales the ABILITY'S OWN base and nothing else.
    // Gear's Added Ability Power lands after it, so a skill level never
    // multiplies what the player bolted on — the same partition the weapon
    // lane keeps between a gun's base and Added Damage.
    //
    // READ LIVE, not from the granted spec's level. GAS carries an ability
    // level and the grant site now sets it honestly, but re-granting on every
    // level-up to keep a spec current would reset ability state for a number
    // that is a pure function of character level anyway. This is the source of
    // truth; the spec's level is for inspection.
    const float Scaled = ScaledAuthoredBase * BreakerSkillLevel::DamageMultiplier(SkillLevelFor(OwnerActor));
    return FMath::Max(0.0f, Scaled + (FMath::IsFinite(Added) ? Added : 0.0f));
}

float UBreakerGameplayAbility::AbilityCooldownReductionFor(const AActor* OwnerActor)
{
    const UBreakerProgressionComponent* Progression = OwnerActor ? OwnerActor->FindComponentByClass<UBreakerProgressionComponent>() : nullptr;
    return Progression ? FMath::Max(0.01f, Progression->GetNodeStats().AbilityCooldownReduction) : 1.0f;
}

float UBreakerGameplayAbility::GetAbilityAreaMultiplier() const
{
    const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
    return AbilityAreaMultiplierFor(Info ? Info->AvatarActor.Get() : nullptr);
}

float UBreakerGameplayAbility::GetAbilityDurationMultiplier() const
{
    const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
    return AbilityDurationMultiplierFor(Info ? Info->AvatarActor.Get() : nullptr);
}

float UBreakerGameplayAbility::AbilityDamageScalarFor(const AActor* OwnerActor)
{
    // The weapon component owns both halves of the reading: the equipped item
    // level (unequipped = 1) and the growth `w`. Asking it, rather than
    // recomputing here, is what keeps an editor retune of ItemLevelDamageGrowth
    // reaching abilities and weapon rounds in the same frame.
    const UBreakerWeaponComponent* Weapon = OwnerActor ? OwnerActor->FindComponentByClass<UBreakerWeaponComponent>() : nullptr;
    return Weapon ? Weapon->GetItemLevelDamageScalar() : 1.0f;
}

ABreakerCharacter* UBreakerGameplayAbility::GetBreakerCharacter() const
{
    const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
    return Info ? Cast<ABreakerCharacter>(Info->AvatarActor.Get()) : nullptr;
}

UBreakerAttributeSet* UBreakerGameplayAbility::GetBreakerAttributes() const
{
    const ABreakerCharacter* Character = GetBreakerCharacter();
    return Character ? Character->GetAttributes() : nullptr;
}

float UBreakerGameplayAbility::GetCurrentClassResource() const
{
    const UBreakerAttributeSet* Attributes = GetBreakerAttributes();
    return Attributes ? Attributes->GetClassResource() : 0.0f;
}

float UBreakerGameplayAbility::GetCurrentClassResourceFloor() const
{
    const UBreakerAttributeSet* Attributes = GetBreakerAttributes();
    return Attributes ? FMath::Min(0.0f, Attributes->GetClassResourceFloor()) : 0.0f;
}

bool UBreakerGameplayAbility::IsAffordable(float CurrentResource, float Cost)
{
    return IsAffordableWithFloor(CurrentResource, Cost, 0.0f);
}

bool UBreakerGameplayAbility::IsAffordableWithFloor(float CurrentResource, float Cost, float Floor)
{
    if (Cost <= 0.0f) return true;
    const float DebtAllowance = FMath::Min(0.0f, Floor);
    // The closed floor case is written as the original expression rather than
    // as "0 - Cost >= 0" so that every existing class keeps bit-identical
    // affordability, float edges included.
    if (DebtAllowance == 0.0f) return CurrentResource >= Cost;
    // Class-Kits §2.1: "No further ability may be cast until Mana is at or
    // above zero."
    if (CurrentResource < 0.0f) return false;
    return CurrentResource - Cost >= DebtAllowance - KINDA_SMALL_NUMBER;
}

bool UBreakerGameplayAbility::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
    const float Cost = GetResourceCost();
    if (Cost <= 0.0f)
    {
        return true;
    }
    const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    if (!ASC)
    {
        return false;
    }
    bool bFound = false;
    const float Current = ASC->GetGameplayAttributeValue(UBreakerAttributeSet::GetClassResourceAttribute(), bFound);
    return bFound && IsAffordable(Current, Cost);
}

void UBreakerGameplayAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
    const float Cost = GetResourceCost();
    if (Cost <= 0.0f || !CostGameplayEffectClass)
    {
        return;
    }
    FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(CostGameplayEffectClass, GetAbilityLevel());
    if (!Spec.IsValid())
    {
        return;
    }
    Spec.Data->SetSetByCallerMagnitude(BreakerAbilityTags::Data_AbilityCost.GetTag(), -Cost);
    ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
}

void UBreakerGameplayAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
    const UBreakerAbilityDefinition* Definition = GetAbilityDefinition();
    if (GetCooldownSeconds() <= 0.0f) return;
    // Cost-gated abilities author no cooldown effect at all, so the HUD can
    // tell "no cooldown" from "cooldown of zero" (spec D3).
    if (!Definition || Definition->CooldownSeconds <= 0.0f || !Definition->CooldownTag.IsValid() || !CooldownGameplayEffectClass)
    {
        return;
    }
    FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(CooldownGameplayEffectClass, GetAbilityLevel());
    if (!Spec.IsValid())
    {
        return;
    }
    // GetCooldownSeconds, not the raw definition number: the AbilityCooldown
    // lane's composed reduction divides the authored seconds here, the way
    // DashCooldownReduction's divisor convention works. The guard above still
    // reads the RAW authored value, so "authors no cooldown" (every Caster
    // ability, T8) never becomes a cooldown of any length under any divisor.
    Spec.Data->SetSetByCallerMagnitude(BreakerAbilityTags::Data_AbilityCooldown.GetTag(), GetCooldownSeconds());
    Spec.Data->DynamicGrantedTags.AddTag(Definition->CooldownTag);
    ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
}

const FGameplayTagContainer* UBreakerGameplayAbility::GetCooldownTags() const
{
    CachedCooldownTags.Reset();
    if (const UBreakerAbilityDefinition* Definition = GetAbilityDefinition())
    {
        if (Definition->CooldownTag.IsValid() && Definition->CooldownSeconds > 0.0f)
        {
            CachedCooldownTags.AddTag(Definition->CooldownTag);
        }
    }
    return CachedCooldownTags.IsEmpty() ? nullptr : &CachedCooldownTags;
}
