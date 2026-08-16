#include "Abilities/BreakerAbilityDefinition.h"

#include "Abilities/BreakerAbilityTags.h"
#include "Abilities/BreakerAbility_CadenceBreak.h"
#include "Abilities/BreakerAbility_Cleave.h"
#include "Abilities/BreakerAbility_Closequarter.h"
#include "Abilities/BreakerAbility_Fracture.h"
#include "Abilities/BreakerAbility_Lead.h"
#include "Abilities/BreakerAbility_Resonance.h"
#include "Abilities/BreakerAbility_Rot.h"
#include "Abilities/BreakerAbility_Siphon.h"
#include "Abilities/BreakerAbility_Unmake.h"
#include "Abilities/BreakerAbility_Overdrive.h"
#include "Abilities/BreakerAbility_Skim.h"
#include "Abilities/BreakerGunsmithAbilities.h"
#include "Abilities/BreakerSupportAbilities.h"
#include "Abilities/BreakerTankAbilities.h"

bool UBreakerAbilityDefinition::CanOccupySlot(EBreakerAbilitySlot Slot) const
{
    if (SlotAffinity == EBreakerAbilitySlot::Ultimate || Slot == EBreakerAbilitySlot::Ultimate)
    {
        return SlotAffinity == Slot;
    }
    return true;
}

FBreakerAbilityVariant UBreakerAbilityDefinition::ResolveVariant(const FGameplayTagContainer& OwnerTags) const
{
    const FBreakerAbilityVariant* Base = nullptr;
    for (const FBreakerAbilityVariant& Variant : Variants)
    {
        if (!Variant.KeystoneTag.IsValid())
        {
            if (!Base)
            {
                Base = &Variant;
            }
            continue;
        }
        if (OwnerTags.HasTag(Variant.KeystoneTag))
        {
            return Variant;
        }
    }
    if (Base)
    {
        return *Base;
    }
    FBreakerAbilityVariant Fallback;
    Fallback.WindowDuration = WindowDuration;
    return Fallback;
}

namespace
{
    UBreakerAbilityDefinition* MakeFallback(const TCHAR* ObjectName)
    {
        UBreakerAbilityDefinition* Definition = NewObject<UBreakerAbilityDefinition>(GetTransientPackage(), FName(ObjectName));
        Definition->AddToRoot();
        return Definition;
    }
}

// ---------------------------------------------------------------------------
// TAGS FOR THE THREE DESIGNED-BUT-UNBUILT CLASSES — FILE-LOCAL, AND THEY SHOULD
// NOT STAY THAT WAY.
// ---------------------------------------------------------------------------
// Every Swift and Caster tag is declared in `Abilities/BreakerAbilityTags.h`,
// whose header comment states the convention plainly: the ability infrastructure
// owns its own vocabulary so content authoring cannot silently drop a tag the
// fallback registry depends on. These belong there too. They are declared here
// instead for one reason and it is not a design one — this pass had write access
// to the registry and not to the tag header — and the split is recorded rather
// than hidden.
//
// **COORDINATOR: MOVE THESE.** Promote each line below to a
// `UE_DECLARE_GAMEPLAY_TAG_EXTERN` in `BreakerAbilityTags.h` plus a
// `UE_DEFINE_GAMEPLAY_TAG` in `BreakerAbilityTags.cpp`, keeping the tag STRINGS
// byte-identical, then delete this block and swap the references below. The
// strings are what a save, a Data Asset and a granted GameplayEffect key off;
// the C++ symbol names are not. Nothing else has to change.
//
// The keystone tags follow the `Keystone.<Class>.<Name>` shape the three
// existing sets use, so `RiorsEdge.Abilities.KeystoneReachability` sees them
// the same way it sees Caster's — and today it takes that test's HONEST
// EMPTINESS arm, because none of these three classes has a branch tree and so
// no node anywhere COULD grant one. The day a Gunsmith, Tank or Support branch
// tree is authored, that arm stops applying and the suite goes red until each
// keystone is sited on a real cornerstone node. That is the intended alarm.
namespace
{
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Ability_Class_Gunsmith_SidearmRig, "Ability.Class.Gunsmith.SidearmRig");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Ability_Class_Gunsmith_Overhaul, "Ability.Class.Gunsmith.Overhaul");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Ability_Class_Gunsmith_Turret, "Ability.Class.Gunsmith.Turret");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Ability_Class_Gunsmith_AmmoCrate, "Ability.Class.Gunsmith.AmmoCrate");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Ability_Class_Gunsmith_MineCluster, "Ability.Class.Gunsmith.MineCluster");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Ability_Class_Gunsmith_Disruptor, "Ability.Class.Gunsmith.Disruptor");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Ability_Class_Gunsmith_FieldAssembly, "Ability.Class.Gunsmith.FieldAssembly");
    // Only the two ARMORY abilities carry cooldowns; the four deployables are
    // cost-gated and author no cooldown tag at all, so the HUD can tell
    // "cost-gated" from "cooldown of zero" (spec D3, Class-Kits-Gunsmith §1.5).
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Cooldown_Class_Gunsmith_SidearmRig, "Cooldown.Class.Gunsmith.SidearmRig");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Cooldown_Class_Gunsmith_Overhaul, "Cooldown.Class.Gunsmith.Overhaul");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Keystone_Gunsmith_Machinist, "Keystone.Gunsmith.Machinist");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Keystone_Gunsmith_Foundry, "Keystone.Gunsmith.Foundry");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Keystone_Gunsmith_Minefield, "Keystone.Gunsmith.Minefield");

    UE_DEFINE_GAMEPLAY_TAG_STATIC(Ability_Class_Tank_Rend, "Ability.Class.Tank.Rend");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Ability_Class_Tank_Bloodline, "Ability.Class.Tank.Bloodline");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Ability_Class_Tank_AnchorPoint, "Ability.Class.Tank.AnchorPoint");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Ability_Class_Tank_Provoke, "Ability.Class.Tank.Provoke");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Ability_Class_Tank_BreachCharge, "Ability.Class.Tank.BreachCharge");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Ability_Class_Tank_GroundZero, "Ability.Class.Tank.GroundZero");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Ability_Class_Tank_Hold, "Ability.Class.Tank.Hold");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Cooldown_Class_Tank_Rend, "Cooldown.Class.Tank.Rend");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Cooldown_Class_Tank_Bloodline, "Cooldown.Class.Tank.Bloodline");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Cooldown_Class_Tank_AnchorPoint, "Cooldown.Class.Tank.AnchorPoint");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Cooldown_Class_Tank_Provoke, "Cooldown.Class.Tank.Provoke");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Cooldown_Class_Tank_BreachCharge, "Cooldown.Class.Tank.BreachCharge");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Cooldown_Class_Tank_GroundZero, "Cooldown.Class.Tank.GroundZero");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Keystone_Tank_Vein, "Keystone.Tank.Vein");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Keystone_Tank_Wall, "Keystone.Tank.Wall");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Keystone_Tank_Detonation, "Keystone.Tank.Detonation");

    UE_DEFINE_GAMEPLAY_TAG_STATIC(Ability_Class_Support_Patch, "Ability.Class.Support.Patch");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Ability_Class_Support_Purge, "Ability.Class.Support.Purge");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Ability_Class_Support_Cadence, "Ability.Class.Support.Cadence");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Ability_Class_Support_Metronome, "Ability.Class.Support.Metronome");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Ability_Class_Support_Mark, "Ability.Class.Support.Mark");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Ability_Class_Support_Suppress, "Ability.Class.Support.Suppress");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Ability_Class_Support_Conduit, "Ability.Class.Support.Conduit");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Cooldown_Class_Support_Patch, "Cooldown.Class.Support.Patch");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Cooldown_Class_Support_Purge, "Cooldown.Class.Support.Purge");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Cooldown_Class_Support_Cadence, "Cooldown.Class.Support.Cadence");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Cooldown_Class_Support_Metronome, "Cooldown.Class.Support.Metronome");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Cooldown_Class_Support_Mark, "Cooldown.Class.Support.Mark");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Cooldown_Class_Support_Suppress, "Cooldown.Class.Support.Suppress");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Keystone_Support_Triage, "Keystone.Support.Triage");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Keystone_Support_Downbeat, "Keystone.Support.Downbeat");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Keystone_Support_Blackout, "Keystone.Support.Blackout");
}

