#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Progression/BreakerProgressionTypes.h"
#include "Save/BreakerMissionContent.h"
#include "Save/BreakerQuestJournal.h"
#include "UI/BreakerAbilityCostLine.h"
#include "UI/BreakerDoctrineWallet.h"

// ---------------------------------------------------------------------------
// UI.AbilityCostLine: what an ability costs, as the screen says it.
//
// Owner, playtest 2026-09-10: "spell costs actually appearing in the skill
// menu". The numbers were authored, loaded and drawn nowhere. These pin the
// wording, and then pin the shipped abilities against it — a formatter that is
// correct over invented inputs and blank over the real ones would pass the
// first half of this file and fail the player.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAbilityCostLineTest,
    "RiorsEdge.UI.AbilityCostLine",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAbilityCostLineTest::RunTest(const FString& Parameters)
{
    using namespace BreakerAbilityCost;

    // --- THE WORDING -------------------------------------------------------
    {
        const FCostLine Line = Compose(35.0f, 0.0f, 0.0f, TEXT("MANA"));
        TestEqual(TEXT("A price names its pool"), Line.CostText, FString(TEXT("35 MANA")));
        // THE RULED DISTINCTION (Class-Kits: "Mana IS the cooldown. An empty
        // cooldown means cost-gated, and the HUD must distinguish that from a
        // cooldown of zero"). "0S" would say the opposite of what it means.
        TestEqual(TEXT("No cooldown and a price is cost-gated"), Line.GateText, FString(TEXT("COST GATED")));
        TestTrue(TEXT("No wind-up prints no cast term"), Line.CastText.IsEmpty());
    }
    {
        const FCostLine Line = Compose(0.0f, 12.0f, 0.0f, TEXT("GRIT"));
        TestTrue(TEXT("A free ability prints no price"), Line.CostText.IsEmpty());
        TestEqual(TEXT("A cooldown prints in whole seconds"), Line.GateText, FString(TEXT("12S COOLDOWN")));
    }
    {
        // NOTHING GATES IT AT ALL, and that is worth saying out loud rather
        // than leaving as the blank space it used to be.
        const FCostLine Line = Compose(0.0f, 0.0f, 0.0f, TEXT("MOMENTUM"));
        TestEqual(TEXT("No price and no cooldown says so"), Line.GateText, FString(TEXT("NO COST")));
        TestTrue(TEXT("A free ability with no gate still prints no price"), Line.CostText.IsEmpty());
    }
    {
        // O266's wind-up, and a fraction of a second is a real difference that
        // rounding to whole seconds would erase.
        const FCostLine Line = Compose(40.0f, 8.0f, 0.6f, TEXT("MANA"));
        TestEqual(TEXT("A wind-up keeps its decimal"), Line.CastText, FString(TEXT("0.6S CAST")));
        TestEqual(TEXT("Three runs flatten in reading order"), Flatten(Line),
            FString(TEXT("40 MANA  8S COOLDOWN  0.6S CAST")));
    }
    {
        // A definition with no class prints its number rather than guessing at
        // a pool name.
        const FCostLine Line = Compose(25.0f, 0.0f, 0.0f, FString());
        TestEqual(TEXT("A classless price is still a price"), Line.CostText, FString(TEXT("25")));
    }

    // Every class's pool has a word, and no two classes share one, or the row
    // would name the wrong pool for four of the five.
    {
        const EBreakerClassId Classes[] = { EBreakerClassId::Swift, EBreakerClassId::Caster,
            EBreakerClassId::Gunsmith, EBreakerClassId::Tank, EBreakerClassId::Support };
        TSet<FString> Words;
        for (const EBreakerClassId ClassId : Classes)
        {
            const FString Word = ResourceWordFor(ClassId);
            TestFalse(TEXT("Every playable class names its pool"), Word.IsEmpty());
            Words.Add(Word);
        }
        TestEqual(TEXT("No two classes share a pool word"), Words.Num(), static_cast<int32>(UE_ARRAY_COUNT(Classes)));
        TestTrue(TEXT("A classless definition has no pool word"), ResourceWordFor(EBreakerClassId::None).IsEmpty());
    }

    // --- THE SHIPPED ABILITIES --------------------------------------------
    // The catalogue draws whatever the registry holds, so the question that
    // matters is whether the registry gives it anything to draw. Walked over
    // the real fallback registry, both class-ability slots and the ultimate.
    {
        const EBreakerAbilitySlot Slots[] = { EBreakerAbilitySlot::ClassAbilityOne,
            EBreakerAbilitySlot::ClassAbilityTwo, EBreakerAbilitySlot::Ultimate };
        int32 Rows = 0;
        int32 Priced = 0;
        for (const EBreakerAbilitySlot Slot : Slots)
        {
            for (const UBreakerAbilityDefinition* Definition
                : UBreakerAbilityDefinition::GetClassAbilities(EBreakerClassId::Caster, Slot))
            {
                if (!Definition) continue;
                ++Rows;
                const FCostLine Line = Compose(Definition->ResourceCost, Definition->CooldownSeconds,
                    Definition->CastTimeSeconds, ResourceWordFor(EBreakerClassId::Caster));
                // NEVER A BLANK ROW. Whatever an ability is, the row says
                // something about what holds it back.
                TestFalse(*FString::Printf(TEXT("%s states a gate"), *Definition->AbilityId.ToString()),
                    Line.GateText.IsEmpty());
                if (Definition->ResourceCost > 0.0f)
                {
                    ++Priced;
                    TestTrue(*FString::Printf(TEXT("%s names Mana in its price"), *Definition->AbilityId.ToString()),
                        Line.CostText.Contains(ResourceWordFor(EBreakerClassId::Caster)));
                }
            }
        }
        TestTrue(TEXT("The Caster has a catalogue at all"), Rows > 0);
        // The Caster's kit is the one the cost line exists for: Mana IS its
        // cooldown, so a Caster row with no price would be a row with nothing
        // on it.
        TestTrue(TEXT("The Caster's abilities are priced"), Priced > 0);
    }
    return true;
}

