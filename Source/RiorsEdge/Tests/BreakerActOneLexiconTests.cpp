#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Interaction/BreakerNPC.h"
#include "Save/BreakerQuestContent.h"

// THE ACT I LEXICON GUARD.
//
// Campaign-And-Story.md locks the terminology: everything the player fights in
// Act I is a VESTIGE ("spill" is field slang for the same thing), and the word
// "Altered" must not appear ANYWHERE in Act I content — dialogue, quest text,
// lore, props — because the Act II turn (A2-1) IS the first time the player
// meets the word's referent, and a single stray mention spends the campaign's
// most carefully guarded beat for nothing. "Bastion" is not this game's word
// for a settlement (it is "Anchor"), and "Aberrant"/"Anomalous" are ITEM
// RARITIES, never enemy words — an enemy called Anomalous would also violate
// the teal object law by attaching the reserved band's vocabulary to a
// combatant.
//
// This test sweeps every player-visible string in the quest registry and both
// hub NPCs' dialogue factories. It exists so the protection outlives every
// author: the day someone types the word three quests from now, the suite goes
// red instead of Act II going flat.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerActOneLexiconTest,
    "RiorsEdge.Save.ActOneLexicon",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerActOneLexiconTest::RunTest(const FString& Parameters)
{
    // Case-insensitive on purpose: "altered" as a common verb is still the
    // word, and the register is clipped enough that no Act I line needs it.
    const TCHAR* Banned[] = { TEXT("altered"), TEXT("bastion"), TEXT("aberrant"), TEXT("anomalous") };

    // (source description, text) pairs so a failure names the exact string.
    TArray<TPair<FString, FString>> Strings;

    for (const FBreakerQuestDefinition& Quest : UBreakerQuestLibrary::GetFallbackQuests())
    {
        const FString Id = Quest.QuestId.ToString();
        Strings.Emplace(FString::Printf(TEXT("%s title"), *Id), Quest.Title);
        Strings.Emplace(FString::Printf(TEXT("%s giver"), *Id), Quest.Giver);
        for (const FBreakerQuestObjective& Objective : Quest.Objectives)
        {
            Strings.Emplace(FString::Printf(TEXT("%s objective %s"), *Id, *Objective.ObjectiveId.ToString()), Objective.Text);
        }
    }

    const auto GatherDialogue = [&Strings](const TCHAR* Speaker, const TArray<FBreakerDialogueNode>& Nodes)
    {
        for (const FBreakerDialogueNode& Node : Nodes)
        {
            Strings.Emplace(FString::Printf(TEXT("%s node %s line"), Speaker, *Node.NodeId.ToString()), Node.SpeakerLine);
            for (const FBreakerDialogueChoice& Choice : Node.Choices)
            {
                Strings.Emplace(FString::Printf(TEXT("%s node %s choice"), Speaker, *Node.NodeId.ToString()), Choice.Text);
            }
        }
    };
    GatherDialogue(TEXT("Kess"), ABreakerNPC::MakeForgeKeeperDialogue());
    GatherDialogue(TEXT("Quartermaster"), ABreakerNPC::MakeQuartermasterDialogue());

    TestTrue(TEXT("There are strings to sweep"), Strings.Num() > 0);
    for (const TPair<FString, FString>& Entry : Strings)
    {
        for (const TCHAR* Word : Banned)
        {
            if (Entry.Value.Contains(Word, ESearchCase::IgnoreCase))
            {
                AddError(FString::Printf(
                    TEXT("Act I lexicon violation: %s contains '%s' — \"%s\". VESTIGE/spill are the Act I words; this one is reserved."),
                    *Entry.Key, Word, *Entry.Value));
            }
        }
    }
    return true;
}

#endif
