#include "Abilities/BreakerTankAbilities.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Abilities/BreakerAbilityTags.h"
#include "Abilities/BreakerMeleeSweep.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerGritComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Combat/BreakerDeployable.h"
#include "Combat/BreakerEnemy.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "TimerManager.h"
#include "Weapons/BreakerWeaponComponent.h"

namespace BreakerTankAbilityLocal
{
    // Prefixed for the unity build, per house rule.

    // Node reads, the Cleave's Edge pattern: rank for the R1/R2 nodes, tag for
    // the pure rewrites. Null-safe: no progression component is rank 0.
    int32 BreakerTankNodeRank(const ABreakerCharacter* Character, const TCHAR* NodeId)
    {
        const UBreakerProgressionComponent* Progression = Character ? Character->FindComponentByClass<UBreakerProgressionComponent>() : nullptr;
        return Progression ? Progression->GetNodeRank(FName(NodeId), EBreakerPointCurrency::ClassPoints) : 0;
    }

    bool BreakerTankHasNode(const ABreakerCharacter* Character, const FGameplayTag& Tag)
    {
        const UBreakerProgressionComponent* Progression = Character ? Character->FindComponentByClass<UBreakerProgressionComponent>() : nullptr;
        return Progression && Progression->HasNodeTag(Tag);
    }

    // A target's maximum health, for Fragmentation's portion-of-health echo.
    // The Support file's accessor pattern, duplicated here because the two
    // files must not depend on one another.
    float BreakerTankMaxHealthOf(const AActor* Target)
    {
        if (const IAbilitySystemInterface* AbilityOwner = Cast<IAbilitySystemInterface>(Target))
        {
            if (const UAbilitySystemComponent* ASC = AbilityOwner->GetAbilitySystemComponent())
            {
                if (const UBreakerAttributeSet* Attributes = ASC->GetSet<UBreakerAttributeSet>())
                {
                    return Attributes->GetMaxHealth();
                }
            }
        }
        return 0.0f;
    }

    // The Demolitionist node payload one blast can carry. Defaults are all
    // inert, so an unmodified call resolves bit-identically to before the
    // struct existed.
    struct FBreakerTankBlastMods
    {
        // D1 Shaped Charge: full damage over this inner fraction of the radius.
        float PlateauFraction = 0.0f;
        // D4 Fragmentation: killed enemies echo for this fraction of their own
        // maximum health, in this radius. 0 disables.
        float FragmentationFraction = 0.0f;
        float FragmentationRadiusCm = 0.0f;
        // D11 Chain Reaction: per-target blast stamps live on the Grit
        // component; non-null enables the stacking flat bonus.
        UBreakerGritComponent* ChainGrit = nullptr;
        float ChainFlatPerStack = 0.0f;
        // Internal: a Fragmentation echo procs nothing and NEVER chains — the
        // anti-recursion guard the node text demands.
        bool bEcho = false;
    };

    // The shared "weapon-scaled with an unarmed floor" base the melee/blast
    // verbs use — the Cleave precedent (O35: reads the SCALED weapon base, so
    // ability damage rides gear depth; item level 1 is the authored number).
    // FULL BLAST, not per pellet (audit F1, the Cleave fix verbatim): the
    // per-pellet base made a shotgun Tank — the class/weapon pairing the
    // theme is built on — swing 10 while a sniper Tank swung 72. Rend,
    // Breach Charge and Ground Zero all mean the whole trigger pull by
    // "weapon damage", so all three read the full-blast accessor through
    // this one seam.
    float BreakerTankAbilityBaseDamage(const ABreakerCharacter* Character, float WeaponCoefficient, float UnarmedDamage)
    {
        float WeaponDamage = 0.0f;
        if (const UBreakerWeaponComponent* Weapon = Character ? Character->GetWeapon() : nullptr)
        {
            if (Weapon->GetActiveDefinition())
            {
                WeaponDamage = Weapon->GetScaledFullBlastDamage();
            }
        }
        const float ScaledUnarmed = UnarmedDamage * UBreakerGameplayAbility::AbilityDamageScalarFor(Character);
        const float Base = WeaponDamage > 0.0f ? WeaponDamage * WeaponCoefficient : ScaledUnarmed;
        return FMath::Max(0.0f, Base);
    }

