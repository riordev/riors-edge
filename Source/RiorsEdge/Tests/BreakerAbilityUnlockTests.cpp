#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Save/BreakerQuestContent.h"
#include "Save/BreakerQuestJournal.h"
#include "Save/BreakerSaveGame.h"

// ---------------------------------------------------------------------------
// O100: ABILITY ACQUISITION
// ---------------------------------------------------------------------------
// Four of five classes listed their whole kit as starters, so everything was
// free at level one and there was no acquisition system; Swift listed two, so
// Swift.CadenceBreak was registered, offered by the picker, and permanently
// refusable. One list doing two jobs is what let those states look alike.
//
// Every test here runs against the SHIPPED configuration. Nothing hands a
// component a token, a level or an unlock it did not earn — a reachability test
// that grants itself the thing it is testing for proves the spend path and says
// nothing about whether a player can ever reach it, which is how six branch
// keystones stayed unpurchasable for a milestone with a green suite.
// ---------------------------------------------------------------------------

namespace BreakerUnlockTest
{
    // Distinctively named for the unity build, per the twice-shipped rule about
    // anonymous-namespace collisions.
    const TArray<EBreakerClassId>& UnlockTestAllClasses()
    {
        static const TArray<EBreakerClassId> Classes = {
            EBreakerClassId::Swift, EBreakerClassId::Caster, EBreakerClassId::Gunsmith,
            EBreakerClassId::Tank, EBreakerClassId::Support };
        return Classes;
    }

    UBreakerProgressionComponent* UnlockTestMakeAt(EBreakerClassId ClassId, int32 Level)
    {
        UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>();
        Progression->ChoosePermanentClassById(ClassId);
        if (Level > 1)
        {
            // Through the shipped curve, so the level is one a player reaches.
            Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(Level, Progression->ExperienceCurve));
        }
        return Progression;
    }

    TSet<FName> UnlockTestRegisteredIds(EBreakerClassId ClassId)
    {
        TSet<FName> Registered;
        for (const EBreakerAbilitySlot Slot : {EBreakerAbilitySlot::ClassAbilityOne, EBreakerAbilitySlot::Ultimate})
        {
            for (const FName Id : UBreakerAbilityDefinition::GetClassAbilityIds(ClassId, Slot)) Registered.Add(Id);
        }
        return Registered;
    }
}

// (a) The partition. This pass's ONLY hand-authored data, so it is the one
// thing a typo can break silently: an omission is an ability nobody can reach,
// an overlap is an ability that is free and also for sale.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAbilityCataloguePartitionTest,
    "RiorsEdge.Abilities.Catalogue.Partition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAbilityCataloguePartitionTest::RunTest(const FString& Parameters)
{
    using namespace BreakerUnlockTest;
    for (const EBreakerClassId ClassId : UnlockTestAllClasses())
    {
        const UBreakerClassDefinition* Definition = UBreakerProgressionLibrary::GetFallbackClassDefinition(ClassId);
        const FString Context = UEnum::GetValueAsString(ClassId);
        if (!TestNotNull(*(Context + TEXT(" has a definition")), Definition)) continue;

        // Disjoint, in both directions and against the ultimate.
        for (const FName Starter : Definition->StarterAbilityIds)
        {
            TestFalse(*(Context + TEXT(" starter is not also unlockable: ") + Starter.ToString()),
                Definition->UnlockableAbilityIds.Contains(Starter));
            TestNotEqual(*(Context + TEXT(" starter is not the ultimate: ") + Starter.ToString()),
                Starter, Definition->BaseUltimateId);
        }
        for (const FName Unlockable : Definition->UnlockableAbilityIds)
        {
            TestNotEqual(*(Context + TEXT(" unlockable is not the ultimate: ") + Unlockable.ToString()),
                Unlockable, Definition->BaseUltimateId);
        }

        // And exhaustive: the three fields together are exactly the registered
        // set, no more and no less.
        TSet<FName> Partitioned;
        for (const FName Id : Definition->StarterAbilityIds) Partitioned.Add(Id);
        for (const FName Id : Definition->UnlockableAbilityIds) Partitioned.Add(Id);
        Partitioned.Add(Definition->BaseUltimateId);

        const TSet<FName> Registered = UnlockTestRegisteredIds(ClassId);
        TestEqual(*(Context + TEXT(" partitions its registered set exactly")), Partitioned.Num(), Registered.Num());
        for (const FName Id : Registered)
        {
            TestTrue(*(Context + TEXT(" catalogues registered id ") + Id.ToString()), Partitioned.Contains(Id));
        }
    }
    return true;
}

