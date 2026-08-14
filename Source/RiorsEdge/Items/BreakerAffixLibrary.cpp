#include "Items/BreakerAffixLibrary.h"

float UBreakerAffixLibrary::ValueForTier(const FBreakerAffixDefinition& Affix, int32 Tier)
{
    const int32 ClampedTier = FMath::Clamp(Tier, TopTier, WorstTier);
    if (ClampedTier == 0) return Affix.ValueAtT1 * TierSpikeT0Multiplier;
    if (ClampedTier == TopTier) return Affix.ValueAtT1 * TierSpikeTopMultiplier;

    // Position along the ladder: 0 at the worst tier, 1 at the best normal one.
    const float Span = static_cast<float>(WorstTier - BestNormalTier);
    const float Position = (static_cast<float>(WorstTier - ClampedTier)) / Span;
    const float Shaped = FMath::Pow(FMath::Clamp(Position, 0.0f, 1.0f), TierCurveExponent);

    // Geometric between the two authored anchors — see the derivation on the
    // declaration. The geometric form needs a positive floor and a ceiling
    // above it; an affix authored with a zero or inverted band falls back to a
    // shaped LERP, which is still monotonic and still back-loaded rather than
    // producing a NaN in the damage pipeline. Nothing in the slice pool takes
    // that branch, and RiorsEdge.Items.TierLadder covers it so the first affix
    // that does is not a silent zero.
    if (Affix.ValueAtT12 <= UE_KINDA_SMALL_NUMBER || Affix.ValueAtT1 <= Affix.ValueAtT12)
    {
        return FMath::Lerp(Affix.ValueAtT12, Affix.ValueAtT1, Shaped);
    }
    return Affix.ValueAtT12 * FMath::Pow(Affix.ValueAtT1 / Affix.ValueAtT12, Shaped);
}

int32 UBreakerAffixLibrary::BestTierForItemLevel(int32 ItemLevel)
{
    // Levels 1-120 map onto T12..T1: one tier unlocked every 10 levels.
    // 120 / 12 tiers is exact, so T1's band is ilvl 111-120 and every other
    // tier owns a full ten levels — the old ladder gave T1 the single level 50
    // and nothing else, which made the top tier a threshold rather than a band.
    const int32 Clamped = FMath::Clamp(ItemLevel, 1, MaxItemLevel);
    const int32 LevelsPerTier = MaxItemLevel / WorstTier;   // 10
    return FMath::Clamp(WorstTier - (Clamped - 1) / LevelsPerTier, BestNormalTier, WorstTier);
}

int32 UBreakerAffixLibrary::TierCapForRarity(EBreakerItemRarity Rarity)
{
    // Re-derived for the 12-tier ladder; the derivation is on the declaration.
    switch (Rarity)
    {
    case EBreakerItemRarity::Standard: return 4;   // O2 PLACEHOLDER
    case EBreakerItemRarity::Uncommon: return 2;   // O2 PLACEHOLDER
    default: return TopTier;
    }
}

void UBreakerAffixLibrary::AffixCountRangeForRarity(EBreakerItemRarity Rarity, int32& OutMinimum, int32& OutMaximum)
{
    switch (Rarity)
    {
    case EBreakerItemRarity::Standard:    OutMinimum = 1; OutMaximum = 2; break;
    case EBreakerItemRarity::Uncommon:    OutMinimum = 2; OutMaximum = 3; break;
    case EBreakerItemRarity::Exceptional: OutMinimum = 3; OutMaximum = 5; break;
    case EBreakerItemRarity::Aberrant:    OutMinimum = 4; OutMaximum = 6; break;
    case EBreakerItemRarity::Anomalous:   OutMinimum = 5; OutMaximum = 6; break;
    default:                              OutMinimum = 1; OutMaximum = 1; break;
    }
}

namespace
{
    FBreakerAffixDefinition MakeAffix(
        FName AffixId,
        const TCHAR* DisplayName,
        EBreakerAffixCategory Category,
        EBreakerStatTarget StatTarget,
        EBreakerStatBucket StatBucket,
        std::initializer_list<EBreakerEquipSlot> Slots,
        float ValueAtT12,
        float ValueAtT1,
        float RollWeight = 100.0f,
        EBreakerBuildCondition Condition = EBreakerBuildCondition::Always)
    {
        FBreakerAffixDefinition Affix;
        Affix.AffixId = AffixId;
        Affix.DisplayName = FText::FromString(DisplayName);
        Affix.Category = Category;
        Affix.StatTarget = StatTarget;
        Affix.StatBucket = StatBucket;
        Affix.AllowedSlots = Slots;
        Affix.ValueAtT12 = ValueAtT12;
        Affix.ValueAtT1 = ValueAtT1;
        Affix.RollWeight = RollWeight;
        Affix.Condition = Condition;
        return Affix;
    }

