#pragma once

#include "CoreMinimal.h"
#include "BreakerBuildConditions.generated.h"

// ---------------------------------------------------------------------------
// Conditional power. Movement was the first pillar; it is no longer the only one.
// ---------------------------------------------------------------------------
// Power-Curve.md §"Choices over accumulation": "This game's pillar is movement,
// so conditional damage that keys off the movement state (airborne, sliding,
// wall-riding, at Redline Momentum) is the obvious place for build identity to
// live, and it is currently absent."
//
// That absence was filled and then became a ceiling. O30's implementation note
// states it exactly: "EBreakerBuildCondition is movement-only today, which is
// why no node can key off combat or status state. Several axes in the taxonomy
// (ailment, crit, stacking) need it widened before they can be authored
// honestly." 53 of the 97 authored tree nodes ship with no stat effect, and
// this enum is named as the blocker in more of their WAITING ON comments than
// any other single cause. Docs/Design/Hook-And-Condition-Vocabulary.md is the
// design that widened it; this is that design in code.
//
// WHAT A CONDITION IS, and the line that keeps this enum from sprawling:
// a condition is a PREDICATE ON LIVE STATE — something that is true or false of
// an actor at an instant, independent of any particular damage event. It is NOT
// a property of a hit. "Was this hit a critical", "was this hit a weak point",
// "did this hit kill" and "was this hit melee" are all outcomes OF an event, and
// they are deliberately absent below: per-event outcomes belong to the HOOK
// vocabulary (the payload a hook hands its listener), and delivery method
// belongs to the STAT TARGET vocabulary as a partition of Damage (MeleeDamage,
// WeaponDamage, AbilityDamage). Encoding them here would burn scarce mask bits
// on facts that are not state, and would give two systems two different answers
// to the same question. See the vocabulary document §2.1.
//
// Both power layers (nodes and affixes) own lines that only pay out while their
// condition holds, and both need the same answer at the same instant, so the
// evaluation lives here once rather than twice. This header consumes Movement/,
// Classes/, Weapons/, Attributes/ and Combat/ — it never edits them; every read
// in the .cpp is an existing public accessor.
//
// The composition rule is unchanged and still LOCKED: a conditional line is an
// ordinary Increased percentage that joins the ONE additive bucket for its stat
// while it is active and is absent while it is not. Nothing here is a second
// multiplier. Widening the set of predicates does not widen the aggregation law.
//
// APPEND ONLY. This enum is serialized BY VALUE into Data Assets, into affix
// table rows (Items/BreakerAffixLibrary.cpp) and into node effects. Inserting or
// reordering an entry silently re-points every authored row that follows it.
// New entries go at the end, before Count, always.
UENUM(BlueprintType)
enum class EBreakerBuildCondition : uint8
{
    // ---- SELF: the original movement set (0-5) ---------------------------
    // Always active. The default, so an ordinary line needs no annotation.
    Always,
    // UBreakerCharacterMovementComponent::IsFalling().
    Airborne,
    // UBreakerCharacterMovementComponent::IsSliding().
    Sliding,
    // RETIRED WITH THE VERB (Part One-R): wall-ride left Movement/ for vault
    // and mantle, so nothing evaluates this any more — the enumerator stays
    // by the append-only rule, IsSelfEvaluable answers false, and nothing may
    // re-author against it. A vault or mantle condition is authored the day
    // those verbs record state, as its own entry at the end.
    WallRiding,
    // UBreakerMomentumComponent::GetMomentumState() == Redline. Swift only by
    // construction — the momentum loop is inert for every other class, so a
    // Redline line on a Tank is dead weight the player can see and avoid.
    Redline,
    // Within RecentDashSeconds of UBreakerCharacterMovementComponent
    // ::GetLastDashTime().
    RecentlyDashed,

    // ---- SELF: posture (6-8), evaluable today ----------------------------
    // The explicit complement of Airborne. There is no negation operator in a
    // requirement — composition is AND only (owner ruling: "conditions do
    // compose yes") — so "while grounded" has to be its own bit or it cannot be
    // authored at all. Cheap, and it is what Swift.Kinetic.NoGround's grounded
    // half and every "planted" defensive line need.
    Grounded,
    // Horizontal speed below StationarySpeedThreshold. The stationary-Swift
    // fantasy (Swift.Marksman.Reserve, Swift.Marksman.Steady) is authored
    // against this. NOT the complement of any other entry: a character can be
    // stationary and airborne at the apex of a jump.
    Stationary,
    // UBreakerWeaponComponent::IsAiming(). Named as a missing predicate by
    // Swift.Marksman.Reserve ("ADS is a weapon state ... not a node stat
    // target") and Swift.Marksman.Steady.
    Aiming,

