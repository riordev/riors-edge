#include "Weapons/BreakerWeaponComponent.h"

#include "Abilities/BreakerAbilityStateComponent.h"
#include "Abilities/BreakerAbility_Lead.h"
#include "Attributes/BreakerAttributeSet.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "Classes/BreakerMomentumComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerStatusComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Progression/BreakerBuildConditions.h"
#include "Progression/BreakerProgressionComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/UObjectIterator.h"
#include "Weapons/BreakerRocketProjectile.h"
#include "Weapons/BreakerWeaponDefinition.h"
#include "Weapons/BreakerWeaponFeel.h"
#include "Weapons/BreakerWeaponMath.h"

namespace
{
    // Salts the shared shot seed so the bleed roll never correlates with the
    // spread or critical rolls drawn from the same shot sequence.
    constexpr uint32 BreakerBleedSalt = 0x51ED0000u;

    // Sub-stream salts for the projectile channels (owner ruling 2026-08-16).
    // Every draw the channels add — an extra pellet's spread, a pierce or
    // chain hit's crit roll — comes from one of these salted streams rather
    // than from ++ShotSequence, so a build with the channels at ZERO produces
    // bit-identical recoil, spread and crit sequences to a build from before
    // the channels existed. That is the whole determinism contract.
    constexpr uint32 BreakerMultishotSalt = 0x3B0057A0u;
    constexpr uint32 BreakerPierceSalt = 0x91E4CE00u;
    constexpr uint32 BreakerChainSalt = 0xC4A15000u;
    constexpr uint32 BreakerRicochetSalt = 0x51C0C4E7u;

