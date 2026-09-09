#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Items/BreakerAffixLibrary.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerForgeLibrary.h"
#include "Items/BreakerLootLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerProlificSignedRuntimeTest,
    "RiorsEdge.Items.Rules.ProlificSignedRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerProlificSignedRuntimeTest::RunTest(const FString& Parameters)
{
    FBreakerItemInstance Item;
    FBreakerForgeWallet Salvage;
    int32 BillIndex = INDEX_NONE;
    // Discover a genuine endgame drop, rather than assigning a rewrite or affix.
    for (int32 Seed = 1; Seed <= 60000; ++Seed)
    {
        auto Candidate = UBreakerLootLibrary::RollItem(TEXT("Prolific.Discovery"),
            EBreakerEquipSlot::Gloves, EBreakerItemRarity::Unwritten, 120, Seed);
        const int32 Index = Candidate.Affixes.IndexOfByPredicate([](const FBreakerRolledAffix& Affix)
        { return Affix.AffixId == TEXT("Downside.Riftplate") && Affix.Tier == 1; });
        if (Candidate.Rule == EBreakerItemRule::Prolific && Index != INDEX_NONE
            && !Candidate.Affixes.ContainsByPredicate([](const FBreakerRolledAffix& Affix)
            { return Affix.AffixId == TEXT("Core.MoveSpeed"); }))
        {
            Item = Candidate;
            BillIndex = Index;
            break;
        }
        Salvage.Add(UBreakerForgeLibrary::SalvageValue(Candidate).Get());
    }
    if (!TestTrue(TEXT("Actual T1 Prolific Riftplate drop discovered"), BillIndex != INDEX_NONE)) return false;
    const auto* Bill = UBreakerAffixLibrary::FindAffix(UBreakerAffixLibrary::GetSliceAffixPool(), TEXT("Downside.Riftplate"));
    if (!TestNotNull(TEXT("Shipped paired bill"), Bill)) return false;
    TestEqual(TEXT("Printed downside T1"), Item.Affixes[BillIndex].Value, -10.0f, .0001f);
    TestEqual(TEXT("Authored signed T0 bill"), UBreakerAffixLibrary::ValueForTier(*Bill, 0), -22.0f, .0001f);
    TestEqual(TEXT("Authored signed top bill"), UBreakerAffixLibrary::ValueForTier(*Bill, -1), -36.0f, .0001f);

    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    World->InitializeActorsForPlay(FURL());
    auto* Player = World->SpawnActor<ABreakerCharacter>();
    if (!Player) return false;
    auto* Attributes = Player->GetAttributes();
    auto* ASC = Player->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Player, Player);
    ASC->AddAttributeSetSubobject(Attributes);
    auto* Equipment = Player->GetEquipment();
    Equipment->BindAttributes(Attributes);

    auto CheckEquipped = [&](const FBreakerItemInstance& Subject)
    {
        if (!TestTrue(TEXT("Actual rolled/tempered item equips"), Equipment->EquipItem(Subject))) return;
        // Diagnostic copies never enter equipment. Remove only the bill to isolate
        // its signed contribution from other naturally rolled movement affixes.
        auto WithoutBill = Subject;
        WithoutBill.Affixes.RemoveAt(BillIndex);
        const float EffectiveBill = UBreakerAffixLibrary::ValueForTier(*Bill,
            FMath::Max(Subject.Affixes[BillIndex].Tier - 1, -1));
        const auto OtherStats = UBreakerEquipmentComponent::AggregateStats({WithoutBill});
        TestTrue(TEXT("Signed result remains finite"), FMath::IsFinite(Equipment->GetStats().MoveSpeedMultiplier));
        TestEqual(TEXT("Real equipped movement includes the entire signed bill"),
            Equipment->GetStats().MoveSpeedMultiplier, OtherStats.MoveSpeedMultiplier + EffectiveBill / 100.0f, .0001f);
        FBreakerItemInstance Saved;
        TestTrue(TEXT("Equipped item remains readable"), Equipment->GetEquippedItem(Subject.Slot, Saved));
        TestEqual(TEXT("Resolution never rewrites saved value"), Saved.Affixes[BillIndex].Value, Subject.Affixes[BillIndex].Value, .0001f);
        TestEqual(TEXT("Resolution never rewrites saved tier"), Saved.Affixes[BillIndex].Tier, Subject.Affixes[BillIndex].Tier);
    };
    CheckEquipped(Item);
    auto WithoutBenefit = Item;
    const int32 BenefitIndex = Item.Affixes.IndexOfByPredicate([](const FBreakerRolledAffix& Affix)
    { return Affix.AffixId == TEXT("Anomaly.Riftplate"); });
    if (!TestTrue(TEXT("Actual paired positive signature exists"), BenefitIndex != INDEX_NONE)) return false;
    WithoutBenefit.Affixes.RemoveAt(BenefitIndex);
    TestEqual(TEXT("Positive in-band benefit still receives its tier ratio"),
        Equipment->GetStats().BonusArmour - UBreakerEquipmentComponent::AggregateStats({WithoutBenefit}).BonusArmour,
        Item.Affixes[BenefitIndex].Value * UBreakerAffixLibrary::TierSpikeT0Multiplier, .001f);

    const auto Ordinary = UBreakerLootLibrary::RollItem(TEXT("Prolific.Control"),
        EBreakerEquipSlot::Helmet, EBreakerItemRarity::Standard, 120, 37);
    TestTrue(TEXT("Unrelated ordinary item equips"), Equipment->EquipItem(Ordinary));
    TestEqual(TEXT("Prolific never uplifts another item's armour"), Equipment->GetStats().BonusArmour,
        UBreakerEquipmentComponent::AggregateStats({Item}).BonusArmour
        + UBreakerEquipmentComponent::AggregateStats({Ordinary}).BonusArmour, .001f);
    // Remove the unrelated control via the normal equipment API before each bill comparison.
    TestTrue(TEXT("Unrelated control unequips"), Equipment->UnequipSlot(EBreakerEquipSlot::Helmet));
    // Pay actual Forge costs from salvaged discovery rolls; no fabricated affix values.
    if (!TestTrue(TEXT("Real Temper pays its cost"),
        UBreakerForgeLibrary::Temper(Item, BillIndex, Salvage, true) == EBreakerForgeResult::Success)) return false;
    TestEqual(TEXT("Temper writes the same signed T0 table"), Item.Affixes[BillIndex].Value, -22.0f, .0001f);
    CheckEquipped(Item);
    const int32 BeforeRefusal = Salvage.Get();
    TestTrue(TEXT("Printed T0 is Prolific's legal ceiling; effective tier is already top"),
        UBreakerForgeLibrary::Temper(Item, BillIndex, Salvage, true) == EBreakerForgeResult::AtTierCeiling);
    TestEqual(TEXT("Ceiling refusal costs nothing"), Salvage.Get(), BeforeRefusal);
    // Defensive aggregation of an existing top-tier saved line remains bounded.
    // This diagnostic copy is not a claim that the current Forge creates it.
    auto ExistingTop = Item;
    ExistingTop.Affixes[BillIndex].Tier = -1;
    ExistingTop.Affixes[BillIndex].Value = -36.0f;
    TestEqual(TEXT("Existing top-tier downside never exceeds its saved budget"),
        UBreakerEquipmentComponent::AggregateStats({ExistingTop}).MoveSpeedMultiplier,
        Equipment->GetStats().MoveSpeedMultiplier, .0001f);
    return true;
}
#endif
