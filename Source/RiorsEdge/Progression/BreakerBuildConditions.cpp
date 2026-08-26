#include "Progression/BreakerBuildConditions.h"

#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Classes/BreakerChargeComponent.h"
#include "Classes/BreakerGritComponent.h"
#include "Classes/BreakerManaComponent.h"
#include "Classes/BreakerMomentumComponent.h"
#include "Classes/BreakerScrapComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerStatusComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Weapons/BreakerWeaponComponent.h"

namespace
{
    // Status tags are authored in Config/DefaultGameplayTags.ini rather than
    // declared natively (the reason is recorded in Abilities/BreakerAbilityTags
    // .h), so they are requested by name. ErrorIfNotFound=false: a cooked build
    // without the ini section must yield "no such status" rather than an ensure,
    // which is the same tolerance EvaluateForActor shows a component-less actor.
    FGameplayTag BleedTag() { return FGameplayTag::RequestGameplayTag(TEXT("Status.Bleed"), false); }
    FGameplayTag PoisonTag() { return FGameplayTag::RequestGameplayTag(TEXT("Status.Poison"), false); }

    // The class-resource fraction, whichever loop this owner runs. All five
    // components expose the same GetXFraction() shape, and exactly one of them
    // is ever active for a given class, so the first one found is the answer.
    // Returns false when the actor runs no resource loop at all — an enemy, a
    // dummy, a test rig — and the resource conditions are then simply absent
    // rather than reading as "empty", which would make every ResourceLow line
    // in the game pay out on a target dummy.
    //
    // bOutRestsFull carries the loop's own IsRestingStateFull() answer —
    // whether the loop idles at a full/positive bank (Mana) or at zero (the
    // banks and Momentum). ResourceDepleted's eligibility question, and asked
    // of the component rather than answered here, so a future loop
    // self-describes instead of growing a case in this file (build-math
    // finding #3).
    bool TryGetResourceFraction(const AActor* Actor, float& OutFraction, bool& bOutRestsFull)
    {
        if (const UBreakerMomentumComponent* Momentum = Actor->FindComponentByClass<UBreakerMomentumComponent>())
        {
            if (Momentum->IsActiveForOwner()) { OutFraction = Momentum->GetMomentumFraction(); bOutRestsFull = Momentum->IsRestingStateFull(); return true; }
        }
        if (const UBreakerManaComponent* Mana = Actor->FindComponentByClass<UBreakerManaComponent>())
        {
            if (Mana->IsActiveForOwner()) { OutFraction = Mana->GetManaFraction(); bOutRestsFull = Mana->IsRestingStateFull(); return true; }
        }
        if (const UBreakerChargeComponent* Charge = Actor->FindComponentByClass<UBreakerChargeComponent>())
        {
            if (Charge->IsActiveForOwner()) { OutFraction = Charge->GetChargeFraction(); bOutRestsFull = Charge->IsRestingStateFull(); return true; }
        }
        if (const UBreakerGritComponent* Grit = Actor->FindComponentByClass<UBreakerGritComponent>())
        {
            if (Grit->IsActiveForOwner()) { OutFraction = Grit->GetGritFraction(); bOutRestsFull = Grit->IsRestingStateFull(); return true; }
        }
        if (const UBreakerScrapComponent* Scrap = Actor->FindComponentByClass<UBreakerScrapComponent>())
        {
            if (Scrap->IsActiveForOwner()) { OutFraction = Scrap->GetScrapFraction(); bOutRestsFull = Scrap->IsRestingStateFull(); return true; }
        }
        return false;
    }

    bool TryGetHealthFraction(const AActor* Actor, float& OutFraction)
    {
        const UAbilitySystemComponent* ASC = Actor->FindComponentByClass<UAbilitySystemComponent>();
        if (!ASC) return false;
        const UBreakerAttributeSet* Attributes = ASC->GetSet<UBreakerAttributeSet>();
        if (!Attributes) return false;
        const float Max = Attributes->GetMaxHealth();
        if (Max <= 0.0f) return false;
        OutFraction = Attributes->GetHealth() / Max;
        return true;
    }
}