    // Marksman node ids and tags this fire path consumes, spelled once. These
    // are the serialized names from Progression/BreakerProgressionLibrary.cpp;
    // RequestGameplayTag(..., false) so a rig without the tag table loaded
    // reads "not owned" rather than asserting.
    const FName BreakerPierceDisciplineNodeId(TEXT("Swift.Marksman.PierceDiscipline"));
    const FName BreakerAngleNodeId(TEXT("Swift.Marksman.Angle"));
    FGameplayTag BreakerSightlineTag() { return FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Swift.Marksman.Sightline"), false); }
    FGameplayTag BreakerOverpenetrationTag() { return FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Swift.Marksman.Overpenetration"), false); }

    // Recoil belongs in the archetype table beside cadence, spread, falloff and
    // damage, so the five weapons kick like five weapons. Every number here is
    // an O2 PLACEHOLDER. The struct's own defaults are the rifle; each case
    // states only what makes that archetype different.
    FBreakerRecoilProfile ArchetypeRecoilProfile(EBreakerWeaponArchetype Archetype)
    {
        FBreakerRecoilProfile Profile;
        switch (Archetype)
        {
        case EBreakerWeaponArchetype::SMG:
            // Buzzy: barely moves per shot, but 900 RPM stacks it fast and it
            // wanders sideways more than it climbs. Recovers quickly.
            Profile.VerticalKickDegrees = 0.26f;              // O2 PLACEHOLDER
            Profile.HorizontalKickDegrees = 0.24f;            // O2 PLACEHOLDER
            Profile.HorizontalPatternPeriod = 5;              // O2 PLACEHOLDER
            Profile.VerticalRandomFraction = 0.18f;           // O2 PLACEHOLDER
            Profile.HorizontalRandomDegrees = 0.09f;          // O2 PLACEHOLDER
            Profile.ClimbRampShots = 9.0f;                    // O2 PLACEHOLDER
            Profile.ClimbRampMultiplier = 2.0f;               // O2 PLACEHOLDER
            Profile.MaxVerticalDegrees = 8.0f;                // O2 PLACEHOLDER
            Profile.MaxHorizontalDegrees = 4.5f;              // O2 PLACEHOLDER
            Profile.AimRecoilMultiplier = 0.72f;              // O2 PLACEHOLDER
            Profile.RecoveryDelaySeconds = 0.06f;             // O2 PLACEHOLDER
            Profile.RecoveryInterpSpeed = 12.0f;              // O2 PLACEHOLDER
            Profile.RecoveryConstantDegreesPerSecond = 20.0f; // O2 PLACEHOLDER
            Profile.BloomPerShotDegrees = 0.11f;              // O2 PLACEHOLDER
            Profile.MaxBloomDegrees = 2.4f;                   // O2 PLACEHOLDER
            Profile.BloomRecoveryDegreesPerSecond = 3.0f;     // O2 PLACEHOLDER
            Profile.AimBloomMultiplier = 0.5f;                // O2 PLACEHOLDER
            Profile.BurstResetSeconds = 0.28f;                // O2 PLACEHOLDER
            Profile.ViewmodelKickUnits = 2.0f;                // O2 PLACEHOLDER
            Profile.ViewmodelKickLateralUnits = 1.0f;         // O2 PLACEHOLDER
            Profile.ViewmodelKickPitchDegrees = 1.6f;         // O2 PLACEHOLDER
            Profile.ViewmodelSpringStiffness = 320.0f;        // O2 PLACEHOLDER
            // Fastest into the sights and the least punished for running with
            // them up: the SMG is the one weapon that is allowed to be a
            // run-and-gun ADS weapon.
            Profile.AimInSeconds = 0.14f;                     // O2 PLACEHOLDER
            Profile.MoveSpreadDegrees = 0.30f;                // O2 PLACEHOLDER
            Profile.AimMoveSpreadMultiplier = 1.8f;           // O2 PLACEHOLDER
            // Loosest aimed speed penalty of the original five, for the same
            // reason it has the loosest aimed movement cone.
            Profile.AimMoveSpeedMultiplier = 0.88f;           // O2 PLACEHOLDER
            break;
        case EBreakerWeaponArchetype::Sniper:
            // One enormous, slow kick that has to be re-aimed rather than
            // ridden. ADS cuts it hardest: the scope is the reason to use it.
            Profile.VerticalKickDegrees = 2.8f;               // O2 PLACEHOLDER
            Profile.HorizontalKickDegrees = 0.45f;            // O2 PLACEHOLDER
            Profile.HorizontalPatternPeriod = 3;              // O2 PLACEHOLDER
            Profile.VerticalRandomFraction = 0.06f;           // O2 PLACEHOLDER
            Profile.HorizontalRandomDegrees = 0.12f;          // O2 PLACEHOLDER
            Profile.ClimbRampShots = 3.0f;                    // O2 PLACEHOLDER
            Profile.ClimbRampMultiplier = 1.15f;              // O2 PLACEHOLDER
            Profile.MaxVerticalDegrees = 9.0f;                // O2 PLACEHOLDER
            Profile.MaxHorizontalDegrees = 2.5f;              // O2 PLACEHOLDER
            Profile.AimRecoilMultiplier = 0.55f;              // O2 PLACEHOLDER
            Profile.RecoveryDelaySeconds = 0.2f;              // O2 PLACEHOLDER
            Profile.RecoveryInterpSpeed = 5.0f;               // O2 PLACEHOLDER
            Profile.RecoveryConstantDegreesPerSecond = 9.0f;  // O2 PLACEHOLDER
            Profile.BloomPerShotDegrees = 0.6f;               // O2 PLACEHOLDER
            Profile.MaxBloomDegrees = 3.0f;                   // O2 PLACEHOLDER
            Profile.BloomRecoveryDegreesPerSecond = 1.6f;     // O2 PLACEHOLDER
            Profile.AimBloomMultiplier = 0.35f;               // O2 PLACEHOLDER
            Profile.BurstResetSeconds = 0.9f;                 // O2 PLACEHOLDER
            Profile.ViewmodelKickUnits = 9.0f;                // O2 PLACEHOLDER
            Profile.ViewmodelKickLateralUnits = 1.6f;         // O2 PLACEHOLDER
            Profile.ViewmodelKickPitchDegrees = 6.5f;         // O2 PLACEHOLDER
            Profile.ViewmodelSpringStiffness = 150.0f;        // O2 PLACEHOLDER
            Profile.ViewmodelSpringDamping = 17.0f;           // O2 PLACEHOLDER
            Profile.AimViewmodelMultiplier = 0.5f;            // O2 PLACEHOLDER
            // Slowest scope in the table and the harshest movement penalty:
            // the sniper is the weapon that must be PLANTED, and hip firing
            // one is deliberately close to useless.
            Profile.AimInSeconds = 0.38f;                     // O2 PLACEHOLDER
            Profile.MoveSpreadDegrees = 1.10f;                // O2 PLACEHOLDER
            Profile.AimMoveSpreadMultiplier = 3.0f;           // O2 PLACEHOLDER
            // Scoped and moving is nearly standing still, which is the point.
            Profile.AimMoveSpeedMultiplier = 0.50f;           // O2 PLACEHOLDER
            break;
        case EBreakerWeaponArchetype::Shotgun:
            // A shove. Note FirstShotSpreadMultiplier stays at 1.0: the pellet
            // cone IS the shotgun, and zeroing it would turn it into a slug.
            Profile.VerticalKickDegrees = 1.9f;               // O2 PLACEHOLDER
            Profile.HorizontalKickDegrees = 0.7f;             // O2 PLACEHOLDER
            Profile.HorizontalPatternPeriod = 4;              // O2 PLACEHOLDER
            Profile.VerticalRandomFraction = 0.14f;           // O2 PLACEHOLDER
            Profile.HorizontalRandomDegrees = 0.25f;          // O2 PLACEHOLDER
            Profile.ClimbRampShots = 4.0f;                    // O2 PLACEHOLDER
            Profile.ClimbRampMultiplier = 1.3f;               // O2 PLACEHOLDER
            Profile.MaxVerticalDegrees = 8.5f;                // O2 PLACEHOLDER
            Profile.MaxHorizontalDegrees = 3.5f;              // O2 PLACEHOLDER
            Profile.AimRecoilMultiplier = 0.75f;              // O2 PLACEHOLDER
            Profile.RecoveryDelaySeconds = 0.14f;             // O2 PLACEHOLDER
            Profile.RecoveryInterpSpeed = 6.5f;               // O2 PLACEHOLDER
            Profile.RecoveryConstantDegreesPerSecond = 13.0f; // O2 PLACEHOLDER
            Profile.FirstShotSpreadMultiplier = 1.0f;         // O2 PLACEHOLDER
            Profile.BloomPerShotDegrees = 0.4f;               // O2 PLACEHOLDER
            Profile.MaxBloomDegrees = 2.0f;                   // O2 PLACEHOLDER
            Profile.BloomRecoveryDegreesPerSecond = 2.4f;     // O2 PLACEHOLDER
            Profile.BurstResetSeconds = 0.6f;                 // O2 PLACEHOLDER
            Profile.ViewmodelKickUnits = 8.0f;                // O2 PLACEHOLDER
            Profile.ViewmodelKickLateralUnits = 1.4f;         // O2 PLACEHOLDER
            Profile.ViewmodelKickPitchDegrees = 5.2f;         // O2 PLACEHOLDER
            Profile.ViewmodelSpringStiffness = 180.0f;        // O2 PLACEHOLDER
            Profile.ViewmodelSpringDamping = 19.0f;           // O2 PLACEHOLDER
            // Smallest movement penalty in the table. Hip firing a shotgun
            // while strafing is the thing it is FOR, and its own 4.5 degree
            // pellet cone dwarfs a quarter-degree of movement anyway.
            Profile.AimInSeconds = 0.16f;                     // O2 PLACEHOLDER
            Profile.MoveSpreadDegrees = 0.25f;                // O2 PLACEHOLDER
            Profile.AimMoveSpreadMultiplier = 2.0f;           // O2 PLACEHOLDER
            // Barely slowed. Strafing into contact is the shotgun's whole job
            // and a speed penalty would delete it.
            Profile.AimMoveSpeedMultiplier = 0.85f;           // O2 PLACEHOLDER
            break;
        case EBreakerWeaponArchetype::Rocket:
            // Heaviest single kick in the table and the slowest settle. Almost
            // no sideways component: it is mass, not muzzle climb.
            Profile.VerticalKickDegrees = 2.4f;               // O2 PLACEHOLDER
            Profile.HorizontalKickDegrees = 0.3f;             // O2 PLACEHOLDER
            Profile.HorizontalPatternPeriod = 2;              // O2 PLACEHOLDER
            Profile.VerticalRandomFraction = 0.05f;           // O2 PLACEHOLDER
            Profile.HorizontalRandomDegrees = 0.06f;          // O2 PLACEHOLDER
            Profile.ClimbRampShots = 2.0f;                    // O2 PLACEHOLDER
            Profile.ClimbRampMultiplier = 1.1f;               // O2 PLACEHOLDER
            Profile.MaxVerticalDegrees = 9.0f;                // O2 PLACEHOLDER
            Profile.MaxHorizontalDegrees = 2.0f;              // O2 PLACEHOLDER
            Profile.AimRecoilMultiplier = 0.7f;               // O2 PLACEHOLDER
            Profile.RecoveryDelaySeconds = 0.22f;             // O2 PLACEHOLDER
            Profile.RecoveryInterpSpeed = 4.5f;               // O2 PLACEHOLDER
            Profile.RecoveryConstantDegreesPerSecond = 8.0f;  // O2 PLACEHOLDER
            Profile.BloomPerShotDegrees = 0.25f;              // O2 PLACEHOLDER
            Profile.MaxBloomDegrees = 1.2f;                   // O2 PLACEHOLDER
            Profile.BloomRecoveryDegreesPerSecond = 1.2f;     // O2 PLACEHOLDER
            Profile.BurstResetSeconds = 0.8f;                 // O2 PLACEHOLDER
            Profile.ViewmodelKickUnits = 10.0f;               // O2 PLACEHOLDER
            Profile.ViewmodelKickLateralUnits = 1.2f;         // O2 PLACEHOLDER
            Profile.ViewmodelKickPitchDegrees = 6.8f;         // O2 PLACEHOLDER
            Profile.ViewmodelSpringStiffness = 140.0f;        // O2 PLACEHOLDER
            Profile.ViewmodelSpringDamping = 16.0f;           // O2 PLACEHOLDER
            Profile.AimInSeconds = 0.30f;                     // O2 PLACEHOLDER
            Profile.MoveSpreadDegrees = 0.30f;                // O2 PLACEHOLDER
            Profile.AimMoveSpeedMultiplier = 0.65f;           // O2 PLACEHOLDER
            break;
        // ---- O27 breadth additions -----------------------------------------
        // "Choices should beat accumulation but there should be significantly
        // more options in all avenues." Each of these three occupies a niche
        // none of the original five did, and the recoil pattern is the main
        // lever that makes it FELT rather than merely tabulated.
        case EBreakerWeaponArchetype::BurstRifle:
            // VOLLEY. The learnable-pattern weapon, taken to its limit: a
            // near-pure vertical ladder with almost no horizontal component and
            // an aggressive in-burst ramp, so the three rounds of a burst climb
            // a straight line the player can pre-aim down. The whole ladder is
            // then paid off in the cycle gap — RecoveryDelaySeconds plus the
            // settle is comfortably shorter than BurstCycleSeconds — so every
            // burst starts from the same place and the pattern is a skill
            // rather than a tax. This is the opposite pole from the SMG, whose
            // pattern is a wander you ride; this one you memorise.
            Profile.VerticalKickDegrees = 0.85f;              // O2 PLACEHOLDER
            Profile.HorizontalKickDegrees = 0.05f;            // O2 PLACEHOLDER
            Profile.HorizontalPatternPeriod = 6;              // O2 PLACEHOLDER
            Profile.VerticalRandomFraction = 0.03f;           // O2 PLACEHOLDER
            Profile.HorizontalRandomDegrees = 0.02f;          // O2 PLACEHOLDER
            Profile.ClimbRampShots = 2.0f;                    // O2 PLACEHOLDER
            Profile.ClimbRampMultiplier = 1.6f;               // O2 PLACEHOLDER
            Profile.MaxVerticalDegrees = 6.0f;                // O2 PLACEHOLDER
            Profile.MaxHorizontalDegrees = 1.0f;              // O2 PLACEHOLDER
            Profile.AimRecoilMultiplier = 0.6f;               // O2 PLACEHOLDER
            Profile.RecoveryDelaySeconds = 0.09f;             // O2 PLACEHOLDER
            Profile.RecoveryInterpSpeed = 11.0f;              // O2 PLACEHOLDER
            Profile.RecoveryConstantDegreesPerSecond = 22.0f; // O2 PLACEHOLDER
            // Bloom is nearly absent. A burst weapon is punished by its own
            // cadence, not by a widening cone, and the burst always resets.
            Profile.BloomPerShotDegrees = 0.05f;              // O2 PLACEHOLDER
            Profile.MaxBloomDegrees = 0.6f;                   // O2 PLACEHOLDER
            Profile.BloomRecoveryDegreesPerSecond = 4.0f;     // O2 PLACEHOLDER
            Profile.AimBloomMultiplier = 0.4f;                // O2 PLACEHOLDER
            // Under the burst cycle gap, so shot 0 of every burst is the
            // dead-accurate first shot. That IS the archetype.
            Profile.BurstResetSeconds = 0.22f;                // O2 PLACEHOLDER
            Profile.ViewmodelKickUnits = 3.4f;                // O2 PLACEHOLDER
            Profile.ViewmodelKickLateralUnits = 0.5f;         // O2 PLACEHOLDER
            Profile.ViewmodelKickPitchDegrees = 2.8f;         // O2 PLACEHOLDER
            Profile.ViewmodelSpringStiffness = 300.0f;        // O2 PLACEHOLDER
            Profile.AimInSeconds = 0.22f;                     // O2 PLACEHOLDER
            Profile.MoveSpreadDegrees = 0.55f;                // O2 PLACEHOLDER
            Profile.AimMoveSpreadMultiplier = 2.4f;           // O2 PLACEHOLDER
            Profile.AimMoveSpeedMultiplier = 0.70f;           // O2 PLACEHOLDER
            break;
        case EBreakerWeaponArchetype::Machinegun:
            // BULWARK. The sustained-fire weapon, and the only one whose recoil
            // is designed to be UNRIDEABLE past a point: a modest per-shot kick
            // with the longest ramp and the highest ceilings in the table, so
            // the first second is controllable and the fifth is not. Bloom does
            // most of the punishing — it is the only archetype whose held cone
            // grows past the shotgun's — which is what turns "hold the trigger"
            // into "hold the trigger in bursts, planted".
            Profile.VerticalKickDegrees = 0.34f;              // O2 PLACEHOLDER
            Profile.HorizontalKickDegrees = 0.30f;            // O2 PLACEHOLDER
            Profile.HorizontalPatternPeriod = 11;             // O2 PLACEHOLDER
            Profile.VerticalRandomFraction = 0.22f;           // O2 PLACEHOLDER
            Profile.HorizontalRandomDegrees = 0.14f;          // O2 PLACEHOLDER
            Profile.ClimbRampShots = 22.0f;                   // O2 PLACEHOLDER
            Profile.ClimbRampMultiplier = 2.6f;               // O2 PLACEHOLDER
            Profile.MaxVerticalDegrees = 11.0f;               // O2 PLACEHOLDER
            Profile.MaxHorizontalDegrees = 6.0f;              // O2 PLACEHOLDER
            Profile.AimRecoilMultiplier = 0.68f;              // O2 PLACEHOLDER
            Profile.RecoveryDelaySeconds = 0.18f;             // O2 PLACEHOLDER
            Profile.RecoveryInterpSpeed = 5.5f;               // O2 PLACEHOLDER
            Profile.RecoveryConstantDegreesPerSecond = 11.0f; // O2 PLACEHOLDER
            Profile.BloomPerShotDegrees = 0.10f;              // O2 PLACEHOLDER
            Profile.MaxBloomDegrees = 4.2f;                   // O2 PLACEHOLDER
            Profile.BloomRecoveryDegreesPerSecond = 2.0f;     // O2 PLACEHOLDER
            Profile.AimBloomMultiplier = 0.6f;                // O2 PLACEHOLDER
            Profile.BurstResetSeconds = 0.5f;                 // O2 PLACEHOLDER
            Profile.ViewmodelKickUnits = 2.6f;                // O2 PLACEHOLDER
            Profile.ViewmodelKickLateralUnits = 1.5f;         // O2 PLACEHOLDER
            Profile.ViewmodelKickPitchDegrees = 1.9f;         // O2 PLACEHOLDER
            Profile.ViewmodelSpringStiffness = 240.0f;        // O2 PLACEHOLDER
            // The heaviest weapon in the table to bring up and the most rooted
            // once it is up — harsher than the sniper on speed, because the
            // sniper's answer to being rushed is to stop aiming and this one's
            // is to keep firing. It is also the least punished for HIP firing
            // while moving relative to its own aimed cost, which is why the
            // aimed movement multiplier is the biggest in the table.
            Profile.AimInSeconds = 0.42f;                     // O2 PLACEHOLDER
            Profile.MoveSpreadDegrees = 0.80f;                // O2 PLACEHOLDER
            Profile.AimMoveSpreadMultiplier = 3.2f;           // O2 PLACEHOLDER
            Profile.AimMoveSpeedMultiplier = 0.45f;           // O2 PLACEHOLDER
            break;
        case EBreakerWeaponArchetype::Sidearm:
            // MARK. The tempo weapon. The smallest kick in the table, fully
            // settled between shots at any realistic trigger speed, so the
            // pattern is not a pattern at all — the limiter is the player's
            // click rate and their aim, and nothing else. It exists to pair
            // with the swap layer: fastest swap-in, fastest reload, deepest
            // reserve. The gun you finish a fight with, not the one you start
            // it with.
            Profile.VerticalKickDegrees = 0.30f;              // O2 PLACEHOLDER
            Profile.HorizontalKickDegrees = 0.10f;            // O2 PLACEHOLDER
            Profile.HorizontalPatternPeriod = 4;              // O2 PLACEHOLDER
            Profile.VerticalRandomFraction = 0.10f;           // O2 PLACEHOLDER
            Profile.HorizontalRandomDegrees = 0.05f;          // O2 PLACEHOLDER
            Profile.ClimbRampShots = 5.0f;                    // O2 PLACEHOLDER
            Profile.ClimbRampMultiplier = 1.25f;              // O2 PLACEHOLDER
            Profile.MaxVerticalDegrees = 4.0f;                // O2 PLACEHOLDER
            Profile.MaxHorizontalDegrees = 1.6f;              // O2 PLACEHOLDER
            Profile.AimRecoilMultiplier = 0.65f;              // O2 PLACEHOLDER
            // The fastest settle in the table by a distance: it must be back on
            // target before a fast trigger finger gets there.
            Profile.RecoveryDelaySeconds = 0.03f;             // O2 PLACEHOLDER
            Profile.RecoveryInterpSpeed = 16.0f;              // O2 PLACEHOLDER
            Profile.RecoveryConstantDegreesPerSecond = 30.0f; // O2 PLACEHOLDER
            Profile.BloomPerShotDegrees = 0.16f;              // O2 PLACEHOLDER
            Profile.MaxBloomDegrees = 1.8f;                   // O2 PLACEHOLDER
            Profile.BloomRecoveryDegreesPerSecond = 5.0f;     // O2 PLACEHOLDER
            Profile.AimBloomMultiplier = 0.5f;                // O2 PLACEHOLDER
            Profile.BurstResetSeconds = 0.20f;                // O2 PLACEHOLDER
            Profile.ViewmodelKickUnits = 2.2f;                // O2 PLACEHOLDER
            Profile.ViewmodelKickLateralUnits = 0.7f;         // O2 PLACEHOLDER
            Profile.ViewmodelKickPitchDegrees = 2.0f;         // O2 PLACEHOLDER
            Profile.ViewmodelSpringStiffness = 340.0f;        // O2 PLACEHOLDER
            // Snaps up faster than anything else and barely slows you: a
            // sidearm you cannot bring up in a hurry is not a sidearm.
            Profile.AimInSeconds = 0.10f;                     // O2 PLACEHOLDER
            Profile.MoveSpreadDegrees = 0.35f;                // O2 PLACEHOLDER
            Profile.AimMoveSpreadMultiplier = 1.6f;           // O2 PLACEHOLDER
            Profile.AimMoveSpeedMultiplier = 0.92f;           // O2 PLACEHOLDER
            break;
        default:
            // Rifle: the struct defaults. A learnable climb with a gentle
            // sideways sway, settling in about a third of a second.
            break;
        }
        return Profile;
    }

    UBreakerWeaponDefinition* GetPrototypeDefinition(EBreakerWeaponArchetype Archetype)
    {
        static TObjectPtr<UBreakerWeaponDefinition> Prototypes[static_cast<int32>(EBreakerWeaponArchetype::Count)];
        const int32 Index = static_cast<int32>(Archetype);
        if (!Prototypes[Index])
        {
            // Indexed by archetype. MUST stay the same length as the enum: a
            // missing row is an out-of-bounds read on the first equip of a new
            // weapon, not a compile error.
            static_assert(static_cast<int32>(EBreakerWeaponArchetype::Count) == 8,
                "Add a prototype name row when you add an archetype.");
            const FName Names[] =
            {
                TEXT("PrototypeRifleDefinition"), TEXT("PrototypeSMGDefinition"), TEXT("PrototypeSniperDefinition"),
                TEXT("PrototypeShotgunDefinition"), TEXT("PrototypeRocketDefinition"),
                TEXT("PrototypeBurstRifleDefinition"), TEXT("PrototypeMachinegunDefinition"),
                TEXT("PrototypeSidearmDefinition")
            };
            Prototypes[Index] = NewObject<UBreakerWeaponDefinition>(GetTransientPackage(), Names[Index]);
            Prototypes[Index]->AddToRoot();
            UBreakerWeaponDefinition* Definition = Prototypes[Index];
            switch (Archetype)
            {
            case EBreakerWeaponArchetype::SMG:
                Definition->WeaponId = TEXT("SMG");
                Definition->DisplayName = FText::FromString(TEXT("SMG"));
                Definition->Damage = 13.0f;
                Definition->WeakPointMultiplier = 1.5f;
                Definition->RoundsPerMinute = 900.0f;
                Definition->bAutomatic = true;
                Definition->HipSpreadDegrees = 2.0f;
                Definition->AimSpreadDegrees = 0.9f;
                Definition->MagazineSize = 35;
                Definition->StartingReserveAmmo = 175;
                Definition->ReloadDuration = 1.5f;
                Definition->FalloffStart = 1800.0f;   // O2 PLACEHOLDER
                Definition->FalloffEnd = 4500.0f;     // O2 PLACEHOLDER
                Definition->MinimumFalloffMultiplier = 0.58f; // O2 PLACEHOLDER
                Definition->MaximumRange = 6000.0f;
                Definition->SwapInDuration = 0.35f;
                Definition->BleedChance = 0.25f;
                Definition->BleedDamagePerTick = 6.0f;
                Definition->BleedDuration = 3.0f;
                Definition->BleedTickInterval = 0.5f;
                break;
            case EBreakerWeaponArchetype::Sniper:
                Definition->WeaponId = TEXT("Sniper");
                Definition->DisplayName = FText::FromString(TEXT("Sniper"));
                Definition->Damage = 72.0f;
                Definition->WeakPointMultiplier = 2.0f;
                Definition->RoundsPerMinute = 150.0f;
                Definition->bAutomatic = false;
                Definition->HipSpreadDegrees = 2.0f;
                Definition->AimSpreadDegrees = 0.05f;
                Definition->MagazineSize = 8;
                Definition->StartingReserveAmmo = 40;
                Definition->ReloadDuration = 2.3f;
                // The sniper barely falls off at all: 12% across sixty metres.
                Definition->FalloffStart = 5000.0f;   // O2 PLACEHOLDER
                Definition->FalloffEnd = 11000.0f;    // O2 PLACEHOLDER
                Definition->MinimumFalloffMultiplier = 0.88f; // O2 PLACEHOLDER
                Definition->MaximumRange = 15000.0f;
                Definition->SwapInDuration = 0.7f;
                break;
            case EBreakerWeaponArchetype::Shotgun:
                Definition->WeaponId = TEXT("Shotgun");
                Definition->DisplayName = FText::FromString(TEXT("Shotgun"));
                Definition->Damage = 10.0f;
                Definition->WeakPointMultiplier = 1.35f;
                Definition->RoundsPerMinute = 85.0f;
                Definition->bAutomatic = false;
                Definition->PelletsPerShot = 8;
                Definition->HipSpreadDegrees = 4.5f;
                Definition->AimSpreadDegrees = 3.0f;
                Definition->MagazineSize = 8;
                Definition->StartingReserveAmmo = 40;
                Definition->ReloadDuration = 2.2f;
                // Still the steepest curve in the table by a wide margin —
                // 3.5% per metre against the rifle's 0.67% — because falling
                // off hard is what a shotgun IS. It just starts a little
                // further out and bottoms out a little higher.
                Definition->FalloffStart = 1100.0f;   // O2 PLACEHOLDER
                Definition->FalloffEnd = 2800.0f;     // O2 PLACEHOLDER
                Definition->MinimumFalloffMultiplier = 0.40f; // O2 PLACEHOLDER
                Definition->MaximumRange = 4000.0f;
                break;
            case EBreakerWeaponArchetype::Rocket:
                Definition->WeaponId = TEXT("Rocket");
                Definition->DisplayName = FText::FromString(TEXT("Rocket Launcher"));
                Definition->Damage = 90.0f;
                Definition->WeakPointMultiplier = 1.0f;
                Definition->RoundsPerMinute = 55.0f;
                Definition->bAutomatic = false;
                Definition->HipSpreadDegrees = 0.6f;
                Definition->AimSpreadDegrees = 0.2f;
                Definition->MagazineSize = 4;
                Definition->StartingReserveAmmo = 16;
                Definition->ReloadDuration = 2.8f;
                Definition->MaximumRange = 12000.0f;
                Definition->SwapInDuration = 0.8f;
                Definition->bProjectile = true;
                Definition->ProjectileSpeed = 3200.0f;
                Definition->ExplosionRadius = 350.0f;
                break;

            // ---- O27 breadth additions ----------------------------------
            // Every number below is the ITEM LEVEL 1 anchor and rides
            // WeaponBase(ilvl) = ArchetypeBase * (1+w)^(ilvl-1) through the
            // shared GetScaledBaseDamage path exactly like the original five.
            // Nothing here scales around the curve. All O2 PLACEHOLDER.
            case EBreakerWeaponArchetype::BurstRifle:
                // VOLLEY — the discipline weapon. Its cadence axis is the one
                // no other archetype has: fixed three-round bursts with a
                // 0.34 s gap you cannot shorten, so DPS is bounded by the CYCLE
                // rather than by the trigger, and the whole gun is built around
                // making each burst land. Per-round damage sits above the rifle
                // and below the sniper; sustained DPS lands under the rifle,
                // which is the price of the accuracy. Magazine is exactly nine
                // bursts, so ammunition is counted in bursts and a reload
                // never strands the player mid-burst.
                Definition->WeaponId = TEXT("BurstRifle");
                Definition->DisplayName = FText::FromString(TEXT("Burst Rifle"));
                Definition->Damage = 29.0f;
                Definition->WeakPointMultiplier = 1.9f;
                Definition->RoundsPerMinute = 720.0f;   // WITHIN a burst only.
                Definition->bAutomatic = true;
                Definition->ShotsPerBurst = 3;
                Definition->BurstCycleSeconds = 0.34f;
                Definition->HipSpreadDegrees = 1.6f;
                Definition->AimSpreadDegrees = 0.12f;
                Definition->MagazineSize = 27;          // nine whole bursts
                Definition->StartingReserveAmmo = 108;  // four magazines
                Definition->ReloadDuration = 2.0f;
                // Between the rifle and the sniper, matching where it fights.
                Definition->FalloffStart = 3600.0f;
                Definition->FalloffEnd = 8500.0f;
                Definition->MinimumFalloffMultiplier = 0.80f;
                Definition->MaximumRange = 13000.0f;
                Definition->SwapInDuration = 0.55f;
                break;
            case EBreakerWeaponArchetype::Machinegun:
                // BULWARK — the sustained-fire weapon, and an AMMUNITION
                // ECONOMY archetype before it is anything else: a 120-round
                // magazine is four SMG magazines in one trigger pull, and it
                // pays for it with the longest reload in the game and a reserve
                // that is only two and a half magazines deep. It is the one
                // weapon that can hold a lane through a whole wave without
                // reloading, and the one that is genuinely helpless if it has
                // to. Per-round damage is the lowest in the table; sustained
                // DPS is the highest, and only while planted.
                Definition->WeaponId = TEXT("Machinegun");
                Definition->DisplayName = FText::FromString(TEXT("Machinegun"));
                Definition->Damage = 11.0f;
                Definition->WeakPointMultiplier = 1.4f;
                Definition->RoundsPerMinute = 700.0f;
                Definition->bAutomatic = true;
                Definition->HipSpreadDegrees = 3.2f;
                Definition->AimSpreadDegrees = 0.8f;
                Definition->MagazineSize = 120;
                Definition->StartingReserveAmmo = 300;
                Definition->ReloadDuration = 4.2f;      // the whole cost
                // A long shallow curve: it reaches further than the rifle but
                // gives ground the entire way, which is what makes it a
                // suppression weapon rather than a long-range one.
                Definition->FalloffStart = 2200.0f;
                Definition->FalloffEnd = 9000.0f;
                Definition->MinimumFalloffMultiplier = 0.55f;
                Definition->MaximumRange = 11000.0f;
                Definition->SwapInDuration = 0.95f;     // heaviest to bring up
                break;
            case EBreakerWeaponArchetype::Sidearm:
                // MARK — the tempo weapon. It exists to make the SWAP a
                // decision: 0.18 s swap-in against the rifle's 0.5 and the
                // machinegun's 0.95, a 1.1 s reload, and by far the deepest
                // reserve in the table, so it is the answer to a dry primary
                // rather than a second primary. Semi-automatic, so its DPS
                // ceiling is the player's trigger; and it is the one archetype
                // whose damage barely falls off inside the 9-19 m band the gym
                // actually fights in, which is what keeps it relevant rather
                // than charitable.
                Definition->WeaponId = TEXT("Sidearm");
                Definition->DisplayName = FText::FromString(TEXT("Sidearm"));
                Definition->Damage = 21.0f;
                Definition->WeakPointMultiplier = 1.8f;
                Definition->RoundsPerMinute = 420.0f;
                Definition->bAutomatic = false;
                Definition->HipSpreadDegrees = 1.1f;
                Definition->AimSpreadDegrees = 0.30f;
                Definition->MagazineSize = 14;
                Definition->StartingReserveAmmo = 210;  // fifteen magazines
                Definition->ReloadDuration = 1.1f;
                Definition->FalloffStart = 1600.0f;
                Definition->FalloffEnd = 4000.0f;
                Definition->MinimumFalloffMultiplier = 0.52f;
                Definition->MaximumRange = 7000.0f;
                Definition->SwapInDuration = 0.18f;     // the whole point
                break;
            default:
                Definition->DisplayName = FText::FromString(TEXT("Rifle"));
                break;
            }
            Definition->Recoil = ArchetypeRecoilProfile(Archetype);
        }
        return Prototypes[Index];
    }
}

UBreakerWeaponComponent::UBreakerWeaponComponent()
{
    // Ticks only while recoil, bloom, or the viewmodel spring have work left;
    // UpdateFeelTickEnabled switches it off again the moment everything is at
    // rest, so an idle weapon still costs nothing.
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    SetIsReplicatedByDefault(true);
}

void UBreakerWeaponComponent::BeginPlay()
{
    Super::BeginPlay();
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        InitializeSlotAmmunition();
        MagazineAmmo = SlotOneMagazineAmmo;
        ReserveAmmo = SlotOneReserveAmmo;
    }
}

void UBreakerWeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UBreakerWeaponComponent, MagazineAmmo);
    DOREPLIFETIME(UBreakerWeaponComponent, ReserveAmmo);
    DOREPLIFETIME(UBreakerWeaponComponent, bReloading);
    DOREPLIFETIME(UBreakerWeaponComponent, CurrentArchetype);
    DOREPLIFETIME(UBreakerWeaponComponent, CurrentSlot);
    DOREPLIFETIME(UBreakerWeaponComponent, bSwapping);
    DOREPLIFETIME(UBreakerWeaponComponent, SlotOneArchetype);
    DOREPLIFETIME(UBreakerWeaponComponent, SlotTwoArchetype);
}

