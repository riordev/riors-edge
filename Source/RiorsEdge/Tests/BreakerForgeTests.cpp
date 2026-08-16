#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Attributes/BreakerAttributeSet.h"
#include "GameFramework/Actor.h"
#include "Items/BreakerAffixLibrary.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerForgeLibrary.h"
#include "Items/BreakerItemRules.h"
#include "Items/BreakerLootLibrary.h"

// ---------------------------------------------------------------------------
// THE FORGE
// ---------------------------------------------------------------------------
// The gap under test: `BestTierForItemLevel` stops at T1 at character level 50
// and the comment at it says "T0/T-1 never come from item level — they are
// crafting/boss territory". There was no crafting and there is no boss, so the
// two tiers the whole value curve SPIKES toward (T0 at 1.4x T1, T-1 at 1.8x)
// could not be reached by any means. The single most important assertion in
// this file is that a player can now get there, and that the route is gated so
// it does not become the whole game.

namespace BreakerForgeTest
{
    // Prefixed: identical anonymous-namespace helper names in two translation
    // units have collided under this project's unity build before.
    FBreakerItemInstance BreakerForgeMakeItem(EBreakerItemRarity Rarity, int32 Tier, const TArray<FName>& AffixIds)
    {
        const TArray<FBreakerAffixDefinition>& Pool = UBreakerAffixLibrary::GetSliceAffixPool();
        FBreakerItemInstance Item;
        Item.ItemId = FGuid::NewGuid();
        Item.DefinitionId = TEXT("ForgeTest");
        Item.Slot = EBreakerEquipSlot::Necklace;
        Item.Rarity = Rarity;
        Item.ItemLevel = 50;
        for (const FName AffixId : AffixIds)
        {
            const FBreakerAffixDefinition* Definition = UBreakerAffixLibrary::FindAffix(Pool, AffixId);
            if (!Definition) continue;
            FBreakerRolledAffix Rolled;
            Rolled.AffixId = AffixId;
            Rolled.Tier = Tier;
            Rolled.Category = Definition->Category;
            Rolled.Value = UBreakerAffixLibrary::ValueForTier(*Definition, Tier);
            Item.Affixes.Add(Rolled);
        }
        return Item;
    }

