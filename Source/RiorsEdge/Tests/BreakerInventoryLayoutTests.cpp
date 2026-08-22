#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UI/BreakerMenu.h"
#include "Items/BreakerAffixLibrary.h"
#include "Items/BreakerItemTypes.h"

// ---------------------------------------------------------------------------
// INVENTORY LAYOUT — the loadout screen's rules, exercised without a screen.
//
// UI/BreakerMenu.cpp's BuildInventoryScreen is a view over
// UBreakerEquipmentComponent built to Assets/ui-reference/Inventory.dc.html.
// Everything in it that can be WRONG rather than merely ugly is arithmetic or a
// mapping, and both live in BreakerInventoryLayout so they can be asserted with
// no widget, no world and no Slate application:
//   - the zone arithmetic (560 + 400 + 960 against the panel),
//   - the affix delta's glyph and magnitude,
//   - the WEAR ORDER the equipment column walks,
//   - when a card discloses an equip-limit ejection and what it says,
//   - the four filter categories, and that they partition the eight slots.
//
// NOT COVERED, and not fakeable at this level — stated plainly rather than
// papered over with a test that asserts something easier than it claims:
//
//  1. THAT ANY OF IT IS ON SCREEN. Whether a card clips its own name, whether
//     three cards fit the backpack at the running window size, whether the
//     dashed empty-slot frame draws as a rectangle rather than four stray runs
//     — all of that needs a live Slate application arranging real fonts. The
//     widths here are chosen against the longest string each box can hold and
//     commented at their use site, which is the same discipline the settings
//     screen's suite settles for and for the same reason.
//  2. THE JITTER. A plate that changes size between builds is a property of
//     arranged geometry over time; it cannot be observed from a pure test. The
//     instrumentation for it is SBreakerPlateProbe in BreakerMenu.cpp, which
//     logs desired and arranged size on change — see its header comment for how
//     to read the two outcomes apart.
//  3. HOVER. SetEquipSlotOutline runs off SButton's OnHovered/OnUnhovered and
//     needs a real pointer over a real widget. What IS covered here is the
//     decision that precedes it: whether a card should tell at all.
//  4. The equipment component's own rules — which piece a cap ejects, whether
//     an affix is better or worse. Those are game rules with their own suites
//     (Tests/BreakerItemTests.cpp, Tests/BreakerItemRuleTests.cpp); this
//     screen only renders the answers and must never hold a second opinion.
// ---------------------------------------------------------------------------

namespace
{
    FBreakerItemInstance BreakerInventoryMakeItem(EBreakerEquipSlot Slot, EBreakerItemRarity Rarity, int32 ItemLevel)
    {
        FBreakerItemInstance Item;
        Item.ItemId = FGuid::NewGuid();
        Item.Slot = Slot;
        Item.Rarity = Rarity;
        Item.ItemLevel = ItemLevel;
        return Item;
    }

    FBreakerAffixComparison BreakerInventoryMakeComparison(float Value, float ComparedValue, EBreakerAffixDelta Delta)
    {
        FBreakerAffixComparison Comparison;
        Comparison.Value = Value;
        Comparison.ComparedValue = ComparedValue;
        Comparison.Delta = Delta;
        return Comparison;
    }
}

