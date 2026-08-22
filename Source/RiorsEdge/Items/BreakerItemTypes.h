#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Progression/BreakerBuildConditions.h"
#include "Weapons/BreakerWeaponArchetype.h"
#include "BreakerItemTypes.generated.h"

// APPEND ONLY. NEVER INSERT, NEVER REORDER, NEVER REUSE A RETIRED VALUE.
// The three enums below are serialized BY VALUE into every saved item — slot,
// rarity and affix category all ride in FBreakerItemInstance — so inserting an
// entry re-points every stored item after the insertion point. A new slot
// silently moves every saved Necklace into the Waist; a new rarity between
// Aberrant and Anomalous turns every stored Anomalous into something else.
// There is no version check that catches this, because the file is not
// corrupt: it is valid data meaning something different. Renaming an
// enumerator is safe (the value does not move); moving one is not.
UENUM(BlueprintType)
enum class EBreakerEquipSlot : uint8
{
    Helmet,
    BodyArmour,
    Gloves,
    Boots,
    Necklace,
    Waist,
    Primary,
    Secondary,
    Count UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EBreakerItemRarity : uint8
{
    Standard,
    Uncommon,
    Exceptional,
    Aberrant,
    Anomalous
};

UENUM(BlueprintType)
enum class EBreakerAffixCategory : uint8
{
    Prefix,
    Suffix
};

// How a rolled value combines during stat aggregation. Flat values sum first,
// then all Increased percentages sum into a single additive bucket applied
// once. More multipliers are reserved for tree/Anomalous rule rewrites and
// multiply individually; affixes must not use them.
UENUM(BlueprintType)
enum class EBreakerStatBucket : uint8
{
    Flat,
    IncreasedPercent,
    MorePercent
};

UENUM(BlueprintType)
enum class EBreakerStatTarget : uint8
{
    Health,
    ResourceRegen,
    MaxResource,
    MoveSpeed,
    DropChance,
    PhysicalDamageReduction,
    // AUTHORED, CONSUMED, NOT YET DROPPABLE (owner ruling 2026-08-16, the
    // defense triad). This target now HAS an aggregated field
    // (FBreakerEquipmentStats::ElementalResistancePercent) and a live consumer
    // (UBreakerCombatComponent::ReceiveDamage folds it into the incoming
    // multiplier for EBreakerDamageFamily::Elemental, the exact mirror of the
    // PhysicalDamageReduction read site) — so a line that rolls it genuinely
    // reduces Elemental damage. What does not exist yet is any enemy that
    // DEALS Elemental damage: every enemy damage site in Combat/ authors
    // Physical or TrueDamage, and the per-element model (Rift/Entropy/Void)
    // is post-slice per O5/O38. The affix definition therefore stays OUT of
    // the droppable pools (UBreakerAffixLibrary::GetElementalResistanceAffix
    // holds it, resolvable but never rolled) so no player spends a suffix on
    // a stat the slice cannot test them with — the "lies to nobody" rule,
    // one step up: the LINE is honest, so the POOL has to be. The day
    // elemental incoming lands, pool entry is one Add() in
    // BuildSliceAffixPool.
    ElementalDamageReduction,
    CriticalChance,
    CriticalDamage,
    SlideSpeed,
    AirControl,
    DashCooldownReduction,
    WeaponDamage,
    // --- Appended under O27, never inserted -------------------------------
    // Rolled affixes are save data keyed by AffixId, but authored affix
    // definitions serialize this enum BY VALUE, so every entry above keeps its
    // number forever. New targets go on the end.
    //
    // Added Damage is the FLAT half of the offensive pair the slice was missing.
    // It bids into the Flat lane of the DamageMultiplier attribute, whose base
    // is 1.0, so it is added BEFORE the Increased bucket multiplies — the "added
    // damage" shape every ARPG has and the reason a flat line and an increased
    // line are different decisions rather than the same line twice. Authored in
    // percentage points of base weapon damage (8.0 == +0.08 to a 1.0 base).
    AddedDamage,
    // The conditional family (Power-Curve §"Choices over accumulation"). Each is
    // an ordinary Increased percentage into the SAME single additive bucket as
    // WeaponDamage — it is simply absent while its condition is false. They roll
    // materially larger than WeaponDamage because they are not always on, which
    // is what makes "build around a movement state" a real decision instead of
    // a strictly-worse version of the unconditional line.
    AirborneDamage,
    SlidingDamage,
    WallRideDamage,
    RedlineDamage,
    RecentlyDashedDamage,
    // Cadence. Appended like everything above it. Authored as an Increased
    // percentage on rounds per minute, so +10% is 10% more shots per second
    // and therefore ~10% more sustained DPS -- deliberately a peer of
    // WeaponDamage rather than a strictly better version of it, because it
    // does nothing for a single-shot burst and everything for held fire.
    FireRate,
    // --- The non-damage breadth pass, appended like everything above ------
    // O27 wants "significantly more options in ALL avenues", and the pool that
    // came out of the first breadth pass was nine offensive lines against a
    // survivability family of exactly one (Physical DR) and a resource family
    // of two. These four widen the other axes, and each one was chosen because
    // a LIVE consumer already existed and was going unused — the standard this
    // project set after shipping a node that could not raise damage.