// ---------------------------------------------------------------------------
// UI.DoctrineWallet: the header chip that should not greet a new character.
//
// Owner: "class points shouldnt be shown till a quests completion". The
// predicate is trivial; what is worth pinning is the DATA behind it — that a
// character who has completed nothing is owed nothing, so the chip really is
// absent at the start, and that the campaign really does pay before the end,
// so it is not absent forever.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerDoctrineWalletTest,
    "RiorsEdge.UI.DoctrineWallet",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerDoctrineWalletTest::RunTest(const FString& Parameters)
{
    TestFalse(TEXT("A character owed nothing has no wallet"), BreakerDoctrineWallet::IsOpen(0, 0, 0));
    TestTrue(TEXT("An unspent point opens it"), BreakerDoctrineWallet::IsOpen(1, 0, 0));
    // ONCE OPEN, IT STAYS OPEN: a spent-out wallet still reads zero rather
    // than vanishing, or the first refusal would have nothing to point at.
    TestTrue(TEXT("A spent point keeps it open"), BreakerDoctrineWallet::IsOpen(0, 8, 8));
    TestTrue(TEXT("A settled grant opens it before anything is spent"), BreakerDoctrineWallet::IsOpen(0, 0, 2));

    // The default-constructed state is what a new character carries.
    const FBreakerProgressionState Fresh;
    TestFalse(TEXT("A new character's wallet is closed"),
        BreakerDoctrineWallet::IsOpen(Fresh.UnspentDoctrinePoints, 0, Fresh.LevelDoctrinePointsGranted));

    // --- THE SHIPPED CAMPAIGN PAYS, AND NOT AT THE END ---------------------
    // Walked one beat at a time over the real missions: the entitlement must
    // start at nothing, never fall, reach the full grant, and cross zero well
    // before the last beat — a wallet that opened only on the finale would
    // satisfy the predicate and none of the intent.
    {
        FBreakerQuestFlagSet Flags;
        TestEqual(TEXT("An empty journal is owed nothing"),
            UBreakerMissionLibrary::DoctrinePointEntitlement(Flags), 0);

        int32 Beats = 0;
        int32 BeatsBeforeFirstPayment = -1;
        int32 Previous = 0;
        for (const FBreakerMissionDefinition& Mission : UBreakerMissionLibrary::GetMissions())
        {
            for (const FBreakerMissionBeat& Beat : Mission.Beats)
            {
                for (const FName Flag : UBreakerMissionLibrary::BeatCompletionFlags(Beat)) Flags.Add(Flag);
                ++Beats;
                const int32 Owed = UBreakerMissionLibrary::DoctrinePointEntitlement(Flags);
                if (!TestTrue(TEXT("The entitlement never falls as beats complete"), Owed >= Previous)) return false;
                if (Owed > 0 && BeatsBeforeFirstPayment < 0) BeatsBeforeFirstPayment = Beats;
                Previous = Owed;
            }
        }
        TestTrue(TEXT("The campaign has beats"), Beats > 0);
        TestTrue(TEXT("Something in the campaign pays a doctrine point"), BeatsBeforeFirstPayment > 0);
        TestTrue(TEXT("The wallet opens before the last beat"), BeatsBeforeFirstPayment < Beats);
        TestTrue(TEXT("A completed campaign is owed the full grant"), Previous > 0);
    }
    return true;
}

#endif   // WITH_DEV_AUTOMATION_TESTS