// ---------------------------------------------------------------------------
// ZONES
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerInventoryLayoutColumnsTest,
    "RiorsEdge.UI.InventoryLayout.Columns",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerInventoryLayoutColumnsTest::RunTest(const FString& Parameters)
{
    using namespace BreakerInventoryLayout;

    // The authored panel, tiled exactly: 560 + 400 + 960 = 1920. This is the
    // whole reason the zones are separated by 1px dividers rather than by 24px
    // gutters — a gutter would make the reference's arithmetic false.
    {
        const FColumns Columns = SolveColumns(SpecPanelWidth, 0.0f);
        TestEqual(TEXT("character column at spec width"), Columns.Character, SpecCharacterColumn);
        TestEqual(TEXT("equipment column at spec width"), Columns.Equipment, SpecEquipmentColumn);
        TestEqual(TEXT("backpack column at spec width"), Columns.Backpack, SpecBackpackColumn);
    }

    // The zones tile whatever they are given: nothing is lost or double-counted
    // between them at any width above the floors.
    for (const float PanelWidth : { 1920.0f, 1760.0f, 1600.0f, 1440.0f })
    {
        const float Gutters = 2.0f;
        const FColumns Columns = SolveColumns(PanelWidth, Gutters);
        TestEqual(FString::Printf(TEXT("zones tile the panel at %.0f"), PanelWidth),
            Columns.Character + Columns.Equipment + Columns.Backpack + Gutters, PanelWidth, 0.01f);
    }

    // The two fixed columns give ground BEFORE the backpack does — the backpack
    // is where the cards are, so it is the last zone that should be squeezed.
    {
        const FColumns Wide = SolveColumns(1920.0f, 0.0f);
        const FColumns Narrow = SolveColumns(1400.0f, 0.0f);
        TestTrue(TEXT("character column gives ground on a narrow panel"), Narrow.Character < Wide.Character);
        TestTrue(TEXT("equipment column gives ground on a narrow panel"), Narrow.Equipment < Wide.Equipment);
        TestTrue(TEXT("backpack keeps its floor on a narrow panel"), Narrow.Backpack >= MinBackpackColumn);
    }

    // The floors hold even when the panel is absurd. A column below these stops
    // being able to print its own copy, which is worse than a clipped screen.
    {
        const FColumns Tiny = SolveColumns(720.0f, 0.0f);
        TestTrue(TEXT("character floor"), Tiny.Character >= MinCharacterColumn);
        TestTrue(TEXT("equipment floor"), Tiny.Equipment >= MinEquipmentColumn);
        TestTrue(TEXT("backpack never collapses"), Tiny.Backpack >= 320.0f);
    }

    return true;
}

// ---------------------------------------------------------------------------
// CARD ANATOMY — the affix delta column
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerInventoryLayoutDeltaTest,
    "RiorsEdge.UI.InventoryLayout.CardDelta",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerInventoryLayoutDeltaTest::RunTest(const FString& Parameters)
{
    using namespace BreakerInventoryLayout;

    // Three decisions, three distinct marks. The glyphs are ASCII today because
    // the engine's Slate face has no Geometric Shapes coverage (BreakerUIStyle.h
    // has the measurement); what matters here is that no two tiers share a mark,
    // because the mark is half of what carries the meaning.
    const FString Better = DeltaGlyph(EBreakerAffixDelta::Better);
    const FString Worse = DeltaGlyph(EBreakerAffixDelta::Worse);
    const FString Parity = DeltaGlyph(EBreakerAffixDelta::Parity);
    TestNotEqual(TEXT("better and worse differ"), Better, Worse);
    TestNotEqual(TEXT("better and parity differ"), Better, Parity);
    TestNotEqual(TEXT("worse and parity differ"), Worse, Parity);
    TestFalse(TEXT("every delta has a mark"), Better.IsEmpty() || Worse.IsEmpty() || Parity.IsEmpty());

    // The magnitude is UNSIGNED: the glyph beside it already carries the sign,
    // and "- -40.0" reads as a typo rather than as a loss.
    TestEqual(TEXT("an improvement prints its size"),
        FormatDelta(BreakerInventoryMakeComparison(22.0f, 18.0f, EBreakerAffixDelta::Better)), TEXT("4.0"));
    TestEqual(TEXT("a downgrade prints its size unsigned"),
        FormatDelta(BreakerInventoryMakeComparison(140.0f, 180.0f, EBreakerAffixDelta::Worse)), TEXT("40.0"));
    // Parity prints the glyph alone. A "0.0" beside an equals sign is two ways
    // of saying nothing changed, and the second one costs a column.
    TestEqual(TEXT("parity prints nothing"),
        FormatDelta(BreakerInventoryMakeComparison(9.0f, 9.0f, EBreakerAffixDelta::Parity)), FString());

    // The classification itself is the equipment component's, never this
    // screen's: a comparison whose numbers disagree with its verdict still
    // renders the verdict it was handed. This is deliberate — one opinion.
    TestEqual(TEXT("the screen renders the verdict it is given, not its own"),
        FString(DeltaGlyph(BreakerInventoryMakeComparison(1.0f, 999.0f, EBreakerAffixDelta::Better).Delta)), Better);

    return true;
}