    // One radial damage application against enemies only, linear falloff to
    // the edge fraction — the rocket's falloff shape, optionally flattened
    // into D1's inner plateau. Returns total damage actually applied to
    // health+shield. Mods carry the Demolitionist node payloads; the default
    // struct resolves bit-identically to the pre-node helper.
    float BreakerTankRadialDamage(UWorld* World, ABreakerCharacter* Character, const FVector& Center,
        float RadiusCm, float BaseDamage, float EdgeFraction, bool bApplyFalloff,
        const FBreakerTankBlastMods& Mods = FBreakerTankBlastMods())
    {
        if (!World || BaseDamage <= 0.0f) return 0.0f;
        const UBreakerAttributeSet* SourceAttributes = Character ? Character->GetAttributes() : nullptr;
        UBreakerCombatComponent* OwnerCombat = Character ? Character->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
        float TotalApplied = 0.0f;
        // D4's echo list: victims killed by THIS blast, resolved after the loop
        // so an echo can never re-enter the iteration it came from.
        TArray<TPair<FVector, float>> FragmentationEchoes;
        for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
        {
            ABreakerEnemy* Enemy = *It;
            if (!Enemy) continue;
            UBreakerCombatComponent* TargetCombat = Enemy->FindComponentByClass<UBreakerCombatComponent>();
            if (!TargetCombat || TargetCombat->IsDead()) continue;
            const float Distance = FVector::Dist(Center, Enemy->GetActorLocation());
            if (Distance > RadiusCm) continue;
            // D1 Shaped Charge: a full-damage plateau over the inner fraction,
            // then the ordinary linear falloff re-based to run from the
            // plateau's edge — peak damage unchanged, exactly as the node says.
            float Falloff = 1.0f;
            if (bApplyFalloff)
            {
                const float Fraction = FMath::Clamp(Distance / RadiusCm, 0.0f, 1.0f);
                const float Plateau = FMath::Clamp(Mods.PlateauFraction, 0.0f, 0.99f);
                const float Rebased = Fraction <= Plateau ? 0.0f : (Fraction - Plateau) / (1.0f - Plateau);
                Falloff = FMath::Lerp(1.0f, EdgeFraction, Rebased);
            }

            FBreakerDamageRequest Damage;
            Damage.BaseDamage = BaseDamage * Falloff;
            // D11 Chain Reaction: a later blast inside the window on the same
            // target carries stacking FLAT damage — flat bucket, hard cap, and
            // the stamps live with the Grit component so both explosive verbs
            // share one ledger. Echoes never stamp and never collect.
            if (Mods.ChainGrit && !Mods.bEcho && Mods.ChainFlatPerStack > 0.0f)
            {
                const int32 Stacks = Mods.ChainGrit->RegisterExplosiveBlast(Enemy);
                if (Stacks > 0) Damage.BaseDamage += Mods.ChainFlatPerStack * Stacks;
            }
            Damage.DamageFamily = EBreakerDamageFamily::Physical;
            Damage.CriticalChance = SourceAttributes ? SourceAttributes->GetCriticalChance() : UBreakerAttributeSet::DefaultCriticalChance;
            Damage.CriticalMultiplier = SourceAttributes ? SourceAttributes->GetCriticalMultiplier() : UBreakerAttributeSet::DefaultCriticalMultiplier;
            UBreakerDamageLibrary::FillSourcePools(SourceAttributes, EBreakerDamageDelivery::Ability, Damage);
            if (Mods.bEcho)
            {
                // Proc coefficient 0 and no crit: the echo is a settlement, not
                // a hit that can seed anything (the Caster MS4 normalization).
                Damage.ProcCoefficient = 0.0f;
                Damage.bCanCritical = false;
            }
            Damage.SourceLocation = Center;
            Damage.bHasSourceLocation = true;
            Damage.SetInstigator(Character);
            if (OwnerCombat) OwnerCombat->ApplyOutgoingModifiers(Damage);
            const FBreakerDamageResult Result = TargetCombat->ReceiveDamage(Damage);
            TotalApplied += Result.HealthDamage + Result.ShieldDamage;

            // D4 Fragmentation: enemies this blast KILLED detonate for a
            // portion of their own health. Collected here, paid below.
            if (Result.bKilled && !Mods.bEcho && Mods.FragmentationFraction > 0.0f)
            {
                const float VictimMaxHealth = BreakerTankMaxHealthOf(Enemy);
                if (VictimMaxHealth > 0.0f)
                {
                    FragmentationEchoes.Emplace(Enemy->GetActorLocation(), VictimMaxHealth * Mods.FragmentationFraction);
                }
            }
        }

        for (const TPair<FVector, float>& Echo : FragmentationEchoes)
        {
            FBreakerTankBlastMods EchoMods;
            EchoMods.bEcho = true;   // procs nothing, never chains, never re-echoes
            TotalApplied += BreakerTankRadialDamage(World, Character, Echo.Key,
                Mods.FragmentationRadiusCm, Echo.Value, EdgeFraction, /*bApplyFalloff=*/true, EchoMods);
        }
        return TotalApplied;
    }
}

// ---------------------------------------------------------------------------
// T1 — REND
// ---------------------------------------------------------------------------

