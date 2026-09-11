#include "Abilities/BreakerAbility_Resonance.h"

#include "Abilities/BreakerAbilityTags.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerManaComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Combat/BreakerStatusComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "UI/BreakerEffectRenderer.h"
#include "UI/BreakerUIStyle.h"

UBreakerAbility_Resonance::UBreakerAbility_Resonance()
{
    FallbackAbilityId = TEXT("Caster.Resonance");
    // Spec §5.6: it mutates another pawn's status list, so it never runs on a
    // client.
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

    FGameplayTagContainer Tags;
    Tags.AddTag(BreakerAbilityTags::Ability_Class_Caster_Resonance.GetTag());
    SetAssetTags(Tags);
}

bool UBreakerAbility_Resonance::AcquireDetonationTarget(AActor*& OutTarget, int32& OutDistinctTypes,
    int32& OutRefundable) const
{
    OutTarget = nullptr;
    OutDistinctTypes = 0;
    OutRefundable = 0;

    const ABreakerCharacter* Character = GetBreakerCharacter();
    const UWorld* World = Character ? Character->GetWorld() : nullptr;
    if (!World) return false;

    FVector ViewLocation = Character->GetActorLocation();
    FRotator ViewRotation = Character->GetControlRotation();
    if (const AController* Controller = Character->GetController())
    {
        Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
    }

    FHitResult Hit;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BreakerResonance), false, Character);
    const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * MaximumRangeCm;
    if (World->LineTraceSingleByChannel(Hit, ViewLocation, TraceEnd, ECC_GameTraceChannel2, QueryParams))
    {
        OutTarget = Hit.GetActor();
    }

    const UBreakerStatusComponent* Status = OutTarget
        ? OutTarget->FindComponentByClass<UBreakerStatusComponent>() : nullptr;
    if (!Status) return false;
    OutDistinctTypes = Status->GetDistinctStatusTypeCount();
    for (const FBreakerActiveStatus& Active : Status->GetActiveStatuses())
        if (Active.ResourceProcCoefficient > 0.0f) ++OutRefundable;
    return OutDistinctTypes > 0;
}

bool UBreakerAbility_Resonance::PrepareCast()
{
    AActor* Target = nullptr;
    int32 DistinctTypes = 0;
    int32 Refundable = 0;
    if (!AcquireDetonationTarget(Target, DistinctTypes, Refundable))
    {
        bCastSnapshotValid = false;
        CastSnapshotTarget.Reset();
        return false;
    }
    CastSnapshotTarget = Target;
    CastSnapshotDistinctTypes = DistinctTypes;
    CastSnapshotRefundable = Refundable;
    bCastSnapshotValid = true;
    return true;
}

