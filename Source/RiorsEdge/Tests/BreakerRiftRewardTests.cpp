#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GameFramework/DefaultPawn.h"
#include "Combat/BreakerEnemy.h"
#include "Game/BreakerRiftDefinition.h"
#include "Items/BreakerDropTable.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerRiftRewardMath.h"
#include "Save/BreakerAccountSave.h"

// ---------------------------------------------------------------------------
// O168's third commit, proven: the rift completion payout. The pure math is
// pinned where it lives, and the handler is exercised by direct call — the
// same seam the BeginPlay bind routes into, minus the world the bind needs.
// The offer (O270: a closed rift offers three, the player chooses one on the
// closing card, the other two are gone) is proven the same way: count, item
// level and rarity floor on every completion, the floor's gate against the
// default-constructed drop table, and the claim — a completion moves the
// pack not at all, a claim moves it by exactly the chosen item and closes the
// offer, and a bad index or a second claim is a refusal that moves nothing.
// O270's salt is proven in the payout test: the clear counter climbs once per
// paid completion and a re-clear of the same rift offers a different trio.
//
// WHAT THIS DOES NOT COVER: the bind itself (one authority-gated
// AddWeakLambda in BeginPlay — a live rift run is the check) and the
// first-clear grants, which wait on an archetype existing on the rift
// definition (O168 records why).
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerRiftRewardMathTest,
    "RiorsEdge.Progression.RiftReward.Math",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRiftRewardMathTest::RunTest(const FString& Parameters)
{
    using namespace BreakerRiftReward;

    // Area level 1 pays exactly the authored bases — the same anchoring rule
    // the chassis holds (area level 1 is bit-identical to the seed).
    TestEqual(TEXT("AL1 Riftglass is the base"), RiftglassForCompletion(1), CompletionRiftglassBase);
    TestEqual(TEXT("AL1 XP is the base"), XpForCompletion(1), CompletionXpBase);

    // The payout rides the chassis's own growth constant — one number, one
    // place. Asserted against the authored default, not a restated 0.09.
    const float Growth = FBreakerMonsterChassisParams{}.HealthGrowthPerLevel;
    TestEqual(TEXT("the completion scale is the chassis's growth"),
        CompletionScale(2), 1.0f + Growth, 0.0001f);

    // Monotone and clamped: a deeper rift never pays less, and garbage area
    // levels clamp instead of exploding.
    int32 Previous = RiftglassForCompletion(1);
    for (int32 Level = 2; Level <= 100; ++Level)
    {
        const int32 Pay = RiftglassForCompletion(Level);
        TestTrue(*FString::Printf(TEXT("AL%d never pays less than AL%d"), Level, Level - 1), Pay >= Previous);
        Previous = Pay;
    }
    TestEqual(TEXT("an unset area level pays the clamp floor"),
        RiftglassForCompletion(0), RiftglassForCompletion(1));
    TestEqual(TEXT("a runaway area level pays the clamp ceiling"),
        RiftglassForCompletion(9999), RiftglassForCompletion(100));

    // THE OFFER, shipped configuration. O270: a closed rift offers three
    // items; the player chooses one. Three, and "decent" is the elite floor.
    TestEqual(TEXT("a closed rift offers three items (O270)"), CompletionOfferCount, 3);
    TestTrue(TEXT("the rarity floor is the elite floor (Exceptional)"),
        CompletionRarityFloor == EBreakerItemRarity::Exceptional);

    // The floor composes with the drop table's gates: below the Exceptional
    // unlock it falls to Uncommon (never Standard), at the unlock it holds.
    // Asserted against the default-constructed table, and the gate asserted
    // as the real authored number so a retune of the table is felt here.
    const FBreakerDropTableParams Table;
    TestEqual(TEXT("the Exceptional unlock is item level 8 (O2)"), Table.ExceptionalMinimumItemLevel, 8);
    TestTrue(TEXT("i5 cannot hand out Exceptional: the floor falls to Uncommon"),
        CompletionRarity(5, Table) == EBreakerItemRarity::Uncommon);
    TestTrue(TEXT("i7 is still below the unlock"),
        CompletionRarity(7, Table) == EBreakerItemRarity::Uncommon);
    TestTrue(TEXT("i8 is the unlock: the floor holds at Exceptional"),
        CompletionRarity(8, Table) == EBreakerItemRarity::Exceptional);
    TestTrue(TEXT("the floor never exceeds itself at any item level"),
        CompletionRarity(UBreakerAffixLibrary::MaxItemLevel, Table) == EBreakerItemRarity::Exceptional);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerRiftRewardPayoutTest,
    "RiorsEdge.Progression.RiftReward.Payout",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRiftRewardPayoutTest::RunTest(const FString& Parameters)
{
    // The refusals under test are loud by design (a refused claim and an
    // overwritten offer both warn), and this test drives each at least once.
    AddExpectedError(TEXT("claim refused"), EAutomationExpectedErrorFlags::Contains, 0);
    AddExpectedError(TEXT("still pending from an earlier completion"), EAutomationExpectedErrorFlags::Contains, 0);

    // A transient, never-persisting account, injected so the suite exercises
    // the record and the first-clear rule WITHOUT touching the machine's
    // real account slot.
    UBreakerAccountSave* Account = NewObject<UBreakerAccountSave>();
    Account->bNeverPersist = true;
    UBreakerAccountSave::InjectForTesting(Account);

    // A pawn owner, because the handler compares the event's pawn against its
    // owner — the guard under test.
    APawn* Owner = NewObject<ADefaultPawn>();
    UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>(Owner);
    UBreakerEquipmentComponent* Equipment = NewObject<UBreakerEquipmentComponent>(Owner);

    FBreakerRiftDefinition Rift;
    Rift.AreaName = FText::FromString(TEXT("Test Substation"));
    Rift.AreaLevel = 3;

    const int32 ExpectedXp = BreakerRiftReward::XpForCompletion(3);
    const int32 ExpectedRiftglass = BreakerRiftReward::RiftglassForCompletion(3);
    const int32 XpBefore = Progression->GetProgressionState().TotalExperience;
    const int32 GlassBefore = Equipment->GetForgeWallet().Get();
    const int32 PackBefore = Equipment->GetBackpack().Num();

    // The forced drops roll at the TOP of the band the briefing prints, from
    // the same call and the same authored elite bonus the handler reads —
    // the test grants itself nothing the game does not.
    int32 BandMin = 0;
    int32 BandMax = 0;
    UBreakerRiftLibrary::GetDropItemLevelRange(Rift.EffectiveAreaLevel(),
        GetDefault<ABreakerEnemy>()->GetEliteDropItemLevelBonus(), BandMin, BandMax);
    const EBreakerItemRarity ExpectedFloor = BreakerRiftReward::CompletionRarity(BandMax, FBreakerDropTableParams{});

    // Every offered item is at the band's ceiling and at or above the floor
    // its item level allows, and the offer is exactly the ruled count.
    // Checked over the pending offer, so each clear proves its own trio.
    const auto CheckOffer = [&](const TCHAR* Clear)
    {
        const TArray<FBreakerItemInstance>& Offer = Progression->GetRiftCompletionOffer();
        TestEqual(*FString::Printf(TEXT("%s: the offer is exactly the ruled count (O270)"), Clear),
            Offer.Num(), BreakerRiftReward::CompletionOfferCount);
        for (int32 Index = 0; Index < Offer.Num(); ++Index)
        {
            const FBreakerItemInstance& Item = Offer[Index];
            TestTrue(*FString::Printf(TEXT("%s: offered item %d is at or above the floor"), Clear, Index),
                Item.Rarity >= ExpectedFloor);
            TestEqual(*FString::Printf(TEXT("%s: offered item %d is at the band ceiling"), Clear, Index),
                Item.ItemLevel, BandMax);
            TestTrue(*FString::Printf(TEXT("%s: offered item %d is a real item"), Clear, Index), Item.IsValid());
        }
    };

    // Whether two rolled items are the same roll. ItemId is a fresh GUID on
    // every RollItem regardless of seed, so it proves nothing here; what a
    // different seed moves is the slot draw, the archetype draw and the affix
    // rows, and those are what this compares.
    const auto SameRoll = [](const FBreakerItemInstance& A, const FBreakerItemInstance& B)
    {
        if (A.Slot != B.Slot || A.Rarity != B.Rarity || A.ItemLevel != B.ItemLevel) return false;
        if (A.WeaponArchetype != B.WeaponArchetype || A.ArmourArchetype != B.ArmourArchetype) return false;
        if (A.Affixes.Num() != B.Affixes.Num()) return false;
        for (int32 Index = 0; Index < A.Affixes.Num(); ++Index)
        {
            if (A.Affixes[Index].AffixId != B.Affixes[Index].AffixId) return false;
            if (A.Affixes[Index].Tier != B.Affixes[Index].Tier) return false;
            if (!FMath::IsNearlyEqual(A.Affixes[Index].Value, B.Affixes[Index].Value)) return false;
        }
        return true;
    };

    // Nothing is offered before anything is cleared, and a claim on nothing
    // is a refusal that moves nothing.
    TestEqual(TEXT("nothing is offered before a clear"), Progression->GetRiftCompletionOffer().Num(), 0);
    TestFalse(TEXT("a claim with no offer pending is refused"), Progression->ClaimRiftCompletionOffer(0));
    TestEqual(TEXT("and the refused claim moves the pack not at all"), Equipment->GetBackpack().Num(), PackBefore);

    // A broadcast for SOMEBODY ELSE'S pawn pays this character nothing —
    // and advances no record, and offers nothing, and counts no clear.
    APawn* Stranger = NewObject<ADefaultPawn>();
    Progression->HandleRiftCompleted(Rift, Stranger);
    TestEqual(TEXT("another pawn's completion pays no XP here"),
        Progression->GetProgressionState().TotalExperience, XpBefore);
    TestEqual(TEXT("another pawn's completion advances no record"), Account->HighestClearedAreaLevel, 0);
    TestEqual(TEXT("another pawn's completion moves no pack here"), Equipment->GetBackpack().Num(), PackBefore);
    TestEqual(TEXT("another pawn's completion leaves the offer empty (O270)"),
        Progression->GetRiftCompletionOffer().Num(), 0);
    TestEqual(TEXT("another pawn's completion counts no clear here (O270)"),
        static_cast<int32>(Progression->GetProgressionState().RiftClearCount), 0);

    // The FIRST clear pays both halves and advances the account record
    // (One-AA: a first clear pays the ladder) — and OFFERS the three. The
    // pack does not move at completion: the pack moves at the claim.
    Progression->HandleRiftCompleted(Rift, Owner);
    TestEqual(TEXT("first clear pays the composed XP"),
        Progression->GetProgressionState().TotalExperience, XpBefore + ExpectedXp);
    TestEqual(TEXT("first clear pays the composed Riftglass onto the wallet"),
        Equipment->GetForgeWallet().Get(), GlassBefore + ExpectedRiftglass);
    TestEqual(TEXT("first clear advances the account record"), Account->HighestClearedAreaLevel, 3);
    TestEqual(TEXT("first clear moves the pack not at all — the offer is not yet a grant (O270)"),
        Equipment->GetBackpack().Num(), PackBefore);
    TestEqual(TEXT("first clear is clear number one (O270)"),
        static_cast<int32>(Progression->GetProgressionState().RiftClearCount), 1);
    CheckOffer(TEXT("first clear"));
    // A copy: the claim below closes the offer, and the salt assertion
    // further down compares this trio against the re-clear's.
    const TArray<FBreakerItemInstance> FirstOffer = Progression->GetRiftCompletionOffer();

    // THE CHOICE. An index past the offer or below it is refused and moves
    // nothing — the offer stays pending for a real answer.
    TestFalse(TEXT("a claim past the offer is refused"),
        Progression->ClaimRiftCompletionOffer(BreakerRiftReward::CompletionOfferCount));
    TestFalse(TEXT("a negative claim is refused"), Progression->ClaimRiftCompletionOffer(-1));
    TestEqual(TEXT("refused claims move the pack not at all"), Equipment->GetBackpack().Num(), PackBefore);
    TestEqual(TEXT("refused claims leave the offer pending"),
        Progression->GetRiftCompletionOffer().Num(), BreakerRiftReward::CompletionOfferCount);
    // The chosen one goes into the pack; the other two are gone.
    TestTrue(TEXT("claiming the second offered item succeeds"), Progression->ClaimRiftCompletionOffer(1));
    TestEqual(TEXT("the claim moves the pack by exactly one"), Equipment->GetBackpack().Num(), PackBefore + 1);
    if (Equipment->GetBackpack().Num() == PackBefore + 1 && FirstOffer.IsValidIndex(1))
    {
        TestTrue(TEXT("the item that landed is the one that was chosen (Offer[1])"),
            SameRoll(Equipment->GetBackpack().Last(), FirstOffer[1]));
    }
    TestEqual(TEXT("the claim closes the offer: the other two are gone (O270)"),
        Progression->GetRiftCompletionOffer().Num(), 0);
    TestFalse(TEXT("a second claim of the same offer is refused"), Progression->ClaimRiftCompletionOffer(0));
    TestEqual(TEXT("and moves the pack not at all"), Equipment->GetBackpack().Num(), PackBefore + 1);

    // A RE-CLEAR pays no purse and moves no record — going back is allowed;
    // going back is not the game. (Drops and kill XP never route through
    // this handler and are untouched.) The offer is NOT the purse: it is the
    // floor of what a closed rift is worth, and a re-clear closes the rift
    // too.
    Progression->HandleRiftCompleted(Rift, Owner);
    TestEqual(TEXT("a re-clear pays no purse"),
        Progression->GetProgressionState().TotalExperience, XpBefore + ExpectedXp);
    TestEqual(TEXT("a re-clear pays no Riftglass"),
        Equipment->GetForgeWallet().Get(), GlassBefore + ExpectedRiftglass);
    TestEqual(TEXT("a re-clear moves no record"), Account->HighestClearedAreaLevel, 3);
    TestEqual(TEXT("a re-clear moves the pack not at all — it offers"),
        Equipment->GetBackpack().Num(), PackBefore + 1);
    TestEqual(TEXT("a re-clear is clear number two (O270)"),
        static_cast<int32>(Progression->GetProgressionState().RiftClearCount), 2);
    CheckOffer(TEXT("re-clear"));

    // O270: "salted per run." The same rift at the same level, cleared again
    // by the same character, OFFERS a different trio: the clear count is the
    // salt, so at least one of the three has to come out a different roll.
    // Both offers are still at the band ceiling and at the floor (CheckOffer
    // above) — the salt moves what the item is, never what it is worth.
    const TArray<FBreakerItemInstance> SecondOffer = Progression->GetRiftCompletionOffer();
    TestEqual(TEXT("both clears offered a full trio"), FirstOffer.Num(), SecondOffer.Num());
    bool bAnyDiffers = false;
    for (int32 Index = 0; Index < FirstOffer.Num() && Index < SecondOffer.Num(); ++Index)
    {
        if (!SameRoll(FirstOffer[Index], SecondOffer[Index])) bAnyDiffers = true;
    }
    TestTrue(TEXT("the re-clear's offer is not the first clear's offer (O270: salted per run)"), bAnyDiffers);

    // A DEEPER first clear pays again and advances again — the ladder is
    // climbed once per rung, not once. The re-clear's offer was never
    // claimed, so this completion overwrites it (one offer at a time) and
    // the pack still does not move.
    FBreakerRiftDefinition Deeper = Rift;
    Deeper.AreaLevel = 5;
    Progression->HandleRiftCompleted(Deeper, Owner);
    TestEqual(TEXT("a deeper first clear pays its own purse"),
        Progression->GetProgressionState().TotalExperience,
        XpBefore + ExpectedXp + BreakerRiftReward::XpForCompletion(5));
    TestEqual(TEXT("and advances the record to it"), Account->HighestClearedAreaLevel, 5);
    TestEqual(TEXT("and moves the pack not at all — an unclaimed offer is overwritten, never banked"),
        Equipment->GetBackpack().Num(), PackBefore + 1);
    TestEqual(TEXT("and is clear number three: the salt counts every rift, not per rift (O270)"),
        static_cast<int32>(Progression->GetProgressionState().RiftClearCount), 3);
    // The owner's rift was i5–i10: area level 5, band ceiling 10, and 10 is
    // past the Exceptional unlock — his "decent" is Exceptional there.
    int32 DeeperMin = 0;
    int32 DeeperMax = 0;
    UBreakerRiftLibrary::GetDropItemLevelRange(Deeper.EffectiveAreaLevel(),
        GetDefault<ABreakerEnemy>()->GetEliteDropItemLevelBonus(), DeeperMin, DeeperMax);
    TestEqual(TEXT("the owner's rift band tops at i10"), DeeperMax, 10);
    TestTrue(TEXT("the owner's rift pays Exceptional at its ceiling"),
        BreakerRiftReward::CompletionRarity(DeeperMax, FBreakerDropTableParams{}) == EBreakerItemRarity::Exceptional);
    const TArray<FBreakerItemInstance>& DeeperOffer = Progression->GetRiftCompletionOffer();
    TestEqual(TEXT("deeper clear: the offer is exactly the ruled count, the stale one overwritten (O270)"),
        DeeperOffer.Num(), BreakerRiftReward::CompletionOfferCount);
    for (int32 Index = 0; Index < DeeperOffer.Num(); ++Index)
    {
        TestTrue(*FString::Printf(TEXT("deeper clear: offered item %d is Exceptional or better"), Index),
            DeeperOffer[Index].Rarity >= EBreakerItemRarity::Exceptional);
        TestEqual(*FString::Printf(TEXT("deeper clear: offered item %d is i10"), Index),
            DeeperOffer[Index].ItemLevel, 10);
    }
    // And the deeper offer claims the same way: the first, this time.
    TestTrue(TEXT("deeper clear: claiming the first offered item succeeds"), Progression->ClaimRiftCompletionOffer(0));
    TestEqual(TEXT("deeper clear: the pack moved by exactly one"), Equipment->GetBackpack().Num(), PackBefore + 2);
    TestEqual(TEXT("deeper clear: the claim closed the offer"), Progression->GetRiftCompletionOffer().Num(), 0);

    UBreakerAccountSave::ResetCacheForTesting();
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
