#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Game/BreakerRiftDefinition.h"
#include "Items/BreakerItemTypes.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UI/BreakerRiftDebriefMath.h"

// ---------------------------------------------------------------------------
// THE DEBRIEF'S COMPOSITION.
//
// What is provable here is that the pane cannot disagree with the run: the
// order is deterministic, the best thing leads, nothing is invented and
// nothing is lost — a haul longer than the pane is COUNTED rather than
// silently truncated. What is not provable here is whether closing a rift
// feels like an achievement, which is beat 6 of the feel review.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerRiftDebriefTest,
    "RiorsEdge.UI.RiftDebrief.Composition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
    FBreakerItemInstance BreakerDebriefItem(EBreakerItemRarity Rarity, int32 Level, EBreakerEquipSlot Slot)
    {
        FBreakerItemInstance Item;
        // A REAL ID, because that is what IsValid asks for and the composer
        // refuses anything else. An item without one is what an unfilled slot
        // looks like, which is the case the invalid-items block below covers.
        Item.ItemId = FGuid::NewGuid();
        Item.Slot = Slot;
        Item.Rarity = Rarity;
        Item.ItemLevel = Level;
        return Item;
    }
}

bool FBreakerRiftDebriefTest::RunTest(const FString& Parameters)
{
    using namespace BreakerRiftDebrief;

    FBreakerRiftDefinition Rift;
    Rift.Tier = EBreakerRiftTier::Campaign;
    Rift.AreaName = FText::FromString(TEXT("Substation Undercroft"));
    // Most blocks compose with no offer; the offer has its own block below.
    const TArray<FBreakerItemInstance> NoOffer;

    // ---- AN EMPTY RUN IS A STATE, NOT A GAP ------------------------------
    {
        const FModel Empty = Compose(Rift, {}, NoOffer, 0, 0);
        TestTrue(TEXT("a run that took nothing says so"), Empty.IsEmptyHanded());
        TestEqual(TEXT("and counts no overflow"), Empty.MoreCount, 0);
        TestEqual(TEXT("the area still names itself"), Empty.AreaName.ToString(), Rift.AreaName.ToString());
        TestFalse(TEXT("and the headline is never empty"), Empty.Headline.IsEmpty());
        TestEqual(TEXT("a campaign rift says which kind it was"), Empty.TierKicker, FString(TEXT("CAMPAIGN RIFT")));
    }
    {
        FBreakerRiftDefinition Endgame = Rift;
        Endgame.Tier = EBreakerRiftTier::Endgame;
        TestEqual(TEXT("and an endgame rift says the other"),
            Compose(Endgame, {}, NoOffer, 0, 0).TierKicker, FString(TEXT("ENDGAME RIFT")));
    }

    // ---- NEITHER TOTAL CAN GO NEGATIVE -----------------------------------
    // A wallet or an XP total that fell across a run is a defect somewhere
    // else; printing it here would put that defect's face on the reward beat.
    {
        const FModel Backwards = Compose(Rift, {}, NoOffer, -400, -900);
        TestEqual(TEXT("a fallen wallet reads as nothing gained"), Backwards.Riftglass, 0);
        TestEqual(TEXT("and so does fallen experience"), Backwards.Experience, 0);
    }
    {
        const FModel Paid = Compose(Rift, {}, NoOffer, 240, 1310);
        TestEqual(TEXT("what was gained is carried through"), Paid.Riftglass, 240);
        TestEqual(TEXT("both of them"), Paid.Experience, 1310);
    }

    // ---- BEST FIRST ------------------------------------------------------
    {
        TArray<FBreakerItemInstance> Taken;
        Taken.Add(BreakerDebriefItem(EBreakerItemRarity::Standard, 12, EBreakerEquipSlot::Boots));
        Taken.Add(BreakerDebriefItem(EBreakerItemRarity::Aberrant, 9, EBreakerEquipSlot::Helmet));
        Taken.Add(BreakerDebriefItem(EBreakerItemRarity::Uncommon, 40, EBreakerEquipSlot::Gloves));
        const FModel Model = Compose(Rift, Taken, NoOffer, 0, 0);
        if (!TestEqual(TEXT("every taken item reaches the pane"), Model.Highlights.Num(), 3)) return false;
        TestFalse(TEXT("a run with a haul is not empty-handed"), Model.IsEmptyHanded());
        // RARITY OUTRANKS LEVEL, and the i40 Uncommon above proves it is not
        // merely sorting by the biggest number.
        TestEqual(TEXT("the rarest leads"), static_cast<int32>(Model.Highlights[0].Rarity),
            static_cast<int32>(EBreakerItemRarity::Aberrant));
        TestEqual(TEXT("then the next"), static_cast<int32>(Model.Highlights[1].Rarity),
            static_cast<int32>(EBreakerItemRarity::Uncommon));
        TestEqual(TEXT("and the plainest last"), static_cast<int32>(Model.Highlights[2].Rarity),
            static_cast<int32>(EBreakerItemRarity::Standard));
        for (const FLine& Line : Model.Highlights)
        {
            TestFalse(TEXT("no line reaches the pane unnamed"), Line.Name.IsEmpty());
        }
    }

    // WITHIN ONE RARITY, THE HIGHER LEVEL LEADS.
    {
        TArray<FBreakerItemInstance> Taken;
        Taken.Add(BreakerDebriefItem(EBreakerItemRarity::Uncommon, 5, EBreakerEquipSlot::Boots));
        Taken.Add(BreakerDebriefItem(EBreakerItemRarity::Uncommon, 50, EBreakerEquipSlot::Helmet));
        const FModel Model = Compose(Rift, Taken, NoOffer, 0, 0);
        if (!TestEqual(TEXT("both lines compose"), Model.Highlights.Num(), 2)) return false;
        TestEqual(TEXT("the higher item level leads within a rarity"), Model.Highlights[0].ItemLevel, 50);
    }

    // ---- NOTHING IS LOST, IT IS COUNTED ----------------------------------
    // A haul longer than the pane must say how much it is not showing. A
    // silent truncation is the one failure a reward screen must not have: the
    // player counts their pack afterwards and the two disagree.
    {
        TArray<FBreakerItemInstance> Taken;
        for (int32 Index = 0; Index < MaxHighlights + 7; ++Index)
        {
            Taken.Add(BreakerDebriefItem(EBreakerItemRarity::Standard, 1 + Index, EBreakerEquipSlot::Boots));
        }
        const FModel Model = Compose(Rift, Taken, NoOffer, 0, 0);
        TestEqual(TEXT("the pane shows exactly what it can hold"), Model.Highlights.Num(), MaxHighlights);
        TestEqual(TEXT("and counts the rest"), Model.MoreCount, 7);
        TestEqual(TEXT("shown plus counted is the whole haul"),
            Model.Highlights.Num() + Model.MoreCount, Taken.Num());
        TestFalse(TEXT("an overflowing haul is never empty-handed"), Model.IsEmptyHanded());
    }

    // ---- INVALID ITEMS ARE NOT LINES -------------------------------------
    // A default-constructed instance is what an unfilled slot looks like, and
    // a reward beat that listed one would be inventing a drop.
    {
        TArray<FBreakerItemInstance> Taken;
        Taken.Add(FBreakerItemInstance());
        Taken.Add(BreakerDebriefItem(EBreakerItemRarity::Exceptional, 11, EBreakerEquipSlot::Waist));
        Taken.Add(FBreakerItemInstance());
        const FModel Model = Compose(Rift, Taken, NoOffer, 0, 0);
        TestEqual(TEXT("only real items become lines"), Model.Highlights.Num(), 1);
        TestEqual(TEXT("and the invalid ones are not counted as overflow either"), Model.MoreCount, 0);
    }

    // ---- THE SAME HAUL COMPOSES THE SAME PANE ----------------------------
    // Deterministic ordering is what lets a capture be compared with the one
    // before it, and what stops two runs of the same haul disagreeing.
    {
        TArray<FBreakerItemInstance> Taken;
        Taken.Add(BreakerDebriefItem(EBreakerItemRarity::Uncommon, 20, EBreakerEquipSlot::Boots));
        Taken.Add(BreakerDebriefItem(EBreakerItemRarity::Uncommon, 20, EBreakerEquipSlot::Helmet));
        Taken.Add(BreakerDebriefItem(EBreakerItemRarity::Uncommon, 20, EBreakerEquipSlot::Gloves));
        const FModel First = Compose(Rift, Taken, NoOffer, 5, 5);
        TArray<FBreakerItemInstance> Shuffled;
        Shuffled.Add(Taken[2]);
        Shuffled.Add(Taken[0]);
        Shuffled.Add(Taken[1]);
        const FModel Second = Compose(Rift, Shuffled, NoOffer, 5, 5);
        if (!TestEqual(TEXT("both compose the same number of lines"),
            First.Highlights.Num(), Second.Highlights.Num())) return false;
        for (int32 Index = 0; Index < First.Highlights.Num(); ++Index)
        {
            TestEqual(TEXT("the order does not depend on the order things were picked up"),
                First.Highlights[Index].Name, Second.Highlights[Index].Name);
        }
    }

    // ---- THE OFFER (O270): ROLLED ORDER, NEVER SORTED ----------------------
    // The index into Offer is the claim key the progression component takes,
    // so the rows must come out exactly as they went in. The three go in
    // descending-then-ascending rarity (Aberrant, Standard, Exceptional): a
    // best-first sort would put Exceptional second and Standard last, and a
    // worst-first one would move Aberrant — either is detectable.
    {
        TArray<FBreakerItemInstance> Offer;
        Offer.Add(BreakerDebriefItem(EBreakerItemRarity::Aberrant, 30, EBreakerEquipSlot::Helmet));
        Offer.Add(BreakerDebriefItem(EBreakerItemRarity::Standard, 30, EBreakerEquipSlot::Boots));
        Offer.Add(BreakerDebriefItem(EBreakerItemRarity::Exceptional, 30, EBreakerEquipSlot::Gloves));
        FModel Model = Compose(Rift, {}, Offer, 0, 0);
        if (!TestEqual(TEXT("every offered item becomes a square"), Model.Offer.Num(), 3)) return false;
        TestEqual(TEXT("the first square is the first rolled"), static_cast<int32>(Model.Offer[0].Line.Rarity),
            static_cast<int32>(EBreakerItemRarity::Aberrant));
        TestEqual(TEXT("the second square is the second rolled, not the next best"),
            static_cast<int32>(Model.Offer[1].Line.Rarity), static_cast<int32>(EBreakerItemRarity::Standard));
        TestEqual(TEXT("the third square is the third rolled"), static_cast<int32>(Model.Offer[2].Line.Rarity),
            static_cast<int32>(EBreakerItemRarity::Exceptional));
        for (int32 Index = 0; Index < 3; ++Index)
        {
            TestEqual(TEXT("the square carries the instance it was rolled from"),
                Model.Offer[Index].Item.ItemId, Offer[Index].ItemId);
            TestFalse(TEXT("no square reaches the pane unnamed"), Model.Offer[Index].Line.Name.IsEmpty());
        }
        // An offer does not touch the haul: the squares are not in the pack.
        TestTrue(TEXT("the offer is not the haul"), Model.IsEmptyHanded());

        // THE GATE. Fresh from Compose nothing is chosen and the verb refuses;
        // a valid choice opens it; an index off the end of the offer does not.
        TestEqual(TEXT("nothing is chosen at composition"), Model.ChosenIndex, INDEX_NONE);
        TestFalse(TEXT("an unchosen offer cannot continue"), Model.CanContinue());
        Model.ChosenIndex = 1;
        TestTrue(TEXT("a chosen square can continue"), Model.CanContinue());
        Model.ChosenIndex = 3;
        TestFalse(TEXT("an index past the offer is not a choice"), Model.CanContinue());
    }
    {
        // With nothing offered there is nothing to gate: CONTINUE stands.
        const FModel Model = Compose(Rift, {}, NoOffer, 0, 0);
        TestEqual(TEXT("no offer composes no squares"), Model.Offer.Num(), 0);
        TestTrue(TEXT("no offer can always continue"), Model.CanContinue());
    }

    // ---- THE CLAIM PRECEDES THE TRAVEL -----------------------------------
    // A source scan, on BreakerBootFlowTests' pattern, because the order of
    // two calls inside a Slate click handler is a statement about the screen
    // TU rather than about any object this suite can hold. The claim must
    // sit inside CloseRiftDebrief and before ReturnFromRift: after it, the
    // run ledger is closed and the pawn is travelling, and the chosen item
    // would land in nothing.
    {
        const FString ScreenPath = FPaths::Combine(FPaths::ProjectDir(),
            TEXT("Source"), TEXT("RiorsEdge"), TEXT("UI"), TEXT("BreakerRiftDebriefScreen.cpp"));
        FString Screen;
        if (FFileHelper::LoadFileToString(Screen, *ScreenPath))
        {
            const int32 CloseBegin = Screen.Find(TEXT("FReply SBreakerMenu::CloseRiftDebrief()"));
            if (TestTrue(TEXT("the screen TU defines CloseRiftDebrief"), CloseBegin != INDEX_NONE))
            {
                // The function's own body only: up to the next member
                // definition, or the end of the file when it is the last.
                const int32 CloseEnd = Screen.Find(TEXT("SBreakerMenu::"), ESearchCase::CaseSensitive,
                    ESearchDir::FromStart, CloseBegin + 40);
                const FString CloseBody = Screen.Mid(CloseBegin, (CloseEnd == INDEX_NONE ? Screen.Len() : CloseEnd) - CloseBegin);
                const int32 ClaimAt = CloseBody.Find(TEXT("ClaimRiftCompletionOffer("));
                const int32 TravelAt = CloseBody.Find(TEXT("ReturnFromRift("));
                TestTrue(TEXT("CloseRiftDebrief claims the chosen offer"), ClaimAt != INDEX_NONE);
                TestTrue(TEXT("CloseRiftDebrief travels out through ReturnFromRift"), TravelAt != INDEX_NONE);
                TestTrue(TEXT("the claim comes before the travel"),
                    ClaimAt != INDEX_NONE && TravelAt != INDEX_NONE && ClaimAt < TravelAt);
            }
        }
        else
        {
            AddInfo(TEXT("Screen source not present (packaged build?); the order scan was skipped."));
        }
    }

    // ---- THE LADDER ITSELF -----------------------------------------------
    // Rank is written out rather than read off the enumerator, so it needs its
    // own assertion: an appended rarity must not silently outrank Unwritten.
    TestTrue(TEXT("the ladder climbs"),
        Rank(EBreakerItemRarity::Standard) < Rank(EBreakerItemRarity::Uncommon)
        && Rank(EBreakerItemRarity::Uncommon) < Rank(EBreakerItemRarity::Exceptional)
        && Rank(EBreakerItemRarity::Exceptional) < Rank(EBreakerItemRarity::Aberrant)
        && Rank(EBreakerItemRarity::Aberrant) < Rank(EBreakerItemRarity::Unwritten));
    return true;
}

#endif   // WITH_DEV_AUTOMATION_TESTS
