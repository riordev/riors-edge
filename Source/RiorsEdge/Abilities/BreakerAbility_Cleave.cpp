#include "Abilities/BreakerAbility_Cleave.h"

#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Abilities/BreakerAbilityTags.h"
#include "Abilities/BreakerMeleeSweep.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Combat/BreakerStatusComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "TimerManager.h"
#include "UI/BreakerEffectRenderer.h"
#include "UI/BreakerEffectMomentMath.h"
#include "UI/BreakerUIStyle.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponDefinition.h"

namespace
{
    // Salts the Bleed roll away from the damage roll so the two stay
    // independent while both remain reproducible on the server.
    constexpr uint32 BreakerCleaveBleedSalt = 0x51EAF00Du;
}

UBreakerAbility_Cleave::UBreakerAbility_Cleave()
{
    FallbackAbilityId = TEXT("Caster.Cleave");
    // Spec §5.1: it deals damage in a volume, so it never runs on a client.
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
    ActivationOwnedTags.AddTag(BreakerAbilityTags::State_Ability_Cleave.GetTag());
    // The animation lock IS the self-block: while the ability is still active
    // it owns State.Ability.Cleave, and that tag blocks its own re-activation.
    ActivationBlockedTags.AddTag(BreakerAbilityTags::State_Ability_Cleave.GetTag());

    FGameplayTagContainer Tags;
    Tags.AddTag(BreakerAbilityTags::Ability_Class_Caster_Cleave.GetTag());
    SetAssetTags(Tags);
}

FName UBreakerAbility_Cleave::SwingWindowKey()
{
    return TEXT("Window.Caster.Cleave");
}

float UBreakerAbility_Cleave::SwingDamage(float WeaponDamage, float Coefficient, float UnarmedFallback)
{
    const float Base = WeaponDamage > 0.0f ? WeaponDamage : FMath::Max(0.0f, UnarmedFallback);
    return FMath::Max(0.0f, Base * FMath::Max(0.0f, Coefficient));
}

float UBreakerAbility_Cleave::AnimationLockFor(bool bHasEdgework, float AuthoredLockSeconds)
{
    return bHasEdgework ? 0.0f : FMath::Max(0.0f, AuthoredLockSeconds);
}

float UBreakerAbility_Cleave::EffectiveArcFor(bool bHasEdge, float AuthoredArcDegrees, float AreaMultiplier)
{
    // Class-Kits §2.3 SB8: "Cleave's arc widens to 180 degrees". The rule half
    // replaces the base; the AbilityArea lane then scales whichever base
    // applies, clamped to a real arc so no stack of area nodes sweeps a
    // circle and a half.
    const float BaseArc = bHasEdge ? 180.0f : FMath::Max(0.0f, AuthoredArcDegrees);
    return FMath::Clamp(BaseArc * FMath::Max(0.0f, AreaMultiplier), 0.0f, 360.0f);
}

float UBreakerAbility_Cleave::ComputeEffectiveArcDegrees(const AActor* OwnerActor) const
{
    // Edge is read off the progression component's aggregated node tags — the
    // same register the loop valve and every rule-rewrite consumer reads — so
    // a respec that drops the node narrows the swing on the next cast.
    const UBreakerProgressionComponent* Progression = OwnerActor ? OwnerActor->FindComponentByClass<UBreakerProgressionComponent>() : nullptr;
    const bool bHasEdge = Progression && Progression->HasNodeTag(BreakerNodeTags::Node_SB_Edge.GetTag());
    return EffectiveArcFor(bHasEdge, ArcDegrees, AbilityAreaMultiplierFor(OwnerActor));
}

float UBreakerAbility_Cleave::ComputeEffectiveRangeCm(const AActor* OwnerActor) const
{
    // Range rides the same AbilityArea multiplier as the arc — the lane is a
    // geometry scale ("radius / arc / range", the enum's own naming), not an
    // area-in-square-metres promise.
    return FMath::Max(0.0f, RangeCm) * FMath::Max(0.0f, AbilityAreaMultiplierFor(OwnerActor));
}