    TArray<FBreakerAffixDefinition> BuildSliceAffixPool()
    {
        const std::initializer_list<EBreakerEquipSlot> AllSlots =
        {
            EBreakerEquipSlot::Helmet, EBreakerEquipSlot::BodyArmour, EBreakerEquipSlot::Gloves,
            EBreakerEquipSlot::Boots, EBreakerEquipSlot::Necklace, EBreakerEquipSlot::Waist,
            EBreakerEquipSlot::Primary, EBreakerEquipSlot::Secondary
        };

        // O29 UPLIFT. Every ceiling anchor below went up ~2.2x with the
        // widened ladder; every FLOOR anchor is untouched. That combination is
        // deliberate and it is what makes the change safe to ship:
        //
        //  - The floor is the item level 1 value, so a level-1 drop is
        //    bit-identical to what it was before O29 and the only content
        //    anybody has played does not silently move.
        //  - The pool scales UNIFORMLY, so no line becomes stronger relative
        //    to another. The per-slot identity table, the archetype leans and
        //    the offensive/defensive balance are all preserved exactly; what
        //    changed is how much a deep item level is worth, which is the
        //    entire subject of O29.
        //
        // Every number here remains an O2 PLACEHOLDER and EditAnywhere on the
        // definition. NOTE FOR THE MOVEMENT LAYER: Move.* and Core.MoveSpeed
        // inherited the same 2.2x, so the composed movement band in
        // Docs/Movement-Design.md is now reachable from gear alone at high item
        // level. That is a balance question for Movement/, which this lane does
        // not own; it is called out rather than pre-emptively retuned.
        TArray<FBreakerAffixDefinition> Pool;
        Pool.Add(MakeAffix(TEXT("Core.Health"), TEXT("Health"), EBreakerAffixCategory::Suffix, EBreakerStatTarget::Health, EBreakerStatBucket::Flat, AllSlots, 25.0f, 400.0f));
        Pool.Add(MakeAffix(TEXT("Core.ResourceRegen"), TEXT("Resource Regeneration"), EBreakerAffixCategory::Suffix, EBreakerStatTarget::ResourceRegen, EBreakerStatBucket::Flat, AllSlots, 0.5f, 7.0f));
        // Ability cost reduction. Owner ruling 2026-08-14, alongside inverting
        // the Mana bar to start full and drain: with a spend-down resource,
        // efficiency and regeneration decide how often a caster gets to act,
        // and Maximum Resource was carrying that whole axis on its own.
        // Suffix, all slots, and deliberately a peer of Resource Regeneration
        // rather than a better version of it -- efficiency pays most to a build
        // casting expensive spells rarely, regeneration to one casting cheap
        // spells constantly, so they are different decisions.
        // O2 PLACEHOLDER: 2% (T12) -> 26% (T1) of cost removed.
        Pool.Add(MakeAffix(TEXT("Core.ResourceEfficiency"), TEXT("Resource Efficiency"), EBreakerAffixCategory::Suffix, EBreakerStatTarget::ResourceEfficiency, EBreakerStatBucket::IncreasedPercent, AllSlots, 2.0f, 26.0f, 55.0f));
        Pool.Add(MakeAffix(TEXT("Core.MaxResource"), TEXT("Maximum Resource"), EBreakerAffixCategory::Suffix, EBreakerStatTarget::MaxResource, EBreakerStatBucket::Flat, AllSlots, 8.0f, 100.0f));
        Pool.Add(MakeAffix(TEXT("Core.MoveSpeed"), TEXT("Movement Speed"), EBreakerAffixCategory::Prefix, EBreakerStatTarget::MoveSpeed, EBreakerStatBucket::IncreasedPercent, AllSlots, 2.0f, 18.0f, 60.0f));
        Pool.Add(MakeAffix(TEXT("Core.DropChance"), TEXT("Drop Chance"), EBreakerAffixCategory::Suffix, EBreakerStatTarget::DropChance, EBreakerStatBucket::IncreasedPercent, AllSlots, 3.0f, 31.0f, 60.0f));
        Pool.Add(MakeAffix(TEXT("Core.PhysicalDR"), TEXT("Physical Damage Reduction"), EBreakerAffixCategory::Suffix, EBreakerStatTarget::PhysicalDamageReduction, EBreakerStatBucket::IncreasedPercent, AllSlots, 2.0f, 18.0f));
        // Slide Speed and Dash Cooldown now roll on the two WEAPON slots as
        // well as their armour homes. That is not breadth for its own sake:
        // the owner's sidearm lean is "sidearm slide speed", and a lean toward
        // a line that cannot roll on the slot at all is a comment, not a
        // feature. It also gives the Secondary slot a movement identity, which
        // pairs with the sidearm's fast swap.
        Pool.Add(MakeAffix(TEXT("Move.SlideSpeed"), TEXT("Slide Speed"), EBreakerAffixCategory::Prefix, EBreakerStatTarget::SlideSpeed, EBreakerStatBucket::IncreasedPercent, {EBreakerEquipSlot::Boots, EBreakerEquipSlot::Waist, EBreakerEquipSlot::Primary, EBreakerEquipSlot::Secondary}, 5.0f, 44.0f, 60.0f));
        Pool.Add(MakeAffix(TEXT("Move.AirControl"), TEXT("Air Control"), EBreakerAffixCategory::Prefix, EBreakerStatTarget::AirControl, EBreakerStatBucket::IncreasedPercent, {EBreakerEquipSlot::Boots, EBreakerEquipSlot::Necklace}, 5.0f, 48.0f, 60.0f));
        Pool.Add(MakeAffix(TEXT("Move.DashCooldown"), TEXT("Dash Cooldown Reduction"), EBreakerAffixCategory::Prefix, EBreakerStatTarget::DashCooldownReduction, EBreakerStatBucket::IncreasedPercent, {EBreakerEquipSlot::Boots, EBreakerEquipSlot::Gloves, EBreakerEquipSlot::Primary, EBreakerEquipSlot::Secondary}, 4.0f, 40.0f, 60.0f));

        // --- Critical, both directions ------------------------------------
        // Crit is a genuine third axis in the variance band (Power-Curve §4),
        // which it can only be if BOTH halves have range and enough slots to
        // reach it. Chance is the scarcer half deliberately: it is what turns
        // Critical Damage on, so a build that wants the crit layer has to spend
        // on both rather than stacking whichever line it happens to find.
        const std::initializer_list<EBreakerEquipSlot> CritChanceSlots =
        {
            EBreakerEquipSlot::Helmet, EBreakerEquipSlot::Gloves, EBreakerEquipSlot::Necklace,
            EBreakerEquipSlot::Primary, EBreakerEquipSlot::Secondary
        };
        const std::initializer_list<EBreakerEquipSlot> CritDamageSlots =
        {
            EBreakerEquipSlot::Helmet, EBreakerEquipSlot::Gloves, EBreakerEquipSlot::Necklace,
            EBreakerEquipSlot::Primary, EBreakerEquipSlot::Secondary
        };
        Pool.Add(MakeAffix(TEXT("Crit.Chance"), TEXT("Critical Chance"), EBreakerAffixCategory::Prefix, EBreakerStatTarget::CriticalChance, EBreakerStatBucket::Flat, CritChanceSlots, 1.0f, 9.0f, 60.0f));   // O2 PLACEHOLDER
        Pool.Add(MakeAffix(TEXT("Crit.Damage"), TEXT("Critical Damage"), EBreakerAffixCategory::Suffix, EBreakerStatTarget::CriticalDamage, EBreakerStatBucket::Flat, CritDamageSlots, 5.0f, 40.0f, 60.0f));    // O2 PLACEHOLDER

        // --- Unconditional damage, on every slot ---------------------------
        // Was gloves/neck/weapons only, which made helmet, body, boots and
        // waist STRUCTURALLY incapable of raising damage (Power-Curve §"More
        // options in every avenue"). It rolls everywhere now, and its per-line
        // value came down as the pool widened: eight small lines that add up,
        // not four large ones you either find or do not.
        // O2 PLACEHOLDER: 3% (T12) -> 35% (T1); T-1 spikes to 126%.
        Pool.Add(MakeAffix(TEXT("Offense.WeaponDamage"), TEXT("Weapon Damage"), EBreakerAffixCategory::Prefix, EBreakerStatTarget::WeaponDamage, EBreakerStatBucket::IncreasedPercent, AllSlots, 3.0f, 35.0f, 80.0f));
        // The flat half. Lands before the Increased bucket, so it is worth most
        // to a build that already has a large bucket to multiply it by — the
        // opposite scaling shape to the line above, which is the point.
        // O2 PLACEHOLDER: 1 -> 11 percentage points of base weapon damage.
        Pool.Add(MakeAffix(TEXT("Offense.AddedDamage"), TEXT("Added Damage"), EBreakerAffixCategory::Suffix, EBreakerStatTarget::AddedDamage, EBreakerStatBucket::Flat,
            {EBreakerEquipSlot::Helmet, EBreakerEquipSlot::Gloves, EBreakerEquipSlot::Waist, EBreakerEquipSlot::Necklace, EBreakerEquipSlot::Primary, EBreakerEquipSlot::Secondary}, 1.0f, 11.0f, 70.0f));

        // Cadence. Rolls only on the two WEAPON slots, deliberately: fire rate
        // is a property of the gun, and putting it on boots would make the
        // per-slot identity table meaningless. This is the line the SMG leans
        // toward (owner: "smg fire rate").
        // O2 PLACEHOLDER: 2% (T12) -> 24% (T1). Smaller than Weapon Damage
        // because it multiplies sustained output the same way while ALSO
        // shortening time-to-empty, so it is not a strictly cheaper peer.
        Pool.Add(MakeAffix(TEXT("Weapon.FireRate"), TEXT("Fire Rate"), EBreakerAffixCategory::Suffix, EBreakerStatTarget::FireRate, EBreakerStatBucket::IncreasedPercent,
            {EBreakerEquipSlot::Primary, EBreakerEquipSlot::Secondary}, 2.0f, 24.0f, 60.0f));

        // --- Conditional damage: the movement pillar as a build axis --------
        // Each rolls roughly twice the unconditional line because it is off
        // whenever you are standing still. Slot allocation is per-slot IDENTITY,
        // not a uniform spread: boots are where airborne and slide power lives,
        // the waist is slide/dash/wall, the necklace is Redline and dash, body
        // armour is the grounded-traversal piece. Two players hunting damage on
        // boots and on a necklace are hunting different lines.
        Pool.Add(MakeAffix(TEXT("Offense.AirborneDamage"), TEXT("Damage while Airborne"), EBreakerAffixCategory::Prefix, EBreakerStatTarget::AirborneDamage, EBreakerStatBucket::IncreasedPercent,
            {EBreakerEquipSlot::Boots, EBreakerEquipSlot::Helmet, EBreakerEquipSlot::Necklace, EBreakerEquipSlot::Primary}, 5.0f, 48.0f, 45.0f, EBreakerBuildCondition::Airborne));   // O2 PLACEHOLDER
        Pool.Add(MakeAffix(TEXT("Offense.SlidingDamage"), TEXT("Damage while Sliding"), EBreakerAffixCategory::Prefix, EBreakerStatTarget::SlidingDamage, EBreakerStatBucket::IncreasedPercent,
            {EBreakerEquipSlot::Boots, EBreakerEquipSlot::Waist, EBreakerEquipSlot::BodyArmour, EBreakerEquipSlot::Secondary}, 5.0f, 48.0f, 45.0f, EBreakerBuildCondition::Sliding));  // O2 PLACEHOLDER
        Pool.Add(MakeAffix(TEXT("Offense.WallRideDamage"), TEXT("Damage while Wall Riding"), EBreakerAffixCategory::Prefix, EBreakerStatTarget::WallRideDamage, EBreakerStatBucket::IncreasedPercent,
            {EBreakerEquipSlot::Boots, EBreakerEquipSlot::Waist, EBreakerEquipSlot::Gloves, EBreakerEquipSlot::BodyArmour}, 6.0f, 57.0f, 40.0f, EBreakerBuildCondition::WallRiding));  // O2 PLACEHOLDER
        Pool.Add(MakeAffix(TEXT("Offense.RedlineDamage"), TEXT("Damage at Redline"), EBreakerAffixCategory::Suffix, EBreakerStatTarget::RedlineDamage, EBreakerStatBucket::IncreasedPercent,
            {EBreakerEquipSlot::Necklace, EBreakerEquipSlot::BodyArmour, EBreakerEquipSlot::Helmet, EBreakerEquipSlot::Primary}, 4.5f, 44.0f, 45.0f, EBreakerBuildCondition::Redline));  // O2 PLACEHOLDER
        Pool.Add(MakeAffix(TEXT("Offense.DashDamage"), TEXT("Damage after Dashing"), EBreakerAffixCategory::Suffix, EBreakerStatTarget::RecentlyDashedDamage, EBreakerStatBucket::IncreasedPercent,
            {EBreakerEquipSlot::Gloves, EBreakerEquipSlot::Waist, EBreakerEquipSlot::Necklace, EBreakerEquipSlot::Secondary}, 5.0f, 48.0f, 45.0f, EBreakerBuildCondition::RecentlyDashed)); // O2 PLACEHOLDER

        // --- The non-damage breadth pass [O27] -----------------------------
        // The first breadth pass took offence from one line to nine and left
        // the other axes where it found them: survivability was Physical DR
        // alone, the resource family was two lines that both did the same
        // thing slowly, and damage-over-time had node support and no gear
        // support at all. O27 asks for "significantly more options in ALL
        // avenues", so these four widen the avenues that were not offence.
        //
        // Each one was chosen because a LIVE consumer already existed and was
        // going unused. None of them is a new pipeline.

        // Flat armour. Consumer: the Armor aggregated attribute ->
        // UBreakerCombatComponent::GetEffectiveArmor() -> the mitigation
        // formula. Armour rolls on the FIVE armour pieces and nowhere else:
        // the necklace and the two weapons are the offence/utility slots in
        // the per-slot identity table, and armour on a gun reads as filler.
        // O2 PLACEHOLDER: 6 (T12) -> 75 (T1). Deliberately meaningful against
        // the target dummy's 100 without approaching the boss armour cap.
        Pool.Add(MakeAffix(TEXT("Core.Armour"), TEXT("Armour"), EBreakerAffixCategory::Suffix, EBreakerStatTarget::Armour, EBreakerStatBucket::Flat,
            {EBreakerEquipSlot::Helmet, EBreakerEquipSlot::BodyArmour, EBreakerEquipSlot::Gloves, EBreakerEquipSlot::Boots, EBreakerEquipSlot::Waist}, 6.0f, 75.0f, 90.0f));

        // Sustain, paid at an event rather than over time. Consumer:
        // UBreakerEquipmentComponent binds UBreakerCombatComponent::OnKillDealt
        // and routes it through ApplyHealing — the one healing path — so it
        // obeys the overheal clamp and is visible to every listener instead of
        // writing Health behind their backs.
        //
        // On-kill rather than on-hit on purpose: on-hit sustain scales with
        // fire rate and turns the SMG into the only defensive weapon in the
        // game, while on-kill scales with how well the build is already doing
        // and pays nothing at all against a boss. That is the correct shape for
        // a game whose difficulty lives in elites and bosses (O27).
        // O2 PLACEHOLDER: 8 (T12) -> 92 (T1) health per kill.
        Pool.Add(MakeAffix(TEXT("Core.LifeOnKill"), TEXT("Health on Kill"), EBreakerAffixCategory::Suffix, EBreakerStatTarget::LifeOnKill, EBreakerStatBucket::Flat,
            {EBreakerEquipSlot::BodyArmour, EBreakerEquipSlot::Gloves, EBreakerEquipSlot::Waist, EBreakerEquipSlot::Necklace,
             EBreakerEquipSlot::Primary, EBreakerEquipSlot::Secondary}, 8.0f, 92.0f, 70.0f));

        // The resource half of the same hook. Consumer: AddClassResource, which
        // both live class loops (Swift's Momentum, Caster's Mana) read as their
        // bank. It is the line that lets a Caster pay for the next cast by
        // landing the last kill, which the flat regen trickle cannot express.
        // O2 PLACEHOLDER: 2 (T12) -> 24 (T1).
        Pool.Add(MakeAffix(TEXT("Core.ResourceOnKill"), TEXT("Resource on Kill"), EBreakerAffixCategory::Suffix, EBreakerStatTarget::ResourceOnKill, EBreakerStatBucket::Flat,
            {EBreakerEquipSlot::Helmet, EBreakerEquipSlot::Gloves, EBreakerEquipSlot::Necklace, EBreakerEquipSlot::Waist,
             EBreakerEquipSlot::Primary, EBreakerEquipSlot::Secondary}, 2.0f, 24.0f, 70.0f));

        // Damage over time. Consumer: the DamageOverTimeMultiplier attribute,
        // snapshotted at application by every DoT in the game (the SMG's Bleed,
        // Cleave, Rot, Fracture). Six skill nodes bid on it and no affix did,
        // so an Affliction build could be assembled in the tree and not in the
        // stash — the same one-sided gap the damage pass found on the other
        // side. Its own bucket, not the weapon-damage bucket: DoTs snapshot
        // separately and always have.
        // O2 PLACEHOLDER: 5% (T12) -> 57% (T1). Larger than Weapon Damage
        // because it moves only the DoT portion of a build's output.
        Pool.Add(MakeAffix(TEXT("Offense.DoTDamage"), TEXT("Damage over Time"), EBreakerAffixCategory::Prefix, EBreakerStatTarget::DamageOverTime, EBreakerStatBucket::IncreasedPercent,
            {EBreakerEquipSlot::Helmet, EBreakerEquipSlot::BodyArmour, EBreakerEquipSlot::Gloves, EBreakerEquipSlot::Necklace, EBreakerEquipSlot::Primary}, 5.0f, 57.0f, 55.0f));
        return Pool;
    }
}