    // ---- SELF: vitals and resource (9-12), evaluable today ---------------
    // Health fraction at or above HighVitalFraction. The ordinary "while
    // healthy" offensive line the node layer has never been able to author.
    HealthHigh,
    // Health fraction at or below LowVitalFraction. The defensive twin, and the
    // one half of the pair with an obvious failure mode: a line that only pays
    // while nearly dead is a line that pays while dying, which is why O18's TTD
    // target (4-5 seconds bare) is the number that prices it, not damage TTK.
    HealthLow,
    // Class-resource fraction at or below LowVitalFraction, across all five
    // loops (Momentum, Mana, Charge, Grit, Scrap) through their own
    // GetXFraction accessors.
    //
    // There is deliberately no ResourceHigh twin. Nothing authored needs it,
    // and Mana INVERTED (it starts full and drains), so "high resource" means
    // opposite things to Caster and to Swift — a single bit cannot honestly
    // carry both readings. Budgeted out; see the vocabulary document §2.4.
    ResourceLow,
    // Resource fraction at or below zero — for a loop whose RESTING state is
    // full/positive, which today means Mana alone. Distinct from ResourceLow
    // because Caster's Overcast drives Mana NEGATIVE and two nodes
    // (Caster.Spellblade.Debt, Caster.Spellblade.Bloodprice) are authored
    // against the debt state specifically, not against "nearly out".
    //
    // The resting-state gate is the condition's MEANING, not a special case
    // (build-math finding #3): "depleted" is DRAINED PAST EMPTY, and the
    // bank-style loops (Grit, Scrap, Charge) plus Momentum all idle at zero —
    // without the gate this bit was simply ON for an idle Tank, Gunsmith and
    // Support and near-always-on for a standing Swift, which made
    // Anomaly.EntropyDebt a free unconditional line for three classes while
    // the Caster it was written for had to earn it. Each loop answers
    // IsRestingStateFull() for itself; only Caster can proc this today.
    ResourceDepleted,

    // ---- SELF: recent-event windows (13-16) ------------------------------
    // NONE OF THESE FOUR CAN BE EVALUATED TODAY. They are state — "did X happen
    // to me inside the last N seconds" is a fact about an actor at an instant,
    // not a property of a hit — but nothing records the timestamps. Each needs
    // a small recorder bound to a delegate that already exists
    // (UBreakerCombatComponent::OnKillDealt, ::OnDamageTaken, ::OnHitDealt) and
    // a last-event time to read, exactly as RecentlyDashed reads
    // GetLastDashTime(). They are authored here rather than withheld because
    // the vocabulary document has to be able to say what the shape is; they are
    // LOUD rather than silent, which is the whole point — see
    // FBreakerBuildConditionState::IsSelfEvaluable and the warn-once below.
    RecentlyKilled,
    RecentlyTookDamage,
    RecentlyCastAbility,
    RecentlyAppliedStatus,

    // ---- TARGET: the state of what is being hit (17-25) ------------------
    // Owner ruling, verbatim: "Target-side conditions: YES." A node may key off
    // the TARGET's state ("more damage to bleeding enemies") and not only the
    // character's own.
    //
    // NONE of these are populated by EvaluateForActor and they never will be:
    // EvaluateForActor takes one actor and target state is a two-actor fact.
    // SupplyTargetState has exactly one honest production call site, and since
    // Stage 6 it is LIVE: UBreakerCombatComponent::ReceiveDamage supplies the
    // target half (target = GetOwner(), attacker = Request.Instigator) and
    // resolves the progression component's target-conditional rider table
    // there. A requirement naming one of these outside that path is still
    // unsatisfiable, and SatisfiesAll warns once rather than quietly
    // returning false. See the vocabulary document §3.
    //
    // O38 holds: Elements are post-slice, so there is no TargetBurning /
    // TargetChilled / TargetShocked here and no elemental predicate of any kind.
    // Bleed and Poison are physical/chemical ailments that exist in the status
    // component today (Status.Bleed, Status.Poison) and are in scope.