const TArray<UBreakerAbilityDefinition*>& UBreakerAbilityDefinition::GetFallbackRegistry()
{
    static TArray<UBreakerAbilityDefinition*> Registry;
    if (Registry.Num() > 0)
    {
        return Registry;
    }

    // ------------------------------------------------------------------
    // Swift, the vertical-slice class. Costs and cooldowns are quoted from
    // Docs/Design/Class-Kits.md §1.2; nothing here is invented balance.
    // Anything NOT quoted from a design doc is marked O2 PLACEHOLDER and must
    // be replaced from wave-mode instrumentation before content lock.
    // ------------------------------------------------------------------

    // S3 Skim — Class-Kits §1.2 row S3: 15 Momentum, 3s cooldown.
    UBreakerAbilityDefinition* Skim = MakeFallback(TEXT("FallbackAbility_Swift_Skim"));
    Skim->AbilityId = TEXT("Swift.Skim");
    Skim->ClassId = EBreakerClassId::Swift;
    Skim->DisplayName = FText::FromString(TEXT("Skim"));
    Skim->Description = FText::FromString(TEXT("Directional impulse that redirects existing horizontal speed."));
    Skim->SlotAffinity = EBreakerAbilitySlot::ClassAbilityOne;
    Skim->AbilityTag = BreakerAbilityTags::Ability_Class_Swift_Skim;
    Skim->CooldownTag = BreakerAbilityTags::Cooldown_Class_Swift_Skim;
    Skim->AbilityClass = UBreakerAbility_Skim::StaticClass();
    Skim->ResourceCost = 15.0f;
    Skim->CooldownSeconds = 3.0f;
    // O2 PLACEHOLDER: the redirect/boost window length is not specified by any
    // design doc. Structure is complete; the number is a guess and must be
    // replaced (Ability-Implementation-Spec §11 GAP list).
    Skim->WindowDuration = 0.25f;
    Registry.Add(Skim);

    // S6 Lead — Class-Kits §1.2 row S6: 40 Momentum, 10s cooldown.
    // The task brief called this ability "Lash"; Class-Kits has no such ability
    // and S6 Lead is the Swift Marksman ability at that slot, so Lead is used.
    UBreakerAbilityDefinition* Lead = MakeFallback(TEXT("FallbackAbility_Swift_Lead"));
    Lead->AbilityId = TEXT("Swift.Lead");
    Lead->ClassId = EBreakerClassId::Swift;
    Lead->DisplayName = FText::FromString(TEXT("Lead"));
    Lead->Description = FText::FromString(TEXT("Marks a target; long-range hits on the mark count as weak points."));
    Lead->SlotAffinity = EBreakerAbilitySlot::ClassAbilityTwo;
    Lead->AbilityTag = BreakerAbilityTags::Ability_Class_Swift_Lead;
    Lead->CooldownTag = BreakerAbilityTags::Cooldown_Class_Swift_Lead;
    Lead->AbilityClass = UBreakerAbility_Lead::StaticClass();
    Lead->ResourceCost = 40.0f;
    Lead->CooldownSeconds = 10.0f;
    Lead->WindowDuration = 6.0f; // Class-Kits §1.2 row S6: mark lasts 6s.
    Registry.Add(Lead);

    // S2 Cadence Break — Class-Kits §1.2 row S2: 35 Momentum, 8s cooldown,
    // 3s state. This row closes the "Swift.CadenceBreak does not exist in the
    // ability fallback registry" gap that Slipcut Mastery and Second Wind both
    // record in Progression/BreakerProgressionLibrary.cpp; the F7 grant itself
    // still waits on that (read-only in this pass) file adding the grant line.
    // ClassAbilityTwo affinity like Lead's: affinity is a preference, not an
    // exclusive claim (CanOccupySlot only fences the Ultimate slot).
    UBreakerAbilityDefinition* CadenceBreak = MakeFallback(TEXT("FallbackAbility_Swift_CadenceBreak"));
    CadenceBreak->AbilityId = TEXT("Swift.CadenceBreak");
    CadenceBreak->ClassId = EBreakerClassId::Swift;
    CadenceBreak->DisplayName = FText::FromString(TEXT("Cadence Break"));
    CadenceBreak->Description = FText::FromString(TEXT("Completes the reload and opens a state where consecutive hits on one target stack flat damage."));
    CadenceBreak->SlotAffinity = EBreakerAbilitySlot::ClassAbilityTwo;
    CadenceBreak->AbilityTag = BreakerAbilityTags::Ability_Class_Swift_CadenceBreak;
    CadenceBreak->CooldownTag = BreakerAbilityTags::Cooldown_Class_Swift_CadenceBreak;
    CadenceBreak->AbilityClass = UBreakerAbility_CadenceBreak::StaticClass();
    CadenceBreak->ResourceCost = 35.0f;
    CadenceBreak->CooldownSeconds = 8.0f;
    CadenceBreak->WindowDuration = 3.0f; // Class-Kits §1.2 row S2: "a 3s state".
    Registry.Add(CadenceBreak);

    // Overdrive — Class-Kits §1.2 ultimate: 100 Momentum (full bar), no
    // cooldown; the cost is the cooldown. Base window 8s.
    UBreakerAbilityDefinition* Overdrive = MakeFallback(TEXT("FallbackAbility_Swift_Overdrive"));
    Overdrive->AbilityId = TEXT("Swift.Overdrive");
    Overdrive->ClassId = EBreakerClassId::Swift;
    Overdrive->DisplayName = FText::FromString(TEXT("Overdrive"));
    Overdrive->Description = FText::FromString(TEXT("Momentum stops decaying and generation doubles for the duration."));
    Overdrive->SlotAffinity = EBreakerAbilitySlot::Ultimate;
    Overdrive->AbilityTag = BreakerAbilityTags::Ability_Class_Swift_Overdrive;
    // Deliberately no cooldown tag: cost-gated, and the HUD must be able to
    // tell "no cooldown" from "cooldown of zero" (spec D3).
    Overdrive->AbilityClass = UBreakerAbility_Overdrive::StaticClass();
    Overdrive->ResourceCost = 100.0f;
    Overdrive->CooldownSeconds = 0.0f;
    Overdrive->WindowDuration = 8.0f;

    // Keystone variant rows (spec D1). Index 0 is the base row. Class-Kits
    // specifies the *behavior* of each rewrite but no numbers for the
    // parametric part, so every SpeedMultiplier below and the Bloodrhythm
    // timeout's companion values are O2 PLACEHOLDER — structure is the
    // deliverable, not balance. Durations are quoted (8s base, 1.5s
    // Bloodrhythm hit timeout).
    {
        FBreakerAbilityVariant BaseRow;
        BaseRow.VariantName = FText::FromString(TEXT("Overdrive"));
        BaseRow.WindowDuration = 8.0f;
        BaseRow.SpeedMultiplier = 1.10f; // O2 PLACEHOLDER
        Overdrive->Variants.Add(BaseRow);

        FBreakerAbilityVariant Bloodrhythm;
        Bloodrhythm.KeystoneTag = BreakerAbilityTags::Keystone_Swift_Bloodrhythm;
        Bloodrhythm.VariantName = FText::FromString(TEXT("Overdrive — Bloodrhythm"));
        Bloodrhythm.WindowDuration = 8.0f;
        Bloodrhythm.SpeedMultiplier = 1.10f; // O2 PLACEHOLDER
        // Class-Kits F12: the ultimate ends if the player goes 1.5s without a hit.
        Bloodrhythm.HitTimeoutSeconds = 1.5f;
        Overdrive->Variants.Add(Bloodrhythm);

        FBreakerAbilityVariant TerminalVelocity;
        TerminalVelocity.KeystoneTag = BreakerAbilityTags::Keystone_Swift_TerminalVelocity;
        TerminalVelocity.VariantName = FText::FromString(TEXT("Overdrive — Terminal Velocity"));
        TerminalVelocity.WindowDuration = 8.0f;
        // Availability rewrite, not a speed rewrite (Class-Kits K12 quotes
        // Master 5.4 explicitly), so the multiplier stays at 1.0.
        TerminalVelocity.SpeedMultiplier = 1.0f;
        Overdrive->Variants.Add(TerminalVelocity);

        FBreakerAbilityVariant StandingWave;
        StandingWave.KeystoneTag = BreakerAbilityTags::Keystone_Swift_StandingWave;
        StandingWave.VariantName = FText::FromString(TEXT("Overdrive — Standing Wave"));
        StandingWave.WindowDuration = 8.0f;
        // The stationary Swift ultimate: no movement contribution at all.
        StandingWave.SpeedMultiplier = 1.0f;
        Overdrive->Variants.Add(StandingWave);
    }
    Registry.Add(Overdrive);

    // ------------------------------------------------------------------
    // Caster. Costs quoted from Class-Kits §2.2. NO COOLDOWNS ANYWHERE in
    // this class: Mana *is* the cooldown (Class-Kits §2.1), so no entry below
    // authors CooldownSeconds or a CooldownTag, and the HUD can therefore tell
    // "cost-gated" from "cooldown of zero" (spec D3).
    // ------------------------------------------------------------------

    // C1 Cleave — Class-Kits §2.2 row C1: 20 Mana, no cooldown.
    UBreakerAbilityDefinition* Cleave = MakeFallback(TEXT("FallbackAbility_Caster_Cleave"));
    Cleave->AbilityId = TEXT("Caster.Cleave");
    Cleave->ClassId = EBreakerClassId::Caster;
    Cleave->DisplayName = FText::FromString(TEXT("Cleave"));
    Cleave->Description = FText::FromString(TEXT("Short forward melee arc that always applies Bleed."));
    Cleave->SlotAffinity = EBreakerAbilitySlot::ClassAbilityOne;
    Cleave->AbilityTag = BreakerAbilityTags::Ability_Class_Caster_Cleave;
    Cleave->AbilityClass = UBreakerAbility_Cleave::StaticClass();
    Cleave->ResourceCost = 20.0f;
    Cleave->CooldownSeconds = 0.0f;
    // O2 PLACEHOLDER: the animation lock is named by Class-Kits (Edgework
    // removes it) but never timed. Mirrors the ability's own default.
    Cleave->WindowDuration = 0.45f;
    Registry.Add(Cleave);

    // C2 Closequarter — Class-Kits §2.2 row C2: 35 Mana, no cooldown.
    UBreakerAbilityDefinition* Closequarter = MakeFallback(TEXT("FallbackAbility_Caster_Closequarter"));
    Closequarter->AbilityId = TEXT("Caster.Closequarter");
    Closequarter->ClassId = EBreakerClassId::Caster;
    Closequarter->DisplayName = FText::FromString(TEXT("Closequarter"));
    Closequarter->Description = FText::FromString(TEXT("Blink to the target under the crosshair, arriving just short of it."));
    Closequarter->SlotAffinity = EBreakerAbilitySlot::ClassAbilityTwo;
    Closequarter->AbilityTag = BreakerAbilityTags::Ability_Class_Caster_Closequarter;
    Closequarter->AbilityClass = UBreakerAbility_Closequarter::StaticClass();
    Closequarter->ResourceCost = 35.0f;
    Closequarter->CooldownSeconds = 0.0f;
    Registry.Add(Closequarter);

    // C3 Rot — Class-Kits §2.2 row C3: 25 Mana, no cooldown, 4 m / 6 s.
    UBreakerAbilityDefinition* Rot = MakeFallback(TEXT("FallbackAbility_Caster_Rot"));
    Rot->AbilityId = TEXT("Caster.Rot");
    Rot->ClassId = EBreakerClassId::Caster;
    Rot->DisplayName = FText::FromString(TEXT("Rot"));
    Rot->Description = FText::FromString(TEXT("A 4 m zone that poisons and strips armour from everything standing in it."));
    Rot->SlotAffinity = EBreakerAbilitySlot::ClassAbilityTwo;
    Rot->AbilityTag = BreakerAbilityTags::Ability_Class_Caster_Rot;
    Rot->AbilityClass = UBreakerAbility_Rot::StaticClass();
    Rot->ResourceCost = 25.0f;
    Rot->CooldownSeconds = 0.0f;
    Rot->WindowDuration = 6.0f; // the puddle's lifetime, for the HUD
    Registry.Add(Rot);

    // C4 Siphon — Class-Kits §2.2 row C4: 30 Mana, no cooldown, 5 s channel.
    UBreakerAbilityDefinition* Siphon = MakeFallback(TEXT("FallbackAbility_Caster_Siphon"));
    Siphon->AbilityId = TEXT("Caster.Siphon");
    Siphon->ClassId = EBreakerClassId::Caster;
    Siphon->DisplayName = FText::FromString(TEXT("Siphon"));
    Siphon->Description = FText::FromString(TEXT("Channel on one target: Void damage over time that heals you for a portion."));
    Siphon->SlotAffinity = EBreakerAbilitySlot::ClassAbilityTwo;
    Siphon->AbilityTag = BreakerAbilityTags::Ability_Class_Caster_Siphon;
    Siphon->AbilityClass = UBreakerAbility_Siphon::StaticClass();
    Siphon->ResourceCost = 30.0f;
    Siphon->CooldownSeconds = 0.0f;
    Siphon->WindowDuration = 5.0f;
    Registry.Add(Siphon);

    // C5 Fracture — Class-Kits §2.2 row C5: 30 Mana, no cooldown.
    UBreakerAbilityDefinition* Fracture = MakeFallback(TEXT("FallbackAbility_Caster_Fracture"));
    Fracture->AbilityId = TEXT("Caster.Fracture");
    Fracture->ClassId = EBreakerClassId::Caster;
    Fracture->DisplayName = FText::FromString(TEXT("Fracture"));
    Fracture->Description = FText::FromString(TEXT("Projectile applying the next status in your cycle."));
    Fracture->SlotAffinity = EBreakerAbilitySlot::ClassAbilityTwo;
    Fracture->AbilityTag = BreakerAbilityTags::Ability_Class_Caster_Fracture;
    Fracture->AbilityClass = UBreakerAbility_Fracture::StaticClass();
    Fracture->ResourceCost = 30.0f;
    Fracture->CooldownSeconds = 0.0f;
    Registry.Add(Fracture);

    // C6 Resonance — Class-Kits §2.2 row C6: 40 Mana, no cooldown.
    UBreakerAbilityDefinition* Resonance = MakeFallback(TEXT("FallbackAbility_Caster_Resonance"));
    Resonance->AbilityId = TEXT("Caster.Resonance");
    Resonance->ClassId = EBreakerClassId::Caster;
    Resonance->DisplayName = FText::FromString(TEXT("Resonance"));
    Resonance->Description = FText::FromString(TEXT("Detonates every status on the target, consuming them."));
    Resonance->SlotAffinity = EBreakerAbilitySlot::ClassAbilityTwo;
    Resonance->AbilityTag = BreakerAbilityTags::Ability_Class_Caster_Resonance;
    Resonance->AbilityClass = UBreakerAbility_Resonance::StaticClass();
    Resonance->ResourceCost = 40.0f;
    Resonance->CooldownSeconds = 0.0f;
    Registry.Add(Resonance);

    // UNMAKE — Class-Kits §2.2 ultimate: 80 Mana, no cooldown, 6s base window.
    UBreakerAbilityDefinition* Unmake = MakeFallback(TEXT("FallbackAbility_Caster_Unmake"));
    Unmake->AbilityId = TEXT("Caster.Unmake");
    Unmake->ClassId = EBreakerClassId::Caster;
    Unmake->DisplayName = FText::FromString(TEXT("Unmake"));
    Unmake->Description = FText::FromString(TEXT("Caster abilities cost nothing and Mana generation stops."));
    Unmake->SlotAffinity = EBreakerAbilitySlot::Ultimate;
    Unmake->AbilityTag = BreakerAbilityTags::Ability_Class_Caster_Unmake;
    Unmake->AbilityClass = UBreakerAbility_Unmake::StaticClass();
    Unmake->ResourceCost = 80.0f;
    Unmake->CooldownSeconds = 0.0f;
    Unmake->WindowDuration = 6.0f;

    // Keystone variant rows (spec D1). Every duration and cost scalar below is
    // quoted from Class-Kits §2.2 — 6s/0% base, 12s/50% for Long Dark. Edgework
    // and Cascade change behavior, not parameters, so their rows carry the base
    // numbers and exist so the selector resolves them rather than silently
    // falling through to base.
    {
        FBreakerAbilityVariant BaseRow;
        BaseRow.VariantName = FText::FromString(TEXT("Unmake"));
        BaseRow.WindowDuration = 6.0f;
        BaseRow.AbilityCostMultiplier = 0.0f;
        Unmake->Variants.Add(BaseRow);

        FBreakerAbilityVariant Edgework;
        Edgework.KeystoneTag = BreakerAbilityTags::Keystone_Caster_Edgework;
        Edgework.VariantName = FText::FromString(TEXT("Unmake - Edgework"));
        Edgework.WindowDuration = 6.0f;
        Edgework.AbilityCostMultiplier = 0.0f;
        Unmake->Variants.Add(Edgework);

        FBreakerAbilityVariant LongDark;
        LongDark.KeystoneTag = BreakerAbilityTags::Keystone_Caster_LongDark;
        LongDark.VariantName = FText::FromString(TEXT("Unmake - Long Dark"));
        LongDark.WindowDuration = 12.0f;
        LongDark.AbilityCostMultiplier = 0.5f;
        Unmake->Variants.Add(LongDark);

        FBreakerAbilityVariant Cascade;
        Cascade.KeystoneTag = BreakerAbilityTags::Keystone_Caster_Cascade;
        Cascade.VariantName = FText::FromString(TEXT("Unmake - Cascade"));
        Cascade.WindowDuration = 6.0f;
        Cascade.AbilityCostMultiplier = 0.0f;
        Unmake->Variants.Add(Cascade);
    }
    Registry.Add(Unmake);

    // ==================================================================
    // GUNSMITH, TANK AND SUPPORT — IMPLEMENTED (owner authorization
    // 2026-08-16: "feel free to do all 5 classes").
    // ==================================================================
    // These twenty-one rows were registered as DATA ONLY with null
    // `AbilityClass`, and this block used to forbid nearest-fit pointing and
    // wait for "the day the abilities run". That day is this pass: every row
    // now names its real `UGameplayAbility` subclass
    // (Abilities/BreakerGunsmithAbilities.h / BreakerTankAbilities.h /
    // BreakerSupportAbilities.h), the minimal deployable system behind five of
    // them exists (Combat/BreakerDeployable.h — O30's hole, opened), the class
    // definitions are registered (Progression/BreakerProgressionLibrary.cpp)
    // AFTER the abilities execute — never before, which is the T7 ordering
    // that recreated the every-ability-reads-locked bug — and
    // `DefaultAbilityIdForSlot` below carries the three classes' starters.
    //
    // Costs and cooldowns remain QUOTED from the three class-kit documents
    // (per-row citations kept), all under the blanket O2 PLACEHOLDER banner:
    // the classes are PLAYABLE, not balanced. Behavioural gaps that could not
    // be built honestly (threat, stagger, status immunity, reload tempo, the
    // party layer) are recorded at each ability's own site, never faked.

    // ------------------------------------------------------------------
    // GUNSMITH — Scrap. Costs and cooldowns quoted from
    // Docs/Design/Class-Kits-Gunsmith.md §3.
    //
    // THE CLASS'S DEFINING ERGONOMIC IS VISIBLE IN THIS TABLE AND NOWHERE ELSE
    // IN CODE: the two Armory abilities cost NOTHING and carry a cooldown; the
    // four deployables cost Scrap and carry NO cooldown. Two clocks in one kit,
    // which is what makes "one of each" a real loadout shape rather than a
    // default. A future editor pass that "tidies" a cooldown onto a deployable
    // deletes the class's ergonomic, so the split is stated here as well as in
    // the design doc.
    // ------------------------------------------------------------------

    // G1 Sidearm Rig — §3 row G1: no cost, 10s cooldown. STARTER (Armory).
    UBreakerAbilityDefinition* SidearmRig = MakeFallback(TEXT("FallbackAbility_Gunsmith_SidearmRig"));
    SidearmRig->AbilityId = TEXT("Gunsmith.SidearmRig");
    SidearmRig->ClassId = EBreakerClassId::Gunsmith;
    SidearmRig->DisplayName = FText::FromString(TEXT("Sidearm Rig"));
    SidearmRig->Description = FText::FromString(TEXT("The next magazine deals bonus flat damage and pierces one more target."));
    SidearmRig->SlotAffinity = EBreakerAbilitySlot::ClassAbilityOne;
    SidearmRig->AbilityTag = Ability_Class_Gunsmith_SidearmRig;
    SidearmRig->CooldownTag = Cooldown_Class_Gunsmith_SidearmRig;
    SidearmRig->AbilityClass = UBreakerAbility_SidearmRig::StaticClass();
    SidearmRig->ResourceCost = 0.0f;
    SidearmRig->CooldownSeconds = 10.0f;
    // WindowDuration stays 0 DELIBERATELY. Sidearm Rig's window is counted in
    // SHOTS, not seconds — it ends when the magazine empties or on reload,
    // whichever comes first — and that is what makes it a magazine-economy
    // ability rather than a burst window. Authoring a seconds value here would
    // be inventing a second, contradictory expiry.
    Registry.Add(SidearmRig);

    // G2 Overhaul — §3 row G2: no cost, 18s cooldown, 10s window.
    UBreakerAbilityDefinition* Overhaul = MakeFallback(TEXT("FallbackAbility_Gunsmith_Overhaul"));
    Overhaul->AbilityId = TEXT("Gunsmith.Overhaul");
    Overhaul->ClassId = EBreakerClassId::Gunsmith;
    Overhaul->DisplayName = FText::FromString(TEXT("Overhaul"));
    Overhaul->Description = FText::FromString(TEXT("Converts reserve ammunition into magazine capacity, and settles the unspent remainder back."));
    Overhaul->SlotAffinity = EBreakerAbilitySlot::ClassAbilityTwo;
    Overhaul->AbilityTag = Ability_Class_Gunsmith_Overhaul;
    Overhaul->CooldownTag = Cooldown_Class_Gunsmith_Overhaul;
    Overhaul->AbilityClass = UBreakerAbility_Overhaul::StaticClass();
    Overhaul->ResourceCost = 0.0f;
    Overhaul->CooldownSeconds = 18.0f;
    Overhaul->WindowDuration = 10.0f;
    Registry.Add(Overhaul);

    // G3 Turret — §3 row G3: 40 Scrap, no cooldown. STARTER (Field Tech).
    UBreakerAbilityDefinition* Turret = MakeFallback(TEXT("FallbackAbility_Gunsmith_Turret"));
    Turret->AbilityId = TEXT("Gunsmith.Turret");
    Turret->ClassId = EBreakerClassId::Gunsmith;
    Turret->DisplayName = FText::FromString(TEXT("Turret"));
    Turret->Description = FText::FromString(TEXT("An emplacement that fires on the nearest target it can see. Consistent, never optimal."));
    Turret->SlotAffinity = EBreakerAbilitySlot::ClassAbilityTwo;
    Turret->AbilityTag = Ability_Class_Gunsmith_Turret;
    Turret->AbilityClass = UBreakerAbility_Turret::StaticClass();
    Turret->ResourceCost = 40.0f;
    Turret->CooldownSeconds = 0.0f;
    Turret->WindowDuration = 30.0f;   // the emplacement's lifetime, for the HUD
    Registry.Add(Turret);

    // G4 Ammo Crate — §3 row G4: 30 Scrap, no cooldown.
    UBreakerAbilityDefinition* AmmoCrate = MakeFallback(TEXT("FallbackAbility_Gunsmith_AmmoCrate"));
    AmmoCrate->AbilityId = TEXT("Gunsmith.AmmoCrate");
    AmmoCrate->ClassId = EBreakerClassId::Gunsmith;
    AmmoCrate->DisplayName = FText::FromString(TEXT("Ammo Crate"));
    AmmoCrate->Description = FText::FromString(TEXT("A crate of reserve ammunition. You are a valid interactor with your own, at full value."));
    AmmoCrate->SlotAffinity = EBreakerAbilitySlot::ClassAbilityTwo;
    AmmoCrate->AbilityTag = Ability_Class_Gunsmith_AmmoCrate;
    AmmoCrate->AbilityClass = UBreakerAbility_AmmoCrate::StaticClass();
    AmmoCrate->ResourceCost = 30.0f;
    AmmoCrate->CooldownSeconds = 0.0f;
    AmmoCrate->WindowDuration = 45.0f;
    Registry.Add(AmmoCrate);

    // G5 Mine Cluster — §3 row G5: 35 Scrap, no cooldown.
    UBreakerAbilityDefinition* MineCluster = MakeFallback(TEXT("FallbackAbility_Gunsmith_MineCluster"));
    MineCluster->AbilityId = TEXT("Gunsmith.MineCluster");
    MineCluster->ClassId = EBreakerClassId::Gunsmith;
    MineCluster->DisplayName = FText::FromString(TEXT("Mine Cluster"));
    MineCluster->Description = FText::FromString(TEXT("Three proximity charges that arm on a delay. The cluster is one placement, not three."));
    MineCluster->SlotAffinity = EBreakerAbilitySlot::ClassAbilityTwo;
    MineCluster->AbilityTag = Ability_Class_Gunsmith_MineCluster;
    MineCluster->AbilityClass = UBreakerAbility_MineCluster::StaticClass();
    MineCluster->ResourceCost = 35.0f;
    MineCluster->CooldownSeconds = 0.0f;
    MineCluster->WindowDuration = 60.0f;
    Registry.Add(MineCluster);

    // G6 Disruptor — §3 row G6: 45 Scrap, no cooldown, 20s lifetime.
    UBreakerAbilityDefinition* Disruptor = MakeFallback(TEXT("FallbackAbility_Gunsmith_Disruptor"));
    Disruptor->AbilityId = TEXT("Gunsmith.Disruptor");
    Disruptor->ClassId = EBreakerClassId::Gunsmith;
    Disruptor->DisplayName = FText::FromString(TEXT("Disruptor"));
    Disruptor->Description = FText::FromString(TEXT("A field that slows enemies inside it and strips a flat amount of their armour."));
    Disruptor->SlotAffinity = EBreakerAbilitySlot::ClassAbilityTwo;
    Disruptor->AbilityTag = Ability_Class_Gunsmith_Disruptor;
    Disruptor->AbilityClass = UBreakerAbility_Disruptor::StaticClass();
    Disruptor->ResourceCost = 45.0f;
    Disruptor->CooldownSeconds = 0.0f;
    Disruptor->WindowDuration = 20.0f;
    Registry.Add(Disruptor);

    // FIELD ASSEMBLY — §3 ultimate: 100 Scrap (full bar), no cooldown, 20s.
    UBreakerAbilityDefinition* FieldAssembly = MakeFallback(TEXT("FallbackAbility_Gunsmith_FieldAssembly"));
    FieldAssembly->AbilityId = TEXT("Gunsmith.FieldAssembly");
    FieldAssembly->ClassId = EBreakerClassId::Gunsmith;
    FieldAssembly->DisplayName = FText::FromString(TEXT("Field Assembly"));
    FieldAssembly->Description = FText::FromString(TEXT("Deploys every unlocked deployable type at once and raises the density cap for the window."));
    FieldAssembly->SlotAffinity = EBreakerAbilitySlot::Ultimate;
    FieldAssembly->AbilityTag = Ability_Class_Gunsmith_FieldAssembly;
    FieldAssembly->AbilityClass = UBreakerAbility_FieldAssembly::StaticClass();
    FieldAssembly->ResourceCost = 100.0f;
    FieldAssembly->CooldownSeconds = 0.0f;
    FieldAssembly->WindowDuration = 20.0f;
    {
        // NOTE ON AbilityCostMultiplier ACROSS ALL FOUR ROWS: it stays 1.0.
        // The field states what the window does to the price of the owner's
        // OTHER abilities, which is Unmake's mechanic. Field Assembly does not
        // discount casts — it performs one free mass placement at activation
        // and then raises a DENSITY CAP, and the cap is a deployable-system
        // concept with no field on this struct and no system behind it (O30).
        // Leaving the multiplier at 1.0 is the accurate statement; setting it
        // to 0 would silently make every subsequent Gunsmith cast free.
        FBreakerAbilityVariant BaseRow;
        BaseRow.VariantName = FText::FromString(TEXT("Field Assembly"));
        BaseRow.WindowDuration = 20.0f;
        FieldAssembly->Variants.Add(BaseRow);

        // Machinist — the solo / no-deployable ultimate: places nothing and
        // applies every unlocked type's effect to the player instead. The most
        // bespoke of the fifteen keystone rewrites in the corpus; the class-kit
        // document authors the SHAPE of its per-type mapping table and
        // explicitly not its magnitudes, so no parametric delta is authored
        // here either.
        FBreakerAbilityVariant Machinist;
        Machinist.KeystoneTag = Keystone_Gunsmith_Machinist;
        Machinist.VariantName = FText::FromString(TEXT("Field Assembly - Machinist"));
        Machinist.WindowDuration = 20.0f;
        FieldAssembly->Variants.Add(Machinist);

        // Foundry — deployables placed during the window never expire (their
        // lifetime clock pauses). A lifetime pause is not a window duration, so
        // this row carries the base 20s and the behaviour is a named C++ branch
        // when the deployable system exists (spec D1's explicit allowance).
        FBreakerAbilityVariant Foundry;
        Foundry.KeystoneTag = Keystone_Gunsmith_Foundry;
        Foundry.VariantName = FText::FromString(TEXT("Field Assembly - Foundry"));
        Foundry.WindowDuration = 20.0f;
        FieldAssembly->Variants.Add(Foundry);

        // Minefield — placements are invisible and excluded from enemy
        // perception until they act. Behavioural, not parametric.
        FBreakerAbilityVariant Minefield;
        Minefield.KeystoneTag = Keystone_Gunsmith_Minefield;
        Minefield.VariantName = FText::FromString(TEXT("Field Assembly - Minefield"));
        Minefield.WindowDuration = 20.0f;
        FieldAssembly->Variants.Add(Minefield);
    }
    Registry.Add(FieldAssembly);

    // ------------------------------------------------------------------
    // TANK — Grit. Costs and cooldowns quoted from
    // Docs/Design/Class-Kits-Tank.md §2. Every Tank ability costs Grit AND
    // carries a cooldown, and the cooldown band (5-12s) is the longest of the
    // five classes because Tank abilities are the most survivability-dense —
    // the invulnerability audit leans on cooldown LENGTH as its primary guard,
    // so shortening one of these numbers is a safety change, not a feel change.
    // ------------------------------------------------------------------

    // T1 Rend — §2 row T1: 25 Grit, 6s. STARTER (Leech).
    UBreakerAbilityDefinition* Rend = MakeFallback(TEXT("FallbackAbility_Tank_Rend"));
    Rend->AbilityId = TEXT("Tank.Rend");
    Rend->ClassId = EBreakerClassId::Tank;
    Rend->DisplayName = FText::FromString(TEXT("Rend"));
    Rend->Description = FText::FromString(TEXT("Melee sweep that heals for a portion of the damage dealt; overheal becomes shield."));
    Rend->SlotAffinity = EBreakerAbilitySlot::ClassAbilityOne;
    Rend->AbilityTag = Ability_Class_Tank_Rend;
    Rend->CooldownTag = Cooldown_Class_Tank_Rend;
    Rend->AbilityClass = UBreakerAbility_Rend::StaticClass();
    Rend->ResourceCost = 25.0f;
    Rend->CooldownSeconds = 6.0f;
    Registry.Add(Rend);

    // T2 Bloodline — §2 row T2: 40 Grit, 12s, 8s window.
    UBreakerAbilityDefinition* Bloodline = MakeFallback(TEXT("FallbackAbility_Tank_Bloodline"));
    Bloodline->AbilityId = TEXT("Tank.Bloodline");
    Bloodline->ClassId = EBreakerClassId::Tank;
    Bloodline->DisplayName = FText::FromString(TEXT("Bloodline"));
    Bloodline->Description = FText::FromString(TEXT("Doubles your Life on Hit and extends it to damage-over-time ticks. Multiplies what you have; grants nothing if you have none."));
    Bloodline->SlotAffinity = EBreakerAbilitySlot::ClassAbilityTwo;
    Bloodline->AbilityTag = Ability_Class_Tank_Bloodline;
    Bloodline->CooldownTag = Cooldown_Class_Tank_Bloodline;
    Bloodline->AbilityClass = UBreakerAbility_Bloodline::StaticClass();
    Bloodline->ResourceCost = 40.0f;
    Bloodline->CooldownSeconds = 12.0f;
    Bloodline->WindowDuration = 8.0f;
    Registry.Add(Bloodline);

    // T3 Anchor Point — §2 row T3: 30 Grit, 10s, 12s lifetime. STARTER (Bastion).
    UBreakerAbilityDefinition* AnchorPoint = MakeFallback(TEXT("FallbackAbility_Tank_AnchorPoint"));
    AnchorPoint->AbilityId = TEXT("Tank.AnchorPoint");
    AnchorPoint->ClassId = EBreakerClassId::Tank;
    AnchorPoint->DisplayName = FText::FromString(TEXT("Anchor Point"));
    AnchorPoint->Description = FText::FromString(TEXT("A frontal cover panel with its own health. Blocks enemy fire; you and allies shoot through it from behind."));
    AnchorPoint->SlotAffinity = EBreakerAbilitySlot::ClassAbilityTwo;
    AnchorPoint->AbilityTag = Ability_Class_Tank_AnchorPoint;
    AnchorPoint->CooldownTag = Cooldown_Class_Tank_AnchorPoint;
    AnchorPoint->AbilityClass = UBreakerAbility_AnchorPoint::StaticClass();
    AnchorPoint->ResourceCost = 30.0f;
    AnchorPoint->CooldownSeconds = 10.0f;
    AnchorPoint->WindowDuration = 12.0f;
    Registry.Add(AnchorPoint);

    // T4 Provoke — §2 row T4: 35 Grit, 12s, 4s forced-target window.
    UBreakerAbilityDefinition* Provoke = MakeFallback(TEXT("FallbackAbility_Tank_Provoke"));
    Provoke->AbilityId = TEXT("Tank.Provoke");
    Provoke->ClassId = EBreakerClassId::Tank;
    Provoke->DisplayName = FText::FromString(TEXT("Provoke"));
    Provoke->Description = FText::FromString(TEXT("Forces nearby enemies to target you, and each one provoked adds flat damage to your next seconds."));
    Provoke->SlotAffinity = EBreakerAbilitySlot::ClassAbilityTwo;
    Provoke->AbilityTag = Ability_Class_Tank_Provoke;
    Provoke->CooldownTag = Cooldown_Class_Tank_Provoke;
    Provoke->AbilityClass = UBreakerAbility_Provoke::StaticClass();
    Provoke->ResourceCost = 35.0f;
    Provoke->CooldownSeconds = 12.0f;
    Provoke->WindowDuration = 4.0f;
    Registry.Add(Provoke);

    // T5 Breach Charge — §2 row T5: 30 Grit, 8s.
    UBreakerAbilityDefinition* BreachCharge = MakeFallback(TEXT("FallbackAbility_Tank_BreachCharge"));
    BreachCharge->AbilityId = TEXT("Tank.BreachCharge");
    BreachCharge->ClassId = EBreakerClassId::Tank;
    BreachCharge->DisplayName = FText::FromString(TEXT("Breach Charge"));
    BreachCharge->Description = FText::FromString(TEXT("Thrown explosive with a short fuse. Full control of the self-knockback; the self-damage is reduced and never zero."));
    BreachCharge->SlotAffinity = EBreakerAbilitySlot::ClassAbilityTwo;
    BreachCharge->AbilityTag = Ability_Class_Tank_BreachCharge;
    BreachCharge->CooldownTag = Cooldown_Class_Tank_BreachCharge;
    BreachCharge->AbilityClass = UBreakerAbility_BreachCharge::StaticClass();
    BreachCharge->ResourceCost = 30.0f;
    BreachCharge->CooldownSeconds = 8.0f;
    Registry.Add(BreachCharge);

    // T6 Ground Zero — §2 row T6: 45 Grit, 10s.
    UBreakerAbilityDefinition* GroundZero = MakeFallback(TEXT("FallbackAbility_Tank_GroundZero"));
    GroundZero->AbilityId = TEXT("Tank.GroundZero");
    GroundZero->ClassId = EBreakerClassId::Tank;
    GroundZero->DisplayName = FText::FromString(TEXT("Ground Zero"));
    GroundZero->Description = FText::FromString(TEXT("Airborne slam that staggers, scaling with how far you fell. A normal jump is enough."));
    GroundZero->SlotAffinity = EBreakerAbilitySlot::ClassAbilityTwo;
    GroundZero->AbilityTag = Ability_Class_Tank_GroundZero;
    GroundZero->CooldownTag = Cooldown_Class_Tank_GroundZero;
    GroundZero->AbilityClass = UBreakerAbility_GroundZero::StaticClass();
    GroundZero->ResourceCost = 45.0f;
    GroundZero->CooldownSeconds = 10.0f;
    Registry.Add(GroundZero);

    // HOLD — §2.1 ultimate: 100 Grit (full bar), no cooldown, 10s base window.
    UBreakerAbilityDefinition* Hold = MakeFallback(TEXT("FallbackAbility_Tank_Hold"));
    Hold->AbilityId = TEXT("Tank.Hold");
    Hold->ClassId = EBreakerClassId::Tank;
    Hold->DisplayName = FText::FromString(TEXT("Hold"));
    Hold->Description = FText::FromString(TEXT("Caps the damage any single hit can do to you, and triples Grit generation for the duration."));
    Hold->SlotAffinity = EBreakerAbilitySlot::Ultimate;
    Hold->AbilityTag = Ability_Class_Tank_Hold;
    Hold->AbilityClass = UBreakerAbility_Hold::StaticClass();
    Hold->ResourceCost = 100.0f;
    Hold->CooldownSeconds = 0.0f;
    Hold->WindowDuration = 10.0f;
    {
        // A PER-HIT CAP IS NOT DAMAGE REDUCTION, and the struct has no field for
        // one. That is the right outcome rather than a gap to paper over: the
        // cap is a `min` composed against other caps, never a product, and
        // expressing it as a multiplier here is precisely the bug the audit's
        // third guard exists to prevent. All four rows therefore carry the
        // window and nothing else, and the cap lands as a named C++ branch on a
        // hook that does not exist yet.
        FBreakerAbilityVariant BaseRow;
        BaseRow.VariantName = FText::FromString(TEXT("Hold"));
        BaseRow.WindowDuration = 10.0f;
        Hold->Variants.Add(BaseRow);

        // Vein — the cap is REMOVED and incoming damage instead converts to
        // healing at a reduced rate: a damped attrition window, explicitly not
        // immunity. A Tank inside Vein still dies to enough incoming damage,
        // and that is the guard rather than a caveat.
        FBreakerAbilityVariant Vein;
        Vein.KeystoneTag = Keystone_Tank_Vein;
        Vein.VariantName = FText::FromString(TEXT("Hold - Vein"));
        Vein.WindowDuration = 10.0f;
        Hold->Variants.Add(Vein);

        // Wall — the cap extends to nearby allies, and SOLO it is twice as
        // effective on the Tank. Party and solo are two different good outcomes
        // rather than a party bonus with a solo penalty, which is what the
        // party branch owing the strongest solo conversion actually means.
        FBreakerAbilityVariant Wall;
        Wall.KeystoneTag = Keystone_Tank_Wall;
        Wall.VariantName = FText::FromString(TEXT("Hold - Wall"));
        Wall.WindowDuration = 10.0f;
        Hold->Variants.Add(Wall);

        // Detonation — ends early on command, releasing the absorbed damage as
        // a radial blast. The only ability in the game needing a second input
        // binding, and the one place the class exempts itself from its own
        // self-damage: the exemption is on the ultimate, not on a repeatable
        // ability, so the rocket case O13 governs is untouched.
        FBreakerAbilityVariant Detonation;
        Detonation.KeystoneTag = Keystone_Tank_Detonation;
        Detonation.VariantName = FText::FromString(TEXT("Hold - Detonation"));
        Detonation.WindowDuration = 10.0f;
        Hold->Variants.Add(Detonation);
    }
    Registry.Add(Hold);

    // ------------------------------------------------------------------
    // SUPPORT — Charge. Costs and cooldowns quoted from
    // Docs/Design/Class-Kits-Support.md §3. Costs run 20-40 and cooldowns 5-10s.
    // Mark is deliberately the cheapest and shortest: it is the loop's ignition,
    // and a loop whose ignition is expensive stalls at low Charge. Suppress is
    // the most expensive because it is the only ability with no generation
    // attached to it.
    // ------------------------------------------------------------------

    // U1 Patch — §3 row U1: 25 Charge, 6s. STARTER (Medic).
    UBreakerAbilityDefinition* Patch = MakeFallback(TEXT("FallbackAbility_Support_Patch"));
    Patch->AbilityId = TEXT("Support.Patch");
    Patch->ClassId = EBreakerClassId::Support;
    Patch->DisplayName = FText::FromString(TEXT("Patch"));
    Patch->Description = FText::FromString(TEXT("Heals the ally under your crosshair, or yourself with no target. The self-cast is worth exactly the same."));
    Patch->SlotAffinity = EBreakerAbilitySlot::ClassAbilityOne;
    Patch->AbilityTag = Ability_Class_Support_Patch;
    Patch->CooldownTag = Cooldown_Class_Support_Patch;
    Patch->AbilityClass = UBreakerAbility_Patch::StaticClass();
    Patch->ResourceCost = 25.0f;
    Patch->CooldownSeconds = 6.0f;
    Registry.Add(Patch);

    // U2 Purge — §3 row U2: 30 Charge, 10s, 3s status immunity.
    UBreakerAbilityDefinition* Purge = MakeFallback(TEXT("FallbackAbility_Support_Purge"));
    Purge->AbilityId = TEXT("Support.Purge");
    Purge->ClassId = EBreakerClassId::Support;
    Purge->DisplayName = FText::FromString(TEXT("Purge"));
    Purge->Description = FText::FromString(TEXT("Strips every status from the target and makes them immune to more for a moment. Self-castable."));
    Purge->SlotAffinity = EBreakerAbilitySlot::ClassAbilityTwo;
    Purge->AbilityTag = Ability_Class_Support_Purge;
    Purge->CooldownTag = Cooldown_Class_Support_Purge;
    Purge->AbilityClass = UBreakerAbility_Purge::StaticClass();
    Purge->ResourceCost = 30.0f;
    Purge->CooldownSeconds = 10.0f;
    Purge->WindowDuration = 3.0f;   // the immunity window, which is the whole value
    Registry.Add(Purge);

    // U3 Cadence — §3 row U3: 30 Charge, 8s, 8s aura.
    UBreakerAbilityDefinition* Cadence = MakeFallback(TEXT("FallbackAbility_Support_Cadence"));
    Cadence->AbilityId = TEXT("Support.Cadence");
    Cadence->ClassId = EBreakerClassId::Support;
    Cadence->DisplayName = FText::FromString(TEXT("Cadence"));
    Cadence->Description = FText::FromString(TEXT("An aura that follows you, improving reload and swap tempo for everyone inside it — starting with you."));
    Cadence->SlotAffinity = EBreakerAbilitySlot::ClassAbilityTwo;
    Cadence->AbilityTag = Ability_Class_Support_Cadence;
    Cadence->CooldownTag = Cooldown_Class_Support_Cadence;
    Cadence->AbilityClass = UBreakerAbility_Cadence::StaticClass();
    Cadence->ResourceCost = 30.0f;
    Cadence->CooldownSeconds = 8.0f;
    Cadence->WindowDuration = 8.0f;
    Registry.Add(Cadence);

    // U4 Metronome — §3 row U4: 35 Charge, 9s, 8s state.
    UBreakerAbilityDefinition* Metronome = MakeFallback(TEXT("FallbackAbility_Support_Metronome"));
    Metronome->AbilityId = TEXT("Support.Metronome");
    Metronome->ClassId = EBreakerClassId::Support;
    Metronome->DisplayName = FText::FromString(TEXT("Metronome"));
    Metronome->Description = FText::FromString(TEXT("Consecutive hits by anyone you have buffed build a cadence ramp. Each holder builds their own."));
    Metronome->SlotAffinity = EBreakerAbilitySlot::ClassAbilityTwo;
    Metronome->AbilityTag = Ability_Class_Support_Metronome;
    Metronome->CooldownTag = Cooldown_Class_Support_Metronome;
    Metronome->AbilityClass = UBreakerAbility_Metronome::StaticClass();
    Metronome->ResourceCost = 35.0f;
    Metronome->CooldownSeconds = 9.0f;
    Metronome->WindowDuration = 8.0f;
    Registry.Add(Metronome);

    // U5 Mark — §3 row U5: 20 Charge, 5s, 10s mark. STARTER (Warden).
    UBreakerAbilityDefinition* Mark = MakeFallback(TEXT("FallbackAbility_Support_Mark"));
    Mark->AbilityId = TEXT("Support.Mark");
    Mark->ClassId = EBreakerClassId::Support;
    Mark->DisplayName = FText::FromString(TEXT("Mark"));
    Mark->Description = FText::FromString(TEXT("Paints a target: it takes more damage from everyone, and the damage you do to it pays you Charge."));
    Mark->SlotAffinity = EBreakerAbilitySlot::ClassAbilityTwo;
    Mark->AbilityTag = Ability_Class_Support_Mark;
    Mark->CooldownTag = Cooldown_Class_Support_Mark;
    Mark->AbilityClass = UBreakerAbility_Mark::StaticClass();
    Mark->ResourceCost = 20.0f;
    Mark->CooldownSeconds = 5.0f;
    Mark->WindowDuration = 10.0f;
    Registry.Add(Mark);

    // U6 Suppress — §3 row U6: 40 Charge, 10s, 6s zone.
    UBreakerAbilityDefinition* Suppress = MakeFallback(TEXT("FallbackAbility_Support_Suppress"));
    Suppress->AbilityId = TEXT("Support.Suppress");
    Suppress->ClassId = EBreakerClassId::Support;
    Suppress->DisplayName = FText::FromString(TEXT("Suppress"));
    Suppress->Description = FText::FromString(TEXT("A zone that slows enemies and spoils their aim. It deals no damage at all."));
    Suppress->SlotAffinity = EBreakerAbilitySlot::ClassAbilityTwo;
    Suppress->AbilityTag = Ability_Class_Support_Suppress;
    Suppress->CooldownTag = Cooldown_Class_Support_Suppress;
    Suppress->AbilityClass = UBreakerAbility_Suppress::StaticClass();
    Suppress->ResourceCost = 40.0f;
    Suppress->CooldownSeconds = 10.0f;
    Suppress->WindowDuration = 6.0f;
    Registry.Add(Suppress);

    // CONDUIT — §3.1 ultimate: 100 Charge (full bar), no cooldown, 12s base.
    UBreakerAbilityDefinition* Conduit = MakeFallback(TEXT("FallbackAbility_Support_Conduit"));
    Conduit->AbilityId = TEXT("Support.Conduit");
    Conduit->ClassId = EBreakerClassId::Support;
    Conduit->DisplayName = FText::FromString(TEXT("Conduit"));
    Conduit->Description = FText::FromString(TEXT("For the duration your abilities cost nothing and reach every valid target near you — always including yourself."));
    Conduit->SlotAffinity = EBreakerAbilitySlot::Ultimate;
    Conduit->AbilityTag = Ability_Class_Support_Conduit;
    Conduit->AbilityClass = UBreakerAbility_Conduit::StaticClass();
    Conduit->ResourceCost = 100.0f;
    Conduit->CooldownSeconds = 0.0f;
    Conduit->WindowDuration = 12.0f;
    {
        // THE ONE ULTIMATE OF THE THREE WHOSE KEYSTONES DIFFER PARAMETRICALLY,
        // and the difference is real rather than cosmetic: two of the three
        // rewrites STOP the free-cast window, because they replace casting
        // rather than enabling it. That is exactly what AbilityCostMultiplier
        // exists to say (Caster's Unmake authors 0.0 and 0.5 through the same
        // field), so these rows carry behaviour and not just identity.
        //
        // Base — free casts, breadth not spam: cooldowns still apply, so the
        // window removes the cost gate and not the cadence gate. Generation
        // deliberately CONTINUES, which partially refunds the ultimate and,
        // bounded by those cooldowns, cannot fully refund it.
        FBreakerAbilityVariant BaseRow;
        BaseRow.VariantName = FText::FromString(TEXT("Conduit"));
        BaseRow.WindowDuration = 12.0f;
        BaseRow.AbilityCostMultiplier = 0.0f;
        Conduit->Variants.Add(BaseRow);

        // Triage — becomes a continuous healing field with one lethal-hit save
        // per target, and STOPS enabling free casts. Hence 1.0, not 0.0: the
        // defensive ultimate buys a field, not a spending spree, and authoring
        // 0.0 here would quietly hand it both.
        FBreakerAbilityVariant Triage;
        Triage.KeystoneTag = Keystone_Support_Triage;
        Triage.VariantName = FText::FromString(TEXT("Conduit - Triage"));
        Triage.WindowDuration = 12.0f;
        Triage.AbilityCostMultiplier = 1.0f;
        Conduit->Variants.Add(Triage);

        // Downbeat — keeps the free casts and doubles the cadence effects,
        // adding flat weapon damage for every buffed target. Flat is
        // load-bearing: it enters the flat-sum stage before the additive
        // Increased bucket and therefore cannot double-dip with gear.
        FBreakerAbilityVariant Downbeat;
        Downbeat.KeystoneTag = Keystone_Support_Downbeat;
        Downbeat.VariantName = FText::FromString(TEXT("Conduit - Downbeat"));
        Downbeat.WindowDuration = 12.0f;
        Downbeat.AbilityCostMultiplier = 0.0f;
        Conduit->Variants.Add(Downbeat);

        // Blackout — marks and suppresses every enemy in radius INSTEAD of
        // casting abilities, so the free-cast discount is meaningless and is
        // not authored. The control ultimate, and the one that turns a full bar
        // straight into damage.
        FBreakerAbilityVariant Blackout;
        Blackout.KeystoneTag = Keystone_Support_Blackout;
        Blackout.VariantName = FText::FromString(TEXT("Conduit - Blackout"));
        Blackout.WindowDuration = 12.0f;
        Blackout.AbilityCostMultiplier = 1.0f;
        Conduit->Variants.Add(Blackout);
    }
    Registry.Add(Conduit);

    return Registry;
}