void UBreakerAbility_Resonance::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    // O266: the wind-up. Returns false when it has started a cast — the cost
    // is already paid, the window is open, and this function is called again
    // when the wind-up completes. Zero authored cast time is a no-op.
    if (!BeginCastIfNeeded(Handle, ActorInfo, ActivationInfo)) return;
    ABreakerCharacter* Character = GetBreakerCharacter();
    UWorld* World = Character ? Character->GetWorld() : nullptr;
    if (!World)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // THE COUNT COMES FROM THE CAST START WHEN THERE WAS ONE. Owner-ruled: the
    // payload is snapshotted like a damage-over-time source's is, or the
    // wind-up eats the very statuses it is paid from. The TARGET is snapshotted
    // with it — a cast commits to what it was aimed at, and re-tracing at the
    // landing would let the payload follow the crosshair for free.
    //
    // With no authored cast time there is no snapshot and this asks the same
    // question inline, which is what keeps a zero-cast-time build identical.
    AActor* Target = nullptr;
    int32 DistinctCount = 0;
    int32 RefundableCount = 0;
    if (bCastSnapshotValid)
    {
        bCastSnapshotValid = false;
        Target = CastSnapshotTarget.Get();
        DistinctCount = CastSnapshotDistinctTypes;
        RefundableCount = CastSnapshotRefundable;
        CastSnapshotTarget.Reset();
    }
    else if (!AcquireDetonationTarget(Target, DistinctCount, RefundableCount))
    {
        // Never charged: the refusal is the same one PrepareCast makes on the
        // cast path, for the reason Closequarter states — charging 40 Mana for
        // a press that provably cannot do anything is a dead key, not a risk
        // the design asked for.
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // THE TARGET CAN DIE OR LEAVE DURING A WIND-UP. There is no refund (O266:
    // the Mana went on the keypress and the wind-up is the risk that buys it
    // back), so this is a spent cast rather than an error.
    UBreakerStatusComponent* Status = IsValid(Target)
        ? Target->FindComponentByClass<UBreakerStatusComponent>() : nullptr;
    if (!Status || DistinctCount <= 0 || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // Read the count, then take the statuses away, then damage. Consuming
    // BEFORE the damage matters: the burst can kill, and a death that fires
    // while the status list is still populated would let a listener detonate a
    // corpse's statuses a second time.
    const UBreakerProgressionComponent* Progression = Character->GetProgression();
    const bool bPreserveStatuses = Progression && Progression->HasNodeTag(BreakerNodeTags::Node_MS_Resonance.GetTag());
    if (bConsumeStatuses && !bPreserveStatuses)
    {
        Status->ConsumeAllStatuses();
    }
    else
    {
        // MS8: the statuses survive at half duration instead of dying.
        Status->ScaleRemainingDurations(DurationScalarWhenNotConsuming);
    }

    // O35: the detonation's authored parameters are item-level-1 numbers; the
    // burst rides the equipped weapon's item-level scalar. Applied to the
    // RESULT of the curve, not its parameters, so the §2.7.5 ratio bound is
    // untouched at every item level (a common scalar cancels out of the ratio).
    const EBreakerDetonationCurve SelectedCurve = Progression && Progression->HasNodeTag(BreakerNodeTags::Node_MS_Interference.GetTag())
        ? EBreakerDetonationCurve::FixedPlusThreshold : Curve;
    const float BaseDamage = AbilityBaseDamageFor(Character,
        UBreakerStatusConsumption::DetonationDamage(DistinctCount, Detonation, SelectedCurve)
        * AbilityDamageScalarFor(Character));
    const UBreakerAttributeSet* SourceAttributes = GetBreakerAttributes();

    // THE BURST, SCALED BY THE COUNT CONSUMED: the whole ability is "consume
    // what is there", so how much was there has to be the thing you see.
    // Radius grows per distinct status against the detonation's own counted
    // cap (six), so a max detonation reads max and one status reads small.
    // Centred on the target it consumed FROM. Figures O2 PLACEHOLDER.
    // (Server-only ability, cosmetic call — see BreakerEffectRenderer.h.)
    if (ABreakerEffectRenderer* Effects = ABreakerEffectRenderer::FindOrSpawn(World))
    {
        const int32 Counted = FMath::Min(DistinctCount, Detonation.MaximumCountedStatuses);
        const FVector BurstCenter = Target->GetActorLocation() + FVector(0.0f, 0.0f, 50.0f);
        const float BurstRadius = 60.0f + 40.0f * Counted;
        BreakerFX::FEffectTiming BurstTiming;
        BurstTiming.DurationSeconds = 0.35f;
        BurstTiming.FadeOutSeconds = 0.28f;
        Effects->AddGlow(BurstCenter, BurstRadius, GetPresentationColor(), 4.5f, BurstTiming);
        Effects->AddBlinkLight(BurstCenter, 300.0f + 120.0f * Counted, GetPresentationColor(),
            2000.0f + 1200.0f * Counted, BurstTiming);
    }

    if (UBreakerCombatComponent* TargetCombat = Target->FindComponentByClass<UBreakerCombatComponent>())
    {
        FBreakerDamageRequest Damage;
        Damage.BaseDamage = BaseDamage;
        // This count-based detonation is untyped Elemental damage. It does not
        // apply Void: MS8 preserves and shortens Rot, rather than consuming it
        // through a Wither reaction after the duration rewrite.
        Damage.DamageFamily = EBreakerDamageFamily::Elemental;
        Damage.SourceTags.AddTag(BreakerAbilityTags::Ability_Class_Caster_Resonance.GetTag());
        Damage.CriticalChance = SourceAttributes ? SourceAttributes->GetCriticalChance() : UBreakerAttributeSet::DefaultCriticalChance;
        Damage.CriticalMultiplier = SourceAttributes ? SourceAttributes->GetCriticalMultiplier() : UBreakerAttributeSet::DefaultCriticalMultiplier;
        UBreakerDamageLibrary::FillSourcePools(SourceAttributes, EBreakerDamageDelivery::Ability, Damage);
        Damage.RandomSeed = HashCombine(GetTypeHash(Character), static_cast<uint32>(World->GetTimeSeconds() * 1000.0));
        Damage.SourceLocation = Character->GetActorLocation();
        Damage.bHasSourceLocation = true;
        // The burst is one hit from the caster: kill credit, Mana generation
        // and every on-hit affix hang off this being a normal damage request.
        Damage.SetInstigator(Character);
        if (UBreakerCombatComponent* OwnerCombat = Character->FindComponentByClass<UBreakerCombatComponent>())
        {
            OwnerCombat->ApplyOutgoingModifiers(Damage);
        }
        TargetCombat->ReceiveDamage(Damage);
    }

    const int32 PaymentRank = Progression ? Progression->GetNodeRank(TEXT("Caster.Multispell.Payment"), EBreakerPointCurrency::DoctrinePoints) : 0;
    const float RefundPerType = PaymentRank >= 2 ? PaymentRankTwoManaPerStatus
        : PaymentRank == 1 ? PaymentRankOneManaPerStatus : RefundManaPerStatus;
    if (RefundPerType > 0.0f)
    {
        if (UBreakerManaComponent* Mana = GetManaComponent())
        {
            // A payback, not generation: metered through the 20/s cap it would
            // read as broken, exactly as Closequarter's refund would.
            Mana->GrantMana(UBreakerStatusConsumption::RefundForConsumed(RefundableCount, RefundPerType, Detonation.MaximumCountedStatuses), true);
        }
    }

    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