const UBreakerWeaponDefinition* UBreakerWeaponComponent::ResolveDefinition() const
{
    return WeaponDefinition ? WeaponDefinition.Get() : GetPrototypeDefinition(CurrentArchetype);
}

int32 UBreakerWeaponComponent::GetEquippedItemLevel() const
{
    // The loadout slot number and the equipment slot correspond POSITIONALLY:
    // weapon slot 1 is the Primary equipment slot, slot 2 the Secondary.
    // FBreakerItemInstance now also carries WeaponArchetype, so both halves of
    // "which gun am I holding" cross the boundary; see SyncArchetypesToEquipment.
    const AActor* Owner = GetOwner();
    const UBreakerEquipmentComponent* Equipment = Owner ? Owner->FindComponentByClass<UBreakerEquipmentComponent>() : nullptr;
    if (Equipment)
    {
        const EBreakerEquipSlot Slot = (CurrentSlot == 2) ? EBreakerEquipSlot::Secondary : EBreakerEquipSlot::Primary;
        FBreakerItemInstance Item;
        if (Equipment->GetEquippedItem(Slot, Item) && Item.IsValid())
        {
            return FMath::Max(1, Item.ItemLevel);
        }
    }
    // Nothing equipped: the archetype table's own level. A clean clone with no
    // loadout is a level-1 weapon, which is a scalar of exactly 1.0.
    return FMath::Max(1, UnequippedItemLevel);
}

float UBreakerWeaponComponent::GetItemLevelDamageScalar() const
{
    return FBreakerWeaponMath::ItemLevelDamageScalar(GetEquippedItemLevel(), ItemLevelDamageGrowth);
}

float UBreakerWeaponComponent::GetScaledBaseDamage() const
{
    const UBreakerWeaponDefinition* Definition = ResolveDefinition();
    if (!Definition) return 0.0f;
    return FBreakerWeaponMath::WeaponBaseDamage(Definition->Damage, GetEquippedItemLevel(), ItemLevelDamageGrowth);
}

float UBreakerWeaponComponent::GetScaledFullBlastDamage() const
{
    // Per-pellet scaled base times the pellet count — see the declaration for
    // the per-archetype table and the burst-rifle per-round ruling. The clamp
    // mirrors the definition's own ClampMin=1 so a hand-built definition with
    // an unserialized pellet count still reads as one pellet.
    const UBreakerWeaponDefinition* Definition = ResolveDefinition();
    if (!Definition) return 0.0f;
    return GetScaledBaseDamage() * FMath::Max(1, Definition->PelletsPerShot);
}

FBreakerRecoilProfile UBreakerWeaponComponent::ResolveRecoilProfile() const
{
    // Component override (editor-tunable per instance, no recompile) beats the
    // definition asset, which beats the archetype fallback table.
    FBreakerRecoilProfile Profile;
    if (const FBreakerRecoilProfile* Override = RecoilOverrides.Find(CurrentArchetype))
    {
        Profile = *Override;
    }
    else if (const UBreakerWeaponDefinition* Definition = ResolveDefinition())
    {
        Profile = Definition->Recoil;
    }
    else
    {
        Profile = ArchetypeRecoilProfile(CurrentArchetype);
    }

    // A single global trim over aim kick only. Recovery, bloom, and viewmodel
    // keep their authored values so scaling the kick cannot desynchronise the
    // settle from the climb.
    const float Scale = FMath::Max(0.0f, RecoilScale);
    if (!FMath::IsNearlyEqual(Scale, 1.0f))
    {
        Profile.VerticalKickDegrees *= Scale;
        Profile.HorizontalKickDegrees *= Scale;
        Profile.HorizontalRandomDegrees *= Scale;
    }
    return Profile;
}

float UBreakerWeaponComponent::GetAimAlpha() const
{
    if (!bAiming) return 0.0f;
    const float AimIn = ResolveRecoilProfile().AimInSeconds;
    if (AimIn <= 0.0f || !GetWorld()) return 1.0f;
    return FMath::Clamp(static_cast<float>(GetWorld()->GetTimeSeconds() - AimStartTime) / AimIn, 0.0f, 1.0f);
}

float UBreakerWeaponComponent::GetSpeedFraction() const
{
    if (!GetOwner() || MoveSpreadReferenceSpeed <= 0.0f) return 0.0f;
    return FMath::Clamp(static_cast<float>(GetOwner()->GetVelocity().Size2D()) / MoveSpreadReferenceSpeed, 0.0f, 1.0f);
}

float UBreakerWeaponComponent::GetAimMoveSpeedMultiplier() const
{
    // Composed against LIVE aim progress, not the aim button, so the penalty
    // arrives at exactly the pace every other ADS benefit does. Nothing reads
    // this yet — see the gap note on the declaration for the one function in
    // Movement/ that has to.
    return FBreakerWeaponFeel::AimMoveSpeedMultiplier(ResolveRecoilProfile(), GetAimAlpha());
}

float UBreakerWeaponComponent::GetMovementSpreadDegrees() const
{
    return FBreakerWeaponFeel::MovementSpreadDegrees(ResolveRecoilProfile(), GetSpeedFraction(), GetAimAlpha());
}

float UBreakerWeaponComponent::GetNextShotSpreadDegrees() const
{
    const UBreakerWeaponDefinition* Definition = ResolveDefinition();
    if (!Definition) return 0.0f;
    const float Alpha = GetAimAlpha();
    // Partway into ADS is partway to the aimed cone, not the whole thing.
    const float BaseSpread = FMath::Lerp(Definition->HipSpreadDegrees, Definition->AimSpreadDegrees, Alpha);
    const FBreakerRecoilProfile Profile = ResolveRecoilProfile();
    const float Movement = FBreakerWeaponFeel::MovementSpreadDegrees(Profile, GetSpeedFraction(), Alpha);
    return FBreakerWeaponFeel::EffectiveSpreadDegrees(Profile, BaseSpread, BloomDegrees, BurstShotIndex, Movement);
}

FVector UBreakerWeaponComponent::GetViewmodelLocationOffset() const
{
    // -X is toward the player: the weapon is driven back into the shoulder.
    return FVector(-Viewmodel.BackOffset, Viewmodel.LateralOffset, 0.0f);
}

FRotator UBreakerWeaponComponent::GetViewmodelRotationOffset() const
{
    // Positive pitch on a camera-relative component points the muzzle up.
    return FRotator(Viewmodel.PitchOffset, 0.0f, 0.0f);
}

