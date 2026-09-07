#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/BreakerStatusComponent.h"
#include "Combat/BreakerStatusRuleMath.h"
#include "Combat/BreakerStatusRules.h"

// The shipped status vocabulary, read off the library the game reads: every
// tag production applies has a row, every row names a registered tag, and
// exactly Poison spreads on pierce. A file that failed to load names its
// breaks first rather than failing a count.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerStatusRulesShippedTest,
    "RiorsEdge.Combat.Status.RulesShipped",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerStatusRulesShippedTest::RunTest(const FString& Parameters)
{
    const TArray<FString>& LoadErrors = BreakerStatusRules::GetDataErrors();
    for (const FString& Error : LoadErrors)
    {
        AddError(Error);
    }
    if (!LoadErrors.IsEmpty())
    {
        return false;
    }

    // The production-applied tags: the weapon's and Cleave's Bleed, Rot's
    // Poison. Status.Void is a damage tag and has no row by design.
    const TCHAR* const AppliedTags[] = { TEXT("Status.Bleed"), TEXT("Status.Poison") };
    for (const TCHAR* Name : AppliedTags)
    {
        const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(Name, false);
        TestTrue(FString::Printf(TEXT("%s is a registered tag"), Name), Tag.IsValid());
        TestNotNull(FString::Printf(TEXT("%s has a row"), Name), BreakerStatusRules::FindRule(Tag));
    }

    int32 Spreading = 0;
    for (const FBreakerStatusRule& Rule : BreakerStatusRules::GetRules())
    {
        TestTrue(FString::Printf(TEXT("Row %s names a registered tag"), *Rule.Tag.ToString()), Rule.Tag.IsValid());
        if (Rule.bSpreadsOnPierce) ++Spreading;
    }
    TestEqual(TEXT("Exactly one status spreads on pierce"), Spreading, 1);
    const FBreakerStatusRule* Poison = BreakerStatusRules::FindRule(FGameplayTag::RequestGameplayTag(TEXT("Status.Poison"), false));
    TestTrue(TEXT("Poison is the one that spreads"), Poison && Poison->bSpreadsOnPierce);
    const FBreakerStatusRule* Bleed = BreakerStatusRules::FindRule(FGameplayTag::RequestGameplayTag(TEXT("Status.Bleed"), false));
    TestTrue(TEXT("Bleed stays on the body it opened"), Bleed && !Bleed->bSpreadsOnPierce);

    const float Fraction = BreakerStatusRules::PierceSpreadPayloadFraction();
    TestTrue(TEXT("The spread payload fraction is in (0, 1]"), Fraction > 0.0f && Fraction <= 1.0f);

    TestNull(TEXT("A tagless lookup finds nothing"), BreakerStatusRules::FindRule(FGameplayTag()));
    TestNull(TEXT("A damage tag has no status row"),
        BreakerStatusRules::FindRule(FGameplayTag::RequestGameplayTag(TEXT("Status.Void"), false)));
    return true;
}

// Pure: the spread copy is the source's remaining budget at a normalized
// payload, with the snapshot untouched, and a copy is never itself a source.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerStatusPierceSpreadTest,
    "RiorsEdge.Combat.Status.PierceSpread",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerStatusPierceSpreadTest::RunTest(const FString& Parameters)
{
    FBreakerActiveStatus Source;
    Source.Spec.StatusTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Poison"), false);
    Source.Spec.BaseDamagePerTick = 8.0f;
    Source.Spec.Duration = 6.0f;
    Source.Spec.TickInterval = 0.5f;
    Source.Spec.InitialStacks = 2;
    Source.Spec.ProcCoefficient = 1.0f;
    Source.Spec.Snapshot.SourcePower = 123.0f;
    Source.Spec.Snapshot.CriticalChance = 0.35f;
    Source.Spec.Snapshot.CriticalMultiplier = 2.25f;
    Source.Spec.Snapshot.DamageOverTimeMultiplier = 1.4f;
    Source.Spec.Snapshot.bRolledCritical = true;
    Source.Stacks = 4;
    Source.RemainingDuration = 2.5f;

    TestTrue(TEXT("An original is a spread source"), FBreakerStatusRuleMath::IsPierceSpreadSource(Source));

    const FBreakerStatusApplicationSpec Copy = FBreakerStatusRuleMath::MakePierceSpreadSpec(Source, 0.5f);
    TestTrue(TEXT("The copy keeps the tag"), Copy.StatusTag == Source.Spec.StatusTag);
    TestEqual(TEXT("The copy's duration is what the source had LEFT, not the authored duration"), Copy.Duration, 2.5f);
    TestEqual(TEXT("The copy carries the normalized payload"), Copy.BaseDamagePerTick, 4.0f);
    TestEqual(TEXT("The copy is at proc coefficient 0"), Copy.ProcCoefficient, 0.0f);
    TestEqual(TEXT("The copy keeps the tick interval"), Copy.TickInterval, 0.5f);
    TestEqual(TEXT("The copy keeps the initial stacks"), Copy.InitialStacks, 2);
    TestEqual(TEXT("The snapshot's source power is identical"), Copy.Snapshot.SourcePower, 123.0f);
    TestEqual(TEXT("The snapshot's crit chance is identical"), Copy.Snapshot.CriticalChance, 0.35f);
    TestEqual(TEXT("The snapshot's crit multiplier is identical"), Copy.Snapshot.CriticalMultiplier, 2.25f);
    TestEqual(TEXT("The snapshot's DoT multiplier is identical"), Copy.Snapshot.DamageOverTimeMultiplier, 1.4f);
    TestTrue(TEXT("The snapshot's rolled crit is identical"), Copy.Snapshot.bRolledCritical);

    // Depth 2: a spread copy, once running on a body, is not a source for
    // the next pierce; the ancestry stops there.
    FBreakerActiveStatus Running;
    Running.Spec = Copy;
    Running.RemainingDuration = 2.0f;
    TestFalse(TEXT("A spread copy is not itself a spread source"), FBreakerStatusRuleMath::IsPierceSpreadSource(Running));

    // A full payload is the ceiling; nothing above it, nothing below zero.
    TestEqual(TEXT("A payload fraction of 1 carries the whole tick"),
        FBreakerStatusRuleMath::MakePierceSpreadSpec(Source, 1.0f).BaseDamagePerTick, 8.0f);
    TestEqual(TEXT("A payload fraction above 1 is clamped to the whole tick"),
        FBreakerStatusRuleMath::MakePierceSpreadSpec(Source, 3.0f).BaseDamagePerTick, 8.0f);

    // An expired source yields a zero-duration copy, which ApplyStatus refuses.
    Source.RemainingDuration = -0.1f;
    TestEqual(TEXT("A spent source copies no duration"), FBreakerStatusRuleMath::MakePierceSpreadSpec(Source, 0.5f).Duration, 0.0f);
    return true;
}

#endif
