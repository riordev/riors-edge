#include "Misc/AutomationTest.h"
#include "Items/BreakerAffixLibrary.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerForgeLibrary.h"
#include "Items/BreakerLootLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerPierceAffixTest, "RiorsEdge.Items.PierceEligibilityAndTreatment",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerPierceAffixTest::RunTest(const FString& Parameters)
{
    const FName PierceId(TEXT("Weapon.Pierce"));
    const auto& Pool = UBreakerAffixLibrary::GetSliceAffixPool();
    const auto* Pierce = UBreakerAffixLibrary::FindAffix(Pool, PierceId);
    const auto* Accuracy = UBreakerAffixLibrary::FindAffix(Pool, TEXT("Weapon.SustainedAccuracy"));
    const auto* Traversal = UBreakerAffixLibrary::FindAffix(Pool, TEXT("Offense.WallRideDamage"));
    if (!Pierce || !Accuracy || !Traversal) return false;
    TestEqual(TEXT("Accuracy T12 anchor"), Accuracy->ValueAtT12, 4.0f);
    TestEqual(TEXT("Accuracy T1 anchor"), Accuracy->ValueAtT1, 18.0f);
    TestEqual(TEXT("Traversal migration retains saved row and authored magnitude"), Traversal->ValueAtT1, 57.0f);
    TestEqual(TEXT("Traversal now reads completed ledge condition"), Traversal->Condition, EBreakerBuildCondition::RecentlyLedgeTraversed);
    for (int32 Tier = 12; Tier >= -1; --Tier)
        for (float Roll : {0.0f, .5f, 1.0f})
            TestEqual(TEXT("Pierce always uses exact discrete tier value"), UBreakerAffixLibrary::RollValueForTier(*Pierce, Tier, Roll),
                Tier > 4 ? 0.0f : Tier == -1 ? 2.0f : 1.0f);
    FBreakerItemInstance Example;
    bool Found = false, SawRocket = false, SawAccuracy = false;
    for (const auto Rarity : {EBreakerItemRarity::Standard, EBreakerItemRarity::Uncommon, EBreakerItemRarity::Exceptional,
        EBreakerItemRarity::Aberrant, EBreakerItemRarity::Anomalous})
    {
        for (int32 Seed = 1; Seed <= 400; ++Seed)
        {
            // T4 lies beyond character-cap item level; these are explicit
            // endgame i120 rolls, not a claim that campaign i50 can roll Pierce.
            const auto Item = UBreakerLootLibrary::RollItem(TEXT("Pierce.Policy"), EBreakerEquipSlot::Primary, Rarity, 120, Seed);
            if (!TestTrue(TEXT("Policy preserves complete loot allocation"), Item.IsValid())) return false;
            SawRocket |= Item.WeaponArchetype == EBreakerWeaponArchetype::Rocket;
            for (const auto& Line : Item.Affixes)
            {
                SawAccuracy |= Line.AffixId == Accuracy->AffixId;
                if (Line.AffixId != PierceId) continue;
                TestTrue(TEXT("All ordinary and special allocator paths exclude Rocket pierce"), Item.WeaponArchetype != EBreakerWeaponArchetype::Rocket);
                TestTrue(TEXT("Pierce never rolls below its eligible tier"), Line.Tier <= 4);
                TestEqual(TEXT("Dropped pierce cannot interpolate"), Line.Value, Line.Tier == -1 ? 2.0f : 1.0f);
                if (Rarity == EBreakerItemRarity::Exceptional) { Example = Item; Found = true; }
            }
            const auto Low = UBreakerLootLibrary::RollItem(TEXT("Pierce.Low"), EBreakerEquipSlot::Primary, EBreakerItemRarity::Standard, 1, Seed);
            TestFalse(TEXT("Low item-level ceiling excludes Pierce"), Low.Affixes.ContainsByPredicate([&](const auto& Line) { return Line.AffixId == PierceId; }));
            const auto CampaignCap = UBreakerLootLibrary::RollItem(TEXT("Pierce.CampaignCap"), EBreakerEquipSlot::Primary, Rarity, 50, Seed);
            TestFalse(TEXT("Campaign i50 ceiling still excludes Pierce across rarities"), CampaignCap.Affixes.ContainsByPredicate([&](const auto& Line) { return Line.AffixId == PierceId; }));
        }
    }
    if (!TestTrue(TEXT("Actual rolls cover Pierce, accuracy and Rocket exclusions"), Found && SawRocket && SawAccuracy)) return false;
    // Isolate the treatment/aggregation consumer with one real rolled line.
    Example.Affixes.RemoveAll([&](const auto& Line) { return Line.AffixId != PierceId; });
    Example.Rule = EBreakerItemRule::None;
    TestEqual(TEXT("Treatment keeps actual rolled Exceptional entitlement"), Example.Rarity, EBreakerItemRarity::Exceptional);
    TestEqual(TEXT("Actual Exceptional temper ceiling reaches T-1"), UBreakerForgeLibrary::TemperCeilingForItem(Example), -1);
    Example.Affixes[0].Tier = 4; Example.Affixes[0].Value = 1;
    FBreakerForgeWallet Wallet; Wallet.Riftglass = 100000000;
    for (int32 Tier = 4; Tier >= -1; --Tier)
    {
        for (int32 Seed = 0; Seed < 16; ++Seed)
        {
            TestEqual(TEXT("Legal pierce reforge succeeds"), UBreakerForgeLibrary::Reforge(Example, Wallet, true, Seed), EBreakerForgeResult::Success);
            TestEqual(TEXT("Reforge never creates fractional pierce"), Example.Affixes[0].Value, Tier == -1 ? 2.0f : 1.0f);
        }
        if (Tier > -1) TestEqual(TEXT("Real temper advances discrete tier"), UBreakerForgeLibrary::Temper(Example, 0, Wallet, true), EBreakerForgeResult::Success);
    }
    Example.Affixes[0].Tier = 0; Example.Affixes[0].Value = 1.7f; Example.Rule = EBreakerItemRule::Prolific;
    TestEqual(TEXT("Prolific reaches exact effective T-1 without fractional inflation"), UBreakerEquipmentComponent::AggregateStats({Example}).PrimaryPierceCount, 2);
    Example.Affixes[0].Tier = 1;
    TestEqual(TEXT("Prolific effective T0 remains one pierce"), UBreakerEquipmentComponent::AggregateStats({Example}).PrimaryPierceCount, 1);
    Example.Rule = EBreakerItemRule::None;
    Example.WeaponArchetype = EBreakerWeaponArchetype::Rocket;
    TestTrue(TEXT("Rocket retains accuracy because its real projectile uses bloom spread"), UBreakerAffixLibrary::IsEligibleForItem(*Accuracy, Example, 1));
    const int32 Before = Wallet.Riftglass;
    TestEqual(TEXT("Malformed Rocket pierce cannot be reforged"), UBreakerForgeLibrary::Reforge(Example, Wallet, true, 1), EBreakerForgeResult::InvalidAffix);
    TestEqual(TEXT("Malformed Rocket pierce cannot be tempered"), UBreakerForgeLibrary::Temper(Example, 0, Wallet, true), EBreakerForgeResult::InvalidAffix);
    TestEqual(TEXT("Refusals do not spend currency"), Wallet.Riftglass, Before);
    TestEqual(TEXT("Malformed Rocket contributes no pierce"), UBreakerEquipmentComponent::AggregateStats({Example}).PrimaryPierceCount, 0);
    auto WrongSlot = Example;
    WrongSlot.WeaponArchetype = EBreakerWeaponArchetype::Rifle;
    WrongSlot.Slot = EBreakerEquipSlot::Secondary;
    TestEqual(TEXT("Malformed Secondary contributes no Primary pierce"), UBreakerEquipmentComponent::AggregateStats({WrongSlot}).PrimaryPierceCount, 0);
    bool AttunedPierce = false;
    for (int32 Seed = 0; Seed < 500; ++Seed)
    {
        auto Rocket = Example;
        TestEqual(TEXT("Attune can replace stale Rocket line legally"), UBreakerForgeLibrary::Attune(Rocket, Wallet, true, Seed), EBreakerForgeResult::Success);
        TestFalse(TEXT("Attune never offers Rocket pierce"), Rocket.Affixes.ContainsByPredicate([&](const auto& Line) { return Line.AffixId == PierceId; }));
        auto Rifle = Example; Rifle.WeaponArchetype = EBreakerWeaponArchetype::Rifle;
        UBreakerForgeLibrary::Attune(Rifle, Wallet, true, Seed);
        for (const auto& Line : Rifle.Affixes)
            if (Line.AffixId == PierceId) { AttunedPierce = true; TestEqual(TEXT("Attuned pierce remains integer"), Line.Value, 1.0f); }
    }
    TestTrue(TEXT("Legal Rifle attune actually offers Pierce"), AttunedPierce);
    return true;
}
#endif
