#include "Progression/BreakerCoreRoster.h"
#include "Progression/BreakerProgressionNode.h"

namespace BreakerCoreRoster
{
void AppendUtility(UObject* Outer, TArray<FBreakerCoreWedgeDefinition>& Wedges)
{
    using T = EBreakerNodeStatTarget; using B = EBreakerNodeStatBucket;
    Wedges.Add({TEXT("Control"), TEXT("Utility"), false,
        Node(Outer, TEXT("Core.Control.Concussive"), TEXT("Concussive"), TEXT("15% increased stagger duration."), {Effect(T::StaggerDuration, B::IncreasedPercent, 15.f)}), // O2 PLACEHOLDER
        {
            {Node(Outer, TEXT("Core.Control.Jarring"), TEXT("Jarring"), TEXT("8% increased stagger duration per rank."), {Effect(T::StaggerDuration, B::IncreasedPercent, 8.f)}), // O2 PLACEHOLDER
             Node(Outer, TEXT("Core.Control.Shockwave"), TEXT("Shockwave"), TEXT("Your stagger also staggers one enemy within 3m."), {}, {TEXT("Progression.Node.Core.Control.Shockwave")})}, // O2 PLACEHOLDER
            {Node(Outer, TEXT("Core.Control.Insistent"), TEXT("Insistent"), TEXT("Reduce enemy stagger resistance by 6 percentage points per rank."), {Effect(T::EnemyStaggerResistanceReduction, B::IncreasedPercent, 6.f)}), // O2 PLACEHOLDER
             Node(Outer, TEXT("Core.Control.Interrupt"), TEXT("Interrupt"), TEXT("Staggered enemies take 15% increased damage."), {}, {TEXT("Progression.Node.Core.Control.Interrupt")})} // O2 PLACEHOLDER
        }, {},
        Node(Outer, TEXT("Core.Control.Lockstep"), TEXT("Lockstep"), TEXT("Your stagger cannot be refused by immunity, but lasts half as long against immune enemies."), {}, {TEXT("Progression.Node.Core.Control.Lockstep")}), nullptr});
    Wedges.Add({TEXT("Threat"), TEXT("Utility"), false,
        Node(Outer, TEXT("Core.Threat.Presence"), TEXT("Presence"), TEXT("20% increased threat generated."), {Effect(T::ThreatGenerated, B::IncreasedPercent, 20.f)}), // O2 PLACEHOLDER
        {
            {Node(Outer, TEXT("Core.Threat.Loud"), TEXT("Loud"), TEXT("10% increased threat generated per rank."), {Effect(T::ThreatGenerated, B::IncreasedPercent, 10.f)}), // O2 PLACEHOLDER
             Node(Outer, TEXT("Core.Threat.Provocation"), TEXT("Provocation"), TEXT("Enemies attacking you have 10% reduced accuracy."), {}, {TEXT("Progression.Node.Core.Provocation")})}, // O2 PLACEHOLDER
            {Node(Outer, TEXT("Core.Threat.Decoy"), TEXT("Decoy"), TEXT("15% increased deployable health per rank."), {Effect(T::DeployableHealth, B::IncreasedPercent, 15.f)}), // O2 PLACEHOLDER
             Node(Outer, TEXT("Core.Threat.Bait"), TEXT("Bait"), TEXT("Enemies attacking your deployables take 12% increased damage."), {}, {TEXT("Progression.Node.Core.Threat.Bait")})} // O2 PLACEHOLDER
        }, {},
        // Consumer gap: self-zero and owned-deployable doubling exist. Ally doubling
        // requires an authoritative party relation; a spawned character is not proof.
        Node(Outer, TEXT("Core.Threat.IgnoreMe"), TEXT("Ignore Me"), TEXT("You generate no threat. Allies and deployables generate double. If no non-zero-threat entity exists, targeting uses its existing selection."), {}, {TEXT("Progression.Node.Core.Threat.IgnoreMe")}), nullptr});
}
}
