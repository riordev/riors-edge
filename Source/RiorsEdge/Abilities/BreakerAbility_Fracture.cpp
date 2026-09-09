#include "Abilities/BreakerAbility_Fracture.h"
#include "Combat/BreakerStatusComponent.h"

#include "Abilities/BreakerAbilityTags.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Combat/BreakerElementSharesMath.h"
#include "Combat/BreakerProjectileBase.h"
#include "Combat/BreakerStatusCycleComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/Controller.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "UI/BreakerEffectMath.h"

UBreakerAbility_Fracture::UBreakerAbility_Fracture()
{
    FallbackAbilityId = TEXT("Caster.Fracture");
    // Spec §5.5: it spawns an actor, so it never runs on a client.
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
    bRetriggerInstancedAbility = false;
    ProjectileClass = ABreakerProjectileBase::StaticClass();

    FGameplayTagContainer Tags;
    Tags.AddTag(BreakerAbilityTags::Ability_Class_Caster_Fracture.GetTag());
    SetAssetTags(Tags);
}

void UBreakerAbility_Fracture::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    // O266: the wind-up. Returns false when it has started a cast — the cost
    // is already paid, the window is open, and this function is called again
    // when the wind-up completes. Zero authored cast time is a no-op.
    if (!BeginCastIfNeeded(Handle, ActorInfo, ActivationInfo)) return;
    ABreakerCharacter* Character = GetBreakerCharacter();
    UWorld* World = Character ? Character->GetWorld() : nullptr;
    UBreakerStatusCycleComponent* Cycle = UBreakerStatusCycleComponent::FindOrAdd(Character);
    // An empty cycle means there is no status to apply, and a Fracture that
    // applies nothing is a 30-Mana bullet. Refuse before the commit, the same
    // rule Closequarter uses for a cast with no target.
    if (!World || !Cycle || Cycle->GetCycleLength() <= 0 || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    FVector ViewLocation = Character->GetActorLocation();
    FRotator ViewRotation = Character->GetControlRotation();
    if (const AController* Controller = Character->GetController())
    {
        Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
    }
    const FVector Direction = ViewRotation.Vector().GetSafeNormal();
    const FVector Muzzle = ViewLocation + Direction * MuzzleForwardCm;

    const UBreakerAttributeSet* SourceAttributes = GetBreakerAttributes();
    UBreakerCombatComponent* OwnerCombat = Character->FindComponentByClass<UBreakerCombatComponent>();

    FBreakerDamageRequest Damage;
    // O35: flat ability damage rides the equipped weapon's item-level scalar
    // (exactly 1.0 at item level 1, preserving the authored base hit there).
    Damage.BaseDamage = AbilityBaseDamageFor(Character, ImpactDamage * AbilityDamageScalarFor(Character));
    Damage.DamageFamily = EBreakerDamageFamily::Physical;
    Damage.SourceTags.AddTag(BreakerAbilityTags::Ability_Class_Caster_Fracture.GetTag());
    Damage.CriticalChance = SourceAttributes ? SourceAttributes->GetCriticalChance() : UBreakerAttributeSet::DefaultCriticalChance;
    Damage.CriticalMultiplier = SourceAttributes ? SourceAttributes->GetCriticalMultiplier() : UBreakerAttributeSet::DefaultCriticalMultiplier;
    UBreakerDamageLibrary::FillSourcePools(SourceAttributes, EBreakerDamageDelivery::Ability, Damage);
    Damage.RandomSeed = HashCombine(GetTypeHash(Character), static_cast<uint32>(World->GetTimeSeconds() * 1000.0));
    Damage.SetInstigator(Character);
    // Route through the outgoing chain like every other damage submission
    // (Cleave, Siphon, Resonance, both weapon paths). The projectile copies the
    // request verbatim at impact, so modifiers active at the moment of CASTING
    // are the ones that count — the rocket's own rule. Without this, windows
    // like Overdrive simply never applied to Fracture.
    if (OwnerCombat)
    {
        OwnerCombat->ApplyOutgoingModifiers(Damage);
    }

    const UBreakerProgressionComponent* Progression = Character->GetProgression();
    const bool bAdvanceOnHit = Cycle->GetAdvanceOnHit()
        || (Progression && Progression->HasNodeTag(BreakerNodeTags::Node_MS_Cycle.GetTag()));
    const bool bDoublePosition = Progression && Progression->HasNodeTag(BreakerNodeTags::Node_MS_Fracture.GetTag());
    const int32 Positions = FMath::Clamp(bDoublePosition ? FMath::Max(2, CyclePositionsPerCast) : CyclePositionsPerCast,
        1, FMath::Max(1, Cycle->GetCycleLength()));
    PendingStatuses.Reset(); bHasPendingColor = false;
    for (int32 Index = 0; Index < Positions; ++Index)
    {
        FBreakerCycleEntry Entry = Cycle->PeekNextEntry(Index);

        if (!Entry.Spec.StatusTag.IsValid()) continue;
        if (Entry.Element != EBreakerElement::None)
        {
            // One actual impact carries conversion, even when several cycle
            // positions are selected. Never grant a threshold status directly.
            FBreakerElementShare Share;
            Share.Element = Entry.Element;
            Share.Fraction = 1.0f;
            Damage.ElementShares.Add(Share);
            if (Index == 0)
            {
                PendingColor = BreakerFX::ColorForStatusTag(Entry.Spec.StatusTag, GetDefault<ABreakerProjectileBase>()->OrbColor);
                bHasPendingColor = true;
            }
            continue;
        }

        // O35: the cycle's authored per-tick numbers are item-level-1 values;
        // the applied copy rides the weapon scalar exactly as the impact hit
        // does. Scaled on the COPY, never on the cycle's own entry, so the
        // authored cycle survives an equipment change untouched.
        Entry.Spec.BaseDamagePerTick = AbilityBaseDamageFor(Character,
            Entry.Spec.BaseDamagePerTick * AbilityDamageScalarFor(Character));

        // Snapshot the caster's offensive stats onto the status HERE, at cast,
        // not at impact: the DoT contract snapshots at application and this is
        // the moment the player paid for. One critical roll per position
        // decides every tick of that application. The snapshot includes the
        // outgoing chain's budgeted window product — a status cast inside a
        // damage window carries it for life, one cast outside never gains it.
        Entry.Spec.Snapshot.SourcePower = UBreakerCombatComponent::ComposeDotSourcePower(SourceAttributes, OwnerCombat,
            EBreakerDamageDelivery::Ability);
        Entry.Spec.Snapshot.CriticalChance = SourceAttributes ? SourceAttributes->GetCriticalChance() : UBreakerAttributeSet::DefaultCriticalChance;
        Entry.Spec.Snapshot.CriticalMultiplier = SourceAttributes ? SourceAttributes->GetCriticalMultiplier() : UBreakerAttributeSet::DefaultCriticalMultiplier;
        Entry.Spec.Snapshot.DamageOverTimeMultiplier = SourceAttributes ? SourceAttributes->GetDamageOverTimeMultiplier() : 1.0f;
        FRandomStream Stream(static_cast<int32>(HashCombine(Damage.RandomSeed, static_cast<uint32>(Index))));
        Entry.Spec.Snapshot.CriticalRollSample = Stream.FRand();
    Entry.Spec.Snapshot.bHasCriticalRollSample = true;
    Entry.Spec.Snapshot.bRolledCritical = Entry.Spec.Snapshot.CriticalRollSample < Entry.Spec.Snapshot.CriticalChance;

        UBreakerStatusComponent::SnapshotAilmentRules(Entry.Spec, Entry.DamageFamily, Character);
        FBreakerCarriedStatus Carried;
        Carried.Spec = Entry.Spec;
        Carried.DamageFamily = Entry.DamageFamily;
        PendingStatuses.Add(Carried);

        // The round is TINTED by the status it will apply — the cycle is
        // Fracture's whole identity and an untinted orb hides which position
        // this cast is on. First position wins when MS7 loads several; an
        // unmapped tag keeps the orb's shipped violet (see ColorForStatusTag).
        if (Index == 0)
        {
            PendingColor = BreakerFX::ColorForStatusTag(Entry.Spec.StatusTag, GetDefault<ABreakerProjectileBase>()->OrbColor);
            bHasPendingColor = true;
        }
    }

    // Selected elemental positions share one conversion budget and one hit.
    Damage.ElementShares = BreakerElementShares::Resolve(Damage);
    if (!Damage.ElementShares.IsEmpty())
    {
        Damage.Element = Damage.ElementShares[0].Element;
        Damage.ElementalFraction = BreakerElementShares::TotalFraction(Damage.ElementShares);
    }
    if (const auto* State = Character->FindComponentByClass<UBreakerAbilityStateComponent>()) State->SnapshotSympatheticEntropy(Damage);
    PendingDamage = Damage;
    PendingMuzzle = Muzzle; PendingDirection = Direction; PendingSpeed = ProjectileSpeed;
    PendingProjectileClass = ProjectileClass;
    PendingPositions = Positions; bPendingAdvanceOnHit = bAdvanceOnHit;
    CastCycle = Cycle; CastCombat = OwnerCombat;
    if (!OwnerCombat || OwnerCombat->IsDead()) { CancelPendingCast(); return; }
    OwnerCombat->OnDeath.AddUniqueDynamic(this, &UBreakerAbility_Fracture::CancelPendingCast);
    const float AuthoredSeconds = FMath::IsFinite(BaseCastSeconds) ? BaseCastSeconds : GetDefault<UBreakerAbility_Fracture>()->BaseCastSeconds;
    if (!FMath::IsFinite(AuthoredSeconds)) { CancelPendingCast(); return; }
    const float Delay = FMath::Max(0.0f, AuthoredSeconds) / AbilityCastRateMultiplierFor(Character);
    if (Delay <= 0) CompleteCast();
    else World->GetTimerManager().SetTimer(CastTimer, this, &UBreakerAbility_Fracture::CompleteCast, Delay, false);
}

