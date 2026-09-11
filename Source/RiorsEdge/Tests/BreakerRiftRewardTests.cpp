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
// The forced drops ("at least two forced drops that are decent") are proven
// the same way: count, item level and rarity floor on every completion, and
// the floor's gate against the default-constructed drop table. O270's salt
// is proven in the payout test: the clear counter climbs once per paid
// completion and a re-clear of the same rift pays a different pair.
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

    // THE FORCED DROPS, shipped configuration. Owner: "at least two forced
    // drops that are decent". Two, and "decent" is the elite floor.
    TestEqual(TEXT("a closed rift is worth two forced items"), CompletionItemCount, 2);
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

    // Every completion item is at the band's ceiling and at or above the
    // floor its item level allows. Checked over the LAST Count items added,
    // so each clear proves its own pair.
    const auto CheckForcedDrops = [&](const TCHAR* Clear)
    {
        const TArray<FBreakerItemInstance>& Pack = Equipment->GetBackpack();
        for (int32 Back = 1; Back <= BreakerRiftReward::CompletionItemCount && Back <= Pack.Num(); ++Back)
        {
            const FBreakerItemInstance& Item = Pack[Pack.Num() - Back];
            TestTrue(*FString::Printf(TEXT("%s: forced item %d is at or above the floor"), Clear, Back),
                Item.Rarity >= ExpectedFloor);
            TestEqual(*FString::Printf(TEXT("%s: forced item %d is at the band ceiling"), Clear, Back),
                Item.ItemLevel, BandMax);
            TestTrue(*FString::Printf(TEXT("%s: forced item %d is a real item"), Clear, Back), Item.IsValid());
        }
    };

    // The LAST Count items in the backpack — the pair this clear just paid.
    const auto LastPair = [&]()
    {
        TArray<FBreakerItemInstance> Pair;
        const TArray<FBreakerItemInstance>& Pack = Equipment->GetBackpack();
        for (int32 Back = BreakerRiftReward::CompletionItemCount; Back >= 1; --Back)
        {
            if (Back <= Pack.Num()) Pair.Add(Pack[Pack.Num() - Back]);
        }
        return Pair;
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

    // A broadcast for SOMEBODY ELSE'S pawn pays this character nothing —
    // and advances no record, and forces no drop, and counts no clear.
    APawn* Stranger = NewObject<ADefaultPawn>();
    Progression->HandleRiftCompleted(Rift, Stranger);
    TestEqual(TEXT("another pawn's completion pays no XP here"),
        Progression->GetProgressionState().TotalExperience, XpBefore);
    TestEqual(TEXT("another pawn's completion advances no record"), Account->HighestClearedAreaLevel, 0);
    TestEqual(TEXT("another pawn's completion forces no drop here"), Equipment->GetBackpack().Num(), PackBefore);
    TestEqual(TEXT("another pawn's completion counts no clear here (O270)"),
        static_cast<int32>(Progression->GetProgressionState().RiftClearCount), 0);

    // The FIRST clear pays both halves and advances the account record
    // (One-AA: a first clear pays the ladder) — and forces the two drops.
    Progression->HandleRiftCompleted(Rift, Owner);
    TestEqual(TEXT("first clear pays the composed XP"),
        Progression->GetProgressionState().TotalExperience, XpBefore + ExpectedXp);
    TestEqual(TEXT("first clear pays the composed Riftglass onto the wallet"),
        Equipment->GetForgeWallet().Get(), GlassBefore + ExpectedRiftglass);
    TestEqual(TEXT("first clear advances the account record"), Account->HighestClearedAreaLevel, 3);
    TestEqual(TEXT("first clear forces exactly the two drops into the backpack"),
        Equipment->GetBackpack().Num(), PackBefore + BreakerRiftReward::CompletionItemCount);
    TestEqual(TEXT("first clear is clear number one (O270)"),
        static_cast<int32>(Progression->GetProgressionState().RiftClearCount), 1);
    CheckForcedDrops(TEXT("first clear"));
    const TArray<FBreakerItemInstance> FirstPair = LastPair();

    // A RE-CLEAR pays no purse and moves no record — going back is allowed;
    // going back is not the game. (Drops and kill XP never route through
    // this handler and are untouched.) The forced drops are NOT the purse:
    // they are the floor of what a closed rift is worth, and a re-clear
    // closes the rift too.
    Progression->HandleRiftCompleted(Rift, Owner);
    TestEqual(TEXT("a re-clear pays no purse"),
        Progression->GetProgressionState().TotalExperience, XpBefore + ExpectedXp);
    TestEqual(TEXT("a re-clear pays no Riftglass"),
        Equipment->GetForgeWallet().Get(), GlassBefore + ExpectedRiftglass);
    TestEqual(TEXT("a re-clear moves no record"), Account->HighestClearedAreaLevel, 3);
    TestEqual(TEXT("a re-clear still forces the two drops"),
        Equipment->GetBackpack().Num(), PackBefore + 2 * BreakerRiftReward::CompletionItemCount);
    TestEqual(TEXT("a re-clear is clear number two (O270)"),
        static_cast<int32>(Progression->GetProgressionState().RiftClearCount), 2);
    CheckForcedDrops(TEXT("re-clear"));

    // O270: "salted per run, never the same pair twice." The same rift at the
    // same level, cleared again by the same character, pays a DIFFERENT pair:
    // the clear count is the salt, so at least one of the two items has to
    // come out a different roll. Both pairs are still at the band ceiling and
    // at the floor (CheckForcedDrops above) — the salt moves what the item is,
    // never what it is worth.
    const TArray<FBreakerItemInstance> SecondPair = LastPair();
    TestEqual(TEXT("both clears paid a full pair"), FirstPair.Num(), SecondPair.Num());
    bool bAnyDiffers = false;
    for (int32 Index = 0; Index < FirstPair.Num() && Index < SecondPair.Num(); ++Index)
    {
        if (!SameRoll(FirstPair[Index], SecondPair[Index])) bAnyDiffers = true;
    }
    TestTrue(TEXT("the re-clear's pair is not the first clear's pair (O270: never the same pair twice)"), bAnyDiffers);

    // A DEEPER first clear pays again and advances again — the ladder is
    // climbed once per rung, not once.
    FBreakerRiftDefinition Deeper = Rift;
    Deeper.AreaLevel = 5;
    Progression->HandleRiftCompleted(Deeper, Owner);
    TestEqual(TEXT("a deeper first clear pays its own purse"),
        Progression->GetProgressionState().TotalExperience,
        XpBefore + ExpectedXp + BreakerRiftReward::XpForCompletion(5));
    TestEqual(TEXT("and advances the record to it"), Account->HighestClearedAreaLevel, 5);
    TestEqual(TEXT("and forces its own two drops"),
        Equipment->GetBackpack().Num(), PackBefore + 3 * BreakerRiftReward::CompletionItemCount);
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
    const TArray<FBreakerItemInstance>& Pack = Equipment->GetBackpack();
    for (int32 Back = 1; Back <= BreakerRiftReward::CompletionItemCount; ++Back)
    {
        TestTrue(*FString::Printf(TEXT("deeper clear: forced item %d is Exceptional or better"), Back),
            Pack[Pack.Num() - Back].Rarity >= EBreakerItemRarity::Exceptional);
        TestEqual(*FString::Printf(TEXT("deeper clear: forced item %d is i10"), Back),
            Pack[Pack.Num() - Back].ItemLevel, 10);
    }

    UBreakerAccountSave::ResetCacheForTesting();
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