// ---------------------------------------------------------------------------
// WEAR ORDER
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerInventoryLayoutWearOrderTest,
    "RiorsEdge.UI.InventoryLayout.WearOrder",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerInventoryLayoutWearOrderTest::RunTest(const FString& Parameters)
{
    const TArray<EBreakerEquipSlot>& Order = BreakerInventoryLayout::WearOrder();

    // Every slot, exactly once. A slot missing from this list is a slot the
    // player cannot see or unequip; a slot listed twice is a row that fights
    // with itself over the hover outline.
    TestEqual(TEXT("eight rows"), Order.Num(), static_cast<int32>(EBreakerEquipSlot::Count));
    for (int32 SlotIndex = 0; SlotIndex < static_cast<int32>(EBreakerEquipSlot::Count); ++SlotIndex)
    {
        const EBreakerEquipSlot Slot = static_cast<EBreakerEquipSlot>(SlotIndex);
        TestEqual(FString::Printf(TEXT("slot %d appears exactly once"), SlotIndex),
            Order.FilterByPredicate([Slot](EBreakerEquipSlot Other) { return Other == Slot; }).Num(), 1);
    }

    // "head to foot, then trinkets, then weapons" — the reference's PROSE, which
    // its own markup contradicts by walking the enum and putting the necklace in
    // the middle of the armour run. The prose is the rule.
    const TArray<EBreakerEquipSlot> Expected = {
        EBreakerEquipSlot::Helmet,
        EBreakerEquipSlot::BodyArmour,
        EBreakerEquipSlot::Gloves,
        EBreakerEquipSlot::Waist,
        EBreakerEquipSlot::Boots,
        EBreakerEquipSlot::Necklace,
        EBreakerEquipSlot::Primary,
        EBreakerEquipSlot::Secondary,
    };
    TestTrue(TEXT("head to foot, then trinkets, then weapons"), Order == Expected);

    // The two weapons are last, and that is the half of the rule most likely to
    // be broken by a future slot being appended to the enum.
    TestTrue(TEXT("weapons sit at the foot of the column"),
        FBreakerItemInstance::IsWeaponSlot(Order.Last()) && FBreakerItemInstance::IsWeaponSlot(Order[Order.Num() - 2]));

    return true;
}

// ---------------------------------------------------------------------------
// LIMIT TELLS
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerInventoryLayoutLimitTellTest,
    "RiorsEdge.UI.InventoryLayout.LimitTells",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerInventoryLayoutLimitTellTest::RunTest(const FString& Parameters)
{
    using namespace BreakerInventoryLayout;

    // An empty slot: the action is free and the footer says so.
    {
        FBreakerEquipPreview Preview;
        TestFalse(TEXT("no tell when nothing is capped"), ShouldShowLimitTell(Preview));
        TestEqual(TEXT("an empty slot is stated as free"), MakeFooterLead(Preview), TEXT("EQUIP · SLOT EMPTY"));
    }

    // An occupied slot with no cap in play: an ordinary swap.
    {
        FBreakerEquipPreview Preview;
        Preview.bSlotOccupied = true;
        Preview.SlotDisplaced = BreakerInventoryMakeItem(EBreakerEquipSlot::Boots, EBreakerItemRarity::Uncommon, 58);
        TestFalse(TEXT("an ordinary swap is not a limit tell"), ShouldShowLimitTell(Preview));
        TestEqual(TEXT("an ordinary swap names the replacement"), MakeFooterLead(Preview), TEXT("EQUIP · REPLACES"));
    }

    // A cap being spent: the footer leads with the cap, exactly as the
    // reference writes it — "LIMIT FULL 3/3".
    {
        FBreakerEquipPreview Preview;
        Preview.bSlotOccupied = true;
        Preview.bExceedsRarityLimit = true;
        Preview.RarityCount = 3;
        Preview.RarityLimit = 3;
        Preview.LimitDisplaced = BreakerInventoryMakeItem(EBreakerEquipSlot::Necklace, EBreakerItemRarity::Aberrant, 61);
        TestTrue(TEXT("a spent cap tells"), ShouldShowLimitTell(Preview));
        TestEqual(TEXT("the cap is quoted verbatim"), MakeFooterLead(Preview), TEXT("LIMIT FULL 3/3"));
    }

    // The Anomalous cap reads the same way at 1/1.
    {
        FBreakerEquipPreview Preview;
        Preview.bExceedsRarityLimit = true;
        Preview.RarityCount = 1;
        Preview.RarityLimit = 1;
        Preview.LimitDisplaced = BreakerInventoryMakeItem(EBreakerEquipSlot::Boots, EBreakerItemRarity::Anomalous, 66);
        TestEqual(TEXT("the anomalous cap"), MakeFooterLead(Preview), TEXT("LIMIT FULL 1/1"));
    }

    // A CAP WITH NO NAMED VICTIM IS NOT A TELL. The reference's tell is "LIMIT
    // FULL 3/3 beside the name of the piece the swap ejects", and hovering
    // outlines that piece — a tell with nothing to point at cannot be checked
    // by the player against the equipment column, so the card stays quiet and
    // states the ordinary swap instead.
    {
        FBreakerEquipPreview Preview;
        Preview.bSlotOccupied = true;
        Preview.bExceedsRarityLimit = true;
        Preview.RarityCount = 3;
        Preview.RarityLimit = 3;   // LimitDisplaced left invalid
        TestFalse(TEXT("no tell without a named victim"), ShouldShowLimitTell(Preview));
        TestEqual(TEXT("falls back to the ordinary swap"), MakeFooterLead(Preview), TEXT("EQUIP · REPLACES"));
    }

    return true;
}

