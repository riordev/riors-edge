#include "Abilities/BreakerAbility_Fracture.h"

#include "Abilities/BreakerAbilityTags.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Combat/BreakerProjectileBase.h"
#include "Combat/BreakerStatusCycleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"

UBreakerAbility_Fracture::UBreakerAbility_Fracture()
{
    FallbackAbilityId = TEXT("Caster.Fracture");
    // Spec §5.5: it spawns an actor, so it never runs on a client.
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
    ProjectileClass = ABreakerProjectileBase::StaticClass();

    FGameplayTagContainer Tags;
    Tags.AddTag(BreakerAbilityTags::Ability_Class_Caster_Fracture.GetTag());
    SetAssetTags(Tags);
}

void UBreakerAbility_Fracture::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
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
    // (exactly 1.0 at item level 1, so the authored 30 is bit-identical there).
    Damage.BaseDamage = ImpactDamage * AbilityDamageScalarFor(Character);
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

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParams.Owner = Character;
    SpawnParams.Instigator = Character;
    ABreakerProjectileBase* Projectile = World->SpawnActor<ABreakerProjectileBase>(
        ProjectileClass ? *ProjectileClass : ABreakerProjectileBase::StaticClass(), Muzzle, Direction.Rotation(), SpawnParams);
    if (!Projectile)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // Advance the cycle NOW, at cast, and load the drawn positions onto the
    // projectile. MS2 R1 moves this to on-hit; that is a data flag on the cycle
    // component (SetAdvanceOnHit) rather than a second code path here, and it
    // is deliberately not wired yet — the projectile would have to call back
    // into the cycle on impact, which is a hook the node can add when it lands.
    const int32 Positions = FMath::Clamp(CyclePositionsPerCast, 1, FMath::Max(1, Cycle->GetCycleLength()));
    for (int32 Index = 0; Index < Positions; ++Index)
    {
        FBreakerCycleEntry Entry = Cycle->PeekNextEntry(0);
        Cycle->AdvanceCycle();
        if (!Entry.Spec.StatusTag.IsValid()) continue;

        // O35: the cycle's authored per-tick numbers are item-level-1 values;
        // the applied copy rides the weapon scalar exactly as the impact hit
        // does. Scaled on the COPY, never on the cycle's own entry, so the
        // authored cycle survives an equipment change untouched.
        Entry.Spec.BaseDamagePerTick *= AbilityDamageScalarFor(Character);

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
        Entry.Spec.Snapshot.bRolledCritical = Stream.FRand() < Entry.Spec.Snapshot.CriticalChance;

        FBreakerCarriedStatus Carried;
        Carried.Spec = Entry.Spec;
        Carried.DamageFamily = Entry.DamageFamily;
        Projectile->AddImpactStatus(Carried);
    }

    Projectile->InitializeProjectile(Damage, Direction, ProjectileSpeed);
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
