#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Progression/BreakerBuildConditions.h"
#include "BreakerProgressionTypes.generated.h"

// APPEND ONLY. NEVER INSERT, NEVER REORDER, NEVER REUSE A RETIRED VALUE.
// Serialized by value into the per-character save as PermanentClass, and class
// selection is permanent — so inserting an entry does not produce a load
// error, it produces a character who is silently now a different class, on a
// choice the player was told they could not change. Renaming an enumerator is
// safe; moving one is not.
UENUM(BlueprintType)
enum class EBreakerClassId : uint8
{
    None,
    Caster,
    Swift,
    Gunsmith,
    Tank,
    Support
};

UENUM(BlueprintType)
enum class EBreakerPointCurrency : uint8
{
    // RETIRED (O111). Class Points are deleted; the fifteen branch trees became
    // doctrines and fund from DoctrinePoints below.
    //
    // THE VALUE STAYS AND IS NEVER REUSED. This enum is serialized by value into
    // saves AND into Data Assets, so removing the enumerator would not delete
    // the data -- it would leave every stored 0 meaning whatever moved into its
    // place. Nothing may be inserted above it for the same reason, which is why
    // DoctrinePoints is APPENDED rather than slotted in where it belongs
    // conceptually. The v5 -> v6 migration clears the state that used it.
    ClassPoints_Retired,
    CorePoints,
    // O111: the second and last pool. Eight points of its own, paid two at a
    // time at four benchmarks -- one per act plus the finale -- and settled
    // against LevelDoctrinePointsGranted like every other entitlement.
    DoctrinePoints
};

UENUM(BlueprintType)
enum class EBreakerAbilitySlot : uint8
{
    ClassAbilityOne,
    ClassAbilityTwo,
    Ultimate
};

// Stats a skill node can move. Mirrors the equipment layer's stat-target
// idea, but stays a separate enum on purpose: nodes and affixes are
// different layers and must not share a value table (CONTEXT.md).
UENUM(BlueprintType)
enum class EBreakerNodeStatTarget : uint8
{
    CriticalChance,
    CriticalDamage,
    MoveSpeed,
    SlideSpeed,
    AirControl,
    DodgeChance,
    BlockChance,
    Health,
    DamageOverTime,
    // Increased damage dealt. Appended rather than inserted: node effects are
    // authored content and saves store node ids, but Data Assets serialize this
    // enum by value, so existing rows must keep their numbers.
    //
    // Until this entry existed a skill node was STRUCTURALLY incapable of
    // raising weapon damage — the reason spending points never felt like
    // anything. It shares the DamageMultiplier attribute's additive Increased
    // bucket with gear's Weapon Damage affix.
    Damage,

    // -----------------------------------------------------------------------
    // O30 widening pass. Everything below here was added at once, and the
    // reason is measurable: 53 of the 97 authored tree nodes shipped with no
    // stat effect, and this enum is named as the blocker in more of their
    // WAITING ON comments than any other cause. Each entry below is traceable
    // to at least one node that could not be authored without it; the mapping
    // is in Docs/Design/Hook-And-Condition-Vocabulary.md §4.
    //
    // APPEND ONLY, and this is the rule the original comment above already
    // states: these are serialized by value into Data Assets. Insert nothing.
    //
    // ADDING AN ENTRY HERE DOES NOT MAKE IT PAY. A stat target needs an
    // aggregation lane in UBreakerProgressionComponent::AggregateStats before a
    // node authored against it does anything at all. HasAggregationLane below
    // is the honest register of which ones have one, and it is what makes an
    // unconsumed target loud instead of silent.
    // -----------------------------------------------------------------------

