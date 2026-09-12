#include "Abilities/BreakerSupportAbilities.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerChargeComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerRangedEnemy.h"
#include "Combat/BreakerWardenEnemy.h"
#include "Combat/BreakerStatusComponent.h"
#include "Combat/BreakerZoneActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameplayEffect.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "TimerManager.h"
#include "UI/BreakerEffectRenderer.h"
#include "UI/BreakerUIStyle.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Movement/BreakerCharacterMovementComponent.h"

namespace BreakerSupportAbilityLocal
{
    // Feet-anchored cast flash (the camera law: never wrap a primitive around
    // the one camera guaranteed to stand in it). Cyan for the system/tempo
    // verbs, violet for the ultimate below. Figures O2 PLACEHOLDER.
    // (Server-only abilities, cosmetic calls — see BreakerEffectRenderer.h.)
    void BreakerSupportCastFlash(ABreakerCharacter* Character, const FLinearColor& Color, float RadiusCm)
    {
        UWorld* World = Character ? Character->GetWorld() : nullptr;
        ABreakerEffectRenderer* Effects = World ? ABreakerEffectRenderer::FindOrSpawn(World) : nullptr;
        if (!Effects) return;
        const FVector Feet = Character->GetActorLocation() - FVector(0.0f, 0.0f, Character->GetSimpleCollisionHalfHeight() * 0.8f);
        BreakerFX::FEffectTiming CastTiming;
        CastTiming.DurationSeconds = 0.30f;
        CastTiming.FadeInSeconds = 0.02f;
        CastTiming.FadeOutSeconds = 0.22f;
        Effects->AddGlow(Feet, RadiusCm, Color, 2.8f, CastTiming);
    }

    // Prefixed for the unity build, per house rule.

    // The target's maximum health, for percentage-of-target healing (§U1) and
    // marked-damage generation (§1.1). Null-safe: 0 when unreadable.
    float BreakerSupportTargetMaxHealth(const AActor* Target)
    {
        if (const ABreakerCharacter* Breaker = Cast<ABreakerCharacter>(Target))
        {
            const UBreakerAttributeSet* Attributes = Breaker->GetAttributes();
            return Attributes ? Attributes->GetMaxHealth() : 0.0f;
        }
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

    // Current health fraction of a target, 1.0 when unreadable — reading a
    // stranger as "full" makes every below-full rule fail closed.
    float BreakerSupportTargetHealthFraction(const AActor* Target)
    {
        const UBreakerAttributeSet* Attributes = nullptr;
        if (const ABreakerCharacter* Breaker = Cast<ABreakerCharacter>(Target))
        {
            Attributes = Breaker->GetAttributes();
        }
        else if (const IAbilitySystemInterface* AbilityOwner = Cast<IAbilitySystemInterface>(Target))
        {
            if (const UAbilitySystemComponent* ASC = AbilityOwner->GetAbilitySystemComponent())
            {
                Attributes = ASC->GetSet<UBreakerAttributeSet>();
            }
        }
        if (!Attributes || Attributes->GetMaxHealth() <= 0.0f) return 1.0f;
        return FMath::Clamp(Attributes->GetHealth() / Attributes->GetMaxHealth(), 0.0f, 1.0f);
    }

    // The one heal-and-credit seam every Medic path uses (instant, HoT tick,
    // Triage pulse). Resolves the heal, then credits Charge EXPLICITLY at the
    // stated proc coefficient — inside the crediting scope, so the component's
    // MD1 listener stands down and nothing pays twice. bOverflow adds MD9's
    // conversion: unrouted overheal returns as shield at half value, paying
    // through the shielding source, never through the overheal it came from.
    FBreakerHealResult BreakerSupportHealAndCredit(ABreakerCharacter* Healer, AActor* Target,
        float Amount, float ProcCoefficient, bool bOverflow, bool bOverhealToShield = false)
    {
        FBreakerHealResult Result;
        UBreakerCombatComponent* TargetCombat = Target ? Target->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
        if (!TargetCombat || Amount <= 0.0f) return Result;
        const float TargetMax = BreakerSupportTargetMaxHealth(Target);
        UBreakerChargeComponent* Charge = Healer ? Healer->FindComponentByClass<UBreakerChargeComponent>() : nullptr;
        const bool bSelf = Target == Healer;

        if (Charge) Charge->BeginSupportHealScope();
        FBreakerHealRequest Heal;
        Heal.Amount = Amount;
        Heal.ProcCoefficient = ProcCoefficient;
        Heal.bOverhealToShield = bOverhealToShield;
        Heal.SetHealer(Healer);
        Result = TargetCombat->ApplyHealing(Heal);

        // MD9 OVERFLOW: overheal is no longer discarded — it becomes shield at
        // a fraction of its value, through the one healing path (a second
        // request against a now-full bar routes wholly to shield).
        float OverflowShield = 0.0f;
        if (bOverflow)
        {
            const float Unrouted = Result.Overheal - Result.ShieldGranted;
            if (Unrouted > 0.0f)
            {
                FBreakerHealRequest Convert;
                Convert.ProcCoefficient = ProcCoefficient;
                Convert.Amount = Unrouted * 0.5f;   // O2 PLACEHOLDER ("a fraction of its value")
                Convert.bOverhealToShield = true;
                Convert.bConversionOnly = true;
                Convert.SetHealer(Healer);
                OverflowShield = TargetCombat->ApplyHealing(Convert).ShieldGranted;
            }
        }
        if (Charge) Charge->EndSupportHealScope();

        // Self-heals credit here, explicitly, at the true proc coefficient.
        // Ally heals credit through the character's OnHealingDealt wiring
        // with the same carried proc coefficient. Overheal pays nothing; the shield it
        // became pays as shield — MD9's exact sentence.
        if (Charge && bSelf)
        {
            if (Result.HealthHealed > 0.0f || Result.Overheal > 0.0f)
            {
                Charge->NotifyHealingDone(Result.HealthHealed, Result.Overheal, TargetMax, true, ProcCoefficient);
            }
            const float ShieldPaid = Result.ShieldGranted + OverflowShield;
            if (ShieldPaid > 0.0f)
            {
                Charge->NotifyShieldingDone(ShieldPaid * (FMath::IsFinite(ProcCoefficient) ? FMath::Clamp(ProcCoefficient, 0.0f, 1.0f) : 0.0f), 0.0f, TargetMax, true);
            }
        }
        // Attending pays actual restored health at the real heal proc weight.
        // Every healing verb shares this seam, including deferred HoT pulses.
        if (Charge && Healer && !Healer->GetCombat()->IsDead() && Result.HealthHealed > 0
            && FMath::IsFinite(ProcCoefficient) && ProcCoefficient > 0)
        {
            const auto* Progression = Healer->FindComponentByClass<UBreakerProgressionComponent>();
            const int32 Rank = Progression ? Progression->GetNodeRank(TEXT("Support.Medic.Attending"), EBreakerPointCurrency::DoctrinePoints) : 0;
            if (auto* State = Healer->FindComponentByClass<UBreakerAbilityStateComponent>())
                if (AActor* Marked = State->GetMarkedTarget(); Rank > 0 && Marked)
                {
                    Charge->NotifyMarkedTargetDamage(Result.HealthHealed * FMath::Clamp(ProcCoefficient, 0.0f, 1.0f), BreakerSupportTargetMaxHealth(Marked));
                    if (Rank >= 1)
                        if (auto* ASC = Healer->GetAbilitySystemComponent())
                            for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
                                if (auto* Mark = Cast<UBreakerAbility_Mark>(Spec.GetPrimaryInstance())) Mark->RefreshDuration(10.0f);   // O2 PLACEHOLDER
                }
        }
        return Result;
    }
}

// ---------------------------------------------------------------------------
// The Support base
// ---------------------------------------------------------------------------

FName UBreakerSupportAbility::ConduitWindowKey() { return TEXT("Window.Support.Conduit"); }

float UBreakerSupportAbility::CostUnderConduit(float AuthoredCost, float WindowScalar)
{
    return FMath::Max(0.0f, AuthoredCost * FMath::Max(0.0f, WindowScalar));
}

float UBreakerSupportAbility::GetUnmodifiedResourceCost() const
{
    const float Authored = Super::GetUnmodifiedResourceCost();
    const ABreakerCharacter* Character = GetBreakerCharacter();
    const UBreakerAbilityStateComponent* State = Character ? Character->FindComponentByClass<UBreakerAbilityStateComponent>() : nullptr;
    if (!State) return Authored;
    // Payload 1.0 when no window is open; 0.0 under base CONDUIT ("costs
    // nothing"); 1.0 under Triage/Blackout, which replace casting rather than
    // enabling it. CheckCost and ApplyCost both read through here, so
    // affordability and the spend cannot disagree (the Caster precedent).
    const float Scalar = State->GetWindowPayload(ConduitWindowKey(), 1.0f);
    return CostUnderConduit(Authored, Scalar);
}

AActor* UBreakerSupportAbility::ResolveAllyTarget(ABreakerCharacter* Caster, float RangeCm)
{
    // §3: the ally under the crosshair, or SELF with no target. With no party
    // layer this trace can only ever find another spawned ABreakerCharacter
    // (a second PIE pawn), so solo it resolves to self every time — the case
    // §2 makes first-class. One path, no self-discount, no self-bonus.
    if (!Caster) return nullptr;
    UWorld* World = Caster->GetWorld();
    if (!World) return Caster;

    FVector ViewLocation = Caster->GetActorLocation();
    FRotator ViewRotation = Caster->GetControlRotation();
    if (const AController* Controller = Caster->GetController())
    {
        Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
    }
    FHitResult Hit;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BreakerSupportAim), false, Caster);
    if (World->LineTraceSingleByChannel(Hit, ViewLocation, ViewLocation + ViewRotation.Vector() * RangeCm, ECC_Pawn, QueryParams))
    {
        if (ABreakerCharacter* Ally = Cast<ABreakerCharacter>(Hit.GetActor()))
        {
            return Ally;
        }
    }
    return Caster;
}

int32 UBreakerSupportAbility::SupportNodeRank(const ABreakerCharacter* Character, const TCHAR* NodeId)
{
    const UBreakerProgressionComponent* Progression = Character ? Character->FindComponentByClass<UBreakerProgressionComponent>() : nullptr;
    return Progression ? Progression->GetNodeRank(FName(NodeId), EBreakerPointCurrency::DoctrinePoints) : 0;
}

bool UBreakerSupportAbility::SupportHasNode(const ABreakerCharacter* Character, const FGameplayTag& Tag)
{
    const UBreakerProgressionComponent* Progression = Character ? Character->FindComponentByClass<UBreakerProgressionComponent>() : nullptr;
    return Progression && Progression->HasNodeTag(Tag);
}

void UBreakerSupportAbility::ShaveOwnCooldownSeconds(float Seconds) const
{
    if (Seconds <= 0.0f) return;
    UAbilitySystemComponent* ASC = CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get() : nullptr;
    const FGameplayTagContainer* Tags = GetCooldownTags();
    if (!ASC || !Tags || Tags->IsEmpty()) return;
    // Moving a cooldown effect's start time BACK shortens its remaining
    // duration by the same amount — the honest GAS shave.
    const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(*Tags);
    for (const FActiveGameplayEffectHandle& Handle : ASC->GetActiveEffects(Query))
    {
        ASC->ModifyActiveEffectStartTime(Handle, -Seconds);
    }
}

void UBreakerSupportAbility::ClearOwnCooldown() const
{
    UAbilitySystemComponent* ASC = CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get() : nullptr;
    const FGameplayTagContainer* Tags = GetCooldownTags();
    if (!ASC || !Tags || Tags->IsEmpty()) return;
    ASC->RemoveActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(*Tags));
}

void UBreakerSupportAbility::RefreshBuffUptime(ABreakerCharacter* Character)
{
    if (!Character) return;
    UBreakerChargeComponent* Charge = Character->FindComponentByClass<UBreakerChargeComponent>();
    const UBreakerAbilityStateComponent* State = Character->FindComponentByClass<UBreakerAbilityStateComponent>();
    if (!Charge || !State) return;
    // A BOOL by construction: two live buffs pay exactly what one pays.
    const bool bAnyBuff = false; // Maintained source keys own generation; receiving a buff earns no upkeep.
    Charge->SetAnyBuffActive(bAnyBuff);
}