void UBreakerWeaponComponent::ResetWeaponFeel()
{
    RecoilPitchAccumulated = 0.0f;
    RecoilYawAccumulated = 0.0f;
    RecoveryDelayRemaining = 0.0f;
    BloomDegrees = 0.0f;
    BurstShotIndex = 0;
    bHasAppliedControlRotation = false;
    Viewmodel = FBreakerViewmodelState();
    UpdateFeelTickEnabled();
}

void UBreakerWeaponComponent::PushRangeTreatmentOverride(FName Key, float Duration)
{
    if (Key.IsNone()) return;
    FRangeTreatmentOverrideEntry Entry;
    const UWorld* World = GetWorld();
    Entry.ExpiryTime = (Duration > 0.0f && World) ? World->GetTimeSeconds() + Duration : -1.0;
    // Re-pushing the same key replaces rather than stacks, matching PushSpeedMultiplier.
    RangeTreatmentOverrides.Add(Key, Entry);
}

void UBreakerWeaponComponent::PopRangeTreatmentOverride(FName Key)
{
    RangeTreatmentOverrides.Remove(Key);
}

void UBreakerWeaponComponent::PruneRangeTreatmentOverrides() const
{
    const UWorld* World = GetWorld();
    if (!World || RangeTreatmentOverrides.Num() == 0) return;
    const double Now = World->GetTimeSeconds();
    for (auto It = RangeTreatmentOverrides.CreateIterator(); It; ++It)
    {
        if (It.Value().ExpiryTime >= 0.0 && It.Value().ExpiryTime <= Now)
        {
            It.RemoveCurrent();
        }
    }
}

bool UBreakerWeaponComponent::IsRangeTreatmentOverridden() const
{
    PruneRangeTreatmentOverrides();
    return RangeTreatmentOverrides.Num() > 0;
}

void UBreakerWeaponComponent::UpdateFeelTickEnabled()
{
    const bool bBusy = RecoilPitchAccumulated != 0.0f || RecoilYawAccumulated != 0.0f
        || BloomDegrees > 0.0f || !Viewmodel.IsAtRest();
    SetComponentTickEnabled(bBusy);
}

void UBreakerWeaponComponent::ApplyShotFeel(const FBreakerShotResult& Shot)
{
    // The shot carries the ADS progress it was fired at, so a round loosed
    // halfway into the sights kicks halfway between hip and aimed — on every
    // machine, not just the shooter's.
    const FBreakerRecoilProfile Profile = FBreakerWeaponFeel::ProfileAtAimAlpha(ResolveRecoilProfile(), Shot.AimAlpha);
    const bool bAimedKick = Shot.AimAlpha > 0.0f;
    const FBreakerRecoilKick Kick = FBreakerWeaponFeel::ComputeShotKick(Profile, Shot.BurstShotIndex, Shot.RecoilSeed, bAimedKick);

    if (bViewmodelKickEnabled)
    {
        FBreakerWeaponFeel::AddViewmodelKick(Profile, Viewmodel, Kick.YawDegrees, bAimedKick);
    }

    // Recoil moves the aim, and only the aim, and only for the player who is
    // actually looking through this weapon.
    APawn* Pawn = Cast<APawn>(GetOwner());
    AController* OwningController = Pawn ? Pawn->GetController() : nullptr;
    if (bRecoilEnabled && Pawn && OwningController && Pawn->IsLocallyControlled())
    {
        const FBreakerRecoilKick Applied = FBreakerWeaponFeel::AccumulateKick(Profile, Kick, RecoilPitchAccumulated, RecoilYawAccumulated);

        const FRotator Current = OwningController->GetControlRotation();
        const float CurrentPitch = FRotator::NormalizeAxis(Current.Pitch);
        // Never let the kick drive the view through vertical; give back to the
        // settle budget whatever the clamp refused, so recovery stays exact.
        const float ClampedPitch = FMath::Clamp(CurrentPitch + Applied.PitchDegrees, -89.0f, 89.0f);
        const float ActualPitchDelta = ClampedPitch - CurrentPitch;
        RecoilPitchAccumulated -= (Applied.PitchDegrees - ActualPitchDelta);

        FRotator Kicked = Current;
        Kicked.Pitch = ClampedPitch;
        Kicked.Yaw = Current.Yaw + Applied.YawDegrees;
        OwningController->SetControlRotation(Kicked);
        LastAppliedControlRotation = OwningController->GetControlRotation();
        bHasAppliedControlRotation = true;
        RecoveryDelayRemaining = FMath::Max(RecoveryDelayRemaining, Profile.RecoveryDelaySeconds);
    }

    UpdateFeelTickEnabled();
}

void UBreakerWeaponComponent::TickComponent(float DeltaSeconds, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaSeconds, TickType, ThisTickFunction);
    TickRecoil(DeltaSeconds);
}

void UBreakerWeaponComponent::TickRecoil(float DeltaSeconds)
{
    if (DeltaSeconds <= 0.0f) return;
    const FBreakerRecoilProfile Profile = ResolveRecoilProfile();

    BloomDegrees = FBreakerWeaponFeel::BloomAfterTime(Profile, BloomDegrees, DeltaSeconds);
    FBreakerWeaponFeel::IntegrateViewmodel(Profile, Viewmodel, DeltaSeconds);

    APawn* Pawn = Cast<APawn>(GetOwner());
    AController* OwningController = Pawn ? Pawn->GetController() : nullptr;
    if (OwningController && Pawn->IsLocallyControlled() && (RecoilPitchAccumulated != 0.0f || RecoilYawAccumulated != 0.0f))
    {
        FRotator Current = OwningController->GetControlRotation();
        if (bHasAppliedControlRotation)
        {
            // Aim movement that opposes the kick is the player compensating.
            // Spend the settle budget on it rather than shoving the view the
            // same distance again once the burst ends.
            const float PlayerPitchDelta = FRotator::NormalizeAxis(Current.Pitch - LastAppliedControlRotation.Pitch);
            const float PlayerYawDelta = FRotator::NormalizeAxis(Current.Yaw - LastAppliedControlRotation.Yaw);
            RecoilPitchAccumulated = FBreakerWeaponFeel::ConsumeCompensation(RecoilPitchAccumulated, PlayerPitchDelta);
            RecoilYawAccumulated = FBreakerWeaponFeel::ConsumeCompensation(RecoilYawAccumulated, PlayerYawDelta);
        }

        if (RecoveryDelayRemaining > 0.0f)
        {
            RecoveryDelayRemaining = FMath::Max(0.0f, RecoveryDelayRemaining - DeltaSeconds);
        }
        else
        {
            const float NewPitch = FBreakerWeaponFeel::RecoverAxis(Profile, RecoilPitchAccumulated, DeltaSeconds);
            const float NewYaw = FBreakerWeaponFeel::RecoverAxis(Profile, RecoilYawAccumulated, DeltaSeconds);
            Current.Pitch = FRotator::NormalizeAxis(Current.Pitch) + (NewPitch - RecoilPitchAccumulated);
            Current.Yaw = Current.Yaw + (NewYaw - RecoilYawAccumulated);
            RecoilPitchAccumulated = NewPitch;
            RecoilYawAccumulated = NewYaw;
            OwningController->SetControlRotation(Current);
        }
        LastAppliedControlRotation = OwningController->GetControlRotation();
        bHasAppliedControlRotation = true;
    }
    else if (!OwningController)
    {
        // No aim to move: drop the budget rather than banking a kick that
        // would be handed back the next time a controller appears.
        RecoilPitchAccumulated = 0.0f;
        RecoilYawAccumulated = 0.0f;
    }

    UpdateFeelTickEnabled();
}

void UBreakerWeaponComponent::EquipArchetype(EBreakerWeaponArchetype NewArchetype)
{
    if (CurrentArchetype == NewArchetype) return;
    StopFire();
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(ReloadTimer);
    CurrentArchetype = NewArchetype;
    bReloading = false;
    // A different weapon starts its pattern from zero.
    BurstShotIndex = 0;
    BloomDegrees = 0.0f;
    const UBreakerWeaponDefinition* Definition = ResolveDefinition();
    MagazineAmmo = Definition ? Definition->MagazineSize : 0;
    ReserveAmmo = Definition ? Definition->StartingReserveAmmo : 0;
    OnReloadChanged.Broadcast(false);
    OnAmmoChanged.Broadcast(MagazineAmmo, ReserveAmmo);
}

void UBreakerWeaponComponent::InitializeSlotAmmunition()
{
    if (SlotOneMagazineAmmo < 0)
    {
        const UBreakerWeaponDefinition* SlotOne = GetPrototypeDefinition(SlotOneArchetype);
        SlotOneMagazineAmmo = SlotOne->MagazineSize;
        SlotOneReserveAmmo = SlotOne->StartingReserveAmmo;
    }
    if (SlotTwoMagazineAmmo < 0)
    {
        const UBreakerWeaponDefinition* SlotTwo = GetPrototypeDefinition(SlotTwoArchetype);
        SlotTwoMagazineAmmo = SlotTwo->MagazineSize;
        SlotTwoReserveAmmo = SlotTwo->StartingReserveAmmo;
    }
}

void UBreakerWeaponComponent::StoreActiveSlotAmmunition()
{
    if (CurrentSlot == 1)
    {
        SlotOneMagazineAmmo = MagazineAmmo;
        SlotOneReserveAmmo = ReserveAmmo;
    }
    else
    {
        SlotTwoMagazineAmmo = MagazineAmmo;
        SlotTwoReserveAmmo = ReserveAmmo;
    }
}

void UBreakerWeaponComponent::EquipSlot(int32 SlotNumber)
{
    SlotNumber = FMath::Clamp(SlotNumber, 1, 2);
    if (GetOwner() && !GetOwner()->HasAuthority())
    {
        ServerEquipSlot(SlotNumber);
        return;
    }
    if (CurrentSlot == SlotNumber) return;

    StopFire();
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(ReloadTimer);
    InitializeSlotAmmunition();
    StoreActiveSlotAmmunition();
    CurrentSlot = SlotNumber;
    CurrentArchetype = CurrentSlot == 1 ? SlotOneArchetype : SlotTwoArchetype;
    MagazineAmmo = CurrentSlot == 1 ? SlotOneMagazineAmmo : SlotTwoMagazineAmmo;
    ReserveAmmo = CurrentSlot == 1 ? SlotOneReserveAmmo : SlotTwoReserveAmmo;
    bReloading = false;
    // The incoming weapon starts its pattern from zero; any kick still in the
    // air keeps settling, because the aim it moved is still the player's aim.
    BurstShotIndex = 0;
    BloomDegrees = 0.0f;
    OnReloadChanged.Broadcast(false);
    OnAmmoChanged.Broadcast(MagazineAmmo, ReserveAmmo);

    // Swap tempo: the incoming weapon is unusable for its SwapInDuration.
    // Swap speed affixes will scale this window; on-swap-in damage windows
    // read GetSecondsSinceSwapIn once it closes.
    const UBreakerWeaponDefinition* Incoming = ResolveDefinition();
    const float SwapDuration = Incoming ? Incoming->SwapInDuration : 0.5f;
    bSwapping = true;
    OnSwapChanged.Broadcast(true, CurrentSlot);
    if (SwapDuration > 0.0f && GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(SwapTimer, this, &ThisClass::FinishSwap, SwapDuration, false);
    }
    else
    {
        FinishSwap();
    }
}

void UBreakerWeaponComponent::FinishSwap()
{
    bSwapping = false;
    LastSwapInTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    OnSwapChanged.Broadcast(false, CurrentSlot);
}

float UBreakerWeaponComponent::GetSecondsSinceSwapIn() const
{
    return GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds() - LastSwapInTime) : BIG_NUMBER;
}

void UBreakerWeaponComponent::ServerEquipSlot_Implementation(int32 SlotNumber)
{
    EquipSlot(SlotNumber);
}

void UBreakerWeaponComponent::SyncArchetypesToEquipment()
{
    // Equipping a weapon ITEM is what arms its archetype. Before this, a
    // dropped Primary supplied an item level and a list of affixes while the
    // loadout screen independently decided which gun you were holding, so a
    // shotgun drop and a sniper drop were the same object wearing different
    // numbers.
    //
    // The loadout screen still works and still writes SetSlotArchetype; this
    // simply means an equipped item OVERRIDES it, which is the direction the
    // owner asked for ("we can keep the loadout system for now but in the
    // future thats what its supposed to be"). An empty slot changes nothing,
    // so a player with no weapon items keeps whatever they picked.
    const AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority()) return;
    const UBreakerEquipmentComponent* Equipment = Owner->FindComponentByClass<UBreakerEquipmentComponent>();
    if (!Equipment) return;

    const EBreakerEquipSlot Slots[2] = { EBreakerEquipSlot::Primary, EBreakerEquipSlot::Secondary };
    for (int32 Index = 0; Index < 2; ++Index)
    {
        FBreakerItemInstance Item;
        if (!Equipment->GetEquippedItem(Slots[Index], Item) || !Item.IsValid()) continue;
        // SetSlotArchetype early-outs when the archetype is unchanged, so this
        // is safe to call every time equipment changes: it will not reset a
        // magazine the player is halfway through.
        SetSlotArchetype(Index + 1, Item.WeaponArchetype);
    }
}

void UBreakerWeaponComponent::SetSlotArchetype(int32 SlotNumber, EBreakerWeaponArchetype NewArchetype)
{
    SlotNumber = FMath::Clamp(SlotNumber, 1, 2);
    if (GetOwner() && !GetOwner()->HasAuthority())
    {
        ServerSetSlotArchetype(SlotNumber, NewArchetype);
        return;
    }
    if (GetSlotArchetype(SlotNumber) == NewArchetype) return;

    const UBreakerWeaponDefinition* Definition = GetPrototypeDefinition(NewArchetype);
    if (SlotNumber == 1)
    {
        SlotOneArchetype = NewArchetype;
        SlotOneMagazineAmmo = Definition->MagazineSize;
        SlotOneReserveAmmo = Definition->StartingReserveAmmo;
    }
    else
    {
        SlotTwoArchetype = NewArchetype;
        SlotTwoMagazineAmmo = Definition->MagazineSize;
        SlotTwoReserveAmmo = Definition->StartingReserveAmmo;
    }
    if (CurrentSlot == SlotNumber)
    {
        StopFire();
        if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(ReloadTimer);
        bReloading = false;
        CurrentArchetype = NewArchetype;
        BurstShotIndex = 0;
        BloomDegrees = 0.0f;
        MagazineAmmo = Definition->MagazineSize;
        ReserveAmmo = Definition->StartingReserveAmmo;
        OnReloadChanged.Broadcast(false);
        OnAmmoChanged.Broadcast(MagazineAmmo, ReserveAmmo);
    }
}

