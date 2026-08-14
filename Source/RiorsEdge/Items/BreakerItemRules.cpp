#include "Items/BreakerItemRules.h"

#define LOCTEXT_NAMESPACE "BreakerItemRules"

bool FBreakerItemRuleSet::IsIdentity() const
{
    return !bAllConditionsSatisfied
        && !bAirborneAlsoGroundTraversal
        && !bAddedDamageAlsoIncreased
        && !bRegenGatedOnTraversal
        && FMath::IsNearlyEqual(PhysicalDamageReductionCap, 60.0f)
        && FireRateToIncreasedDamage == 0.0f
        && AirControlPercentDelta == 0.0f;
}

namespace
{
    // Named with the file's subject rather than something generic: this project
    // has twice shipped a unity-build collision between identically named
    // helpers in two anonymous namespaces.
    FBreakerItemRuleDefinition BreakerMakeRuleDefinition(EBreakerItemRule Rule, FText Name, FText Description, bool bRollable)
    {
        FBreakerItemRuleDefinition Definition;
        Definition.Rule = Rule;
        Definition.DisplayName = MoveTemp(Name);
        Definition.Description = MoveTemp(Description);
        Definition.bRollable = bRollable;
        return Definition;
    }

    TArray<FBreakerItemRuleDefinition> BreakerBuildRuleTable()
    {
        TArray<FBreakerItemRuleDefinition> Table;

        // ---- Rollable Anomalous rewrites ----------------------------------
        // Consumer for all four: UBreakerEquipmentComponent::AggregateStats,
        // whose output is submitted to UBreakerAttributeSet and read by
        // UBreakerCombatComponent. Nothing here stops at a card.
        Table.Add(BreakerMakeRuleDefinition(EBreakerItemRule::Unbound,
            LOCTEXT("Rule_Unbound", "UNBOUND"),
            LOCTEXT("Rule_Unbound_Desc", "Conditional affixes are always active."),
            true));
        Table.Add(BreakerMakeRuleDefinition(EBreakerItemRule::Overflow,
            LOCTEXT("Rule_Overflow", "OVERFLOW"),
            LOCTEXT("Rule_Overflow_Desc", "Each point of Added Damage also grants 1% Increased Damage."),
            true));
        Table.Add(BreakerMakeRuleDefinition(EBreakerItemRule::Prolific,
            LOCTEXT("Rule_Prolific", "PROLIFIC"),
            LOCTEXT("Rule_Prolific_Desc", "Every affix on this item resolves one tier better."),
            true));
        Table.Add(BreakerMakeRuleDefinition(EBreakerItemRule::Relentless,
            LOCTEXT("Rule_Relentless", "RELENTLESS"),
            LOCTEXT("Rule_Relentless_Desc", "Your Physical Damage Reduction is capped at 80% instead of 60%."),
            true));

        // ---- Legendary rules, never rolled --------------------------------
        Table.Add(BreakerMakeRuleDefinition(EBreakerItemRule::Deadfall,
            LOCTEXT("Rule_Deadfall", "DEADFALL"),
            LOCTEXT("Rule_Deadfall_Desc", "Damage while Airborne also applies while Sliding and while Wall Riding. 40% less Air Control."),
            false));
        Table.Add(BreakerMakeRuleDefinition(EBreakerItemRule::Cadence,
            LOCTEXT("Rule_Cadence", "CADENCE"),
            LOCTEXT("Rule_Cadence_Desc", "Fire Rate also grants Increased Damage at half its value. Cannot be worn with a Secondary."),
            false));
        Table.Add(BreakerMakeRuleDefinition(EBreakerItemRule::Overrun,
            LOCTEXT("Rule_Overrun", "OVERRUN"),
            LOCTEXT("Rule_Overrun_Desc", "Triple Resource Regeneration while airborne, sliding or wall riding. None otherwise."),
            false));
        return Table;
    }