    // ---- Ability scaling (O30's ABILITIES axis; owner ruling 3) -----------
    // "trees modify abilities i want that to be an avenue players can scale not
    // just weapons or character (think caster for instance should be heavily
    // spell oriented over weapons)". These five are that ruling.
    //
    // AbilityDamage and WeaponDamage PARTITION Damage rather than replacing it:
    // under O54 the Damage target is the SHARED pool, feeding both delivery
    // lanes, and these two are the narrower, more characterful lines a Caster
    // or a Gunsmith tree wants. A node authors one of the three, never Damage
    // plus a partition — that is one lane's additive bucket double-dipped from
    // a single node. BreakerDamagePoolFor below is the mapping, in one place.
    AbilityDamage,
    // Resource cost of abilities. The aggregated attribute ALREADY EXISTS
    // (EBreakerAggregatedAttribute::ResourceCostMultiplier) and gear already
    // bids into it; only the tree-side enum entry was missing. Authored as an
    // Increased percentage OF THE REDUCTION, matching the affix, because that
    // is the only shape two layers can share one additive bucket in.
    // Named by Swift.Frenzy.NoSafety, Swift.Kinetic.SpendToLive,
    // Swift.Marksman.Ledger and the whole Caster cost side.
    AbilityCost,
    // Cooldown reduction on abilities, stored as the divisor exactly as
    // DashCooldownReduction is. WIRED 2026-08-16: UBreakerGameplayAbility::
    // ApplyCooldown now reads through GetCooldownSeconds, which divides the
    // authored seconds by the composed reduction; a definition that authors
    // no cooldown (every Caster ability, T8) stays cooldown-free under any
    // divisor. Named by Swift.Kinetic.Redirect, which now authors it.
    AbilityCooldown,
    // Radius / arc / range of abilities. Named by Caster.Spellblade.Edge
    // ("Cleave's arc widens"), Swift.Marksman.CalledShot (Lead's range gate)
    // and Rot's zone. The expensive one: radius has no shared representation
    // at all — it is a differently-named UPROPERTY on each ability subclass
    // (Cleave::RangeCm, Cleave::ArcDegrees, Rot::RadiusCm), so this entry
    // needed a base-class accessor before it could land anywhere. WIRED
    // 2026-08-16: UBreakerGameplayAbility::AbilityAreaMultiplierFor is that
    // accessor, applied by Cleave's and Rot's Effective* geometry methods.
    // CalledShot's range-gate rewrite stays a tag — a gate is not a scale.
    AbilityArea,
    // Duration of ability windows and zones. Named by
    // Caster.VoidWhisperer.LongDark and Caster.VoidWhisperer.Lingering. Same
    // shape problem as AbilityArea; the same 2026-08-16 accessor seam
    // (AbilityDurationMultiplierFor) answers it, consumed by Rot's zone
    // duration today. Window durations on definitions adopt it as their
    // abilities are touched. LongDark was waiting on the A4 ruling for a DoT
    // More; under O95 it authors THIS target instead, so the lane it was
    // merely named by is now the lane it pays into.
    AbilityDuration,
    // The delivery-method partition's other half. WeaponDamage is O30's GUNS
    // axis; MeleeDamage is what Caster.Spellblade.Edgework's reserved More is
    // priced against ("melee-only is the tax"). Discriminated by the SourceTags
    // already on every damage request — Damage_Melee exists today in
    // Abilities/BreakerAbilityTags.h — which is why these are stat targets and
    // not conditions: the discriminator is a property of the hit, not of live
    // state. See the vocabulary document §2.1.
    WeaponDamage,
    MeleeDamage,

    // ---- Defence ---------------------------------------------------------
    // Incoming damage reduction. The node layer has never been able to author
    // defence beyond flat Health, which is why Swift.Kinetic.MomentumShield
    // could not be written. Stored as a REDUCTION percentage joining one
    // additive bucket, never as a multiplier per source — stacking multiplier
    // sources is how a build reaches immunity by accident.
    IncomingDamageReduction,
    // Flat mitigation. The aggregated attribute EXISTS and its comment invites
    // this entry by name: "the moment a second layer does (a Bulwark node) the
    // two are additive from day one instead of repeating the gear-x-tree
    // multiplication bug."
    Armor,
    // Health leech. Named by Caster.Spellblade.Bloodprice.
    Lifesteal,

    // ---- Class resource --------------------------------------------------
    // Maximum resource. Class-Kits flags Caster.Multispell.Reservoir as "the
    // one intentional stat node in the Caster kit, and the enum has no Maximum
    // Resource target to carry it" — the single clearest case in the content of
    // a design that wanted a number and could not write it. Attribute exists
    // (MaxClassResource).
    MaxClassResource,
    // Resource generated per second. Attribute exists (ClassResourceRegen);
    // gear bids, the class loops do not yet. Named by
    // Caster.VoidWhisperer.Patience, .Seep and .StandingWater.
    ClassResourceRegen,
    // Rate at which the resource DECAYS. No attribute, and it cannot simply get
    // one: decay is a per-loop rule computed inside each class component
    // (UBreakerMomentumComponent::DecayRateForSpeed and its four siblings), not
    // a shared stat. The five components already have the seam that consumes it
    // — PushLoopOverride — so this entry is plumbing to an existing valve, not
    // a new system. Named by Swift.Kinetic.NoGround, Swift.Marksman.Reserve and
    // Swift.Frenzy.NoSafety, the three tier-4 nodes with real downsides.
    // WIRED 2026-08-16: exactly that plumbing. The aggregate lands on
    // FBreakerNodeStats::ClassResourceDecayMultiplier and
    // UBreakerProgressionComponent::PushLoopValveOverrides delivers it as a
    // keyed loop override; all three naming nodes now author it. Sign
    // convention and per-loop consumer status are on the lane register below.
    ClassResourceDecay,

    // ---- Weapon handling (O30's GUNS axis) -------------------------------
    // Attribute exists (FireRateMultiplier); O34 names fire rate a "named,
    // watched, currently-uncapped lane", so a tree line here is watched too.
    FireRate,
    // Attribute exists (DashCooldownReduction). AggregateStats already carries
    // a comment asking for exactly this: "There is deliberately NO
    // dash-cooldown line here: EBreakerNodeStatTarget has no dash entry, so no
    // node can author one ... a tree dash node only needs the enum entry and
    // one line here, and it will be additive from the first day it exists."
    DashCooldown,
    // Recoil recovery rate. Core.Volley.TriggerDiscipline is a tier-1, cost-1
    // GATEWAY node — the cheapest and most-purchased node of its constellation
    // — and it does literally nothing today for want of this entry.
    RecoilRecovery,
    // Spread / accuracy. Named by Swift.Marksman.Steady.
    WeaponSpread,
    // Projectile count. Named by Core.Volley.LastRound and the Salvo/Barrage
    // pair. Whole projectiles, so it is a Flat-bucket stat and rounds down; a
    // "+50% projectiles" line is meaningless on a single-shot weapon and the
    // aggregation lane must say so rather than silently firing 1.5 bullets.
    ProjectileCount,
    // Pierce count / falloff. Named by Swift.Marksman.PierceDiscipline,
    // .Overpenetration and .Sightline.
    Pierce,