// ---------------------------------------------------------------------------
// Naming and classification
// ---------------------------------------------------------------------------
const TCHAR* FBreakerBuildConditionState::DescribeCondition(EBreakerBuildCondition Condition)
{
    switch (Condition)
    {
    case EBreakerBuildCondition::Always:                return TEXT("Always");
    case EBreakerBuildCondition::Airborne:              return TEXT("Airborne");
    case EBreakerBuildCondition::Sliding:               return TEXT("Sliding");
    case EBreakerBuildCondition::WallRiding:            return TEXT("WallRiding");
    case EBreakerBuildCondition::Redline:               return TEXT("Redline");
    case EBreakerBuildCondition::RecentlyDashed:        return TEXT("RecentlyDashed");
    case EBreakerBuildCondition::Grounded:              return TEXT("Grounded");
    case EBreakerBuildCondition::Stationary:            return TEXT("Stationary");
    case EBreakerBuildCondition::Aiming:                return TEXT("Aiming");
    case EBreakerBuildCondition::HealthHigh:            return TEXT("HealthHigh");
    case EBreakerBuildCondition::HealthLow:             return TEXT("HealthLow");
    case EBreakerBuildCondition::ResourceLow:           return TEXT("ResourceLow");
    case EBreakerBuildCondition::ResourceDepleted:      return TEXT("ResourceDepleted");
    case EBreakerBuildCondition::RecentlyKilled:        return TEXT("RecentlyKilled");
    case EBreakerBuildCondition::RecentlyTookDamage:    return TEXT("RecentlyTookDamage");
    case EBreakerBuildCondition::RecentlyCastAbility:   return TEXT("RecentlyCastAbility");
    case EBreakerBuildCondition::RecentlyAppliedStatus: return TEXT("RecentlyAppliedStatus");
    case EBreakerBuildCondition::TargetAiling:          return TEXT("TargetAiling");
    case EBreakerBuildCondition::TargetBleeding:        return TEXT("TargetBleeding");
    case EBreakerBuildCondition::TargetPoisoned:        return TEXT("TargetPoisoned");
    case EBreakerBuildCondition::TargetMultiStatus:     return TEXT("TargetMultiStatus");
    case EBreakerBuildCondition::TargetLowHealth:       return TEXT("TargetLowHealth");
    case EBreakerBuildCondition::TargetElite:           return TEXT("TargetElite");
    case EBreakerBuildCondition::TargetAtCloseRange:    return TEXT("TargetAtCloseRange");
    case EBreakerBuildCondition::TargetBandBroken:      return TEXT("TargetBandBroken");
    // No default. A new enum entry must fail to compile here rather than fall
    // through to a placeholder — "STAT" in UI/BreakerMenu.cpp's two label
    // switches is exactly that mistake, and it is why every damage node on the
    // board once read "+4% STAT".
    case EBreakerBuildCondition::Count:                 break;
    }
    return TEXT("Unknown");
}

bool FBreakerBuildConditionState::IsSelfEvaluable(EBreakerBuildCondition Condition)
{
    // Target conditions are never self-evaluable by definition: EvaluateForActor
    // takes one actor and they are a two-actor fact.
    if (IsTargetCondition(Condition)) return false;

    switch (Condition)
    {
    // The Recently* family. Each needs a recorder that does not exist: a small
    // component (or four fields on an existing one) that binds
    // UBreakerCombatComponent::OnKillDealt / ::OnDamageTaken / ::OnHitDealt and
    // the ability activation path, stores the world time of the last such
    // event, and exposes it the way UBreakerCharacterMovementComponent
    // ::GetLastDashTime() already does for RecentlyDashed. RecentlyDashed is
    // the working template for all four.
    case EBreakerBuildCondition::RecentlyKilled:
    case EBreakerBuildCondition::RecentlyTookDamage:
    case EBreakerBuildCondition::RecentlyCastAbility:
    case EBreakerBuildCondition::RecentlyAppliedStatus:
        return false;
    default:
        return true;
    }
}

