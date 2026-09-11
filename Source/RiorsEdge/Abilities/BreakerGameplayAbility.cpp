#include "Abilities/BreakerGameplayAbility.h"
#include "UI/BreakerHUDMath.h"

#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbilityComponent.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Abilities/BreakerAbilityData.h"
#include "UObject/UnrealType.h"
#include "Abilities/BreakerAbilityTags.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"
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

void UBreakerGameplayAbility::PostInitProperties()
{
    Super::PostInitProperties();
    if (HasAnyFlags(RF_ClassDefaultObject)) return;
    // O246: native instance construction can retain zero-initialized storage
    // despite a patched CDO. Apply the same authoritative Data row to the
    // completed instance; no second table of authored magnitudes exists.
    const UBreakerAbilityDefinition* Definition = GetAbilityDefinition();
    if (!Definition) return;
    for (FNumericProperty* Property : BreakerAbilityData::NumberProperties(GetClass()))
    {
        const float* Authored = Definition->Numbers.Find(Property->GetFName());
        if (!Authored) continue;
        void* Value = Property->ContainerPtrToValuePtr<void>(this);
        if (Property->IsFloatingPoint()) Property->SetFloatingPointPropertyValue(Value, *Authored);
        else Property->SetIntPropertyValue(Value, static_cast<int64>(FMath::RoundToInt(*Authored)));
    }
}

bool UBreakerGameplayAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
    const AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
    const UBreakerCombatComponent* Combat = Avatar ? Avatar->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
    if (Combat && (Combat->IsDead() || Combat->IsStaggered())) return false;
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

FLinearColor UBreakerGameplayAbility::GetPresentationColor() const
{
    const UBreakerAbilityDefinition* Definition = GetAbilityDefinition();
    // An unsaid verb takes the resting border rather than a guess — the same
    // answer the HUD rail gives, for the same reason.
    return BreakerHUDMath::AbilityRailColor(
        Definition ? Definition->Verb : EBreakerAbilityVerb::None,
        Definition && Definition->IsUltimate());
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
    // O266: a cast paid on the keypress. The re-entry after the wind-up runs
    // the ability's own CommitAbility, and charging there would take the price
    // twice. One claim per cast, released when the cast resolves or is
    // interrupted.
    if (bCastCommitted) return true;
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
    // O266: the COMPOSED attribute when one exists, so gear and tree share one
    // additive bucket; node stats remain the fallback for a rig with no
    // attribute set, which is most of the pure-maths fixtures.
    const auto* Attributes = OwnerActor ? OwnerActor->FindComponentByClass<UAbilitySystemComponent>() : nullptr;
    const UBreakerAttributeSet* Set = Attributes ? Attributes->GetSet<UBreakerAttributeSet>() : nullptr;
    float Rate = Set ? Set->GetAbilityCastRate()
        : (Progression ? Progression->GetNodeStats().AbilityCastRateMultiplier : 1.0f);
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
    // Composed attribute first: gear and tree in one additive bucket.
    const auto* AreaASC = OwnerActor ? OwnerActor->FindComponentByClass<UAbilitySystemComponent>() : nullptr;
    if (const UBreakerAttributeSet* Set = AreaASC ? AreaASC->GetSet<UBreakerAttributeSet>() : nullptr) return Set->GetAbilityArea();
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
    const auto* CdASC = OwnerActor ? OwnerActor->FindComponentByClass<UAbilitySystemComponent>() : nullptr;
    if (const UBreakerAttributeSet* Set = CdASC ? CdASC->GetSet<UBreakerAttributeSet>() : nullptr)
        return FMath::Max(0.01f, Set->GetAbilityCooldownReduction());
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

// ---------------------------------------------------------------------------
// THE CAST (O266). Spells no longer appear on the keypress.
// ---------------------------------------------------------------------------

float UBreakerGameplayAbility::EffectiveCastSeconds(float AuthoredSeconds, float CastSpeedMultiplier)
{
    // The DIVISOR convention every rate lane in this project already uses
    // (DashCooldownReduction's): a multiplier of 1.25 is a 20% shorter cast,
    // never a 25% longer one. A malformed multiplier is floored rather than
    // dividing by zero, and a negative authored time is not a negative cast.
    const float Authored = FMath::Max(0.0f, AuthoredSeconds);
    if (Authored <= 0.0f) return 0.0f;
    return Authored / FMath::Max(0.01f, CastSpeedMultiplier);
}

FName UBreakerGameplayAbility::CastWindowKey(FName AbilityId)
{
    // "Window." is the prefix the HUD's ability-window bar already filters on,
    // so a pending cast draws itself with no HUD change.
    return FName(*FString::Printf(TEXT("Window.Cast.%s"), *AbilityId.ToString()));
}

bool UBreakerGameplayAbility::BeginCastIfNeeded(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
    // The re-entry after a finished wind-up: let the body run.
    if (bCastPending) { bCastPending = false; return true; }

    const UBreakerAbilityDefinition* Definition = GetAbilityDefinition();
    ABreakerCharacter* Character = GetBreakerCharacter();
    UWorld* World = Character ? Character->GetWorld() : nullptr;
    // CAST SPEED IS NOT AUTHORED YET, and this reads 1.0 deliberately rather
    // than inventing a lane: O266's second half needs a canon row in
    // power-and-scaling.md and a conformance test before any node or affix may
    // bid into it. The seam is here so that lands as one number, not a rewrite.
    // O266's second half: the wind-up is divided by the owner's composed cast
    // rate. THE LANE ALREADY EXISTED — AbilityCastRate, authored by Core
    // Tempo's Metronome, Quicken and Cascade and already dividing Fracture's
    // own cast phase — so a generic wind-up plugs into it rather than
    // inventing a second cast-speed concept beside it.
    const float Seconds = Definition
        ? EffectiveCastSeconds(Definition->GetCastTimeSeconds(), AbilityCastRateMultiplierFor(Character)) : 0.0f;
    if (Seconds <= 0.0f || !World) return true;

    // THE ABILITY'S OWN REFUSAL, BEFORE THE PRICE. Every "this press would do
    // nothing" guard an ability owns used to sit in its body, which a wind-up
    // moves to the far side of the payment. Asking here keeps a dead key free.
    if (!PrepareCast())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return false;
    }

    // O266: the price goes on the KEYPRESS. A refused commit is a refused
    // cast — no window, no timer, and the ability ends the way it always did.
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return false;
    }
    bCastCommitted = true;
    bCastPending = true;
    CastHandle = Handle;
    CastActivationInfo = ActivationInfo;

    if (UBreakerAbilityStateComponent* State = UBreakerAbilityStateComponent::FindOrAdd(Character))
    {
        State->StartWindow(CastWindowKey(Definition->AbilityId), Seconds);
    }
    // Whatever this ability must start at the cast rather than at the landing.
    // After the window opens, so an override can read its own cast window.
    OnCastBegan();
    // Damage interrupts (O266). Bound per cast and released with it, so an
    // ability that is not casting pays nothing for the rule.
    if (UBreakerCombatComponent* Combat = Character->FindComponentByClass<UBreakerCombatComponent>())
    {
        Combat->OnDamageReceived.AddDynamic(this, &UBreakerGameplayAbility::HandleCastInterrupt);
    }

    FTimerDelegate Resolve;
    Resolve.BindWeakLambda(this, [this, Handle, ActorInfo, ActivationInfo]()
    {
        EndCastBinding();
        // bCastPending is still true, so this re-entry passes the gate above
        // and the ability's own body finally runs.
        ActivateAbility(Handle, ActorInfo, ActivationInfo, nullptr);
        bCastCommitted = false;
        // O178: THE CUE FIRES AT THE LANDING. TryActivateSlot withheld its
        // OnAbilityActivated because this ability was still casting when the
        // press returned; the resolution is the moment the ability exists, so
        // this is where the slot is announced. The interrupt and cancel paths
        // clear the timer and never reach here, which is the point.
        ABreakerCharacter* Character = GetBreakerCharacter();
        if (UBreakerAbilityComponent* Abilities = Character ? Character->GetAbilities() : nullptr)
        {
            Abilities->NotifyCastResolved(Handle);
        }
    });
    World->GetTimerManager().SetTimer(CastTimer, Resolve, Seconds, false);
    return false;
}