    // ---- Status ----------------------------------------------------------
    // Duration of statuses this character applies. Named by
    // Caster.Multispell.Resonance ("halves their remaining duration instead").
    StatusDuration,
    // Chance to apply a status on hit. The magnitude twin of
    // Core.Affliction.OpenWound, whose own rewrite (guaranteed application on
    // weak point) stays a tag because bypassing a roll is not a percentage.
    StatusChance,

    // ---- Swift projectile identity (owner ruling 2026-08-16) --------------
    // "swifts identity should be based around multishot, pierce, chain,
    // ricochet, movement, manipulation of projectiles with your momentum".
    // Chain and ricochet join ProjectileCount and Pierce above as MECHANIC
    // COUNTS — Flat-bucket whole numbers, deliberately not Mores (O3/O34: new
    // Swift payoffs are Increased-bucket or mechanic-count, never a fourth
    // More source). Appended at the end because this enum is serialized by
    // ordinal into Data Assets; the vocabulary tests pin the original block.

    // Arcs the shot takes to a further enemy after its final target: on hit,
    // the shot jumps to the nearest legal enemy within the weapon's chain
    // radius for a reduced-damage hit. A count, so it stacks additively with
    // the momentum-state grant that is Swift's native source of it.
    ChainCount,
    // Bounces the shot may take off geometry: a shot that hits the world seeks
    // the nearest enemy in line of sight of the impact instead of dying there.
    // Swift.Marksman.Angle is the authoring node (Class-Kits §1.5 M4 rewrites
    // the geometric reflection into a seek; with no Ricochet Chance affix in
    // the item layer yet, the node also has to be the count's source).
    RicochetCount,

    Count UMETA(Hidden)
};