// ---------------------------------------------------------------------------
// The loud path
// ---------------------------------------------------------------------------
// A conditional line that can never pay is this project's recurring bug: the
// third jump gated on a level nothing awarded, the phantom keystones granting
// ability ids that resolved to nothing, the MorePercent on a stat target that
// composes no More product. Every one of them compiled, shipped, and did
// nothing quietly. The rule adopted after those, and applied here, is that a
// dead lane announces itself the first time it is asked.
//
// Warn-once per condition rather than per call: a conditional effect is
// evaluated on every stat recomputation, which on a moving character is many
// times a second. One line per offending condition per session is a signal; the
// same line at 60Hz is noise that trains people to filter the channel.
bool FBreakerBuildConditionState::SatisfiesOne(EBreakerBuildCondition Condition) const
{
    if (Condition == EBreakerBuildCondition::Always) return true;

    // The bit wins. If something set this condition — a live evaluation, a
    // future recorder, All()'s hypothetical, a test fixture — then it holds and
    // there is nothing to complain about. Order matters here: testing
    // evaluability first would make All() report the Recently* family as false,
    // which would understate every tooltip's potential value and would be a
    // worse bug than the one this function exists to catch.
    if (IsActive(Condition)) return true;

    // It is false. The question is whether it is false because the state says
    // so, or false because nothing was ever in a position to say otherwise.
    // Only the second is a defect.
    const bool bNeedsTargetState = IsTargetCondition(Condition);
    const bool bUnevaluable = bNeedsTargetState ? !bTargetStateSupplied : !IsSelfEvaluable(Condition);
    if (bUnevaluable)
    {
        static TSet<EBreakerBuildCondition> WarnedOnce;
        if (!WarnedOnce.Contains(Condition))
        {
            WarnedOnce.Add(Condition);
            UE_LOG(LogTemp, Warning,
                TEXT("[BreakerConditions] an effect requires condition '%s', which cannot be evaluated here and therefore never pays out. %s"),
                DescribeCondition(Condition),
                bNeedsTargetState
                    ? TEXT("It is a TARGET condition and this state was built without target information. The production path DOES supply it -- UBreakerCombatComponent::ReceiveDamage calls SupplyTargetState and resolves target riders there (Stage 6) -- so this warning means a target requirement reached a state built OUTSIDE the damage pipeline: either a call site that should route through the rider table, or an effect on a layer with no target-side consumer.")
                    : TEXT("Nothing records the state it reads yet -- see FBreakerBuildConditionState::IsSelfEvaluable for what is missing."));
        }
    }
    return false;
}

bool FBreakerBuildConditionState::SatisfiesAll(EBreakerBuildCondition Primary, const TArray<EBreakerBuildCondition>& AlsoRequires) const
{
    // AND across the whole requirement. Evaluated in full rather than
    // short-circuited on the first failure ON PURPOSE: short-circuiting would
    // hide the warning for every condition after the first unevaluable one, so a
    // node requiring two dead conditions would report one and look half-fixed.
    // The cost is a handful of bit tests on a list that is almost always empty.
    bool bSatisfied = SatisfiesOne(Primary);
    for (const EBreakerBuildCondition Condition : AlsoRequires)
    {
        bSatisfied &= SatisfiesOne(Condition);
    }
    return bSatisfied;
}