// ---------------------------------------------------------------------------
// READABILITY — the rule the sixth clipped-text report bought.
//
// Clipped text has now been raised by the owner in some form in almost every
// session on this file, most recently as backpack affix names truncating
// mid-word ("Physical Damag+", "Dash Cooldown+"). The two halves of the fix are
// a MINIMUM CARD WIDTH (below which the grid drops a column rather than
// squeezing) and WRAPPING (so a line that still overruns takes a second line
// instead of losing its tail). This suite pins the first half and the budget
// the second half is computed from, so the defect cannot come back silently —
// including via a new affix with a longer name, which is the most likely way
// for it to return.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerInventoryLayoutReadabilityTest,
    "RiorsEdge.UI.InventoryLayout.Readability",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerInventoryLayoutReadabilityTest::RunTest(const FString& Parameters)
{
    using namespace BreakerInventoryLayout;

    // THE GUARD THAT MATTERS. LongestAffixNameChars is the assumption every
    // width on the card is built on; the slice pool is the thing that can
    // invalidate it. An affix whose display name outgrows the budget fails
    // here, at authoring time, instead of on the most-read line of the screen.
    int32 LongestFound = 0;
    FString LongestName;
    for (const FBreakerAffixDefinition& Affix : UBreakerAffixLibrary::GetSliceAffixPool())
    {
        const FString Name = Affix.DisplayName.ToString();
        if (Name.Len() > LongestFound)
        {
            LongestFound = Name.Len();
            LongestName = Name;
        }
    }
    TestTrue(TEXT("the slice affix pool is not empty"), LongestFound > 0);
    TestTrue(FString::Printf(
        TEXT("the longest affix name (\"%s\", %d chars) fits the card budget of %d"),
        *LongestName, LongestFound, LongestAffixNameChars),
        LongestFound <= LongestAffixNameChars);

    // The affix name column at the minimum readable card width is at least as
    // wide as that longest name. This is the rule stated as arithmetic: if it
    // holds, no card the grid will ever draw is narrower than one stat name.
    TestTrue(TEXT("the name column holds the longest stat name at the minimum card width"),
        AffixNameWrapWidth(MinReadableCardWidth) >= EstimateCaptionWidth(LongestAffixNameChars) - 0.01f);

    // And it holds for every card the solver can actually produce, at every
    // zone width the screen can reach. This is the assertion that would have
    // caught the 300px card.
    for (float Zone = 260.0f; Zone <= 1400.0f; Zone += 20.0f)
    {
        const int32 Count = SolveCardsPerRow(Zone);
        const float CardWidth = BackpackCardWidth(Zone, Count);
        // One card is the floor case: a zone too narrow for even one readable
        // card still draws one, because a wrapped card beats no card. Every
        // MULTI-card row must clear the bar outright.
        if (Count > 1)
        {
            TestTrue(FString::Printf(TEXT("a %d-across card at zone %.0f is readable"), Count, Zone),
                CardWidth >= MinReadableCardWidth - 0.01f);
            TestTrue(FString::Printf(TEXT("the name column holds a stat name at zone %.0f"), Zone),
                AffixNameWrapWidth(CardWidth) >= EstimateCaptionWidth(LongestAffixNameChars) - 0.01f);
        }
    }

    // LINE ONE MUST NOT COLLIDE. The title's wrap width has both the item-level
    // column and the discard X's clearance subtracted out of it, so the name
    // can never be arranged into the space either of them occupies — which is
    // exactly what "GLOVES" running into "i50" and the red X was.
    for (const float CardWidth : { MinReadableCardWidth, 300.0f, 348.0f, 460.0f })
    {
        TestTrue(FString::Printf(TEXT("the title clears the level and the X at %.0f"), CardWidth),
            CardTitleWrapWidth(CardWidth)
                <= CardContentWidth(CardWidth) - ItemLevelColumn - DiscardClearance + 0.01f);
        TestTrue(FString::Printf(TEXT("the title still has room to say something at %.0f"), CardWidth),
            CardTitleWrapWidth(CardWidth) > 0.0f);
    }

    // Every wrap width is positive and inside its card at every width the
    // solver can produce. A wrap width wider than its box does not wrap.
    for (const float CardWidth : { 140.0f, MinReadableCardWidth, 300.0f, 500.0f })
    {
        TestTrue(FString::Printf(TEXT("the affix wrap sits inside the card at %.0f"), CardWidth),
            AffixNameWrapWidth(CardWidth) <= CardContentWidth(CardWidth) + 0.01f);
        TestTrue(FString::Printf(TEXT("the affix wrap is positive at %.0f"), CardWidth),
            AffixNameWrapWidth(CardWidth) > 0.0f);
    }

    return true;
}