    // Flat armour. Consumer: EBreakerAggregatedAttribute::Armor ->
    // UBreakerCombatComponent::GetEffectiveArmor() -> the mitigation formula.
    // The player's Armor attribute was authored 0 and written by NOBODY, so the
    // most ordinary defensive stat in the genre had a whole pipeline behind it
    // and no way in.
    Armour,
    // Health restored when the wearer lands a killing blow. Consumer:
    // UBreakerEquipmentComponent binds UBreakerCombatComponent::OnKillDealt and
    // routes the heal through ApplyHealing — the one healing path — so it is
    // visible to overheal rules and to every listener, rather than poking
    // Health directly.
    LifeOnKill,
    // Class resource granted on a killing blow. Same hook, AddClassResource.
    // Deliberately a peer of LifeOnKill rather than a strictly worse one: it is
    // the line that lets a Caster pay for a cast by killing something.
    ResourceOnKill,
    // Increased damage-over-time. Consumer: the DamageOverTimeMultiplier
    // attribute, snapshotted by every DoT application (Bleed on the SMG,
    // Cleave, Rot, Fracture). Skill nodes already bid on it; no affix did, so
    // a Bleed build could be built in the tree and not in the stash.
    DamageOverTime,
    // Ability cost reduction, authored as an Increased percentage OF THE
    // REDUCTION (12.0 == casts cost 12% less). Owner ruling 2026-08-14, the
    // same one inverting the Mana bar to start full and drain: once a resource
    // is spent down rather than banked up, EFFICIENCY and REGENERATION are the
    // two stats that decide how often a caster gets to act, and only
    // regeneration existed. A spend-down resource with no efficiency axis makes
    // Maximum Resource the only interesting resource stat, which is one line
    // holding up a whole class's gearing.
    ResourceEfficiency,
    // ---- The defense triad (owner ruling 2026-08-16/17) -------------------
    // Chance to resist an ailment/status infliction ENTIRELY, in whole
    // percent. The middle leg of the triad (Physical DR / ailment avoidance /
    // elemental resistance): Physical DR shrinks bullets, this REFUSES
    // ailments — the roll happens once per application inside
    // UBreakerStatusComponent::ApplyStatus (before the immunity primitive,
    // deterministic like dodge), and a refused application never lands its
    // DoT, its stacks, or its refresh. Ticks of a status that already landed
    // never re-roll — avoidance is a door, not a per-tick tax. Deliberately
    // NOT a percentage the DoT math reads: a 30% avoidance is "3 of 10
    // Bleeds never happen", which is a felt event, where "-30% DoT damage"
    // is a smaller number on a tick nobody reads.
    AilmentAvoidance,
    Count UMETA(Hidden)
};

// ---------------------------------------------------------------------------
// RULE REWRITES — what rarity MEANS above Exceptional.
// ---------------------------------------------------------------------------
// Before this, rarity gated affix COUNT and a tier ceiling and nothing else, so
// an Anomalous item was a Standard item with more lines and finding one was an
// arithmetic event rather than a build event. Item-Foundation always said
// Anomalous was meant to be the home of rule rewrites (the locked aggregation
// rule reserves More multipliers "for tree nodes and Anomalous rule rewrites")
// and nothing implemented it.
//
// THE CONSTRAINT THAT SHAPED EVERY ENTRY BELOW. O3 caps a build at THREE
// composed More multipliers and the trees already author six options against
// it, so an Anomalous rewrite that is simply a fourth More is either dead
// weight (the global clamp in FBreakerAttributeAggregator eats it) or a quiet
// nerf to the three the player chose. So none of these is a More. Each one
// changes a RULE the aggregation obeys — the precedent is a tree keystone,
// which removes an animation lock or makes casts free rather than adding a
// number.
//
// Every entry is applied inside UBreakerEquipmentComponent::AggregateStats,
// which is the one function whose output reaches both the attribute set and
// the combat component. That is deliberate: a rewrite that could only be seen
// by a card would be the same failure as an affix nobody consumes.
UENUM(BlueprintType)
enum class EBreakerItemRule : uint8
{
    None,