UBreakerAbility_Rend::UBreakerAbility_Rend()
{
    FallbackAbilityId = TEXT("Tank.Rend");
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void UBreakerAbility_Rend::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    ABreakerCharacter* Character = GetBreakerCharacter();
    UWorld* World = Character ? Character->GetWorld() : nullptr;
    if (!World || !CommitAbility(Handle, ActorInfo, ActivationInfo))
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
    FVector Forward = ViewRotation.Vector();
    Forward.Z = 0.0;
    if (!Forward.Normalize()) Forward = Character->GetActorForwardVector();

    using namespace BreakerTankAbilityLocal;

    // L7 REND MASTERY: the arc widens to 180 degrees and the heal pays per
    // target rather than once per cast (below).
    const bool bRendMastery = BreakerTankHasNode(Character, BreakerNodeTags::Node_L_RendMastery.GetTag());

    FBreakerMeleeSweepParams Params;
    Params.Origin = Character->GetActorLocation();
    Params.Forward = Forward;
    Params.RangeCm = RangeCm;
    Params.ArcDegrees = bRendMastery ? 180.0f : ArcDegrees;   // L7

    const float BaseDamage = BreakerTankAbilityLocal::BreakerTankAbilityBaseDamage(Character, WeaponDamageCoefficient, UnarmedDamage);
    const UBreakerAttributeSet* SourceAttributes = GetBreakerAttributes();
    UBreakerCombatComponent* OwnerCombat = Character->FindComponentByClass<UBreakerCombatComponent>();
    UBreakerGritComponent* Grit = Character->FindComponentByClass<UBreakerGritComponent>();

    // L1 CLOT: the overheal-to-shield ratio rises above 1:1.
    const int32 ClotRank = BreakerTankNodeRank(Character, TEXT("Tank.Leech.Clot"));
    const float ClotRatio = ClotRank >= 2 ? 1.5f : (ClotRank == 1 ? 1.25f : 1.0f);   // node text
    // L3 OPEN WOUND: Life on Hit fires on the first target of a sweep (R2:
    // every target). SAME RECORDED SUBSTITUTION AS BLOODLINE: no Life on Hit
    // stat exists anywhere, so the gear's LifeOnKill magnitude stands in —
    // still reads gear, still grants nothing without it.
    const int32 OpenWoundRank = BreakerTankNodeRank(Character, TEXT("Tank.Leech.OpenWound"));
    const UBreakerEquipmentComponent* Equipment = Character->GetEquipment();
    const float OpenWoundLeech = (OpenWoundRank > 0 && Equipment) ? Equipment->GetStats().LifeOnKill : 0.0f;
    // L5 BLOODLET: melee kills heal a fraction of maximum health, overheal
    // routed through the normal, capped path.
    const int32 BloodletRank = BreakerTankNodeRank(Character, TEXT("Tank.Leech.Bloodlet"));
    const float BloodletFraction = BloodletRank >= 2 ? 0.14f : (BloodletRank == 1 ? 0.08f : 0.0f);   // node text
    const float OwnMaxHealth = SourceAttributes ? SourceAttributes->GetMaxHealth() : 0.0f;

    float TotalPostMitigation = 0.0f;
    int32 TargetIndex = 0;
    for (AActor* Target : UBreakerMeleeSweep::SweepTargets(World, Character, Params))
    {
        UBreakerCombatComponent* TargetCombat = Target ? Target->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
        if (!TargetCombat) continue;

        FBreakerDamageRequest Damage;
        Damage.BaseDamage = BaseDamage;
        Damage.DamageFamily = EBreakerDamageFamily::Physical;
        // Damage.Melee gates the melee affix/More layer, exactly as Cleave.
        Damage.SourceTags.AddTag(BreakerAbilityTags::Damage_Melee.GetTag());
        Damage.CriticalChance = SourceAttributes ? SourceAttributes->GetCriticalChance() : UBreakerAttributeSet::DefaultCriticalChance;
        Damage.CriticalMultiplier = SourceAttributes ? SourceAttributes->GetCriticalMultiplier() : UBreakerAttributeSet::DefaultCriticalMultiplier;
        // O55: melee, so weapon-delivered — the same reading as Cleave, and
        // the same reading the Damage_Melee tag above already gives the
        // affix layer.
        UBreakerDamageLibrary::FillSourcePools(SourceAttributes, EBreakerDamageDelivery::Weapon, Damage);
        Damage.RandomSeed = HashCombine(GetTypeHash(Character), static_cast<uint32>(TargetIndex) + static_cast<uint32>(World->GetTimeSeconds() * 1000.0));
        Damage.SourceLocation = Params.Origin;
        Damage.bHasSourceLocation = true;
        Damage.SetInstigator(Character);
        if (OwnerCombat) OwnerCombat->ApplyOutgoingModifiers(Damage);
        const FBreakerDamageResult Result = TargetCombat->ReceiveDamage(Damage);
        const float TargetPostMitigation = Result.HealthDamage + Result.ShieldDamage;
        TotalPostMitigation += TargetPostMitigation;

        // L7's per-target heal: each target hit pays its own heal at the
        // sweep's coefficient, through the same overheal-to-shield route, so
        // Clot's ratio applies to every conversion the cast makes.
        if (bRendMastery && TargetPostMitigation > 0.0f && OwnerCombat)
        {
            FBreakerHealRequest Heal;
            Heal.Amount = TargetPostMitigation * HealFraction;
            Heal.bOverhealToShield = true;
            Heal.OverhealToShieldFraction = OverhealShieldFraction;
            Heal.SetHealer(Character);
            const FBreakerHealResult HealResult = OwnerCombat->ApplyHealing(Heal);
            if (ClotRatio > 1.0f && HealResult.ShieldGranted > 0.0f && SourceAttributes)
            {
                GetBreakerAttributes()->ApplyShield(FMath::Min(SourceAttributes->GetMaxShield(),
                    SourceAttributes->GetShield() + HealResult.ShieldGranted * (ClotRatio - 1.0f)));
            }
        }

        // L3 Open Wound: the leech stand-in pays on the FIRST target (R2: on
        // every target), a plain heal through the one healing path.
        if (OpenWoundLeech > 0.0f && OwnerCombat && (TargetIndex == 0 || OpenWoundRank >= 2))
        {
            OwnerCombat->ApplyHealingAmount(OpenWoundLeech, Character, FGameplayTag());
        }

        // §1.1's aggression source: a melee kill pays Grit, and Rend is the
        // Tank's melee verb — the one honest caller NotifyMeleeKill has.
        if (Result.bKilled)
        {
            if (Grit) Grit->NotifyMeleeKill();
            // L5 Bloodlet: the kill also heals a fraction of maximum health,
            // overheal routed through the normal capped shield path.
            if (BloodletFraction > 0.0f && OwnMaxHealth > 0.0f && OwnerCombat)
            {
                FBreakerHealRequest KillHeal;
                KillHeal.Amount = OwnMaxHealth * BloodletFraction;
                KillHeal.bOverhealToShield = true;
                KillHeal.OverhealToShieldFraction = OverhealShieldFraction;
                KillHeal.SetHealer(Character);
                OwnerCombat->ApplyHealing(KillHeal);
            }
        }
        ++TargetIndex;
    }

    // §T1: heals 35% of POST-MITIGATION damage dealt; overheal becomes shield.
    // Two recorded differences from the doc: the shield cap here is a fraction
    // of MaxShield (the healing path's cap) rather than 25% of maximum health,
    // and the shield decay clock lives with the Grit component (L2's home).
    // Under REND MASTERY the heal was already paid per target above.
    if (!bRendMastery && TotalPostMitigation > 0.0f && OwnerCombat)
    {
        // Raise the ceiling this ability's own overheal will cap against. Without
        // it the cap is whatever some other class's nodes happened to set, which
        // for a Tank is zero — see ShieldCeilingHealthFraction's declaration.
        if (SourceAttributes)
        {
            const float Ceiling = SourceAttributes->GetMaxHealth() * ShieldCeilingHealthFraction;
            if (Ceiling > SourceAttributes->GetMaxShield())
            {
                GetBreakerAttributes()->ApplyMaxShield(Ceiling);
            }
        }
        FBreakerHealRequest Heal;
        Heal.Amount = TotalPostMitigation * HealFraction;
        Heal.bOverhealToShield = true;
        Heal.OverhealToShieldFraction = OverhealShieldFraction;
        Heal.SourceTag = FGameplayTag();   // no heal-source vocabulary exists yet; empty is the honest tag
        Heal.SetHealer(Character);
        const FBreakerHealResult HealResult = OwnerCombat->ApplyHealing(Heal);
        // L1 Clot: overheal converts at 1.25:1 (R2 1.5:1) instead of 1:1 — the
        // extra quarter rides on what the 1:1 path actually granted, so the
        // shield cap keeps its authority.
        if (ClotRatio > 1.0f && HealResult.ShieldGranted > 0.0f && SourceAttributes)
        {
            GetBreakerAttributes()->ApplyShield(FMath::Min(SourceAttributes->GetMaxShield(),
                SourceAttributes->GetShield() + HealResult.ShieldGranted * (ClotRatio - 1.0f)));
        }
    }

    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

// ---------------------------------------------------------------------------
// T2 — BLOODLINE
// ---------------------------------------------------------------------------

UBreakerAbility_Bloodline::UBreakerAbility_Bloodline()
{
    FallbackAbilityId = TEXT("Tank.Bloodline");
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

FName UBreakerAbility_Bloodline::WindowKey() { return TEXT("Window.Tank.Bloodline"); }

void UBreakerAbility_Bloodline::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    const UBreakerAbilityDefinition* Definition = GetAbilityDefinition();
    ABreakerCharacter* Character = GetBreakerCharacter();
    UWorld* World = Character ? Character->GetWorld() : nullptr;
    UBreakerCombatComponent* Combat = Character ? Character->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
    if (!World || !Combat || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // L11 EXSANGUINATE: the window no longer expires on the authored clock —
    // it expires 2 seconds after the last landing hit. The cast opens a 2s
    // grace; every hit dealt re-opens it (HandleHitDealt). The downside is
    // real: forced off the target, the window ends almost immediately.
    bExsanguinate = BreakerTankAbilityLocal::BreakerTankHasNode(Character, BreakerNodeTags::Node_L_Exsanguinate.GetTag());
    const float AuthoredDuration = Definition ? Definition->WindowDuration : 8.0f;
    const float Duration = bExsanguinate ? 2.0f : AuthoredDuration;   // node text
    if (UBreakerAbilityStateComponent* State = UBreakerAbilityStateComponent::FindOrAdd(Character))
    {
        State->StartWindow(WindowKey(), Duration);
    }
    BoundCombat = Combat;
    Combat->OnHitDealt.AddDynamic(this, &UBreakerAbility_Bloodline::HandleHitDealt);
    bBloodlineActive = true;
    World->GetTimerManager().SetTimer(WindowTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { CloseBloodline(); }), Duration, false);
}

void UBreakerAbility_Bloodline::HandleHitDealt(const FBreakerHitContext& Hit)
{
    if (!bBloodlineActive) return;
    ABreakerCharacter* Character = GetBreakerCharacter();
    if (!Character) return;
    // L11 Exsanguinate: a landing hit re-opens the 2s grace. Non-DoT hits only
    // (the recorded nearest-honest melee filter — see the header); a bleed
    // ticking on its own must not sustain the window forever.
    if (bExsanguinate && !Hit.bFromDoT)
    {
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().SetTimer(WindowTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { CloseBloodline(); }), 2.0f, false);
        }
        if (UBreakerAbilityStateComponent* State = Character->FindComponentByClass<UBreakerAbilityStateComponent>())
        {
            State->StartWindow(WindowKey(), 2.0f);
        }
    }
    const UBreakerEquipmentComponent* Equipment = Character->GetEquipment();
    // MULTIPLIES WHAT YOU HAVE, GRANTS NOTHING IF YOU HAVE NONE (§T2): the
    // gear stat is the whole payout, so a Tank with no leech-side gear feels
    // Bloodline do exactly nothing — the intended gear-payoff identity. DoT
    // ticks pay too (the hit context flags them), which is the §T2 extension.
    const float LeechStat = Equipment ? Equipment->GetStats().LifeOnKill : 0.0f;
    if (LeechStat <= 0.0f) return;
    if (UBreakerCombatComponent* Combat = Character->FindComponentByClass<UBreakerCombatComponent>())
    {
        Combat->ApplyHealingAmount(LeechStat, Character, FGameplayTag());
    }
}