    // Carries any status at all. The generic gateway predicate, so an
    // ailment-axis line does not have to enumerate every status to mean "an
    // afflicted enemy".
    TargetAiling,
    // Carries Status.Bleed.
    TargetBleeding,
    // Carries Status.Poison.
    //
    // Bleed and Poison are the only two status tags in
    // Config/DefaultGameplayTags.ini that anything actually applies AND that are
    // not elemental. Burn, Frost and Shock are elemental and Void is the Void
    // element's own status, so all four are out under O38. There is deliberately
    // no TargetImpaired/TargetSlowed either: no slow or stun tag exists at all,
    // and the nearest thing (Status.Frost, "frost buildup and slow") is
    // elemental. Adding a control-state predicate would mean either inventing
    // a tag or keying off an element, and both are somebody else's ruling.
    TargetPoisoned,
    // Carries at least MultiStatusThreshold distinct status types. This is O30's
    // "stacking" axis, and it is the predicate Caster.Multispell.Cascade's
    // reserved More and Caster.Multispell.Chain are both written against. A
    // threshold rather than a count because a condition is a boolean; the
    // threshold is a named constant so it is visible and reviewable.
    TargetMultiStatus,
    // Target health fraction at or below LowVitalFraction. The execute
    // predicate.
    TargetLowHealth,
    // Target is Veteran rank or above (O9's Rank field). "Pays only against
    // things worth killing" is the honest way to author a strong conditional
    // without inflating trash clear, which O27 wants trivialized anyway.
    TargetElite,
    // Target within CloseRangeCm of the character. Named by
    // Caster.Spellblade.Close ("Weapon hits at close range generate double
    // Mana") and by the Spellblade branch's whole play-distance identity.
    TargetAtCloseRange,
    // The PREVIOUS hit on this target removed a health band
    // (Attributes/BreakerHealthBands.h — bands are state on every rank, Trash
    // included). Deliberately about the previous hit, not the current one:
    // riders evaluate on pre-damage state thirteen lines before the damage
    // lands, so "this hit will cross a band" is a fixpoint (the rider changes
    // the damage that decides whether the rider applies) and cannot be built
    // honestly. One bit on the target's combat component, written after each
    // non-dodged hit lands and read by the next — break a band, the next hit
    // lands heavier. Per-target, previous-hit lifetime: any landed hit,
    // a snapshotted DoT tick included, overwrites it; a dodge neither sets
    // nor clears it; the enemy pool clears it on revive.
    TargetBandBroken,

    // ---- SELF, appended after the closed target block (26) ---------------
    // The enum is serialized by value, so a new SELF condition cannot be
    // inserted before the target block — it appends HERE, and classification
    // stopped being "everything from TargetAiling onward" the day this
    // arrived: the target block is CLOSED, [TargetAiling, TargetBandBroken],
    // and IsTargetCondition tests the range. A future target condition
    // appends at the end and extends LastTargetConditionIndex deliberately.
    //
    // Within RecentEventSeconds of the movement component's
    // GetLastLedgeTraversalTime() — the completion timestamp KIT's recorder
    // writes for BOTH ledge verbs (vault and mantle share the exit path). A
    // COMPLETED traversal only: an aborted mantle records nothing, so this
    // can never pay for a verb that granted nothing. One-T named the window
    // "RecentlyMantled"; it widened to the traversal pair because the
    // recorder is shared and both verbs are deliberate triggers — a
    // mantle-only window waits until a node actually wants the distinction,
    // and KIT splits the recorder then rather than a duration being sniffed.
    RecentlyLedgeTraversed,

    Count UMETA(Hidden)
};

// The set of conditions true for one actor at one instant. A bitmask rather
// than a bool array so a layer can cheaply notice "nothing changed" and skip a
// resubmission — the components re-derive their whole contribution on any
// change, which is only affordable because most frames change nothing.
struct RIORSEDGE_API FBreakerBuildConditionState
{
    static constexpr int32 ConditionCount = static_cast<int32>(EBreakerBuildCondition::Count);