const TArray<FBreakerAffixDefinition>& UBreakerAffixLibrary::GetSliceAffixPool()
{
    static const TArray<FBreakerAffixDefinition> Pool = BuildSliceAffixPool();
    return Pool;
}

bool UBreakerAffixLibrary::IsOffensiveTarget(EBreakerStatTarget Target)
{
    switch (Target)
    {
    case EBreakerStatTarget::WeaponDamage:
    case EBreakerStatTarget::AddedDamage:
    case EBreakerStatTarget::CriticalChance:
    case EBreakerStatTarget::CriticalDamage:
    case EBreakerStatTarget::AirborneDamage:
    case EBreakerStatTarget::SlidingDamage:
    case EBreakerStatTarget::WallRideDamage:
    case EBreakerStatTarget::RedlineDamage:
    case EBreakerStatTarget::RecentlyDashedDamage:
    // Fire rate raises sustained damage output, so the breadth test counts it
    // as offence even though it lands on a different attribute.
    case EBreakerStatTarget::FireRate:
    // Damage over time is damage. It lands on its own attribute and its own
    // snapshot, but a build whose output is a DoT is not a defensive build.
    case EBreakerStatTarget::DamageOverTime:
        return true;
    default:
        return false;
    }
}

const FBreakerAffixDefinition* UBreakerAffixLibrary::FindAffix(const TArray<FBreakerAffixDefinition>& Pool, FName AffixId)
{
    return Pool.FindByPredicate([AffixId](const FBreakerAffixDefinition& Affix) { return Affix.AffixId == AffixId; });
}