float UBreakerAbility_Cleave::ComputeSwingBaseDamage(const AActor* OwnerActor) const
{
    // O35: the weapon-coefficient path reads the SCALED weapon base — the
    // number every weapon round already uses — instead of the raw archetype
    // constant, which stood still while weapon rounds grew (1 + w)^(ilvl - 1)
    // and turned "1.5x weapon damage" into a rounding error by ilvl 50.
    //
    // FULL BLAST, not per pellet (audit F1): the per-pellet base read a
    // shotgun at 10 against a sniper's 72, so a shotgun Caster's swing was
    // the weakest in the game — a 7.2x thematic inversion. "Weapon damage"
    // for a swing means the whole trigger pull; see the accessor for the
    // per-archetype table. Single-projectile weapons are bit-identical, so
    // nothing moves at the anchor for any pre-existing test.
    float WeaponDamage = 0.0f;
    if (const UBreakerWeaponComponent* Weapon = OwnerActor ? OwnerActor->FindComponentByClass<UBreakerWeaponComponent>() : nullptr)
    {
        if (Weapon->GetActiveDefinition())
        {
            WeaponDamage = Weapon->GetScaledFullBlastDamage();
        }
    }
    // The unarmed fallback is flat ability damage and rides the same scalar
    // (1.0 with no weapon component, so a bare Caster is bit-identical).
    const float ScaledUnarmed = UnarmedDamage * AbilityDamageScalarFor(OwnerActor);
    return SwingDamage(WeaponDamage, WeaponDamageCoefficient, ScaledUnarmed);
}

float UBreakerAbility_Cleave::GetAuthoredResourceCost() const
{
    const auto* Character = GetBreakerCharacter();
    const auto* Combat = Character ? Character->GetCombat() : nullptr;
    return Combat && Combat->HasReprisalCharge() ? 0.0f : Super::GetAuthoredResourceCost();
}