    // The CLOSED target block: [TargetAiling, TargetBandBroken]. Everything
    // inside it is a two-actor fact that EvaluateForActor structurally cannot
    // answer. The block was "everything from TargetAiling onward" until the
    // first post-block SELF condition (RecentlyLedgeTraversed) had to append
    // at the serialized end — the enum is append-only by value, so a self
    // condition cannot be inserted before the block. A future TARGET
    // condition appends at the end and extends LastTargetConditionIndex in
    // the same commit; the partition test holds the block closed until then.
    static constexpr int32 FirstTargetConditionIndex = static_cast<int32>(EBreakerBuildCondition::TargetAiling);
    static constexpr int32 LastTargetConditionIndex = static_cast<int32>(EBreakerBuildCondition::TargetBandBroken);

    // O2 PLACEHOLDER: how long "recently dashed" lasts. Long enough to cover a
    // burst of fire after the dash, short enough that it is not just "on".
    static constexpr float RecentDashSeconds = 3.0f;
    // O2 PLACEHOLDER: the shared window for every Recently* condition. One
    // constant rather than four so the family reads as a family; if playtest
    // wants them to differ they split then, with evidence.
    static constexpr float RecentEventSeconds = 3.0f;
    // O2 PLACEHOLDER: cm/s below which a character counts as Stationary. Not
    // zero — a character settling on a slope or nudged by a corpse is standing
    // still as far as the player is concerned.
    static constexpr float StationarySpeedThreshold = 40.0f;
    // O2 PLACEHOLDER: the fractions that define "high" and "low" for health and
    // resource alike. Shared so the phrases mean one thing across every line
    // that uses them.
    static constexpr float HighVitalFraction = 0.85f;
    static constexpr float LowVitalFraction = 0.35f;
    // O2 PLACEHOLDER: distinct status types a target must carry for
    // TargetMultiStatus. Class-Kits' Cascade says three; two is the generic
    // "stacked" reading. Seeded at the design's number.
    static constexpr int32 MultiStatusThreshold = 3;
    // O2 PLACEHOLDER: what "close range" means, in cm.
    static constexpr float CloseRangeCm = 500.0f;

    FBreakerBuildConditionState() = default;

    // Always is true by definition, so an empty state still pays unconditional
    // lines. That is what lets one code path serve both.
    bool IsActive(EBreakerBuildCondition Condition) const
    {
        if (Condition == EBreakerBuildCondition::Always) return true;
        return (Mask & Bit(Condition)) != 0;
    }

    void Set(EBreakerBuildCondition Condition, bool bActive)
    {
        if (Condition == EBreakerBuildCondition::Always) return;
        if (bActive) Mask |= Bit(Condition);
        else Mask &= ~Bit(Condition);
    }

    // ---- Composition -----------------------------------------------------
    // Owner ruling, verbatim: "conditions do compose yes". An effect may name
    // more than one condition and pays only while ALL of them hold. AND only:
    // there is no OR and no NOT, on purpose. OR is expressible as two effects
    // (and reads more honestly on a tooltip, which has to print the line twice
    // anyway); NOT is a trap, because "not airborne" and "grounded" differ on
    // ladders, in water and mid-teleport, and a player cannot tell which they
    // bought. Where a complement is genuinely wanted it gets its own entry —
    // that is what Grounded is.
    //
    // Returns false, and warns ONCE per offending condition, when a requirement
    // names something this state could not evaluate. A conditional line that can
    // never pay is the exact bug class this project keeps finding (the third
    // jump, the phantom keystones, the silently-dropped More); it must be
    // audible from the log on the first frame it is asked, not discovered by a
    // player wondering why a node did nothing.
    bool SatisfiesAll(EBreakerBuildCondition Primary, const TArray<EBreakerBuildCondition>& AlsoRequires) const;

    // The same test for a single condition, with the same warn-once behaviour.
    // SatisfiesAll delegates to it; it is public because the affix layer
    // currently authors single conditions only and can adopt the loudness
    // without adopting composition.
    bool SatisfiesOne(EBreakerBuildCondition Condition) const;

    bool operator==(const FBreakerBuildConditionState& Other) const { return Mask == Other.Mask; }
    bool operator!=(const FBreakerBuildConditionState& Other) const { return Mask != Other.Mask; }

