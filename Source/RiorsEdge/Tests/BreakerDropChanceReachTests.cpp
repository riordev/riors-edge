#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Items/BreakerDropTable.h"
#include "Combat/BreakerMonsterChassis.h"

// ---------------------------------------------------------------------------
// A STAT THE PLAYER CAN SEE MUST DO SOMETHING AT EVERY RANK.
//
// BreakerDropTable.h says this itself, at the Drop Chance affix's own
// declaration: a stat printed as "Drop Chance" that changed only which rarity
// came out of a drop you were getting anyway is "the kind of quiet lie this
// project has shipped before".
//
// It is that, on the two ranks where it matters most. GetEffectiveDropChance is
// Clamp(Base * (1 + Bonus), 0, 1), and BossDropChance is authored at 1.0 --
// already the top of its own ClampMax. So a boss drops on every kill with no
// affix at all, and every point of Drop Chance a player has ever rolled buys
// exactly nothing there. ModifierBearing at 0.90 saturates on an 11% roll.
//
// TWO CONSEQUENCES, and the second is why this is a test rather than a note:
//
//   The affix is dead where the loot is best. That is a player-facing lie in
//   the precise sense the header names.
//
//   The drop-chance AXIS cannot express a difficulty ratio above 10x at any
//   tuning, because a probability cannot exceed 1.0 against a 0.10 trash floor.
//   It is a saturated scale, not a tuned value -- which is exactly why the Boss
//   row disagrees with its own Riftglass row by 11x. Any reward composition
//   built on top of it inherits that ceiling.
//
// EXPECTED RED. "A boss always drops" may well be the right design; what cannot
// stand is a visible stat that silently does nothing on the rank it is most
// bought for. The ruling decides which half moves.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerDropChanceReachesEveryRankTest,
    "RiorsEdge.Items.Drops.DropChanceReachesEveryRank",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerDropChanceReachesEveryRankTest::RunTest(const FString& Parameters)
{
    const FBreakerDropTableParams Params;

    // A generous but reachable roll, so a failure cannot be blamed on the
    // bonus being too small to notice.
    constexpr float GenerousBonusPercent = 50.0f;

    struct FRank { EBreakerMonsterRank Rank; const TCHAR* Name; };
    const FRank Ranks[] = {
        { EBreakerMonsterRank::Trash,           TEXT("Trash") },
        { EBreakerMonsterRank::Elite,           TEXT("Elite") },
        { EBreakerMonsterRank::ModifierBearing, TEXT("ModifierBearing") },
        { EBreakerMonsterRank::Boss,            TEXT("Boss") },
    };

    for (const FRank& Entry : Ranks)
    {
        const float Without = UBreakerDropTableLibrary::GetEffectiveDropChance(Entry.Rank, 0.0f, Params);
        const float With = UBreakerDropTableLibrary::GetEffectiveDropChance(Entry.Rank, GenerousBonusPercent, Params);
        AddInfo(FString::Printf(TEXT("%s: %.3f without the affix, %.3f with +%.0f%%"),
            Entry.Name, Without, With, GenerousBonusPercent));
        TestTrue(*FString::Printf(
            TEXT("%s: a Drop Chance roll buys something (%.3f -> %.3f)"), Entry.Name, Without, With),
            With > Without + UE_KINDA_SMALL_NUMBER);
    }

    // And the axis itself, stated independently of the affix: the spread this
    // scale can express is capped by the probability ceiling, which is what
    // makes it unusable as a difficulty term.
    const float TrashChance = UBreakerDropTableLibrary::GetEffectiveDropChance(EBreakerMonsterRank::Trash, 0.0f, Params);
    const float BossChance = UBreakerDropTableLibrary::GetEffectiveDropChance(EBreakerMonsterRank::Boss, 0.0f, Params);
    AddInfo(FString::Printf(
        TEXT("Drop-chance axis spread, trash to boss: %.1fx. A probability cannot exceed 1.0, so this scale "
             "cannot express more than %.1fx at any tuning."),
        TrashChance > 0.0f ? BossChance / TrashChance : 0.0f,
        TrashChance > 0.0f ? 1.0f / TrashChance : 0.0f));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