// (b) The CadenceBreak defect as an assertion: nothing the picker offers may be
// permanently refusable.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAbilityNoPermanentlyRefusableTest,
    "RiorsEdge.Abilities.Catalogue.NoPermanentlyRefusable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAbilityNoPermanentlyRefusableTest::RunTest(const FString& Parameters)
{
    using namespace BreakerUnlockTest;
    for (const EBreakerClassId ClassId : UnlockTestAllClasses())
    {
        const UBreakerClassDefinition* Definition = UBreakerProgressionLibrary::GetFallbackClassDefinition(ClassId);
        const FString Context = UEnum::GetValueAsString(ClassId);
        if (!Definition) continue;
        for (const FName Id : UnlockTestRegisteredIds(ClassId))
        {
            const bool bReachable = Definition->StarterAbilityIds.Contains(Id)
                || Definition->UnlockableAbilityIds.Contains(Id)
                || Definition->BaseUltimateId == Id;
            TestTrue(*FString::Printf(TEXT("%s: %s is a starter, the ultimate, or unlockable"),
                *Context, *Id.ToString()), bReachable);
        }
    }
    return true;
}

// (c) The authored starter shape per class, and it is what a fresh character
// holds. This was "exactly two starters" universally until ORDERS ruling 1:
// Swift is a ONE-starter class — Skim plus the enhanced-dash tree node, with
// ClassAbilityTwo shipping EMPTY until the first quartermaster unlock. The
// empty slot is the feature, so it is asserted as EMPTY rather than skipped:
// a default leaking back into slot two would silently retire the ruling.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAbilityStarterPairTest,
    "RiorsEdge.Abilities.StarterPair",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAbilityStarterPairTest::RunTest(const FString& Parameters)
{
    using namespace BreakerUnlockTest;
    for (const EBreakerClassId ClassId : UnlockTestAllClasses())
    {
        const UBreakerClassDefinition* Definition = UBreakerProgressionLibrary::GetFallbackClassDefinition(ClassId);
        const FString Context = UEnum::GetValueAsString(ClassId);
        if (!Definition) continue;
        const bool bOneStarterClass = ClassId == EBreakerClassId::Swift;
        TestEqual(*(Context + TEXT(" has its authored starter count")),
            Definition->StarterAbilityIds.Num(), bOneStarterClass ? 1 : 2);

        UBreakerProgressionComponent* Progression = UnlockTestMakeAt(ClassId, 1);
        const FBreakerAbilityLoadout& Loadout = Progression->GetProgressionState().AbilityLoadout;
        TestEqual(*(Context + TEXT(" seeds slot one from its first starter")), Loadout.ClassAbilityOne, Definition->StarterAbilityIds[0]);
        if (bOneStarterClass)
        {
            TestEqual(*(Context + TEXT(" seeds slot two EMPTY (ruling 1: the first token fills it)")),
                Loadout.ClassAbilityTwo, FName(NAME_None));
        }
        else
        {
            TestEqual(*(Context + TEXT(" seeds slot two from its second starter")), Loadout.ClassAbilityTwo, Definition->StarterAbilityIds[1]);
        }
        TestEqual(*(Context + TEXT(" seeds the ultimate")), Loadout.Ultimate, Definition->BaseUltimateId);
        // Free at level one means free: every starter and the ultimate answer
        // unlocked with nothing bought.
        for (const FName Starter : Definition->StarterAbilityIds)
        {
            TestTrue(*(Context + TEXT(" starter is free: ") + Starter.ToString()), Progression->IsAbilityUnlocked(Starter));
        }
        TestTrue(*(Context + TEXT(" the ultimate is free")), Progression->IsAbilityUnlocked(Definition->BaseUltimateId));
        for (const FName Id : Definition->UnlockableAbilityIds)
        {
            TestFalse(*(Context + TEXT(" unlockable starts locked: ") + Id.ToString()), Progression->IsAbilityUnlocked(Id));
        }
    }
    return true;
}