// ---------------------------------------------------------------------------
// SELF evaluation
// ---------------------------------------------------------------------------
FBreakerBuildConditionState FBreakerBuildConditionState::EvaluateForActor(const AActor* Actor)
{
    FBreakerBuildConditionState State;
    if (!Actor) return State;

    if (const UBreakerCharacterMovementComponent* Movement = Actor->FindComponentByClass<UBreakerCharacterMovementComponent>())
    {
        const bool bFalling = Movement->IsFalling();
        State.Set(EBreakerBuildCondition::Airborne, bFalling);
        State.Set(EBreakerBuildCondition::Sliding, Movement->IsSliding());
        // WallRiding retired with the verb (Part One-R): the enumerator stays
        // by the append-only rule and joins the never-evaluable set — see
        // IsSelfEvaluable. Nothing may re-author against it. (Line carried in
        // KIT's removal commit by agreement with LEDGER; their same-window
        // half owns IsSelfEvaluable and the census ceiling.)
        // Grounded is not merely !Airborne as a matter of taste: it is written
        // as the negation of the SAME read, in the same frame, so the two can
        // never both be false (or both true) because two accessors disagreed.
        State.Set(EBreakerBuildCondition::Grounded, !bFalling);
        // Stationary is independent of Grounded — a character is stationary at
        // the apex of a jump — so it is not folded into the branch above.
        State.Set(EBreakerBuildCondition::Stationary, Movement->GetHorizontalSpeed() < StationarySpeedThreshold);

        // GetLastDashTime is world time and starts at a large negative
        // sentinel, so a character that has never dashed is never "recently
        // dashed" without a special case.
        if (const UWorld* World = Actor->GetWorld())
        {
            const float SinceDash = World->GetTimeSeconds() - Movement->GetLastDashTime();
            State.Set(EBreakerBuildCondition::RecentlyDashed, SinceDash >= 0.0f && SinceDash <= RecentDashSeconds);
        }
    }

    if (const UBreakerMomentumComponent* Momentum = Actor->FindComponentByClass<UBreakerMomentumComponent>())
    {
        // IsActiveForOwner is the loop's own "am I Swift and switched on" test;
        // without it a cached Redline state could outlive a class change.
        State.Set(EBreakerBuildCondition::Redline,
            Momentum->IsActiveForOwner() && Momentum->GetMomentumState() == EBreakerMomentumState::Redline);
    }

    if (const UBreakerWeaponComponent* Weapon = Actor->FindComponentByClass<UBreakerWeaponComponent>())
    {
        State.Set(EBreakerBuildCondition::Aiming, Weapon->IsAiming());
    }

    float HealthFraction = 0.0f;
    if (TryGetHealthFraction(Actor, HealthFraction))
    {
        State.Set(EBreakerBuildCondition::HealthHigh, HealthFraction >= HighVitalFraction);
        State.Set(EBreakerBuildCondition::HealthLow, HealthFraction <= LowVitalFraction);
    }

    float ResourceFraction = 0.0f;
    bool bResourceRestsFull = false;
    if (TryGetResourceFraction(Actor, ResourceFraction, bResourceRestsFull))
    {
        State.Set(EBreakerBuildCondition::ResourceLow, ResourceFraction <= LowVitalFraction);
        // Caster's Overcast drives Mana below zero, which is the state
        // Spellblade's Debt and Bloodprice are written against. <= rather than
        // < so an exactly-empty bar counts as depleted.
        //
        // Gated on the loop resting full (build-math finding #3): "depleted"
        // means DRAINED past empty, and a loop that IDLES at zero — Grit,
        // Scrap and Charge bank from empty, Momentum decays to empty at rest —
        // is not drained by standing still. Without the gate,
        // Anomaly.EntropyDebt's "while resource is empty" line was always-on
        // for an idle Tank/Gunsmith/Support and near-always-on for a standing
        // Swift, while the Caster it was written against had to EARN it
        // through Overcast. Today only Mana rests full, so only a Caster can
        // proc this condition; the loop's own IsRestingStateFull() is the
        // authority, so a future full-resting loop joins with no edit here.
        State.Set(EBreakerBuildCondition::ResourceDepleted, bResourceRestsFull && ResourceFraction <= 0.0f);
    }

    // The Recently* family is deliberately not set here. It is not an oversight
    // and it is not silent: SatisfiesOne warns the first time any effect asks
    // for one. See IsSelfEvaluable.
    return State;
}

