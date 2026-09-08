#include "Progression/BreakerCoreRoster.h"

void BreakerCoreRoster::AppendWeapon(UObject* Outer, TArray<FBreakerCoreWedgeDefinition>& Out)
{
    using T = EBreakerNodeStatTarget;
    using B = EBreakerNodeStatBucket;
    Out.Add({TEXT("Precision"), TEXT("Weapon"), true,
        Node(Outer,TEXT("Core.Precision.Sightline"),TEXT("Sightline"),TEXT("+6% Critical Damage."),{Effect(T::CriticalDamage,B::Flat,6)}), // O2 PLACEHOLDER
        {
            {Node(Outer,TEXT("Core.Precision.Angle"),TEXT("Angle"),TEXT("+2% Critical Chance per rank."),{Effect(T::CriticalChance,B::Flat,2)}), // O2 PLACEHOLDER
             Node(Outer,TEXT("Core.Precision.CalledShot"),TEXT("Called Shot"),TEXT("+20% Increased Weapon Damage."),{Effect(T::WeaponDamage,B::IncreasedPercent,20)})}, // O2 PLACEHOLDER
            {Node(Outer,TEXT("Core.Precision.Cadence"),TEXT("Cadence"),TEXT("+3% Fire Rate per rank."),{Effect(T::FireRate,B::IncreasedPercent,3)}), // O2 PLACEHOLDER
             Node(Outer,TEXT("Core.Precision.TriggerDiscipline"),TEXT("Trigger Discipline"),TEXT("+25% Critical Damage."),{Effect(T::CriticalDamage,B::Flat,25)})}, // O2 PLACEHOLDER
            {Node(Outer,TEXT("Core.Precision.Ledger"),TEXT("Ledger"),TEXT("+5% Increased Weapon Damage per rank."),{Effect(T::WeaponDamage,B::IncreasedPercent,5)}), // O2 PLACEHOLDER
             Node(Outer,TEXT("Core.Precision.LongLens"),TEXT("Long Lens"),TEXT("+8% Fire Rate and +15% effective range."),{Effect(T::FireRate,B::IncreasedPercent,8),Effect(T::WeaponRange,B::IncreasedPercent,15)})} // O2 PLACEHOLDER
        },
        {Node(Outer,TEXT("Core.Precision.Steady"),TEXT("Steady"),TEXT("+12% recoil recovery."),{Effect(T::RecoilRecovery,B::IncreasedPercent,12)}), // O2 PLACEHOLDER
         Node(Outer,TEXT("Core.Precision.ColdBarrel"),TEXT("Cold Barrel"),TEXT("-10% base spread."),{Effect(T::WeaponBaseSpreadReduction,B::Flat,10)})}, // O2 PLACEHOLDER
        Node(Outer,TEXT("Core.Precision.Fixate"),TEXT("Fixate"),TEXT("1.22x More weapon damage on critical hits."),{Effect(T::WeaponCriticalDamage,B::MorePercent,22)}), // O2 PLACEHOLDER
        Node(Outer,TEXT("Core.Precision.Deadeye"),TEXT("Deadeye"),TEXT("You always critically strike. FORFEIT: all Critical Damage bonuses are halved."),{},{TEXT("Progression.Node.Core.Deadeye")})
    });
    Out.Add({TEXT("Vector"),TEXT("Weapon"),true,
        Node(Outer,TEXT("Core.Vector.Line"),TEXT("Line"),TEXT("+1 pierce."),{Effect(T::Pierce,B::Flat,1)}), // O2 PLACEHOLDER
        {
            {Node(Outer,TEXT("Core.Vector.Spread"),TEXT("Spread"),TEXT("+3 Added Damage per rank."),{Effect(T::AddedWeaponDamage,B::Flat,3)}), // O2 PLACEHOLDER
             Node(Outer,TEXT("Core.Vector.Multishot"),TEXT("Multishot"),TEXT("+1 projectile."),{Effect(T::ProjectileCount,B::Flat,1)})}, // O2 PLACEHOLDER
            {Node(Outer,TEXT("Core.Vector.Carom"),TEXT("Carom"),TEXT("+5% Increased Weapon Damage per rank."),{Effect(T::WeaponDamage,B::IncreasedPercent,5)}), // O2 PLACEHOLDER
             Node(Outer,TEXT("Core.Vector.Chainwork"),TEXT("Chainwork"),TEXT("+1 chain."),{Effect(T::ChainCount,B::Flat,1)})}, // O2 PLACEHOLDER
            {Node(Outer,TEXT("Core.Vector.Wake"),TEXT("Wake"),TEXT("+4 Added Damage per rank."),{Effect(T::AddedWeaponDamage,B::Flat,4)}), // O2 PLACEHOLDER
             Node(Outer,TEXT("Core.Vector.Overpenetration"),TEXT("Overpenetration"),TEXT("+1 pierce; pierced hits lose 15% less damage."),{Effect(T::Pierce,B::Flat,1),Effect(T::PierceLossReduction,B::Flat,15)})} // O2 PLACEHOLDER
        },
        {Node(Outer,TEXT("Core.Vector.Ricochet"),TEXT("Ricochet"),TEXT("+1 ricochet."),{Effect(T::RicochetCount,B::Flat,1)}), // O2 PLACEHOLDER
         Node(Outer,TEXT("Core.Vector.Lead"),TEXT("Lead"),TEXT("+10% projectile speed."),{Effect(T::ProjectileSpeed,B::IncreasedPercent,10)})}, // O2 PLACEHOLDER
        Node(Outer,TEXT("Core.Vector.Splinter"),TEXT("Splinter"),TEXT("1.20x More weapon damage to targets beyond the first."),{Effect(T::WeaponBeyondFirstDamage,B::MorePercent,20)}), // O2 PLACEHOLDER
        Node(Outer,TEXT("Core.Vector.Fan"),TEXT("Fan"),TEXT("+2 projectiles. FORFEIT: your spread cannot be reduced by anything."),{},{TEXT("Progression.Node.Core.Fan")}) // O2 PLACEHOLDER
    });
    // Delivery gap: projectile Pierce/Chain continuation has no authored
    // rocket explosion policy. Counts are live for hitscan; do not substitute
    // extra radial explosions for an unspecified continuation rule.
    Out.Add({TEXT("Ballistics"),TEXT("Weapon"),true,
        Node(Outer,TEXT("Core.Ballistics.WeightOfIt"),TEXT("Weight of It"),TEXT("+5 Added Damage."),{Effect(T::AddedWeaponDamage,B::Flat,5)}), // O2 PLACEHOLDER
        {
            {Node(Outer,TEXT("Core.Ballistics.ShapedCharge"),TEXT("Shaped Charge"),TEXT("+5 Added Damage per rank."),{Effect(T::AddedWeaponDamage,B::Flat,5)}), // O2 PLACEHOLDER
             Node(Outer,TEXT("Core.Ballistics.Siege"),TEXT("Siege"),TEXT("+22% Increased Weapon Damage."),{Effect(T::WeaponDamage,B::IncreasedPercent,22)})}, // O2 PLACEHOLDER
            {Node(Outer,TEXT("Core.Ballistics.Range"),TEXT("Range"),TEXT("+8% effective range per rank."),{Effect(T::WeaponRange,B::IncreasedPercent,8)}), // O2 PLACEHOLDER
             Node(Outer,TEXT("Core.Ballistics.FlatTrajectory"),TEXT("Flat Trajectory"),TEXT("Falloff begins 40% further out."),{Effect(T::WeaponFalloffStart,B::IncreasedPercent,40)})}, // O2 PLACEHOLDER
            {Node(Outer,TEXT("Core.Ballistics.Concussion"),TEXT("Concussion"),TEXT("+6% weapon splash area per rank."),{Effect(T::WeaponSplashArea,B::IncreasedPercent,6)}), // O2 PLACEHOLDER
             Node(Outer,TEXT("Core.Ballistics.Break"),TEXT("Break"),TEXT("Weapon hits shred 8% armour for 4 seconds, up to 3 stacks."),{},{TEXT("Progression.Node.Core.Ballistics.Break")})} // O2 PLACEHOLDER
        },
        {Node(Outer,TEXT("Core.Ballistics.Cull"),TEXT("Cull"),TEXT("+8% Increased Weapon Damage."),{Effect(T::WeaponDamage,B::IncreasedPercent,8)}), // O2 PLACEHOLDER
         Node(Outer,TEXT("Core.Ballistics.Loud"),TEXT("Loud"),TEXT("Splash also hits the nearest enemy outside the blast within one further radius."),{},{TEXT("Progression.Node.Core.Ballistics.Loud")})},
        Node(Outer,TEXT("Core.Ballistics.Collapse"),TEXT("Collapse"),TEXT("1.24x More weapon damage."),{Effect(T::WeaponDamage,B::MorePercent,24)}), // O2 PLACEHOLDER
        Node(Outer,TEXT("Core.Ballistics.Overpressure"),TEXT("Overpressure"),TEXT("Every weapon hit splashes 40% at 3 metres. FORFEIT: effective range is halved."),{},{TEXT("Progression.Node.Core.Ballistics.Overpressure")}) // O2 PLACEHOLDER
    });
    Out.Add({TEXT("Loadout"),TEXT("Weapon"),false,
        Node(Outer,TEXT("Core.Loadout.Sling"),TEXT("Sling"),TEXT("+12% weapon swap speed."),{Effect(T::WeaponSwapSpeed,B::IncreasedPercent,12)}), // O2 PLACEHOLDER
        {
            {Node(Outer,TEXT("Core.Loadout.ReloadDrill"),TEXT("Reload Drill"),TEXT("+6% reload speed per rank."),{Effect(T::WeaponReloadSpeed,B::IncreasedPercent,6)}), // O2 PLACEHOLDER
             Node(Outer,TEXT("Core.Loadout.Magazine"),TEXT("Magazine"),TEXT("+20% magazine capacity."),{Effect(T::WeaponMagazineCapacity,B::IncreasedPercent,20)})}, // O2 PLACEHOLDER
            {Node(Outer,TEXT("Core.Loadout.Reserve"),TEXT("Reserve"),TEXT("+15% reserve ammunition per rank."),{Effect(T::WeaponReserveAmmo,B::IncreasedPercent,15)}), // O2 PLACEHOLDER
             Node(Outer,TEXT("Core.Loadout.Quickdraw"),TEXT("Quickdraw"),TEXT("+25% swap speed; first shot after swapping deals +30% damage."),{Effect(T::WeaponSwapSpeed,B::IncreasedPercent,25)},{TEXT("Progression.Node.Core.Quickdraw")})} // O2 PLACEHOLDER
        },{},
        Node(Outer,TEXT("Core.Loadout.TwoGuns"),TEXT("Two Guns"),TEXT("Your holstered weapon reloads itself over 6 seconds."),{},{TEXT("Progression.Node.Core.TwoGuns")}),nullptr // O2 PLACEHOLDER
    });
}
