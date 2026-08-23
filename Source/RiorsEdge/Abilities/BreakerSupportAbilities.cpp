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
#include "Combat/BreakerStatusComponent.h"
#include "Combat/BreakerZoneActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameplayEffect.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "TimerManager.h"

namespace BreakerSupportAbilityLocal
{
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
                Convert.Amount = Unrouted * 0.5f;   // O2 PLACEHOLDER ("a fraction of its value")
                Convert.bOverhealToShield = true;
                Convert.SetHealer(Healer);
                OverflowShield = TargetCombat->ApplyHealing(Convert).ShieldGranted;
            }
        }
        if (Charge) Charge->EndSupportHealScope();

        // Self-heals credit here, explicitly, at the true proc coefficient.
        // Ally heals credit through the character's OnHealingDealt wiring
        // (recorded gap there: ally credits run at 1.0 until heal contexts
        // carry a coefficient). The overheal pays NOTHING; the shield it
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
                Charge->NotifyShieldingDone(ShieldPaid, 0.0f, TargetMax, true);
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

float UBreakerSupportAbility::GetResourceCost() const
{
    const float Authored = Super::GetResourceCost();
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
    return Progression ? Progression->GetNodeRank(FName(NodeId), EBreakerPointCurrency::CorePoints) : 0;
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
    const bool bAnyBuff = State->IsWindowActive(UBreakerAbility_Cadence::WindowKey())
        || State->IsWindowActive(UBreakerAbility_Metronome::WindowKey());
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

float UBreakerAbility_Patch::GetResourceCost() const
{
    // MD11 NO TRIAGE: far cheaper (half, O2 PLACEHOLDER). The self-only and
    // shorter-cooldown halves live in ActivateAbility.
    const float Authored = Super::GetResourceCost();
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
        // (R2's Purge-immunity scaling WAITS: the immunity window has no
        // magnitude to scale — it is a duration, owned by Purge.)
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
            // R2: half of it echoes onto you. Solo (target == self) the echo is
            // vacuous by construction, exactly as the treatment guards.
            if (SecondOpinionRank >= 2 && Target != Character)
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

        // MD6 ATTENDING: healing while your mark is live also pays the
        // marked-target source at the damage rate (R2: and refreshes the mark).
        const int32 AttendingRank = SupportNodeRank(Character, TEXT("Support.Medic.Attending"));
        if (AttendingRank > 0)
        {
            if (UBreakerAbilityStateComponent* State = Character->FindComponentByClass<UBreakerAbilityStateComponent>())
            {
                if (AActor* Marked = State->GetMarkedTarget())
                {
                    if (UBreakerChargeComponent* Charge = Character->FindComponentByClass<UBreakerChargeComponent>())
                    {
                        Charge->NotifyMarkedTargetDamage(Amount, BreakerSupportTargetMaxHealth(Marked));
                    }
                    if (AttendingRank >= 2)
                    {
                        State->SetMark(Marked, FMath::Max(State->GetMarkRemaining(), 10.0f));   // refresh, never shorten
                    }
                }
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

float UBreakerAbility_Purge::GetResourceCost() const
{
    // MD11 NO TRIAGE, the Patch twin: far cheaper (half, O2 PLACEHOLDER).
    const float Authored = Super::GetResourceCost();
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
            // MD3 CLEAN HANDS: cooldown refunds per status ACTUALLY removed
            // (R2: doubled), bounded by the cleanse source's own 0.5s ICD —
            // which the Charge component's cleanse interval already enforces.
            const int32 CleanHandsRank = SupportNodeRank(Character, TEXT("Support.Medic.CleanHands"));
            if (CleanHandsRank > 0)
            {
                const float RefundPerStatus = CleanHandsRank >= 2 ? 2.0f : 1.0f;   // O2 PLACEHOLDER
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
            Status->GrantStatusImmunity(Definition && Definition->WindowDuration > 0.0f ? Definition->WindowDuration : 3.0f);   // §U2: 3s
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
    const UBreakerAbilityDefinition* Definition = GetAbilityDefinition();
    ABreakerCharacter* Character = GetBreakerCharacter();
    UWorld* World = Character ? Character->GetWorld() : nullptr;
    if (!World || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    UBreakerChargeComponent* Charge = Character->FindComponentByClass<UBreakerChargeComponent>();

    // CO4 REHEARSAL: re-applying a live Conductor buff refunds part of its
    // cost (R2: a larger refund). The refresh itself is the retrigger path.
    const int32 RehearsalRank = SupportNodeRank(Character, TEXT("Support.Conductor.Rehearsal"));
    if (RehearsalRank > 0 && bReappliedWhileLive && Charge)
    {
        Charge->GrantCharge(GetResourceCost() * (RehearsalRank >= 2 ? 0.5f : 0.25f));   // O2 PLACEHOLDER
    }
    bReappliedWhileLive = false;

    // CO1 DOWNBEAT DISCIPLINE: your own copy outlasts the handed-out copies —
    // solo, the only copy IS yours, so the node reads as +2s (R2: +4s).
    float Duration = Definition ? Definition->WindowDuration : 8.0f;
    const int32 DisciplineRank = SupportNodeRank(Character, TEXT("Support.Conductor.DownbeatDiscipline"));
    if (DisciplineRank > 0) Duration += DisciplineRank >= 2 ? 4.0f : 2.0f;   // O2 PLACEHOLDER
    // CO9 STANDING OVATION: at Resonant the buff lands extended. The
    // cannot-be-stripped half is structurally true already — no enemy strips
    // buffs — and stands recorded as such.
    if (SupportHasNode(Character, BreakerNodeTags::Node_CO_StandingOvation.GetTag())
        && Charge && Charge->GetChargeBand() == EBreakerChargeBand::Resonant)
    {
        Duration *= 1.5f;   // O2 PLACEHOLDER ("extended")
    }

    // CO11 DETACHED BATON: planted as a much larger STATIONARY zone which no
    // longer applies to you first — your own copy runs only while you stand
    // inside it. A party trade, declined solo, exactly as authored.
    if (SupportHasNode(Character, BreakerNodeTags::Node_CO_DetachedBaton.GetTag()))
    {
        FBreakerZoneSpec Spec;
        Spec.ZoneTag = FGameplayTag::RequestGameplayTag(TEXT("Zone.Support.Cadence"), false);
        Spec.RadiusCm = DetachedBatonRadiusCm;
        Spec.Duration = Duration;
        Spec.TickInterval = 1.0f;
        Spec.ZoneColor = FLinearColor(0.85f, 0.75f, 0.3f);   // brass; teal reserved (O19)
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        SpawnParams.Owner = Character;
        BatonZone = World->SpawnActor<ABreakerZoneActor>(ABreakerZoneActor::StaticClass(), Character->GetActorLocation(), FRotator::ZeroRotator, SpawnParams);
        if (BatonZone)
        {
            BatonZone->ConfigureZone(Spec, Character);
            BatonZone->OnOccupantEntered.AddDynamic(this, &UBreakerAbility_Cadence::HandleBatonOccupantEntered);
            BatonZone->OnOccupantExited.AddDynamic(this, &UBreakerAbility_Cadence::HandleBatonOccupantExited);
            BatonZone->OnZoneExpired.AddDynamic(this, &UBreakerAbility_Cadence::HandleBatonZoneExpired);
        }
        // No self-first window here; entering the zone opens it.
    }
    else if (UBreakerAbilityStateComponent* State = UBreakerAbilityStateComponent::FindOrAdd(Character))
    {
        State->StartWindow(WindowKey(), Duration);
    }
    // Being a BUFF, it drives the count-independent uptime source (§U3) —
    // that, plus the HUD window, is the whole shipped payload; the tempo half
    // is the header's recorded gap.
    RefreshBuffUptime(Character);
    bCadenceActive = true;

    // CO7 CONDUCTING: Cadence also speeds ability cooldown recovery for the
    // buffed — solo, you. A once-a-second shave through the Charge component's
    // GAS path while the buff runs. The detach-tail clause is vacuous while
    // the aura follows its caster and stands recorded.
    if (SupportHasNode(Character, BreakerNodeTags::Node_CO_Conducting.GetTag()))
    {
        World->GetTimerManager().SetTimer(ConductingTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { ShaveTick(); }), 1.0f, /*bLoop=*/true);
    }

    World->GetTimerManager().SetTimer(WindowTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
    {
        if (CurrentActorInfo) EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
    }), Duration, false);
}

void UBreakerAbility_Cadence::ShaveTick()
{
    ABreakerCharacter* Character = GetBreakerCharacter();
    UBreakerChargeComponent* Charge = Character ? Character->FindComponentByClass<UBreakerChargeComponent>() : nullptr;
    // The buffed set is "whoever the buff is on" — solo, you, and only while
    // the window is actually open (the Detached Baton zone can close it).
    const UBreakerAbilityStateComponent* State = Character ? Character->FindComponentByClass<UBreakerAbilityStateComponent>() : nullptr;
    if (Charge && State && State->IsWindowActive(WindowKey()))
    {
        Charge->ShaveAllAbilityCooldowns(0.25f);   // O2 PLACEHOLDER
    }
}

void UBreakerAbility_Cadence::HandleBatonOccupantEntered(AActor* Occupant)
{
    ABreakerCharacter* Character = GetBreakerCharacter();
    if (!Character || Occupant != Character || !BatonZone) return;
    if (UBreakerAbilityStateComponent* State = UBreakerAbilityStateComponent::FindOrAdd(Character))
    {
        State->StartWindow(WindowKey(), BatonZone->GetRemainingDuration());
    }
    RefreshBuffUptime(Character);
}

void UBreakerAbility_Cadence::HandleBatonOccupantExited(AActor* Occupant)
{
    ABreakerCharacter* Character = GetBreakerCharacter();
    if (!Character || Occupant != Character) return;
    if (UBreakerAbilityStateComponent* State = Character->FindComponentByClass<UBreakerAbilityStateComponent>())
    {
        State->CloseWindow(WindowKey());
    }
    RefreshBuffUptime(Character);
}

void UBreakerAbility_Cadence::HandleBatonZoneExpired()
{
    BatonZone = nullptr;
}

void UBreakerAbility_Cadence::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (bCadenceActive)
    {
        bCadenceActive = false;
        if (ABreakerCharacter* Character = GetBreakerCharacter())
        {
            if (UBreakerAbilityStateComponent* State = Character->FindComponentByClass<UBreakerAbilityStateComponent>())
            {
                // CO4's re-application detection: a teardown that finds real
                // time still on the window is a refresh, not an expiry.
                bReappliedWhileLive = State->GetWindowRemaining(WindowKey()) > 0.1f;
                State->CloseWindow(WindowKey());
            }
            RefreshBuffUptime(Character);
        }
        if (BatonZone)
        {
            BatonZone->Destroy();
            BatonZone = nullptr;
        }
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(WindowTimer);
            World->GetTimerManager().ClearTimer(ConductingTimer);
        }
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
    // CO4 Rehearsal: re-applying a live buff must be a legal cast.
    bRetriggerInstancedAbility = true;
}

FName UBreakerAbility_Metronome::WindowKey() { return TEXT("Window.Support.Metronome"); }
FName UBreakerAbility_Metronome::OutgoingModifierKey() { return TEXT("Metronome"); }

void UBreakerAbility_Metronome::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
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

    UBreakerChargeComponent* Charge = Character->FindComponentByClass<UBreakerChargeComponent>();

    // CO4 REHEARSAL: a re-application refreshes STACKS INTACT and refunds part
    // of the cost (R2: larger). Without the node a re-cast resets the ramp,
    // exactly as before.
    const int32 RehearsalRank = SupportNodeRank(Character, TEXT("Support.Conductor.Rehearsal"));
    const bool bRehearsalRefresh = RehearsalRank > 0 && bReappliedWhileLive;
    if (bRehearsalRefresh && Charge)
    {
        Charge->GrantCharge(GetResourceCost() * (RehearsalRank >= 2 ? 0.5f : 0.25f));   // O2 PLACEHOLDER
    }
    bReappliedWhileLive = false;

    // CO1 DOWNBEAT DISCIPLINE and CO9 STANDING OVATION, the Cadence twins.
    float Duration = Definition ? Definition->WindowDuration : 8.0f;
    const int32 DisciplineRank = SupportNodeRank(Character, TEXT("Support.Conductor.DownbeatDiscipline"));
    if (DisciplineRank > 0) Duration += DisciplineRank >= 2 ? 4.0f : 2.0f;   // O2 PLACEHOLDER
    if (SupportHasNode(Character, BreakerNodeTags::Node_CO_StandingOvation.GetTag())
        && Charge && Charge->GetChargeBand() == EBreakerChargeBand::Resonant)
    {
        Duration *= 1.5f;   // O2 PLACEHOLDER
    }

    if (UBreakerAbilityStateComponent* State = UBreakerAbilityStateComponent::FindOrAdd(Character))
    {
        State->StartWindow(WindowKey(), Duration);
    }
    RefreshBuffUptime(Character);
    if (!bRehearsalRefresh)
    {
        Stacks = 0;
        LastHitTime = -1000.0;
    }
    BoundCombat = Combat;
    Combat->OnHitDealt.AddDynamic(this, &UBreakerAbility_Metronome::HandleHitDealt);
    bMetronomeActive = true;
    World->GetTimerManager().SetTimer(WindowTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { CloseMetronome(); }), Duration, false);
}

void UBreakerAbility_Metronome::HandleHitDealt(const FBreakerHitContext& Hit)
{
    if (!bMetronomeActive) return;
    UWorld* World = GetWorld();
    if (!World) return;
    ABreakerCharacter* Character = GetBreakerCharacter();
    // CO8 COUNTERPOINT: the ramp climbs from ANY damage the buffed target
    // deals — ticks included, at their own cadence. WITHOUT it, DoT ticks do
    // not count (§U4 authors weapon hits; a bleed climbing the ramp on its own
    // was over-generous). RECORDED LIMIT: the hit context cannot tell a weapon
    // hit from an ability hit, so that finer cut waits on a context tag.
    const bool bCounterpoint = SupportHasNode(Character, BreakerNodeTags::Node_CO_Counterpoint.GetTag());
    if (Hit.bFromDoT && !bCounterpoint) return;
    const double Now = World->GetTimeSeconds();
    // §U4: a full second without a hit resets the ramp. PER HOLDER — solo the
    // Support's own ramp is the only one, and it is not shared or averaged.
    // CO5 TEMPO: the ramp stacks higher and resets slower for YOU specifically
    // (R2's everyone-you-buffed clause waits on a party existing).
    const int32 TempoRank = SupportNodeRank(Character, TEXT("Support.Conductor.Tempo"));
    const float EffectiveGap = TempoRank > 0 ? StreakGapSeconds * 1.5f : StreakGapSeconds;   // O2 PLACEHOLDER
    const int32 EffectiveMaxStacks = TempoRank > 0 ? MaximumStacks + 3 : MaximumStacks;      // O2 PLACEHOLDER
    if (Now - LastHitTime > EffectiveGap)
    {
        Stacks = 0;
    }
    LastHitTime = Now;
    Stacks = FMath::Min(Stacks + 1, EffectiveMaxStacks);
    if (UBreakerCombatComponent* Combat = BoundCombat.Get())
    {
        // Re-pushing the key replaces the entry, so the ramp reads as one
        // growing flat contribution, never a stack of modifiers.
        Combat->PushOutgoingModifier(OutgoingModifierKey(), FlatDamagePerStack * Stacks, 1.0f, EffectiveGap);
    }
}

void UBreakerAbility_Metronome::CloseMetronome()
{
    if (CurrentActorInfo)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
    }
}