// (d) A token that cannot buy anything is a counter the player watches and
// cannot use. Swift is why this exists: a flat four-token schedule would pay it
// four against one unlockable and strand three on a shipped class.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAbilityNoUnspendableTokensTest,
    "RiorsEdge.Progression.AbilityUnlocks.NoUnspendableTokens",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAbilityNoUnspendableTokensTest::RunTest(const FString& Parameters)
{
    using namespace BreakerUnlockTest;
    for (const EBreakerClassId ClassId : UnlockTestAllClasses())
    {
        const UBreakerClassDefinition* Definition = UBreakerProgressionLibrary::GetFallbackClassDefinition(ClassId);
        const FString Context = UEnum::GetValueAsString(ClassId);
        if (!Definition) continue;
        const int32 Unlockables = Definition->UnlockableAbilityIds.Num();
        // Every level in the range, not just the cap: a schedule that overpaid
        // at level 12 and corrected itself by 50 would still have shown the
        // player a token they could not spend.
        for (int32 Level = 1; Level <= UBreakerExperienceLibrary::MaxCharacterLevel; ++Level)
        {
            const int32 Earned = UBreakerProgressionLibrary::AbilityTokenEntitlement(Level, Unlockables);
            TestTrue(*FString::Printf(TEXT("%s at level %d is never paid more tokens (%d) than it has unlockables (%d)"),
                *Context, Level, Earned, Unlockables), Earned <= Unlockables);
        }
    }
    return true;
}

// (d1b) The probe's dev equip (Part One-U item 16): unlock deliberately
// bypassed, every impossibility still refused — the probe photographs
// loadouts a levelled character COULD hold, never ones nobody could.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerDevForceEquipTest,
    "RiorsEdge.Progression.AbilityUnlocks.DevForceEquip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerDevForceEquipTest::RunTest(const FString& Parameters)
{
    using namespace BreakerUnlockTest;
    UBreakerProgressionComponent* Progression = UnlockTestMakeAt(EBreakerClassId::Swift, 1);

    // An un-unlocked unlockable equips through the dev surface — the whole
    // point — while the real path still refuses it.
    FText Failure;
    TestFalse(TEXT("the real path still refuses an un-unlocked id"),
        Progression->EquipAbility(EBreakerAbilitySlot::ClassAbilityTwo, TEXT("Swift.Slipcut"), Failure));
    Progression->DevForceEquipAbility(EBreakerAbilitySlot::ClassAbilityTwo, TEXT("Swift.Slipcut"));
    TestEqual(TEXT("the dev surface equips it"),
        Progression->GetProgressionState().AbilityLoadout.ClassAbilityTwo, FName(TEXT("Swift.Slipcut")));

    // Impossibilities still refuse: a foreign-class id, and a non-ultimate in
    // the ultimate slot.
    Progression->DevForceEquipAbility(EBreakerAbilitySlot::ClassAbilityOne, TEXT("Caster.Rot"));
    TestEqual(TEXT("a foreign-class id is refused even by the dev surface"),
        Progression->GetProgressionState().AbilityLoadout.ClassAbilityOne, FName(TEXT("Swift.Skim")));
    Progression->DevForceEquipAbility(EBreakerAbilitySlot::Ultimate, TEXT("Swift.Lead"));
    TestEqual(TEXT("a non-ultimate is refused from the ultimate slot"),
        Progression->GetProgressionState().AbilityLoadout.Ultimate, FName(TEXT("Swift.Overdrive")));
    return true;
}