    // ---- Rolled on ordinary Anomalous drops ------------------------------
    // UNBOUND. Every conditional affix line the wearer has pays out regardless
    // of whether its condition holds. Rewrites the predicate, not the number:
    // the lines still land in the same single additive bucket, they simply stop
    // being switched off. Worth most to the player who over-committed to the
    // conditional family and least to one who has none, which is what makes it
    // a build item rather than a bonus.
    Unbound,
    // OVERFLOW. Each point of Added Damage also grants 1% Increased Damage.
    // A bucket-CROSSING rule: Added Damage is Flat and multiplies before the
    // bucket, and this makes the same roll count in both lanes at once. It is
    // not a More — it moves a number the player already has from one lane into
    // two.
    Overflow,
    // PROLIFIC. Every affix on THIS item resolves one tier better, T1 -> T0 ->
    // T-1. Rewrites tier resolution, and it is the only path to the T0/T-1
    // spike that does not go through the Forge.
    Prolific,
    // RELENTLESS. The wearer's Physical Damage Reduction cap becomes 80%
    // instead of 60%. A CAP rewrite, and the survivability family's first
    // reason to keep stacking a line past the point it stopped paying.
    Relentless,

    // ---- Legendary rules: never rolled, only carried by a named item -----
    // DEADFALL (boots). Damage while Airborne also pays while Sliding and while
    // Wall Riding; Air Control is reduced by 40%. Bends the CONDITION system.
    Deadfall,
    // CADENCE (primary). Fire Rate also grants Increased Damage at half its
    // value, and the weapon occupies both hands: equipping it ejects the
    // Secondary and equipping a Secondary ejects it. Bends the BUCKET rule and
    // the SLOT rule.
    Cadence,
    // OVERRUN (waist). Resource Regeneration is tripled while airborne,
    // sliding or wall riding, and zero while it is not. Bends the REGEN rule.
    Overrun,