// ---------------------------------------------------------------------------
// PER-ARCHETYPE AFFIX LEANS
// ---------------------------------------------------------------------------
// Each archetype leans toward the stats that express what it already IS
// mechanically, so the lean reinforces a niche the weapon table already
// authored rather than inventing a second, contradictory identity for it.
// A machinegun is the sustained-fire gun, so it leans damage; a sidearm is the
// tempo gun with the fastest swap in the game, so it leans movement.
//
// Multipliers are deliberately modest. At 2.5x a leaned line is roughly two
// and a half times as likely as it would otherwise be, which is enough to feel
// like a pattern across a session of drops and nowhere near enough to make the
// off-lean roll rare enough to be frustrating. Anything much above 4x starts
// to read as a filter, which is exactly what the owner said this must not be.
//
// Every value here is an O2 PLACEHOLDER.
namespace BreakerArchetypeLeans
{
    struct FLean
    {
        const TCHAR* AffixId;
        float Multiplier;
    };

    // Leans are additive in intent but multiplicative in effect: one affix can
    // appear once per archetype row, so a line is never double-counted.
    static const FLean SMGLeans[] = {
        // The owner named this one directly. An SMG's identity IS its cadence.
        { TEXT("Weapon.FireRate"),          3.0f },
        { TEXT("Crit.Chance"),              1.8f },
        { TEXT("Offense.AddedDamage"),      1.5f },  // flat pays a fast gun most
        // The SMG is the gun that applies Bleed on hit, so it is the one gun
        // whose own mechanics make a damage-over-time roll worth having.
        { TEXT("Offense.DoTDamage"),        2.0f },
    };