// (d2) The derived schedule (O138). The count is read from the thing it
// counts, so the four-against-five disagreement that stranded Swift's last
// ability cannot recur — and the derivation must not silently retune the four
// classes whose schedule was already authored by feel.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAbilityTokenScheduleDerivesTest,
    "RiorsEdge.Progression.AbilityUnlocks.ScheduleDerives",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAbilityTokenScheduleDerivesTest::RunTest(const FString& Parameters)
{
    using L = UBreakerProgressionLibrary;

    // BIT-IDENTITY with the retired authored list for the four-unlockable
    // classes: {5, 12, 20, 30}. The convex exponent was chosen BECAUSE it
    // reproduces the authored feel exactly (gaps 7, 8, 10); if a retune moves
    // the exponent, this is the pin that makes the four-class consequence a
    // decision instead of a side effect.
    const int32 FourClass[] = {5, 12, 20, 30};
    for (int32 Index = 0; Index < 4; ++Index)
    {
        TestEqual(*FString::Printf(TEXT("a four-unlockable class's token %d arrives at the authored level"), Index + 1),
            L::AbilityTokenLevelForIndex(Index, 4), FourClass[Index]);
    }

    // Swift's five: first two close, last two apart, completion shared.
    const int32 FiveClass[] = {5, 10, 16, 23, 30};
    for (int32 Index = 0; Index < 5; ++Index)
    {
        TestEqual(*FString::Printf(TEXT("a five-unlockable class's token %d arrives at the derived level"), Index + 1),
            L::AbilityTokenLevelForIndex(Index, 5), FiveClass[Index]);
    }

    // The ends are the two authored numbers, at any count — the derivation's
    // whole contract, and the owner's ruling ("all classes should finish at
    // the same time") as arithmetic.
    for (int32 Count = 2; Count <= 8; ++Count)
    {
        TestEqual(*FString::Printf(TEXT("a %d-token schedule starts at the first-token level"), Count),
            L::AbilityTokenLevelForIndex(0, Count), L::FirstAbilityTokenLevel);
        TestEqual(*FString::Printf(TEXT("a %d-token schedule completes at the shared completion level"), Count),
            L::AbilityTokenLevelForIndex(Count - 1, Count), L::AbilityCompletionLevel);
        // Strictly increasing: two tokens on one level would be one milestone
        // wearing two names.
        for (int32 Index = 1; Index < Count; ++Index)
        {
            TestTrue(*FString::Printf(TEXT("a %d-token schedule strictly increases at token %d"), Count, Index + 1),
                L::AbilityTokenLevelForIndex(Index, Count) > L::AbilityTokenLevelForIndex(Index - 1, Count));
        }
    }

    // Every SHIPPED class completes exactly at the completion level — the
    // ruling asserted against the real definitions, not just the arithmetic.
    using namespace BreakerUnlockTest;
    for (const EBreakerClassId ClassId : UnlockTestAllClasses())
    {
        const UBreakerClassDefinition* Definition = UBreakerProgressionLibrary::GetFallbackClassDefinition(ClassId);
        if (!Definition || Definition->UnlockableAbilityIds.Num() == 0) continue;
        const int32 Count = Definition->UnlockableAbilityIds.Num();
        TestEqual(*FString::Printf(TEXT("%s completes its kit at the shared level"), *UEnum::GetValueAsString(ClassId)),
            L::AbilityTokenLevelForIndex(Count - 1, Count), L::AbilityCompletionLevel);
    }
    return true;
}

// (e) Reachability at the shipped entitlement, with no test grant anywhere.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAbilityReachableByFiftyTest,
    "RiorsEdge.Progression.AbilityUnlocks.ReachableByFifty",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAbilityReachableByFiftyTest::RunTest(const FString& Parameters)
{
    using namespace BreakerUnlockTest;
    for (const EBreakerClassId ClassId : UnlockTestAllClasses())
    {
        const UBreakerClassDefinition* Definition = UBreakerProgressionLibrary::GetFallbackClassDefinition(ClassId);
        const FString Context = UEnum::GetValueAsString(ClassId);
        if (!Definition) continue;

        UBreakerProgressionComponent* Progression = UnlockTestMakeAt(ClassId, UBreakerExperienceLibrary::MaxCharacterLevel);
        for (const FName Id : Definition->UnlockableAbilityIds)
        {
            FText Failure;
            TestTrue(*FString::Printf(TEXT("%s: %s is buyable by level 50"), *Context, *Id.ToString()),
                Progression->SpendAbilityToken(Id, Failure));
        }
        // Everything the class offers, reachable, on one character.
        for (const FName Id : UnlockTestRegisteredIds(ClassId))
        {
            TestTrue(*FString::Printf(TEXT("%s: %s is unlocked at 50"), *Context, *Id.ToString()),
                Progression->IsAbilityUnlocked(Id));
        }
        TestEqual(*(Context + TEXT(" has spent every token it was paid")), Progression->GetUnspentAbilityTokens(), 0);
    }
    return true;
}