void UBreakerAbility_Bloodline::CloseBloodline()
{
    if (CurrentActorInfo)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
    }
}

void UBreakerAbility_Bloodline::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (bBloodlineActive)
    {
        bBloodlineActive = false;
        if (UBreakerCombatComponent* Combat = BoundCombat.Get())
        {
            Combat->OnHitDealt.RemoveDynamic(this, &UBreakerAbility_Bloodline::HandleHitDealt);
        }
        BoundCombat.Reset();
        if (ABreakerCharacter* Character = GetBreakerCharacter())
        {
            if (UBreakerAbilityStateComponent* State = Character->FindComponentByClass<UBreakerAbilityStateComponent>())
            {
                State->CloseWindow(WindowKey());
            }
        }
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(WindowTimer);
        }
    }
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// ---------------------------------------------------------------------------
// T3 — ANCHOR POINT
// ---------------------------------------------------------------------------

UBreakerAbility_AnchorPoint::UBreakerAbility_AnchorPoint()
{
    FallbackAbilityId = TEXT("Tank.AnchorPoint");
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void UBreakerAbility_AnchorPoint::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    ABreakerCharacter* Character = GetBreakerCharacter();
    UWorld* World = Character ? Character->GetWorld() : nullptr;
    if (!Character || !World)
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
    FVector Forward = ViewRotation.Vector();
    Forward.Z = 0.0;
    if (!Forward.Normalize()) Forward = Character->GetActorForwardVector();

    using namespace BreakerTankAbilityLocal;

    // Validate BEFORE commit — a refused placement costs no Grit, matching the
    // deployable rule (§2.3 by symmetry).
    FVector PlaceLocation;
    bool bPlaced = ABreakerDeployable::ResolvePlacement(World, Character, Character->GetActorLocation() + FVector(0, 0, 50.0f), Forward, PlacementRangeCm, PlaceLocation);
    // B7 EMPLACEMENT: with no floor under the aim, the panel may take a wall or
    // ceiling instead — a plain aim trace, the surface the ray actually hit.
    // The behind-cover stationary-spread half WAITS on the weapon layer's
    // spread posture read (sibling-owned this pass).
    const bool bEmplacement = BreakerTankHasNode(Character, BreakerNodeTags::Node_B_Emplacement.GetTag());
    if (!bPlaced && bEmplacement)
    {
        FVector ViewLocation2 = Character->GetActorLocation();
        FRotator ViewRotation2 = Character->GetControlRotation();
        if (const AController* Controller = Character->GetController())
        {
            Controller->GetPlayerViewPoint(ViewLocation2, ViewRotation2);
        }
        FHitResult SurfaceHit;
        FCollisionQueryParams SurfaceParams(SCENE_QUERY_STAT(BreakerAnchorSurface), false, Character);
        if (World->LineTraceSingleByChannel(SurfaceHit, ViewLocation2, ViewLocation2 + ViewRotation2.Vector() * PlacementRangeCm * 2.0f, ECC_Visibility, SurfaceParams))
        {
            PlaceLocation = SurfaceHit.ImpactPoint + SurfaceHit.ImpactNormal * 20.0f;
            bPlaced = true;
        }
    }
    if (!bPlaced)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParams.Owner = Character;
    // Faces back at the Tank, so the panel stands between the Tank and where
    // they were looking.
    const FRotator PanelFacing = FRotator(0.0f, Forward.Rotation().Yaw, 0.0f);
    if (ABreakerDeployable* Panel = World->SpawnActor<ABreakerDeployable>(ABreakerDeployable::StaticClass(), PlaceLocation, PanelFacing, SpawnParams))
    {
        // B6 BULK: half again the health (R2: double), applied to the fraction
        // BEFORE the deployable computes its pool from it. The shrug-off-AoE
        // half WAITS: no aimed-at-vs-splash attribution exists on any damage
        // request. B11 halves the panel's exposure a different way below.
        const int32 BulkRank = BreakerTankNodeRank(Character, TEXT("Tank.Bastion.Bulk"));
        if (BulkRank > 0)
        {
            Panel->AnchorHealthFraction *= (BulkRank >= 2 ? 2.0f : 1.5f);   // node text
        }
        // Cost 0 into the deployable: Grit was already spent through the GAS
        // cost effect, and Anchor Point refunds nothing ("this is not a Scrap
        // economy", §T3).
        Panel->InitializeDeployable(EBreakerDeployableType::AnchorPoint, Character, 0.0f);

        // B1 LINE OF SIGHT: the panel stands 16s (R2: 20s) instead of 12 — and
        // the ability-duration seam the tree's AbilityDuration lane composes
        // into is adopted at the same site, so the library's first authored
        // line against Anchor Point pays the day it lands.
        float Lifetime = Panel->GetRemainingLifetime() * GetAbilityDurationMultiplier();
        const int32 SightRank = BreakerTankNodeRank(Character, TEXT("Tank.Bastion.LineOfSight"));
        if (SightRank > 0)
        {
            Lifetime = (SightRank >= 2 ? 20.0f : 16.0f) * GetAbilityDurationMultiplier();   // node text
        }
        // B11 IMMOVABLE OBJECT: indestructible for its first 4s — and its total
        // lifetime is halved. Cost-for-power with the downside in the same line.
        if (BreakerTankHasNode(Character, BreakerNodeTags::Node_B_ImmovableObject.GetTag()))
        {
            Lifetime *= 0.5f;   // node text
            if (UBreakerCombatComponent* PanelCombat = Panel->FindComponentByClass<UBreakerCombatComponent>())
            {
                PanelCombat->PushIncomingDamageModifier(TEXT("Bastion.Immovable"), 0.0f);
                TWeakObjectPtr<ABreakerDeployable> WeakPanel(Panel);
                FTimerHandle GateTimer;
                World->GetTimerManager().SetTimer(GateTimer, FTimerDelegate::CreateLambda([WeakPanel]()
                {
                    if (ABreakerDeployable* Live = WeakPanel.Get())
                    {
                        if (UBreakerCombatComponent* LiveCombat = Live->FindComponentByClass<UBreakerCombatComponent>())
                        {
                            LiveCombat->RemoveIncomingDamageModifier(TEXT("Bastion.Immovable"));
                        }
                    }
                }), 4.0f, false);   // node text
            }
        }
        Panel->LifetimeRemaining = Lifetime;

        // B4 Held Ground R2: placing an Anchor Point re-triggers the entry
        // grant, once per combat state — the Grit component owns the gate.
        if (UBreakerGritComponent* Grit = Character->FindComponentByClass<UBreakerGritComponent>())
        {
            Grit->NotifyAnchorPlaced();
        }
    }

    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

// ---------------------------------------------------------------------------
// T4 — PROVOKE
// ---------------------------------------------------------------------------

UBreakerAbility_Provoke::UBreakerAbility_Provoke()
{
    FallbackAbilityId = TEXT("Tank.Provoke");
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

FName UBreakerAbility_Provoke::OutgoingModifierKey() { return TEXT("Provoke"); }

void UBreakerAbility_Provoke::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    ABreakerCharacter* Character = GetBreakerCharacter();
    UWorld* World = Character ? Character->GetWorld() : nullptr;
    if (!World || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    using namespace BreakerTankAbilityLocal;

    // B3 LOUD: Provoke reaches 13 m (R2: 16 m) instead of 10.
    const int32 LoudRank = BreakerTankNodeRank(Character, TEXT("Tank.Bastion.Loud"));
    const float EffectiveRadius = LoudRank >= 2 ? 1600.0f : (LoudRank == 1 ? 1300.0f : RadiusCm);   // node text
    // B10 STANDING ORDER: the window holds 10s — "until the enemy is damaged by
    // someone who is not you" is vacuous solo (there is no one else), so the
    // 10s clock is the whole reachable rewrite and stands recorded as such.
    const bool bStandingOrder = BreakerTankHasNode(Character, BreakerNodeTags::Node_B_StandingOrder.GetTag());
    const float EffectiveDuration = bStandingOrder ? 10.0f : BonusDurationSeconds;   // node text

    // Count the provoked. The FORCED-TARGETING half is honestly absent (no
    // threat concept on enemy AI — Class-Kits-Unbuilt §5.3); solo it is
    // vacuous anyway. The SOLO CONVERSION below is §T4's implemented half.
    int32 Provoked = 0;
    for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
    {
        const ABreakerEnemy* Enemy = *It;
        if (!Enemy) continue;
        const UBreakerCombatComponent* EnemyCombat = Enemy->FindComponentByClass<UBreakerCombatComponent>();
        if (!EnemyCombat || EnemyCombat->IsDead()) continue;
        if (FVector::DistSquared(Character->GetActorLocation(), Enemy->GetActorLocation()) <= EffectiveRadius * EffectiveRadius)
        {
            ++Provoked;
        }
    }

    // §T4: each enemy provoked grants stacking FLAT damage (+4% of weapon base
    // per enemy, 6 stacks max, 6s), delivered as a flat contribution so it
    // cannot double-dip with the Increased bucket.
    if (Provoked > 0)
    {
        float WeaponBase = 0.0f;
        if (const UBreakerWeaponComponent* Weapon = Character->GetWeapon())
        {
            WeaponBase = Weapon->GetScaledBaseDamage();
        }
        const int32 Stacks = FMath::Min(Provoked, MaximumStacks);
        const float FlatBonus = WeaponBase * FlatDamagePerEnemyFraction * Stacks;
        if (UBreakerCombatComponent* Combat = Character->FindComponentByClass<UBreakerCombatComponent>())
        {
            Combat->PushOutgoingModifier(OutgoingModifierKey(), FlatBonus, 1.0f, EffectiveDuration);
        }

        // B5 ANSWERING FIRE: enemies you have Provoked pay proximity Grit at
        // 1.5x (R2: 2x). RECORDED SUBSTITUTION: no threat list exists to ask
        // whether the near enemy is a provoked one, so the boost rides
        // Provoke's own window — still count-independent, still capped, and it
        // exists only while something was actually provoked.
        const int32 AnsweringRank = BreakerTankNodeRank(Character, TEXT("Tank.Bastion.AnsweringFire"));
        if (AnsweringRank > 0)
        {
            if (UBreakerGritComponent* Grit = Character->FindComponentByClass<UBreakerGritComponent>())
            {
                Grit->PushProximityRateBoost(OutgoingModifierKey(), AnsweringRank >= 2 ? 2.0f : 1.5f, EffectiveDuration);   // node text
            }
        }
    }

    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

// ---------------------------------------------------------------------------
// T5 — BREACH CHARGE
// ---------------------------------------------------------------------------

UBreakerAbility_BreachCharge::UBreakerAbility_BreachCharge()
{
    FallbackAbilityId = TEXT("Tank.BreachCharge");
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void UBreakerAbility_BreachCharge::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    ABreakerCharacter* Character = GetBreakerCharacter();
    UWorld* World = Character ? Character->GetWorld() : nullptr;
    if (!World || !CommitAbility(Handle, ActorInfo, ActivationInfo))
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

    // The throw resolves as an aim trace rather than a projectile actor: no
    // grenade arc exists to reuse and inventing one is not this pass. Where
    // the ray lands, the charge sits; 1.2s later it detonates (§T5).
    FHitResult Hit;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BreakerBreachAim), false, Character);
    const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * ThrowRangeCm;
    const FVector BlastLocation = World->LineTraceSingleByChannel(Hit, ViewLocation, TraceEnd, ECC_Visibility, QueryParams)
        ? Hit.ImpactPoint : TraceEnd;

    TWeakObjectPtr<UBreakerAbility_BreachCharge> WeakThis(this);
    World->GetTimerManager().SetTimer(FuseTimer, FTimerDelegate::CreateLambda([WeakThis, BlastLocation]()
    {
        if (UBreakerAbility_BreachCharge* Ability = WeakThis.Get())
        {
            Ability->Detonate(BlastLocation);
        }
    }), FMath::Max(0.05f, FuseSeconds), false);
    // The ability itself ends now; the fuse timer owns the detonation. The
    // cooldown (8s) already prevents a second charge racing the first fuse.
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

bool UBreakerAbility_BreachCharge::CheckCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
    if (Super::CheckCooldown(Handle, ActorInfo, OptionalRelevantTags))
    {
        // The one cooldown has expired: both charges are home again.
        bDemolitionSecondSpent = false;
        return true;
    }
    // D7 DEMOLITION: one extra cast is admitted into a running cooldown.
    return BreakerTankAbilityLocal::BreakerTankHasNode(GetBreakerCharacter(), BreakerNodeTags::Node_D_Demolition.GetTag())
        && !bDemolitionSecondSpent;
}

void UBreakerAbility_BreachCharge::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
    // The second charge does NOT restart the timer — both charges share the
    // one cooldown the first cast started, per the node's exact wording.
    if (BreakerTankAbilityLocal::BreakerTankHasNode(GetBreakerCharacter(), BreakerNodeTags::Node_D_Demolition.GetTag())
        && !Super::CheckCooldown(Handle, ActorInfo, nullptr))
    {
        bDemolitionSecondSpent = true;
        return;
    }
    bDemolitionSecondSpent = false;
    Super::ApplyCooldown(Handle, ActorInfo, ActivationInfo);
}