    static const FLean MachinegunLeans[] = {
        // Owner: "lmg damage". The sustained gun wants the damage lines.
        { TEXT("Offense.WeaponDamage"),     3.0f },
        { TEXT("Offense.AddedDamage"),      2.0f },
        { TEXT("Core.Health"),              1.6f },  // it is the planted gun
    };

    static const FLean SidearmLeans[] = {
        // Owner: "sidearm slide speed". The tempo gun leans movement, which
        // also makes it the natural Secondary for a movement build.
        { TEXT("Move.SlideSpeed"),          3.0f },
        { TEXT("Core.MoveSpeed"),           2.2f },
        { TEXT("Move.DashCooldown"),        1.8f },
        { TEXT("Offense.SlidingDamage"),    1.8f },
        // The tempo gun refunds tempo: the fastest swap in the game paired
        // with the resource to do something with it.
        { TEXT("Core.ResourceOnKill"),      1.8f },
    };

    static const FLean SniperLeans[] = {
        { TEXT("Crit.Damage"),              3.0f },
        { TEXT("Crit.Chance"),              1.6f },
        { TEXT("Offense.WeaponDamage"),     1.5f },
    };

    static const FLean ShotgunLeans[] = {
        { TEXT("Offense.AddedDamage"),      2.6f },  // flat pays per pellet
        { TEXT("Core.Health"),              1.8f },  // it is the close-range gun
        { TEXT("Offense.SlidingDamage"),    1.8f },
        // Sustain belongs on the gun that has to be in the fight to work.
        { TEXT("Core.LifeOnKill"),          2.0f },
    };

