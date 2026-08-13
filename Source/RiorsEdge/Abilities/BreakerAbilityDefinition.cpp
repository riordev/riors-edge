#include "Abilities/BreakerAbilityDefinition.h"

#include "Abilities/BreakerAbilityTags.h"
#include "Abilities/BreakerAbility_Cleave.h"
#include "Abilities/BreakerAbility_Closequarter.h"
#include "Abilities/BreakerAbility_Lead.h"
#include "Abilities/BreakerAbility_Unmake.h"
#include "Abilities/BreakerAbility_Overdrive.h"
#include "Abilities/BreakerAbility_Skim.h"

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
        // Class-Kits §2.2 names Cleave and Rot as the Caster starters. Rot needs
        // the zone actor system, which does not exist, so slot two ships
        // Closequarter — the other Spellblade ability and the one buildable
        // without new Combat/ systems. Swap it for Rot when zones land.
        case EBreakerAbilitySlot::ClassAbilityOne: return TEXT("Caster.Cleave");
        case EBreakerAbilitySlot::ClassAbilityTwo: return TEXT("Caster.Closequarter");
        case EBreakerAbilitySlot::Ultimate:        return TEXT("Caster.Unmake");
        default: return NAME_None;
        }
    default:
        return NAME_None;
    }
}