UBreakerAbilityDefinition* UBreakerAbilityDefinition::FindFallback(FName InAbilityId)
{
    if (InAbilityId.IsNone())
    {
        return nullptr;
    }
    for (UBreakerAbilityDefinition* Definition : GetFallbackRegistry())
    {
        if (Definition && Definition->AbilityId == InAbilityId)
        {
            return Definition;
        }
    }
    return nullptr;
}

TArray<UBreakerAbilityDefinition*> UBreakerAbilityDefinition::GetClassAbilities(EBreakerClassId ClassId, EBreakerAbilitySlot Slot)
{
    TArray<UBreakerAbilityDefinition*> Result;
    if (ClassId == EBreakerClassId::None) return Result;
    for (UBreakerAbilityDefinition* Definition : GetFallbackRegistry())
    {
        // CanOccupySlot is the slot rule in one place: an ultimate may only sit
        // in the ultimate slot, and a class ability may sit in either of the
        // two. Reading it here means the picker and the equip validation cannot
        // disagree about what fits where.
        if (Definition && Definition->ClassId == ClassId && Definition->CanOccupySlot(Slot))
        {
            Result.Add(Definition);
        }
    }
    return Result;
}

TArray<FName> UBreakerAbilityDefinition::GetClassAbilityIds(EBreakerClassId ClassId, EBreakerAbilitySlot Slot)
{
    TArray<FName> Ids;
    for (const UBreakerAbilityDefinition* Definition : GetClassAbilities(ClassId, Slot))
    {
        Ids.Add(Definition->AbilityId);
    }
    return Ids;
}