void UBreakerWeaponComponent::ServerSetSlotArchetype_Implementation(int32 SlotNumber, EBreakerWeaponArchetype NewArchetype)
{
    SetSlotArchetype(SlotNumber, NewArchetype);
}

float UBreakerWeaponComponent::GetFireRateMultiplier() const
{
    // Read live rather than cached: the player equips and unequips mid-fight,
    // and a cached cadence would keep firing at the old gun's rate until
    // something happened to invalidate it.
    const AActor* Owner = GetOwner();
    if (!Owner) return 1.0f;
    if (const IAbilitySystemInterface* AbilityOwner = Cast<const IAbilitySystemInterface>(Owner))
    {
        if (const UAbilitySystemComponent* ASC = AbilityOwner->GetAbilitySystemComponent())
        {
            if (const UBreakerAttributeSet* Attributes = ASC->GetSet<UBreakerAttributeSet>())
            {
                // Floored for the same reason PreAttributeChange floors it: a
                // multiplier at zero turns the fire interval into an infinity,
                // which hangs the weapon rather than slowing it.
                return FMath::Max(0.05f, Attributes->GetFireRateMultiplier());
            }
        }
    }
    return 1.0f;
}

// Effective rounds per minute: the archetype's authored cadence times whatever
// the Fire Rate affix and any future tree node have composed. Every fire-timing
// call site goes through here, so a cadence stat can never apply to some of
// them and not others -- which is exactly how a "50% fire rate" line ends up
// making a weapon fire faster but reload-lock at the old rate.
float UBreakerWeaponComponent::GetEffectiveRoundsPerMinute(const UBreakerWeaponDefinition* Definition) const
{
    if (!Definition) return 0.0f;
    return Definition->RoundsPerMinute * GetFireRateMultiplier();
}

FString UBreakerWeaponComponent::GetArchetypeName() const
{
    // One name table, in BreakerWeaponArchetype.h, shared with dropped items
    // and the loadout screen. A gun named in three places gets renamed in two.
    return BreakerWeaponArchetypeNames::Short(CurrentArchetype);
}

void UBreakerWeaponComponent::StartFire()
{
    if (!GetOwner()) return;
    if (!GetOwner()->HasAuthority())
    {
        ServerStartFire();
        return;
    }
    bTriggerHeld = true;
    RoundsInFireBurst = 0;
    const UBreakerWeaponDefinition* Definition = ResolveDefinition();
    const bool bBurstWeapon = Definition && Definition->ShotsPerBurst > 1;
    const bool bFired = FireOnce();
    if (bFired && bBurstWeapon) ++RoundsInFireBurst;

    if (!Definition || !Definition->bAutomatic) return;
    if (bBurstWeapon)
    {
        // A one-shot chain, because the interval alternates. Non-burst weapons
        // keep the repeating timer below untouched: a repeating timer holds
        // exact cadence, while a chain re-arms from the callback and would shed
        // a callback's latency off every shot.
        ScheduleBurstFire(RoundsInFireBurst >= Definition->ShotsPerBurst
            ? FMath::Max(Definition->BurstCycleSeconds, FBreakerWeaponMath::FireInterval(GetEffectiveRoundsPerMinute(Definition)))
            : FBreakerWeaponMath::FireInterval(GetEffectiveRoundsPerMinute(Definition)));
        return;
    }
    GetWorld()->GetTimerManager().SetTimer(AutomaticFireTimer, this, &ThisClass::FireOnceTimer,
        FBreakerWeaponMath::FireInterval(GetEffectiveRoundsPerMinute(Definition)), true);
}

void UBreakerWeaponComponent::ScheduleBurstFire(float DelaySeconds)
{
    if (!GetWorld()) return;
    GetWorld()->GetTimerManager().SetTimer(AutomaticFireTimer, this, &ThisClass::AdvanceBurstFire,
        FMath::Max(DelaySeconds, UE_KINDA_SMALL_NUMBER), false);
}

void UBreakerWeaponComponent::AdvanceBurstFire()
{
    const UBreakerWeaponDefinition* Definition = ResolveDefinition();
    if (!bTriggerHeld || !Definition || Definition->ShotsPerBurst <= 1) return;

    // A burst that ran out of magazine, or was interrupted by a reload or a
    // swap, starts a fresh burst rather than resuming a stale one — otherwise
    // reloading mid-burst would hand the player a one-round burst.
    if (RoundsInFireBurst >= Definition->ShotsPerBurst) RoundsInFireBurst = 0;

    const bool bFired = FireOnce();
    if (bFired) ++RoundsInFireBurst;

    const float Interval = FBreakerWeaponMath::FireInterval(GetEffectiveRoundsPerMinute(Definition));
    // Not firing (reloading, swapping, dry) re-checks at the fire interval
    // rather than giving up, so the trigger stays live across a reload.
    if (!bFired) { ScheduleBurstFire(Interval); return; }
    ScheduleBurstFire(RoundsInFireBurst >= Definition->ShotsPerBurst
        ? FMath::Max(Definition->BurstCycleSeconds, Interval)
        : Interval);
}

void UBreakerWeaponComponent::FireOnceTimer()
{
    FireOnce();
}

void UBreakerWeaponComponent::StopFire()
{
    if (!GetOwner()) return;
    if (!GetOwner()->HasAuthority())
    {
        ServerStopFire();
        return;
    }
    bTriggerHeld = false;
    // Releasing the trigger ends the burst. A burst weapon must not resume a
    // half-finished burst on the next pull: the first round of every pull is
    // shot 0 of a fresh burst, which is what makes the pattern learnable.
    RoundsInFireBurst = 0;
    // Guarded like every other timer touch in this component: an owned but
    // worldless component (a test fixture, a CDO) has no timer manager, and
    // StopFire was the one place that assumed it did.
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(AutomaticFireTimer);
}

void UBreakerWeaponComponent::StartReload()
{
    if (!GetOwner()) return;
    if (!GetOwner()->HasAuthority())
    {
        ServerStartReload();
        return;
    }
    const UBreakerWeaponDefinition* Definition = ResolveDefinition();
    if (!Definition || bReloading || bSwapping || MagazineAmmo >= GetEffectiveMagazineSize() || ReserveAmmo <= 0) return;
    StopFire();
    bReloading = true;
    OnReloadChanged.Broadcast(true);
    GetWorld()->GetTimerManager().SetTimer(ReloadTimer, this, &ThisClass::FinishReload, Definition->ReloadDuration, false);
}

void UBreakerWeaponComponent::SetAimingInternal(bool bNewAiming)
{
    // Only a fresh press restarts the ramp; re-asserting an aim already held
    // must not hand the player a second aim-in window.
    if (bNewAiming && !bAiming)
    {
        AimStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    }
    bAiming = bNewAiming;
}

void UBreakerWeaponComponent::SetAiming(bool bNewAiming)
{
    SetAimingInternal(bNewAiming);
    if (GetOwner() && !GetOwner()->HasAuthority())
    {
        ServerSetAiming(bNewAiming);
    }
}

void UBreakerWeaponComponent::ServerStartFire_Implementation() { StartFire(); }
void UBreakerWeaponComponent::ServerStopFire_Implementation() { StopFire(); }
void UBreakerWeaponComponent::ServerStartReload_Implementation() { StartReload(); }
void UBreakerWeaponComponent::ServerSetAiming_Implementation(bool bNewAiming) { SetAimingInternal(bNewAiming); }

bool UBreakerWeaponComponent::CanFire() const
{
    const UBreakerWeaponDefinition* Definition = ResolveDefinition();
    if (!Definition || bReloading || bSwapping || MagazineAmmo <= 0 || !GetWorld()) return false;
    return GetWorld()->GetTimeSeconds() - LastShotTime + UE_KINDA_SMALL_NUMBER >= FBreakerWeaponMath::FireInterval(GetEffectiveRoundsPerMinute(Definition));
}

bool UBreakerWeaponComponent::FireOnce()
{
    if (!CanFire()) return false;
    const UBreakerWeaponDefinition* Definition = ResolveDefinition();
    if (!Definition) return false;

    const FBreakerRecoilProfile RecoilProfile = ResolveRecoilProfile();
    // A burst is a run of shots with no meaningful gap. Let the trigger rest
    // and the weapon is dead accurate again, its pattern back at shot zero:
    // that is the whole reward for trigger discipline.
    const double IdleSeconds = GetWorld()->GetTimeSeconds() - LastShotTime;
    if (IdleSeconds > RecoilProfile.BurstResetSeconds)
    {
        BurstShotIndex = 0;
        BloomDegrees = 0.0f;
    }

    LastShotTime = GetWorld()->GetTimeSeconds();
    // Scrap's magazine-dump clause: the cycle counts as "started full" only
    // when this round left a genuinely full magazine (Class-Kits-Gunsmith
    // §1.1 — topping off at 1/30 and firing one round does not re-arm it).
    if (MagazineAmmo >= GetEffectiveMagazineSize())
    {
        bFireCycleStartedFull = true;
    }
    --MagazineAmmo;
    OnAmmoChanged.Broadcast(MagazineAmmo, ReserveAmmo);
    if (MagazineAmmo <= 0)
    {
        // On the LAST ROUND LEAVING the magazine — never on the reload.
        OnMagazineEmptied.Broadcast(bFireCycleStartedFull);
        bFireCycleStartedFull = false;
    }

    FVector ViewLocation;
    FRotator ViewRotation;
    GetViewPoint(ViewLocation, ViewRotation);
    // ADS tightens the cone twice over: the definition's aimed spread is the
    // floor, and bloom grows more slowly on top of it. Both are now bought
    // over AimInSeconds rather than granted on the button down, and both are
    // paid for in movement — a moving aimed shot is wider than a moving hip
    // shot, which is the whole reason to ever leave the sights.
    const float ShotAimAlpha = GetAimAlpha();
    const FBreakerRecoilProfile AimedProfile = FBreakerWeaponFeel::ProfileAtAimAlpha(RecoilProfile, ShotAimAlpha);
    const float BaseSpread = FMath::Lerp(Definition->HipSpreadDegrees, Definition->AimSpreadDegrees, ShotAimAlpha);
    const float MovementSpread = FBreakerWeaponFeel::MovementSpreadDegrees(AimedProfile, GetSpeedFraction(), ShotAimAlpha);
    const float Spread = FBreakerWeaponFeel::EffectiveSpreadDegrees(AimedProfile, BaseSpread, BloomDegrees, BurstShotIndex, MovementSpread);

    // Recoil state for this shot, resolved before the pellets so the cosmetic
    // event can carry it to every machine and they all kick identically.
    const int32 FiredBurstIndex = BurstShotIndex;
    const int32 RecoilSeed = static_cast<int32>(HashCombine(GetTypeHash(GetOwner()), static_cast<uint32>(ShotSequence + 1)));
    ++BurstShotIndex;
    BloomDegrees = FBreakerWeaponFeel::BloomAfterShot(AimedProfile, BloomDegrees, ShotAimAlpha > 0.0f);
    UpdateFeelTickEnabled();

    if (Definition->bProjectile)
    {
        FireProjectile(Definition, ViewLocation, ViewRotation, Spread, FiredBurstIndex, RecoilSeed, ShotAimAlpha);
        if (MagazineAmmo <= 0 && ReserveAmmo > 0) StartReload();
        return true;
    }

    FBreakerShotResult Shot;
    Shot.bFired = true;
    Shot.BurstShotIndex = FiredBurstIndex;
    Shot.RecoilSeed = RecoilSeed;
    Shot.bAimedShot = bAiming;
    Shot.AimAlpha = ShotAimAlpha;
    Shot.TraceStart = ViewLocation;

    // Lead's mark, resolved once per shot rather than once per pellet: the mark
    // cannot change between the pellets of a single trigger pull. A mark with
    // no remaining time reads as no mark at all.
    const UBreakerAbilityStateComponent* AbilityState = GetOwner() ? GetOwner()->FindComponentByClass<UBreakerAbilityStateComponent>() : nullptr;
    const AActor* MarkedTarget = (AbilityState && AbilityState->GetMarkRemaining() > 0.0f) ? AbilityState->GetMarkedTarget() : nullptr;
    const float LeadMinimumRangeCm = UBreakerAbility_Lead::DefaultMinimumRangeCm();

    // Item level, read once per trigger pull. Every pellet of a shotgun blast
    // and any bleed those pellets apply share this one reading, so a shot can
    // never straddle an equipment change.
    const float LevelScalar = GetItemLevelDamageScalar();
    const float ScaledBaseDamage = FMath::Max(0.0f, Definition->Damage) * LevelScalar;

    // ---- Projectile channels (owner ruling 2026-08-16) --------------------
    // Composed once per trigger pull, so every pellet of this shot fires with
    // one reading of the tree, the ability windows and the Momentum state.
    const FBreakerShotChannels Channels = GetShotChannels();
    // MULTISHOT: whole extra pellets fire now; the fraction banks across
    // pulls. Zero channels drain nothing and the accumulator stays untouched.
    const int32 ExtraPellets = FBreakerWeaponMath::ConsumeMultishot(Channels.AdditionalProjectiles, MultishotAccumulator);

    const UBreakerAttributeSet* SourceAttributes = nullptr;
    if (const IAbilitySystemInterface* AbilityOwner = Cast<IAbilitySystemInterface>(GetOwner()))
    {
        if (const UAbilitySystemComponent* ASC = AbilityOwner->GetAbilitySystemComponent()) SourceAttributes = ASC->GetSet<UBreakerAttributeSet>();
    }

    const int32 BasePelletCount = FMath::Max(1, Definition->PelletsPerShot);
    const int32 PelletCount = BasePelletCount + ExtraPellets;
    // One record per pellet, hits and misses alike. Reserved once: a spread is
    // a fixed size and this is on the fire path.
    Shot.Pellets.Reserve(PelletCount);
    int32 PiercedThisPull = 0;
    for (int32 PelletIndex = 0; PelletIndex < PelletCount; ++PelletIndex)
    {
        // DETERMINISM CONTRACT. Base pellets advance ShotSequence exactly as
        // they always have; extra multishot pellets draw from a salted
        // sub-stream keyed off the pull's final base sequence value and never
        // touch the counter. A build with the channels at zero therefore
        // produces a bit-identical spread/recoil/crit sequence to a build from
        // before the channels existed.
        const bool bExtraPellet = PelletIndex >= BasePelletCount;
        const int32 PelletSeed = bExtraPellet
            ? FBreakerWeaponMath::SecondaryShotSeed(GetTypeHash(GetOwner()), ShotSequence, BreakerMultishotSalt, PelletIndex - BasePelletCount)
            : ++ShotSequence;
        const FVector Direction = FBreakerWeaponMath::ApplyConeSpread(ViewRotation.Vector(), Spread, PelletSeed);
        const FVector PelletEnd = ViewLocation + Direction * Definition->MaximumRange;
        if (PelletIndex == 0) Shot.TraceEnd = PelletEnd;

        // Added BEFORE the resolution so that every code path below — miss,
        // continue, geometry with no combat component — still leaves exactly
        // one entry per pellet. A spread with a hole in it would silently drop
        // a tracer, which is the failure this whole change exists to remove.
        FBreakerPelletImpact& Pellet = Shot.Pellets.AddDefaulted_GetRef();
        Pellet.End = PelletEnd;

        // PIERCE / CHAIN / RICOCHET live inside the pellet's resolution; with
        // the channels at zero it performs exactly one trace and one damage
        // submission, the legacy path to the bit.
        PiercedThisPull += ResolvePelletImpacts(Definition, ViewLocation, Direction, Channels, ScaledBaseDamage,
            SourceAttributes, MarkedTarget, LeadMinimumRangeCm, LevelScalar, PelletSeed, Shot, Pellet);
    }

    // Pierce Discipline (Class-Kits §1.5 M6, transcribed): each target pierced
    // by a single shot generates +4 Momentum (R2: +7), capped at 3 targets "to
    // bound Multishot/Pierce interaction" — the cap is per trigger pull for
    // exactly that reason. Swift-gated twice over: the node is Swift-locked
    // and the momentum component is inert for every other class.
    if (PiercedThisPull > 0 && GetOwner())
    {
        UBreakerMomentumComponent* Momentum = GetOwner()->FindComponentByClass<UBreakerMomentumComponent>();
        const UBreakerProgressionComponent* Progression = GetOwner()->FindComponentByClass<UBreakerProgressionComponent>();
        const int32 DisciplineRank = Progression ? Progression->GetNodeRank(BreakerPierceDisciplineNodeId, EBreakerPointCurrency::ClassPoints) : 0;
        if (Momentum && Momentum->IsActiveForOwner() && DisciplineRank > 0)
        {
            const float PerTarget = DisciplineRank >= 2 ? 7.0f : 4.0f;   // Class-Kits §1.5 M6 R1/R2
            Momentum->GrantMomentum(static_cast<float>(FMath::Min(PiercedThisPull, 3)) * PerTarget);   // §1.5 M6 cap: 3
        }
    }
    MulticastShotCosmetics(Shot);

    if (MagazineAmmo <= 0 && ReserveAmmo > 0) StartReload();
    return true;
}