// ---------------------------------------------------------------------------
// U1 — PATCH
// ---------------------------------------------------------------------------

UBreakerAbility_Patch::UBreakerAbility_Patch()
{
    FallbackAbilityId = TEXT("Support.Patch");
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

float UBreakerAbility_Patch::GetUnmodifiedResourceCost() const
{
    // MD11 NO TRIAGE: far cheaper (half, O2 PLACEHOLDER). The self-only and
    // shorter-cooldown halves live in ActivateAbility.
    const float Authored = Super::GetUnmodifiedResourceCost();
    return SupportHasNode(GetBreakerCharacter(), BreakerNodeTags::Node_MD_NoTriage.GetTag()) ? Authored * 0.5f : Authored;
}

void UBreakerAbility_Patch::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    using namespace BreakerSupportAbilityLocal;
    ABreakerCharacter* Character = GetBreakerCharacter();
    if (!Character || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // MD11 NO TRIAGE: self-only — and the cooldown that just started is
    // half-shaved, the per-ability scope the class-wide lane cannot reach.
    const bool bNoTriage = SupportHasNode(Character, BreakerNodeTags::Node_MD_NoTriage.GetTag());
    if (bNoTriage) ShaveOwnCooldownSeconds(GetCooldownSeconds() * 0.5f);   // O2 PLACEHOLDER

    AActor* Target = bNoTriage ? Character : ResolveAllyTarget(Character, TargetRangeCm);
    const float TargetMaxHealth = BreakerSupportAbilityLocal::BreakerSupportTargetMaxHealth(Target);
    if (Target && TargetMaxHealth > 0.0f)
    {
        // §U1: a percentage of the TARGET'S maximum health, so it is equally
        // meaningful on a Tank and on a Caster. Overheal is discarded unless a
        // node says otherwise — the heal result reports it separately and the
        // crediting seam pays only the effective half (the loop's single most
        // load-bearing rule).
        float Amount = TargetMaxHealth * HealFractionOfTargetMax;

        // MD2 TRIAGE PRIORITY: harder the further below full the target is,
        // less on the healthy, at equal total throughput across the band.
        // The same health curve scales Field Kit's Purge immunity duration
        // (single-rank node, O272).
        if (SupportNodeRank(Character, TEXT("Support.Medic.TriagePriority")) > 0)
        {
            const float Missing = 1.0f - BreakerSupportTargetHealthFraction(Target);
            Amount *= FMath::Lerp(0.6f, 1.4f, Missing);   // O2 PLACEHOLDER, symmetric about 1.0
        }

        const bool bOverflow = SupportHasNode(Character, BreakerNodeTags::Node_MD_Overflow.GetTag());
        const bool bTargetFull = BreakerSupportTargetHealthFraction(Target) >= 1.0f - KINDA_SMALL_NUMBER;
        const int32 SecondOpinionRank = SupportNodeRank(Character, TEXT("Support.Medic.SecondOpinion"));

        if (SecondOpinionRank > 0 && bTargetFull)
        {
            // MD5 SECOND OPINION: Patch on a full-health target grants a shield
            // instead, paying from the SHIELDING source (the full-bar request
            // routes wholly to shield; the credit seam pays shield as shield).
            const float ShieldAmount = Amount * 0.5f;   // O2 PLACEHOLDER
            BreakerSupportHealAndCredit(Character, Target, ShieldAmount, 1.0f, /*bOverflow=*/false, /*bOverhealToShield=*/true);
            // Half of it echoes onto you (single-rank node, O272). Solo
            // (target == self) the echo is vacuous by construction, exactly
            // as the treatment guards.
            if (SecondOpinionRank >= 1 && Target != Character)
            {
                BreakerSupportHealAndCredit(Character, Character, ShieldAmount * 0.5f, 1.0f, false, true);
            }
        }
        else if (SupportHasNode(Character, BreakerNodeTags::Node_MD_SustainedCare.GetTag()))
        {
            // MD8 SUSTAINED CARE: an instant portion and a heal-over-time whose
            // ticks pay Charge at proc coefficient, never at full rate — so the
            // split cannot out-generate the instant it replaces.
            const float InstantAmount = Amount * 0.5f;   // O2 PLACEHOLDER
            const int32 TickCount = 4;                   // O2 PLACEHOLDER
            const float TickAmount = (Amount - InstantAmount) / TickCount;
            const float TickProcCoefficient = 0.5f;      // O2 PLACEHOLDER, sub-1.0 by rule
            BreakerSupportHealAndCredit(Character, Target, InstantAmount, 1.0f, bOverflow);
            if (UWorld* World = Character->GetWorld())
            {
                TWeakObjectPtr<ABreakerCharacter> WeakHealer(Character);
                TWeakObjectPtr<AActor> WeakTarget(Target);
                for (int32 Tick = 1; Tick <= TickCount; ++Tick)
                {
                    FTimerHandle TickTimer;
                    World->GetTimerManager().SetTimer(TickTimer, FTimerDelegate::CreateLambda(
                        [WeakHealer, WeakTarget, TickAmount, TickProcCoefficient, bOverflow]()
                    {
                        if (ABreakerCharacter* Healer = WeakHealer.Get())
                        {
                            if (AActor* HealTarget = WeakTarget.Get())
                            {
                                BreakerSupportAbilityLocal::BreakerSupportHealAndCredit(Healer, HealTarget, TickAmount, TickProcCoefficient, bOverflow);
                            }
                        }
                    }), static_cast<float>(Tick), false);
                }
            }
        }
        else
        {
            BreakerSupportHealAndCredit(Character, Target, Amount, 1.0f, bOverflow);
        }

        // THE HEAL, VISIBLE: a gold pulse and a short rising stroke at the
        // HEALED actor -- self or ally, wherever the value landed. Gold is
        // the reward family and a heal is a payment received. Drawn only
        // when a target resolved, so a whiffed ally-cast (which still
        // committed) shows nothing it did not do. Figures O2 PLACEHOLDER.
        // (Server-only, cosmetic call -- see BreakerEffectRenderer.h.)
        if (ABreakerEffectRenderer* Effects = ABreakerEffectRenderer::FindOrSpawn(Character->GetWorld()))
        {
            BreakerFX::FEffectTiming HealTiming;
            HealTiming.DurationSeconds = 0.35f;
            HealTiming.FadeInSeconds = 0.05f;
            HealTiming.FadeOutSeconds = 0.25f;
            if (Target == Character)
            {
                // SELF-CAST DRAWS AT THE FEET, off-axis: the ally composition
                // below, placed at the caster's own chest, rises straight
                // through the first-person camera and fills the screen with a
                // gold pillar (the Support probe photographed it). Same
                // lesson, third site: never wrap a primitive around the one
                // camera guaranteed to be standing in it.
                const FVector Feet = Character->GetActorLocation() - FVector(0.0f, 0.0f, Character->GetSimpleCollisionHalfHeight() * 0.8f);
                Effects->AddGlow(Feet, 40.0f, BreakerUI::Gold, 2.6f, HealTiming);
                const FVector Side = FVector::CrossProduct(Character->GetControlRotation().Vector(), FVector::UpVector).GetSafeNormal();
                Effects->AddStroke(Feet + Side * 50.0f, Feet + Side * 55.0f + FVector(0, 0, 70.0f), 3.0f, BreakerUI::Gold, 2.2f, HealTiming, 0.05f);
                Effects->AddStroke(Feet - Side * 50.0f, Feet - Side * 55.0f + FVector(0, 0, 70.0f), 3.0f, BreakerUI::Gold, 2.2f, HealTiming, 0.05f);
            }
            else
            {
                const FVector Chest = Target->GetActorLocation() + FVector(0.0f, 0.0f, 25.0f);
                Effects->AddGlow(Chest, 32.0f, BreakerUI::Gold, 3.0f, HealTiming);
                Effects->AddStroke(Chest + FVector(0, 0, 10.0f), Chest + FVector(0, 0, 85.0f), 3.0f, BreakerUI::Gold, 2.4f, HealTiming, 0.05f);
            }
        }
    }

    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

// ---------------------------------------------------------------------------
// U2 — PURGE
// ---------------------------------------------------------------------------

UBreakerAbility_Purge::UBreakerAbility_Purge()
{
    FallbackAbilityId = TEXT("Support.Purge");
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

float UBreakerAbility_Purge::GetUnmodifiedResourceCost() const
{
    // MD11 NO TRIAGE, the Patch twin: far cheaper (half, O2 PLACEHOLDER).
    const float Authored = Super::GetUnmodifiedResourceCost();
    return SupportHasNode(GetBreakerCharacter(), BreakerNodeTags::Node_MD_NoTriage.GetTag()) ? Authored * 0.5f : Authored;
}

void UBreakerAbility_Purge::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    using namespace BreakerSupportAbilityLocal;
    ABreakerCharacter* Character = GetBreakerCharacter();
    if (!Character || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // MD11 NO TRIAGE: self-only, shorter cooldown — the Patch twin.
    const bool bNoTriage = SupportHasNode(Character, BreakerNodeTags::Node_MD_NoTriage.GetTag());
    if (bNoTriage) ShaveOwnCooldownSeconds(GetCooldownSeconds() * 0.5f);   // O2 PLACEHOLDER

    AActor* Target = bNoTriage ? Character : ResolveAllyTarget(Character, TargetRangeCm);
    if (UBreakerStatusComponent* Status = Target ? Target->FindComponentByClass<UBreakerStatusComponent>() : nullptr)
    {
        // Distinct TYPES, never stacks — a 10-stack Bleed cleansed is one
        // status removed for generation (§1.1's cleanse row).
        const int32 DistinctRemoved = Status->GetDistinctStatusTypeCount();
        Status->ConsumeAllStatuses();
        if (DistinctRemoved > 0)
        {
            if (UBreakerChargeComponent* Charge = Character->FindComponentByClass<UBreakerChargeComponent>())
            {
                Charge->NotifyStatusCleansed(DistinctRemoved);
            }
            // MD3 CLEAN HANDS: cooldown refunds per status ACTUALLY removed,
            // bounded by the cleanse source's own 0.5s ICD — which the Charge
            // component's cleanse interval already enforces. Single-rank
            // node (O272): rank one carries the full refund.
            const int32 CleanHandsRank = SupportNodeRank(Character, TEXT("Support.Medic.CleanHands"));
            if (CleanHandsRank >= 1)
            {
                const float RefundPerStatus = 2.0f;   // O2 PLACEHOLDER
                ShaveOwnCooldownSeconds(RefundPerStatus * DistinctRemoved);
            }
        }
        // MD7 FIELD KIT's immunity half: the window now BLOCKS new statuses
        // from landing, through the status component's immunity primitive
        // (built for this node; the base §U2 immunity stays a recorded gap for
        // a build without it). The enemy buff-strip half WAITS: enemies carry
        // no buff a Purge could strip.
        if (SupportHasNode(Character, BreakerNodeTags::Node_MD_FieldKit.GetTag()))
        {
            const UBreakerAbilityDefinition* Definition = GetAbilityDefinition();
            float Duration = Definition && Definition->WindowDuration > 0.0f ? Definition->WindowDuration : 3.0f;
            if (SupportNodeRank(Character, TEXT("Support.Medic.TriagePriority")) >= 1)
                Duration *= FMath::Lerp(0.6f, 1.4f, 1.0f - BreakerSupportTargetHealthFraction(Target));   // O2 PLACEHOLDER
            Status->GrantStatusImmunity(Duration);
        }

        // THE CLEANSE, VISIBLE: a cyan wash and a rising ring of short
        // strokes shrugging off the target -- cyan is the player/system
        // family, and a cleanse is the system scrubbing something OFF, not a
        // payment. Intensity does not scale with statuses removed; a zero
        // cleanse still bought the immunity-shaped moment the cast paid for.
        // Figures O2 PLACEHOLDER. (Server-only, cosmetic call -- see
        // BreakerEffectRenderer.h.)
        if (ABreakerEffectRenderer* Effects = ABreakerEffectRenderer::FindOrSpawn(Character->GetWorld()))
        {
            const FVector Base = Target->GetActorLocation();
            BreakerFX::FEffectTiming CleanseTiming;
            CleanseTiming.DurationSeconds = 0.35f;
            CleanseTiming.FadeInSeconds = 0.03f;
            CleanseTiming.FadeOutSeconds = 0.28f;
            // Self-cast: no centre glow (the camera stands in it — Patch's
            // photographed lesson); the shrug ring alone, wider so its
            // strokes sit outside the first-person view's near plane.
            const bool bSelf = Target == Character;
            if (!bSelf)
            {
                Effects->AddGlow(Base + FVector(0, 0, 40.0f), 42.0f, BreakerUI::Cyan, 2.6f, CleanseTiming);
            }
            const float RingCm = bSelf ? 75.0f : 45.0f;
            for (int32 Index = 0; Index < 6; ++Index)
            {
                const FVector Out = FRotator(0.0f, 60.0f * Index, 0.0f).Vector();
                Effects->AddStroke(Base + Out * RingCm + FVector(0, 0, 15.0f),
                    Base + Out * (RingCm + 15.0f) + FVector(0, 0, 95.0f), 3.0f, BreakerUI::Cyan, 2.2f, CleanseTiming, 0.02f * Index);
            }
        }
    }

    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

// ---------------------------------------------------------------------------
// U3 — CADENCE
// ---------------------------------------------------------------------------

UBreakerAbility_Cadence::UBreakerAbility_Cadence()
{
    FallbackAbilityId = TEXT("Support.Cadence");
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
    // CO4 Rehearsal exists: re-applying a live buff must be a legal cast (it
    // refreshes and refunds). Retriggering ends the running window through the
    // ordinary teardown, then re-activates — one code path for both casts.
    bRetriggerInstancedAbility = true;
}

FName UBreakerAbility_Cadence::WindowKey() { return TEXT("Window.Support.Cadence"); }

void UBreakerAbility_Cadence::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    ABreakerCharacter* Character = GetBreakerCharacter();
    UWorld* World = Character ? Character->GetWorld() : nullptr;
    if (!World || Character->GetCombat()->IsDead() || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }
    ClearTempoTails();
    bAfterimageAtCast = SupportHasNode(Character, FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Afterimage")));
    Character->GetProgression()->OnProgressionChanged.AddUniqueDynamic(this, &ThisClass::HandleCadenceProgressionChanged);
    const int32 Rehearsal = SupportNodeRank(Character, TEXT("Support.Conductor.Rehearsal"));
    if (Rehearsal >= 1 && bReappliedWhileLive)
        if (auto* Charge = Character->FindComponentByClass<UBreakerChargeComponent>())
            Charge->GrantCharge(GetLastPaidResourceCost() * 0.5f);   // O2 PLACEHOLDER, single-rank (O272)
    bReappliedWhileLive = false;
    const UBreakerAbilityDefinition* Definition = GetAbilityDefinition();
    float Duration = Definition ? Definition->WindowDuration : 8.0f;
    const int32 Discipline = SupportNodeRank(Character, TEXT("Support.Conductor.DownbeatDiscipline"));
    SelfTailSeconds = Discipline >= 1 ? 4.0f : 0.0f;   // O2 PLACEHOLDER, single-rank (O272)
    auto* Charge = Character->FindComponentByClass<UBreakerChargeComponent>();
    if (SupportHasNode(Character, BreakerNodeTags::Node_CO_StandingOvation.GetTag()) && Charge
        && Charge->GetChargeBand() == EBreakerChargeBand::Resonant)
    { Duration *= 1.5f; SelfTailSeconds *= 1.5f; }
    bDetached = SupportHasNode(Character, BreakerNodeTags::Node_CO_DetachedBaton.GetTag());
    bConducting = SupportHasNode(Character, BreakerNodeTags::Node_CO_Conducting.GetTag());
    const int32 Section = SupportNodeRank(Character, TEXT("Support.Conductor.Section"));
    bSection = Section > 0;
    ActiveAuraRadius = bDetached ? DetachedBatonRadiusCm : AuraRadiusCm;
    // Single-rank node (O272): the RankOne key carries the whole bonus; the
    // RankTwo member stays declared and unread.
    ActiveAuraRadius += Section >= 1 ? SectionRankOneRadiusBonusCm : 0.0f;
    AuraEndTime = World->GetTimeSeconds() + Duration;
    LastAuraUpdateTime = World->GetTimeSeconds();
    TempoOwnerKey = FName(*FString::Printf(TEXT("Cadence.%u"), GetUniqueID()));
    FBreakerZoneSpec Spec;
    Spec.ZoneTag = FGameplayTag::RequestGameplayTag(TEXT("Zone.Support.Cadence"), false);
    Spec.RadiusCm = ActiveAuraRadius;
    Spec.Duration = Duration;
    Spec.bMobileFootprint = !bDetached;
    Spec.bShowFilledFootprint = false;
    Spec.ZoneColor = BreakerUI::Orange;
    FActorSpawnParameters Spawn;
    Spawn.Owner = Character; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    const FVector Feet = Character->GetActorLocation() - FVector(0, 0, Character->GetSimpleCollisionHalfHeight());
    BatonZone = World->SpawnActor<ABreakerZoneActor>(ABreakerZoneActor::StaticClass(), Feet, FRotator::ZeroRotator, Spawn);
    if (!BatonZone) { EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return; }
    BatonZone->ConfigureZone(Spec, Character);
    if (!bDetached && bSection) BatonZone->SetFollowActor(Character);
    bCadenceActive = true;
    Character->GetCombat()->OnDeath.AddUniqueDynamic(this, &ThisClass::HandleCadenceDeath);
    RefreshAura();
    World->GetTimerManager().SetTimer(WindowTimer, this, &ThisClass::RefreshAura, .05f, true);
    if (bConducting) World->GetTimerManager().SetTimer(ConductingTimer, this, &ThisClass::ShaveTick, 1.0f, true);
    BreakerSupportAbilityLocal::BreakerSupportCastFlash(Character, BreakerUI::Orange, 45.0f);
}

void UBreakerAbility_Cadence::RemoveRecipient(ABreakerCharacter* Recipient, bool bNatural)
{
    if (!Recipient) return;
    Recipients.Remove(Recipient);
    InsideRecipients.Remove(Recipient);
    if (auto* Weapon = Recipient->GetWeapon())
    {
        if (bNatural) { TempoTailRecipients.Add(Recipient); Weapon->FinishWindowTempoBonus(TempoOwnerKey); }
        else Weapon->PopTempoBonus(TempoOwnerKey);
    }
    if (Recipient != GetBreakerCharacter()) Recipient->GetCombat()->OnDeath.RemoveDynamic(this, &ThisClass::HandleCadenceDeath);
    if (auto* State = Recipient->FindComponentByClass<UBreakerAbilityStateComponent>())
    {
        State->OnWindowEnded.RemoveDynamic(this, &ThisClass::HandleCadenceWindowEnded);
        State->OnOwnedWindowEnded.RemoveDynamic(this, &ThisClass::HandleCadenceOwnedWindowEnded);
        State->CloseOwnedWindow(WindowKey(), TempoOwnerKey);
    }
    RefreshBuffUptime(Recipient);
}

void UBreakerAbility_Cadence::HandleCadenceProgressionChanged()
{
    if (!SupportHasNode(GetBreakerCharacter(), FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Afterimage"))))
        bAfterimageAtCast = false;
}

void UBreakerAbility_Cadence::HandleCadenceOwnedWindowEnded(AActor* Holder, FName Key, FName OwnerKey, bool bNatural)
{
    auto* Recipient = Cast<ABreakerCharacter>(Holder);
    if (!bCadenceActive || Key != WindowKey() || OwnerKey != TempoOwnerKey || !Recipients.Contains(Recipient)) return;
    const auto* Source = GetBreakerCharacter();
    RemoveRecipient(Recipient, bNatural && Source && !Source->IsActorBeingDestroyed() && !Source->GetCombat()->IsDead()
        && Recipient && !Recipient->IsActorBeingDestroyed() && !Recipient->GetCombat()->IsDead());
}

void UBreakerAbility_Cadence::ClearTempoTails()
{
    const auto Previous = TempoTailRecipients;
    TempoTailRecipients.Reset();
    for (const auto& Weak : Previous) if (auto* Holder = Weak.Get()) Holder->GetWeapon()->PopTempoBonus(TempoOwnerKey);
}

void UBreakerAbility_Cadence::OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
    ClearTempoTails();
    if (IsActive()) EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
    Super::OnRemoveAbility(ActorInfo, Spec);
}

void UBreakerAbility_Cadence::HandleCadenceWindowEnded(FName Key)
{
    if (Key == WindowKey()) RefreshAura();
}

void UBreakerAbility_Cadence::HandleCadenceDeath() { RefreshAura(); }

void UBreakerAbility_Cadence::RefreshAura()
{
    if (!bCadenceActive || bRefreshingAura) return;
    TGuardValue<bool> Guard(bRefreshingAura, true);
    ABreakerCharacter* Character = GetBreakerCharacter();
    UWorld* World = Character ? Character->GetWorld() : nullptr;
    if (!World || Character->IsActorBeingDestroyed() || Character->GetCombat()->IsDead())
    { EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true); return; }
    const float AuraRemaining = FMath::Max(0.0, AuraEndTime - World->GetTimeSeconds());
    const float Elapsed = FMath::Max(0.0, World->GetTimeSeconds() - LastAuraUpdateTime);
    LastAuraUpdateTime = World->GetTimeSeconds();
    TSet<TWeakObjectPtr<ABreakerCharacter>> Inside;
    if (AuraRemaining > 0 && IsValid(BatonZone) && !BatonZone->IsActorBeingDestroyed())
    {
        // Query living cooperative player characters directly: the generic zone
        // overlap excludes its caster and includes enemies, neither is a buff rule.
        const FVector Desired = Character->GetActorLocation() - FVector(0, 0, Character->GetSimpleCollisionHalfHeight());
        if (!bDetached)
            BatonZone->SetActorLocation(bSection ? Desired : FMath::VInterpConstantTo(BatonZone->GetActorLocation(), Desired, Elapsed, Character->GetBreakerMovement()->GetWalkSpeedCap()));
        const FVector Center = BatonZone->GetActorLocation();
        for (TActorIterator<ABreakerCharacter> It(World); It; ++It)
        {
            ABreakerCharacter* Recipient = *It;
            if (Recipient->IsActorBeingDestroyed() || Recipient->GetCombat()->IsDead()) continue;
            const FVector Delta = Recipient->GetActorLocation() - Center;
            if ((bDetached || Recipient != Character)
                && (Delta.SizeSquared2D() > FMath::Square(ActiveAuraRadius) || FMath::Abs(Delta.Z) > 250.0f)) continue;
            Inside.Add(Recipient);
            auto* State = UBreakerAbilityStateComponent::FindOrAdd(Recipient);
            if (!Recipients.Contains(Recipient))
            {
                Recipients.Add(Recipient);
                TempoTailRecipients.Remove(Recipient);
                Recipient->GetWeapon()->PushWindowTempoBonus(TempoOwnerKey, ReloadTempoMultiplier, SwapTempoMultiplier, Character, bAfterimageAtCast);
                State->OnOwnedWindowEnded.AddUniqueDynamic(this, &ThisClass::HandleCadenceOwnedWindowEnded);
                State->StartOwnedWindow(WindowKey(), TempoOwnerKey, AuraRemaining + (Recipient == Character ? SelfTailSeconds : 0));
                State->OnWindowEnded.AddUniqueDynamic(this, &ThisClass::HandleCadenceWindowEnded);
                Recipient->GetCombat()->OnDeath.AddUniqueDynamic(this, &ThisClass::HandleCadenceDeath);
            }
            else if (!InsideRecipients.Contains(Recipient))
                State->StartOwnedWindow(WindowKey(), TempoOwnerKey, FMath::Max(State->GetOwnedWindowRemaining(WindowKey(), TempoOwnerKey), AuraRemaining + (Recipient == Character ? SelfTailSeconds : 0)));
        }
    }
    const auto Previous = Recipients.Array();
    for (const auto& Held : Previous)
    {
        ABreakerCharacter* Recipient = Held.Get();
        if (!Recipient) { Recipients.Remove(Held); InsideRecipients.Remove(Held); continue; }
        auto* State = Recipient->FindComponentByClass<UBreakerAbilityStateComponent>();
        if (Recipient->IsActorBeingDestroyed() || Recipient->GetCombat()->IsDead() || !State)
        { RemoveRecipient(Recipient); continue; }
        if (AuraRemaining > 0 && !Inside.Contains(Recipient))
        {
            if (Recipient == Character && bConducting && InsideRecipients.Contains(Recipient))
            {
                const float Extension = FMath::Max(0.0f, State->GetOwnedWindowRemaining(WindowKey(), TempoOwnerKey) - AuraRemaining - SelfTailSeconds);
                State->StartOwnedWindow(WindowKey(), TempoOwnerKey, ConductingTailSeconds + SelfTailSeconds + Extension);
            }
            else if (Recipient != Character || !bConducting) { RemoveRecipient(Recipient); continue; }
        }
        if (State->GetOwnedWindowRemaining(WindowKey(), TempoOwnerKey) <= 0)
        { RemoveRecipient(Recipient); continue; }
        const auto* SourceState = Character->FindComponentByClass<UBreakerAbilityStateComponent>();
        const bool bDownbeat = SupportHasNode(Character, FGameplayTag::RequestGameplayTag(TEXT("Keystone.Support.Downbeat"), false))
            && SourceState && SourceState->IsWindowActive(ConduitWindowKey());
        const float BonusScale = bDownbeat ? 2.0f : 1.0f;
        Recipient->GetWeapon()->UpdateWindowTempoBonus(TempoOwnerKey, BonusScale);
        RefreshBuffUptime(Recipient);
    }
    InsideRecipients = MoveTemp(Inside);
    TArray<AActor*> LiveHolders;
    for (const auto& Held : Recipients) if (auto* Recipient = Held.Get()) LiveHolders.Add(Recipient);
    if (auto* State = UBreakerAbilityStateComponent::FindOrAdd(Character)) State->SetMaintainedBuffRecipients(TempoOwnerKey, LiveHolders);
    if (auto* Charge = Character->FindComponentByClass<UBreakerChargeComponent>())
        Charge->SetMaintainedBuffActive(TempoOwnerKey, !Recipients.IsEmpty());
    // Owned window expiry is the payload clock too: Continuance extensions
    // remain real after the footprint ends, rather than extending only the HUD.
    if (AuraRemaining <= 0 && Recipients.IsEmpty())
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UBreakerAbility_Cadence::ShaveTick()
{
    RefreshAura();
    if (!bCadenceActive) return;
    for (const auto& Held : Recipients)
        if (ABreakerCharacter* Recipient = Held.Get())
            if (auto* Charge = Recipient->FindComponentByClass<UBreakerChargeComponent>()) Charge->ShaveAllAbilityCooldowns(.25f);
}

// Kept as native delegate seams for existing authored actors; membership is
// evaluated by the same live-player query for normal and detached auras.
void UBreakerAbility_Cadence::HandleBatonOccupantEntered(AActor*) { RefreshAura(); }
void UBreakerAbility_Cadence::HandleBatonOccupantExited(AActor*) { RefreshAura(); }
void UBreakerAbility_Cadence::HandleBatonZoneExpired() { BatonZone = nullptr; }

void UBreakerAbility_Cadence::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (bCadenceActive)
    {
        const bool bNaturalCompletion = Recipients.IsEmpty() && !bWasCancelled;
        bCadenceActive = false;
        if (!bNaturalCompletion) ClearTempoTails();
        bReappliedWhileLive = false;
        if (auto* Character = GetBreakerCharacter())
        {
            Character->GetProgression()->OnProgressionChanged.RemoveDynamic(this, &ThisClass::HandleCadenceProgressionChanged);
            if (auto* Charge = Character->FindComponentByClass<UBreakerChargeComponent>()) Charge->SetMaintainedBuffActive(TempoOwnerKey, false);
            Character->GetCombat()->OnDeath.RemoveDynamic(this, &ThisClass::HandleCadenceDeath);
            if (auto* State = Character->FindComponentByClass<UBreakerAbilityStateComponent>())
                { bReappliedWhileLive = !bWasCancelled && State->GetOwnedWindowRemaining(WindowKey(), TempoOwnerKey) > .1f; State->ClearMaintainedBuffRecipients(TempoOwnerKey, bWasCancelled || Character->GetCombat()->IsDead()); }
        }
        const auto Previous = Recipients.Array();
        for (const auto& Held : Previous) if (auto* Recipient = Held.Get()) RemoveRecipient(Recipient);
        Recipients.Reset(); InsideRecipients.Reset();
        if (IsValid(BatonZone)) BatonZone->Destroy();
        BatonZone = nullptr;
        if (UWorld* World = GetWorld())
        { World->GetTimerManager().ClearTimer(WindowTimer); World->GetTimerManager().ClearTimer(ConductingTimer); }
    }
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
// ---------------------------------------------------------------------------
// U4 — METRONOME
// ---------------------------------------------------------------------------

UBreakerAbility_Metronome::UBreakerAbility_Metronome()
{
    FallbackAbilityId = TEXT("Support.Metronome");
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
    bRetriggerInstancedAbility = true;
}

FName UBreakerAbility_Metronome::WindowKey() { return TEXT("Window.Support.Metronome"); }
FName UBreakerAbility_Metronome::OutgoingModifierKey() { return TEXT("Metronome"); }

void UBreakerAbility_Metronome::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    ABreakerCharacter* Character = GetBreakerCharacter();
    UWorld* World = Character ? Character->GetWorld() : nullptr;
    UBreakerChargeComponent* Charge = Character ? Character->FindComponentByClass<UBreakerChargeComponent>() : nullptr;
    const bool bResonantAtCast = Charge && Charge->GetChargeBand() == EBreakerChargeBand::Resonant;
    const float PaidCost = GetResourceCost();
    if (!World || Character->GetCombat()->IsDead() || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }
    ClearRampTails();
    const int32 RehearsalRank = SupportNodeRank(Character, TEXT("Support.Conductor.Rehearsal"));
    const bool bRefresh = RehearsalRank > 0 && bReappliedWhileLive && World->GetTimeSeconds() < RehearsalUntil;
    if (bRefresh && Charge) Charge->GrantCharge(PaidCost * 0.5f); // O2 PLACEHOLDER, single-rank (O272)
    bReappliedWhileLive = false;
    const UBreakerAbilityDefinition* Definition = GetAbilityDefinition();
    float Duration = Definition ? Definition->WindowDuration : 8.0f;
    if (bResonantAtCast && SupportHasNode(Character, BreakerNodeTags::Node_CO_StandingOvation.GetTag())) Duration *= 1.5f; // O2 PLACEHOLDER
    const int32 Discipline = SupportNodeRank(Character, TEXT("Support.Conductor.DownbeatDiscipline"));
    float SelfTail = Discipline >= 1 ? 4.0f : 0.0f; // O2 PLACEHOLDER, single-rank (O272)
    if (bResonantAtCast && SupportHasNode(Character, BreakerNodeTags::Node_CO_StandingOvation.GetTag())) SelfTail *= 1.5f;
    RampOwnerKey = FName(*FString::Printf(TEXT("Metronome.%u"), GetUniqueID()));
    bMetronomeActive = true;
    for (TActorIterator<ABreakerCharacter> It(World); It; ++It)
    {
        ABreakerCharacter* Holder = *It;
        if (!Holder || Holder->IsActorBeingDestroyed() || !Holder->GetCombat() || Holder->GetCombat()->IsDead()) continue;
        if (Holder != Character && FVector::DistSquared(Holder->GetActorLocation(), Character->GetActorLocation()) > FMath::Square(RecipientRadiusCm)) continue;
        FHolderRamp Ramp;
        if (bRefresh) if (const auto* Previous = RehearsalRamps.Find(Holder)) Ramp = *Previous;
        const float HolderDuration = Duration + (Holder == Character ? SelfTail : 0);
        Ramp.EndTime = World->GetTimeSeconds() + HolderDuration;
        Holders.Add(Holder, Ramp);
        Holder->GetCombat()->PushWindowWeaponFlatDamage(RampOwnerKey, 0, HolderDuration, Character);
        auto* State = UBreakerAbilityStateComponent::FindOrAdd(Holder);
        State->StartOwnedWindow(WindowKey(), RampOwnerKey, HolderDuration);
        State->OnWindowEnded.AddUniqueDynamic(this, &ThisClass::HandleMetronomeWindowEnded);
        Holder->GetCombat()->OnHitDealt.AddUniqueDynamic(this, &ThisClass::HandleHitDealt);
        Holder->GetCombat()->OnDeath.AddUniqueDynamic(this, &ThisClass::HandleMetronomeDeath);
    }
    RehearsalRamps.Reset();
    RefreshHolders();
    BreakerSupportAbilityLocal::BreakerSupportCastFlash(Character, BreakerUI::Cyan, 40.0f);
    World->GetTimerManager().SetTimer(WindowTimer, this, &ThisClass::RefreshHolders, .05f, true);
}

void UBreakerAbility_Metronome::RemoveHolder(ABreakerCharacter* Holder, bool bNaturalExpiry)
{
    if (!Holder) return;
    Holders.Remove(Holder);
    if (auto* Combat = Holder->GetCombat())
    {
        Combat->OnHitDealt.RemoveDynamic(this, &ThisClass::HandleHitDealt);
        Combat->OnDeath.RemoveDynamic(this, &ThisClass::HandleMetronomeDeath);
        if (bNaturalExpiry)
        {
            TailRecipients.Add(Holder);
            Combat->FinishWindowWeaponFlatDamage(RampOwnerKey);
        }
        else Combat->PopWeaponFlatDamage(RampOwnerKey);
    }
    if (auto* State = Holder->FindComponentByClass<UBreakerAbilityStateComponent>())
    {
        State->OnWindowEnded.RemoveDynamic(this, &ThisClass::HandleMetronomeWindowEnded);
        State->CloseOwnedWindow(WindowKey(), RampOwnerKey);
    }
    RefreshBuffUptime(Holder);
}

void UBreakerAbility_Metronome::RefreshHolders()
{
    if (!bMetronomeActive || bRefreshingHolders) return;
    TGuardValue<bool> Guard(bRefreshingHolders, true);
    ABreakerCharacter* Character = GetBreakerCharacter();
    if (!Character || Character->IsActorBeingDestroyed() || Character->GetCombat()->IsDead()) { CloseMetronome(); return; }
    const double Now = GetWorld()->GetTimeSeconds();
    const int32 Tempo = SupportNodeRank(Character, TEXT("Support.Conductor.Tempo"));
    const auto* SourceState = Character->FindComponentByClass<UBreakerAbilityStateComponent>();
    const bool bDownbeat = SupportHasNode(Character, FGameplayTag::RequestGameplayTag(TEXT("Keystone.Support.Downbeat"), false))
        && SourceState && SourceState->IsWindowActive(ConduitWindowKey());
    TArray<TWeakObjectPtr<ABreakerCharacter>> Previous;
    Holders.GetKeys(Previous);
    TArray<AActor*> Living;
    for (const auto& Weak : Previous)
    {
        ABreakerCharacter* Holder = Weak.Get();
        if (!Holder) { Holders.Remove(Weak); continue; }
        auto* State = Holder->FindComponentByClass<UBreakerAbilityStateComponent>();
        if (Holder->IsActorBeingDestroyed() || Holder->GetCombat()->IsDead() || !State || State->GetOwnedWindowRemaining(WindowKey(), RampOwnerKey) <= 0)
        {
            const FHolderRamp* Expiring = Holders.Find(Weak);
            const bool bNatural = State && !Holder->IsActorBeingDestroyed() && !Holder->GetCombat()->IsDead()
                && Expiring && (State->IsNaturalWindowEnd(WindowKey()) || Now >= Expiring->EndTime);
            RemoveHolder(Holder, bNatural);
            continue;
        }
        FHolderRamp* Ramp = Holders.Find(Weak);
        const bool bTempo = Tempo > 0;   // single-rank (O272): every holder
        const float Gap = StreakGapSeconds * (bTempo ? 1.5f : 1.0f); // O2 PLACEHOLDER
        if (Now - Ramp->LastHitTime >= Gap) Ramp->Stacks = 0;
        Ramp->Stacks = FMath::Min(Ramp->Stacks, static_cast<float>(MaximumStacks + (bTempo ? 3 : 0))); // O2 PLACEHOLDER
        Holder->GetCombat()->UpdateWindowWeaponFlatDamage(RampOwnerKey, FlatDamagePerStack * Ramp->Stacks * (bDownbeat ? 2.0f : 1.0f));
        Living.Add(Holder);
        RefreshBuffUptime(Holder);
    }
    if (auto* State = UBreakerAbilityStateComponent::FindOrAdd(Character)) State->SetMaintainedBuffRecipients(RampOwnerKey, Living);
    if (auto* Charge = Character->FindComponentByClass<UBreakerChargeComponent>()) Charge->SetMaintainedBuffActive(RampOwnerKey, !Living.IsEmpty());
    if (Holders.IsEmpty()) CloseMetronome();
}

void UBreakerAbility_Metronome::HandleHitDealt(const FBreakerHitContext& Hit)
{
    if (!bMetronomeActive || Hit.Result.bDodged || Hit.Result.HealthDamage + Hit.Result.ShieldDamage <= 0
        || !FMath::IsFinite(Hit.ProcCoefficient) || Hit.ProcCoefficient <= 0) return;
    ABreakerCharacter* Character = GetBreakerCharacter();
    ABreakerCharacter* Holder = Cast<ABreakerCharacter>(Hit.Instigator);
    if (!Character || !Holder || !Holders.Contains(Holder)) return;
    const bool bCounterpoint = SupportHasNode(Character, BreakerNodeTags::Node_CO_Counterpoint.GetTag());
    const FGameplayTag AbilityTag = FGameplayTag::RequestGameplayTag(TEXT("Ability"), false);
    const FGameplayTag MeleeTag = FGameplayTag::RequestGameplayTag(TEXT("Damage.Melee"), false);
    if (!bCounterpoint && (Hit.bFromDoT || Hit.Delivery != EBreakerDamageDelivery::Weapon
        || Hit.SourceTags.HasTag(AbilityTag) || Hit.SourceTags.HasTagExact(MeleeTag))) return;
    RefreshHolders();
    FHolderRamp* Ramp = Holders.Find(Holder);
    if (!Ramp) return;
    const int32 Tempo = SupportNodeRank(Character, TEXT("Support.Conductor.Tempo"));
    const bool bTempo = Tempo > 0;   // single-rank (O272): every holder
    Ramp->LastHitTime = GetWorld()->GetTimeSeconds();
    Ramp->Stacks = FMath::Min(Ramp->Stacks + FMath::Clamp(Hit.ProcCoefficient, 0.0f, 1.0f), static_cast<float>(MaximumStacks + (bTempo ? 3 : 0)));
    RefreshHolders();
}

void UBreakerAbility_Metronome::ClearRampTails()
{
    const auto Previous = TailRecipients;
    TailRecipients.Reset();
    for (const auto& Weak : Previous)
        if (auto* Holder = Weak.Get()) if (auto* Combat = Holder->GetCombat()) Combat->PopWeaponFlatDamage(RampOwnerKey);
}

void UBreakerAbility_Metronome::OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
    ClearRampTails();
    if (IsActive()) EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
    Super::OnRemoveAbility(ActorInfo, Spec);
}

void UBreakerAbility_Metronome::HandleMetronomeDeath() { RefreshHolders(); }
void UBreakerAbility_Metronome::HandleMetronomeWindowEnded(FName Key) { if (Key == WindowKey()) RefreshHolders(); }
void UBreakerAbility_Metronome::CloseMetronome()
{
    if (CurrentActorInfo && IsActive()) EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UBreakerAbility_Metronome::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (bMetronomeActive)
    {
        const bool bNaturalCompletion = Holders.IsEmpty() && !bWasCancelled;
        bMetronomeActive = false;
        if (!bNaturalCompletion) ClearRampTails();
        RehearsalRamps.Reset(); bReappliedWhileLive = false; RehearsalUntil = 0;
        ABreakerCharacter* Character = GetBreakerCharacter();
        TArray<TWeakObjectPtr<ABreakerCharacter>> Previous;
        Holders.GetKeys(Previous);
        for (const auto& Weak : Previous)
        {
            if (ABreakerCharacter* Holder = Weak.Get())
            {
                if (Character && !Character->GetCombat()->IsDead() && !Holder->GetCombat()->IsDead())
                    if (auto* State = Holder->FindComponentByClass<UBreakerAbilityStateComponent>())
                        if (const float Remaining = State->GetOwnedWindowRemaining(WindowKey(), RampOwnerKey); !bWasCancelled && Remaining > .1f)
                        {
                            RehearsalRamps.Add(Weak, Holders.FindChecked(Weak));
                            bReappliedWhileLive = true;
                            RehearsalUntil = FMath::Max(RehearsalUntil, GetWorld()->GetTimeSeconds() + Remaining);
                        }
                RemoveHolder(Holder);
            }
        }
        Holders.Reset();
        if (Character)
        {
            if (auto* State = Character->FindComponentByClass<UBreakerAbilityStateComponent>()) State->ClearMaintainedBuffRecipients(RampOwnerKey, bWasCancelled || Character->GetCombat()->IsDead());
            if (auto* Charge = Character->FindComponentByClass<UBreakerChargeComponent>()) Charge->SetMaintainedBuffActive(RampOwnerKey, false);
        }
        if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(WindowTimer);
    }
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// ---------------------------------------------------------------------------
// U5 — MARK
// ---------------------------------------------------------------------------
UBreakerAbility_Mark::UBreakerAbility_Mark()
{
    FallbackAbilityId = TEXT("Support.Mark");
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
    // WA2 (re-mark) and WA8 (deepen) require casting onto a live mark; the
    // retrigger tears the old mark down through the ordinary path first.
    bRetriggerInstancedAbility = true;
}

float UBreakerAbility_Mark::GetUnmodifiedResourceCost() const
{
    // WA11 HUNTER'S ECONOMY: Mark costs nothing — the floor-recovery answer.
    // (The much-shorter leash is applied at activation.)
    return SupportHasNode(GetBreakerCharacter(), BreakerNodeTags::Node_WA_HuntersEconomy.GetTag()) ? 0.0f : Super::GetUnmodifiedResourceCost();
}

FName UBreakerAbility_Mark::IncomingModifierKey() { return TEXT("Support.Mark"); }
FName UBreakerAbility_Mark::TellModifierKey() { return TEXT("Support.Mark.Tell"); }

bool UBreakerAbility_Mark::ShouldShowTell(const ABreakerCharacter* Viewer, const ABreakerEnemy* Enemy)
{
    if (!IsValid(Viewer) || !IsValid(Enemy) || Enemy->IsDeadEnemy() || Viewer->GetCombat()->IsDead()
        || Viewer->GetProgression()->GetProgressionState().PermanentClass != EBreakerClassId::Support
        || !SupportHasNode(Viewer, BreakerNodeTags::Node_WA_Tell.GetTag())) return false;
    const auto* State = Viewer->FindComponentByClass<UBreakerAbilityStateComponent>();
    if (!State || !State->IsMarked(Enemy)) return false;
    const auto* Ranged = Cast<ABreakerRangedEnemy>(Enemy);
    const auto* Warden = Cast<ABreakerWardenEnemy>(Enemy);
    return Enemy->IsLungeWindingUp() || (Ranged && Ranged->IsWindingUp())
        || (Warden && (Warden->IsSweeping() || Warden->IsSlamming()));
}

void UBreakerAbility_Mark::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    const UBreakerAbilityDefinition* Definition = GetAbilityDefinition();
    ABreakerCharacter* Character = GetBreakerCharacter();
    UWorld* World = Character ? Character->GetWorld() : nullptr;
    if (!Character || !World)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // Aim resolution BEFORE commit: marking nothing refuses the cast rather
    // than eating 20 Charge — the loop's ignition must not misfire on a whiff.
    FVector ViewLocation = Character->GetActorLocation();
    FRotator ViewRotation = Character->GetControlRotation();
    if (const AController* Controller = Character->GetController())
    {
        Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
    }
    FHitResult Hit;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BreakerMarkAim), false, Character);
    AActor* Target = nullptr;
    if (World->LineTraceSingleByChannel(Hit, ViewLocation, ViewLocation + ViewRotation.Vector() * TargetRangeCm, ECC_GameTraceChannel2, QueryParams))
    {
        if (Cast<ABreakerEnemy>(Hit.GetActor())) Target = Hit.GetActor();
    }
    UBreakerCombatComponent* TargetCombat = Target ? Target->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
    if (!TargetCombat || TargetCombat->IsDead())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // WA2 LONG WATCH's cooldown clause and WA8 DEEP MARK both key on whether
    // this cast lands on a STILL-MARKED target. The state component's mark
    // survives the retrigger teardown, so the question is answerable here.
    bool bReMarkSame = false;
    if (const UBreakerAbilityStateComponent* State = Character->FindComponentByClass<UBreakerAbilityStateComponent>())
    {
        bReMarkSame = State->GetMarkedTarget() == Target && State->GetMarkRemaining() > 0.0f;
    }

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // WA2 LONG WATCH: marks last longer (+8s — O2 PLACEHOLDER; single-rank,
    // O272), and re-marking a still-marked target spends no cooldown. The
    // duration seam (AbilityDurationMultiplierFor) is adopted at the same site.
    const int32 LongWatchRank = SupportNodeRank(Character, TEXT("Support.Warden.LongWatch"));
    float Duration = (Definition ? Definition->WindowDuration : 10.0f) + (LongWatchRank >= 1 ? 8.0f : 0.0f);
    Duration *= GetAbilityDurationMultiplier();
    if (LongWatchRank > 0 && bReMarkSame)
    {
        ClearOwnCooldown();
    }
    // WA11 HUNTER'S ECONOMY: free — but it runs much shorter and holds one
    // target only (the single-mark surface already guarantees one).
    if (SupportHasNode(Character, BreakerNodeTags::Node_WA_HuntersEconomy.GetTag()))
    {
        Duration = HuntersEconomyDuration;
    }

    // WA8 DEEP MARK: marking a marked target deepens it — more damage taken,
    // richer Charge yield, capped. Deepening never resets the anti-farm window
    // (this rewrites the modifier and the clock, never generation eligibility,
    // which the Charge component's caller contract owns).
    if (SupportHasNode(Character, BreakerNodeTags::Node_WA_DeepMark.GetTag()) && bReMarkSame)
    {
        MarkDepth = FMath::Min(MarkDepth + 1, DeepMarkMaxDepth);
    }
    else
    {
        MarkDepth = 0;
    }
    ActiveMarkDuration = Duration;

    // The paint, on the shared mark surface the weapon already reads for its
    // marked-target treatment, so the two mark consumers cannot disagree.
    if (UBreakerAbilityStateComponent* State = UBreakerAbilityStateComponent::FindOrAdd(Character))
    {
        State->SetMark(Target, Duration);
        // The painting, Lead's proven composition (weapon-side offset start —
        // a camera-origin stroke reads as a dot to its own caster): a gold
        // line to the marked enemy plus a glow at the impact. The mark's
        // lifetime stays the HUD diamond's job. Figures O2 PLACEHOLDER.
        if (ABreakerEffectRenderer* Effects = ABreakerEffectRenderer::FindOrSpawn(World))
        {
            BreakerFX::FEffectTiming PaintTiming;
            PaintTiming.DurationSeconds = 0.30f;
            PaintTiming.FadeInSeconds = 0.03f;
            PaintTiming.FadeOutSeconds = 0.22f;
            const FVector PaintSide = FVector::CrossProduct(ViewRotation.Vector(), FVector::UpVector).GetSafeNormal();
            Effects->AddStroke(ViewLocation + ViewRotation.Vector() * 90.0f + PaintSide * 25.0f - FVector(0.0f, 0.0f, 20.0f),
                Hit.ImpactPoint, 2.0f, BreakerUI::Gold, 2.6f, PaintTiming);
            Effects->AddGlow(Hit.ImpactPoint, 28.0f, BreakerUI::Gold, 3.2f, PaintTiming);
        }
    }
    // Incoming vulnerability and Tell reconcile across living cast owners.
    MarkedTarget = Target;
    BoundCombat = TargetCombat;
    TargetCombat->OnDamageTaken.AddUniqueDynamic(this, &ThisClass::HandleHitDealt);
    bMarkActive = true;
    Character->GetCombat()->OnDeath.AddUniqueDynamic(this, &ThisClass::HandleMarkOwnerDeath);
    ReconcileTarget(Target);
    World->GetTimerManager().SetTimer(MarkTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { CloseMark(); }), Duration, false);
}