// Which stat targets AggregateStats actually composes today.
//
// This exists because the widening pass above added twenty-one entries and the
// aggregator consumes ten of them. Without this register, authoring a node
// against AbilityCooldown would compile, purchase, display on the board and pay
// nothing — the precise failure the project has now shipped four times (the
// third jump, the phantom ability grants, the phantom keystones, the MorePercent
// silently dropped on every stat target but Damage). O40(c) makes reachability
// part of definition-of-done; for an enum entry, "reachable" means "has an
// aggregation lane", and this is where that is stated rather than assumed.
//
// The register is maintained BY HAND on purpose. It cannot be derived — the
// aggregator's lanes are individually written lines — and a hand-maintained list
// that a test pins is honest, whereas a derived one would agree with the code by
// construction and prove nothing. When the coordinator adds a lane, this returns
// true for that entry in the same commit, and the audit test that reports the
// count is what makes the progress visible.
inline bool BreakerStatTargetHasAggregationLane(EBreakerNodeStatTarget Target)
{
    switch (Target)
    {
    case EBreakerNodeStatTarget::CriticalChance:
    case EBreakerNodeStatTarget::CriticalDamage:
    case EBreakerNodeStatTarget::MoveSpeed:
    case EBreakerNodeStatTarget::SlideSpeed:
    case EBreakerNodeStatTarget::AirControl:
    case EBreakerNodeStatTarget::DodgeChance:
    case EBreakerNodeStatTarget::BlockChance:
    case EBreakerNodeStatTarget::Health:
    case EBreakerNodeStatTarget::DamageOverTime:
    case EBreakerNodeStatTarget::Damage:
    // ---- Wired 2026-08-15, the first five of the O30 widening ------------
    // Each of these had an aggregated attribute the whole time and no way for
    // a NODE to reach it. One line each in AggregateStats, all joining the
    // same additive Increased bucket gear already bids into.
    //
    // DashCooldown, wired at last. The reason recorded against it for a
    // milestone was false when it was written: it claimed
    // EBreakerAggregatedAttribute had no dash entry at all. It has
    // DashCooldownReduction — replicated, floored, and bid into by gear's
    // Move.DashCooldown affix — so a tree line joins that same additive bucket
    // rather than multiplying beside it, which is the whole reason the
    // attribute stores the divisor.
    case EBreakerNodeStatTarget::AbilityCost:
    case EBreakerNodeStatTarget::MaxClassResource:
    case EBreakerNodeStatTarget::ClassResourceRegen:
    case EBreakerNodeStatTarget::FireRate:
    case EBreakerNodeStatTarget::Armor:
    // ---- Wired 2026-08-16, the Swift projectile pass ---------------------
    // Owner ruling of the same date: Swift's identity is multishot / pierce /
    // chain / ricochet, manipulated by Momentum. These four are Flat
    // mechanic-count lanes in AggregateStats, landing on FBreakerNodeStats
    // (BonusProjectileCount / BonusPierceCount / BonusChainCount /
    // BonusRicochetCount) and read by UBreakerWeaponComponent::GetShotChannels
    // on the fire path. They deliberately do NOT route through the aggregated
    // attribute set: no EBreakerAggregatedAttribute entry exists for any of
    // them and no gear affix bids on them yet, so the tree is the only bidder
    // and the weapon reads the progression component directly — the same
    // honest single-bidder shape Armor had before gear arrived, recorded here
    // so the day a Multishot/Pierce affix lands, these move onto the shared
    // additive bucket instead of multiplying beside it.
    case EBreakerNodeStatTarget::ProjectileCount:
    case EBreakerNodeStatTarget::Pierce:
    case EBreakerNodeStatTarget::ChainCount:
    case EBreakerNodeStatTarget::RicochetCount:
    // ---- Wired 2026-08-16, the loop valve and ability geometry -----------
    // Four more single-bidder lanes landing on FBreakerNodeStats (no
    // aggregated attribute exists for any of them and gear does not bid), the
    // same honest shape as the projectile channels above.
    //
    // ClassResourceDecay: an Increased bucket where POSITIVE means FASTER
    // decay — the sign convention comes from the three consuming nodes, which
    // author decay as a DOWNSIDE (F11 "decay is doubled" = +100, K11
    // "grounded decay increases by 50%" = +50 while Grounded) and a
    // suspension as a negative line (M9 "does not decay while ADS" = -100
    // while Aiming). The composed multiplier is floored at 0 and delivered by
    // UBreakerProgressionComponent::PushLoopValveOverrides as a keyed
    // PushLoopOverride on the owner's class resource component — the seam the
    // enum comment above names. Momentum and Grit consume it (the two loops
    // with a decay rule); Mana and Scrap have no decay at all, and Charge's
    // out-of-combat settle stays unconsumed until a Gunsmith decay node
    // exists to need it — recorded here rather than silently multiplied in.
    //
    // AbilityArea / AbilityDuration: composed to 1.0-based multipliers read
    // through the UBreakerGameplayAbility accessor seam
    // (AbilityAreaMultiplierFor / AbilityDurationMultiplierFor) and applied
    // by the subclasses that own geometry — Cleave's arc and range, Rot's
    // radius and zone duration.
    //
    // AbilityCooldown: stored as the DIVISOR exactly as DashCooldownReduction
    // is (x1.20 == a 20% shorter cooldown); UBreakerGameplayAbility::
    // ApplyCooldown divides the authored seconds by it. Caster abilities
    // author no cooldown at all (Mana IS the cooldown, T8), so the lane is
    // structurally inert for them rather than specially cased.
    case EBreakerNodeStatTarget::ClassResourceDecay:
    case EBreakerNodeStatTarget::AbilityArea:
    case EBreakerNodeStatTarget::AbilityDuration:
    case EBreakerNodeStatTarget::AbilityCooldown:
    // ---- Wired by the O54 pool split -------------------------------------
    // These two could not be wired a line at a time like everything above,
    // because until the split there was ONE damage bucket for them to land in
    // and a partition needs two. EBreakerAggregatedAttribute now carries
    // AbilityDamageMultiplier beside DamageMultiplier; WeaponDamage bids the
    // first, AbilityDamage the second, and the shared pool (the Damage target)
    // bids both through FBreakerAttributeContribution::AddSharedIncreasedDamage.
    //
    // MeleeDamage stays false, and not for want of a bucket. Melee is
    // weapon-DELIVERED under O55, so it is not a third pool — it is a narrower
    // slice of the weapon pool selected by the Damage_Melee source tag on the
    // request, which is a tag-keyed rider rather than an attribute lane. That
    // mechanism exists: BuildTargetConditionRiders publishes MeleeDamage rows
    // carrying the tag, and ReceiveDamage pays them into the weapon lane's
    // additive Increased bucket on tagged hits. An attribute lane here would
    // be a SECOND payment of the same line.
    case EBreakerNodeStatTarget::WeaponDamage:
    case EBreakerNodeStatTarget::AbilityDamage:
    case EBreakerNodeStatTarget::DashCooldown:
    // ---- Wired 2026-08-24, the Core-atlas lane pass (Phase 4, step 1) ------
    // Five single-bidder lanes landing on FBreakerNodeStats, the projectile-
    // channel shape: no aggregated attribute exists for any of them and gear
    // does not bid yet — the affix owner has their own lines queued, so the
    // day an affix lands, each of these moves onto a shared additive bucket
    // instead of multiplying beside it. Lifesteal is the one O30 target left
    // unwired on purpose: no bidder in either layer, owed a lane or a
    // retirement as its own ruling.
    //
    // IncomingDamageReduction: FLAT percentage points, joining gear's
    // family-reduction bucket in UBreakerCombatComponent::ReceiveDamage —
    // (gear + tree) summed, ONE 1-R application, never two multipliers.
    // TrueDamage answers to neither layer.
    //
    // RecoilRecovery / StatusChance / StatusDuration: one additive Increased
    // bucket each, composed to a 1.0-based multiplier floored at 0. Consumed
    // by TickRecoil's settle, ApplyBleedOnHit's roll, and ApplyStatus's
    // application-time snapshot respectively.
    //
    // WeaponSpread: the DIVISOR convention (DashCooldownReduction's): a
    // composed 1.20 is a 20% tighter cone, applied identically to the fired
    // cone and the predicted one so the crosshair cannot lie about the
    // purchase.
    case EBreakerNodeStatTarget::IncomingDamageReduction:
    case EBreakerNodeStatTarget::RecoilRecovery:
    case EBreakerNodeStatTarget::WeaponSpread:
    case EBreakerNodeStatTarget::StatusChance:
    case EBreakerNodeStatTarget::StatusDuration:
        return true;
    default:
        // Every O30 widening entry. They become true one at a time as the
        // coordinator wires each lane; see the vocabulary document §6.
        return false;
    }
}

