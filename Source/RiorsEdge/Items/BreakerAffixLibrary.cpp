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
    // TWO SLOPES, not one. Owner ruling after playtesting O29: "the item level
    // tier capping at 8 might make for awkward feeling progression, let's bring
    // that to 6."
    //
    // A single slope of one tier per 10 levels put the CHARACTER CAP at T8 --
    // eight of twelve tiers, meaning a player who finished the levelling game
    // had seen only a third of the ladder and every tier they had met was in
    // its shallow, back-loaded lower half. The ladder was authored for the
    // endgame and the levelling game paid for it.
    //
    // So the levelling band is steeper than the endgame band:
    //
    //   ilvl   1 -> 50    T12 -> T6    ~8.2 levels per tier  (the campaign)
    //   ilvl  50 -> 120   T6  -> T1    ~14 levels per tier   (the chase)
    //
    // Reaching T6 by the character cap means a levelling player crosses half
    // the ladder and finishes standing on the shoulder of the curve, where the
    // steps start to be worth something. The endgame is longer per tier BECAUSE
    // each of those tiers is worth more, not as a tax.
    //
    // O2 PLACEHOLDER: both the anchor (T6 at the character cap) and the two
    // slopes are shape rather than balance.
    const int32 Clamped = FMath::Clamp(ItemLevel, 1, MaxItemLevel);
    if (Clamped <= CharacterCapItemLevel)
    {
        // T12 at ilvl 1 down to T6 at ilvl 50. Six steps across 49 levels.
        const int32 Steps = ((Clamped - 1) * (WorstTier - TierAtCharacterCap)) / (CharacterCapItemLevel - 1);
        return FMath::Clamp(WorstTier - Steps, TierAtCharacterCap, WorstTier);
    }
    // T6 at ilvl 50 down to T1 at ilvl 120. Five steps across 70 levels.
    const int32 Steps = ((Clamped - CharacterCapItemLevel) * (TierAtCharacterCap - BestNormalTier))
        / (MaxItemLevel - CharacterCapItemLevel);
    return FMath::Clamp(TierAtCharacterCap - Steps, BestNormalTier, TierAtCharacterCap);
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
        EBreakerBuildCondition Condition = EBreakerBuildCondition::Always,
        EBreakerItemRarity MinimumRarity = EBreakerItemRarity::Standard,
        FName PairedAffixId = NAME_None)
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
        Affix.MinimumRarity = MinimumRarity;
        Affix.PairedAffixId = PairedAffixId;
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
        // The bullet leg of the defense triad (owner ruling 2026-08-16/17).
        // CEILING EXTENDED 18 -> 26 under the ruling's "extend its tiers if
        // thin" — and the build-profile audit (finding 2) showed it was thin:
        // monster damage grows x8.5 across L10->L50 while this was the only
        // percent-DR lane on gear, so hits-to-die INVERTED (7.9 -> 3.9). The
        // floor stays 2.0 per the O29 rule (floors never move; a level-1 drop
        // stays bit-identical). At the character-cap tier T6 the line pays
        // ~6.7% instead of ~5.6%; five committed lines are ~33% (bullets
        // taken x0.67) instead of ~28%. The 60% cap (80 with Relentless)
        // still bites at three T1 lines, which is the over-commit tax.
        // O2 PLACEHOLDER: 2% (T12) -> 26% (T1).
        Pool.Add(MakeAffix(TEXT("Core.PhysicalDR"), TEXT("Physical Damage Reduction"), EBreakerAffixCategory::Suffix, EBreakerStatTarget::PhysicalDamageReduction, EBreakerStatBucket::IncreasedPercent, AllSlots, 2.0f, 26.0f));
        // The ailment leg of the triad: a chance to refuse a status
        // infliction outright, rolled once per application at
        // UBreakerStatusComponent::ApplyStatus (seeded/deterministic like
        // dodge; ticks of a landed status never re-roll). Rolls on the five
        // ARMOUR-shaped homes and not the weapons/necklace: refusing wounds
        // is what plating is for, and the per-slot identity table keeps the
        // necklace and guns as the offence/utility slots.
        // O2 PLACEHOLDER: 3% (T12) -> 30% (T1); T6 (character cap) ~8.8%, so
        // three committed lines at 50 refuse ~26% of ailments. Larger per
        // line than Physical DR because it is a COIN FLIP against one damage
        // stream, not a guaranteed cut against the main one; the 75% cap on
        // FBreakerEquipmentStats is where full T1 stacking lands.
        Pool.Add(MakeAffix(TEXT("Core.AilmentAvoidance"), TEXT("Ailment Avoidance"), EBreakerAffixCategory::Suffix, EBreakerStatTarget::AilmentAvoidance, EBreakerStatBucket::IncreasedPercent,
            {EBreakerEquipSlot::Helmet, EBreakerEquipSlot::BodyArmour, EBreakerEquipSlot::Gloves, EBreakerEquipSlot::Boots, EBreakerEquipSlot::Waist}, 3.0f, 30.0f, 70.0f));
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
        // --- O54's other two pools -----------------------------------------
        // The line above became a POOL rather than the only damage line when
        // the three-pool split landed, and these two are the rest of it.
        //
        // Increased Ability Damage, on every slot and at the same anchors as
        // Weapon Damage. Deliberately a peer and not a smaller line: it is the
        // only route an ability build has to scale, where a weapon build also
        // has fire rate, added damage and the whole projectile family. The
        // affix-breadth invariant reads "every slot can raise weapon damage;
        // every slot can raise ability damage", so AllSlots is the rule and not
        // a generosity.
        // O2 PLACEHOLDER: 3% (T12) -> 35% (T1), mirroring the weapon line
        // exactly. The ability lane has never been measured against the weapon
        // lane; PowerBand.AbilityLane is what will say whether this is right,
        // and it is the reason these values are a mirror rather than a guess
        // with a story attached.
        Pool.Add(MakeAffix(TEXT("Offense.AbilityDamage"), TEXT("Ability Damage"), EBreakerAffixCategory::Prefix, EBreakerStatTarget::AbilityDamage, EBreakerStatBucket::IncreasedPercent, AllSlots, 3.0f, 35.0f, 80.0f));
        // O56's shared line: smaller, rarer, and it feeds BOTH pools. Priced at
        // ~43% of a specific pool at the same tier, inside the ruled 40-50%
        // band — enough that a build with both a weapon and an ability payload
        // genuinely wants it, and never enough that it is the strictly better
        // pick for a build that only has one.
        //
        // EVERY SLOT, per the ruling, and "rarer" carried by VALUE alone. An
        // earlier draft expressed rarity as a narrower slot list, which reads
        // reasonable and is a different rule: the ruling says every slot, and a
        // shared line missing from five slots would make those slots' ability
        // scaling depend on finding the one narrow ability line rather than on
        // a choice between two.
        // O2 PLACEHOLDER: 1.5% (T12) -> 15% (T1), ~43% of a specific pool at
        // the same tier and inside O56's 40-50% band.
        Pool.Add(MakeAffix(TEXT("Offense.SharedDamage"), TEXT("Increased Damage"), EBreakerAffixCategory::Prefix, EBreakerStatTarget::SharedDamage, EBreakerStatBucket::IncreasedPercent, AllSlots, 1.5f, 15.0f, 80.0f));
        // The flat half. Lands before the Increased bucket, so it is worth most
        // to a build that already has a large bucket to multiply it by — the
        // opposite scaling shape to the line above, which is the point.
        // O2 PLACEHOLDER: 1 -> 11 percentage points of base weapon damage.
        Pool.Add(MakeAffix(TEXT("Offense.AddedDamage"), TEXT("Added Damage"), EBreakerAffixCategory::Suffix, EBreakerStatTarget::AddedDamage, EBreakerStatBucket::Flat,
            {EBreakerEquipSlot::Helmet, EBreakerEquipSlot::Gloves, EBreakerEquipSlot::Waist, EBreakerEquipSlot::Necklace, EBreakerEquipSlot::Primary, EBreakerEquipSlot::Secondary}, 1.0f, 11.0f, 70.0f));
        // The ability lane's flat half (the flat-half ruling, 2026-08-30). A
        // DELIBERATE MIRROR of Added Damage — same slots, same category, same
        // anchors, same weight — for the same reason Offense.AbilityDamage
        // mirrors Offense.WeaponDamage: the ability lane has never been felt,
        // so these values are a mirror rather than a guess with a story
        // attached, and PowerBand.AbilityLane is what will say whether the
        // mirror is right.
        // O2 PLACEHOLDER: 1 -> 11 percentage points of base ability damage.
        Pool.Add(MakeAffix(TEXT("Offense.AddedAbilityDamage"), TEXT("Added Ability Damage"), EBreakerAffixCategory::Suffix, EBreakerStatTarget::AddedAbilityDamage, EBreakerStatBucket::Flat,
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
        // WALL-RIDE'S LINE RETIRED WITH ITS VERB (Part One-R / O144; the affix
        // half ruled 2026-08-30). Offense.WallRideDamage lived here — {Boots,
        // Waist, Gloves, BodyArmour}, 6->57, weight 40, condition WallRiding —
        // and became a suffix-slot tax when the verb left Movement/: the
        // condition is permanently never-true, so the line paid nothing except
        // through UNBOUND's condition rewrite. A rolled copy on an old save
        // now drops and refunds at load (RestoreState's dead-line repair, the
        // O180 item half — which is what made this deletion safe to take).
        // The WallRideDamage STAT TARGET and the WallRiding enumerator stay,
        // append-only forever; nothing may re-author against either.
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

    // -----------------------------------------------------------------------
    // THE ABERRANT POOL — O11's "1-2 unique modifier affixes", landed.
    // -----------------------------------------------------------------------
    // Owner (2026-08-16): "make some cool weapons (their special affixes for
    // abberants and very special affixes on anomalous)". The design rules that
    // shaped every entry:
    //
    //  - EXISTING CHANNELS ONLY. Every line is an ordinary Flat or Increased
    //    roll on a stat target the aggregation already consumes, gated (or not)
    //    by a SELF-EVALUABLE build condition. No new attribute, no new bucket,
    //    no More — the locked aggregation law is untouched, and nothing here
    //    needs a consumer that does not already run.
    //  - BUILD-BENDERS, not bigger numbers. Each entry is either a magnitude
    //    roughly DOUBLE the ordinary pool's ceiling paid for with a condition
    //    or a companion downside, or a condition-FLIPPED payoff (Grounded,
    //    Stationary, low health, empty resource) that pays the player the
    //    ordinary pool ignores. In a game whose pillar is momentum, "pays for
    //    standing your ground" is a real build decision, not filler.
    //  - PERCEPTIBLE. Every magnitude is an O2 PLACEHOLDER, and per the O2
    //    rule a placeholder must be felt in play — these are sized to be the
    //    headline of the item they roll on.
    //
    // Exclusive to Aberrant BY POOL MEMBERSHIP: the generic loop and the Forge
    // never iterate this array, and Anomalous draws from its own pool below.
    TArray<FBreakerAffixDefinition> BreakerBuildAberrantAffixPool()
    {
        TArray<FBreakerAffixDefinition> Pool;

        // RIFTBURN. The DoT build's flagship: more than double the ordinary
        // ceiling, billed as a constant cut to direct hit damage — the gun
        // becomes the applicator, the rift does the killing.
        // O2 PLACEHOLDER: 12% -> 130% Increased DoT; bill is -12% Increased
        // Weapon Damage, constant (Downside.Riftburn).
        Pool.Add(MakeAffix(TEXT("Aberrant.Riftburn"), TEXT("Riftburn"), EBreakerAffixCategory::Prefix, EBreakerStatTarget::DamageOverTime, EBreakerStatBucket::IncreasedPercent,
            {EBreakerEquipSlot::Helmet, EBreakerEquipSlot::Necklace, EBreakerEquipSlot::Primary, EBreakerEquipSlot::Secondary}, 12.0f, 130.0f, 100.0f,
            EBreakerBuildCondition::Always, EBreakerItemRarity::Aberrant, TEXT("Downside.Riftburn")));

        // TERMINAL VELOCITY. The airborne conditional at roughly double its
        // ordinary band — the over-commit line for the Freefall family.
        // O2 PLACEHOLDER: 12% -> 110% Increased damage while Airborne.
        Pool.Add(MakeAffix(TEXT("Aberrant.TerminalVelocity"), TEXT("Terminal Velocity"), EBreakerAffixCategory::Prefix, EBreakerStatTarget::AirborneDamage, EBreakerStatBucket::IncreasedPercent,
            {EBreakerEquipSlot::Boots, EBreakerEquipSlot::Helmet, EBreakerEquipSlot::Primary, EBreakerEquipSlot::Secondary}, 12.0f, 110.0f, 100.0f,
            EBreakerBuildCondition::Airborne, EBreakerItemRarity::Aberrant));

        // BALLAST. The condition-FLIPPED one: unconditional-sized damage that
        // pays only while GROUNDED, in a game that rewards never being. The
        // tension is the movement pillar itself.
        // O2 PLACEHOLDER: 8% -> 80% Increased damage while Grounded.
        Pool.Add(MakeAffix(TEXT("Aberrant.Ballast"), TEXT("Ballast"), EBreakerAffixCategory::Prefix, EBreakerStatTarget::WeaponDamage, EBreakerStatBucket::IncreasedPercent,
            {EBreakerEquipSlot::BodyArmour, EBreakerEquipSlot::Waist, EBreakerEquipSlot::Gloves}, 8.0f, 80.0f, 100.0f,
            EBreakerBuildCondition::Grounded, EBreakerItemRarity::Aberrant));

        // OVERWOUND RECEIVER. Double the Fire Rate band, billed as a constant
        // flat cut to Critical Chance — the spray gun that cannot aim.
        // O2 PLACEHOLDER: 5% -> 48% Increased fire rate; bill is -3 flat crit
        // chance, constant (Downside.Overwound).
        Pool.Add(MakeAffix(TEXT("Aberrant.Overwound"), TEXT("Overwound Receiver"), EBreakerAffixCategory::Suffix, EBreakerStatTarget::FireRate, EBreakerStatBucket::IncreasedPercent,
            {EBreakerEquipSlot::Primary, EBreakerEquipSlot::Secondary}, 5.0f, 48.0f, 100.0f,
            EBreakerBuildCondition::Always, EBreakerItemRarity::Aberrant, TEXT("Downside.Overwound")));

        // REDLINE CHORUS. The Momentum spike, doubled. Swift-only by
        // construction exactly as Offense.RedlineDamage already is; a rarity
        // whose identity is build-benders is allowed a class-shaped line.
        // O2 PLACEHOLDER: 10% -> 100% Increased damage at Redline.
        Pool.Add(MakeAffix(TEXT("Aberrant.RedlineChorus"), TEXT("Redline Chorus"), EBreakerAffixCategory::Suffix, EBreakerStatTarget::RedlineDamage, EBreakerStatBucket::IncreasedPercent,
            {EBreakerEquipSlot::Necklace, EBreakerEquipSlot::BodyArmour, EBreakerEquipSlot::Primary}, 10.0f, 100.0f, 100.0f,
            EBreakerBuildCondition::Redline, EBreakerItemRarity::Aberrant));

        // FAILSAFE OVERRIDE. Pays only below the low-vital line (35%). The
        // biggest Increased band on the rarity because it is live precisely
        // when the player is 4-5 seconds from death (O18's TTD prices it).
        // O2 PLACEHOLDER: 15% -> 120% Increased damage while health is low.
        Pool.Add(MakeAffix(TEXT("Aberrant.Failsafe"), TEXT("Failsafe Override"), EBreakerAffixCategory::Prefix, EBreakerStatTarget::WeaponDamage, EBreakerStatBucket::IncreasedPercent,
            {EBreakerEquipSlot::Helmet, EBreakerEquipSlot::BodyArmour, EBreakerEquipSlot::Necklace}, 15.0f, 120.0f, 90.0f,
            EBreakerBuildCondition::HealthLow, EBreakerItemRarity::Aberrant));

        // DEADSTILL PROTOCOL. The other movement flip: crit damage for the
        // Breaker who plants and holds still — the marksman fantasy the
        // Stationary condition exists for.
        // O2 PLACEHOLDER: +15 -> +95 flat Critical Damage while Stationary.
        Pool.Add(MakeAffix(TEXT("Aberrant.Deadstill"), TEXT("Deadstill Protocol"), EBreakerAffixCategory::Suffix, EBreakerStatTarget::CriticalDamage, EBreakerStatBucket::Flat,
            {EBreakerEquipSlot::Helmet, EBreakerEquipSlot::Gloves, EBreakerEquipSlot::Primary}, 15.0f, 95.0f, 100.0f,
            EBreakerBuildCondition::Stationary, EBreakerItemRarity::Aberrant));

        // BREAKER'S TITHE. The clean one: no condition, no bill, just sustain
        // at 2.4x the ordinary ceiling, so the pool is not ALL knives.
        // O2 PLACEHOLDER: 20 -> 220 health per kill.
        Pool.Add(MakeAffix(TEXT("Aberrant.Tithe"), TEXT("Breaker's Tithe"), EBreakerAffixCategory::Suffix, EBreakerStatTarget::LifeOnKill, EBreakerStatBucket::Flat,
            {EBreakerEquipSlot::Gloves, EBreakerEquipSlot::Waist, EBreakerEquipSlot::Necklace, EBreakerEquipSlot::Secondary, EBreakerEquipSlot::Boots}, 20.0f, 220.0f, 90.0f,
            EBreakerBuildCondition::Always, EBreakerItemRarity::Aberrant));

        return Pool;
    }

    // -----------------------------------------------------------------------
    // THE ANOMALOUS POOL — the "very special" signature lines.
    // -----------------------------------------------------------------------
    // One per drop, sitting BESIDE the rule rewrite rather than replacing it:
    // an Anomalous is now a rule plus a signature line plus its ordinary rolls.
    // Stronger than the Aberrant pool tier for tier, and a separate pool so
    // neither rarity is a subset of the other.
    TArray<FBreakerAffixDefinition> BreakerBuildAnomalousAffixPool()
    {
        TArray<FBreakerAffixDefinition> Pool;

        // EVENT HORIZON. The apex airborne line — more than triple the
        // ordinary conditional's ceiling. The item the Deadfall wearer dreams
        // about and the Unbound wearer retires on.
        // O2 PLACEHOLDER: 25% -> 170% Increased damage while Airborne.
        Pool.Add(MakeAffix(TEXT("Anomaly.EventHorizon"), TEXT("Event Horizon"), EBreakerAffixCategory::Prefix, EBreakerStatTarget::AirborneDamage, EBreakerStatBucket::IncreasedPercent,
            {EBreakerEquipSlot::Boots, EBreakerEquipSlot::Helmet, EBreakerEquipSlot::Necklace, EBreakerEquipSlot::Primary, EBreakerEquipSlot::Secondary}, 25.0f, 170.0f, 100.0f,
            EBreakerBuildCondition::Airborne, EBreakerItemRarity::Anomalous));

        // PHASE SHEAR. The dash window as a rotation: three seconds of triple
        // the ordinary dash conditional, every dash.
        // O2 PLACEHOLDER: 20% -> 150% Increased damage after dashing.
        Pool.Add(MakeAffix(TEXT("Anomaly.PhaseShear"), TEXT("Phase Shear"), EBreakerAffixCategory::Suffix, EBreakerStatTarget::RecentlyDashedDamage, EBreakerStatBucket::IncreasedPercent,
            {EBreakerEquipSlot::Gloves, EBreakerEquipSlot::Waist, EBreakerEquipSlot::Necklace, EBreakerEquipSlot::Secondary}, 20.0f, 150.0f, 100.0f,
            EBreakerBuildCondition::RecentlyDashed, EBreakerItemRarity::Anomalous));

        // ENTROPY DEBT. Pays only while the class resource is at or below
        // ZERO — dead weight on a full tank, monstrous on a Caster running
        // Overcast into negative Mana. The most condition-flipped line in the
        // game, and it costs nothing to author because ResourceDepleted is
        // already self-evaluable.
        // O2 PLACEHOLDER: 30% -> 200% Increased damage while resource is empty.
        Pool.Add(MakeAffix(TEXT("Anomaly.EntropyDebt"), TEXT("Entropy Debt"), EBreakerAffixCategory::Prefix, EBreakerStatTarget::WeaponDamage, EBreakerStatBucket::IncreasedPercent,
            {EBreakerEquipSlot::Helmet, EBreakerEquipSlot::Necklace, EBreakerEquipSlot::Primary, EBreakerEquipSlot::BodyArmour, EBreakerEquipSlot::Gloves, EBreakerEquipSlot::Waist}, 30.0f, 200.0f, 80.0f,
            EBreakerBuildCondition::ResourceDepleted, EBreakerItemRarity::Anomalous));

        // SINGULARITY FEED. The resource engine: every kill pays roughly
        // triple the ordinary on-kill line, which is a whole cast for a Caster
        // and a Redline extension for a Swift.
        // O2 PLACEHOLDER: 8 -> 70 class resource per kill.
        Pool.Add(MakeAffix(TEXT("Anomaly.SingularityFeed"), TEXT("Singularity Feed"), EBreakerAffixCategory::Suffix, EBreakerStatTarget::ResourceOnKill, EBreakerStatBucket::Flat,
            {EBreakerEquipSlot::Helmet, EBreakerEquipSlot::Gloves, EBreakerEquipSlot::Necklace, EBreakerEquipSlot::Waist, EBreakerEquipSlot::Primary, EBreakerEquipSlot::Secondary, EBreakerEquipSlot::BodyArmour, EBreakerEquipSlot::Boots}, 8.0f, 70.0f, 100.0f,
            EBreakerBuildCondition::Always, EBreakerItemRarity::Anomalous));

        // RIFTPLATE. The survivability signature: over five times the ordinary
        // armour ceiling, billed as a constant cut to Movement Speed — plating
        // cut from the far side of a rift does not care what you wanted.
        // O2 PLACEHOLDER: 30 -> 400 flat Armour; bill is -10% Increased
        // Movement Speed, constant (Downside.Riftplate).
        Pool.Add(MakeAffix(TEXT("Anomaly.Riftplate"), TEXT("Riftplate"), EBreakerAffixCategory::Suffix, EBreakerStatTarget::Armour, EBreakerStatBucket::Flat,
            {EBreakerEquipSlot::Helmet, EBreakerEquipSlot::BodyArmour, EBreakerEquipSlot::Gloves, EBreakerEquipSlot::Boots, EBreakerEquipSlot::Waist}, 30.0f, 400.0f, 100.0f,
            EBreakerBuildCondition::Always, EBreakerItemRarity::Anomalous, TEXT("Downside.Riftplate")));

        return Pool;
    }

    // -----------------------------------------------------------------------
    // THE BILLS — companion downsides, never drawn on their own.
    // -----------------------------------------------------------------------
    // Constant negative rolls in ORDINARY buckets, following the precedent
    // Deadfall's Air Control bill set: a downside is an ordinary negative line
    // in the same additive lane everything else bids in, never a sub-1.0 More.
    // Authored with equal anchors so the bill does not scale with tier — the
    // deal is "this much, always", which is the only version a player can
    // price. (ValueForTier's degenerate-band branch makes an equal-anchor
    // definition constant across the normal tiers; the roll pipeline adds
    // these at the carrier's tier, which item level caps at T1, so the spike
    // tiers never touch them on a drop.)
    //
    // AllSlots on every entry: a bill must be legal wherever its carrier is,
    // and a carrier's slot list can be retuned without the bill silently
    // becoming an illegal line.
    TArray<FBreakerAffixDefinition> BreakerBuildSpecialDownsidePool()
    {
        const std::initializer_list<EBreakerEquipSlot> AllSlots =
        {
            EBreakerEquipSlot::Helmet, EBreakerEquipSlot::BodyArmour, EBreakerEquipSlot::Gloves,
            EBreakerEquipSlot::Boots, EBreakerEquipSlot::Necklace, EBreakerEquipSlot::Waist,
            EBreakerEquipSlot::Primary, EBreakerEquipSlot::Secondary
        };

        TArray<FBreakerAffixDefinition> Pool;
        // Riftburn's bill: direct hits pay for the burn.
        // O2 PLACEHOLDER: -12% Increased Weapon Damage, constant.
        Pool.Add(MakeAffix(TEXT("Downside.Riftburn"), TEXT("Unstable Combustion"), EBreakerAffixCategory::Suffix, EBreakerStatTarget::WeaponDamage, EBreakerStatBucket::IncreasedPercent,
            AllSlots, -12.0f, -12.0f, 1.0f, EBreakerBuildCondition::Always, EBreakerItemRarity::Aberrant));
        // Overwound's bill: the spray gun cannot aim.
        // O2 PLACEHOLDER: -3 flat Critical Chance, constant.
        Pool.Add(MakeAffix(TEXT("Downside.Overwound"), TEXT("Thrown Pattern"), EBreakerAffixCategory::Prefix, EBreakerStatTarget::CriticalChance, EBreakerStatBucket::Flat,
            AllSlots, -3.0f, -3.0f, 1.0f, EBreakerBuildCondition::Always, EBreakerItemRarity::Aberrant));
        // Riftplate's bill: mass is mass.
        // O2 PLACEHOLDER: -10% Increased Movement Speed, constant.
        Pool.Add(MakeAffix(TEXT("Downside.Riftplate"), TEXT("Dead Mass"), EBreakerAffixCategory::Prefix, EBreakerStatTarget::MoveSpeed, EBreakerStatBucket::IncreasedPercent,
            AllSlots, -10.0f, -10.0f, 1.0f, EBreakerBuildCondition::Always, EBreakerItemRarity::Anomalous));
        return Pool;
    }
}

const TArray<FBreakerAffixDefinition>& UBreakerAffixLibrary::GetSliceAffixPool()
{
    static const TArray<FBreakerAffixDefinition> Pool = BuildSliceAffixPool();
    return Pool;
}

const TArray<FBreakerAffixDefinition>& UBreakerAffixLibrary::GetAberrantAffixPool()
{
    static const TArray<FBreakerAffixDefinition> Pool = BreakerBuildAberrantAffixPool();
    return Pool;
}

const TArray<FBreakerAffixDefinition>& UBreakerAffixLibrary::GetAnomalousAffixPool()
{
    static const TArray<FBreakerAffixDefinition> Pool = BreakerBuildAnomalousAffixPool();
    return Pool;
}

const TArray<FBreakerAffixDefinition>& UBreakerAffixLibrary::GetSpecialDownsidePool()
{
    static const TArray<FBreakerAffixDefinition> Pool = BreakerBuildSpecialDownsidePool();
    return Pool;
}

const FBreakerAffixDefinition& UBreakerAffixLibrary::GetElementalResistanceAffix()
{
    // The elemental leg of the defense triad (owner ruling 2026-08-16/17),
    // AUTHORED-BUT-UNGATED — the stated choice, and why:
    //
    // The reserved-target precedent ("so it lies to nobody") bans a line that
    // aggregates to nothing. This line no longer would: the aggregated field
    // and the ReceiveDamage consumer are live, so the tooltip "Reduces
    // Elemental damage taken" is TRUE the moment it prints. But truth is not
    // the whole standard — no enemy in the slice deals Elemental damage
    // (every enemy damage site authors Physical or TrueDamage; the
    // Rift/Entropy/Void model is post-slice per O5/O38), so a droppable line
    // would be a suffix that costs a real suffix slot and pays nothing a
    // playtester can feel. That is the same failure with better paperwork.
    // So the definition lives HERE, resolvable through FindAffix (a granted
    // or test item carrying it aggregates truthfully) and iterated by NO roll
    // loop — pool exclusion by membership, the exact mechanism the special
    // pools already use. Entering the drop pool is one Add() in
    // BuildSliceAffixPool the day elemental incoming exists.
    //
    // Band mirrors Physical DR's pre-extension reference shape scaled to its
    // own job: one family of a future damage split rather than today's whole
    // incoming stream. O2 PLACEHOLDER: 2% (T12) -> 18% (T1), 60% cap
    // (FBreakerEquipmentStats::ElementalResistanceCapPercent).
    static const FBreakerAffixDefinition Affix = MakeAffix(
        TEXT("Core.ElementalResist"), TEXT("Elemental Resistance"), EBreakerAffixCategory::Suffix,
        EBreakerStatTarget::ElementalDamageReduction, EBreakerStatBucket::IncreasedPercent,
        {EBreakerEquipSlot::Helmet, EBreakerEquipSlot::BodyArmour, EBreakerEquipSlot::Gloves,
         EBreakerEquipSlot::Boots, EBreakerEquipSlot::Waist, EBreakerEquipSlot::Necklace}, 2.0f, 18.0f);
    return Affix;
}

bool UBreakerAffixLibrary::IsOffensiveTarget(EBreakerStatTarget Target)
{
    switch (Target)
    {
    case EBreakerStatTarget::WeaponDamage:
    // O54's other two pools. Both are damage by any reading, and the breadth
    // test's per-slot "can this slot raise damage at all" question has to count
    // them or a slot carrying only ability lines would read as defensive.
    case EBreakerStatTarget::AbilityDamage:
    case EBreakerStatTarget::SharedDamage:
    case EBreakerStatTarget::AddedDamage:
    case EBreakerStatTarget::AddedAbilityDamage:
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
    auto ById = [AffixId](const FBreakerAffixDefinition& Affix) { return Affix.AffixId == AffixId; };
    if (const FBreakerAffixDefinition* Found = Pool.FindByPredicate(ById)) return Found;
    // The special-pool fallback. Aggregation, comparison rows, tooltips and the
    // Forge all resolve rolled ids through this function with the SLICE pool as
    // the argument; a special line on an Aberrant/Anomalous item must resolve
    // there or it would aggregate to nothing — a line that lies. Falling back
    // here (instead of merging the pools) is what keeps the special entries out
    // of every generic candidate walk, so they can never be OFFERED below their
    // rarity while still always being READ.
    if (const FBreakerAffixDefinition* Found = GetAberrantAffixPool().FindByPredicate(ById)) return Found;
    if (const FBreakerAffixDefinition* Found = GetAnomalousAffixPool().FindByPredicate(ById)) return Found;
    // The authored-but-ungated elemental leg resolves here for the same
    // reason the special pools do: an item that CARRIES the line (granted,
    // test fixture, future content) must aggregate and print it truthfully,
    // while no generic candidate walk can ever OFFER it.
    if (GetElementalResistanceAffix().AffixId == AffixId) return &GetElementalResistanceAffix();
    return GetSpecialDownsidePool().FindByPredicate(ById);
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