void UBreakerAbility_BreachCharge::Detonate(FVector BlastLocation)
{
    using namespace BreakerTankAbilityLocal;
    ABreakerCharacter* Character = GetBreakerCharacter();
    UWorld* World = Character ? Character->GetWorld() : nullptr;
    if (!World) return;

    // D9 BLAST RADIUS: the enemy-facing radius grows by half; the self-facing
    // reads below stay on the OLD radius — each side of the explosion reads a
    // different geometry, which is the node's whole point.
    const bool bBlastRadius = BreakerTankHasNode(Character, BreakerNodeTags::Node_D_BlastRadius.GetTag());
    const float EnemyRadius = BlastRadiusCm * (bBlastRadius ? 1.5f : 1.0f);   // node text

    FBreakerTankBlastMods Mods;
    const int32 ShapedRank = BreakerTankNodeRank(Character, TEXT("Tank.Demolitionist.ShapedCharge"));
    if (ShapedRank > 0) Mods.PlateauFraction = ShapedRank >= 2 ? 0.6f : 0.4f;   // D1
    const int32 FragRank = BreakerTankNodeRank(Character, TEXT("Tank.Demolitionist.Fragmentation"));
    if (FragRank > 0)
    {
        Mods.FragmentationFraction = 0.25f;   // O2 PLACEHOLDER ("a portion of their health")
        Mods.FragmentationRadiusCm = FragRank >= 2 ? 400.0f : 300.0f;   // D4
    }
    if (BreakerTankHasNode(Character, BreakerNodeTags::Node_D_ChainReaction.GetTag()))
    {
        Mods.ChainGrit = Character->FindComponentByClass<UBreakerGritComponent>();
        Mods.ChainFlatPerStack = 6.0f * AbilityDamageScalarFor(Character);   // O2 PLACEHOLDER, flat bucket
    }

    const float BaseDamage = BreakerTankAbilityLocal::BreakerTankAbilityBaseDamage(Character, WeaponDamageCoefficient, UnarmedDamage);
    BreakerTankAbilityLocal::BreakerTankRadialDamage(World, Character, BlastLocation, EnemyRadius, BaseDamage, EdgeDamageFraction, /*bApplyFalloff=*/true, Mods);

    // The Tank's end of the tool: knockback with FULL directional control (the
    // impulse is the blast normal, undamped) and self-damage that is reduced
    // and NEVER zero (O13). The self-hit goes through the Tank's own combat
    // component as a real self-instigated request, so post-mitigation Grit
    // generation sees it at the self-damage rate through the ordinary wiring.
    const float SelfDistance = FVector::Dist(BlastLocation, Character->GetActorLocation());
    if (SelfDistance <= BlastRadiusCm)
    {
        FVector Away = Character->GetActorLocation() - BlastLocation;
        if (!Away.Normalize()) Away = FVector::UpVector;
        // D2 BOOTSTRAPS: the launch follows the AIM, not the blast normal —
        // the Tank flies away from where they were looking, whatever angle the
        // charge actually sat at. The launch never reads the self-damage
        // numbers on either branch, which is R2's clause standing structurally.
        if (BreakerTankNodeRank(Character, TEXT("Tank.Demolitionist.Bootstraps")) > 0)
        {
            FVector Aim = Character->GetControlRotation().Vector();
            if (Aim.Normalize()) Away = -Aim;
        }
        // Guarantee real lift so a floor-adjacent blast still moves the Tank.
        Away.Z = FMath::Max(Away.Z, 0.35f);
        Away.Normalize();
        const float Proximity = 1.0f - FMath::Clamp(SelfDistance / BlastRadiusCm, 0.0f, 1.0f);
        Character->LaunchCharacter(Away * KnockbackImpulse * FMath::Max(0.4f, Proximity), true, true);

        // D3 BRACED FOR IMPACT: self-damage reduction 50% -> 65% (R2: 80%, the
        // branch ceiling — NEVER 100). Lowering the self-hit LOWERS the Grit it
        // pays, automatically, through the post-mitigation rule.
        const int32 BracedRank = BreakerTankNodeRank(Character, TEXT("Tank.Demolitionist.BracedForImpact"));
        const float EffectiveSelfFraction = BracedRank >= 2 ? 0.2f : (BracedRank == 1 ? 0.35f : SelfDamageFraction);   // node text
        const float Falloff = FMath::Lerp(1.0f, EdgeDamageFraction, FMath::Clamp(SelfDistance / BlastRadiusCm, 0.0f, 1.0f));
        FBreakerDamageRequest SelfDamage;
        SelfDamage.BaseDamage = FMath::Max(1.0f, BaseDamage * Falloff * EffectiveSelfFraction);
        SelfDamage.DamageFamily = EBreakerDamageFamily::Physical;
        SelfDamage.bCanCritical = false;
        SelfDamage.SourceLocation = BlastLocation;
        SelfDamage.bHasSourceLocation = true;
        SelfDamage.SetInstigator(Character);
        if (UBreakerCombatComponent* OwnCombat = Character->FindComponentByClass<UBreakerCombatComponent>())
        {
            OwnCombat->ReceiveDamage(SelfDamage);
        }
    }
}