bool UBreakerWeaponComponent::ResolveWeakPointHit(const FHitResult& Hit, const FVector& RayOrigin, const FVector& RayDirection) const
{
    // The exact geometric hit always counts and costs nothing to check.
    if (Hit.GetComponent() && Hit.GetComponent()->ComponentHasTag(TEXT("WeakPoint"))) return true;
    if (WeakPointToleranceCm <= 0.0f) return false;

    const AActor* HitActor = Hit.GetActor();
    if (!HitActor) return false;

    // Only a round that already hit THIS actor may be upgraded. The halo is
    // forgiveness for imprecise aim on a target you hit, never a second chance
    // at a target you missed, and never a reason to reward shooting the wall
    // in front of an enemy's head.
    for (const UActorComponent* Component : HitActor->GetComponents())
    {
        const UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component);
        if (!Primitive || !Primitive->ComponentHasTag(TEXT("WeakPoint"))) continue;
        // A weak point whose collision is off (a corpse, a phase) is not a
        // weak point; the halo must not outlive the thing it surrounds.
        if (!Primitive->IsCollisionEnabled()) continue;
        if (FBreakerWeaponMath::IsWithinWeakPointTolerance(RayOrigin, RayDirection,
            Primitive->GetComponentLocation(), Primitive->Bounds.SphereRadius, WeakPointToleranceCm))
        {
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Projectile channels (owner ruling 2026-08-16): the composition, the momentum
// coupling table, and the pellet resolution loop.
// ---------------------------------------------------------------------------

FBreakerShotChannels UBreakerWeaponComponent::GetShotChannels() const
{
    FBreakerShotChannels Channels;
    const AActor* Owner = GetOwner();
    if (!Owner) return Channels;

    // 1. The tree's Flat lanes. Read live, like GetFireRateMultiplier: a
    // respec mid-fight changes the next shot, not the next equip. Pierce,
    // chain and ricochet floor to whole mechanics here (the enum comment on
    // ProjectileCount: the lane must round down rather than silently firing
    // 1.5 bullets); the projectile fraction stays fractional because the
    // accumulator makes it perceptible instead of lost.
    if (const UBreakerProgressionComponent* Progression = Owner->FindComponentByClass<UBreakerProgressionComponent>())
    {
        const FBreakerNodeStats& Stats = Progression->GetNodeStats();
        Channels.AdditionalProjectiles += FMath::Max(0.0f, Stats.BonusProjectileCount);
        Channels.PierceCount += FMath::Max(0, FMath::FloorToInt32(Stats.BonusPierceCount));
        Channels.ChainCount += FMath::Max(0, FMath::FloorToInt32(Stats.BonusChainCount));
        Channels.RicochetCount += FMath::Max(0, FMath::FloorToInt32(Stats.BonusRicochetCount));
    }

    // 2. Keyed ability-window pushes (Sidearm Rig's +1 Pierce is the first).
    PruneShotChannelBonuses();
    for (const TPair<FName, FShotChannelBonusEntry>& Bonus : ShotChannelBonuses)
    {
        Channels += Bonus.Value.Channels;
    }

    // 3. Momentum manipulates projectiles — the Swift identity mechanic. The
    // gate is the momentum component's own IsActiveForOwner, which is already
    // Swift-locked, so a Caster (no momentum component, or an inert one left
    // by a dev class swap) fires exactly the shot it always did.
    if (const UBreakerMomentumComponent* Momentum = Owner->FindComponentByClass<UBreakerMomentumComponent>())
    {
        if (Momentum->IsActiveForOwner())
        {
            const FBreakerBuildConditionState Conditions = FBreakerBuildConditionState::EvaluateForActor(Owner);
            Channels += MomentumChannelBonus(Momentum->GetMomentumState(),
                Conditions.IsActive(EBreakerBuildCondition::Airborne),
                Conditions.IsActive(EBreakerBuildCondition::Sliding));
        }
    }

    // Floors, so a future negative-authored line can suppress but never
    // invert a mechanic into nonsense.
    Channels.AdditionalProjectiles = FMath::Max(0.0f, Channels.AdditionalProjectiles);
    Channels.PierceCount = FMath::Max(0, Channels.PierceCount);
    Channels.ChainCount = FMath::Max(0, Channels.ChainCount);
    Channels.RicochetCount = FMath::Max(0, Channels.RicochetCount);
    return Channels;
}

FBreakerShotChannels UBreakerWeaponComponent::MomentumChannelBonus(EBreakerMomentumState State, bool bAirborne, bool bSliding)
{
    // The coupling table (see the declaration comment for the design intent).
    // Every number is an O2 PLACEHOLDER chosen to be PERCEPTIBLE: a whole
    // pierce, a whole chain, a whole airborne pellet — states the HUD already
    // shows as bands, answered by shots that visibly behave differently.
    FBreakerShotChannels Bonus;
    if (State >= EBreakerMomentumState::Running)
    {
        Bonus.PierceCount += 1;   // O2 PLACEHOLDER — Running: rounds punch through
        // Momentum in the bar is what arms the posture bonuses: the coupling
        // is Momentum manipulating projectiles, not airtime doing it for free.
        if (bAirborne) Bonus.AdditionalProjectiles += 1.0f;   // O2 PLACEHOLDER — airborne: the shot doubles
        else if (bSliding) Bonus.AdditionalProjectiles += 0.5f;   // O2 PLACEHOLDER — sliding: a second pellet every other shot
    }
    if (State == EBreakerMomentumState::Redline)
    {
        Bonus.ChainCount += 1;   // O2 PLACEHOLDER — Redline: hits arc onward
    }
    return Bonus;
}

void UBreakerWeaponComponent::PushShotChannelBonus(FName Key, float AdditionalProjectiles, int32 PierceBonus, int32 ChainBonus, int32 RicochetBonus, float Duration)
{
    if (Key.IsNone()) return;
    FShotChannelBonusEntry Entry;
    Entry.Channels.AdditionalProjectiles = AdditionalProjectiles;
    Entry.Channels.PierceCount = PierceBonus;
    Entry.Channels.ChainCount = ChainBonus;
    Entry.Channels.RicochetCount = RicochetBonus;
    const UWorld* World = GetWorld();
    Entry.ExpiryTime = (Duration > 0.0f && World) ? World->GetTimeSeconds() + Duration : -1.0;
    // Re-pushing the same key replaces rather than stacks, matching
    // PushRangeTreatmentOverride and PushSpeedMultiplier.
    ShotChannelBonuses.Add(Key, Entry);
}

void UBreakerWeaponComponent::PopShotChannelBonus(FName Key)
{
    ShotChannelBonuses.Remove(Key);
}

void UBreakerWeaponComponent::PruneShotChannelBonuses() const
{
    const UWorld* World = GetWorld();
    if (!World || ShotChannelBonuses.Num() == 0) return;
    const double Now = World->GetTimeSeconds();
    for (auto It = ShotChannelBonuses.CreateIterator(); It; ++It)
    {
        if (It.Value().ExpiryTime >= 0.0 && It.Value().ExpiryTime <= Now)
        {
            It.RemoveCurrent();
        }
    }
}

AActor* UBreakerWeaponComponent::FindNearestChainTarget(const FVector& Origin, float RadiusCm, const TArray<const AActor*>& ExcludedActors) const
{
    // The enemy query behind chain arcs and ricochet seeks: nearest living
    // combat-component owner within the radius that this shot has not already
    // struck, with line of sight from the origin. Combat components are the
    // one honest register of "things weapon fire can damage" — enemies and
    // target dummies both carry one, and nothing else does.
    UWorld* World = GetWorld();
    if (!World || RadiusCm <= 0.0f) return nullptr;

    struct FCandidate { AActor* Actor = nullptr; double DistanceSquared = 0.0; };
    TArray<FCandidate> Candidates;
    const double RadiusSquared = FMath::Square(static_cast<double>(RadiusCm));
    for (TObjectIterator<UBreakerCombatComponent> It; It; ++It)
    {
        UBreakerCombatComponent* Combat = *It;
        AActor* Actor = Combat ? Combat->GetOwner() : nullptr;
        if (!Actor || Actor->GetWorld() != World || Actor == GetOwner()) continue;
        if (Combat->IsDead() || ExcludedActors.Contains(Actor)) continue;
        const double DistanceSquared = FVector::DistSquared(Origin, Actor->GetActorLocation());
        if (DistanceSquared > RadiusSquared) continue;
        Candidates.Add({ Actor, DistanceSquared });
    }
    Candidates.Sort([](const FCandidate& A, const FCandidate& B) { return A.DistanceSquared < B.DistanceSquared; });

    // Nearest first, but only through open air: an arc that passed through a
    // wall would read as a bug, not a mechanic.
    FCollisionQueryParams LineOfSight(SCENE_QUERY_STAT(BreakerChainSeek), true, GetOwner());
    for (const AActor* Excluded : ExcludedActors) LineOfSight.AddIgnoredActor(Excluded);
    for (const FCandidate& Candidate : Candidates)
    {
        FHitResult Hit;
        const bool bBlocked = World->LineTraceSingleByChannel(Hit, Origin, Candidate.Actor->GetActorLocation(), ECC_GameTraceChannel2, LineOfSight);
        if (!bBlocked || Hit.GetActor() == Candidate.Actor) return Candidate.Actor;
    }
    return nullptr;
}

FBreakerDamageResult UBreakerWeaponComponent::SubmitWeaponDamage(const UBreakerWeaponDefinition* Definition, UBreakerCombatComponent* TargetCombat,
    const UBreakerAttributeSet* SourceAttributes, float BaseDamage, float DistanceFromMuzzle, bool bWeakPoint,
    float ArmorPenetrationOverride, const FVector& ImpactPoint, int32 DamageSeed)
{
    FBreakerDamageRequest Damage;
    // The multiplicand: archetype base carried up the item-level curve, then
    // falloff. While a range-treatment override is active (Standing Wave's
    // Overdrive rewrite), the falloff computation itself is short-circuited
    // to 1.0 — every other term here is untouched.
    const float FalloffMultiplier = IsRangeTreatmentOverridden()
        ? 1.0f
        : FBreakerWeaponMath::DamageMultiplierAtDistance(Definition, DistanceFromMuzzle);
    Damage.BaseDamage = BaseDamage * FalloffMultiplier;
    Damage.DamageFamily = EBreakerDamageFamily::Physical;
    Damage.WeakPointMultiplier = Definition->WeakPointMultiplier;
    Damage.ArmorPenetration = ArmorPenetrationOverride;
    Damage.bWeakPointHit = bWeakPoint;
    Damage.CriticalChance = SourceAttributes ? SourceAttributes->GetCriticalChance() : UBreakerAttributeSet::DefaultCriticalChance;
    Damage.CriticalMultiplier = SourceAttributes ? SourceAttributes->GetCriticalMultiplier() : UBreakerAttributeSet::DefaultCriticalMultiplier;
    // ONE number. Gear's Weapon Damage affix and every skill node that raises
    // damage are already summed into the DamageMultiplier attribute's single
    // additive Increased bucket; multiplying gear in separately here is what
    // used to break the locked rule.
    Damage.SourceDamageMultiplier = SourceAttributes ? SourceAttributes->GetDamageMultiplier() : 1.0f;
    // STAGE 6: the source split, alongside the composed value it re-derives.
    // ReceiveDamage needs the Increased bucket and the More product SEPARATELY
    // to let a target-conditional rider join the additive bucket instead of
    // multiplying (Hook-And-Condition-Vocabulary §3.3). The More half is the
    // aggregator's post-clamp product; the Increased half is derived by
    // division so (1 + Increased/100) x More == SourceDamageMultiplier holds
    // exactly and a request with no satisfied rider recomposes to the same
    // number it carried in. With no attribute set the defaults (0% / x1.0)
    // are already the truthful split of the 1.0 composed value.
    Damage.bHasSourceSplit = true;
    if (SourceAttributes)
    {
        Damage.SourceMoreProduct = SourceAttributes->GetAttributeAggregator().ComposedMoreProduct(EBreakerAggregatedAttribute::DamageMultiplier);
        Damage.SourceIncreasedPercent = (Damage.SourceDamageMultiplier / FMath::Max(Damage.SourceMoreProduct, UE_SMALL_NUMBER) - 1.0f) * 100.0f;
    }
    Damage.RandomSeed = DamageSeed;
    Damage.SourceLocation = GetOwner()->GetActorLocation();
    Damage.bHasSourceLocation = true;
    // The traced impact point, so the damage number draws where this hit
    // actually landed instead of at the enemy's pivot.
    Damage.ImpactLocation = ImpactPoint;
    Damage.bHasImpactLocation = true;
    Damage.SetInstigator(GetOwner());
    // Outgoing modifiers compose on the shooter's own component before the
    // request leaves the weapon — for every leg, so an ability window can
    // never apply to the entry wound and miss the exit.
    if (UBreakerCombatComponent* OwnerCombat = GetOwner()->FindComponentByClass<UBreakerCombatComponent>())
    {
        OwnerCombat->ApplyOutgoingModifiers(Damage);
    }
    return TargetCombat->ReceiveDamage(Damage);
}

int32 UBreakerWeaponComponent::ResolvePelletImpacts(const UBreakerWeaponDefinition* Definition, const FVector& ViewLocation, const FVector& Direction,
    const FBreakerShotChannels& Channels, float ScaledBaseDamage, const UBreakerAttributeSet* SourceAttributes,
    const AActor* MarkedTarget, float LeadMinimumRangeCm, float LevelScalar, int32 PelletSeed,
    FBreakerShotResult& Shot, FBreakerPelletImpact& Pellet)
{
    const uint32 OwnerHash = GetTypeHash(GetOwner());
    const UBreakerProgressionComponent* Progression = GetOwner() ? GetOwner()->FindComponentByClass<UBreakerProgressionComponent>() : nullptr;
    // Sightline (Class-Kits §1.5 M7 rule half): "its pierce also ignores
    // Armour on the second and subsequent targets" — consumed here, off the
    // node tag the tree already publishes.
    const bool bSightline = Progression && Progression->HasNodeTag(BreakerSightlineTag());
    // Overpenetration (§1.5 M10): a killing hit skips the pierce falloff step.
    const bool bOverpenetration = Progression && Progression->HasNodeTag(BreakerOverpenetrationTag());
    // Angle (§1.5 M4, transcribed): ricochet seeks within 12 m at rank 1 and
    // 20 m at rank 2, overriding the authored base radius when larger.
    float SeekRadiusCm = RicochetSeekRadiusCm;
    if (const int32 AngleRank = Progression ? Progression->GetNodeRank(BreakerAngleNodeId, EBreakerPointCurrency::ClassPoints) : 0)
    {
        SeekRadiusCm = FMath::Max(SeekRadiusCm, AngleRank >= 2 ? 2000.0f : 1200.0f);   // Class-Kits §1.5 M4 R2/R1
    }

    FCollisionQueryParams Params(SCENE_QUERY_STAT(BreakerWeaponTrace), true, GetOwner());
    TArray<const AActor*> StruckActors;
    FVector SegmentStart = ViewLocation;
    FVector SegmentDirection = Direction;
    float TravelledCm = 0.0f;
    float CurrentMultiplier = 1.0f;
    int32 EnemiesStruck = 0;
    int32 RicochetsRemaining = Channels.RicochetCount;
    int32 SecondarySeedIndex = 0;
    FVector LastEnemyImpact = FVector::ZeroVector;
    float LastEnemyDistanceCm = 0.0f;

    while (TravelledCm < Definition->MaximumRange - 1.0f)
    {
        const float RemainingRange = Definition->MaximumRange - TravelledCm;
        const FVector SegmentEnd = SegmentStart + SegmentDirection * RemainingRange;
        const bool bIsFirstLeg = EnemiesStruck == 0 && TravelledCm == 0.0f;

        FHitResult Hit;
        if (!GetWorld()->LineTraceSingleByChannel(Hit, SegmentStart, SegmentEnd, ECC_GameTraceChannel2, Params))
        {
            // Flew off. The first leg leaves the pellet record exactly as the
            // caller seeded it (full-range end, no hit) — the legacy miss.
            if (!bIsFirstLeg)
            {
                FBreakerSecondaryImpact& Leg = Shot.SecondaryImpacts.AddDefaulted_GetRef();
                Leg.Start = SegmentStart;
                Leg.End = SegmentEnd;
            }
            break;
        }

        AActor* HitActor = Hit.GetActor();
        if (bIsFirstLeg)
        {
            // The pre-pierce contract, byte for byte: the pellet record and the
            // shot's single-impact accessors describe the FIRST thing the
            // pellet touched.
            Pellet.bHit = true;
            Pellet.End = Hit.ImpactPoint;
            Pellet.HitActor = HitActor;
            Shot.bHit = true;
            Shot.HitActor = HitActor;
            Shot.ImpactPoint = Hit.ImpactPoint;
            Shot.TraceEnd = Hit.ImpactPoint;
        }
        else
        {
            FBreakerSecondaryImpact& Leg = Shot.SecondaryImpacts.AddDefaulted_GetRef();
            Leg.Start = SegmentStart;
            Leg.End = Hit.ImpactPoint;
            Leg.bHit = true;
            Leg.HitActor = HitActor;
        }

        UBreakerCombatComponent* TargetCombat = HitActor ? HitActor->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
        if (!TargetCombat)
        {
            // RICOCHET: the shot hit the world. Spend a bounce seeking the
            // nearest enemy in line of sight of the impact; a bounce that
            // finds nobody dies on the wall, honestly.
            if (RicochetsRemaining > 0)
            {
                if (AActor* Sought = FindNearestChainTarget(Hit.ImpactPoint, SeekRadiusCm, StruckActors))
                {
                    --RicochetsRemaining;
                    CurrentMultiplier *= FMath::Clamp(RicochetDamageMultiplier, 0.0f, 1.0f);
                    TravelledCm += Hit.Distance;
                    SegmentStart = Hit.ImpactPoint;
                    SegmentDirection = (Sought->GetActorLocation() - Hit.ImpactPoint).GetSafeNormal();
                    if (SegmentDirection.IsNearlyZero()) break;
                    continue;
                }
            }
            break;
        }

        // The geometric hit, the forgiveness halo around it, or Lead's mark.
        // Distance for Lead's range gate is from the MUZZLE — a mark's payoff
        // must not be reachable by bouncing a round off a nearby wall.
        const float DistanceFromMuzzleCm = TravelledCm + Hit.Distance;
        const bool bWeakPoint = ResolveWeakPointHit(Hit, SegmentStart, SegmentDirection)
            || UBreakerAbility_Lead::ShouldTreatAsWeakPoint(
                MarkedTarget != nullptr && HitActor == MarkedTarget, DistanceFromMuzzleCm, LeadMinimumRangeCm);
        if (bIsFirstLeg)
        {
            Shot.bWeakPoint |= bWeakPoint;
            Pellet.bWeakPoint = bWeakPoint;
        }

        // The first hit rolls with the legacy seed material so an unchannelled
        // build's crit sequence does not move; every later leg draws from the
        // pierce sub-stream.
        const int32 DamageSeed = EnemiesStruck == 0
            ? static_cast<int32>(HashCombine(OwnerHash, static_cast<uint32>(PelletSeed)))
            : FBreakerWeaponMath::SecondaryShotSeed(OwnerHash, PelletSeed, BreakerPierceSalt, SecondarySeedIndex++);
        const float ArmorPenetration = (EnemiesStruck > 0 && bSightline) ? 1.0f : Definition->ArmorPenetration;
        const FBreakerDamageResult HitDamage = SubmitWeaponDamage(Definition, TargetCombat, SourceAttributes,
            ScaledBaseDamage * CurrentMultiplier, DistanceFromMuzzleCm, bWeakPoint, ArmorPenetration, Hit.ImpactPoint, DamageSeed);
        Shot.DamageResult.RawDamage += HitDamage.RawDamage;
        Shot.DamageResult.MitigatedDamage += HitDamage.MitigatedDamage;
        Shot.DamageResult.ShieldDamage += HitDamage.ShieldDamage;
        Shot.DamageResult.HealthDamage += HitDamage.HealthDamage;
        Shot.DamageResult.RemainingShield = HitDamage.RemainingShield;
        Shot.DamageResult.RemainingHealth = HitDamage.RemainingHealth;
        Shot.DamageResult.bCritical |= HitDamage.bCritical;
        Shot.DamageResult.bWeakPoint |= HitDamage.bWeakPoint;
        Shot.DamageResult.bShieldBroken |= HitDamage.bShieldBroken;
        Shot.DamageResult.bKilled |= HitDamage.bKilled;
        // The first hit's bleed seeds from the raw pellet sequence value —
        // exactly the material the pre-channel code used, so an unchannelled
        // build's bleed rolls do not move either.
        ApplyBleedOnHit(Definition, HitActor, SourceAttributes, LevelScalar, EnemiesStruck == 0 ? PelletSeed : DamageSeed);

        StruckActors.Add(HitActor);
        ++EnemiesStruck;
        LastEnemyImpact = Hit.ImpactPoint;
        LastEnemyDistanceCm = DistanceFromMuzzleCm;

        // PIERCE: the budget is enemies CONTINUED THROUGH, so the first hit is
        // free and PierceCount 0 stops here — the legacy single-hit shot.
        if (EnemiesStruck > Channels.PierceCount) break;
        CurrentMultiplier = FBreakerWeaponMath::NextPierceMultiplier(CurrentMultiplier, PierceDamageFalloff, HitDamage.bKilled, bOverpenetration);
        Params.AddIgnoredActor(HitActor);
        TravelledCm += Hit.Distance;
        SegmentStart = Hit.ImpactPoint;
    }

    // CHAIN: on hit, from wherever the shot's last victim stood. On HIT rather
    // than on kill, deliberately: Class-Kits already owns the on-kill
    // continuation (§1.5 M10 Overpenetration lets a killing shot carry on),
    // so chain-on-kill would duplicate a rule the branch already sells —
    // chain-on-hit is the distinct mechanic the owner's ruling adds, and it
    // is the one a player can rely on feeling every shot at Redline.
    if (EnemiesStruck > 0 && Channels.ChainCount > 0)
    {
        FVector ArcOrigin = LastEnemyImpact;
        float ArcMultiplier = ScaledBaseDamage > 0.0f ? CurrentMultiplier : 0.0f;
        for (int32 Arc = 0; Arc < Channels.ChainCount; ++Arc)
        {
            AActor* Target = FindNearestChainTarget(ArcOrigin, ChainRadiusCm, StruckActors);
            UBreakerCombatComponent* TargetCombat = Target ? Target->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
            if (!TargetCombat) break;
            ArcMultiplier *= FMath::Clamp(ChainDamageMultiplier, 0.0f, 1.0f);

            const FVector ArcImpact = Target->GetActorLocation();
            FBreakerSecondaryImpact& Leg = Shot.SecondaryImpacts.AddDefaulted_GetRef();
            Leg.Start = ArcOrigin;
            Leg.End = ArcImpact;
            Leg.bHit = true;
            Leg.HitActor = Target;

            // The arc inherits its parent hit's range falloff rather than
            // re-measuring: the reduced-damage fraction is the arc's whole
            // price, and paying falloff twice would make chain quietly
            // worthless at exactly the ranges Marksman fights at.
            const int32 ArcSeed = FBreakerWeaponMath::SecondaryShotSeed(OwnerHash, PelletSeed, BreakerChainSalt, Arc);
            const FBreakerDamageResult ArcDamage = SubmitWeaponDamage(Definition, TargetCombat, SourceAttributes,
                ScaledBaseDamage * ArcMultiplier, LastEnemyDistanceCm, false, Definition->ArmorPenetration, ArcImpact, ArcSeed);
            Shot.DamageResult.RawDamage += ArcDamage.RawDamage;
            Shot.DamageResult.MitigatedDamage += ArcDamage.MitigatedDamage;
            Shot.DamageResult.ShieldDamage += ArcDamage.ShieldDamage;
            Shot.DamageResult.HealthDamage += ArcDamage.HealthDamage;
            Shot.DamageResult.bCritical |= ArcDamage.bCritical;
            Shot.DamageResult.bShieldBroken |= ArcDamage.bShieldBroken;
            Shot.DamageResult.bKilled |= ArcDamage.bKilled;
            ApplyBleedOnHit(Definition, Target, SourceAttributes, LevelScalar, ArcSeed);

            StruckActors.Add(Target);
            ArcOrigin = ArcImpact;
        }
    }

    return FMath::Max(0, EnemiesStruck - 1);
}

void UBreakerWeaponComponent::ApplyBleedOnHit(const UBreakerWeaponDefinition* Definition, AActor* Target, const UBreakerAttributeSet* SourceAttributes, float LevelScalar, int32 SeedBasis)
{
    if (!Definition || !Target || Definition->BleedChance <= 0.0f || Definition->BleedDamagePerTick <= 0.0f || Definition->BleedDuration <= 0.0f) return;
    UBreakerStatusComponent* Status = Target->FindComponentByClass<UBreakerStatusComponent>();
    if (!Status) return;

    // Same seed material as the hit's damage, salted so bleed and critical
    // rolls stay independent while remaining reproducible on the server.
    FRandomStream Stream(static_cast<int32>(HashCombine(HashCombine(GetTypeHash(GetOwner()), static_cast<uint32>(SeedBasis)), BreakerBleedSalt)));
    if (Stream.FRand() > Definition->BleedChance) return;

    FBreakerStatusApplicationSpec Spec;
    Spec.StatusTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Bleed"), false);
    // The DoT's base is a weapon base damage number like any other, so it rides
    // the same item-level curve. Leaving it flat would make bleed a smaller and
    // smaller fraction of an SMG's output as the game went on, which is a
    // silent nerf to the archetype's identity rather than a design choice.
    Spec.BaseDamagePerTick = Definition->BleedDamagePerTick * FMath::Max(0.0f, LevelScalar);
    Spec.Duration = Definition->BleedDuration;
    Spec.TickInterval = FMath::Max(0.05f, Definition->BleedTickInterval);
    // DoTs snapshot at APPLICATION (locked rule), and the snapshot now folds in
    // the outgoing chain's budgeted window product: a bleed applied inside an
    // Overdrive window ticks at window strength for its whole life, one applied
    // outside never gains it, and a window opened after application changes
    // nothing. Application-time only — never per tick.
    Spec.Snapshot.SourcePower = UBreakerCombatComponent::ComposeDotSourcePower(
        SourceAttributes, GetOwner() ? GetOwner()->FindComponentByClass<UBreakerCombatComponent>() : nullptr);
    Spec.Snapshot.CriticalChance = SourceAttributes ? SourceAttributes->GetCriticalChance() : UBreakerAttributeSet::DefaultCriticalChance;
    Spec.Snapshot.CriticalMultiplier = SourceAttributes ? SourceAttributes->GetCriticalMultiplier() : UBreakerAttributeSet::DefaultCriticalMultiplier;
    Spec.Snapshot.DamageOverTimeMultiplier = SourceAttributes ? SourceAttributes->GetDamageOverTimeMultiplier() : 1.0f;
    // The critical result is rolled once at application; every tick of this
    // application then crits or does not for its whole lifetime.
    Spec.Snapshot.bRolledCritical = Stream.FRand() < Spec.Snapshot.CriticalChance;
    Status->ApplyStatus(Spec, EBreakerDamageFamily::Physical, GetOwner());
}

void UBreakerWeaponComponent::FireProjectile(const UBreakerWeaponDefinition* Definition, const FVector& ViewLocation, const FRotator& ViewRotation, float Spread, int32 BurstIndex, int32 RecoilSeed, float ShotAimAlpha)
{
    const FVector Direction = FBreakerWeaponMath::ApplyConeSpread(ViewRotation.Vector(), Spread, ++ShotSequence);

    const UBreakerAttributeSet* SourceAttributes = nullptr;
    if (const IAbilitySystemInterface* AbilityOwner = Cast<IAbilitySystemInterface>(GetOwner()))
    {
        if (const UAbilitySystemComponent* ASC = AbilityOwner->GetAbilitySystemComponent()) SourceAttributes = ASC->GetSet<UBreakerAttributeSet>();
    }
    FBreakerDamageRequest Damage;
    // Same multiplicand as the hitscan path: the rocket's payload is a weapon
    // base damage number and scales with item level identically.
    Damage.BaseDamage = FMath::Max(0.0f, Definition->Damage) * GetItemLevelDamageScalar();
    Damage.DamageFamily = EBreakerDamageFamily::Physical;
    Damage.WeakPointMultiplier = 1.0f;
    Damage.ArmorPenetration = Definition->ArmorPenetration;
    Damage.CriticalChance = SourceAttributes ? SourceAttributes->GetCriticalChance() : UBreakerAttributeSet::DefaultCriticalChance;
    Damage.CriticalMultiplier = SourceAttributes ? SourceAttributes->GetCriticalMultiplier() : UBreakerAttributeSet::DefaultCriticalMultiplier;
    // Same single composed number as the hitscan path.
    Damage.SourceDamageMultiplier = SourceAttributes ? SourceAttributes->GetDamageMultiplier() : 1.0f;
    // STAGE 6: the same source split the hitscan path fills, and the reason
    // the split lives on the REQUEST at all: a rocket has no target at fire
    // time, so its target-conditional riders can only resolve at impact, in
    // ReceiveDamage, from the halves snapshotted here. The rocket carries the
    // fire-time split exactly as it carries the fire-time modifiers below.
    Damage.bHasSourceSplit = true;
    if (SourceAttributes)
    {
        Damage.SourceMoreProduct = SourceAttributes->GetAttributeAggregator().ComposedMoreProduct(EBreakerAggregatedAttribute::DamageMultiplier);
        Damage.SourceIncreasedPercent = (Damage.SourceDamageMultiplier / FMath::Max(Damage.SourceMoreProduct, UE_SMALL_NUMBER) - 1.0f) * 100.0f;
    }
    Damage.RandomSeed = HashCombine(GetTypeHash(GetOwner()), ShotSequence);
    Damage.SetInstigator(GetOwner());
    // The rocket carries an already-composed request; modifiers active at the
    // moment of firing are the ones that count, not those at detonation.
    if (UBreakerCombatComponent* OwnerCombat = GetOwner() ? GetOwner()->FindComponentByClass<UBreakerCombatComponent>() : nullptr)
    {
        OwnerCombat->ApplyOutgoingModifiers(Damage);
    }

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Params.Owner = GetOwner();
    Params.Instigator = Cast<APawn>(GetOwner());
    // Spawn ahead of the view so the rocket clears the shooter's capsule.
    const FVector SpawnLocation = ViewLocation + Direction * 80.0f;
    if (ABreakerRocketProjectile* Rocket = GetWorld()->SpawnActor<ABreakerRocketProjectile>(ABreakerRocketProjectile::StaticClass(), SpawnLocation, Direction.Rotation(), Params))
    {
        Rocket->InitializeRocket(Damage, Definition->ProjectileSpeed, Definition->ExplosionRadius);
    }

    FBreakerShotResult Shot;
    Shot.bFired = true;
    Shot.BurstShotIndex = BurstIndex;
    Shot.RecoilSeed = RecoilSeed;
    Shot.bAimedShot = bAiming;
    Shot.AimAlpha = ShotAimAlpha;
    Shot.TraceStart = ViewLocation;
    Shot.TraceEnd = SpawnLocation + Direction * 400.0f;
    MulticastShotCosmetics(Shot);
}

void UBreakerWeaponComponent::FinishReload()
{
    const UBreakerWeaponDefinition* Definition = ResolveDefinition();
    if (!Definition) return;
    // Fills to the EFFECTIVE capacity, so a reload completing inside an
    // Overhaul window fills to the overridden size and one completing after
    // the pop fills to base (Class-Kits-Gunsmith §3 G2's stated requirement).
    const int32 Needed = FMath::Max(0, GetEffectiveMagazineSize() - MagazineAmmo);
    const int32 Loaded = FMath::Min(Needed, ReserveAmmo);
    MagazineAmmo += Loaded;
    ReserveAmmo -= Loaded;
    bReloading = false;
    OnReloadChanged.Broadcast(false);
    OnAmmoChanged.Broadcast(MagazineAmmo, ReserveAmmo);
    // Scrap's reload clause as a parameter: rounds only ever leave a magazine
    // by being FIRED, so "the reload loaded anything" is exactly "a round left
    // the magazine since it was last full" — a top-off of an untouched
    // magazine cannot start (StartReload's full-magazine gate), and a reload
    // that loads zero because reserve ran dry mid-cycle still credits nothing.
    OnReloadCompleted.Broadcast(Loaded > 0);
}

int32 UBreakerWeaponComponent::PushMagazineCapacityOverride(FName Key, int32 DeltaRounds, int32 ReservePerRound)
{
    if (Key.IsNone() || DeltaRounds <= 0) return 0;
    // Re-pushing the same key SETTLES the old entry first rather than
    // stacking: a re-cast refreshes, and stacking two conversions under one
    // key would strand the first one's economy.
    PopMagazineCapacityOverride(Key);

    FMagazineCapacityOverrideEntry Entry;
    Entry.ReservePerRound = FMath::Max(0, ReservePerRound);
    if (Entry.ReservePerRound > 0)
    {
        // The conversion form (G2): draw what reserve can actually pay for.
        const int32 Affordable = ReserveAmmo / Entry.ReservePerRound;
        Entry.DeltaRounds = FMath::Clamp(Affordable, 0, DeltaRounds);
        if (Entry.DeltaRounds <= 0) return 0;
        ReserveAmmo -= Entry.DeltaRounds * Entry.ReservePerRound;
        MagazineAmmo += Entry.DeltaRounds;
        OnAmmoChanged.Broadcast(MagazineAmmo, ReserveAmmo);
    }
    else
    {
        Entry.DeltaRounds = DeltaRounds;
    }
    MagazineCapacityOverrides.Add(Key, Entry);
    return Entry.DeltaRounds;
}

void UBreakerWeaponComponent::PopMagazineCapacityOverride(FName Key)
{
    const FMagazineCapacityOverrideEntry* Entry = MagazineCapacityOverrides.Find(Key);
    if (!Entry) return;
    const FMagazineCapacityOverrideEntry Removed = *Entry;
    MagazineCapacityOverrides.Remove(Key);

    if (Removed.ReservePerRound > 0)
    {
        // Settle the unspent remainder back (G2): whatever converted rounds
        // are still sitting above the restored capacity return to reserve at
        // the ratio they were bought at. Fired rounds refund nothing.
        const int32 Unspent = FMath::Clamp(MagazineAmmo - GetEffectiveMagazineSize(), 0, Removed.DeltaRounds);
        if (Unspent > 0)
        {
            MagazineAmmo -= Unspent;
            ReserveAmmo += Unspent * Removed.ReservePerRound;
            OnAmmoChanged.Broadcast(MagazineAmmo, ReserveAmmo);
        }
    }
}

int32 UBreakerWeaponComponent::GetEffectiveMagazineSize() const
{
    const UBreakerWeaponDefinition* Definition = ResolveDefinition();
    int32 Size = Definition ? Definition->MagazineSize : 0;
    for (const TPair<FName, FMagazineCapacityOverrideEntry>& Override : MagazineCapacityOverrides)
    {
        Size += Override.Value.DeltaRounds;
    }
    return FMath::Max(1, Size);
}

FVector UBreakerWeaponComponent::GetVisualMuzzleLocation() const
{
    FVector ViewLocation;
    FRotator ViewRotation;
    GetViewPoint(ViewLocation, ViewRotation);
    const FVector Offset = bAiming ? AimedMuzzleViewOffset : MuzzleViewOffset;
    return ViewLocation + ViewRotation.RotateVector(Offset);
}

void UBreakerWeaponComponent::GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
    if (const APawn* Pawn = Cast<APawn>(GetOwner()))
    {
        if (const AController* Controller = Pawn->GetController())
        {
            Controller->GetPlayerViewPoint(OutLocation, OutRotation);
            return;
        }
    }
    if (GetOwner()) GetOwner()->GetActorEyesViewPoint(OutLocation, OutRotation);
}

void UBreakerWeaponComponent::MulticastShotCosmetics_Implementation(const FBreakerShotResult& Shot)
{
    LastShot = Shot;
    LastCosmeticShotTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    // Feel runs on the cosmetic path, after the trace has already been
    // resolved: the round goes where the player was aiming when they pulled,
    // and the kick then moves the aim for the shot after it.
    if (Shot.bFired) ApplyShotFeel(Shot);
    OnShot.Broadcast(Shot);
}

float UBreakerWeaponComponent::GetSecondsSinceLastShot() const
{
    return GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds() - LastCosmeticShotTime) : BIG_NUMBER;
}