void UBreakerAbility_Mark::PointMarkAt(AActor* NewTarget, float Duration)
{
    // WA4's jump: move the paint, the modifier and the clock to the new
    // target. The jump itself pays NOTHING — no credit call lives here.
    ABreakerCharacter* Character = GetBreakerCharacter();
    UWorld* World = Character ? Character->GetWorld() : nullptr;
    if (!Character || !World || !NewTarget) return;
    AActor* PreviousTarget = MarkedTarget.Get();
    if (AActor* Old = PreviousTarget)
    {
        if (UBreakerCombatComponent* OldCombat = Old->FindComponentByClass<UBreakerCombatComponent>())
        {
            OldCombat->OnDamageTaken.RemoveDynamic(this, &ThisClass::HandleHitDealt);
        }
    }
    MarkDepth = 0;   // a jumped mark lands shallow
    if (UBreakerCombatComponent* NewCombat = NewTarget->FindComponentByClass<UBreakerCombatComponent>())
    {
        BoundCombat = NewCombat;
        NewCombat->OnDamageTaken.AddUniqueDynamic(this, &ThisClass::HandleHitDealt);
    }
    if (UBreakerAbilityStateComponent* State = UBreakerAbilityStateComponent::FindOrAdd(Character))
    {
        State->SetMark(NewTarget, Duration);
    }
    MarkedTarget = NewTarget;
    ReconcileTarget(PreviousTarget);
    ReconcileTarget(NewTarget);
    World->GetTimerManager().SetTimer(MarkTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { CloseMark(); }), Duration, false);
}