// ---------------------------------------------------------------------------
// T6 — GROUND ZERO
// ---------------------------------------------------------------------------

UBreakerAbility_GroundZero::UBreakerAbility_GroundZero()
{
    FallbackAbilityId = TEXT("Tank.GroundZero");
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void UBreakerAbility_GroundZero::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    ABreakerCharacter* Character = GetBreakerCharacter();
    UWorld* World = Character ? Character->GetWorld() : nullptr;
    const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
    // §T6: usable ONLY while airborne — checked before commit, so a grounded
    // press refuses without spending 45 Grit.
    if (!World || !Movement || !Movement->IsFalling() || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    using namespace BreakerTankAbilityLocal;

    // NEAREST HONEST fall scaling (see the header): current downward speed
    // stands in for fall distance. A normal jump's fall reaches the minimum
    // fraction, so the verb is fully usable from ordinary geometry (§T6).
    // D8 TERMINAL DESCENT raises the CAP (12 m -> 25 m, expressed through the
    // speed stand-in as the TerminalDescentPowerCap ceiling); its
    // from-any-airborne-state clause is already structural — the IsFalling
    // gate above admits a plain jump, exactly as O13 requires.
    const bool bTerminalDescent = BreakerTankHasNode(Character, BreakerNodeTags::Node_D_TerminalDescent.GetTag());
    const float PowerCeiling = bTerminalDescent ? FMath::Max(1.0f, TerminalDescentPowerCap) : 1.0f;
    const float DownSpeed = FMath::Max(0.0f, -Movement->Velocity.Z);
    const float Power = FMath::Clamp(DownSpeed / FullPowerFallSpeed, MinimumPowerFraction, PowerCeiling);

    // The slam itself: drive the Tank hard into the ground.
    Character->LaunchCharacter(FVector(0.0f, 0.0f, -SlamDownSpeed), false, true);

    // Damage resolves at the floor under the Tank, now — waiting for a landing
    // event would need a landing hook this ability does not own.
    FHitResult FloorHit;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BreakerGroundZero), false, Character);
    const FVector Start = Character->GetActorLocation();
    const FVector Center = World->LineTraceSingleByChannel(FloorHit, Start, Start - FVector(0, 0, 2000.0f), ECC_Visibility, QueryParams)
        ? FloorHit.ImpactPoint : Start;

    // The Demolitionist blast payload, shared with Breach Charge through the
    // one radial seam. Ground Zero has no self-damage side, so D9's radius
    // growth applies to its single (enemy-facing) geometry outright.
    const bool bBlastRadius = BreakerTankHasNode(Character, BreakerNodeTags::Node_D_BlastRadius.GetTag());
    const float EnemyRadius = BlastRadiusCm * (bBlastRadius ? 1.5f : 1.0f);   // D9
    FBreakerTankBlastMods Mods;
    const int32 ShapedRank = BreakerTankNodeRank(Character, TEXT("Tank.Demolitionist.ShapedCharge"));
    if (ShapedRank > 0) Mods.PlateauFraction = ShapedRank >= 2 ? 0.6f : 0.4f;   // D1
    const int32 FragRank = BreakerTankNodeRank(Character, TEXT("Tank.Demolitionist.Fragmentation"));
    if (FragRank > 0)
    {
        Mods.FragmentationFraction = 0.25f;   // O2 PLACEHOLDER
        Mods.FragmentationRadiusCm = FragRank >= 2 ? 400.0f : 300.0f;   // D4
    }
    if (BreakerTankHasNode(Character, BreakerNodeTags::Node_D_ChainReaction.GetTag()))
    {
        Mods.ChainGrit = Character->FindComponentByClass<UBreakerGritComponent>();
        Mods.ChainFlatPerStack = 6.0f * AbilityDamageScalarFor(Character);   // O2 PLACEHOLDER
    }

    const float BaseDamage = BreakerTankAbilityLocal::BreakerTankAbilityBaseDamage(Character, WeaponDamageCoefficient, UnarmedDamage) * Power;
    BreakerTankAbilityLocal::BreakerTankRadialDamage(World, Character, Center, EnemyRadius, BaseDamage, 0.5f, /*bApplyFalloff=*/true, Mods);

    // D5 CONCUSSION: the stagger runs 2.0s (R2: 2.5s) instead of 1.5. Its
    // mid-air clause is already structural — the stop below never asked
    // whether the enemy stood on the floor.
    const int32 ConcussionRank = BreakerTankNodeRank(Character, TEXT("Tank.Demolitionist.Concussion"));
    const float EffectiveStagger = ConcussionRank >= 2 ? 2.5f : (ConcussionRank == 1 ? 2.0f : StaggerSeconds);   // node text

    // NEAREST HONEST STAGGER (see the header): a full stop through the public
    // movement-profile mutator, restored on a timer. Not a real stagger.
    for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
    {
        ABreakerEnemy* Enemy = *It;
        if (!Enemy) continue;
        const UBreakerCombatComponent* EnemyCombat = Enemy->FindComponentByClass<UBreakerCombatComponent>();
        if (!EnemyCombat || EnemyCombat->IsDead()) continue;
        if (FVector::DistSquared(Center, Enemy->GetActorLocation()) > EnemyRadius * EnemyRadius) continue;
        Enemy->ApplyModifierMovementProfile(0.0f, -1.0f);
        TWeakObjectPtr<ABreakerEnemy> WeakEnemy(Enemy);
        FTimerHandle RestoreTimer;
        World->GetTimerManager().SetTimer(RestoreTimer, FTimerDelegate::CreateLambda([WeakEnemy]()
        {
            if (ABreakerEnemy* Restored = WeakEnemy.Get())
            {
                Restored->ApplyModifierMovementProfile(1.0f, -1.0f);
            }
        }), EffectiveStagger, false);
    }

    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