// O54: which of the damage pools a stat target feeds, if any. One function so
// the aggregator, the target-rider builder and the tests cannot disagree about
// what "a damage line" means — they each used to answer it with their own
// `== EBreakerNodeStatTarget::Damage`, which is how a partition target could be
// added to the enum and then quietly pay nothing anywhere.
//
// Damage is the SHARED pool. It predates the split and thirty authored rows use
// it, so rather than reauthor them it takes the meaning that leaves weapon
// composition bit-identical and hands abilities the same lines: shared feeds
// both. WeaponDamage and AbilityDamage are the narrow lines new content
// authors when it wants one lane only.
enum class EBreakerDamagePool : uint8
{
    None,
    Weapon,
    Ability,
    Shared,
    DamageOverTime
};

inline EBreakerDamagePool BreakerDamagePoolFor(EBreakerNodeStatTarget Target)
{
    switch (Target)
    {
    case EBreakerNodeStatTarget::Damage:          return EBreakerDamagePool::Shared;
    case EBreakerNodeStatTarget::WeaponDamage:    return EBreakerDamagePool::Weapon;
    case EBreakerNodeStatTarget::AbilityDamage:   return EBreakerDamagePool::Ability;
    case EBreakerNodeStatTarget::DamageOverTime:  return EBreakerDamagePool::DamageOverTime;
    default:                                      return EBreakerDamagePool::None;
    }
}

// True for a target that is DELIVERED BY A RIDER and will therefore never have
// an aggregation lane, however long anyone waits.
//
// This exists because "has no lane" was doing two jobs and the report could not
// tell them apart: a target waiting for plumbing nobody has written, and a
// target that is correctly laneless BY RULE. MeleeDamage is the second kind —
// O98 rules melee a tag-keyed slice of the weapon pool rather than a fourth
// pool, so it is selected by the Damage_Melee source tag on the request and an
// aggregation lane is the wrong shape for it.
//
// The distinction is not cosmetic. Counted as a deficiency, MeleeDamage inflates
// a ceiling that is supposed to ratchet down, and it has been mis-shelved as
// "one lane away" three times — including once in a report that offered it as a
// lane the doctrine pass would light up. A number that cannot go to zero teaches
// people to stop reading it.
//
// The rider itself is live: a true return here routes the effect through
// BuildTargetConditionRiders as a source-tag row (see RequiredSourceTag on the
// rider struct) instead of leaving it to the aggregator's dead bucket.
inline bool BreakerStatTargetIsRiderDelivered(EBreakerNodeStatTarget Target)
{
    return Target == EBreakerNodeStatTarget::MeleeDamage;
}

// True for the pools a DIRECT hit reads — the two delivery lanes and the shared
// pool that feeds both. DamageOverTime is a damage pool but not a delivered
// one: it is snapshotted at application and composed by ComposeDotSourcePower,
// so the figures that describe what a hit will do (the conditional-damage
// readout, the target-side riders) exclude it, exactly as they did when Damage
// was the only entry.
inline bool BreakerIsDeliveredDamagePool(EBreakerNodeStatTarget Target)
{
    const EBreakerDamagePool Pool = BreakerDamagePoolFor(Target);
    return Pool == EBreakerDamagePool::Weapon
        || Pool == EBreakerDamagePool::Ability
        || Pool == EBreakerDamagePool::Shared;
}

// The lane a RIDER ROW pays into. For a pool-targeted row this is the pool
// itself; for a rider-delivered slice it is the pool the slice cuts —
// MeleeDamage is weapon-delivered under O55/O98, so its rows ride the weapon
// lane and pay only on weapon-delivered hits. The tag gate on the row is what
// makes it a SLICE of that lane rather than the lane itself: without it this
// mapping alone would quietly fold melee into the whole weapon pool, which is
// exactly the fold the vocabulary tests pin against.
inline EBreakerDamagePool BreakerRiderLanePoolFor(EBreakerNodeStatTarget Target)
{
    if (Target == EBreakerNodeStatTarget::MeleeDamage)
    {
        return EBreakerDamagePool::Weapon;
    }
    return BreakerDamagePoolFor(Target);
}

