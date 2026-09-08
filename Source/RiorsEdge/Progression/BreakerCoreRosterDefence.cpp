#include "Progression/BreakerCoreRoster.h"
#include "Progression/BreakerCoreTree.h"
#include "Progression/BreakerProgressionNode.h"

void BreakerCoreRoster::AppendDefence(UObject* Outer, TArray<FBreakerCoreWedgeDefinition>& Wedges)
{
    {
        auto* Footing = Node(Outer, TEXT("Core.Aegis.Footing"), TEXT("Footing"),
            TEXT("+60 Health."), {Effect(EBreakerNodeStatTarget::Health, EBreakerNodeStatBucket::Flat, 60.0f)}, {}); // O2 PLACEHOLDER
        auto* IronFrame = Node(Outer, TEXT("Core.Aegis.IronFrame"), TEXT("Iron Frame"),
            TEXT("+50 Health per rank."), {Effect(EBreakerNodeStatTarget::Health, EBreakerNodeStatBucket::Flat, 50.0f)}, {}); // O2 PLACEHOLDER
        auto* Brace = Node(Outer, TEXT("Core.Aegis.Brace"), TEXT("Brace"),
            TEXT("+20% Increased Armour."), {Effect(EBreakerNodeStatTarget::Armor, EBreakerNodeStatBucket::IncreasedPercent, 20.0f)}, {}); // O2 PLACEHOLDER
        auto* Plate = Node(Outer, TEXT("Core.Aegis.Plate"), TEXT("Plate"),
            TEXT("+12% Increased Armour per rank."), {Effect(EBreakerNodeStatTarget::Armor, EBreakerNodeStatBucket::IncreasedPercent, 12.0f)}, {}); // O2 PLACEHOLDER
        auto* Bulk = Node(Outer, TEXT("Core.Aegis.Bulk"), TEXT("Bulk"),
            TEXT("+8% of maximum Health as a front shield pool."), {Effect(EBreakerNodeStatTarget::FrontShieldPercentMaxHealth, EBreakerNodeStatBucket::Flat, 8.0f)}, {}); // O2 PLACEHOLDER
        auto* SecondSkin = Node(Outer, TEXT("Core.Aegis.SecondSkin"), TEXT("Second Skin"),
            TEXT("+6% physical damage reduction per rank."), {Effect(EBreakerNodeStatTarget::PhysicalDamageReduction, EBreakerNodeStatBucket::Flat, 6.0f)}, {}); // O2 PLACEHOLDER
        auto* AnsweringFire = Node(Outer, TEXT("Core.Aegis.AnsweringFire"), TEXT("Answering Fire"),
            TEXT("+18% Increased Armour and +80 Health."), {Effect(EBreakerNodeStatTarget::Armor, EBreakerNodeStatBucket::IncreasedPercent, 18.0f), Effect(EBreakerNodeStatTarget::Health, EBreakerNodeStatBucket::Flat, 80.0f)}, {}); // O2 PLACEHOLDER
        auto* CleanHands = Node(Outer, TEXT("Core.Aegis.CleanHands"), TEXT("Clean Hands"),
            TEXT("+40 Health."), {Effect(EBreakerNodeStatTarget::Health, EBreakerNodeStatBucket::Flat, 40.0f)}, {}); // O2 PLACEHOLDER
        auto* SetStance = Node(Outer, TEXT("Core.Aegis.SetStance"), TEXT("Set Stance"),
            TEXT("+10% Increased Armour."), {Effect(EBreakerNodeStatTarget::Armor, EBreakerNodeStatBucket::IncreasedPercent, 10.0f)}, {}); // O2 PLACEHOLDER
        auto* Bastion = Node(Outer, TEXT("Core.Aegis.Bastion"), TEXT("Bastion"),
            TEXT("1.20x More effective health."), {Effect(EBreakerNodeStatTarget::EffectiveHealth, EBreakerNodeStatBucket::MorePercent, 20.0f)}, {}); // O2 PLACEHOLDER
        auto* Immovable = Node(Outer, TEXT("Core.Aegis.Immovable"), TEXT("Immovable"),
            TEXT("You cannot be staggered or displaced. FORFEIT: you cannot exceed walking pace."), {}, {TEXT("Progression.Node.Core.Immovable")});
        Wedges.Add({TEXT("Aegis"), TEXT("Defence"), true, Footing,
            {{IronFrame, Brace}, {Plate, Bulk}, {SecondSkin, AnsweringFire}},
            {CleanHands, SetStance}, Bastion, Immovable});
    }
    {
        auto* Read = Node(Outer, TEXT("Core.Bulwark.Read"), TEXT("Read"),
            TEXT("+5% Block Chance."), {Effect(EBreakerNodeStatTarget::BlockChance, EBreakerNodeStatBucket::Flat, 5.0f)}, {}); // O2 PLACEHOLDER
        auto* Guard = Node(Outer, TEXT("Core.Bulwark.Guard"), TEXT("Guard"),
            TEXT("+3% Block Chance per rank."), {Effect(EBreakerNodeStatTarget::BlockChance, EBreakerNodeStatBucket::Flat, 3.0f)}, {}); // O2 PLACEHOLDER
        auto* Parry = Node(Outer, TEXT("Core.Bulwark.Parry"), TEXT("Parry"),
            TEXT("Grants Parry: 0.25s window, 2s cooldown."), {}, {TEXT("Progression.Verb.Parry")});
        auto* Evade = Node(Outer, TEXT("Core.Bulwark.Evade"), TEXT("Evade"),
            TEXT("+3% Dodge Chance per rank."), {Effect(EBreakerNodeStatTarget::DodgeChance, EBreakerNodeStatBucket::Flat, 3.0f)}, {}); // O2 PLACEHOLDER
        auto* Counterweight = Node(Outer, TEXT("Core.Bulwark.Counterweight"), TEXT("Counterweight"),
            TEXT("Parry cooldown -0.5s and window +0.10s."), {Effect(EBreakerNodeStatTarget::ParryCooldownReductionSeconds, EBreakerNodeStatBucket::Flat, 0.5f), Effect(EBreakerNodeStatTarget::ParryWindowAddedSeconds, EBreakerNodeStatBucket::Flat, 0.1f)}, {}); // O2 PLACEHOLDER
        auto* Interpose = Node(Outer, TEXT("Core.Bulwark.Interpose"), TEXT("Interpose"),
            TEXT("+6% of maximum Health as a front shield pool per rank."), {Effect(EBreakerNodeStatTarget::FrontShieldPercentMaxHealth, EBreakerNodeStatBucket::Flat, 6.0f)}, {}); // O2 PLACEHOLDER
        auto* Riposte = Node(Outer, TEXT("Core.Bulwark.Riposte"), TEXT("Riposte"),
            TEXT("A successful parry restores 8% of maximum Health."), {}, {TEXT("Progression.Node.Core.Bulwark.Riposte")});
        auto* Anticipate = Node(Outer, TEXT("Core.Bulwark.Anticipate"), TEXT("Anticipate"),
            TEXT("+8% parry cooldown recovery."), {Effect(EBreakerNodeStatTarget::ParryCooldownRecovery, EBreakerNodeStatBucket::IncreasedPercent, 8.0f)}, {}); // O2 PLACEHOLDER
        auto* Footwork = Node(Outer, TEXT("Core.Bulwark.Footwork"), TEXT("Footwork"),
            TEXT("+3% Dodge Chance."), {Effect(EBreakerNodeStatTarget::DodgeChance, EBreakerNodeStatBucket::Flat, 3.0f)}, {}); // O2 PLACEHOLDER
        auto* Wall = Node(Outer, TEXT("Core.Bulwark.Wall"), TEXT("Wall"),
            TEXT("Your front shield pool rebuilds fully on parry."), {}, {TEXT("Progression.Node.Core.Bulwark.Wall")});
        auto* PerfectGuard = Node(Outer, TEXT("Core.Bulwark.PerfectGuard"), TEXT("Perfect Guard"),
            TEXT("Parry negates all damage for 1s instead of one hit. FORFEIT: Block and Dodge Chance are set to zero."), {}, {TEXT("Progression.Node.Core.Bulwark.PerfectGuard")});
        Wedges.Add({TEXT("Bulwark"), TEXT("Defence"), true, Read,
            {{Guard, Parry}, {Evade, Counterweight}, {Interpose, Riposte}},
            {Anticipate, Footwork}, Wall, PerfectGuard});
    }
    {
        auto* Frame = Node(Outer, TEXT("Core.Constitution.Frame"), TEXT("Frame"),
            TEXT("+80 Health."), {Effect(EBreakerNodeStatTarget::Health, EBreakerNodeStatBucket::Flat, 80.0f)}, {}); // O2 PLACEHOLDER
        auto* Mass = Node(Outer, TEXT("Core.Constitution.Mass"), TEXT("Mass"),
            TEXT("+60 Health per rank."), {Effect(EBreakerNodeStatTarget::Health, EBreakerNodeStatBucket::Flat, 60.0f)}, {}); // O2 PLACEHOLDER
        auto* DeepReserve = Node(Outer, TEXT("Core.Constitution.DeepReserve"), TEXT("Deep Reserve"),
            TEXT("+12% maximum Health."), {Effect(EBreakerNodeStatTarget::Health, EBreakerNodeStatBucket::IncreasedPercent, 12.0f)}, {}); // O2 PLACEHOLDER
        auto* Layered = Node(Outer, TEXT("Core.Constitution.Layered"), TEXT("Layered"),
            TEXT("+4% of maximum Health as shield per rank."), {Effect(EBreakerNodeStatTarget::ShieldPercentMaxHealth, EBreakerNodeStatBucket::Flat, 4.0f)}, {}); // O2 PLACEHOLDER
        auto* ThirdLayer = Node(Outer, TEXT("Core.Constitution.ThirdLayer"), TEXT("Third Layer"),
            TEXT("Your front pool also covers the rear arc at half strength."), {}, {TEXT("Progression.Node.Core.Constitution.ThirdLayer")});
        auto* Endurance = Node(Outer, TEXT("Core.Constitution.Endurance"), TEXT("Endurance"),
            TEXT("Health, shield and front pools count toward your current and maximum health view."), {}, {TEXT("Progression.Node.Core.Endurance")});
        Wedges.Add({TEXT("Constitution"), TEXT("Defence"), false, Frame,
            {{Mass, DeepReserve}, {Layered, ThirdLayer}},
            {}, Endurance, nullptr});
    }
    {
        auto* Resist = Node(Outer, TEXT("Core.Ward.Resist"), TEXT("Resist"),
            TEXT("+10% all elemental resistance."), {Effect(EBreakerNodeStatTarget::ElementalResistance, EBreakerNodeStatBucket::Flat, 10.0f)}, {}); // O2 PLACEHOLDER
        auto* Tolerance = Node(Outer, TEXT("Core.Ward.Tolerance"), TEXT("Tolerance"),
            TEXT("+6% elemental resistance per rank."), {Effect(EBreakerNodeStatTarget::ElementalResistance, EBreakerNodeStatBucket::Flat, 6.0f)}, {}); // O2 PLACEHOLDER
        auto* Refusal = Node(Outer, TEXT("Core.Ward.Refusal"), TEXT("Refusal"),
            TEXT("+12% ailment avoidance."), {Effect(EBreakerNodeStatTarget::AilmentAvoidance, EBreakerNodeStatBucket::Flat, 12.0f)}, {}); // O2 PLACEHOLDER
        auto* Clean = Node(Outer, TEXT("Core.Ward.Clean"), TEXT("Clean"),
            TEXT("+5% ailment avoidance per rank."), {Effect(EBreakerNodeStatTarget::AilmentAvoidance, EBreakerNodeStatBucket::Flat, 5.0f)}, {}); // O2 PLACEHOLDER
        auto* Insulation = Node(Outer, TEXT("Core.Ward.Insulation"), TEXT("Insulation"),
            TEXT("Elemental buildup on you decays twice as fast."), {}, {TEXT("Progression.Node.Core.Insulation")});
        auto* Null = Node(Outer, TEXT("Core.Ward.Null"), TEXT("Null"),
            TEXT("The first ailment applied to you each fight is refused."), {}, {TEXT("Progression.Node.Core.Null")});
        Wedges.Add({TEXT("Ward"), TEXT("Defence"), false, Resist,
            {{Tolerance, Refusal}, {Clean, Insulation}},
            {}, Null, nullptr});
    }
    {
        auto* Mend = Node(Outer, TEXT("Core.Recovery.Mend"), TEXT("Mend"),
            TEXT("+15% healing received."), {Effect(EBreakerNodeStatTarget::HealingReceived, EBreakerNodeStatBucket::IncreasedPercent, 15.0f)}, {}); // O2 PLACEHOLDER
        auto* Knit = Node(Outer, TEXT("Core.Recovery.Knit"), TEXT("Knit"),
            TEXT("Regenerate 0.4% of maximum Health per second per rank."), {Effect(EBreakerNodeStatTarget::HealthRegenPercentMaxHealth, EBreakerNodeStatBucket::Flat, 0.4f)}, {}); // O2 PLACEHOLDER
        auto* FieldDressing = Node(Outer, TEXT("Core.Recovery.FieldDressing"), TEXT("Field Dressing"),
            TEXT("+30% healing received."), {Effect(EBreakerNodeStatTarget::HealingReceived, EBreakerNodeStatBucket::IncreasedPercent, 30.0f)}, {}); // O2 PLACEHOLDER
        auto* Recharge = Node(Outer, TEXT("Core.Recovery.Recharge"), TEXT("Recharge"),
            TEXT("Shield recharge delay -0.4s per rank."), {Effect(EBreakerNodeStatTarget::ShieldRechargeDelayReduction, EBreakerNodeStatBucket::Flat, 0.4f)}, {}); // O2 PLACEHOLDER
        auto* Overheal = Node(Outer, TEXT("Core.Recovery.Overheal"), TEXT("Overheal"),
            TEXT("Excess healing becomes shield up to 15% of maximum Health."), {}, {TEXT("Progression.Node.Core.Recovery.Overheal")});
        auto* SecondLife = Node(Outer, TEXT("Core.Recovery.SecondLife"), TEXT("Second Life"),
            TEXT("Regeneration continues in combat at half rate."), {}, {TEXT("Progression.Node.Core.SecondLife")});
        Wedges.Add({TEXT("Recovery"), TEXT("Defence"), false, Mend,
            {{Knit, FieldDressing}, {Recharge, Overheal}},
            {}, SecondLife, nullptr});
    }
}