void UBreakerAbility_Cleave::CommitExecute(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
    // CommitAbility has already frozen this cast's quoted price. Claim before
    // applying its cost or broadcasting any callbacks; rejected casts never
    // reach CommitExecute and keep their still-live opportunity.
    if (auto* Character = GetBreakerCharacter())
        if (auto* Combat = Character->GetCombat()) Combat->ClaimReprisalCharge();
    Super::CommitExecute(Handle, ActorInfo, ActivationInfo);
}
void UBreakerAbility_Cleave::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    // O266: the wind-up. Returns false when it has started a cast — the cost
    // is already paid, the window is open, and this function is called again
    // when the wind-up completes. Zero authored cast time is a no-op.
    if (!BeginCastIfNeeded(Handle, ActorInfo, ActivationInfo)) return;
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

    // The arc is horizontal — a melee swing is not a downward cone, and
    // aiming at the floor must not shorten the reach.
    FVector Forward = ViewRotation.Vector();
    Forward.Z = 0.0;
    if (!Forward.Normalize())
    {
        Forward = Character->GetActorForwardVector();
    }

    FBreakerMeleeSweepParams Params;
    // Sweep from the pawn, not from the camera: 3 m is a body reach.
    Params.Origin = Character->GetActorLocation();
    Params.Forward = Forward;
    // The geometry seam: SB8 Edge's 180-degree rule and the AbilityArea
    // lane's multiplier, composed once in the accessors above. With no Edge
    // and no area ranks these are exactly the authored UPROPERTYs.
    // Edge's other half — "its Bleed applies to every target hit" — is the
    // base behaviour of the loop below already (every swept target takes the
    // 100%-chance Bleed), so the node's live payoff is the geometry.
    Params.RangeCm = ComputeEffectiveRangeCm(Character);
    Params.ArcDegrees = ComputeEffectiveArcDegrees(Character);

    const UBreakerAttributeSet* SourceAttributes = GetBreakerAttributes();

    // One item-level reading for the whole swing — every target and any bleed
    // those hits apply share it, so a swing can never straddle an equipment
    // change (the weapon path's own rule).
    const float BaseDamage = ComputeSwingBaseDamage(Character);
    const float LevelScalar = AbilityDamageScalarFor(Character);

    UBreakerCombatComponent* OwnerCombat = Character->FindComponentByClass<UBreakerCombatComponent>();
    const TArray<AActor*> Targets = UBreakerMeleeSweep::SweepTargets(World, Character, Params);

    // THE SWING, VISIBLE: the arc's outer rim in pooled strokes at the exact
    // geometry the sweep just resolved with — same Origin, same Forward, the
    // EFFECTIVE range and arc, so an Edge-widened 180 draws as 180. Strokes
    // appear left to right on a short stagger, which is what makes it read
    // as a swing rather than as a fence popping up. Lifted to mid-torso;
    // the sweep itself is height-tolerant. Timings O2 PLACEHOLDER.
    // (Server-only ability, cosmetic call — see BreakerEffectRenderer.h.)
    if (ABreakerEffectRenderer* Effects = ABreakerEffectRenderer::FindOrSpawn(World))
    {
        // ON THE GROUND, NOT AT THE EYE. Params.Origin is the capsule centre
        // and the old +60 cm put the arc within a few centimetres of the
        // first-person camera: a horizontal arc drawn at eye height, seen from
        // its own centre, PROJECTS TO A HORIZONTAL LINE no matter how much
        // geometry it has. That is the whole of "a blue line that goes across
        // your screen", and dropping it to the chest only moves the line down.
        //
        // At the feet the same arc is seen from above, so it reads as what it
        // is: a fan opening across the floor in front of you. This is also
        // O179's camera law — a self-anchored draw sits at the feet, off the
        // axis of the one camera guaranteed to be standing in it.
        const float FeetDrop = Character->GetCapsuleComponent()
            ? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88.0f;
        const FVector ArcOrigin = Params.Origin - FVector(0.0f, 0.0f, FeetDrop - 8.0f);
        const FLinearColor Paint = GetPresentationColor();
        BreakerFX::FEffectTiming SwingTiming;
        SwingTiming.DurationSeconds = 0.22f;
        SwingTiming.FadeInSeconds = 0.02f;
        SwingTiming.FadeOutSeconds = 0.12f;
        const float SweepSeconds = 0.16f;
        for (int32 Ring = 0; Ring < BreakerFX::SweptArcRings; ++Ring)
        {
            const float Radius = Params.RangeCm * BreakerFX::SweptArcRingFraction(Ring);
            const float Thickness = BreakerFX::SweptArcRingThickness(Ring, 7.0f);
            // The near rings live a little shorter, so what is left at the end
            // of the swing is the edge and not a stack of three.
            BreakerFX::FEffectTiming RingTiming = SwingTiming;
            RingTiming.DurationSeconds = SwingTiming.DurationSeconds * (0.7f + 0.15f * Ring);
            for (int32 Index = 0; Index < BreakerFX::SweptArcStrokes; ++Index)
            {
                FVector A, B;
                BreakerFX::ArcStroke(ArcOrigin, Params.Forward, Params.ArcDegrees, Radius,
                    Index, BreakerFX::SweptArcStrokes, A, B);
                Effects->AddStroke(A, B, Thickness, Paint, 3.0f, RingTiming,
                    BreakerFX::SweptArcRingDelay(Ring, SweepSeconds)
                        + SweepSeconds * Index / BreakerFX::SweptArcStrokes);
            }
        }
        // AND THE FAN HAS RIBS. Four spokes from the inner ring out to the
        // edge, swept in the same left-to-right order as the arcs: without
        // them three concentric arcs are three lines, and with them the swing
        // is one shape that opens.
        for (int32 Spoke = 0; Spoke <= 4; ++Spoke)
        {
            const FVector Inner = BreakerFX::ArcVertex(ArcOrigin, Params.Forward, Params.ArcDegrees,
                Params.RangeCm * BreakerFX::SweptArcRingFraction(0), Spoke * 2, BreakerFX::SweptArcStrokes);
            const FVector Outer = BreakerFX::ArcVertex(ArcOrigin, Params.Forward, Params.ArcDegrees,
                Params.RangeCm, Spoke * 2, BreakerFX::SweptArcStrokes);
            Effects->AddStroke(Inner, Outer, 3.0f, Paint, 2.2f, SwingTiming,
                SweepSeconds * (Spoke * 2) / BreakerFX::SweptArcStrokes);
        }
        // The swing starts at the hand: the cast moment is the character's
        // (O284, HandleAbilityCast on OnAbilityActivated), one burst for
        // every ability, so this composition draws the arc and nothing at
        // the caster.
    }

    int32 TargetIndex = 0;
    for (AActor* Target : Targets)
    {
        UBreakerCombatComponent* TargetCombat = Target ? Target->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
        if (!TargetCombat)
        {
            continue;
        }

        FBreakerDamageRequest Damage;
        Damage.BaseDamage = BaseDamage;
        Damage.DamageFamily = EBreakerDamageFamily::Physical;
        // Damage.Melee is what the Spellblade 1.30x More and Melee Damage %
        // affixes gate on; without it a melee hit is indistinguishable from a
        // bullet downstream.
        Damage.SourceTags.AddTag(BreakerAbilityTags::Damage_Melee.GetTag());
        Damage.SourceTags.AddTag(BreakerAbilityTags::Ability_Class_Caster_Cleave.GetTag());
        Damage.CriticalChance = SourceAttributes ? SourceAttributes->GetCriticalChance() : UBreakerAttributeSet::DefaultCriticalChance;
        Damage.CriticalMultiplier = SourceAttributes ? SourceAttributes->GetCriticalMultiplier() : UBreakerAttributeSet::DefaultCriticalMultiplier;
        // O55: Cleave SWINGS THE EQUIPPED WEAPON, so it is weapon-delivered
        // and draws the weapon pool — the rule is decided by what delivers
        // the damage, never by the fact that an ability triggered it. This is
        // the case the rule exists to pre-answer, and the Damage_Melee tag
        // above is the same reading at the affix layer.
        UBreakerDamageLibrary::FillSourcePools(SourceAttributes, EBreakerDamageDelivery::Weapon, Damage);
        Damage.RandomSeed = HashCombine(GetTypeHash(Character), static_cast<uint32>(TargetIndex) + static_cast<uint32>(World->GetTimeSeconds() * 1000.0));
        Damage.SourceLocation = Params.Origin;
        Damage.bHasSourceLocation = true;
        Damage.SetInstigator(Character);
        if (OwnerCombat)
        {
            OwnerCombat->ApplyOutgoingModifiers(Damage);
        }
        const FBreakerDamageResult Result = TargetCombat->ReceiveDamage(Damage);

        // Class-Kits §2.2 C1: Bleed at a 100% base chance — no roll at all.
        if (!Result.bDodged && !Result.bParried && !Result.bKilled && !TargetCombat->IsDead()
            && Result.HealthDamage + Result.ShieldDamage > 0.0f)
            ApplyCleaveBleed(Target, SourceAttributes, OwnerCombat, LevelScalar, TargetIndex);
        ++TargetIndex;
    }

    // "Rewrites Unmake: DURING IT, Cleave has no animation lock" — the node
    // text and Class-Kits §2.2 both scope the rewrite to Unmake's window. The
    // keystone tag alone is published permanently by node purchase, so gating
    // on the tag alone shipped a permanent uncapped Cleave-spam buff as an
    // ultimate rewrite: buying Edgework removed the lock forever. Both halves
    // are required — the tag says the rewrite is owned, the window says it is
    // currently rewriting.
    UBreakerAbilityStateComponent* State = UBreakerAbilityStateComponent::FindOrAdd(Character);
    const bool bHasEdgework = ActorInfo && ActorInfo->AbilitySystemComponent.IsValid()
        && ActorInfo->AbilitySystemComponent->HasMatchingGameplayTag(BreakerAbilityTags::Keystone_Caster_Edgework.GetTag());
    const bool bDuringUnmake = State && State->IsWindowActive(UBreakerCasterAbility::UnmakeWindowKey());
    const float Lock = AnimationLockFor(bHasEdgework && bDuringUnmake, AnimationLockSeconds);

    if (State)
    {
        // Published for the HUD and for cosmetics; the ability's own lock is
        // the GAS activation, not this window.
        State->StartWindow(SwingWindowKey(), FMath::Max(Lock, 0.05f));
    }

    if (Lock <= 0.0f)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    TWeakObjectPtr<UBreakerAbility_Cleave> WeakThis(this);
    World->GetTimerManager().SetTimer(LockTimer, FTimerDelegate::CreateLambda([WeakThis]()
    {
        if (UBreakerAbility_Cleave* Ability = WeakThis.Get())
        {
            Ability->EndAbility(Ability->CurrentSpecHandle, Ability->CurrentActorInfo, Ability->CurrentActivationInfo, true, false);
        }
    }), Lock, false);
}