    Count UMETA(Hidden)
};

// Tiers run T12 (worst) to T1 on a BACK-LOADED geometric curve, then spike:
// T0 = 2.2x T1, T-1 = 3.6x T1. Stored as the printed number, so Tier ranges
// 12..-1. The ladder widened from T8..T-1 under O29 (endgame power is gear
// depth; item level runs to 120). The full derivation, the value table and the
// re-siting of the spikes are on UBreakerAffixLibrary::ValueForTier.
//
// The two anchors below are the only per-affix balance numbers: the curve
// interpolates between them and authors nothing of its own.
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerAffixDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName AffixId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EBreakerAffixCategory Category = EBreakerAffixCategory::Prefix;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EBreakerStatTarget StatTarget = EBreakerStatTarget::Health;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EBreakerStatBucket StatBucket = EBreakerStatBucket::Flat;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<EBreakerEquipSlot> AllowedSlots;
    // Renamed from ValueAtT8 with the ladder (O29). Definitions are immutable
    // CONTENT, never save data — a rolled affix stores its Tier and Value — so
    // renaming the anchor cannot invalidate an existing item.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float ValueAtT12 = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float ValueAtT1 = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1")) float RollWeight = 100.0f;
    // While this is false the line contributes nothing at all. Default Always,
    // so every pre-existing affix keeps behaving exactly as authored.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EBreakerBuildCondition Condition = EBreakerBuildCondition::Always;

    // ---- The high-rarity identity pass (O11's reserved seat) --------------
    // The rarity FLOOR for this line. Standard means "the ordinary pool", which
    // is every affix that existed before this field did — the default keeps all
    // of them exactly as authored. Aberrant/Anomalous mark the SPECIAL pools:
    // those definitions live in UBreakerAffixLibrary::GetAberrantAffixPool /
    // GetAnomalousAffixPool rather than in the slice pool, so the generic affix
    // loop and the Forge's Attune candidate walk (which both iterate the slice
    // pool) structurally cannot offer them below their rarity. The field is the
    // stated intent; the pool separation is the enforcement.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EBreakerItemRarity MinimumRarity = EBreakerItemRarity::Standard;

    // The BILL. When a special affix names a companion here, rolling it also
    // adds the companion line — a constant NEGATIVE roll in an ordinary bucket
    // (never a sub-1.0 More; the locked aggregation rule has no downside
    // exemption, exactly as Deadfall's Air Control bill established). The
    // companion definitions live in UBreakerAffixLibrary::GetSpecialDownsidePool
    // and are never drawable on their own — a downside is part of the deal that
    // carried it, not loot.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName PairedAffixId = NAME_None;

    bool AllowsSlot(EBreakerEquipSlot Slot) const { return AllowedSlots.Contains(Slot); }
    bool IsConditional() const { return Condition != EBreakerBuildCondition::Always; }
    bool IsSpecial() const { return MinimumRarity >= EBreakerItemRarity::Aberrant; }
};

USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerRolledAffix
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName AffixId = NAME_None;
    // ClampMax follows the widened ladder (O29). Items rolled before it carry
    // tiers 8..-1 and stay legal — 8 is simply no longer the worst tier, so an
    // old item reads as a mid-ladder roll, which is exactly what it is.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="-1", ClampMax="12")) int32 Tier = 12;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Value = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EBreakerAffixCategory Category = EBreakerAffixCategory::Prefix;
};

