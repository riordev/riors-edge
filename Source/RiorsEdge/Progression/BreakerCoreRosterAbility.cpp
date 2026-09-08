#include "Progression/BreakerCoreRoster.h"
#include "Progression/BreakerProgressionNode.h"

namespace BreakerCoreRoster
{
void AppendAbility(UObject* Outer, TArray<FBreakerCoreWedgeDefinition>& Wedges)
{
    using T = EBreakerNodeStatTarget;
    using B = EBreakerNodeStatBucket;
    Wedges.Add({TEXT("Arc"), TEXT("Ability"), true,
        Node(Outer, TEXT("Core.Arc.Prime"), TEXT("Prime"), TEXT("8% increased Ability Damage."), {Effect(T::AbilityDamage, B::IncreasedPercent, 8.f)}), // O2 PLACEHOLDER
        {
            {Node(Outer, TEXT("Core.Arc.Channel"), TEXT("Channel"), TEXT("6% increased Ability Damage per rank."), {Effect(T::AbilityDamage, B::IncreasedPercent, 6.f)}), // O2 PLACEHOLDER
             Node(Outer, TEXT("Core.Arc.Widen"), TEXT("Widen"), TEXT("15% increased Ability Area."), {Effect(T::AbilityArea, B::IncreasedPercent, 15.f)})}, // O2 PLACEHOLDER
            {Node(Outer, TEXT("Core.Arc.Vent"), TEXT("Vent"), TEXT("4 Added Ability Power per rank."), {Effect(T::AddedAbilityPower, B::Flat, 4.f)}), // O2 PLACEHOLDER
             Node(Outer, TEXT("Core.Arc.Reach"), TEXT("Reach"), TEXT("25% increased Ability Damage."), {Effect(T::AbilityDamage, B::IncreasedPercent, 25.f)})}, // O2 PLACEHOLDER
            {Node(Outer, TEXT("Core.Arc.Anchor"), TEXT("Anchor"), TEXT("6% increased Ability Area per rank."), {Effect(T::AbilityArea, B::IncreasedPercent, 6.f)}), // O2 PLACEHOLDER
             Node(Outer, TEXT("Core.Arc.Persistence"), TEXT("Persistence"), TEXT("20% increased zone and window duration."), {Effect(T::ZoneAndWindowDuration, B::IncreasedPercent, 20.f)})} // O2 PLACEHOLDER
        },
        {Node(Outer, TEXT("Core.Arc.Recycle"), TEXT("Recycle"), TEXT("3 Added Ability Power."), {Effect(T::AddedAbilityPower, B::Flat, 3.f)}), // O2 PLACEHOLDER
         Node(Outer, TEXT("Core.Arc.Spillover"), TEXT("Spillover"), TEXT("8% increased Ability Area."), {Effect(T::AbilityArea, B::IncreasedPercent, 8.f)})}, // O2 PLACEHOLDER
        Node(Outer, TEXT("Core.Arc.Overflow"), TEXT("Overflow"), TEXT("26% More Ability Damage."), {Effect(T::AbilityDamage, B::MorePercent, 26.f)}), // O2 PLACEHOLDER
        Node(Outer, TEXT("Core.Arc.Detonation"), TEXT("Detonation"), TEXT("Zones no longer tick. They pay their whole remaining damage on expiry. FORFEIT: no sustained pressure."), {}, {TEXT("Progression.Node.Core.Detonation")})});
    Wedges.Add({TEXT("Tempo"), TEXT("Ability"), true,
        Node(Outer, TEXT("Core.Tempo.Metronome"), TEXT("Metronome"), TEXT("6% increased cast rate."), {Effect(T::AbilityCastRate, B::IncreasedPercent, 6.f)}), // O2 PLACEHOLDER
        {
            {Node(Outer, TEXT("Core.Tempo.Quicken"), TEXT("Quicken"), TEXT("4% increased cast rate per rank."), {Effect(T::AbilityCastRate, B::IncreasedPercent, 4.f)}), // O2 PLACEHOLDER
             Node(Outer, TEXT("Core.Tempo.Reset"), TEXT("Reset"), TEXT("15% increased Ability Damage."), {Effect(T::AbilityDamage, B::IncreasedPercent, 15.f)})}, // O2 PLACEHOLDER
            {Node(Outer, TEXT("Core.Tempo.Recovery"), TEXT("Recovery"), TEXT("5% increased cooldown recovery per rank."), {Effect(T::AbilityCooldown, B::IncreasedPercent, 5.f)}), // O2 PLACEHOLDER
             Node(Outer, TEXT("Core.Tempo.SecondWind"), TEXT("Second Wind"), TEXT("Abilities cost 12% less."), {Effect(T::AbilityCost, B::IncreasedPercent, 12.f)})}, // O2 PLACEHOLDER
            {Node(Outer, TEXT("Core.Tempo.Flow"), TEXT("Flow"), TEXT("4% increased channel rate per rank."), {Effect(T::AbilityChannelRate, B::IncreasedPercent, 4.f)}), // O2 PLACEHOLDER
             Node(Outer, TEXT("Core.Tempo.Cascade"), TEXT("Cascade"), TEXT("12% increased cooldown recovery and cast rate."), {Effect(T::AbilityCooldown, B::IncreasedPercent, 12.f), Effect(T::AbilityCastRate, B::IncreasedPercent, 12.f)})} // O2 PLACEHOLDER
        },
        {Node(Outer, TEXT("Core.Tempo.Prep"), TEXT("Prep"), TEXT("Abilities cost 8% less."), {Effect(T::AbilityCost, B::IncreasedPercent, 8.f)}), // O2 PLACEHOLDER
         Node(Outer, TEXT("Core.Tempo.FollowThrough"), TEXT("Follow Through"), TEXT("5% increased cast rate."), {Effect(T::AbilityCastRate, B::IncreasedPercent, 5.f)})}, // O2 PLACEHOLDER
        Node(Outer, TEXT("Core.Tempo.Overclock"), TEXT("Overclock"), TEXT("Cooldown recovery also applies to cast and channel rate at half value."), {}, {TEXT("Progression.Node.Core.Overclock")}),
        Node(Outer, TEXT("Core.Tempo.Conduction"), TEXT("Conduction"), TEXT("Abilities have no cooldown. FORFEIT: each cast raises your next cast's cost by 40%, decaying over 5s."), {}, {TEXT("Progression.Node.Core.Conduction")})});
    Wedges.Add({TEXT("Reservoir"), TEXT("Ability"), false,
        Node(Outer, TEXT("Core.Reservoir.Capacity"), TEXT("Capacity"), TEXT("10% increased maximum class resource."), {Effect(T::MaxClassResource, B::IncreasedPercent, 10.f)}), // O2 PLACEHOLDER
        {
            {Node(Outer, TEXT("Core.Reservoir.Draw"), TEXT("Draw"), TEXT("5% increased resource generation per rank."), {Effect(T::ClassResourceGeneration, B::IncreasedPercent, 5.f)}), // O2 PLACEHOLDER
             Node(Outer, TEXT("Core.Reservoir.DeepPockets"), TEXT("Deep Pockets"), TEXT("20% increased maximum class resource."), {Effect(T::MaxClassResource, B::IncreasedPercent, 20.f)})}, // O2 PLACEHOLDER
            {Node(Outer, TEXT("Core.Reservoir.Tithe"), TEXT("Tithe"), TEXT("Abilities cost 5% less per rank."), {Effect(T::AbilityCost, B::IncreasedPercent, 5.f)}), // O2 PLACEHOLDER
             Node(Outer, TEXT("Core.Reservoir.Wellspring"), TEXT("Wellspring"), TEXT("15% increased resource generation and 1 flat regeneration per second."), {Effect(T::ClassResourceGeneration, B::IncreasedPercent, 15.f), Effect(T::ClassResourceRegen, B::Flat, 1.f)})} // O2 PLACEHOLDER
        }, {},
        Node(Outer, TEXT("Core.Reservoir.SecondShift"), TEXT("Second Shift"), TEXT("Your resource does not decay outside combat."), {}, {TEXT("Progression.Node.Core.SecondShift")}), nullptr});
    Wedges.Add({TEXT("Duration"), TEXT("Ability"), false,
        Node(Outer, TEXT("Core.Duration.Hold"), TEXT("Hold"), TEXT("10% increased ability duration."), {Effect(T::AbilityDuration, B::IncreasedPercent, 10.f)}), // O2 PLACEHOLDER
        {
            {Node(Outer, TEXT("Core.Duration.Extend"), TEXT("Extend"), TEXT("6% increased ability and zone duration per rank."), {Effect(T::AbilityDuration, B::IncreasedPercent, 6.f)}), // O2 PLACEHOLDER
             Node(Outer, TEXT("Core.Duration.Uptime"), TEXT("Uptime"), TEXT("15% increased buff and window duration."), {Effect(T::BuffAndWindowDuration, B::IncreasedPercent, 15.f)})}, // O2 PLACEHOLDER
            {Node(Outer, TEXT("Core.Duration.Settle"), TEXT("Settle"), TEXT("5% increased Ability Damage per rank."), {Effect(T::AbilityDamage, B::IncreasedPercent, 5.f)}), // O2 PLACEHOLDER
             Node(Outer, TEXT("Core.Duration.Standing"), TEXT("Standing"), TEXT("Zones deal 10% increased damage after 3s active."), {}, {TEXT("Progression.Node.Core.Standing")})}
        }, {},
        // O228: existing Afterimage consumers are partial; parked Support enhancement tails remain pending.
        Node(Outer, TEXT("Core.Duration.Afterimage"), TEXT("Afterimage"), TEXT("Windows leave a 2s tail at half their lane contributions. Non-lane effects end normally."), {}, {TEXT("Progression.Node.Core.Afterimage")}), nullptr});
}
}