// (f) Refusals, and that a refused spend costs nothing — the same rule as a
// refused craft, and the same failure class: one refusal that debits becomes a
// lost-currency report nobody can reproduce.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAbilitySpendRefusalsTest,
    "RiorsEdge.Progression.AbilityUnlocks.SpendRefusals",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAbilitySpendRefusalsTest::RunTest(const FString& Parameters)
{
    using namespace BreakerUnlockTest;
    const UBreakerClassDefinition* Caster = UBreakerProgressionLibrary::GetFallbackClassDefinition(EBreakerClassId::Caster);
    if (!Caster || Caster->UnlockableAbilityIds.Num() == 0) { AddError(TEXT("No Caster unlockables")); return false; }
    const FName First = Caster->UnlockableAbilityIds[0];

    // Zero tokens: a level-1 character has earned none.
    {
        UBreakerProgressionComponent* Fresh = UnlockTestMakeAt(EBreakerClassId::Caster, 1);
        TestEqual(TEXT("A level-1 character holds no tokens"), Fresh->GetUnspentAbilityTokens(), 0);
        FText Failure;
        TestFalse(TEXT("A spend with no tokens is refused"), Fresh->SpendAbilityToken(First, Failure));
        TestFalse(TEXT("The refusal says why"), Failure.IsEmpty());
        TestFalse(TEXT("And nothing was unlocked"), Fresh->IsAbilityUnlocked(First));
    }

    UBreakerProgressionComponent* Progression = UnlockTestMakeAt(EBreakerClassId::Caster, UBreakerExperienceLibrary::MaxCharacterLevel);
    const int32 Funded = Progression->GetUnspentAbilityTokens();
    TestTrue(TEXT("A level-50 Caster has tokens to spend"), Funded > 0);

    // An id this class does not sell: a starter, the ultimate, another class's
    // ability, and an id that does not exist at all.
    const TArray<FName> Rejected = {
        Caster->StarterAbilityIds[0], Caster->BaseUltimateId,
        FName(TEXT("Swift.CadenceBreak")), FName(TEXT("Not.An.Ability")) };
    for (const FName Id : Rejected)
    {
        FText Failure;
        TestFalse(*FString::Printf(TEXT("%s is not for sale here"), *Id.ToString()),
            Progression->SpendAbilityToken(Id, Failure));
        TestEqual(*FString::Printf(TEXT("A refused spend of %s cost nothing"), *Id.ToString()),
            Progression->GetUnspentAbilityTokens(), Funded);
    }

    // A real purchase, then the same purchase again.
    FText Failure;
    TestTrue(TEXT("The first purchase succeeds"), Progression->SpendAbilityToken(First, Failure));
    TestEqual(TEXT("It cost exactly one token"), Progression->GetUnspentAbilityTokens(), Funded - 1);
    const int32 AfterBuy = Progression->GetUnspentAbilityTokens();
    TestFalse(TEXT("Buying it twice is refused"), Progression->SpendAbilityToken(First, Failure));
    TestEqual(TEXT("And the second attempt cost nothing"), Progression->GetUnspentAbilityTokens(), AfterBuy);
    return true;
}