void UBreakerAbility_Mark::HandleHitDealt(const FBreakerHitContext& Hit)
{
    if (!bMarkActive || Hit.Target != MarkedTarget.Get() || !Hit.Target) return;
    ABreakerCharacter* Character = GetBreakerCharacter();
    if (!Character) return;
    if (Character->GetCombat()->IsDead() || Character->IsActorBeingDestroyed()) return;
    ABreakerCharacter* Dealer = Cast<ABreakerCharacter>(Hit.Instigator);
    const bool bOwner = Dealer == Character;
    const bool bLivingPlayer = Dealer && !Dealer->IsActorBeingDestroyed() && Dealer->GetCombat() && !Dealer->GetCombat()->IsDead();
    const bool bWeaponShot = Hit.Delivery == EBreakerDamageDelivery::Weapon && !Hit.bFromDoT
        && !Hit.SourceTags.HasTag(FGameplayTag::RequestGameplayTag(TEXT("Ability"), false))
        && !Hit.SourceTags.HasTagExact(FGameplayTag::RequestGameplayTag(TEXT("Damage.Melee"), false));
    const float ActualDamage = Hit.Result.HealthDamage + Hit.Result.ShieldDamage;
    const bool bPayingHit = bLivingPlayer && !Hit.Result.bDodged && FMath::IsFinite(ActualDamage) && ActualDamage > 0
        && FMath::IsFinite(Hit.ProcCoefficient) && Hit.ProcCoefficient > 0;
    const int32 PaintedRank = SupportNodeRank(Character, TEXT("Support.Warden.Painted"));
    if (UBreakerChargeComponent* Charge = Character->FindComponentByClass<UBreakerChargeComponent>())
    {
        // WA5 PAINTED, single-rank (O272): the owner's ability hits pay, and
        // an ally's hits pay at PaintedAllyYieldMultiplier.
        if (bPayingHit && (bOwner ? (bWeaponShot || PaintedRank >= 1) : PaintedRank >= 1))
        {
            const float YieldScale = (1.0f + DeepMarkYieldPerDepth * MarkDepth)
                * FMath::Clamp(Hit.ProcCoefficient, 0.0f, 1.0f)
                * (bOwner ? 1.0f : FMath::Clamp(PaintedAllyYieldMultiplier, 0.0f, 1.0f));
            Charge->NotifyMarkedTargetDamage(ActualDamage * YieldScale,
                BreakerSupportAbilityLocal::BreakerSupportTargetMaxHealth(Hit.Target));
        }
        // MD10 BLOOD DEBT: the next weapon hit on a marked target spends the
        // banked pool as flat damage — a one-shot settlement request, flat
        // bucket, no crit, proc 0, so it can neither double-dip nor seed.
        const float Debt = Charge->GetBloodDebtPool();
        if (Debt > 0.0f && bOwner && bWeaponShot && bPayingHit && !Hit.Result.bKilled)
        {
            if (UBreakerCombatComponent* TargetCombat = Hit.Target->FindComponentByClass<UBreakerCombatComponent>())
            {
                Charge->ConsumeBloodDebt();
                FBreakerDamageRequest Settlement;
                Settlement.BaseDamage = Debt;
                Settlement.SourceTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.Class.Support.Mark"), false));
                Settlement.DamageFamily = EBreakerDamageFamily::Physical;
                Settlement.bCanCritical = false;
                Settlement.ProcCoefficient = 0.0f;
                Settlement.SetInstigator(Character);
                // O34: even a banked settlement composes the outgoing chain,
                // so a live window More counts inside the one ceiling rather
                // than riding beside it (AbilitySubmissionConformance's rule).
                if (UBreakerCombatComponent* OwnerCombat = Character->FindComponentByClass<UBreakerCombatComponent>())
                {
                    OwnerCombat->ApplyOutgoingModifiers(Settlement);
                }
                TargetCombat->ReceiveDamage(Settlement);
            }
        }
    }

    // WA9 EXECUTIONER'S LEDGER: killing a marked target refunds Mark's cost
    // and cooldown in proportion to the mark's UNSPENT duration.
    if (Hit.Result.bKilled)
    {
        const UBreakerAbilityStateComponent* State = Character->FindComponentByClass<UBreakerAbilityStateComponent>();
        const float Remaining = State ? State->GetMarkRemainingFor(Hit.Target) : 0.0f;
        const float UnspentFraction = ActiveMarkDuration > 0.0f ? FMath::Clamp(Remaining / ActiveMarkDuration, 0.0f, 1.0f) : 0.0f;
        if (bOwner && UnspentFraction > 0.0f && SupportHasNode(Character, BreakerNodeTags::Node_WA_ExecutionersLedger.GetTag()))
        {
            if (UBreakerChargeComponent* Charge = Character->FindComponentByClass<UBreakerChargeComponent>())
            {
                // Refund what THIS build actually pays (a free Hunter's Economy
                // mark refunds nothing — there is nothing to refund).
                Charge->GrantCharge(GetLastPaidResourceCost() * UnspentFraction);
            }
            ShaveOwnCooldownSeconds(GetCooldownSeconds() * UnspentFraction);
        }

        // WA4 HANDOFF: the mark survives its target's death and jumps to the
        // nearest unmarked enemy. The jump pays nothing.
        const int32 HandoffRank = SupportNodeRank(Character, TEXT("Support.Warden.Handoff"));
        if (HandoffRank >= 1 && Remaining > 0.0f)
        {
            const float JumpRange = 2500.0f;   // O2 PLACEHOLDER, single-rank (O272)
            ABreakerEnemy* Nearest = nullptr;
            float BestDistSq = JumpRange * JumpRange;
            if (UWorld* World = GetWorld())
            {
                for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
                {
                    ABreakerEnemy* Candidate = *It;
                    if (!Candidate || Candidate == Hit.Target) continue;
                    bool bMarked = false;
                    for (TActorIterator<ABreakerCharacter> Player(World); Player; ++Player)
                        if (const auto* PlayerState = Player->FindComponentByClass<UBreakerAbilityStateComponent>()) bMarked |= PlayerState->IsMarked(Candidate);
                    if (bMarked) continue;
                    const UBreakerCombatComponent* CandidateCombat = Candidate->FindComponentByClass<UBreakerCombatComponent>();
                    if (!CandidateCombat || CandidateCombat->IsDead()) continue;
                    const float DistSq = FVector::DistSquared(Hit.Target->GetActorLocation(), Candidate->GetActorLocation());
                    if (DistSq < BestDistSq)
                    {
                        BestDistSq = DistSq;
                        Nearest = Candidate;
                    }
                }
            }
            if (Nearest)
            {
                PointMarkAt(Nearest, Remaining);
            }
        }
    }
}