    TArray<FBreakerLegendaryDefinition> BreakerBuildLegendaryTable()
    {
        TArray<FBreakerLegendaryDefinition> Table;

        // -------------------------------------------------------------------
        // DEADFALL — bends the CONDITION system.
        // -------------------------------------------------------------------
        // Airborne is the richest conditional line in the pool and the hardest
        // to hold: you are airborne in bursts, and the affix and node families
        // built around it (Freefall, Downforce, Terminal Velocity) pay nothing
        // the rest of the time. Deadfall makes the two GROUNDED traversal
        // states count as airborne for those lines, which turns a burst build
        // into a rotation and rewards exactly the player who over-committed.
        //
        // The bill is real and lands on the thing the build cares about most:
        // 40% less Air Control, an ordinary negative Increased percentage into
        // the same additive bucket every other layer bids on. Consumer:
        // EBreakerAggregatedAttribute::AirControlMultiplier ->
        // UBreakerCharacterMovementComponent's air steer rate, live since the
        // movement conformance pass.
        //
        // It is NOT bUnbound. Making every conditional line free would be
        // strictly better than the Anomalous rewrite that does exactly that,
        // and a legendary that outclasses the generic rewrite makes the
        // generic rewrite dead content.
        FBreakerLegendaryDefinition Deadfall;
        Deadfall.LegendaryId = TEXT("Legendary.Deadfall");
        Deadfall.DisplayName = LOCTEXT("Legendary_Deadfall", "DEADFALL");
        Deadfall.Slot = EBreakerEquipSlot::Boots;
        Deadfall.Rule = EBreakerItemRule::Deadfall;
        Deadfall.GuaranteedAffixIds = {TEXT("Offense.AirborneDamage"), TEXT("Move.SlideSpeed"), TEXT("Core.MoveSpeed")};
        Table.Add(Deadfall);

        // -------------------------------------------------------------------
        // CADENCE — bends the BUCKET rule and the SLOT rule.
        // -------------------------------------------------------------------
        // Fire Rate is a peer of Weapon Damage that lands on a DIFFERENT
        // attribute (FireRateMultiplier -> GetEffectiveRoundsPerMinute), so the
        // two never compound. Cadence makes half of it compound, which is the
        // first reason in the game to stack cadence past the point the gun
        // feels fast.
        //
        // The bill is the Secondary slot, and that is the whole design: a
        // second gun is worth roughly a whole item's affixes plus a swap-tempo
        // option, so this is a genuine loadout decision rather than an upside.
        // It EJECTS rather than refuses, in both directions, because nothing in
        // this component refuses an equip — the cap does not, and a rule should
        // not invent a second, harsher failure mode for the player to discover.
        FBreakerLegendaryDefinition Cadence;
        Cadence.LegendaryId = TEXT("Legendary.Cadence");
        Cadence.DisplayName = LOCTEXT("Legendary_Cadence", "CADENCE");
        Cadence.Slot = EBreakerEquipSlot::Primary;
        Cadence.Rule = EBreakerItemRule::Cadence;
        Cadence.GuaranteedAffixIds = {TEXT("Weapon.FireRate"), TEXT("Offense.WeaponDamage"), TEXT("Offense.AddedDamage")};
        Table.Add(Cadence);

        // -------------------------------------------------------------------
        // OVERRUN — bends the RESOURCE REGEN rule.
        // -------------------------------------------------------------------
        // Gear resource regeneration is a flat per-second trickle ticked on the
        // server by UBreakerEquipmentComponent, which means it pays a player
        // standing in the safe ring exactly as well as one in a fight. Overrun
        // deletes the trickle and pays triple for movement, which is the only
        // item in the game that makes the class-resource loop a movement
        // question — and the Momentum and Mana loops are both live consumers of
        // ClassResource.
        //
        // Deliberately NOT a damage item. Two of the three legendaries would
        // otherwise be offence, and the survivability/resource axes are the
        // thin ones.
        FBreakerLegendaryDefinition Overrun;
        Overrun.LegendaryId = TEXT("Legendary.Overrun");
        Overrun.DisplayName = LOCTEXT("Legendary_Overrun", "OVERRUN");
        Overrun.Slot = EBreakerEquipSlot::Waist;
        Overrun.Rule = EBreakerItemRule::Overrun;
        Overrun.GuaranteedAffixIds = {TEXT("Core.ResourceRegen"), TEXT("Core.MaxResource"), TEXT("Offense.SlidingDamage")};
        Table.Add(Overrun);

        return Table;
    }
}

const TArray<FBreakerItemRuleDefinition>& UBreakerItemRuleLibrary::GetRuleDefinitions()
{
    static const TArray<FBreakerItemRuleDefinition> Table = BreakerBuildRuleTable();
    return Table;
}

FBreakerItemRuleDefinition UBreakerItemRuleLibrary::FindRule(EBreakerItemRule Rule)
{
    for (const FBreakerItemRuleDefinition& Definition : GetRuleDefinitions())
    {
        if (Definition.Rule == Rule) return Definition;
    }
    return FBreakerItemRuleDefinition();
}