// Same aggregation law as equipment: flat values sum, then one additive
// Increased bucket per stat, then More multipliers compose as a product.
UENUM(BlueprintType)
enum class EBreakerNodeStatBucket : uint8
{
    Flat,
    IncreasedPercent,
    // O3: "More multipliers multiply as an unordered product; a build may hold
    // 2-3 Mores total (hard cap 3). Trees may author them only on branch
    // keystones and constellation Convergence/Keystone nodes." Appended, so
    // authored rows keep their serialized numbers.
    //
    // Authored in whole percent ABOVE 1.0 — 25.0 means x1.25 — to match how
    // every other percentage in the node tables is written. The aggregator
    // enforces the cap; see UBreakerProgressionComponent::AggregateStats.
    MorePercent
};

USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerNodeEffect
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly) EBreakerNodeStatTarget StatTarget = EBreakerNodeStatTarget::CriticalChance;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) EBreakerNodeStatBucket StatBucket = EBreakerNodeStatBucket::Flat;
    // Multiplied by the owned rank. Percent stats are authored in whole
    // percent (5.0 == 5%), matching the affix tables.
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float ValuePerRank = 0.0f;
    // A conditional effect pays out only while its condition holds, and pays
    // nothing otherwise. This is where a node stops being "+3% to everything"
    // and starts being a build decision (O27, Power-Curve §"Choices over
    // accumulation"). Default Always, so authored rows that predate the field
    // are unchanged.
    UPROPERTY(EditAnywhere, BlueprintReadOnly) EBreakerBuildCondition Condition = EBreakerBuildCondition::Always;
    // Owner ruling, verbatim: "conditions do compose yes". An effect may name
    // further conditions here and pays only while Condition AND every entry in
    // this array hold at once. That is what lets a node say "while airborne and
    // against a bleeding enemy" — two axes at once — which is where the texture
    // the owner asked for actually comes from.
    //
    // A separate array rather than a widened Condition field, for two reasons.
    // Serialization: Condition keeps its meaning and its value, so every
    // authored row and every affix row that predates composition is untouched
    // and the append-only rule is not strained. And readability: an empty array
    // is the overwhelmingly common case and costs nothing, while a bitmask
    // UPROPERTY would make the ordinary single-condition line harder to author
    // and to read in a diff than it is today.
    //
    // AND only — no OR, no NOT. The reasoning is on
    // FBreakerBuildConditionState::SatisfiesAll.
    //
    // A CAUTION THE CONTENT PASS MUST HEAR, and it is the owner's fourth
    // ruling: "conditionals should be flavour that add spice overall". Each
    // condition added to a line makes it pay less often, so the temptation is
    // to price a two-condition line dramatically higher. Do not. The backbone
    // is unconditional and ability scaling; conditions are texture. A build
    // whose only route to power is satisfying three predicates at once is the
    // failure mode, not the goal. See the vocabulary document §5.3.
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<EBreakerBuildCondition> AlsoRequires;

    // True when this effect pays less than always. Both halves matter: a line
    // with Always plus a non-empty AlsoRequires is still conditional.
    bool IsConditional() const
    {
        return Condition != EBreakerBuildCondition::Always || AlsoRequires.Num() > 0;
    }

    // Whether satisfying this effect needs to know about the actor being hit.
    // The damage pipeline uses it to decide whether a node's contribution can
    // be composed once on the source (the cheap, existing path) or has to be
    // deferred to the target side (the expensive new one) — see the vocabulary
    // document §3.2.
    bool RequiresTargetState() const
    {
        if (FBreakerBuildConditionState::IsTargetCondition(Condition)) return true;
        for (const EBreakerBuildCondition Required : AlsoRequires)
        {
            if (FBreakerBuildConditionState::IsTargetCondition(Required)) return true;
        }
        return false;
    }
};

