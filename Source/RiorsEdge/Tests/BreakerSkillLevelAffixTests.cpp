#include "Misc/AutomationTest.h"
#include "Abilities/BreakerSkillLevelMath.h"
#include "Items/BreakerAffixLibrary.h"
#include "Items/BreakerItemTypes.h"
#include "Items/BreakerLootLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS

// ---------------------------------------------------------------------------
// O253, THE ROLLED HALF. RiorsEdge.Abilities.SkillLevel already proves the
// band table as arithmetic; this proves the SHIPPED CONFIGURATION — that the
// line exists in the live pool, that it rolls only where O253 says it may,
// and that what a real drop carries is a whole number and never an
// interpolation. A rule proved only against a hand-built definition is the
// failure this project has already shipped once.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerSkillLevelAffixTest,
    "RiorsEdge.Items.SkillLevelAffix",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerSkillLevelAffixTest::RunTest(const FString&)
{
    const FName SkillId(TEXT("Ability.SkillLevel"));
    const auto& Pool = UBreakerAffixLibrary::GetSliceAffixPool();
    const auto* Skill = UBreakerAffixLibrary::FindAffix(Pool, SkillId);
    if (!TestNotNull(TEXT("the shipped slice pool carries the skill-level line"), Skill)) return false;

    // ---- The shipped row is what O253 authorises, and no more -------------
    TestEqual(TEXT("it bids flat, never into an increased bucket"), Skill->StatBucket, EBreakerStatBucket::Flat);
    TestEqual(TEXT("it targets the skill-level lane"), Skill->StatTarget, EBreakerStatTarget::SkillLevel);
    TestTrue(TEXT("it rolls on the Necklace"), Skill->AllowsSlot(EBreakerEquipSlot::Necklace));
    TestTrue(TEXT("it rolls on the Waist"), Skill->AllowsSlot(EBreakerEquipSlot::Waist));
    for (const auto Slot : {EBreakerEquipSlot::Helmet, EBreakerEquipSlot::BodyArmour, EBreakerEquipSlot::Gloves,
        EBreakerEquipSlot::Boots, EBreakerEquipSlot::Primary, EBreakerEquipSlot::Secondary})
    {
        TestFalse(TEXT("and on no other slot (O253 names exactly two)"), Skill->AllowsSlot(Slot));
    }

    // ---- A whole number at every tier, whatever the in-band roll ----------
    // The variance lerp is what would turn 3 into 2.37, so it is asserted
    // absent rather than assumed absent: same value at every roll position.
    for (int32 Tier = 12; Tier >= -1; --Tier)
    {
        const int32 Expected = BreakerSkillLevel::AffixLevelsForTier(Tier);
        for (const float Roll : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f})
        {
            const float Value = UBreakerAffixLibrary::RollValueForTier(*Skill, Tier, Roll);
            TestEqual(*FString::Printf(TEXT("T%d is exactly %d levels at roll %.2f"), Tier, Expected, Roll),
                Value, static_cast<float>(Expected));
            TestEqual(*FString::Printf(TEXT("T%d carries no fraction"), Tier), Value, FMath::RoundToFloat(Value));
        }
    }

    // ---- What actually drops ---------------------------------------------
    // Rolled through the shipped allocator rather than constructed, because
    // the question worth asking is whether a player can find this line at all.
    bool bFoundOnNecklace = false;
    bool bFoundOnWaist = false;
    for (const auto Rarity : {EBreakerItemRarity::Standard, EBreakerItemRarity::Uncommon,
        EBreakerItemRarity::Exceptional, EBreakerItemRarity::Aberrant, EBreakerItemRarity::Unwritten})
    {
        for (const auto Slot : {EBreakerEquipSlot::Necklace, EBreakerEquipSlot::Waist,
            EBreakerEquipSlot::Helmet, EBreakerEquipSlot::Boots})
        {
            for (const int32 ItemLevel : {1, 50, 120})
            {
                for (int32 Seed = 1; Seed <= 60; ++Seed)
                {
                    const auto Item = UBreakerLootLibrary::RollItem(TEXT("SkillLevel.Policy"), Slot, Rarity, ItemLevel, Seed);
                    if (!TestTrue(TEXT("the allocator returns a complete item"), Item.IsValid())) return false;
                    for (const auto& Line : Item.Affixes)
                    {
                        if (Line.AffixId != SkillId) continue;
                        bFoundOnNecklace |= Slot == EBreakerEquipSlot::Necklace;
                        bFoundOnWaist |= Slot == EBreakerEquipSlot::Waist;
                        TestTrue(TEXT("a dropped skill line never lands off its two slots"),
                            Slot == EBreakerEquipSlot::Necklace || Slot == EBreakerEquipSlot::Waist);
                        TestEqual(TEXT("a dropped skill line is a whole number of levels"),
                            Line.Value, FMath::RoundToFloat(Line.Value));
                        TestEqual(TEXT("and it is the value its own tier authorises"),
                            Line.Value, static_cast<float>(BreakerSkillLevel::AffixLevelsForTier(Line.Tier)));
                        TestTrue(TEXT("never more than the per-affix ceiling"),
                            Line.Value <= static_cast<float>(BreakerSkillLevel::MaxAffixLevels));
                    }
                }
            }
        }
    }

    // REACHABILITY. Content the player cannot reach is not built, so the line
    // has to be findable on both slots the ruling names — not merely legal on
    // them. If this goes red the affix exists only in the pool file.
    TestTrue(TEXT("the line is reachable on a rolled Necklace"), bFoundOnNecklace);
    TestTrue(TEXT("the line is reachable on a rolled Waist"), bFoundOnWaist);

    return true;
}

#endif
