#include "Misc/AutomationTest.h"
#include "Abilities/BreakerSkillLevelMath.h"
#include "Progression/BreakerExperience.h"

#if WITH_DEV_AUTOMATION_TESTS

// ---------------------------------------------------------------------------
// O252. The rule is arithmetic, so it is proved here without a world — and
// then proved AGAINST THE SHIPPED CONFIGURATION, which is the half that
// matters: RiorsEdge.Movement.JumpGrant passed for a whole milestone against
// a level the game could not produce. The character cap is not restated as a
// literal anywhere below; it is read from the library that owns it, so a
// change to the cap moves this test rather than silently invalidating it.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerSkillLevelTest,
    "RiorsEdge.Abilities.SkillLevel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerSkillLevelTest::RunTest(const FString&)
{
    using namespace BreakerSkillLevel;
    constexpr int32 Cap = UBreakerExperienceLibrary::MaxCharacterLevel;

    // ---- The ladder's two ends, against the SHIPPED cap -------------------
    TestEqual(TEXT("a level 1 character carries skill level 1"), ForCharacterLevel(1, Cap), MinLevel);
    TestEqual(TEXT("a capped character carries the top skill level"), ForCharacterLevel(Cap, Cap), MaxLevel);
    TestEqual(TEXT("O252's ladder is fifteen rungs"), MaxLevel, 15);

    // ---- Monotonic, and never outside the ladder --------------------------
    int32 Previous = ForCharacterLevel(1, Cap);
    for (int32 Level = 1; Level <= Cap; ++Level)
    {
        const int32 Skill = ForCharacterLevel(Level, Cap);
        TestTrue(*FString::Printf(TEXT("skill level never falls, character %d"), Level), Skill >= Previous);
        TestTrue(*FString::Printf(TEXT("skill level stays inside [1,15], character %d"), Level),
            Skill >= MinLevel && Skill <= MaxLevel);
        Previous = Skill;
    }

    // ---- Out-of-range input is clamped, never extrapolated ----------------
    TestEqual(TEXT("below level 1 clamps to the floor"), ForCharacterLevel(0, Cap), MinLevel);
    TestEqual(TEXT("negative clamps to the floor"), ForCharacterLevel(-40, Cap), MinLevel);
    TestEqual(TEXT("past the cap clamps to the ceiling"), ForCharacterLevel(Cap * 3, Cap), MaxLevel);
    TestEqual(TEXT("a degenerate cap still answers"), ForCharacterLevel(5, 1), MinLevel);

    // ---- The multiplier ---------------------------------------------------
    // Skill level 1 must be EXACTLY 1.0. Every ability's authored numbers were
    // tuned at the bottom of this ladder, so a multiplier that started above
    // one would silently rescale the whole existing kit.
    TestEqual(TEXT("skill level 1 multiplies by exactly one"), DamageMultiplier(MinLevel), 1.0f);
    TestTrue(TEXT("the multiplier rises with the level"),
        DamageMultiplier(MaxLevel) > DamageMultiplier(MinLevel));
    TestEqual(TEXT("the top of the ladder composes to the authored total"),
        DamageMultiplier(MaxLevel), 1.0f + DamagePerLevel * static_cast<float>(MaxLevel - MinLevel), 0.0001f);

    // Clamped at both ends rather than extrapolated: a caller that hands this
    // a level off the ladder gets the ladder's edge, not a made-up number.
    TestEqual(TEXT("a sub-floor skill level clamps"), DamageMultiplier(-3), DamageMultiplier(MinLevel));
    TestEqual(TEXT("a super-ceiling skill level clamps"), DamageMultiplier(MaxLevel + 9), DamageMultiplier(MaxLevel));

    // Every step is the same size, which is what makes the ladder readable.
    for (int32 Level = MinLevel; Level < MaxLevel; ++Level)
    {
        TestEqual(*FString::Printf(TEXT("step %d is one authored increment"), Level),
            DamageMultiplier(Level + 1) - DamageMultiplier(Level), DamagePerLevel, 0.0001f);
    }

    return true;
}

#endif
