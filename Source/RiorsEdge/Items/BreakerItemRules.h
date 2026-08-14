#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Items/BreakerItemTypes.h"
#include "BreakerItemRules.generated.h"

// ---------------------------------------------------------------------------
// WHAT RARITY MEANS, and the three build-defining legendaries.
// ---------------------------------------------------------------------------
// The problem this file exists to fix, stated plainly: rarity gated affix COUNT
// and a tier ceiling and nothing else, so an Anomalous item was a Standard item
// with more lines. Finding one was arithmetic. Item-Foundation had always
// reserved Anomalous as the home of rule rewrites and nothing implemented it,
// and Docs/Vertical-Slice.md scoped "three build-defining legendary items" of
// which zero existed.
//
// THE RULE THAT SHAPED EVERY DESIGN HERE: an Anomalous rewrite is not allowed
// to be a fourth More multiplier. O3 caps a build at three composed Mores and
// the trees already offer six options against that cap, so a fourth would
// either be eaten by the global clamp in FBreakerAttributeAggregator or
// silently displace one the player chose. Every rewrite below therefore changes
// a RULE the aggregation obeys, in the precedent of a tree keystone.
//
// THE SECOND RULE: every effect must have a live consumer, verified before it
// was authored. This project has already shipped a node structurally incapable
// of doing anything and an affix pool where four of eight slots could not raise
// damage; a legendary that lies is that failure at higher volume. The consumer
// for each rule is named at its entry, and RiorsEdge.Items.Rules.* asserts each
// one actually moves the number it claims to.

// A rule's authored metadata. Content, not state.
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerItemRuleDefinition
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) EBreakerItemRule Rule = EBreakerItemRule::None;
    UPROPERTY(BlueprintReadOnly) FText DisplayName;
    // Player-facing, and deliberately phrased as the rewrite rather than as a
    // stat: "conditional lines are always active", never "+X% damage".
    UPROPERTY(BlueprintReadOnly) FText Description;
    // False for a legendary rule, which is carried by exactly one named item
    // and must never appear on an ordinary Anomalous drop.
    UPROPERTY(BlueprintReadOnly) bool bRollable = false;
};

// One named legendary. A fixed signature plus a rule; the rest of the item
// rolls normally, so two Deadfalls are still different items.
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerLegendaryDefinition
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FName LegendaryId = NAME_None;
    UPROPERTY(BlueprintReadOnly) FText DisplayName;
    UPROPERTY(BlueprintReadOnly) EBreakerEquipSlot Slot = EBreakerEquipSlot::Boots;
    UPROPERTY(BlueprintReadOnly) EBreakerItemRule Rule = EBreakerItemRule::None;
    // Affixes this item always has. Rolled at the item's ordinary tier, so a
    // low-level Deadfall is a real but weak Deadfall — the rule is the constant,
    // the numbers are not. Anything the roll adds beyond these is free variance.
    UPROPERTY(BlueprintReadOnly) TArray<FName> GuaranteedAffixIds;

    bool IsValid() const { return !LegendaryId.IsNone(); }
};

// Everything the equipped set's rules add up to, resolved once per
// recalculation. A struct rather than a bag of bools because AggregateStats
// consults it in five separate places and a caller that forgot one of them
// would produce an item that half works.
struct RIORSEDGE_API FBreakerItemRuleSet
{
    // Unbound / Deadfall: conditional affix lines pay without their condition.
    bool bAllConditionsSatisfied = false;
    // Deadfall: airborne lines also pay while sliding or wall riding, WITHOUT
    // making every other conditional line free. Narrower than bUnbound on
    // purpose — the legendary is a redirection, not a superset.
    bool bAirborneAlsoGroundTraversal = false;
    // Overflow: each point of Added Damage also grants 1% Increased Damage.
    bool bAddedDamageAlsoIncreased = false;
    // Relentless: the Physical DR cap. Defaults to the same named constant
    // FBreakerEquipmentStats::PhysicalDamageReductionCap displays when no
    // rewrite is in force (audit item 11 — the two were previously separate
    // unflagged 60.0f literals).
    float PhysicalDamageReductionCap = FBreakerEquipmentStats::DefaultPhysicalDamageReductionCap;
    // Cadence: Fire Rate crosses into the damage bucket at this fraction.
    float FireRateToIncreasedDamage = 0.0f;
    // Deadfall's bill, as an ordinary negative Increased percentage rather than
    // a sub-1.0 More. Affixes and items must never author a More (there is a
    // test), and a downside is not an exemption from the locked rule.
    float AirControlPercentDelta = 0.0f;
    // Overrun: regen multiplier while a fast-traversal condition holds, and
    // while none does. Two numbers rather than one flag because the rule is a
    // trade and both halves have to be authored.
    bool bRegenGatedOnTraversal = false;
    float TraversalRegenMultiplier = 1.0f;

    bool IsIdentity() const;
};

UCLASS()
class RIORSEDGE_API UBreakerItemRuleLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // ---- Rules -----------------------------------------------------------
    static const TArray<FBreakerItemRuleDefinition>& GetRuleDefinitions();
    UFUNCTION(BlueprintPure, Category="Items|Rules")
    static FBreakerItemRuleDefinition FindRule(EBreakerItemRule Rule);
    // The rules an ordinary Anomalous drop may roll, in a stable order so a
    // seed reproduces the same rewrite forever.
    static const TArray<EBreakerItemRule>& GetRollableRules();
    // Deterministic pick for a drop.
    UFUNCTION(BlueprintPure, Category="Items|Rules")
    static EBreakerItemRule RollRule(int32 RandomSeed);

    // ---- Legendaries -----------------------------------------------------
    static const TArray<FBreakerLegendaryDefinition>& GetLegendaries();
    UFUNCTION(BlueprintPure, Category="Items|Legendary")
    static FBreakerLegendaryDefinition FindLegendary(FName LegendaryId);
    // The legendary that occupies this slot, or an invalid definition. One per
    // slot at most, which is what keeps "you will hold exactly one legendary"
    // (the Anomalous equip cap) from being a coin flip about which.
    UFUNCTION(BlueprintPure, Category="Items|Legendary")
    static FBreakerLegendaryDefinition FindLegendaryForSlot(EBreakerEquipSlot Slot);

    // ---- Composition -----------------------------------------------------
    // Folds every equipped item's rule into one resolved set. Order-independent:
    // the caps take the loosest value and the flags OR together, so which slot
    // carries the rule cannot change the answer.
    static FBreakerItemRuleSet ResolveRules(const TArray<FBreakerItemInstance>& Items);

    // Cadence's slot rule, asked as a question so the equipment component and
    // its preview cannot disagree about the answer. True when these two items
    // cannot be worn together.
    static bool RulesConflict(const FBreakerItemInstance& A, const FBreakerItemInstance& B);

    // How many tiers better this item's affixes resolve (Prolific). Kept as a
    // count rather than a bool so a second uplift rule needs no new plumbing.
    static int32 TierUpliftForItem(const FBreakerItemInstance& Item);
};
