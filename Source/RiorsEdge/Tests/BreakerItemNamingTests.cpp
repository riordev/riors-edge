#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Items/BreakerItemNaming.h"

namespace
{
    FBreakerRolledAffix BreakerItemNamingRoll(const TCHAR* Id, int32 Tier, EBreakerAffixCategory Category)
    {
        FBreakerRolledAffix Affix;
        Affix.AffixId = FName(Id);
        Affix.Tier = Tier;
        Affix.Category = Category;
        return Affix;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerItemNamingTest,
    "RiorsEdge.Items.Naming.Headline",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerItemNamingTest::RunTest(const FString& Parameters)
{
    using namespace BreakerItemNaming;
    const auto Prefix = EBreakerAffixCategory::Prefix;
    const auto Suffix = EBreakerAffixCategory::Suffix;

    // TIERS COUNT DOWN. The strongest roll is the smallest number, which is
    // the one thing a naming rule can get backwards without anyone noticing
    // until every item in the game is named after its worst line.
    TArray<FBreakerRolledAffix> Item{
        BreakerItemNamingRoll(TEXT("Offense.WeaponDamage"), 8, Prefix),
        BreakerItemNamingRoll(TEXT("Crit.Chance"), 2, Prefix),
        BreakerItemNamingRoll(TEXT("Core.Health"), 11, Suffix),
        BreakerItemNamingRoll(TEXT("Core.ResourceRegen"), 4, Suffix),
    };
    const FBreakerRolledAffix* BestPrefix = StrongestOfCategory(Item, Prefix);
    const FBreakerRolledAffix* BestSuffix = StrongestOfCategory(Item, Suffix);
    if (!TestNotNull(TEXT("an item with prefixes has a prefix headline"), BestPrefix)) return false;
    if (!TestNotNull(TEXT("an item with suffixes has a suffix headline"), BestSuffix)) return false;
    TestEqual(TEXT("the strongest prefix is the lowest tier"), BestPrefix->AffixId, FName(TEXT("Crit.Chance")));
    TestEqual(TEXT("the strongest suffix is the lowest tier"), BestSuffix->AffixId, FName(TEXT("Core.ResourceRegen")));

    // An untiered line is a special with no ladder at all. On an item that has
    // one it IS the reason the item exists, so it outranks every tiered roll.
    TArray<FBreakerRolledAffix> Special{
        BreakerItemNamingRoll(TEXT("Crit.Chance"), 1, Prefix),
        BreakerItemNamingRoll(TEXT("Anomaly.EntropyDebt"), -1, Prefix),
    };
    TestEqual(TEXT("an untiered special outranks even a T1"),
        StrongestOfCategory(Special, Prefix)->AffixId, FName(TEXT("Anomaly.EntropyDebt")));

    // Two items rolled with the same lines must never disagree about their own
    // name, so a tie breaks on the id rather than on array order.
    TArray<FBreakerRolledAffix> Forward{
        BreakerItemNamingRoll(TEXT("Bravo"), 3, Prefix),
        BreakerItemNamingRoll(TEXT("Alpha"), 3, Prefix),
    };
    TArray<FBreakerRolledAffix> Reversed{
        BreakerItemNamingRoll(TEXT("Alpha"), 3, Prefix),
        BreakerItemNamingRoll(TEXT("Bravo"), 3, Prefix),
    };
    TestEqual(TEXT("a tie is broken deterministically, not by array order"),
        StrongestOfCategory(Forward, Prefix)->AffixId, StrongestOfCategory(Reversed, Prefix)->AffixId);

    // An item with nothing rolled has no headline. That is the correct answer
    // for a Standard drop, not a missing case.
    TArray<FBreakerRolledAffix> Bare;
    TestNull(TEXT("a bare item has no prefix headline"), StrongestOfCategory(Bare, Prefix));
    TestNull(TEXT("and no suffix headline"), StrongestOfCategory(Bare, Suffix));

    // The grammar. Either side may be missing and the name still reads.
    TestEqual(TEXT("both halves"), Compose(TEXT("Havoc"), TEXT("Boots"), TEXT("Cast Speed")),
        FString(TEXT("Havoc Boots of Cast Speed")));
    TestEqual(TEXT("suffix only"), Compose(FString(), TEXT("Boots"), TEXT("Cast Speed")),
        FString(TEXT("Boots of Cast Speed")));
    TestEqual(TEXT("prefix only"), Compose(TEXT("Havoc"), TEXT("Boots"), FString()),
        FString(TEXT("Havoc Boots")));
    TestEqual(TEXT("neither, so the base stands alone"), Compose(FString(), TEXT("Boots"), FString()),
        FString(TEXT("Boots")));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