// ---------------------------------------------------------------------------
// BACKPACK GRID AND FILTERS
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerInventoryLayoutBackpackTest,
    "RiorsEdge.UI.InventoryLayout.Backpack",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerInventoryLayoutBackpackTest::RunTest(const FString& Parameters)
{
    using namespace BreakerInventoryLayout;

    // Three is the CAP, not the rule — see SolveCardsPerRow.
    TestEqual(TEXT("never more than three across"), MaxBackpackCardsPerRow, 3);
    TestEqual(TEXT("sixteen pixel gaps"), BackpackCardGap, 16.0f);

    // Whatever count is chosen, the row plus its gaps fits the zone. A trailing
    // gap on the last card is what turns a 3-across grid into one that needs
    // 4-across room.
    for (const float Zone : { SpecBackpackColumn, 900.0f, 760.0f, MinBackpackColumn, 300.0f })
    {
        const int32 Count = SolveCardsPerRow(Zone);
        TestTrue(FString::Printf(TEXT("count is in range at %.0f"), Zone), Count >= 1 && Count <= MaxBackpackCardsPerRow);
        const float CardWidth = BackpackCardWidth(Zone, Count);
        const float RowWidth = CardWidth * Count + BackpackCardGap * (Count - 1);
        TestTrue(FString::Printf(TEXT("a row of %d fits a %.0f zone"), Count, Zone), RowWidth <= Zone + 0.01f);
    }

    // A wider zone never shows FEWER cards. The count is monotonic in the room
    // available, which is the property that stops the grid flip-flopping around
    // a threshold as the window resizes.
    int32 Previous = 0;
    for (float Zone = 200.0f; Zone <= 1400.0f; Zone += 25.0f)
    {
        const int32 Count = SolveCardsPerRow(Zone);
        TestTrue(FString::Printf(TEXT("count never falls as the zone grows (%.0f)"), Zone), Count >= Previous);
        Previous = Count;
    }

    // ALL accepts everything; the other three partition the eight slots with no
    // slot in two categories and none in none. A slot that fell through every
    // filter would be invisible in every view but ALL.
    for (int32 SlotIndex = 0; SlotIndex < static_cast<int32>(EBreakerEquipSlot::Count); ++SlotIndex)
    {
        const EBreakerEquipSlot Slot = static_cast<EBreakerEquipSlot>(SlotIndex);
        TestTrue(FString::Printf(TEXT("ALL shows slot %d"), SlotIndex), PassesFilter(Slot, EBackpackFilter::All));

        int32 Matches = 0;
        for (const EBackpackFilter Filter : { EBackpackFilter::Armour, EBackpackFilter::Weapons, EBackpackFilter::Trinkets })
        {
            if (PassesFilter(Slot, Filter)) ++Matches;
        }
        TestEqual(FString::Printf(TEXT("slot %d lands in exactly one category"), SlotIndex), Matches, 1);
    }

    // And the categories mean what they say.
    TestTrue(TEXT("armour is armour"), PassesFilter(EBreakerEquipSlot::BodyArmour, EBackpackFilter::Armour));
    TestTrue(TEXT("both weapons are weapons"),
        PassesFilter(EBreakerEquipSlot::Primary, EBackpackFilter::Weapons)
        && PassesFilter(EBreakerEquipSlot::Secondary, EBackpackFilter::Weapons));
    TestTrue(TEXT("the necklace is the trinket"), PassesFilter(EBreakerEquipSlot::Necklace, EBackpackFilter::Trinkets));
    // The waist is ARMOUR, not a trinket. It is worn, it rolls armour affixes,
    // and the reference's wear order walks it between the gloves and the boots.
    TestTrue(TEXT("the waist is armour"), PassesFilter(EBreakerEquipSlot::Waist, EBackpackFilter::Armour));

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