void UBreakerAbility_Mark::HandleMarkOwnerDeath() { CloseMark(); }

void UBreakerAbility_Mark::ReconcileTarget(AActor* Target)
{
    if (!Target || !Target->GetWorld()) return;
    bool bAny = false;
    bool bTell = false;
    float Strongest = 1.0f;
    float Softest = 1.0f;
    // Same named mark does not multiply with itself. Live source instances
    // retain ownership while the shared target payload uses the strongest.
    for (TActorIterator<ABreakerCharacter> Player(Target->GetWorld()); Player; ++Player)
    {
        if (Player->IsActorBeingDestroyed() || Player->GetCombat()->IsDead()) continue;
        auto* ASC = Player->GetAbilitySystemComponent();
        if (!ASC) continue;
        for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
        {
            const auto* Mark = Cast<UBreakerAbility_Mark>(Spec.GetPrimaryInstance());
            if (!Mark || !Mark->bMarkActive || Mark->MarkedTarget.Get() != Target) continue;
            bAny = true;
            Strongest = FMath::Max(Strongest, Mark->MarkedDamageMultiplier + Mark->DeepMarkDamagePerDepth * Mark->MarkDepth);
            if (SupportHasNode(*Player, BreakerNodeTags::Node_WA_Tell.GetTag()))
            { bTell = true; Softest = FMath::Min(Softest, Mark->TellOutgoingMultiplier); }
        }
    }
    if (auto* Combat = Target->FindComponentByClass<UBreakerCombatComponent>())
    {
        if (bAny) Combat->PushIncomingDamageModifier(IncomingModifierKey(), Strongest);
        else Combat->RemoveIncomingDamageModifier(IncomingModifierKey());
    }
    if (auto* Enemy = Cast<ABreakerEnemy>(Target))
    {
        if (bTell) Enemy->PushOutgoingDamageMultiplier(TellModifierKey(), Softest);
        else Enemy->PopOutgoingDamageMultiplier(TellModifierKey());
    }
}
void UBreakerAbility_Mark::RefreshDuration(float MinimumRemainingSeconds)
{
    if (!bMarkActive || !GetWorld() || !FMath::IsFinite(MinimumRemainingSeconds)) return;
    ABreakerCharacter* Character = GetBreakerCharacter();
    AActor* Target = MarkedTarget.Get();
    if (!Character || Character->GetCombat()->IsDead() || !Target) return;
    auto* TargetCombat = Target->FindComponentByClass<UBreakerCombatComponent>();
    auto* State = Character->FindComponentByClass<UBreakerAbilityStateComponent>();
    if (!TargetCombat || TargetCombat->IsDead() || !State) return;
    const float Remaining = FMath::Max(State->GetMarkRemainingFor(Target), MinimumRemainingSeconds);
    if (Remaining <= 0) return;
    ActiveMarkDuration = FMath::Max(ActiveMarkDuration, Remaining);
    State->SetMark(Target, Remaining);
    GetWorld()->GetTimerManager().SetTimer(MarkTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { CloseMark(); }), Remaining, false);
}
void UBreakerAbility_Mark::CloseMark()
{
    if (CurrentActorInfo)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
    }
}