// A generated item. References a definition by stable id and carries rolled
// affixes separately — definitions are immutable content, instances are save
// data (same rule as progression state: ids and numbers, never pointers).
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerItemInstance
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGuid ItemId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName DefinitionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EBreakerEquipSlot Slot = EBreakerEquipSlot::Primary;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EBreakerItemRarity Rarity = EBreakerItemRarity::Standard;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1")) int32 ItemLevel = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FBreakerRolledAffix> Affixes;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1")) int32 SaveVersion = 1;

    // WHICH GUN THIS IS. Meaningful only on Primary and Secondary; ignored on
    // armour, where it stays at its default and nothing reads it.
    //
    // Before this field, an item instance carried an item level and a list of
    // affixes and nothing else, so a dropped Primary had no answer to "which
    // of the eight archetypes am I" — the loadout screen chose the gun and the
    // item supplied only numbers. Power-Curve.md flagged that as the open
    // boundary between Items/ and Weapons/, and this closes it: a weapon drop
    // is now a WEAPON, not a stat sheet that happens to be equipped in a
    // weapon slot.
    //
    // Defaults to Rifle rather than Count so that every item saved before this
    // field existed loads as a rifle instead of as an invalid archetype.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EBreakerWeaponArchetype WeaponArchetype = EBreakerWeaponArchetype::Rifle;

    // THE RULE THIS ITEM REWRITES, or None. Rolled onto Anomalous drops and
    // fixed on legendaries; every other item in the game leaves it None.
    //
    // It is a FIELD, not something derived from Rarity, and that distinction is
    // load-bearing. Deriving it from rarity would silently hand a rewrite to
    // every Anomalous item that already exists in a save, in a test fixture, or
    // in the power-band loadouts — which build every piece at Anomalous purely
    // to lift the tier cap. An item earns a rewrite when it is ROLLED one.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EBreakerItemRule Rule = EBreakerItemRule::None;

    // Named legendary this item is, or None. Kept separate from Rule because a
    // rule is a mechanic and a legendary is an identity: two legendaries could
    // one day share a rewrite, and the display name, the signature and the drop
    // table all key off the identity.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName LegendaryId = NAME_None;

    bool HasRule() const { return Rule != EBreakerItemRule::None; }
    bool IsLegendary() const { return !LegendaryId.IsNone(); }
    bool IsValid() const { return ItemId.IsValid(); }
    // The two slots where WeaponArchetype means anything. Static so callers
    // can ask about a slot before an item exists (the loot roll needs this).
    static bool IsWeaponSlot(EBreakerEquipSlot Slot)
    {
        return Slot == EBreakerEquipSlot::Primary || Slot == EBreakerEquipSlot::Secondary;
    }
    bool IsWeapon() const { return IsWeaponSlot(Slot); }
};

// Which way one affix line moves the player relative to the piece it would
// replace. Every stat target in the slice pool is "higher is better" — Dash
// Cooldown Reduction included, because it is authored as an increase to the
// reduction — so one polarity rule covers the whole pool. A target where lower
// is better would need a polarity flag on FBreakerAffixDefinition first.
UENUM(BlueprintType)
enum class EBreakerAffixDelta : uint8
{
    Better,
    Worse,
    Parity
};

// One affix line of a candidate item, already compared against the piece it
// would replace. The UI renders the glyph; the rule lives here.
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerAffixComparison
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FName AffixId = NAME_None;
    UPROPERTY(BlueprintReadOnly) int32 Tier = 8;
    // The candidate's rolled value.
    UPROPERTY(BlueprintReadOnly) float Value = 0.0f;
    // What the equipped piece contributes to the SAME stat target in the SAME
    // bucket, summed. Zero when the equipped piece does not touch that stat.
    UPROPERTY(BlueprintReadOnly) float ComparedValue = 0.0f;
    UPROPERTY(BlueprintReadOnly) EBreakerAffixDelta Delta = EBreakerAffixDelta::Better;

    float GetDifference() const { return Value - ComparedValue; }
};