// ---------------------------------------------------------------------------
// ULTIMATE — HOLD
// ---------------------------------------------------------------------------

UBreakerAbility_Hold::UBreakerAbility_Hold()
{
    FallbackAbilityId = TEXT("Tank.Hold");
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

FName UBreakerAbility_Hold::WindowKey() { return TEXT("Window.Tank.Hold"); }

void UBreakerAbility_Hold::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    const UBreakerAbilityDefinition* Definition = GetAbilityDefinition();
    ABreakerCharacter* Character = GetBreakerCharacter();
    UWorld* World = Character ? Character->GetWorld() : nullptr;
    const float Threshold = Definition ? Definition->ResourceCost : 100.0f;
    if (!Character || !World || GetCurrentClassResource() < Threshold || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    FGameplayTagContainer OwnerTags;
    if (const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
    {
        ASC->GetOwnedGameplayTags(OwnerTags);
    }
    const FBreakerAbilityVariant Variant = Definition ? Definition->ResolveVariant(OwnerTags) : FBreakerAbilityVariant();
    const float Duration = Variant.WindowDuration > 0.0f ? Variant.WindowDuration : 10.0f;

    bVein = Variant.KeystoneTag == FGameplayTag::RequestGameplayTag(TEXT("Keystone.Tank.Vein"), false);
    bDetonation = Variant.KeystoneTag == FGameplayTag::RequestGameplayTag(TEXT("Keystone.Tank.Detonation"), false);
    const bool bWall = Variant.KeystoneTag == FGameplayTag::RequestGameplayTag(TEXT("Keystone.Tank.Wall"), false);
    AbsorbedDamage = 0.0f;

    if (UBreakerAbilityStateComponent* State = UBreakerAbilityStateComponent::FindOrAdd(Character))
    {
        State->StartWindow(WindowKey(), Duration);
    }
    // §2.1: generation triples against a raised cap. One push does both,
    // because the Grit loop multiplies its cap by the composed override.
    if (UBreakerGritComponent* Grit = Character->FindComponentByClass<UBreakerGritComponent>())
    {
        Grit->PushLoopOverride(WindowKey(), /*bSuspendDecay=*/false, GenerationMultiplier, Duration);
    }

    UBreakerCombatComponent* Combat = Character->FindComponentByClass<UBreakerCombatComponent>();
    if (Combat)
    {
        if (bVein)
        {
            // VEIN removes the cap and converts incoming damage to healing at
            // a reduced rate. EXPLICITLY NOT IMMUNITY: the hit lands first and
            // the heal is partial, so enough incoming damage still kills.
        }
        else
        {
            // The multiplicative stand-in for the per-hit cap (class comment).
            // Wall solo: doubled effectiveness on the Tank; the ally extension
            // is unreachable until a party exists.
            Combat->PushIncomingDamageModifier(WindowKey(), bWall ? WallSoloIncomingMultiplier : BaseIncomingMultiplier);
        }
        if (bVein || bDetonation)
        {
            Combat->OnDamageTaken.AddDynamic(this, &UBreakerAbility_Hold::HandleDamageTaken);
        }
        BoundCombat = Combat;
    }

    bHoldActive = true;
    World->GetTimerManager().SetTimer(WindowTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { CloseHold(); }), Duration, false);
}

void UBreakerAbility_Hold::HandleDamageTaken(const FBreakerHitContext& Hit)
{
    if (!bHoldActive) return;
    ABreakerCharacter* Character = GetBreakerCharacter();
    if (!Character || Hit.Target != Character) return;
    const float Taken = Hit.Result.HealthDamage + Hit.Result.ShieldDamage;
    if (Taken <= 0.0f) return;

    if (bVein)
    {
        // §2.1 Vein: heal at 60% of the post-mitigation amount. Instant, not
        // over 1.5s (recorded on the tunable) — and a damped attrition window
        // either way, never immunity.
        if (UBreakerCombatComponent* Combat = BoundCombat.Get())
        {
            Combat->ApplyHealingAmount(Taken * VeinHealFraction, Character, FGameplayTag());
        }
    }
    if (bDetonation)
    {
        AbsorbedDamage += Taken;
    }
}

void UBreakerAbility_Hold::CloseHold()
{
    if (CurrentActorInfo)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
    }
}