void UBreakerAbility_Mark::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (bMarkActive)
    {
        bMarkActive = false;
        if (auto* Character = GetBreakerCharacter())
        {
            Character->GetCombat()->OnDeath.RemoveDynamic(this, &ThisClass::HandleMarkOwnerDeath);
            if (bWasCancelled || Character->GetCombat()->IsDead())
                if (auto* State = Character->FindComponentByClass<UBreakerAbilityStateComponent>())
                    if (State->GetMarkRemainingFor(MarkedTarget.Get()) > 0.0f) State->ClearMark();
        }
        ReconcileTarget(MarkedTarget.Get());
        MarkedTarget.Reset();
        if (UBreakerCombatComponent* Combat = BoundCombat.Get())
        {
            Combat->OnDamageTaken.RemoveDynamic(this, &UBreakerAbility_Mark::HandleHitDealt);
        }
        BoundCombat.Reset();
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(MarkTimer);
        }
    }
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// ---------------------------------------------------------------------------
// U6 — SUPPRESS
// ---------------------------------------------------------------------------

UBreakerAbility_Suppress::UBreakerAbility_Suppress()
{
    FallbackAbilityId = TEXT("Support.Suppress");
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void UBreakerAbility_Suppress::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    const UBreakerAbilityDefinition* Definition = GetAbilityDefinition();
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
    FHitResult Hit;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BreakerSuppressAim), false, Character);
    const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * AimRangeCm;
    const FVector Center = World->LineTraceSingleByChannel(Hit, ViewLocation, TraceEnd, ECC_Visibility, QueryParams)
        ? Hit.ImpactPoint : TraceEnd;

    // §U6: a zone that deals NO damage at all — BaseDamage 0 is a legal and
    // useful zone, the zone actor's own comment names Suppress as the case.
    FBreakerZoneSpec Spec;
    Spec.ZoneTag = FGameplayTag::RequestGameplayTag(TEXT("Zone.Support.Suppress"), false);
    // WA3 FIELD OF VIEW: Suppress reaches further (6 m -> 8 m, O2 PLACEHOLDER).
    // Its instant-slow clause is already structural — the slow lands on the
    // occupant-entered edge, frame one. The accuracy cut pays through the
    // enemy aim-error seam in HandleOccupantEntered: delayed without the
    // node, INSTANT with it (single-rank, O272) — the rank captured here
    // decides.
    FieldOfViewRank = SupportNodeRank(Character, TEXT("Support.Warden.FieldOfView"));
    const bool bFieldOfView = FieldOfViewRank > 0;
    Spec.RadiusCm = bFieldOfView ? 800.0f : RadiusCm;
    Spec.Duration = Definition ? Definition->WindowDuration : 6.0f;
    Spec.TickInterval = 1.0f;
    Spec.ZoneColor = FLinearColor(0.55f, 0.35f, 0.85f);   // violet; teal is reserved (O19)
    // WA7 SUPPRESSION: a FLAT armour cut on enemies inside — flat, never a
    // percentage (the boss-cap protection) — through the zone's own keyed
    // armour lane, which pushes on entry and pops on exit for us.
    if (SupportHasNode(Character, BreakerNodeTags::Node_WA_Suppression.GetTag()))
    {
        Spec.FlatArmorReduction = SuppressionArmorCut;
    }
    // WA5 PRESSURE: enemies inside pay Charge at a slow, COUNT-INDEPENDENT
    // rate — the occupancy bool drives a once-a-second trickle; one enemy pays
    // exactly what six do.
    InvalidatePressure();
    PressureRank = SupportNodeRank(Character, TEXT("Support.Warden.Pressure"));

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParams.Owner = Character;
    ActiveZone = World->SpawnActor<ABreakerZoneActor>(ABreakerZoneActor::StaticClass(), Center, FRotator::ZeroRotator, SpawnParams);
    if (ActiveZone)
    {
        ActiveZone->ConfigureZone(Spec, Character);
        ActiveZone->OnOccupantEntered.AddDynamic(this, &UBreakerAbility_Suppress::HandleOccupantEntered);
        ActiveZone->OnOccupantExited.AddDynamic(this, &UBreakerAbility_Suppress::HandleOccupantExited);
        ActiveZone->OnZoneExpired.AddDynamic(this, &UBreakerAbility_Suppress::HandleZoneExpired);
        if (PressureRank > 0)
        {
            PressureCombat = Character->GetCombat();
            PressureProgression = Character->GetProgression();
            PressureCombat->OnDeath.AddUniqueDynamic(this, &UBreakerAbility_Suppress::InvalidatePressure);
            PressureProgression->OnProgressionChanged.AddUniqueDynamic(this, &UBreakerAbility_Suppress::RefreshPressurePermission);
        }
    }

    // The ability ends now; the zone owns the 6s. The 10s cooldown guarantees
    // no second Suppress races this zone's teardown handlers.
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    // GAS clears this ability's timers on EndAbility. Pressure belongs to the
    // surviving zone lease, so install its timer after that normal teardown.
    RefreshPressurePermission();
    if (ActiveZone && PressureRank > 0)
        World->GetTimerManager().SetTimer(PressureTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { PressureTick(); }), 1.0f, /*bLoop=*/true);
}