// ---------------------------------------------------------------------------
// TARGET evaluation — LIVE since Stage 6. The one production caller is
// UBreakerCombatComponent::ApplyTargetConditionRiders (inside ReceiveDamage),
// exactly the site the vocabulary document named: the target is GetOwner()
// and the attacker is Request.Instigator.
// ---------------------------------------------------------------------------
void FBreakerBuildConditionState::SupplyTargetState(const AActor* Target, const AActor* Attacker)
{
    // Marked supplied even for a null or component-less target. "We asked and
    // the answer was no" and "we never asked" must not read the same: the first
    // is a legitimate miss on an enemy carrying no statuses, the second is a
    // wiring bug, and only the second should warn.
    bTargetStateSupplied = true;
    if (!Target) return;

    if (const UBreakerStatusComponent* Status = Target->FindComponentByClass<UBreakerStatusComponent>())
    {
        const int32 DistinctStatuses = Status->GetDistinctStatusTypeCount();
        Set(EBreakerBuildCondition::TargetAiling, DistinctStatuses > 0);
        Set(EBreakerBuildCondition::TargetMultiStatus, DistinctStatuses >= MultiStatusThreshold);

        // RequestGameplayTag returns an invalid tag when the ini section is
        // missing; HasStatus on an invalid tag would match nothing anyway, but
        // testing validity first keeps the intent readable.
        const FGameplayTag Bleed = BleedTag();
        if (Bleed.IsValid()) Set(EBreakerBuildCondition::TargetBleeding, Status->HasStatus(Bleed));
        const FGameplayTag Poison = PoisonTag();
        if (Poison.IsValid()) Set(EBreakerBuildCondition::TargetPoisoned, Status->HasStatus(Poison));
    }

    float TargetHealthFraction = 0.0f;
    if (TryGetHealthFraction(Target, TargetHealthFraction))
    {
        Set(EBreakerBuildCondition::TargetLowHealth, TargetHealthFraction <= LowVitalFraction);
    }

    // The band-break bit lives on the target's combat component — written
    // there after each landed hit (ReceiveDamage compares BreakerHealthBands::
    // IndexOf on pre- and post-damage health), read here by the NEXT hit. The
    // one-frame-earlier seam is the whole design: the previous hit is settled
    // state, so there is no fixpoint between the rider and the damage it rides.
    if (const UBreakerCombatComponent* TargetCombat = Target->FindComponentByClass<UBreakerCombatComponent>())
    {
        Set(EBreakerBuildCondition::TargetBandBroken, TargetCombat->WasBandBrokenByPreviousHit());
    }

    if (const ABreakerEnemy* Enemy = Cast<ABreakerEnemy>(Target))
    {
        // O9's Rank field. Anything above Trash counts: the point of the
        // predicate is "worth killing", and O27 wants trash trivialized rather
        // than fought, so the line should not pay for deleting it.
        Set(EBreakerBuildCondition::TargetElite, Enemy->GetMonsterRank() != EBreakerMonsterRank::Trash);
    }

    if (Attacker)
    {
        const float DistanceSq = FVector::DistSquared(Attacker->GetActorLocation(), Target->GetActorLocation());
        Set(EBreakerBuildCondition::TargetAtCloseRange, DistanceSq <= (CloseRangeCm * CloseRangeCm));
    }
}

FBreakerBuildConditionState FBreakerBuildConditionState::All()
{
    FBreakerBuildConditionState State;
    for (int32 Index = 0; Index < ConditionCount; ++Index)
    {
        State.Set(static_cast<EBreakerBuildCondition>(Index), true);
    }
    // Every condition is on, which means the target half has been answered in
    // the most generous way possible. Marking it supplied is what stops All()
    // from tripping the warn-once path on the tooltip and test paths that use
    // it — those callers are not misconfigured, they are asking a hypothetical.
    State.bTargetStateSupplied = true;
    return State;
}