// The complete consequence of equipping one item, answered before the click.
// Two displacements can happen at once and they are deliberately separate:
// the ORDINARY one is the piece in the same slot, which every equip swaps out;
// the LIMIT one is a second piece ejected because the rarity cap (O11 /
// master sheet 4.1: Aberrant 3, Anomalous 1) is already met. The cap is never
// a refusal — it is disclosed, and the named piece is the piece that actually
// leaves.
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerEquipPreview
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) bool bSlotOccupied = false;
    UPROPERTY(BlueprintReadOnly) FBreakerItemInstance SlotDisplaced;
    // True when equipping would put the player over the cap for the
    // candidate's rarity, counting the slot swap's own departure first.
    UPROPERTY(BlueprintReadOnly) bool bExceedsRarityLimit = false;
    UPROPERTY(BlueprintReadOnly) FBreakerItemInstance LimitDisplaced;
    // Equipped count of the candidate's rarity right now, and the cap for it.
    // A limit of INDEX_NONE means the rarity is uncapped.
    UPROPERTY(BlueprintReadOnly) int32 RarityCount = 0;
    UPROPERTY(BlueprintReadOnly) int32 RarityLimit = INDEX_NONE;
    UPROPERTY(BlueprintReadOnly) TArray<FBreakerAffixComparison> AffixDeltas;

    // A THIRD displacement, and it has nothing to do with the rarity cap: a
    // legendary whose rule claims another slot ejects whatever is standing in
    // it. Cadence is the first — it occupies both hands, so it and a Secondary
    // cannot both be worn, in either direction.
    //
    // Disclosed rather than refused, exactly like the rarity cap: the piece the
    // UI names as doomed is the piece that leaves. Nothing REFUSES an equip in
    // this component and this does not start.
    UPROPERTY(BlueprintReadOnly) bool bRuleDisplaces = false;
    UPROPERTY(BlueprintReadOnly) FBreakerItemInstance RuleDisplaced;
};