const TArray<EBreakerItemRule>& UBreakerItemRuleLibrary::GetRollableRules()
{
    static const TArray<EBreakerItemRule> Rollable = []
    {
        TArray<EBreakerItemRule> Rules;
        for (const FBreakerItemRuleDefinition& Definition : GetRuleDefinitions())
        {
            if (Definition.bRollable) Rules.Add(Definition.Rule);
        }
        return Rules;
    }();
    return Rollable;
}

EBreakerItemRule UBreakerItemRuleLibrary::RollRule(int32 RandomSeed)
{
    const TArray<EBreakerItemRule>& Rollable = GetRollableRules();
    if (Rollable.IsEmpty()) return EBreakerItemRule::None;
    // Uniform, for the same reason the weapon archetype draw is uniform: a
    // weighted table here would be a rarity system for RULES, which is a
    // separate design nobody has ruled on.
    FRandomStream Random(RandomSeed);
    return Rollable[Random.RandRange(0, Rollable.Num() - 1)];
}

const TArray<FBreakerLegendaryDefinition>& UBreakerItemRuleLibrary::GetLegendaries()
{
    static const TArray<FBreakerLegendaryDefinition> Table = BreakerBuildLegendaryTable();
    return Table;
}

FBreakerLegendaryDefinition UBreakerItemRuleLibrary::FindLegendary(FName LegendaryId)
{
    for (const FBreakerLegendaryDefinition& Definition : GetLegendaries())
    {
        if (Definition.LegendaryId == LegendaryId) return Definition;
    }
    return FBreakerLegendaryDefinition();
}

FBreakerLegendaryDefinition UBreakerItemRuleLibrary::FindLegendaryForSlot(EBreakerEquipSlot Slot)
{
    for (const FBreakerLegendaryDefinition& Definition : GetLegendaries())
    {
        if (Definition.Slot == Slot) return Definition;
    }
    return FBreakerLegendaryDefinition();
}

FBreakerItemRuleSet UBreakerItemRuleLibrary::ResolveRules(const TArray<FBreakerItemInstance>& Items)
{
    FBreakerItemRuleSet Set;
    for (const FBreakerItemInstance& Item : Items)
    {
        switch (Item.Rule)
        {
        case EBreakerItemRule::Unbound:
            Set.bAllConditionsSatisfied = true;
            break;
        case EBreakerItemRule::Overflow:
            Set.bAddedDamageAlsoIncreased = true;
            break;
        case EBreakerItemRule::Relentless:
            // Loosest wins, so two sources cannot compose into something
            // neither of them authored.
            Set.PhysicalDamageReductionCap = FMath::Max(Set.PhysicalDamageReductionCap, 80.0f);  // O2 PLACEHOLDER
            break;
        case EBreakerItemRule::Deadfall:
            Set.bAirborneAlsoGroundTraversal = true;
            Set.AirControlPercentDelta -= 40.0f;  // O2 PLACEHOLDER
            break;
        case EBreakerItemRule::Cadence:
            Set.FireRateToIncreasedDamage = FMath::Max(Set.FireRateToIncreasedDamage, 0.5f);  // O2 PLACEHOLDER
            break;
        case EBreakerItemRule::Overrun:
            Set.bRegenGatedOnTraversal = true;
            Set.TraversalRegenMultiplier = FMath::Max(Set.TraversalRegenMultiplier, 3.0f);  // O2 PLACEHOLDER
            break;
        default:
            // Prolific is resolved per ITEM in TierUpliftForItem, not here: it
            // rewrites the tier of the affixes on the item carrying it, and
            // folding it into a wearer-wide set would leak it onto every other
            // piece.
            break;
        }
    }
    return Set;
}

bool UBreakerItemRuleLibrary::RulesConflict(const FBreakerItemInstance& A, const FBreakerItemInstance& B)
{
    if (!A.IsValid() || !B.IsValid()) return false;
    // Cadence occupies both hands. Symmetric by construction so the equipment
    // component and PreviewEquip cannot disagree about which direction ejects.
    auto BreakerCadenceBlocks = [](const FBreakerItemInstance& Rule, const FBreakerItemInstance& Other)
    {
        return Rule.Rule == EBreakerItemRule::Cadence && Other.Slot == EBreakerEquipSlot::Secondary;
    };
    return BreakerCadenceBlocks(A, B) || BreakerCadenceBlocks(B, A);
}

int32 UBreakerItemRuleLibrary::TierUpliftForItem(const FBreakerItemInstance& Item)
{
    return Item.Rule == EBreakerItemRule::Prolific ? 1 : 0;
}

#undef LOCTEXT_NAMESPACE