    FBreakerForgeWallet BreakerForgeRichWallet()
    {
        FBreakerForgeWallet Wallet;
        Wallet.Add(1000000);
        return Wallet;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerForgeWalletTest,
    "RiorsEdge.Items.Forge.Wallet",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerForgeWalletTest::RunTest(const FString& Parameters)
{
    using namespace BreakerForgeTest;

    FBreakerForgeWallet Wallet;
    TestEqual(TEXT("A fresh wallet is empty"), Wallet.Get(), 0);
    Wallet.Add(30);

    FBreakerForgeCost Affordable;
    Affordable.Amount = 20;
    TestTrue(TEXT("An affordable cost is affordable"), Wallet.CanAfford(Affordable));
    TestTrue(TEXT("...and spends"), Wallet.Spend(Affordable));
    TestEqual(TEXT("...exactly its amount"), Wallet.Get(), 10);

    FBreakerForgeCost TooMuch;
    TooMuch.Amount = 50;
    TestFalse(TEXT("An unaffordable cost is refused"), Wallet.Spend(TooMuch));
    // The property that matters: a refused spend takes NOTHING. A partial spend
    // is how one refused craft turns into a lost-currency bug report.
    TestEqual(TEXT("A refused spend takes nothing at all"), Wallet.Get(), 10);

    // THE ONE-CURRENCY MIGRATION ARITHMETIC (owner ruling 2026-08-16). A v3
    // wallet's Slag/Flux/Sigil array folds into Riftglass at the stated 1/6/60
    // conversion, and doing it twice cannot double a balance.
    FBreakerForgeWallet Legacy;
    Legacy.Amounts = { 42, 7, 1 };
    TestTrue(TEXT("A legacy wallet reports a collapse"), Legacy.CollapseLegacyDenominations());
    TestEqual(TEXT("42 Slag + 7 Flux + 1 Sigil = 42 + 42 + 60 Riftglass"), Legacy.Get(), 144);
    TestTrue(TEXT("The legacy array is emptied"), Legacy.Amounts.IsEmpty());
    TestFalse(TEXT("A second collapse is a no-op"), Legacy.CollapseLegacyDenominations());
    TestEqual(TEXT("...and doubles nothing"), Legacy.Get(), 144);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerForgeSalvageTest,
    "RiorsEdge.Items.Forge.Salvage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerForgeSalvageTest::RunTest(const FString& Parameters)
{
    using namespace BreakerForgeTest;

    // Salvage's rarity ordering is what makes a drop worth picking up rather
    // than walking past.
    float Previous = -1.0f;
    for (int32 RarityIndex = 0; RarityIndex <= static_cast<int32>(EBreakerItemRarity::Anomalous); ++RarityIndex)
    {
        const FBreakerItemInstance Item = BreakerForgeMakeItem(static_cast<EBreakerItemRarity>(RarityIndex), 5, {TEXT("Core.Health")});
        const float Riftglass = static_cast<float>(UBreakerForgeLibrary::SalvageValue(Item).Get());
        TestTrue(TEXT("Every rarity pays some Riftglass"), Riftglass > 0.0f);
        TestTrue(TEXT("Salvage value rises with rarity"), Riftglass > Previous);
        Previous = Riftglass;
    }

    // Item level scales only the BASE part of the yield; the rarity bonus is
    // flat, so farming a low level cannot substitute for finding the rarity —
    // the shortest route from "minimal crafting" to "crafting replaces
    // looting". Proven by the high-level yield being strictly LESS than the
    // low-level yield fully scaled: if the whole row scaled, they would match.
    FBreakerItemInstance LowLevel = BreakerForgeMakeItem(EBreakerItemRarity::Aberrant, 5, {TEXT("Core.Health")});
    LowLevel.ItemLevel = 1;
    const int32 Low = UBreakerForgeLibrary::SalvageValue(LowLevel).Get();
    const int32 High = UBreakerForgeLibrary::SalvageValue(BreakerForgeMakeItem(EBreakerItemRarity::Aberrant, 5, {TEXT("Core.Health")})).Get();
    TestTrue(TEXT("Item level scales the yield"), High > Low);
    // ilvl 50 scalar is 1 + 49*0.06 = 3.94; a fully-scaled row would pay
    // Low * 3.94. The flat rarity bonus keeps the real yield well under that.
    TestTrue(TEXT("The rarity bonus does not scale with item level"),
        static_cast<float>(High) < static_cast<float>(Low) * 3.94f);
    return true;
}

// THE ONE THIS FILE EXISTS FOR.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerForgeTemperTest,
    "RiorsEdge.Items.Forge.TemperReachesTheSpike",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerForgeTemperTest::RunTest(const FString& Parameters)
{
    using namespace BreakerForgeTest;

    const FBreakerAffixDefinition* Definition =
        UBreakerAffixLibrary::FindAffix(UBreakerAffixLibrary::GetSliceAffixPool(), TEXT("Offense.WeaponDamage"));
    TestNotNull(TEXT("The fixture affix exists"), Definition);
    if (!Definition) return false;

    FBreakerItemInstance Item = BreakerForgeMakeItem(EBreakerItemRarity::Exceptional, 4, {TEXT("Offense.WeaponDamage")});
    FBreakerForgeWallet Wallet = BreakerForgeRichWallet();

    // Forge-gated exactly like the respec. This is checked BEFORE anything is
    // spent, so a refused craft cannot cost currency.
    const int32 BalanceBefore = Wallet.Get();
    TestEqual(TEXT("Tempering away from a Forge is refused"),
        static_cast<int32>(UBreakerForgeLibrary::Temper(Item, 0, Wallet, /*bIsAtForge=*/false)),
        static_cast<int32>(EBreakerForgeResult::NotAtForge));
    TestEqual(TEXT("A refused craft costs nothing"), Wallet.Get(), BalanceBefore);
    TestEqual(TEXT("...and changes nothing"), Item.Affixes[0].Tier, 4);

    // Walk the whole ladder to the bottom of the curve. The price must climb
    // monotonically rung over rung — with one currency, the LADDER is the
    // gate, so a cheaper deep rung would be an economy bug, not a tuning nit.
    int32 Steps = 0;
    int32 PreviousCost = 0;
    while (Item.Affixes[0].Tier > -1 && Steps < 20)
    {
        const int32 Before = Item.Affixes[0].Tier;
        const FBreakerForgeCost Cost = UBreakerForgeLibrary::TemperCost(Item, 0);
        const EBreakerForgeResult Result = UBreakerForgeLibrary::Temper(Item, 0, Wallet, true);
        TestEqual(*FString::Printf(TEXT("Tempering T%d succeeds"), Before),
            static_cast<int32>(Result), static_cast<int32>(EBreakerForgeResult::Success));
        TestEqual(TEXT("Tempering advances exactly one tier"), Item.Affixes[0].Tier, Before - 1);
        TestTrue(TEXT("The published cost is a real price"), Cost.Amount > 0);
        TestTrue(*FString::Printf(TEXT("The ladder climbs: into T%d costs more than the rung before"), Before - 1),
            Cost.Amount > PreviousCost);
        PreviousCost = Cost.Amount;
        ++Steps;
    }

    TestEqual(TEXT("T-1 IS REACHABLE — the top of the value curve is no longer a comment"), Item.Affixes[0].Tier, -1);
    // And it is worth what the curve says it is. RE-SITED by O29: the spike was
    // 1.8x T1 against a linear ladder whose top step was +14%; against the
    // back-loaded ladder's +36.5% top step it is 3.6x, which keeps the spike
    // about four ordinary steps above T1 rather than letting it collapse into
    // one. Read from the constant, not from a literal, so the next re-siting
    // moves one number.
    TestEqual(TEXT("A tempered T-1 is worth the authored spike"),
        Item.Affixes[0].Value,
        UBreakerAffixLibrary::ValueForTier(*Definition, 1) * UBreakerAffixLibrary::TierSpikeTopMultiplier, 0.01f);

    // Nowhere left to go, and the refusal is explicit rather than a silent
    // no-op that takes the currency.
    const int32 AtCeilingBalance = Wallet.Get();
    TestEqual(TEXT("T-1 is the ceiling"),
        static_cast<int32>(UBreakerForgeLibrary::Temper(Item, 0, Wallet, true)),
        static_cast<int32>(EBreakerForgeResult::AtTierCeiling));
    TestEqual(TEXT("A ceiling refusal costs nothing"), Wallet.Get(), AtCeilingBalance);

    // RARITY STILL CAPS CRAFTING. Otherwise crafting would erase rarity's
    // meaning in the same session this pass gave it one.
    // Built exactly one tier BELOW the rarity's cap, derived rather than
    // written down, so the fixture cannot silently start at the ceiling the day
    // the caps are re-derived again — which is what O29 just did to it.
    FBreakerItemInstance Standard = BreakerForgeMakeItem(EBreakerItemRarity::Standard,
        UBreakerAffixLibrary::TierCapForRarity(EBreakerItemRarity::Standard) + 1, {TEXT("Offense.WeaponDamage")});
    FBreakerForgeWallet Rich = BreakerForgeRichWallet();
    // T4 rather than T3 since O29 re-derived the rarity caps against 11 ladder
    // steps instead of 7. Read from the library so crafting and dropping can
    // never disagree about what a rarity is allowed to reach.
    TestEqual(TEXT("A Standard item's temper ceiling is its rarity's cap"),
        UBreakerForgeLibrary::TemperCeilingForItem(Standard),
        UBreakerAffixLibrary::TierCapForRarity(EBreakerItemRarity::Standard));
    TestEqual(TEXT("Tempering it up to that cap works"),
        static_cast<int32>(UBreakerForgeLibrary::Temper(Standard, 0, Rich, true)), static_cast<int32>(EBreakerForgeResult::Success));
    TestEqual(TEXT("No amount of currency takes a Standard past its rarity cap"),
        static_cast<int32>(UBreakerForgeLibrary::Temper(Standard, 0, Rich, true)),
        static_cast<int32>(EBreakerForgeResult::AtTierCeiling));

    // THE SPIKE IS PRICED, NOT GATED. With one currency the old "Sigil-only"
    // rule became a flat boss-scale price: a wallet one short of the T0 rung
    // is refused, and the refused craft spends nothing.
    FBreakerItemInstance Nearly = BreakerForgeMakeItem(EBreakerItemRarity::Anomalous, 1, {TEXT("Offense.WeaponDamage")});
    const FBreakerForgeCost SpikeCost = UBreakerForgeLibrary::TemperCost(Nearly, 0);
    TestTrue(TEXT("The spike outprices the whole normal ladder"),
        SpikeCost.Amount > UBreakerForgeLibrary::TemperCost(
            BreakerForgeMakeItem(EBreakerItemRarity::Anomalous, 2, {TEXT("Offense.WeaponDamage")}), 0).Amount);
    FBreakerForgeWallet OneShort;
    OneShort.Add(SpikeCost.Amount - 1);
    TestEqual(TEXT("One Riftglass short of the spike is refused"),
        static_cast<int32>(UBreakerForgeLibrary::Temper(Nearly, 0, OneShort, true)),
        static_cast<int32>(EBreakerForgeResult::Unaffordable));
    TestEqual(TEXT("The refusal spends nothing"), OneShort.Get(), SpikeCost.Amount - 1);

    // Prolific already grants a tier at aggregation time, so its printed
    // ceiling tightens by the same step. Two rules that each grant a tier must
    // compose to one T-1 ceiling, not to a T-2 the value curve has no entry for.
    FBreakerItemInstance ProlificItem = BreakerForgeMakeItem(EBreakerItemRarity::Anomalous, 1, {TEXT("Offense.WeaponDamage")});
    ProlificItem.Rule = EBreakerItemRule::Prolific;
    TestEqual(TEXT("Prolific tightens the printed temper ceiling by its uplift"),
        UBreakerForgeLibrary::TemperCeilingForItem(ProlificItem), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerForgeReforgeAttuneTest,
    "RiorsEdge.Items.Forge.ReforgeAndAttune",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerForgeReforgeAttuneTest::RunTest(const FString& Parameters)
{
    using namespace BreakerForgeTest;

    // REFORGE moves values, never tiers. Two verbs that both moved tiers would
    // make the cheaper one strictly redundant.
    FBreakerItemInstance Item = BreakerForgeMakeItem(EBreakerItemRarity::Exceptional, 2,
        {TEXT("Offense.WeaponDamage"), TEXT("Core.Health"), TEXT("Crit.Chance")});
    const TArray<FBreakerRolledAffix> Before = Item.Affixes;
    FBreakerForgeWallet Wallet = BreakerForgeRichWallet();

    TestEqual(TEXT("Reforging away from a Forge is refused"),
        static_cast<int32>(UBreakerForgeLibrary::Reforge(Item, Wallet, false, 1)), static_cast<int32>(EBreakerForgeResult::NotAtForge));
    TestEqual(TEXT("Reforging succeeds at one"),
        static_cast<int32>(UBreakerForgeLibrary::Reforge(Item, Wallet, true, 1234)), static_cast<int32>(EBreakerForgeResult::Success));

    TestEqual(TEXT("Reforge keeps the affix count"), Item.Affixes.Num(), Before.Num());
    bool bAnyValueMoved = false;
    for (int32 Index = 0; Index < Item.Affixes.Num(); ++Index)
    {
        TestEqual(TEXT("Reforge keeps every affix id"), Item.Affixes[Index].AffixId, Before[Index].AffixId);
        TestEqual(TEXT("Reforge keeps every tier"), Item.Affixes[Index].Tier, Before[Index].Tier);
        if (!FMath::IsNearlyEqual(Item.Affixes[Index].Value, Before[Index].Value, 0.0001f)) bAnyValueMoved = true;
    }
    TestTrue(TEXT("Reforge actually moves values"), bAnyValueMoved);

    // Deterministic from the seed, like every other roll in the pipeline.
    FBreakerItemInstance A = BreakerForgeMakeItem(EBreakerItemRarity::Exceptional, 2, {TEXT("Offense.WeaponDamage"), TEXT("Core.Health")});
    FBreakerItemInstance B = A;
    FBreakerForgeWallet WalletA = BreakerForgeRichWallet();
    FBreakerForgeWallet WalletB = BreakerForgeRichWallet();
    UBreakerForgeLibrary::Reforge(A, WalletA, true, 9001);
    UBreakerForgeLibrary::Reforge(B, WalletB, true, 9001);
    for (int32 Index = 0; Index < A.Affixes.Num(); ++Index)
    {
        TestEqual(TEXT("A seed reproduces a reforge exactly"), A.Affixes[Index].Value, B.Affixes[Index].Value, 0.0001f);
    }

    // ATTUNE moves ids, never tiers, and the result has to be an item the drop
    // pipeline could have produced: slot-legal, no duplicates.
    FBreakerItemInstance Attuned = BreakerForgeMakeItem(EBreakerItemRarity::Anomalous, 2,
        {TEXT("Offense.WeaponDamage"), TEXT("Core.Health"), TEXT("Crit.Chance"), TEXT("Crit.Damage")});
    const TArray<FBreakerRolledAffix> AttuneBefore = Attuned.Affixes;
    FBreakerForgeWallet AttuneWallet = BreakerForgeRichWallet();
    TestEqual(TEXT("Attune succeeds at a Forge"),
        static_cast<int32>(UBreakerForgeLibrary::Attune(Attuned, AttuneWallet, true, 555)), static_cast<int32>(EBreakerForgeResult::Success));
    TestEqual(TEXT("Attune keeps the affix count"), Attuned.Affixes.Num(), AttuneBefore.Num());

    const TArray<FBreakerAffixDefinition>& Pool = UBreakerAffixLibrary::GetSliceAffixPool();
    TSet<FName> SeenIds;
    for (int32 Index = 0; Index < Attuned.Affixes.Num(); ++Index)
    {
        TestEqual(TEXT("Attune preserves tiers position by position"), Attuned.Affixes[Index].Tier, AttuneBefore[Index].Tier);
        TestFalse(TEXT("Attune never duplicates a line"), SeenIds.Contains(Attuned.Affixes[Index].AffixId));
        SeenIds.Add(Attuned.Affixes[Index].AffixId);
        const FBreakerAffixDefinition* Definition = UBreakerAffixLibrary::FindAffix(Pool, Attuned.Affixes[Index].AffixId);
        TestNotNull(TEXT("Every attuned line is a real affix"), Definition);
        if (Definition) TestTrue(TEXT("Every attuned line is legal on the slot"), Definition->AllowsSlot(Attuned.Slot));
    }

    // A LEGENDARY KEEPS ITS SIGNATURE through an attune. Its identity is its
    // guaranteed lines plus its rule; a craft that rolled those away would turn
    // the build-defining item into a lottery ticket.
    FBreakerItemInstance Deadfall = UBreakerLootLibrary::RollLegendary(TEXT("Legendary.Deadfall"), 50, 31337);
    FBreakerForgeWallet LegendaryWallet = BreakerForgeRichWallet();
    UBreakerForgeLibrary::Attune(Deadfall, LegendaryWallet, true, 4);
    for (const FName AffixId : UBreakerItemRuleLibrary::FindLegendary(TEXT("Legendary.Deadfall")).GuaranteedAffixIds)
    {
        TestTrue(*(FString(TEXT("Attune keeps the signature line ")) + AffixId.ToString()),
            Deadfall.Affixes.ContainsByPredicate([AffixId](const FBreakerRolledAffix& R) { return R.AffixId == AffixId; }));
    }
    TestEqual(TEXT("Attune keeps the legendary's rule"),
        static_cast<int32>(Deadfall.Rule), static_cast<int32>(EBreakerItemRule::Deadfall));
    return true;
}

// The loop end to end, through the component that owns the wallet: salvage
// something, temper something, and see an equipped item's numbers actually move.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerForgeLoopTest,
    "RiorsEdge.Items.Forge.Loop",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerForgeLoopTest::RunTest(const FString& Parameters)
{
    using namespace BreakerForgeTest;

    AActor* Owner = NewObject<AActor>();
    UBreakerEquipmentComponent* Equipment = NewObject<UBreakerEquipmentComponent>(Owner);
    UBreakerAttributeSet* Attributes = NewObject<UBreakerAttributeSet>(Owner);
    Equipment->BindAttributes(Attributes);

    TestEqual(TEXT("The wallet starts empty"), Equipment->GetForgeWallet().Get(), 0);

    // Salvage is the only currency source, and it destroys the item.
    FBreakerItemInstance Junk = BreakerForgeMakeItem(EBreakerItemRarity::Uncommon, 6, {TEXT("Core.Health")});
    Equipment->AddToBackpack(Junk);
    TestEqual(TEXT("The junk is in the backpack"), Equipment->GetBackpack().Num(), 1);
    TestTrue(TEXT("Salvage succeeds"), Equipment->SalvageFromBackpack(Junk.ItemId));
    TestEqual(TEXT("Salvage destroys the item"), Equipment->GetBackpack().Num(), 0);
    TestTrue(TEXT("Salvage pays into the wallet"), Equipment->GetForgeWallet().Get() > 0);
    TestFalse(TEXT("Salvaging a missing item is refused"), Equipment->SalvageFromBackpack(FGuid::NewGuid()));

    // Bulk salvage shares its predicate with the bulk discard, so the count the
    // confirmation modal prints and the count that melts cannot drift.
    for (int32 Index = 0; Index < 5; ++Index)
    {
        Equipment->AddToBackpack(BreakerForgeMakeItem(EBreakerItemRarity::Standard, 7, {TEXT("Core.Health")}));
    }
    Equipment->AddToBackpack(BreakerForgeMakeItem(EBreakerItemRarity::Anomalous, 2, {TEXT("Core.Health")}));
    const int32 Expected = Equipment->CountBackpackBelowRarity(EBreakerItemRarity::Exceptional);
    TestEqual(TEXT("Bulk salvage melts exactly what the count promised"),
        Equipment->SalvageBackpackBelowRarity(EBreakerItemRarity::Exceptional), Expected);
    TestEqual(TEXT("Bulk salvage never touches the good item"), Equipment->GetBackpack().Num(), 1);

    // Temper an EQUIPPED item and watch the composed attribute move. This is
    // the assertion that the Forge is a gameplay system rather than a data
    // editor: a craft has to re-fold the contribution, not just repaint a card.
    FBreakerItemInstance Worn = BreakerForgeMakeItem(EBreakerItemRarity::Anomalous, 5, {TEXT("Core.Health")});
    TestTrue(TEXT("The item equips"), Equipment->EquipItem(Worn));
    const float BeforeHealth = Attributes->GetMaxHealth();

    Equipment->GrantForgeCurrency(1000000);
    TestEqual(TEXT("Tempering an equipped item away from a Forge is refused"),
        static_cast<int32>(Equipment->TemperItem(Worn.ItemId, 0, false)), static_cast<int32>(EBreakerForgeResult::NotAtForge));
    TestEqual(TEXT("Tempering an unknown id is refused"),
        static_cast<int32>(Equipment->TemperItem(FGuid::NewGuid(), 0, true)), static_cast<int32>(EBreakerForgeResult::InvalidItem));
    TestEqual(TEXT("Tempering an equipped item at a Forge succeeds"),
        static_cast<int32>(Equipment->TemperItem(Worn.ItemId, 0, true)), static_cast<int32>(EBreakerForgeResult::Success));

    TestTrue(*FString::Printf(TEXT("A temper is FELT: max health %.1f -> %.1f"), BeforeHealth, Attributes->GetMaxHealth()),
        Attributes->GetMaxHealth() > BeforeHealth);
    return true;
}

#endif