// Aggregated result of everything equipped. Flat and Increased buckets are
// already combined; consumers read final values.
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerEquipmentStats
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) float BonusHealth = 0.0f;
    UPROPERTY(BlueprintReadOnly) float ResourceRegenPerSecond = 0.0f;
    UPROPERTY(BlueprintReadOnly) float BonusMaxResource = 0.0f;
    UPROPERTY(BlueprintReadOnly) float MoveSpeedMultiplier = 1.0f;
    UPROPERTY(BlueprintReadOnly) float DropChancePercent = 0.0f;
    UPROPERTY(BlueprintReadOnly) float PhysicalDamageReductionPercent = 0.0f;
    UPROPERTY(BlueprintReadOnly) float CriticalChanceBonus = 0.0f;
    UPROPERTY(BlueprintReadOnly) float CriticalMultiplierBonus = 0.0f;
    UPROPERTY(BlueprintReadOnly) float SlideSpeedMultiplier = 1.0f;
    UPROPERTY(BlueprintReadOnly) float AirControlMultiplier = 1.0f;
    UPROPERTY(BlueprintReadOnly) float DashCooldownMultiplier = 1.0f;
    // DISPLAY ONLY. Gear-granted increased weapon damage, on its own. Combat
    // does NOT read this: the same raw percentage is submitted to the
    // DamageMultiplier attribute's shared additive Increased bucket, where it
    // sums with skill-tree damage instead of multiplying against it. Reading
    // this at a damage site would double-count gear.
    UPROPERTY(BlueprintReadOnly) float WeaponDamageMultiplier = 1.0f;
    // DISPLAY ONLY, same rule as WeaponDamageMultiplier. Gear-granted flat
    // Added Damage, in percentage points of base weapon damage; combat reads it
    // through the DamageMultiplier attribute's Flat lane, never from here.
    UPROPERTY(BlueprintReadOnly) float AddedDamagePercent = 0.0f;
    // DISPLAY ONLY. Increased damage from conditional lines that are live RIGHT
    // NOW, in whole percent. Zero on a rig with no movement component, which is
    // why the aggregation tests still read clean numbers.
    UPROPERTY(BlueprintReadOnly) float ActiveConditionalDamagePercent = 0.0f;
    // DISPLAY ONLY. What the conditional lines would be worth with every
    // condition satisfied at once — the tooltip figure, so a player can see what
    // a piece is offering before they are airborne.
    UPROPERTY(BlueprintReadOnly) float PotentialConditionalDamagePercent = 0.0f;

    // ---- The non-damage breadth pass --------------------------------------
    // Flat armour. Submitted into the Armor attribute; this field is the
    // gear-only display figure, same rule as WeaponDamageMultiplier.
    UPROPERTY(BlueprintReadOnly) float BonusArmour = 0.0f;
    // Health and class resource granted on a killing blow. NOT display-only:
    // UBreakerEquipmentComponent's OnKillDealt handler reads these two directly,
    // because they are not attributes — they are amounts paid at an event.
    UPROPERTY(BlueprintReadOnly) float LifeOnKill = 0.0f;
    UPROPERTY(BlueprintReadOnly) float ResourceOnKill = 0.0f;
    // Increased damage-over-time, gear only. Combat reads the composed
    // DamageOverTimeMultiplier attribute; this is the inventory figure.
    UPROPERTY(BlueprintReadOnly) float DamageOverTimeMultiplier = 1.0f;

    // ---- Rule rewrites ----------------------------------------------------
    // Every rule currently in force from equipped items, in the order the slots
    // were walked. Duplicates are impossible in practice — a rollable rule
    // never doubles up because the non-legendary Anomalous axis caps at one
    // (O37), a legendary's rule never doubles up because the legendary axis
    // ALSO caps at one (O37, its own axis, separate from the Anomalous one
    // per O32), and the two pools never share a value — but the array does
    // not assume it.
    UPROPERTY(BlueprintReadOnly) TArray<EBreakerItemRule> ActiveRules = {};
    // O2 PLACEHOLDER: the baseline Physical Damage Reduction cap with no
    // rewrite equipped. Named once (audit item 11) so
    // FBreakerItemRuleSet::PhysicalDamageReductionCap — the value RELENTLESS
    // raises — cannot drift from this display default; the two were
    // previously two unflagged 60.0f literals.
    static constexpr float DefaultPhysicalDamageReductionCap = 60.0f;
    // The Physical DR cap actually applied, after Relentless. Published so the
    // inventory can show a raised cap instead of a number that stopped moving
    // for no visible reason.
    UPROPERTY(BlueprintReadOnly) float PhysicalDamageReductionCap = DefaultPhysicalDamageReductionCap;

    // ---- The defense triad (owner ruling 2026-08-16/17) -------------------
    // Ailment avoidance from gear, in whole percent, already capped. Consumer:
    // UBreakerStatusComponent::GetEffectiveAilmentAvoidanceChance reads this
    // (plus its own baseline) and rolls at ApplyStatus. NOT an attribute on
    // purpose — like Physical DR it is read from GetStats() at its one
    // consumer, so gear is the single writer and the cap below cannot be
    // outbid from a second lane nobody audits.
    //
    // O2 PLACEHOLDER cap: 75%, and the ceiling is load-bearing rather than
    // cosmetic — 100% avoidance would be the immunity primitive
    // (GrantStatusImmunity) rebuilt out of suffixes, and immunity is a
    // granted WINDOW, never a standing gear state. Full T1 stacking (5 lines
    // x 30%) is meant to slam into this cap; the last two lines are the
    // over-commit tax.
    static constexpr float AilmentAvoidanceCapPercent = 75.0f;
    UPROPERTY(BlueprintReadOnly) float AilmentAvoidanceChancePercent = 0.0f;

    // Elemental resistance from gear, in whole percent, already capped.
    // Consumer: UBreakerCombatComponent::ReceiveDamage, the Elemental-family
    // mirror of PhysicalDamageReductionPercent two fields up. Aggregated
    // honestly TODAY so the day an enemy deals Elemental damage the stat
    // pays with zero further wiring; the affix that feeds it is deliberately
    // not droppable yet — see EBreakerStatTarget::ElementalDamageReduction.
    // O2 PLACEHOLDER cap: mirrors the Physical DR default. No rule rewrite
    // raises it (Relentless is explicitly the PHYSICAL cap), so it is a
    // plain constant rather than a published, rewritable field.
    static constexpr float ElementalResistanceCapPercent = 60.0f;
    UPROPERTY(BlueprintReadOnly) float ElementalResistancePercent = 0.0f;
};