    // Reads the live movement/momentum/posture/vitals state off an actor. A null
    // actor, or one with none of those components, yields the empty state —
    // which is exactly what a test rig with no world wants, and why every
    // conditional line is simply absent there instead of erroring.
    //
    // It populates SELF conditions only, and only the ones something can
    // actually answer today. It does NOT populate the Recently* family (nothing
    // records those timestamps yet) and it CANNOT populate the Target* family
    // (target state is a two-actor fact). Both gaps are announced once through
    // the warn-once path rather than left to be discovered.
    static FBreakerBuildConditionState EvaluateForActor(const AActor* Actor);

    // The second half of the pair, LIVE since Stage 6. Fills the Target*
    // conditions from the actor being hit, marks target state as supplied so
    // SatisfiesAll stops warning, and takes the attacker only to measure
    // TargetAtCloseRange. Its one production caller is the site the design
    // named: UBreakerCombatComponent::ReceiveDamage (via
    // ApplyTargetConditionRiders), where the target is GetOwner() and the
    // attacker is Request.Instigator — see the vocabulary document §3.2.
    void SupplyTargetState(const AActor* Target, const AActor* Attacker);

    // True once SupplyTargetState has run. Distinct from "some target condition
    // is set": a target with no statuses supplies a legitimately empty target
    // state, and that must not read the same as never having asked.
    bool HasTargetState() const { return bTargetStateSupplied; }

    // Every condition on at once, target state included and marked supplied.
    // Used by tests and by the tooltip path that wants to show what a line would
    // be worth when it is live.
    //
    // A caution the tooltip path must respect: with target conditions in the set
    // this is now "the best case against the most convenient enemy", not "the
    // best case". PotentialConditionalDamagePercent computed from All() is an
    // upper bound and should be labelled as one.
    static FBreakerBuildConditionState All();

    // ---- Classification and naming ---------------------------------------
    // Whether a condition is a fact about the character or about what it is
    // hitting. Drives which half of the pipeline has to supply it.
    static bool IsTargetCondition(EBreakerBuildCondition Condition)
    {
        const int32 Index = static_cast<int32>(Condition);
        return Index >= FirstTargetConditionIndex && Index <= LastTargetConditionIndex;
    }

    // Whether EvaluateForActor populates this condition today. False for the
    // Recently* family (no recorder) and for every target condition (wrong
    // number of actors). This is the honest answer to "will a node authored
    // against this do anything", and the test suite pins it so that the day a
    // recorder lands, the entry moves here and the pin fails until the list is
    // updated deliberately.
    static bool IsSelfEvaluable(EBreakerBuildCondition Condition);

    // Distinct, non-empty, stable name per entry. Not the UEnum display name:
    // this is used in warnings and in the audit tests, both of which must work
    // in a cooked build with reflection data the editor would otherwise supply.
    static const TCHAR* DescribeCondition(EBreakerBuildCondition Condition);

    // Raw mask access, for tests and for the equality-shortcut path.
    uint32 GetMask() const { return Mask; }

private:
    // Widened uint8 -> uint32 (pre-O30 hardening pass). A uint8 Mask/Bit() pair
    // silently breaks the 9th entry: 1u << 8 overflows a uint8 to 0 with no
    // warning, so that condition would compile, purchase, and simply never
    // activate. O30's taxonomy (ailment, crit, stacking, ...) is exactly the
    // kind of pass that adds several entries at once — this widening pass added
    // nineteen — so the width was widened before it silently broke rather than
    // the day it did.
    //
    // THIRTY-TWO IS THE HARD CEILING and it is a real budget, not a formality.
    // The vocabulary document §2.6 records what was left out to stay inside it.
    static_assert(ConditionCount <= 32,
        "EBreakerBuildCondition has grown past 32 entries; FBreakerBuildConditionState::Mask needs a wider type.");
    static uint32 Bit(EBreakerBuildCondition Condition) { return 1u << static_cast<uint32>(Condition); }

    uint32 Mask = 0;
    // Not part of operator== on purpose. Equality exists so a layer can skip a
    // resubmission when nothing changed, and two states with the same live
    // conditions produce the same contribution whether or not the target half
    // was asked for.
    bool bTargetStateSupplied = false;
};
