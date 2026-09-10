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
    TestTrue(TEXT("the headline leads with the prefix word"),
        Headline.StartsWith(Prefix->DisplayName.ToString()));
    TestTrue(TEXT("the headline closes with 'of' the suffix word"),
        Headline.EndsWith(FString(TEXT(" of ")) + Suffix->DisplayName.ToString()));

    // And the pin that would have caught the two screens disagreeing: the kind
    // a narrow row prints is a PART of the headline a card prints. They can
    // differ in length; they can no longer differ about the item.
    TestEqual(TEXT("the picker prints the kind alone"), BaseName(Rolled), FString(TEXT("BOOTS")));
    TestTrue(TEXT("and that kind is inside the card's headline"),
        Headline.Contains(BaseName(Rolled)));
    return true;
}

#endif   // WITH_DEV_AUTOMATION_TESTS