    static const FLean RocketLeans[] = {
        { TEXT("Offense.WeaponDamage"),     2.6f },
        { TEXT("Offense.AirborneDamage"),   2.0f },
    };

    static const FLean BurstRifleLeans[] = {
        { TEXT("Crit.Chance"),              2.2f },
        { TEXT("Crit.Damage"),              2.0f },
        { TEXT("Weapon.FireRate"),          1.6f },
    };

    static const FLean RifleLeans[] = {
        // The generalist leans generalist. Deliberately the flattest row in
        // the table: the rifle's identity is that it has no sharp edge, and a
        // strong lean here would take that away.
        { TEXT("Offense.WeaponDamage"),     1.6f },
        { TEXT("Crit.Chance"),              1.4f },
    };

    static void Rows(EBreakerWeaponArchetype Archetype, const FLean*& OutRows, int32& OutCount)
    {
        #define BREAKER_LEAN_ROW(Table) OutRows = Table; OutCount = UE_ARRAY_COUNT(Table); return;
        switch (Archetype)
        {
            case EBreakerWeaponArchetype::SMG:        BREAKER_LEAN_ROW(SMGLeans);
            case EBreakerWeaponArchetype::Machinegun: BREAKER_LEAN_ROW(MachinegunLeans);
            case EBreakerWeaponArchetype::Sidearm:    BREAKER_LEAN_ROW(SidearmLeans);
            case EBreakerWeaponArchetype::Sniper:     BREAKER_LEAN_ROW(SniperLeans);
            case EBreakerWeaponArchetype::Shotgun:    BREAKER_LEAN_ROW(ShotgunLeans);
            case EBreakerWeaponArchetype::Rocket:     BREAKER_LEAN_ROW(RocketLeans);
            case EBreakerWeaponArchetype::BurstRifle: BREAKER_LEAN_ROW(BurstRifleLeans);
            default:                                  BREAKER_LEAN_ROW(RifleLeans);
        }
        #undef BREAKER_LEAN_ROW
    }
}

float UBreakerAffixLibrary::ArchetypeAffixWeightMultiplier(EBreakerWeaponArchetype Archetype, FName AffixId)
{
    const BreakerArchetypeLeans::FLean* Rows = nullptr;
    int32 Count = 0;
    BreakerArchetypeLeans::Rows(Archetype, Rows, Count);
    for (int32 Index = 0; Index < Count; ++Index)
    {
        if (AffixId == FName(Rows[Index].AffixId))
        {
            // Clamped at 1.0 from below: a "lean" may only ever make a line
            // MORE likely. Making one less likely is a different feature with
            // a different failure mode (a stat that quietly cannot be found),
            // and nobody has asked for it.
            return FMath::Max(1.0f, Rows[Index].Multiplier);
        }
    }
    return 1.0f;
}