void UBreakerAbility_Metronome::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (bMetronomeActive)
    {
        bMetronomeActive = false;
        if (UBreakerCombatComponent* Combat = BoundCombat.Get())
        {
            Combat->OnHitDealt.RemoveDynamic(this, &UBreakerAbility_Metronome::HandleHitDealt);
            Combat->RemoveOutgoingModifier(OutgoingModifierKey());
        }
        BoundCombat.Reset();
        if (ABreakerCharacter* Character = GetBreakerCharacter())
        {
            if (UBreakerAbilityStateComponent* State = Character->FindComponentByClass<UBreakerAbilityStateComponent>())
            {
                // CO4's re-application detection, the Cadence twin.
                bReappliedWhileLive = State->GetWindowRemaining(WindowKey()) > 0.1f;
                State->CloseWindow(WindowKey());
            }
            RefreshBuffUptime(Character);
        }
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(WindowTimer);
        }
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

float UBreakerAbility_Mark::GetResourceCost() const
{
    // WA11 HUNTER'S ECONOMY: Mark costs nothing — the floor-recovery answer.
    // (The much-shorter leash is applied at activation.)
    return SupportHasNode(GetBreakerCharacter(), BreakerNodeTags::Node_WA_HuntersEconomy.GetTag()) ? 0.0f : Super::GetResourceCost();
}