void UBreakerGameplayAbility::HandleCastInterrupt(const FBreakerDamageResult& Result)
{
    if (!bCastPending) return;
    // A dodged or fully-parried hit is not damage taken, so it is not an
    // interrupt: the rule is "taking damage", and reading the result rather
    // than the attempt is what keeps a whiff from cancelling a cast.
    if (Result.bDodged || Result.bParried) return;
    if (Result.HealthDamage + Result.ShieldDamage <= 0.0f) return;

    ABreakerCharacter* Character = GetBreakerCharacter();
    UWorld* World = Character ? Character->GetWorld() : nullptr;
    if (World) World->GetTimerManager().ClearTimer(CastTimer);
    const UBreakerAbilityDefinition* Definition = GetAbilityDefinition();
    if (Definition)
    {
        if (UBreakerAbilityStateComponent* State = UBreakerAbilityStateComponent::FindOrAdd(Character))
        {
            State->CloseWindow(CastWindowKey(Definition->AbilityId));
        }
    }
    EndCastBinding();
    bCastPending = false;
    // NO REFUND, owner-ruled. The Mana went on the keypress and the wind-up is
    // the risk that buys it back; handing it over on an interrupt would make
    // casting free to attempt.
    bCastCommitted = false;
    EndAbility(CastHandle, CurrentActorInfo, CastActivationInfo, true, true);
}

void UBreakerGameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    // Only while a cast is actually pending: an ordinary end — the body ran
    // and finished — has an already-fired timer and a closed window, so this
    // costs nothing and changes nothing on the path every ability takes.
    if (bCastPending)
    {
        ABreakerCharacter* Character = GetBreakerCharacter();
        if (UWorld* World = Character ? Character->GetWorld() : nullptr)
        {
            World->GetTimerManager().ClearTimer(CastTimer);
        }
        if (const UBreakerAbilityDefinition* Definition = GetAbilityDefinition())
        {
            if (UBreakerAbilityStateComponent* State = Character
                ? Character->FindComponentByClass<UBreakerAbilityStateComponent>() : nullptr)
            {
                State->CloseWindow(CastWindowKey(Definition->AbilityId));
            }
        }
        EndCastBinding();
        bCastPending = false;
        // NO REFUND, the same rule the damage interrupt states: the Mana went
        // on the keypress and the wind-up is the risk that buys it back.
        bCastCommitted = false;
    }
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UBreakerGameplayAbility::EndCastBinding()
{
    ABreakerCharacter* Character = GetBreakerCharacter();
    if (UBreakerCombatComponent* Combat = Character ? Character->FindComponentByClass<UBreakerCombatComponent>() : nullptr)
    {
        Combat->OnDamageReceived.RemoveDynamic(this, &UBreakerGameplayAbility::HandleCastInterrupt);
    }
}