void UBreakerAbility_Hold::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (bHoldActive)
    {
        bHoldActive = false;
        ABreakerCharacter* Character = GetBreakerCharacter();

        // §2.1 Detonation: the absorbed damage releases as a radial blast —
        // 70%, 8 m, NO falloff, self-exempt (the one self-damage exemption in
        // the class, and it is on the ultimate). Released here, at the window
        // end, because the second input binding it wants does not exist.
        if (bDetonation && !bWasCancelled && AbsorbedDamage > 0.0f && Character)
        {
            BreakerTankAbilityLocal::BreakerTankRadialDamage(Character->GetWorld(), Character,
                Character->GetActorLocation(), DetonationRadiusCm,
                AbsorbedDamage * DetonationReleaseFraction, 1.0f, /*bApplyFalloff=*/false);
        }
        AbsorbedDamage = 0.0f;

        if (Character)
        {
            if (UBreakerGritComponent* Grit = Character->FindComponentByClass<UBreakerGritComponent>())
            {
                Grit->PopLoopOverride(WindowKey());
            }
            if (UBreakerAbilityStateComponent* State = Character->FindComponentByClass<UBreakerAbilityStateComponent>())
            {
                State->CloseWindow(WindowKey());
            }
        }
        if (UBreakerCombatComponent* Combat = BoundCombat.Get())
        {
            Combat->RemoveIncomingDamageModifier(WindowKey());
            Combat->OnDamageTaken.RemoveDynamic(this, &UBreakerAbility_Hold::HandleDamageTaken);
        }
        BoundCombat.Reset();
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(WindowTimer);
        }
    }
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