void UBreakerWeaponComponent::ResetAmmunition()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    StopFire();
    GetWorld()->GetTimerManager().ClearTimer(ReloadTimer);
    SlotOneMagazineAmmo = -1;
    SlotTwoMagazineAmmo = -1;
    InitializeSlotAmmunition();
    MagazineAmmo = CurrentSlot == 1 ? SlotOneMagazineAmmo : SlotTwoMagazineAmmo;
    ReserveAmmo = CurrentSlot == 1 ? SlotOneReserveAmmo : SlotTwoReserveAmmo;
    bReloading = false;
    ResetWeaponFeel();
    OnReloadChanged.Broadcast(false);
    OnAmmoChanged.Broadcast(MagazineAmmo, ReserveAmmo);
}

void UBreakerWeaponComponent::AddReserveAmmoFraction(float Fraction)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || Fraction <= 0.0f) return;
    InitializeSlotAmmunition();

    // O2 placeholder: cap at 2x StartingReserveAmmo. Enough headroom that a
    // good streak banks a cushion, tight enough that reserve still matters.
    const float ReserveCapMultiplier = 2.0f;

    auto GrantToSlot = [this, Fraction, ReserveCapMultiplier](EBreakerWeaponArchetype Archetype, int32& SlotReserve)
    {
        const UBreakerWeaponDefinition* Definition = GetPrototypeDefinition(Archetype);
        if (!Definition) return;
        const int32 Starting = Definition->StartingReserveAmmo;
        // Round up so small fractions on low-reserve weapons (rocket: 16)
        // still grant at least one round.
        const int32 Granted = FMath::CeilToInt(Starting * Fraction);
        const int32 Cap = FMath::CeilToInt(Starting * ReserveCapMultiplier);
        SlotReserve = FMath::Min(SlotReserve + Granted, Cap);
    };

    // The equipped slot's live counters are the source of truth; sync them
    // into slot storage first so nothing is lost.
    StoreActiveSlotAmmunition();
    GrantToSlot(SlotOneArchetype, SlotOneReserveAmmo);
    GrantToSlot(SlotTwoArchetype, SlotTwoReserveAmmo);
    ReserveAmmo = CurrentSlot == 1 ? SlotOneReserveAmmo : SlotTwoReserveAmmo;
    OnAmmoChanged.Broadcast(MagazineAmmo, ReserveAmmo);
}

void UBreakerWeaponComponent::OnRep_Ammo() { OnAmmoChanged.Broadcast(MagazineAmmo, ReserveAmmo); }
void UBreakerWeaponComponent::OnRep_Reloading() { OnReloadChanged.Broadcast(bReloading); }
void UBreakerWeaponComponent::OnRep_Swapping()
{
    if (!bSwapping) LastSwapInTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    OnSwapChanged.Broadcast(bSwapping, CurrentSlot);
}