FName UBreakerAbility_Mark::IncomingModifierKey() { return TEXT("Support.Mark"); }
FName UBreakerAbility_Mark::TellModifierKey() { return TEXT("Support.Mark.Tell"); }

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
    if (World->LineTraceSingleByChannel(Hit, ViewLocation, ViewLocation + ViewRotation.Vector() * TargetRangeCm, ECC_Pawn, QueryParams))
    {
        if (Cast<ABreakerEnemy>(Hit.GetActor())) Target = Hit.GetActor();
    }
    UBreakerCombatComponent* TargetCombat = Target ? Target->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
    if (!TargetCombat)
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

    // WA2 LONG WATCH: marks last longer (+4s, R2: +8s — O2 PLACEHOLDER), and
    // re-marking a still-marked target spends no cooldown. The duration seam
    // (AbilityDurationMultiplierFor) is adopted at the same site.
    const int32 LongWatchRank = SupportNodeRank(Character, TEXT("Support.Warden.LongWatch"));
    float Duration = (Definition ? Definition->WindowDuration : 10.0f) + (LongWatchRank >= 2 ? 8.0f : (LongWatchRank == 1 ? 4.0f : 0.0f));
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
    }
    // "Takes more damage from ALL SOURCES including allies" is structural: the
    // keyed incoming modifier sits on the TARGET, so every damage request from
    // anyone passes through it (§U5). Deep Mark rides the same key.
    TargetCombat->PushIncomingDamageModifier(IncomingModifierKey(), MarkedDamageMultiplier + DeepMarkDamagePerDepth * MarkDepth);

    // WA6 TELL, the softening half: while the mark lives the marked enemy hits
    // you softer — a keyed push on the ENEMY's outgoing-damage seam, popped
    // wherever the incoming key pops so the two halves cannot drift. Solo the
    // Support is the only target an enemy has, so mark-lifetime scope IS
    // "while it attacks you" (R2's allies clause waits on a party existing).
    // The telegraph half of the node text is carried by the enemies' universal
    // wind-up tells; a mark-scoped EXTRA telegraph read remains recorded
    // absent — there is no HUD surface for it yet.
    if (SupportHasNode(Character, BreakerNodeTags::Node_WA_Tell.GetTag()))
    {
        if (ABreakerEnemy* MarkedEnemy = Cast<ABreakerEnemy>(Target))
        {
            MarkedEnemy->PushOutgoingDamageMultiplier(TellModifierKey(), TellOutgoingMultiplier);
        }
    }

    MarkedTarget = Target;
    if (UBreakerCombatComponent* OwnCombat = Character->FindComponentByClass<UBreakerCombatComponent>())
    {
        BoundCombat = OwnCombat;
        OwnCombat->OnHitDealt.AddDynamic(this, &UBreakerAbility_Mark::HandleHitDealt);
    }
    bMarkActive = true;
    World->GetTimerManager().SetTimer(MarkTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { CloseMark(); }), Duration, false);
}