void UBreakerAbility_Suppress::InvalidatePressure()
{
    PressureRank = 0;
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(PressureTimer);
    if (auto* Combat = PressureCombat.Get()) Combat->OnDeath.RemoveDynamic(this, &UBreakerAbility_Suppress::InvalidatePressure);
    if (auto* Progression = PressureProgression.Get()) Progression->OnProgressionChanged.RemoveDynamic(this, &UBreakerAbility_Suppress::RefreshPressurePermission);
    PressureCombat.Reset(); PressureProgression.Reset();
}

void UBreakerAbility_Suppress::RefreshPressurePermission()
{
    const auto* Character = GetBreakerCharacter();
    if (!Character || Character->IsActorBeingDestroyed() || Character->GetCombat()->IsDead()
        || Character->GetProgression()->GetProgressionState().PermanentClass != EBreakerClassId::Support)
    { InvalidatePressure(); return; }
    PressureRank = FMath::Min(PressureRank, SupportNodeRank(Character, TEXT("Support.Warden.Pressure")));
    if (PressureRank <= 0) InvalidatePressure();
}

void UBreakerAbility_Suppress::PressureTick()
{
    RefreshPressurePermission();
    if (PressureRank <= 0) return;
    // Count-independent by construction: the question is "is anyone inside",
    // never "how many".
    if (!ActiveZone || SlowedEnemies.Num() == 0) return;
    bool bAnyLive = false;
    for (const TWeakObjectPtr<AActor>& Slowed : SlowedEnemies)
    {
        const AActor* Enemy = Slowed.Get();
        const UBreakerCombatComponent* EnemyCombat = Enemy ? Enemy->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
        if (EnemyCombat && !EnemyCombat->IsDead()) { bAnyLive = true; break; }
    }
    if (!bAnyLive) return;
    if (ABreakerCharacter* Character = GetBreakerCharacter())
    {
        if (UBreakerChargeComponent* Charge = Character->FindComponentByClass<UBreakerChargeComponent>())
        {
            // Single-rank node (O272): PressureChargePerSecond carries the
            // whole rate; PressureChargePerSecondRank2 stays declared, unread.
            Charge->GrantCharge(PressureChargePerSecond);
        }
    }
}

FName UBreakerAbility_Suppress::AccuracyModifierKey() { return TEXT("Support.Suppress.Accuracy"); }

void UBreakerAbility_Suppress::HandleOccupantEntered(AActor* Occupant)
{
    if (ABreakerEnemy* Enemy = Cast<ABreakerEnemy>(Occupant))
    {
        Enemy->ApplyModifierMovementProfile(SlowMultiplier, -1.0f);
        SlowedEnemies.AddUnique(Enemy);

        // §U6's accuracy half, through the enemy aim-error seam. Base: the cut
        // lands after the application delay (and only if the enemy is STILL
        // inside when it elapses). With WA3 FIELD OF VIEW (single-rank, O272)
        // it lands on this edge, frame one — the same instant/delayed split as
        // the node's slow clause. One shared key: the 10s cooldown against the
        // 6s zone means two Suppress fields never coexist, and keyed replace
        // would merely refresh if they did.
        if (FieldOfViewRank >= 1 || AccuracyApplyDelaySeconds <= 0.0f)
        {
            ApplyAccuracyCut(Enemy);
        }
        else if (UWorld* World = GetWorld())
        {
            const TWeakObjectPtr<AActor> WeakEnemy = Enemy;
            FTimerHandle DelayHandle;
            World->GetTimerManager().SetTimer(DelayHandle,
                FTimerDelegate::CreateWeakLambda(this, [this, WeakEnemy]()
                {
                    // Still inside? SlowedEnemies is the occupancy ledger; an
                    // exited (or expired — the ledger resets) enemy shoots
                    // straight again and must not receive a late cut.
                    if (AActor* Still = WeakEnemy.Get())
                    {
                        if (SlowedEnemies.Contains(WeakEnemy)) ApplyAccuracyCut(Still);
                    }
                }),
                AccuracyApplyDelaySeconds, false);
        }
    }
}

void UBreakerAbility_Suppress::ApplyAccuracyCut(AActor* Occupant)
{
    if (ABreakerEnemy* Enemy = Cast<ABreakerEnemy>(Occupant))
    {
        Enemy->PushAimErrorMultiplier(AccuracyModifierKey(), FMath::Max(1.0f, SuppressAccuracyMultiplier));
        AccuracyCutEnemies.AddUnique(Enemy);
    }
}

void UBreakerAbility_Suppress::HandleOccupantExited(AActor* Occupant)
{
    if (ABreakerEnemy* Enemy = Cast<ABreakerEnemy>(Occupant))
    {
        Enemy->ApplyModifierMovementProfile(1.0f, -1.0f);
        SlowedEnemies.Remove(Enemy);
        if (AccuracyCutEnemies.Remove(Enemy) > 0)
        {
            Enemy->PopAimErrorMultiplier(AccuracyModifierKey());
        }
    }
}

void UBreakerAbility_Suppress::HandleZoneExpired()
{
    InvalidatePressure();
    for (const TWeakObjectPtr<AActor>& Slowed : SlowedEnemies)
    {
        if (ABreakerEnemy* Enemy = Cast<ABreakerEnemy>(Slowed.Get()))
        {
            Enemy->ApplyModifierMovementProfile(1.0f, -1.0f);
        }
    }
    SlowedEnemies.Reset();
    // The zone dying must not leave anyone aiming wide forever.
    for (const TWeakObjectPtr<AActor>& Cut : AccuracyCutEnemies)
    {
        if (ABreakerEnemy* Enemy = Cast<ABreakerEnemy>(Cut.Get()))
        {
            Enemy->PopAimErrorMultiplier(AccuracyModifierKey());
        }
    }
    AccuracyCutEnemies.Reset();
    ActiveZone = nullptr;
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(PressureTimer);
    }
}

// ---------------------------------------------------------------------------
// ULTIMATE — CONDUIT
// ---------------------------------------------------------------------------

