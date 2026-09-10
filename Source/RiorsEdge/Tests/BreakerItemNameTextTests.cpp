#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Items/BreakerAffixLibrary.h"
#include "Items/BreakerEquipmentComponent.h"
#include "UI/BreakerItemNameText.h"
#include "UI/BreakerStashLayout.h"

// ---------------------------------------------------------------------------
// ONE NAME PER ITEM, ACROSS EVERY SCREEN THAT PRINTS ONE.
//
// The defect this pins was silent and shipped: ItemDisplayName was file-local
// to BreakerMenu.cpp, the swap picker carried a hand-copied duplicate, and when
// O267 gave rolled items a headline only the original learned it. The inventory
// card and the swap picker then printed DIFFERENT NAMES for the same item, and
// nothing anywhere could see it.
//
// So the assertions below are about AGREEMENT, not about wording: the two
// answers a screen may ask for are the headline and the kind, and the kind must
// be part of the headline rather than a second opinion about what the item is.
// ---------------------------------------------------------------------------

namespace
{
    FBreakerRolledAffix BreakerItemNameTextRoll(FName Id, int32 Tier, EBreakerAffixCategory Category)
    {
        FBreakerRolledAffix Affix;
        Affix.AffixId = Id;
        Affix.Tier = Tier;
        Affix.Category = Category;
        return Affix;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerItemNameTextTest,
    "RiorsEdge.UI.ItemName.OnePath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerItemNameTextTest::RunTest(const FString& Parameters)
{
    using namespace BreakerItemNameText;

    // EVERY SLOT HAS A WORD. The fallback exists for an out-of-range value, so
    // a slot that reaches it is an appended enum entry nobody named — which
    // would print "SLOT" on a real cell in three screens at once.
    for (int32 Index = 0; Index < static_cast<int32>(EBreakerEquipSlot::Count); ++Index)
    {
        const EBreakerEquipSlot Slot = static_cast<EBreakerEquipSlot>(Index);
        TestNotEqual(FString::Printf(TEXT("slot %d is named"), Index),
            FString(SlotWord(Slot)), FString(TEXT("SLOT")));
        TestEqual(FString::Printf(TEXT("the stash reads the same word for slot %d"), Index),
            FString(BreakerStashLayout::SlotWord(Slot)), FString(SlotWord(Slot)));
    }

    // Unrolled armour is named by what it is. Nothing to headline, so the two
    // answers are the same string.
    FBreakerItemInstance Boots;
    Boots.Slot = EBreakerEquipSlot::Boots;
    TestEqual(TEXT("unrolled armour is named by its slot"), DisplayName(Boots), FString(TEXT("BOOTS")));
    TestEqual(TEXT("and the picker's kind agrees"), BaseName(Boots), DisplayName(Boots));

    // The starter rifle: the one non-legendary item that is CALLED something.
    // The grammar must not wrap it, on either path.
    FBreakerItemInstance Starter;
    Starter.Slot = EBreakerEquipSlot::Primary;
    Starter.DefinitionId = UBreakerEquipmentComponent::StarterRifleDefinitionId;
    TestEqual(TEXT("the starter keeps its issued name"), DisplayName(Starter), FString(TEXT("ISSUE RIFLE")));
    TestEqual(TEXT("and the picker gives that same name"), BaseName(Starter), DisplayName(Starter));

    // THE REGRESSION ITSELF, against real authored affixes rather than invented
    // ones: a rolled item leads with its strongest prefix and closes with its
    // strongest suffix, and still says what it is in between.
    const FBreakerAffixLibraryData& Library = UBreakerAffixLibrary::GetData();
    const FBreakerAffixDefinition* Prefix = nullptr;
    const FBreakerAffixDefinition* Suffix = nullptr;
    for (const FBreakerAffixDefinition& Affix : Library.Slice)
    {
        if (Affix.DisplayName.IsEmpty()) continue;
        if (!Prefix && Affix.Category == EBreakerAffixCategory::Prefix) Prefix = &Affix;
        if (!Suffix && Affix.Category == EBreakerAffixCategory::Suffix) Suffix = &Affix;
    }
    if (!TestNotNull(TEXT("the authored library supplies a named prefix"), Prefix)) return false;
    if (!TestNotNull(TEXT("the authored library supplies a named suffix"), Suffix)) return false;

    FBreakerItemInstance Rolled = Boots;
    Rolled.Affixes.Add(BreakerItemNameTextRoll(Prefix->AffixId, 1, EBreakerAffixCategory::Prefix));
    Rolled.Affixes.Add(BreakerItemNameTextRoll(Suffix->AffixId, 1, EBreakerAffixCategory::Suffix));

    const FString Headline = DisplayName(Rolled);
    // THE AUTHORED NAME WORD, NOT THE DISPLAY NAME. A display name describes a
    // stat and is as long as it needs to be; a name word is a name. Building
    // names out of display names is what produced "Movement Speed BOOTS of
    // Ailment Avoidance" in the owner's own inventory.
    TestTrue(TEXT("the headline leads with the prefix's name word"),
        Headline.StartsWith(Prefix->NameWord));
    TestTrue(TEXT("the headline closes with 'of' the suffix's name word"),
        Headline.EndsWith(FString(TEXT(" of ")) + Suffix->NameWord));
    TestTrue(*FString::Printf(TEXT("and it is at most %d words: \"%s\""),
            BreakerItemNaming::MaxNameWords, *Headline),
        BreakerItemNaming::CountNameWords(Headline) <= BreakerItemNaming::MaxNameWords);

    // ---- THE CEILING, OVER THE WHOLE LIBRARY -----------------------------
    // Owner-ruled: "it should at max be 3 words". One sample name proves the
    // grammar; this proves the CONTENT, which is where a three-word rule
    // actually breaks — a single affix authored with a two-word name word
    // lengthens every item that ever rolls it, and nothing else would notice.
    // Composed against the widest base in the game, so a name that fits here
    // fits everywhere.
    int32 Checked = 0;
    const auto SweepPool = [&](const TArray<FBreakerAffixDefinition>& Pool)
    {
        for (const FBreakerAffixDefinition& Affix : Pool)
        {
            TestFalse(*FString::Printf(TEXT("%s authors a name word"), *Affix.AffixId.ToString()),
                Affix.NameWord.IsEmpty());
            TestFalse(*FString::Printf(TEXT("%s's name word is ONE word, got \"%s\""),
                    *Affix.AffixId.ToString(), *Affix.NameWord),
                Affix.NameWord.TrimStartAndEnd().Contains(TEXT(" ")));

            FBreakerItemInstance Widest;
            Widest.Slot = EBreakerEquipSlot::BodyArmour;   // the longest label there is
            Widest.Affixes.Add(BreakerItemNameTextRoll(Affix.AffixId, 1, Affix.Category));
            const FString Name = DisplayName(Widest);
            TestTrue(*FString::Printf(TEXT("%s composes inside the ceiling: \"%s\""),
                    *Affix.AffixId.ToString(), *Name),
                BreakerItemNaming::CountNameWords(Name) <= BreakerItemNaming::MaxNameWords);
            ++Checked;
        }
    };
    SweepPool(Library.Slice);
    SweepPool(Library.Aberrant);
    SweepPool(Library.Unwritten);
    SweepPool(Library.Downsides);
    TestTrue(TEXT("the sweep actually walked the library"), Checked >= 90);
    AddInfo(FString::Printf(TEXT("Name words checked: %d. Sample: \"%s\""), Checked, *Headline));

    // The multi-word LABELS keep their words; only the NAME takes one. A card's
    // rarity line still reads BODY ARMOUR.
    FBreakerItemInstance Plate;
    Plate.Slot = EBreakerEquipSlot::BodyArmour;
    TestEqual(TEXT("the slot label is unchanged"), FString(SlotWord(EBreakerEquipSlot::BodyArmour)),
        FString(TEXT("BODY ARMOUR")));
    TestEqual(TEXT("but the name takes one word for it"), DisplayName(Plate), FString(TEXT("PLATE")));

    // And the pin that would have caught the two screens disagreeing: the kind
    // a narrow row prints is a PART of the headline a card prints. They can
    // differ in length; they can no longer differ about the item.
    TestEqual(TEXT("the picker prints the kind alone"), BaseName(Rolled), FString(TEXT("BOOTS")));
    TestTrue(TEXT("and that kind is inside the card's headline"),
        Headline.Contains(BaseName(Rolled)));
    return true;
}

#endif   // WITH_DEV_AUTOMATION_TESTS