// Aggregated output of every owned node rank. Multipliers are 1.0-based,
// bonuses are absolute (crit/dodge/block are 0..1 fractions).
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerNodeStats
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) float BonusHealth = 0.0f;
    UPROPERTY(BlueprintReadOnly) float CriticalChanceBonus = 0.0f;
    UPROPERTY(BlueprintReadOnly) float CriticalMultiplierBonus = 0.0f;
    UPROPERTY(BlueprintReadOnly) float DodgeChanceBonus = 0.0f;
    UPROPERTY(BlueprintReadOnly) float BlockChanceBonus = 0.0f;
    UPROPERTY(BlueprintReadOnly) float MoveSpeedMultiplier = 1.0f;
    UPROPERTY(BlueprintReadOnly) float SlideSpeedMultiplier = 1.0f;
    UPROPERTY(BlueprintReadOnly) float AirControlMultiplier = 1.0f;
    UPROPERTY(BlueprintReadOnly) float DamageOverTimeMultiplier = 1.0f;
    // Composed from node Damage effects PLUS the per-spent-point baseline, so
    // this is the whole tree layer's contribution to the shared additive
    // Increased bucket on the DamageMultiplier attribute.
    UPROPERTY(BlueprintReadOnly) float DamageMultiplier = 1.0f;
    // The composed More product for outgoing damage, AFTER the O3 cap has
    // been applied. 1.0 when the build holds no More node. This is a separate
    // field from DamageMultiplier on purpose: they are different buckets and
    // merging them would be the exact bug the aggregation rule exists to stop.
    UPROPERTY(BlueprintReadOnly) float DamageMoreMultiplier = 1.0f;
    // O54's ability lane, the twin of the two fields above. The shared pool is
    // inside BOTH DamageMultiplier and this one, because that is what shared
    // means; a card printing them side by side is printing what each kind of
    // hit will actually use, not two halves of one number to be added.
    UPROPERTY(BlueprintReadOnly) float AbilityDamageMultiplier = 1.0f;
    UPROPERTY(BlueprintReadOnly) float AbilityDamageMoreMultiplier = 1.0f;
    // How many More sources the build actually holds, before the cap. The skill
    // screen prints "3 / 3 MORE" from this so a fourth purchase visibly does
    // nothing rather than silently doing nothing.
    UPROPERTY(BlueprintReadOnly) int32 DamageMoreSourceCount = 0;
    // Increased damage from conditional effects that are live right now, and
    // what the same effects would be worth with every condition satisfied.
    // Whole percent, display only — the live half is already in DamageMultiplier.
    UPROPERTY(BlueprintReadOnly) float ActiveConditionalDamagePercent = 0.0f;
    UPROPERTY(BlueprintReadOnly) float PotentialConditionalDamagePercent = 0.0f;
    // ---- Swift projectile channels (owner ruling 2026-08-16) --------------
    // Flat mechanic counts, summed from the ProjectileCount / Pierce /
    // ChainCount / RicochetCount lanes. Floats because a node may author a
    // fractional projectile (the weapon accumulates the fraction across shots
    // and fires the whole pellet when it crosses 1 — a perceptible every-other-
    // shot rhythm rather than a rounding loss); pierce/chain/ricochet floor at
    // consumption. Consumed by UBreakerWeaponComponent::GetShotChannels.
    UPROPERTY(BlueprintReadOnly) float BonusProjectileCount = 0.0f;
    UPROPERTY(BlueprintReadOnly) float BonusPierceCount = 0.0f;
    UPROPERTY(BlueprintReadOnly) float BonusChainCount = 0.0f;
    UPROPERTY(BlueprintReadOnly) float BonusRicochetCount = 0.0f;
    // ---- Loop valve and ability geometry (2026-08-16) ---------------------
    // Class-resource decay rate scale, 1.0-based, floored at 0. POSITIVE
    // authored percent means FASTER decay (the consuming nodes author decay
    // as a downside; see the lane-register comment). Delivered to the class
    // resource component as a keyed loop override by
    // UBreakerProgressionComponent::PushLoopValveOverrides.
    UPROPERTY(BlueprintReadOnly) float ClassResourceDecayMultiplier = 1.0f;
    // Ability geometry scales, 1.0-based, floored at 0. Read through the
    // UBreakerGameplayAbility accessor seam by the subclasses that own
    // radius/arc/range (AbilityArea) and window/zone duration (AbilityDuration).
    UPROPERTY(BlueprintReadOnly) float AbilityAreaMultiplier = 1.0f;
    UPROPERTY(BlueprintReadOnly) float AbilityDurationMultiplier = 1.0f;
    // Cooldown reduction as the DIVISOR (DashCooldownReduction's convention:
    // 1.20 == 20% shorter). Floored just above zero so a malformed authored
    // row can never divide by zero or lengthen a cooldown to infinity.
    UPROPERTY(BlueprintReadOnly) float AbilityCooldownReduction = 1.0f;
    // ---- Core-atlas lanes (2026-08-24) -------------------------------------
    // Five more single-bidder lanes; shapes and consumers are on the lane
    // register in this file. IncomingDamageReduction is raw percentage POINTS
    // (Flat bucket) because its consumer sums it with gear's family reduction
    // into one bucket before the single 1-R application; the other four are
    // composed multipliers like their neighbours above.
    UPROPERTY(BlueprintReadOnly) float IncomingDamageReductionPercent = 0.0f;
    UPROPERTY(BlueprintReadOnly) float RecoilRecoveryMultiplier = 1.0f;
    // Spread reduction as the DIVISOR (1.20 == 20% tighter cone), floored
    // just above zero for the same reason AbilityCooldownReduction is.
    UPROPERTY(BlueprintReadOnly) float WeaponSpreadReduction = 1.0f;
    UPROPERTY(BlueprintReadOnly) float StatusChanceMultiplier = 1.0f;
    UPROPERTY(BlueprintReadOnly) float StatusDurationMultiplier = 1.0f;
    // Rule-rewrite and verb-grant nodes cannot be expressed as stats; they
    // publish a tag here and the owning system reads it.
    UPROPERTY(BlueprintReadOnly) FGameplayTagContainer GrantedTags;
};

USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerNodeRank
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName NodeId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) int32 Rank = 0;
};

USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerAbilityLoadout
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ClassAbilityOne = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ClassAbilityTwo = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Ultimate = NAME_None;

    bool Contains(FName AbilityId) const
    {
        return ClassAbilityOne == AbilityId || ClassAbilityTwo == AbilityId || Ultimate == AbilityId;
    }
};

USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerProgressionState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) EBreakerClassId PermanentClass = EBreakerClassId::None;
    // DERIVED from TotalExperience, never authoritative. It is kept in the
    // struct because a great deal of code already reads it, but the XP total
    // is the stored truth: re-deriving means a retuned curve moves every
    // existing character's level instead of leaving saved levels the new curve
    // can no longer afford. UBreakerProgressionComponent::RefreshLevelFromXp
    // is the only thing that should write it.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1", ClampMax="50")) int32 CharacterLevel = 1;
    // The XP loop O40(b) was waiting for. Cumulative and monotonic — never
    // "XP into the current level", because that representation cannot survive
    // a curve retune without stranding characters mid-level.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) int32 TotalExperience = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) int32 UnspentClassPoints = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) int32 UnspentCorePoints = 0;
    // How many level-entitled points have already been paid into the pools
    // above (XP-And-Pacing §4: 1 Class Point per level to 30, 1 Core Point
    // per level to 50). Cumulative like TotalExperience and for the same
    // reason: the entitlement is min(Level, cap) and the component pays only
    // the positive difference, so a curve retune that moves levels can never
    // double-pay, and a retune that LOWERS a level claws nothing back. The
    // slice lump seeds these to its own values — it is an advance, not a
    // bonus (see UBreakerProgressionLibrary::SliceClassPointGrant). Defaulted
    // so an existing save loads as "nothing paid yet"; LoadProgressionState
    // repairs pre-entitlement saves that already received the lump.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) int32 LevelClassPointsGranted = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) int32 LevelCorePointsGranted = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FBreakerNodeRank> ClassNodeRanks;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FBreakerNodeRank> CoreNodeRanks;
    // O111: the doctrine pool. NEW FIELDS rather than the retired class ones --
    // reusing ClassNodeRanks would work only because the migration zeroes it
    // first, and would silently reinterpret a v5 save's class build as a
    // doctrine build if that order ever moved. There is deliberately no
    // LevelDoctrinePointsGranted: the eight arrive whole at commitment, so
    // there is no cumulative entitlement to settle up against.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) int32 UnspentDoctrinePoints = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FBreakerNodeRank> DoctrineNodeRanks;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FBreakerAbilityLoadout AbilityLoadout;
    // O37: subclass commitment. None until UBreakerProgressionComponent::
    // CommitToBranch is called; that call is one-way (refuses once this is
    // already set) and only RespecAtForge(ClassPoints, ...) clears it back to
    // None. Committing EMPOWERS rather than excludes — it unlocks the named
    // branch's keystone/cornerstone tier; ordinary nodes of every branch stay
    // freely purchasable regardless (O15 intact). A defaulted UPROPERTY, so
    // an existing save loads with no commitment and stays save-compatible.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName CommittedBranch = NAME_None;
    // O100: ability acquisition, per character.
    //
    // The starters and the ultimate are free and are NOT listed here — they are
    // read from the class definition, so this holds only what was bought. An
    // empty array on a fresh character is therefore correct rather than a
    // character with nothing.
    //
    // A save written before v5 has no such property and deserializes to empty,
    // which for a legacy character means "everything you had is gone". That is
    // the one reading this field must never be given, so the v4 -> v5 step in
    // UBreakerSaveGame::MigrateToCurrent fills it before anything reads it.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> UnlockedAbilityIds;
    // Tokens in hand, and tokens ever paid. The second is the same cumulative
    // shape as LevelClassPointsGranted above and exists for the same reason: the
    // component pays the positive difference between the level entitlement and
    // what it has already paid, so a schedule retune can never double-pay and a
    // retune that LOWERS the entitlement claws nothing back.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) int32 UnspentAbilityTokens = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) int32 AbilityTokensGranted = 0;
    // O111's doctrine points, settled the same way as the two counters above.
    //
    // IT WAS DELIBERATELY ABSENT UNTIL NOW, and the reason it is here is that
    // the ruling changed underneath it: the pool used to be paid WHOLE at
    // commitment, which is an event with nothing to settle against. It is now
    // two points at each of four benchmarks, which is an entitlement -- and an
    // entitlement without a cumulative counter cannot survive a curve retune
    // that moves a character past two benchmarks at once.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) int32 LevelDoctrinePointsGranted = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1")) int32 SaveVersion = 1;
};

USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerDamageSnapshot
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) float SourcePower = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float CriticalChance = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float CriticalMultiplier = 1.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float DamageOverTimeMultiplier = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTagContainer SourceTags;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bRolledCritical = false;
};

USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerStatusApplicationSpec
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTag StatusTag;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float BaseDamagePerTick = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Duration = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float TickInterval = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 InitialStacks = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float ProcCoefficient = 1.0f;
    // DoTs snapshot this structure at application. Later source-stat changes
    // do not rewrite an already-running effect.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FBreakerDamageSnapshot Snapshot;
};