// (g) A Forge respec refunds everything in reach — ranks, points, the loadout,
// the commitment. Unlocks are the exception, and an exception nobody stated is
// an exception nobody keeps.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAbilitySurvivesRespecTest,
    "RiorsEdge.Progression.AbilityUnlocks.SurvivesRespec",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAbilitySurvivesRespecTest::RunTest(const FString& Parameters)
{
    using namespace BreakerUnlockTest;
    const UBreakerClassDefinition* Caster = UBreakerProgressionLibrary::GetFallbackClassDefinition(EBreakerClassId::Caster);
    if (!Caster || Caster->UnlockableAbilityIds.Num() == 0) { AddError(TEXT("No Caster unlockables")); return false; }

    UBreakerProgressionComponent* Progression = UnlockTestMakeAt(EBreakerClassId::Caster, UBreakerExperienceLibrary::MaxCharacterLevel);
    const FName Bought = Caster->UnlockableAbilityIds[0];
    FText Failure;
    TestTrue(TEXT("Bought an ability"), Progression->SpendAbilityToken(Bought, Failure));
    const int32 TokensAfterBuy = Progression->GetUnspentAbilityTokens();

    TestTrue(TEXT("Respec succeeds"), Progression->RespecAtForge(EBreakerPointCurrency::DoctrinePoints, true, Failure));

    TestTrue(TEXT("The unlock survives a Forge respec"), Progression->IsAbilityUnlocked(Bought));
    TestEqual(TEXT("The token count survives a Forge respec"), Progression->GetUnspentAbilityTokens(), TokensAfterBuy);
    // And the spent token is not handed back: a refund that returned the token
    // while taking the ability would cost the player the difference.
    TestFalse(TEXT("The spent token is not refunded"), Progression->SpendAbilityToken(Bought, Failure));
    return true;
}

// (h) Round-trip through a load.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAbilitySaveLoadTest,
    "RiorsEdge.Progression.AbilityUnlocks.SurviveSaveLoad",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAbilitySaveLoadTest::RunTest(const FString& Parameters)
{
    using namespace BreakerUnlockTest;
    const UBreakerClassDefinition* Caster = UBreakerProgressionLibrary::GetFallbackClassDefinition(EBreakerClassId::Caster);
    if (!Caster || Caster->UnlockableAbilityIds.Num() == 0) { AddError(TEXT("No Caster unlockables")); return false; }

    UBreakerProgressionComponent* Source = UnlockTestMakeAt(EBreakerClassId::Caster, UBreakerExperienceLibrary::MaxCharacterLevel);
    FText Failure;
    Source->SpendAbilityToken(Caster->UnlockableAbilityIds[0], Failure);
    const FBreakerProgressionState Saved = Source->GetProgressionState();

    UBreakerProgressionComponent* Loaded = NewObject<UBreakerProgressionComponent>();
    Loaded->LoadProgressionState(Saved);
    TestEqual(TEXT("The unlocked set round-trips"),
        Loaded->GetProgressionState().UnlockedAbilityIds.Num(), Saved.UnlockedAbilityIds.Num());
    TestTrue(TEXT("The bought ability is still unlocked after a load"),
        Loaded->IsAbilityUnlocked(Caster->UnlockableAbilityIds[0]));
    TestEqual(TEXT("Unspent tokens round-trip"), Loaded->GetUnspentAbilityTokens(), Saved.UnspentAbilityTokens);
    TestEqual(TEXT("The granted counter round-trips"),
        Loaded->GetProgressionState().AbilityTokensGranted, Saved.AbilityTokensGranted);
    return true;
}