UBreakerAbility_Conduit::UBreakerAbility_Conduit()
{
    FallbackAbilityId = TEXT("Support.Conduit");
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

FName UBreakerAbility_Conduit::BlackoutModifierKey() { return TEXT("Conduit.Blackout"); }
FName UBreakerAbility_Conduit::DownbeatModifierKey() { return TEXT("Conduit.Downbeat"); }

void UBreakerAbility_Conduit::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
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
    const float Duration = Variant.WindowDuration > 0.0f ? Variant.WindowDuration : 12.0f;

    const bool bTriage = Variant.KeystoneTag == FGameplayTag::RequestGameplayTag(TEXT("Keystone.Support.Triage"), false);
    const bool bDownbeat = Variant.KeystoneTag == FGameplayTag::RequestGameplayTag(TEXT("Keystone.Support.Downbeat"), false);
    const bool bBlackout = Variant.KeystoneTag == FGameplayTag::RequestGameplayTag(TEXT("Keystone.Support.Blackout"), false);

    // The window CARRIES the cost scalar (0.0 base and Downbeat, 1.0 Triage
    // and Blackout — the variant rows author it), so the ultimate and the
    // abilities it discounts cannot drift apart. Generation deliberately
    // CONTINUES: a well-played window partially refunds itself, bounded by
    // cooldowns (§3.1).
    if (UBreakerAbilityStateComponent* State = UBreakerAbilityStateComponent::FindOrAdd(Character))
    {
        State->StartWindowWithPayload(ConduitWindowKey(), Duration, Variant.AbilityCostMultiplier);
    }
    bConduitActive = true;
    Character->GetCombat()->OnDeath.AddUniqueDynamic(this, &ThisClass::HandleConduitDeath);
    World->GetTimerManager().SetTimer(WindowTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { bNaturalConduitEnd = true; CloseConduit(); }), Duration, false);
    // The ultimates' violet ignition (Overdrive's precedent), feet-anchored.
    if (ABreakerEffectRenderer* Effects = ABreakerEffectRenderer::FindOrSpawn(World))
    {
        const FVector Centre = Character->GetActorLocation();
        const FVector Feet = Centre - FVector(0.0f, 0.0f, Character->GetSimpleCollisionHalfHeight() * 0.8f);
        BreakerFX::FEffectTiming BurstTiming;
        BurstTiming.DurationSeconds = 0.55f;
        BurstTiming.FadeInSeconds = 0.02f;
        BurstTiming.FadeOutSeconds = 0.40f;
        Effects->AddGlow(Feet, 70.0f, BreakerUI::Violet, 3.6f, BurstTiming);
        Effects->AddBlinkLight(Centre, 650.0f, BreakerUI::Violet, 3600.0f, BurstTiming);
        for (int32 Index = 0; Index < 6; ++Index)
        {
            const FVector Out = FRotator(0.0f, 60.0f * Index, 0.0f).Vector();
            Effects->AddStroke(Feet + Out * 40.0f, Feet + Out * 150.0f, 4.5f, BreakerUI::Violet, 2.8f, BurstTiming, 0.03f * Index);
        }
    }

    if (bTriage)
    {
        TriageOwnerKey = FName(*FString::Printf(TEXT("Conduit.Triage.%u.%u"), GetUniqueID(), ++TriageCastSerial));
        TriageEndTime = World->GetTimeSeconds() + Duration;
        FBreakerZoneSpec Visual;
        Visual.RadiusCm = RadiusCm; Visual.Duration = Duration;
        Visual.bMobileFootprint = true; Visual.bShowFilledFootprint = false; Visual.ZoneColor = BreakerUI::Gold;
        FActorSpawnParameters Spawn; Spawn.Owner = Character;
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        const FVector Feet = Character->GetActorLocation() - FVector(0, 0, Character->GetSimpleCollisionHalfHeight());
        TriageBoundary = World->SpawnActor<ABreakerZoneActor>(ABreakerZoneActor::StaticClass(), Feet, FRotator::ZeroRotator, Spawn);
        if (TriageBoundary) { TriageBoundary->ConfigureZone(Visual, Character); TriageBoundary->SetFollowActor(Character); }
        RefreshTriageRecipients();
        World->GetTimerManager().SetTimer(TriageRecipientsTimer, this, &ThisClass::RefreshTriageRecipients, .05f, true);
        World->GetTimerManager().SetTimer(TriageTimer, this, &ThisClass::HandleTriagePulse, 1.0f, true);
    }
    else if (bDownbeat)
    {
        // §3.1 DOWNBEAT: free casts stay; live Cadence doubles tempo bonuses
        // while this window is active. Every buffed target adds FLAT damage — solo,
        // the live unique holders published by this caster determine the count.
        if (UBreakerCombatComponent* Combat = Character->FindComponentByClass<UBreakerCombatComponent>())
        {
            DownbeatOwnerKey = FName(*FString::Printf(TEXT("Conduit.Downbeat.%u"), GetUniqueID()));
            Combat->PushWindowWeaponFlatDamage(DownbeatOwnerKey, 0.0f, Duration);
            if (auto* State = UBreakerAbilityStateComponent::FindOrAdd(Character))
                State->OnMaintainedBuffRecipientsChanged.AddUniqueDynamic(this, &ThisClass::RefreshDownbeat);
            RefreshDownbeat();
            World->GetTimerManager().SetTimer(DownbeatTimer, this, &ThisClass::RefreshDownbeat, .05f, true);
        }
    }
    else if (bBlackout)
    {
        // §3.1 BLACKOUT: marks and suppresses every enemy in radius INSTEAD
        // of casting abilities, and the marks are YOURS for generation.
        UBreakerCombatComponent* OwnCombat = Character->FindComponentByClass<UBreakerCombatComponent>();
        for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
        {
            ABreakerEnemy* Enemy = *It;
            if (!Enemy) continue;
            UBreakerCombatComponent* EnemyCombat = Enemy->FindComponentByClass<UBreakerCombatComponent>();
            if (!EnemyCombat || EnemyCombat->IsDead()) continue;
            if (FVector::DistSquared(Character->GetActorLocation(), Enemy->GetActorLocation()) > RadiusCm * RadiusCm) continue;
            EnemyCombat->PushIncomingDamageModifier(BlackoutModifierKey(), BlackoutMarkedDamageMultiplier);
            Enemy->ApplyModifierMovementProfile(BlackoutSlowMultiplier, -1.0f);
            BlackoutTargets.Add(Enemy);
        }
        if (OwnCombat)
        {
            BoundCombat = OwnCombat;
            OwnCombat->OnHitDealt.AddDynamic(this, &UBreakerAbility_Conduit::HandleBlackoutHit);
        }
    }
}

void UBreakerAbility_Conduit::RefreshDownbeat()
{
    if (!bConduitActive) return;
    ABreakerCharacter* Character = GetBreakerCharacter();
    if (!Character || Character->IsActorBeingDestroyed() || Character->GetCombat()->IsDead()) { CloseConduit(); return; }
    const auto* State = Character->FindComponentByClass<UBreakerAbilityStateComponent>();
    const bool bEnabled = State && State->IsWindowActive(ConduitWindowKey())
        && SupportHasNode(Character, FGameplayTag::RequestGameplayTag(TEXT("Keystone.Support.Downbeat"), false));
    Character->GetCombat()->UpdateWindowWeaponFlatDamage(DownbeatOwnerKey,
        bEnabled ? DownbeatFlatDamagePerBuffedTarget * State->GetMaintainedBuffRecipientCount() : 0.0f);
}

void UBreakerAbility_Conduit::HandleConduitDeath() { CloseConduit(); }
void UBreakerAbility_Conduit::RefreshTriageRecipients()
{
    if (!bConduitActive) return;
    ABreakerCharacter* Character = GetBreakerCharacter();
    if (!Character || Character->GetCombat()->IsDead()) { CloseConduit(); return; }
    UWorld* World = Character->GetWorld();
    const float Remaining = static_cast<float>(TriageEndTime - World->GetTimeSeconds());
    if (Remaining <= 0) return;
    for (TActorIterator<ABreakerCharacter> It(World); It; ++It)
    {
        ABreakerCharacter* Recipient = *It;
        if (Recipient->IsActorBeingDestroyed() || Recipient->GetCombat()->IsDead()) continue;
        // Register outside the field too; the damage seam checks the current
        // distance, so entering between maintenance ticks is still protected.
        Recipient->GetCombat()->GrantLethalSave(TriageOwnerKey, Character, Remaining, RadiusCm);
        TriageRecipients.Add(Recipient);
    }
}

void UBreakerAbility_Conduit::HandleTriagePulse()
{
    RefreshTriageRecipients();
    if (!bConduitActive) return;
    ABreakerCharacter* Character = GetBreakerCharacter();
    if (!Character || !GetWorld() || GetWorld()->GetTimeSeconds() >= TriageEndTime) return;
    const bool bOverflow = SupportHasNode(Character, BreakerNodeTags::Node_MD_Overflow.GetTag());
    const auto Snapshot = TriageRecipients.Array();
    for (const auto& Weak : Snapshot)
    {
        if (!bConduitActive || Character->IsActorBeingDestroyed() || Character->GetCombat()->IsDead()) break;
        if (ABreakerCharacter* Recipient = Weak.Get())
            if (!Recipient->IsActorBeingDestroyed() && !Recipient->GetCombat()->IsDead()
                && FVector::DistSquared(Character->GetActorLocation(), Recipient->GetActorLocation()) <= FMath::Square(RadiusCm))
                BreakerSupportAbilityLocal::BreakerSupportHealAndCredit(Character, Recipient,
                    BreakerSupportAbilityLocal::BreakerSupportTargetMaxHealth(Recipient) * TriageHealFractionPerSecond, 1.0f, bOverflow);
    }
}
void UBreakerAbility_Conduit::HandleBlackoutHit(const FBreakerHitContext& Hit)
{
    if (Hit.bFundedWeaponSplash) return;
    if (!bConduitActive || !Hit.Target || !BlackoutTargets.Contains(Hit.Target)) return;
    if (ABreakerCharacter* Character = GetBreakerCharacter())
    {
        if (UBreakerChargeComponent* Charge = Character->FindComponentByClass<UBreakerChargeComponent>())
        {
            Charge->NotifyMarkedTargetDamage(Hit.Result.HealthDamage + Hit.Result.ShieldDamage,
                BreakerSupportAbilityLocal::BreakerSupportTargetMaxHealth(Hit.Target));
        }
    }
}

void UBreakerAbility_Conduit::OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
    // Revoke before end callbacks can activate a replacement on this source.
    if (auto* Character = GetBreakerCharacter()) Character->GetCombat()->PopWeaponFlatDamage(DownbeatOwnerKey);
    bNaturalConduitEnd = false;
    if (IsActive()) CloseConduit();
    Super::OnRemoveAbility(ActorInfo, Spec);
}

void UBreakerAbility_Conduit::CloseConduit()
{
    if (CurrentActorInfo)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
    }
}

void UBreakerAbility_Conduit::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    const bool bKeepTail = bNaturalConduitEnd && !bWasCancelled;
    bNaturalConduitEnd = false;
    if (bConduitActive)
    {
        bConduitActive = false;
        for (const auto& Weak : TriageRecipients) if (auto* Recipient = Weak.Get()) Recipient->GetCombat()->RemoveLethalSave(TriageOwnerKey);
        TriageRecipients.Reset();
        if (IsValid(TriageBoundary)) TriageBoundary->Destroy();
        TriageBoundary = nullptr;
        ABreakerCharacter* Character = GetBreakerCharacter();
        if (Character)
        {
            if (UBreakerAbilityStateComponent* State = Character->FindComponentByClass<UBreakerAbilityStateComponent>())
            {
                State->OnMaintainedBuffRecipientsChanged.RemoveDynamic(this, &ThisClass::RefreshDownbeat);
                State->CloseWindow(ConduitWindowKey());
            }
            if (UBreakerCombatComponent* Combat = Character->FindComponentByClass<UBreakerCombatComponent>())
            {
                if (bKeepTail) Combat->FinishWindowWeaponFlatDamage(DownbeatOwnerKey);
                else Combat->PopWeaponFlatDamage(DownbeatOwnerKey);
                Combat->OnDeath.RemoveDynamic(this, &ThisClass::HandleConduitDeath);
            }
        }
        for (const TWeakObjectPtr<AActor>& Target : BlackoutTargets)
        {
            if (AActor* Enemy = Target.Get())
            {
                if (UBreakerCombatComponent* EnemyCombat = Enemy->FindComponentByClass<UBreakerCombatComponent>())
                {
                    EnemyCombat->RemoveIncomingDamageModifier(BlackoutModifierKey());
                }
                if (ABreakerEnemy* AsEnemy = Cast<ABreakerEnemy>(Enemy))
                {
                    AsEnemy->ApplyModifierMovementProfile(1.0f, -1.0f);
                }
            }
        }
        BlackoutTargets.Reset();
        if (UBreakerCombatComponent* Combat = BoundCombat.Get())
        {
            Combat->OnHitDealt.RemoveDynamic(this, &UBreakerAbility_Conduit::HandleBlackoutHit);
        }
        BoundCombat.Reset();
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(WindowTimer);
            World->GetTimerManager().ClearTimer(TriageTimer);
            World->GetTimerManager().ClearTimer(TriageRecipientsTimer);
            World->GetTimerManager().ClearTimer(DownbeatTimer);
        }
    }
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