bool UBreakerAbilityDefinition::ClassGrantsAbility(EBreakerClassId ClassId, FName AbilityId)
{
    if (ClassId == EBreakerClassId::None || AbilityId.IsNone()) return false;
    const UBreakerAbilityDefinition* Definition = FindFallback(AbilityId);
    return Definition && Definition->ClassId == ClassId;
}

bool UBreakerAbilityDefinition::ClassHasImplementedKit(EBreakerClassId ClassId)
{
    if (ClassId == EBreakerClassId::None) return false;
    // ONE implemented ability is enough to say a kit exists — the same
    // any-of shape the class screen's own gate uses — because the question is
    // whether locking into this class grants a player anything that runs, not
    // whether the kit is finished. Deliberately does NOT consult
    // GetFallbackClassDefinition: that is the progression layer's separate
    // gate on the same decision, and two independent answers to "is this class
    // real" is how the Caster null-definition bug stayed invisible.
    for (const UBreakerAbilityDefinition* Definition : GetFallbackRegistry())
    {
        if (Definition && Definition->ClassId == ClassId && Definition->IsImplemented()) return true;
    }
    return false;
}

FName UBreakerAbilityDefinition::DefaultAbilityIdForSlot(EBreakerClassId ClassId, EBreakerAbilitySlot Slot)
{
    // This is the whole reachability chain for a class with no authored class
    // definition: UBreakerProgressionComponent::ChoosePermanentClassById leaves
    // the loadout FNames as None for anything but Swift, and
    // UBreakerAbilityComponent::ResolveDefinition then asks here. A class
    // missing from this switch has three dead keys.
    switch (ClassId)
    {
    case EBreakerClassId::Swift:
        switch (Slot)
        {
        case EBreakerAbilitySlot::ClassAbilityOne: return TEXT("Swift.Skim");
        case EBreakerAbilitySlot::ClassAbilityTwo: return TEXT("Swift.Lead");
        case EBreakerAbilitySlot::Ultimate:        return TEXT("Swift.Overdrive");
        default: return NAME_None;
        }
    case EBreakerClassId::Caster:
        switch (Slot)
        {
        // Class-Kits §2.2 names Cleave and Rot as the Caster STARTERS, so they
        // are what an unchosen loadout is handed. This is now a DEFAULT and no
        // longer the only answer: GetClassAbilities is the catalogue,
        // UBreakerAbilityComponent::TryEquipAbility is how a choice is made,
        // and ResolveDefinition prefers the chosen id over this row. Editing
        // this line changes what a fresh character starts with — it no longer
        // decides which four abilities are unreachable.
        case EBreakerAbilitySlot::ClassAbilityOne: return TEXT("Caster.Cleave");
        case EBreakerAbilitySlot::ClassAbilityTwo: return TEXT("Caster.Rot");
        case EBreakerAbilitySlot::Ultimate:        return TEXT("Caster.Unmake");
        default: return NAME_None;
        }
    // The three formerly-unbuilt classes (implemented 2026-08-16, owner
    // authorization). Each default pair is the kit's two STARTERS, sitting on
    // opposite sides of the class tension exactly as the treatments name them:
    // Gunsmith holds a gun buff and a turret (Class-Kits-Gunsmith §3), Tank
    // holds sustain and position (Class-Kits-Tank §2), Support holds a heal
    // and a mark (Class-Kits-Support §3).
    case EBreakerClassId::Gunsmith:
        switch (Slot)
        {
        case EBreakerAbilitySlot::ClassAbilityOne: return TEXT("Gunsmith.SidearmRig");
        case EBreakerAbilitySlot::ClassAbilityTwo: return TEXT("Gunsmith.Turret");
        case EBreakerAbilitySlot::Ultimate:        return TEXT("Gunsmith.FieldAssembly");
        default: return NAME_None;
        }
    case EBreakerClassId::Tank:
        switch (Slot)
        {
        case EBreakerAbilitySlot::ClassAbilityOne: return TEXT("Tank.Rend");
        case EBreakerAbilitySlot::ClassAbilityTwo: return TEXT("Tank.AnchorPoint");
        case EBreakerAbilitySlot::Ultimate:        return TEXT("Tank.Hold");
        default: return NAME_None;
        }
    case EBreakerClassId::Support:
        switch (Slot)
        {
        case EBreakerAbilitySlot::ClassAbilityOne: return TEXT("Support.Patch");
        case EBreakerAbilitySlot::ClassAbilityTwo: return TEXT("Support.Mark");
        case EBreakerAbilitySlot::Ultimate:        return TEXT("Support.Conduit");
        default: return NAME_None;
        }
    default:
        return NAME_None;
    }
}