void UBreakerAbility_Mark::PointMarkAt(AActor* NewTarget, float Duration)
{
    // WA4's jump: move the paint, the modifier and the clock to the new
    // target. The jump itself pays NOTHING — no credit call lives here.
    ABreakerCharacter* Character = GetBreakerCharacter();
    UWorld* World = Character ? Character->GetWorld() : nullptr;
    if (!Character || !World || !NewTarget) return;
    if (AActor* Old = MarkedTarget.Get())
    {
        if (UBreakerCombatComponent* OldCombat = Old->FindComponentByClass<UBreakerCombatComponent>())
        {
            OldCombat->RemoveIncomingDamageModifier(IncomingModifierKey());
        }
        // WA6: the softening travels with the paint (pop of an unpushed key is
        // a no-op for a build without the node).
        if (ABreakerEnemy* OldEnemy = Cast<ABreakerEnemy>(Old))
        {
            OldEnemy->PopOutgoingDamageMultiplier(TellModifierKey());
        }
    }
    MarkDepth = 0;   // a jumped mark lands shallow
    if (UBreakerCombatComponent* NewCombat = NewTarget->FindComponentByClass<UBreakerCombatComponent>())
    {
        NewCombat->PushIncomingDamageModifier(IncomingModifierKey(), MarkedDamageMultiplier);
    }
    if (SupportHasNode(Character, BreakerNodeTags::Node_WA_Tell.GetTag()))
    {
        if (ABreakerEnemy* NewEnemy = Cast<ABreakerEnemy>(NewTarget))
        {
            NewEnemy->PushOutgoingDamageMultiplier(TellModifierKey(), TellOutgoingMultiplier);
        }
    }
    if (UBreakerAbilityStateComponent* State = UBreakerAbilityStateComponent::FindOrAdd(Character))
    {
        State->SetMark(NewTarget, Duration);
    }
    MarkedTarget = NewTarget;
    World->GetTimerManager().SetTimer(MarkTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { CloseMark(); }), Duration, false);
}

