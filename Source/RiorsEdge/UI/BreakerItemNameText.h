#pragma once

#include "CoreMinimal.h"
#include "Items/BreakerAffixLibrary.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerItemNaming.h"
#include "Items/BreakerItemRules.h"
#include "Items/BreakerItemTypes.h"
#include "Weapons/BreakerWeaponArchetype.h"

// ---------------------------------------------------------------------------
// WHAT AN ITEM IS CALLED ON SCREEN — one rule, because three screens were
// answering the question separately and two of them disagreed.
//
// The GRAMMAR is not here: it is pure, world-free and proved on a bare array
// in Items/BreakerItemNaming.h (O267). What is here is the lookup that turns a
// rolled affix id into its word and the base every screen wraps that grammar
// around — the part that needs the libraries and therefore could not live
// beside the rule.
//
// THE DEFECT THIS CLOSES: ItemDisplayName was file-local to BreakerMenu.cpp,
// so the swap picker grew its own copy of the same four answers
// (BreakerSwapItemName) and never learned the O267 grammar when it landed. The
// picker and the inventory card printed DIFFERENT NAMES for the same item. The
// stash carried a third copy of the eight slot words for the same reason. That
// is the duplicate-concept mistake the CastSpeed lane already charged this
// project for once, and the answer is the same: one definition, called.
//
// TWO NAMES, DELIBERATELY, AND THE DIFFERENCE IS NAMED HERE RATHER THAN
// ACCIDENTAL. DisplayName is the full headline and is what a card prints.
// BaseName is the item's kind alone, for a row too narrow to hold a headline:
// the swap picker's row is 64 px tall in a 640 px plate, which leaves its name
// column about 336 px against roughly 440 px of "Sustained Accuracy Sidearm of
// Cast Speed". Widening that row is a change to the owner's own sheet and is
// his to make, so the picker asks for the base ON PURPOSE and says so at its
// call site. It is no longer able to disagree with the card by accident.
// ---------------------------------------------------------------------------
namespace BreakerItemNameText
{
    // The eight slot words, once. The stash and the loadout both had their own
    // copy and their own comment apologising for it.
    inline const TCHAR* SlotWord(EBreakerEquipSlot Slot)
    {
        switch (Slot)
        {
            case EBreakerEquipSlot::Helmet:     return TEXT("HELMET");
            case EBreakerEquipSlot::BodyArmour: return TEXT("BODY ARMOUR");
            case EBreakerEquipSlot::Gloves:     return TEXT("GLOVES");
            case EBreakerEquipSlot::Boots:      return TEXT("BOOTS");
            case EBreakerEquipSlot::Necklace:   return TEXT("NECKLACE");
            case EBreakerEquipSlot::Waist:      return TEXT("WAIST");
            case EBreakerEquipSlot::Primary:    return TEXT("PRIMARY");
            case EBreakerEquipSlot::Secondary:  return TEXT("SECONDARY");
            default:                            return TEXT("SLOT");
        }
    }

    // The two items in the game that are CALLED something rather than
    // identified by what they are: a legendary carries authored copy, and the
    // starter rifle is the one non-legendary piece every character is
    // guaranteed to meet. Empty for every rolled drop, which is the signal to
    // fall through to the grammar.
    //
    // A LEGENDARY IS NEVER RENAMED BY THE GRAMMAR. One that called itself
    // "Havoc Riftplate of Cast Speed" would bury the only name in the game
    // somebody actually wrote.
    inline FString AuthoredName(const FBreakerItemInstance& Item)
    {
        if (Item.IsLegendary())
        {
            const FBreakerLegendaryDefinition Legendary = UBreakerItemRuleLibrary::FindLegendary(Item.LegendaryId);
            if (Legendary.IsValid() && !Legendary.DisplayName.IsEmpty())
            {
                return Legendary.DisplayName.ToString().ToUpper();
            }
        }
        if (Item.DefinitionId == UBreakerEquipmentComponent::StarterRifleDefinitionId)
        {
            return TEXT("ISSUE RIFLE");
        }
        return FString();
    }

    // The base the grammar wraps: a weapon is its ARCHETYPE, armour its slot.
    // The archetype is the one fact not already implied by where the card
    // sits, and weapon drops randomise it, so it is the only place the class
    // is visible before equipping.
    //
    // ONE WORD, ALWAYS, and that is a NAME rule rather than a label rule. The
    // slot words and archetype names stay exactly as they are wherever they are
    // read as labels — a card's rarity line still says BODY ARMOUR. Three of
    // them are two words, and inside a name each would spend a word the
    // three-word ceiling does not have: "Fleet Body Armour of Warding" is four.
    // So the name takes the half that identifies the thing.
    inline FString KindWord(const FBreakerItemInstance& Item)
    {
        const FString Label = Item.IsWeapon()
            ? BreakerWeaponArchetypeNames::Short(Item.WeaponArchetype)
            : FString(SlotWord(Item.Slot));
        if (Label.Equals(TEXT("BODY ARMOUR"))) return TEXT("PLATE");
        if (Label.Equals(TEXT("ROCKET LAUNCHER"))) return TEXT("LAUNCHER");
        if (Label.Equals(TEXT("BURST RIFLE"))) return TEXT("BURSTGUN");
        return Label;
    }

    // The word a rolled line contributes to the name — the affix's authored
    // NameWord, not its display name.
    //
    // THIS IS THE FIX FOR THE OWNER'S OWN BOOTS. O267 built a name out of
    // display names, so "Movement Speed" + BOOTS + "Ailment Avoidance" came out
    // as a six-word sentence. A display name has to describe a stat; a name
    // word has to be a name. They are different jobs and now they are different
    // fields. Empty is impossible for an authored line — the library refuses to
    // load an affix without one — so an empty here means an unauthored id, and
    // the composer drops that half rather than inventing words for it.
    inline FString AffixWord(const FBreakerRolledAffix& Rolled)
    {
        const FBreakerAffixLibraryData& Library = UBreakerAffixLibrary::GetData();
        for (const TArray<FBreakerAffixDefinition>* Pool : { &Library.Slice, &Library.Aberrant,
            &Library.Unwritten, &Library.Downsides })
            for (const FBreakerAffixDefinition& Affix : *Pool)
                if (Affix.AffixId == Rolled.AffixId) return Affix.NameWord;
        if (Library.Elemental.AffixId == Rolled.AffixId) return Library.Elemental.NameWord;
        return FString();
    }

    // The item's KIND alone — no headline. For a row whose width cannot hold
    // one; see the note at the top of this file.
    inline FString BaseName(const FBreakerItemInstance& Item)
    {
        const FString Authored = AuthoredName(Item);
        return Authored.IsEmpty() ? KindWord(Item) : Authored;
    }

    // The full headline: strongest prefix, base, "of" strongest suffix. An
    // item with no rolled lines is named by its base alone, which is correct
    // rather than a fallback.
    inline FString DisplayName(const FBreakerItemInstance& Item)
    {
        const FString Authored = AuthoredName(Item);
        if (!Authored.IsEmpty()) return Authored;
        using namespace BreakerItemNaming;
        const FBreakerRolledAffix* Prefix = StrongestOfCategory(Item.Affixes, EBreakerAffixCategory::Prefix);
        const FBreakerRolledAffix* Suffix = StrongestOfCategory(Item.Affixes, EBreakerAffixCategory::Suffix);
        return Compose(Prefix ? AffixWord(*Prefix) : FString(), KindWord(Item),
            Suffix ? AffixWord(*Suffix) : FString());
    }
}