// (h2) THE STORY TOKEN GRANT — abilities unlock from story missions.
// Mechanism only: the mission layer wires beats to it in a later wave, so
// nothing here fabricates a beat — the fixture sets a SHIPPED registered flag
// the way the shipped dialogue choice would, then observes it. The level
// schedule stands beside this untouched (the fallback until the owner rules),
// which is why the fixture runs at level 1: the shipped entitlement there is
// ZERO tokens, so every token in the test is the story's and the reachability
// claim — a story beat can unlock an ability before the first level milestone
// — is proved with nothing granted that the game does not grant.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerStoryTokenGrantTest,
    "RiorsEdge.Progression.AbilityUnlocks.StoryTokenGrant",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerStoryTokenGrantTest::RunTest(const FString& Parameters)
{
    using namespace BreakerUnlockTest;
    const UBreakerClassDefinition* Swift = UBreakerProgressionLibrary::GetFallbackClassDefinition(EBreakerClassId::Swift);
    if (!Swift || Swift->UnlockableAbilityIds.Num() == 0) { AddError(TEXT("No Swift unlockables")); return false; }

    UBreakerProgressionComponent* Progression = UnlockTestMakeAt(EBreakerClassId::Swift, 1);
    UBreakerQuestJournal* Journal = NewObject<UBreakerQuestJournal>();
    TestEqual(TEXT("A level-1 character holds no level tokens"), Progression->GetUnspentAbilityTokens(), 0);

    const FName StoryFlag = BreakerQuestFlags::FirstContractTurnedIn;

    // The grant OBSERVES — a flag nobody has set pays nothing.
    TestFalse(TEXT("An unset story flag is refused"), Progression->GrantStoryAbilityToken(StoryFlag, Journal));
    TestEqual(TEXT("And it paid nothing"), Progression->GetUnspentAbilityTokens(), 0);

    // The beat sets the flag (the shipped dialogue choice's own act), then the
    // grant observes it.
    Journal->SetFlag(StoryFlag);
    TestTrue(TEXT("A set, registered story flag grants once"),
        Progression->GrantStoryAbilityToken(StoryFlag, Journal));
    TestEqual(TEXT("Exactly one token"), Progression->GetUnspentAbilityTokens(), 1);
    TestTrue(TEXT("The payment is stamped in the flag set"),
        Journal->HasFlag(UBreakerProgressionComponent::StoryTokenPaidFlagFor(StoryFlag)));

    // Monotonic: observed once, granted once, never revoked.
    TestFalse(TEXT("The same flag does not pay twice"), Progression->GrantStoryAbilityToken(StoryFlag, Journal));
    TestEqual(TEXT("And the second attempt moved nothing"), Progression->GetUnspentAbilityTokens(), 1);

    // Registry flags only: a name the campaign never sets is refused even when
    // someone has managed to write it into the journal.
    const FName Unregistered = TEXT("Not.A.Registered.Flag");
    Journal->SetFlag(Unregistered);
    TestFalse(TEXT("An unregistered flag is refused"), Progression->GrantStoryAbilityToken(Unregistered, Journal));
    TestEqual(TEXT("And it minted nothing"), Progression->GetUnspentAbilityTokens(), 1);
    TestFalse(TEXT("A null journal is refused"), Progression->GrantStoryAbilityToken(StoryFlag, nullptr));

    // The round trip: both halves of the payment persist together (the stamp
    // in the journal state, the token in the progression state), so a reload
    // neither double-grants nor loses the token.
    const FBreakerProgressionState SavedProgression = Progression->GetProgressionState();
    const FBreakerQuestFlagSet SavedFlags = Journal->GetState();

    UBreakerProgressionComponent* Loaded = NewObject<UBreakerProgressionComponent>();
    Loaded->LoadProgressionState(SavedProgression);
    UBreakerQuestJournal* LoadedJournal = NewObject<UBreakerQuestJournal>();
    LoadedJournal->RestoreFrom(SavedFlags);
    TestEqual(TEXT("The story token survives a load"), Loaded->GetUnspentAbilityTokens(), 1);
    TestFalse(TEXT("A reload does not pay the same flag again"),
        Loaded->GrantStoryAbilityToken(StoryFlag, LoadedJournal));
    TestEqual(TEXT("And the reloaded wallet holds exactly one"), Loaded->GetUnspentAbilityTokens(), 1);

    // The token is the SAME currency the level schedule pays: it buys a real
    // unlockable through the one spend path, at a level where the shipped
    // schedule has paid nothing — the story route reaches an ability first.
    FText Failure;
    const FName First = Swift->UnlockableAbilityIds[0];
    TestTrue(TEXT("The story token buys an unlockable at level 1"), Loaded->SpendAbilityToken(First, Failure));
    TestTrue(TEXT("And the ability is unlocked"), Loaded->IsAbilityUnlocked(First));
    TestEqual(TEXT("The wallet is spent to zero"), Loaded->GetUnspentAbilityTokens(), 0);
    return true;
}