void UBreakerAbility_Mark::HandleHitDealt(const FBreakerHitContext& Hit)
{
    if (!bMarkActive || Hit.Target != MarkedTarget.Get() || !Hit.Target) return;
    ABreakerCharacter* Character = GetBreakerCharacter();
    if (!Character) return;
    // WA1 PAINTED: marked-target Charge pays on ability and DoT damage, not
    // weapon hits alone. WITHOUT the node, DoT ticks pay nothing (§1.1 authors
    // the weapon-hit source; ticks riding it for free was over-generous).
    // RECORDED LIMIT: weapon-vs-ability is indistinguishable on the context,
    // so that finer cut waits on a context tag; the DoT half is the honest cut
    // available today. R2's allied-damage clause waits on a party existing.
    const bool bPainted = SupportHasNode(Character, BreakerNodeTags::Node_WA_Painted.GetTag());
    if (Hit.bFromDoT && !bPainted) return;
    // §1.1: damage YOU deal to your mark generates, at +1 per 2% of the
    // TARGET'S maximum health — a boss pays the same total for the same
    // fraction, so the bar is bounded and predictable. WA8 deepens the yield.
    if (UBreakerChargeComponent* Charge = Character->FindComponentByClass<UBreakerChargeComponent>())
    {
        const float YieldScale = 1.0f + DeepMarkYieldPerDepth * MarkDepth;
        Charge->NotifyMarkedTargetDamage((Hit.Result.HealthDamage + Hit.Result.ShieldDamage) * YieldScale,
            BreakerSupportAbilityLocal::BreakerSupportTargetMaxHealth(Hit.Target));

        // MD10 BLOOD DEBT: the next weapon hit on a marked target spends the
        // banked pool as flat damage — a one-shot settlement request, flat
        // bucket, no crit, proc 0, so it can neither double-dip nor seed.
        const float Debt = Charge->GetBloodDebtPool();
        if (Debt > 0.0f && !Hit.bFromDoT && !Hit.Result.bKilled)
        {
            if (UBreakerCombatComponent* TargetCombat = Hit.Target->FindComponentByClass<UBreakerCombatComponent>())
            {
                Charge->ConsumeBloodDebt();
                FBreakerDamageRequest Settlement;
                Settlement.BaseDamage = Debt;
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
        const float Remaining = State ? State->GetMarkRemaining() : 0.0f;
        const float UnspentFraction = ActiveMarkDuration > 0.0f ? FMath::Clamp(Remaining / ActiveMarkDuration, 0.0f, 1.0f) : 0.0f;
        if (UnspentFraction > 0.0f && SupportHasNode(Character, BreakerNodeTags::Node_WA_ExecutionersLedger.GetTag()))
        {
            if (UBreakerChargeComponent* Charge = Character->FindComponentByClass<UBreakerChargeComponent>())
            {
                // Refund what THIS build actually pays (a free Hunter's Economy
                // mark refunds nothing — there is nothing to refund).
                Charge->GrantCharge(GetResourceCost() * UnspentFraction);
            }
            ShaveOwnCooldownSeconds(GetCooldownSeconds() * UnspentFraction);
        }

        // WA4 HANDOFF: the mark survives its target's death and jumps to the
        // nearest unmarked enemy (R2: further). The jump pays nothing.
        const int32 HandoffRank = SupportNodeRank(Character, TEXT("Support.Warden.Handoff"));
        if (HandoffRank > 0 && Remaining > 0.0f)
        {
            const float JumpRange = HandoffRank >= 2 ? 2500.0f : 1500.0f;   // O2 PLACEHOLDER ("nearest", R2 "further")
            ABreakerEnemy* Nearest = nullptr;
            float BestDistSq = JumpRange * JumpRange;
            if (UWorld* World = GetWorld())
            {
                for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
                {
                    ABreakerEnemy* Candidate = *It;
                    if (!Candidate || Candidate == Hit.Target) continue;
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
        if (AActor* Target = MarkedTarget.Get())
        {
            if (UBreakerCombatComponent* TargetCombat = Target->FindComponentByClass<UBreakerCombatComponent>())
            {
                TargetCombat->RemoveIncomingDamageModifier(IncomingModifierKey());
            }
            // WA6: the softening dies with the mark, unconditionally — a pop
            // of a never-pushed key is a no-op, and gating it on the node tag
            // would leak the softening across a respec.
            if (ABreakerEnemy* MarkedEnemy = Cast<ABreakerEnemy>(Target))
            {
                MarkedEnemy->PopOutgoingDamageMultiplier(TellModifierKey());
            }
        }
        MarkedTarget.Reset();
        if (UBreakerCombatComponent* Combat = BoundCombat.Get())
        {
            Combat->OnHitDealt.RemoveDynamic(this, &UBreakerAbility_Mark::HandleHitDealt);
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
    // occupant-entered edge, frame one. The accuracy cut (formerly recorded
    // absent) pays through the enemy aim-error seam in HandleOccupantEntered:
    // delayed at base, INSTANT at R2 — the rank captured here decides.
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
            World->GetTimerManager().SetTimer(PressureTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { PressureTick(); }), 1.0f, /*bLoop=*/true);
        }
    }

    // The ability ends now; the zone owns the 6s. The 10s cooldown guarantees
    // no second Suppress races this zone's teardown handlers.
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UBreakerAbility_Suppress::PressureTick()
{
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
            Charge->GrantCharge(PressureRank >= 2 ? PressureChargePerSecondRank2 : PressureChargePerSecond);
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
        // inside when it elapses). WA3 R2: it lands on this edge, frame one —
        // the same instant/delayed split as the node's slow clause. One shared
        // key: the 10s cooldown against the 6s zone means two Suppress fields
        // never coexist, and keyed replace would merely refresh if they did.
        if (FieldOfViewRank >= 2 || AccuracyApplyDelaySeconds <= 0.0f)
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
    World->GetTimerManager().SetTimer(WindowTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { CloseConduit(); }), Duration, false);

    if (bTriage)
    {
        // §3.1 TRIAGE: a continuous healing field, every valid target — solo,
        // the set is the Support, unconditionally. The one-lethal-hit-save per
        // target is RECORDED ABSENT (no lethal-prevention hook exists).
        World->GetTimerManager().SetTimer(TriageTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { HandleTriagePulse(); }), 1.0f, /*bLoop=*/true);
    }
    else if (bDownbeat)
    {
        // §3.1 DOWNBEAT: free casts stay, cadence effects double (the cadence
        // payload is the recorded Cadence gap, so the doubling has nothing to
        // double yet), and every buffed target adds FLAT weapon damage — solo,
        // one buffed target: the Support.
        if (UBreakerCombatComponent* Combat = Character->FindComponentByClass<UBreakerCombatComponent>())
        {
            Combat->PushOutgoingModifier(DownbeatModifierKey(), DownbeatFlatDamagePerBuffedTarget, 1.0f, Duration);
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

void UBreakerAbility_Conduit::HandleTriagePulse()
{
    ABreakerCharacter* Character = GetBreakerCharacter();
    if (!Character) return;
    // Through the one heal-and-credit seam: generation deliberately CONTINUES
    // under CONDUIT (§3.1), and Overflow's conversion applies if owned.
    const float MaxHealth = BreakerSupportAbilityLocal::BreakerSupportTargetMaxHealth(Character);
    const bool bOverflow = SupportHasNode(Character, BreakerNodeTags::Node_MD_Overflow.GetTag());
    BreakerSupportAbilityLocal::BreakerSupportHealAndCredit(Character, Character,
        MaxHealth * TriageHealFractionPerSecond, 1.0f, bOverflow);
}

void UBreakerAbility_Conduit::HandleBlackoutHit(const FBreakerHitContext& Hit)
{
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

void UBreakerAbility_Conduit::CloseConduit()
{
    if (CurrentActorInfo)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
    }
}

void UBreakerAbility_Conduit::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (bConduitActive)
    {
        bConduitActive = false;
        ABreakerCharacter* Character = GetBreakerCharacter();
        if (Character)
        {
            if (UBreakerAbilityStateComponent* State = Character->FindComponentByClass<UBreakerAbilityStateComponent>())
            {
                State->CloseWindow(ConduitWindowKey());
            }
            if (UBreakerCombatComponent* Combat = Character->FindComponentByClass<UBreakerCombatComponent>())
            {
                Combat->RemoveOutgoingModifier(DownbeatModifierKey());
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
        }
    }
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