void UBreakerAbility_Fracture::CompleteCast()
{
    if (!IsActive() || PendingPositions <= 0) return;
    auto* Character = GetBreakerCharacter();
    UWorld* World = Character ? Character->GetWorld() : nullptr;
    if (!World || !CastCombat.IsValid() || CastCombat->IsDead() || !CastCycle.IsValid()) { CancelPendingCast(); return; }

    // Claim this lease before spawning: BeginPlay, material setup, initialization
    // and cycle broadcasts may call back into the ability or kill its owner.
    const uint64 Generation = CastGeneration;
    const auto Combat = CastCombat;
    const auto Cycle = CastCycle;
    const auto Damage = PendingDamage;
    const auto Statuses = PendingStatuses;
    const auto Class = PendingProjectileClass;
    const FVector Muzzle = PendingMuzzle, Direction = PendingDirection;
    const float Speed = PendingSpeed;
    const FLinearColor Color = PendingColor;
    const bool bTint = bHasPendingColor, bAdvanceOnHit = bPendingAdvanceOnHit;
    const int32 Positions = PendingPositions;
    PendingPositions = 0;
    World->GetTimerManager().ClearTimer(CastTimer);
    auto LeaseIsLive = [&]()
    {
        return Generation == CastGeneration && IsActive() && Combat.IsValid()
            && !Combat->IsDead() && Cycle.IsValid();
    };
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParams.Owner = Character; SpawnParams.Instigator = Character;
    auto* Projectile = World->SpawnActor<ABreakerProjectileBase>(Class ? *Class : ABreakerProjectileBase::StaticClass(), Muzzle, Direction.Rotation(), SpawnParams);
    if (!IsValid(Projectile) || !LeaseIsLive())
    {
        if (IsValid(Projectile)) Projectile->Destroy();
        if (Generation == CastGeneration) CancelPendingCast();
        return;
    }
    Projectile->SetImpactStatuses(Statuses);
    if (bTint) Projectile->SetOrbColor(Color);
    if (bAdvanceOnHit) Projectile->SetCycleAdvanceOnHit(Cycle.Get(), Positions);
    if (!IsValid(Projectile) || !LeaseIsLive())
    {
        if (IsValid(Projectile)) Projectile->Destroy();
        if (Generation == CastGeneration) CancelPendingCast();
        return;
    }
    Projectile->InitializeProjectile(Damage, Direction, Speed);
    if (!LeaseIsLive())
    {
        if (IsValid(Projectile)) Projectile->Destroy();
        if (Generation == CastGeneration) CancelPendingCast();
        return;
    }
    if (!bAdvanceOnHit)
        for (int32 Index = 0; Index < Positions && LeaseIsLive(); ++Index) Cycle->AdvanceCycle();
    if (LeaseIsLive()) EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
void UBreakerAbility_Fracture::ClearPendingCast()
{
    ++CastGeneration;
    if (auto* World = GetWorld()) World->GetTimerManager().ClearTimer(CastTimer);
    if (CastCombat.IsValid()) CastCombat->OnDeath.RemoveDynamic(this, &UBreakerAbility_Fracture::CancelPendingCast);
    CastCombat.Reset(); CastCycle.Reset(); PendingStatuses.Reset(); PendingDamage = FBreakerDamageRequest();
    PendingProjectileClass = nullptr; PendingPositions = 0; bHasPendingColor = false;
}

void UBreakerAbility_Fracture::CancelPendingCast()
{
    if (IsActive()) EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
    else ClearPendingCast();
}

void UBreakerAbility_Fracture::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    ClearPendingCast();
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UBreakerAbility_Fracture::OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
    CancelPendingCast();
    Super::OnRemoveAbility(ActorInfo, Spec);
}