void UBreakerAbility_Cleave::ApplyCleaveBleed(AActor* Target, const UBreakerAttributeSet* SourceAttributes, const UBreakerCombatComponent* OwnerCombat, float LevelScalar, int32 Salt) const
{
    UBreakerStatusComponent* Status = Target ? Target->FindComponentByClass<UBreakerStatusComponent>() : nullptr;
    if (!Status || BleedDamagePerTick <= 0.0f || BleedDuration <= 0.0f)
    {
        return;
    }

    FBreakerStatusApplicationSpec Spec;
    Spec.StatusTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Bleed"), false);
    Spec.Snapshot.SourceTags.AddTag(BreakerAbilityTags::Ability_Class_Caster_Cleave.GetTag());
    // O35: the per-tick base rides the equipped weapon's item-level scalar,
    // exactly as the weapon's own bleed does. Item level 1 is x1.0.
    Spec.BaseDamagePerTick = BleedDamagePerTick * FMath::Max(0.0f, LevelScalar);
    Spec.Duration = BleedDuration;
    Spec.TickInterval = FMath::Max(0.05f, BleedTickInterval);
    // DoTs snapshot at APPLICATION, and the snapshot now includes the outgoing
    // chain's budgeted window product: a bleed applied inside an Overdrive-like
    // window keeps that strength for its whole life, one applied outside never
    // gains it retroactively.
    // Weapon-delivered, like the swing that applied it (O55).
    Spec.Snapshot.SourcePower = UBreakerCombatComponent::ComposeDotSourcePower(SourceAttributes, OwnerCombat,
        EBreakerDamageDelivery::Weapon);
    Spec.Snapshot.CriticalChance = SourceAttributes ? SourceAttributes->GetCriticalChance() : UBreakerAttributeSet::DefaultCriticalChance;
    Spec.Snapshot.CriticalMultiplier = SourceAttributes ? SourceAttributes->GetCriticalMultiplier() : UBreakerAttributeSet::DefaultCriticalMultiplier;
    Spec.Snapshot.DamageOverTimeMultiplier = SourceAttributes ? SourceAttributes->GetDamageOverTimeMultiplier() : 1.0f;

    const ABreakerCharacter* Character = GetBreakerCharacter();
    FRandomStream Stream(static_cast<int32>(HashCombine(HashCombine(GetTypeHash(Character), static_cast<uint32>(Salt)), BreakerCleaveBleedSalt)));
    // Snapshot criticals: one roll at application decides every tick of this
    // application, exactly as the weapon path does it.
    Spec.Snapshot.CriticalRollSample = Stream.FRand();
    Spec.Snapshot.bHasCriticalRollSample = true;
    Spec.Snapshot.bRolledCritical = Spec.Snapshot.CriticalRollSample < Spec.Snapshot.CriticalChance;

    Status->ApplyStatus(Spec, EBreakerDamageFamily::Physical, const_cast<ABreakerCharacter*>(Character));
}