// (i) The migration, in isolation, and as a chain.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerSaveMigrationV4ToV5Test,
    "RiorsEdge.Save.Migration.V4ToV5",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerSaveMigrationV4ToV5Test::RunTest(const FString& Parameters)
{
    // A faithful v4 payload: a level-30 Caster who, before O100, could reach
    // its whole kit for free and therefore recorded no unlocks at all.
    UBreakerSaveGame* Old = NewObject<UBreakerSaveGame>();
    Old->SaveVersion = 4;
    Old->Progression.PermanentClass = EBreakerClassId::Caster;
    Old->Progression.CharacterLevel = 30;

    FString Note;
    TestTrue(TEXT("A v4 save migrates"), UBreakerSaveGame::MigrateToCurrent(*Old, Note));
    TestEqual(TEXT("It arrives at the head version"), Old->SaveVersion, UBreakerSaveGame::CurrentSaveVersion);

    // Everything it could previously reach, still reachable. An empty array
    // would be valid data that now means "this character lost its kit".
    TestEqual(TEXT("A legacy Caster keeps its six class abilities"), Old->Progression.UnlockedAbilityIds.Num(), 6);
    TestTrue(TEXT("A non-starter it had is still unlocked"),
        Old->Progression.UnlockedAbilityIds.Contains(FName(TEXT("Caster.Resonance"))));

    // And it is not paid retroactively for a kit it already owns.
    TestEqual(TEXT("The granted counter is stamped to its level"), Old->Progression.AbilityTokensGranted, 4);

    // Swift's documented inexactness, asserted rather than left to be noticed:
    // CadenceBreak was never previously available, and is granted anyway
    // because granting is not the failure this step guards against.
    UBreakerSaveGame* LegacySwift = NewObject<UBreakerSaveGame>();
    LegacySwift->SaveVersion = 4;
    LegacySwift->Progression.PermanentClass = EBreakerClassId::Swift;
    LegacySwift->Progression.CharacterLevel = 30;
    TestTrue(TEXT("A v4 Swift save migrates"), UBreakerSaveGame::MigrateToCurrent(*LegacySwift, Note));
    TestTrue(TEXT("A legacy Swift gains CadenceBreak, deliberately"),
        LegacySwift->Progression.UnlockedAbilityIds.Contains(FName(TEXT("Swift.CadenceBreak"))));
    TestEqual(TEXT("A legacy Swift's counter is stamped to its one unlockable"),
        LegacySwift->Progression.AbilityTokensGranted, 1);

    // THE CHAIN. A v3 save must arrive at v5 through the steps in order, not by
    // a shortcut — the wallet collapse and the unlock seed both have to happen.
    UBreakerSaveGame* V3 = NewObject<UBreakerSaveGame>();
    V3->SaveVersion = 3;
    V3->Progression.PermanentClass = EBreakerClassId::Tank;
    V3->Progression.CharacterLevel = 20;
    V3->ForgeWallet.Amounts = { 42, 7, 1 };
    TestTrue(TEXT("A v3 save migrates the whole way"), UBreakerSaveGame::MigrateToCurrent(*V3, Note));
    TestEqual(TEXT("It arrives at the head version"), V3->SaveVersion, UBreakerSaveGame::CurrentSaveVersion);
    TestTrue(TEXT("The v3 to v4 wallet step still ran"), V3->ForgeWallet.Get() > 0);
    TestEqual(TEXT("The v4 to v5 unlock step also ran"), V3->Progression.UnlockedAbilityIds.Num(), 6);

    // IDEMPOTENCE. A v5 save is left exactly as it is; a migration that ran
    // again on an already-migrated file would re-seed a character who has since
    // respecced or spent, which is the quiet way a migration corrupts.
    UBreakerSaveGame* Current = NewObject<UBreakerSaveGame>();
    Current->SaveVersion = UBreakerSaveGame::CurrentSaveVersion;
    Current->Progression.PermanentClass = EBreakerClassId::Caster;
    Current->Progression.CharacterLevel = 30;
    Current->Progression.AbilityTokensGranted = 4;
    TestTrue(TEXT("A current save passes through"), UBreakerSaveGame::MigrateToCurrent(*Current, Note));
    TestEqual(TEXT("A v5 save gains no unlocks"), Current->Progression.UnlockedAbilityIds.Num(), 0);
    TestEqual(TEXT("A v5 save's counter is untouched"), Current->Progression.AbilityTokensGranted, 4);
    return true;
}

#endif
