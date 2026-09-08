#include "Progression/BreakerCoreRoster.h"
#include "Progression/BreakerCoreTree.h"
#include "NativeGameplayTags.h"

namespace BreakerCoreStatusTags
{
    UE_DEFINE_GAMEPLAY_TAG(Node_SecondOrder, "Progression.Node.Core.Reaction.SecondOrder");
    UE_DEFINE_GAMEPLAY_TAG(Node_Overlap, "Progression.Node.Core.Reaction.Overlap");
    UE_DEFINE_GAMEPLAY_TAG(Node_Sympathetic, "Progression.Node.Core.Reaction.Sympathetic");
}

void BreakerCoreRoster::AppendStatus(UObject* Outer, TArray<FBreakerCoreWedgeDefinition>& Wedges)
{
    using Target = EBreakerNodeStatTarget;
    using Bucket = EBreakerNodeStatBucket;

    {
        const auto* OpenWound = Node(Outer, TEXT("Core.Affliction.OpenWound"), TEXT("Open Wound"),
            TEXT("+10% Increased damage over time."),
            {Effect(Target::DamageOverTime, Bucket::IncreasedPercent, 10.0f)}); // O2 PLACEHOLDER
        const auto* Bloodlet = Node(Outer, TEXT("Core.Affliction.Bloodlet"), TEXT("Bloodlet"),
            TEXT("+7% Increased damage over time per rank."),
            {Effect(Target::DamageOverTime, Bucket::IncreasedPercent, 7.0f)}); // O2 PLACEHOLDER
        const auto* SlowBleed = Node(Outer, TEXT("Core.Affliction.SlowBleed"), TEXT("Slow Bleed"),
            TEXT("+15% status chance."),
            {Effect(Target::StatusChance, Bucket::IncreasedPercent, 15.0f)}); // O2 PLACEHOLDER
        const auto* Seep = Node(Outer, TEXT("Core.Affliction.Seep"), TEXT("Seep"),
            TEXT("+5% status chance per rank."),
            {Effect(Target::StatusChance, Bucket::IncreasedPercent, 5.0f)}); // O2 PLACEHOLDER
        const auto* Fester = Node(Outer, TEXT("Core.Affliction.Fester"), TEXT("Fester"),
            TEXT("+25% Increased damage over time."),
            {Effect(Target::DamageOverTime, Bucket::IncreasedPercent, 25.0f)}); // O2 PLACEHOLDER
        const auto* Linger = Node(Outer, TEXT("Core.Affliction.Linger"), TEXT("Linger"),
            TEXT("+8% status duration per rank."),
            {Effect(Target::StatusDuration, Bucket::IncreasedPercent, 8.0f)}); // O2 PLACEHOLDER
        const auto* Deepen = Node(Outer, TEXT("Core.Affliction.Deepen"), TEXT("Deepen"),
            TEXT("Ailments stack one higher."), {}, {TEXT("Progression.Node.Core.Deepen")});
        const auto* Attrition = Node(Outer, TEXT("Core.Affliction.Attrition"), TEXT("Attrition"),
            TEXT("+10% status duration."),
            {Effect(Target::StatusDuration, Bucket::IncreasedPercent, 10.0f)}); // O2 PLACEHOLDER
        const auto* Bloodwork = Node(Outer, TEXT("Core.Affliction.Bloodwork"), TEXT("Bloodwork"),
            TEXT("+6% status chance."),
            {Effect(Target::StatusChance, Bucket::IncreasedPercent, 6.0f)}); // O2 PLACEHOLDER
        const auto* Compound = Node(Outer, TEXT("Core.Affliction.Compound"), TEXT("Compound"),
            TEXT("1.24x More damage over time."),
            {Effect(Target::DamageOverTime, Bucket::MorePercent, 24.0f)}); // O2 PLACEHOLDER
        const auto* Hemorrhage = Node(Outer, TEXT("Core.Affliction.Hemorrhage"), TEXT("Hemorrhage"),
            TEXT("Ailments tick twice as fast for half as long. FORFEIT: active ailments cannot be refreshed."), {}, {TEXT("Progression.Node.Core.Hemorrhage")});
        Wedges.Add({TEXT("Affliction"), TEXT("Status"), true, OpenWound,
            {{Bloodlet, SlowBleed}, {Seep, Fester}, {Linger, Deepen}}, {Attrition, Bloodwork}, Compound, Hemorrhage});
    }

    {
        const auto* Attunement = Node(Outer, TEXT("Core.Entropy.Attunement"), TEXT("Attunement"),
            TEXT("+10% Entropy buildup."),
            {Effect(Target::EntropyBuildup, Bucket::IncreasedPercent, 10.0f)}); // O2 PLACEHOLDER
        const auto* Catalyst = Node(Outer, TEXT("Core.Entropy.Catalyst"), TEXT("Catalyst"),
            TEXT("+6% elemental buildup per rank."),
            {Effect(Target::ElementalBuildup, Bucket::IncreasedPercent, 6.0f)}); // O2 PLACEHOLDER
        const auto* Threshold = Node(Outer, TEXT("Core.Entropy.Threshold"), TEXT("Threshold"),
            TEXT("Elemental thresholds are reached 15% sooner."),
            {Effect(Target::ElementalThresholdReduction, Bucket::Flat, 15.0f)}); // O2 PLACEHOLDER
        const auto* Decay = Node(Outer, TEXT("Core.Entropy.Decay"), TEXT("Decay"),
            TEXT("+7% Rot damage per rank."),
            {Effect(Target::RotDamage, Bucket::IncreasedPercent, 7.0f)}); // O2 PLACEHOLDER
        const auto* Density = Node(Outer, TEXT("Core.Entropy.Density"), TEXT("Density"),
            TEXT("Rot ticks two additional times over the same duration."), {}, {TEXT("Progression.Node.Core.Density")});
        const auto* Conductive = Node(Outer, TEXT("Core.Entropy.Conductive"), TEXT("Conductive"),
            TEXT("+6% Increased elemental damage per rank."),
            {Effect(Target::ElementalDamage, Bucket::IncreasedPercent, 6.0f)}); // O2 PLACEHOLDER
        const auto* Sympathy = Node(Outer, TEXT("Core.Entropy.Sympathy"), TEXT("Sympathy"),
            TEXT("On natural expiry, Rot spreads to one enemy within 4m."), {}, {TEXT("Progression.Node.Core.Sympathy")});
        const auto* Penetrance = Node(Outer, TEXT("Core.Entropy.Penetrance"), TEXT("Penetrance"),
            TEXT("-10% enemy buildup resistance."),
            {Effect(Target::ElementalBuildupPenetration, Bucket::Flat, 10.0f)}); // O2 PLACEHOLDER
        const auto* Sequence = Node(Outer, TEXT("Core.Entropy.Sequence"), TEXT("Sequence"),
            TEXT("+12% status duration."),
            {Effect(Target::StatusDuration, Bucket::IncreasedPercent, 12.0f)}); // O2 PLACEHOLDER
        const auto* Cascade = Node(Outer, TEXT("Core.Entropy.Cascade"), TEXT("Cascade"),
            TEXT("1.22x More elemental damage."),
            {Effect(Target::ElementalDamage, Bucket::MorePercent, 22.0f)}); // O2 PLACEHOLDER
        const auto* LongDark = Node(Outer, TEXT("Core.Entropy.LongDark"), TEXT("Long Dark"),
            TEXT("Your Rot never expires on its own. FORFEIT: you can maintain exactly one."), {}, {TEXT("Progression.Node.Core.LongDark")});
        Wedges.Add({TEXT("Entropy"), TEXT("Status"), true, Attunement,
            {{Catalyst, Threshold}, {Decay, Density}, {Conductive, Sympathy}}, {Penetrance, Sequence}, Cascade, LongDark});
    }

    {
        const auto* Catalysis = Node(Outer, TEXT("Core.Reaction.Catalysis"), TEXT("Catalysis"),
            TEXT("+12% Increased reaction damage."),
            {Effect(Target::ReactionDamage, Bucket::IncreasedPercent, 12.0f)}); // O2 PLACEHOLDER
        const auto* Ignition = Node(Outer, TEXT("Core.Reaction.Ignition"), TEXT("Ignition"),
            TEXT("+8% Increased reaction damage per rank."),
            {Effect(Target::ReactionDamage, Bucket::IncreasedPercent, 8.0f)}); // O2 PLACEHOLDER
        const auto* Chain = Node(Outer, TEXT("Core.Reaction.Chain"), TEXT("Chain"),
            TEXT("Reactions also hit one enemy within 5m for half their damage."), {}, {TEXT("Progression.Node.Core.Reaction.Chain")});
        const auto* Residue = Node(Outer, TEXT("Core.Reaction.Residue"), TEXT("Residue"),
            TEXT("Consumed statuses retain 10% of their remaining damage per rank."),
            {Effect(Target::ReactionResiduePercent, Bucket::Flat, 10.0f)}); // O2 PLACEHOLDER
        // MISSING CONSUMER: No consumer yet: the elemental hit transaction still excludes statuses created by that hit. No substitute stat effect.
        const auto* SecondOrder = Node(Outer, TEXT("Core.Reaction.SecondOrder"), TEXT("Second Order"),
            TEXT("NOT IMPLEMENTED: A reaction may consume a status it created this hit. Purchasing this node currently grants no effect."), {}, {TEXT("Progression.Node.Core.Reaction.SecondOrder")});
        const auto* Feedback = Node(Outer, TEXT("Core.Reaction.Feedback"), TEXT("Feedback"),
            TEXT("+5% elemental buildup per rank."),
            {Effect(Target::ElementalBuildup, Bucket::IncreasedPercent, 5.0f)}); // O2 PLACEHOLDER
        // MISSING CONSUMER: No consumer yet: reaction preparation does not fund or queue third-element buildup. No substitute stat effect.
        const auto* Overlap = Node(Outer, TEXT("Core.Reaction.Overlap"), TEXT("Overlap"),
            TEXT("NOT IMPLEMENTED: Reactions apply fresh buildup of the third element. Purchasing this node currently grants no effect."), {}, {TEXT("Progression.Node.Core.Reaction.Overlap")});
        const auto* Spark = Node(Outer, TEXT("Core.Reaction.Spark"), TEXT("Spark"),
            TEXT("+8% Increased reaction damage."),
            {Effect(Target::ReactionDamage, Bucket::IncreasedPercent, 8.0f)}); // O2 PLACEHOLDER
        const auto* Echo = Node(Outer, TEXT("Core.Reaction.Echo"), TEXT("Echo"),
            TEXT("+6% status duration."),
            {Effect(Target::StatusDuration, Bucket::IncreasedPercent, 6.0f)}); // O2 PLACEHOLDER
        const auto* Resonance = Node(Outer, TEXT("Core.Reaction.Resonance"), TEXT("Resonance"),
            TEXT("1.20x More reaction damage."),
            {Effect(Target::ReactionDamage, Bucket::MorePercent, 20.0f)}); // O2 PLACEHOLDER
        // MISSING CONSUMER: No consumer yet: multi-pair hit preparation and per-source/enemy deferred applications are absent. No substitute stat effect.
        const auto* Sympathetic = Node(Outer, TEXT("Core.Reaction.Sympathetic"), TEXT("Sympathetic"),
            TEXT("NOT IMPLEMENTED: Every reaction triggers on all eligible statuses instead of the first pair. FORFEIT: each affected enemy defers your elemental applications for 3s while buildup continues. Allies and physical ailments are exempt. Purchasing this node currently grants no effect."), {}, {TEXT("Progression.Node.Core.Reaction.Sympathetic")}); // O2 PLACEHOLDER
        Wedges.Add({TEXT("Reaction"), TEXT("Status"), true, Catalysis,
            {{Ignition, Chain}, {Residue, SecondOrder}, {Feedback, Overlap}}, {Spark, Echo}, Resonance, Sympathetic});
    }

    {
        const auto* Displace = Node(Outer, TEXT("Core.Rift.Displace"), TEXT("Displace"),
            TEXT("+12% Rift buildup."),
            {Effect(Target::RiftBuildup, Bucket::IncreasedPercent, 12.0f)}); // O2 PLACEHOLDER
        const auto* Shove = Node(Outer, TEXT("Core.Rift.Shove"), TEXT("Shove"),
            TEXT("+8% Rift burst damage per rank."),
            {Effect(Target::RiftBurstDamage, Bucket::IncreasedPercent, 8.0f)}); // O2 PLACEHOLDER
        const auto* Impact = Node(Outer, TEXT("Core.Rift.Impact"), TEXT("Impact"),
            TEXT("Displacement also hits one enemy in its path."), {}, {TEXT("Progression.Node.Core.Impact")});
        const auto* Instability = Node(Outer, TEXT("Core.Rift.Instability"), TEXT("Instability"),
            TEXT("+6% elemental buildup per rank."),
            {Effect(Target::ElementalBuildup, Bucket::IncreasedPercent, 6.0f)}); // O2 PLACEHOLDER
        const auto* HardLanding = Node(Outer, TEXT("Core.Rift.HardLanding"), TEXT("Hard Landing"),
            TEXT("Enemies shoved into a wall take an additional 25% of the burst."), {}, {TEXT("Progression.Node.Core.HardLanding")});
        const auto* VectorField = Node(Outer, TEXT("Core.Rift.VectorField"), TEXT("Vector Field"),
            TEXT("Unstable displaces enemies toward you instead of away."), {}, {TEXT("Progression.Node.Core.VectorField")});
        Wedges.Add({TEXT("Rift"), TEXT("Status"), false, Displace,
            {{Shove, Impact}, {Instability, HardLanding}}, {}, VectorField, nullptr});
    }

    {
        const auto* Erase = Node(Outer, TEXT("Core.Void.Erase"), TEXT("Erase"),
            TEXT("+12% Void buildup."),
            {Effect(Target::VoidBuildup, Bucket::IncreasedPercent, 12.0f)}); // O2 PLACEHOLDER
        const auto* Hollow = Node(Outer, TEXT("Core.Void.Hollow"), TEXT("Hollow"),
            TEXT("+8% Erased burst damage per rank."),
            {Effect(Target::VoidBurstDamage, Bucket::IncreasedPercent, 8.0f)}); // O2 PLACEHOLDER
        const auto* Patience = Node(Outer, TEXT("Core.Void.Patience"), TEXT("Patience"),
            TEXT("Erased pays 40% more, one second later."), {}, {TEXT("Progression.Node.Core.Patience")});
        const auto* Siphon = Node(Outer, TEXT("Core.Void.Siphon"), TEXT("Siphon"),
            TEXT("+6% Increased elemental damage per rank."),
            {Effect(Target::ElementalDamage, Bucket::IncreasedPercent, 6.0f)}); // O2 PLACEHOLDER
        const auto* Debt = Node(Outer, TEXT("Core.Void.Debt"), TEXT("Debt"),
            TEXT("Erased also pays 15% of damage dealt during its delay."), {}, {TEXT("Progression.Node.Core.Debt")});
        const auto* NothingLeft = Node(Outer, TEXT("Core.Void.NothingLeft"), TEXT("Nothing Left"),
            TEXT("1.18x More Void damage."),
            {Effect(Target::VoidDamage, Bucket::MorePercent, 18.0f)}); // O2 PLACEHOLDER
        Wedges.Add({TEXT("Void"), TEXT("Status"), false, Erase,
            {{Hollow, Patience}, {Siphon, Debt}}, {}, NothingLeft, nullptr});
    }
}
