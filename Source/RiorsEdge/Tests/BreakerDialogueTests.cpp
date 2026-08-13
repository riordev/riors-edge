#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Interaction/BreakerNPC.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerDialogueIntegrityTest,
    "RiorsEdge.Interaction.DialogueIntegrity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerDialogueIntegrityTest::RunTest(const FString& Parameters)
{
    // The fallback conversations must have a resolvable start node, at
    // least one exit per node, and no dangling links. Validate on plain
    // NewObject instances so no world is needed.
    ABreakerNPC* Kess = NewObject<ABreakerNPC>();
    Kess->DisplayName = FText::FromString(TEXT("Test Kess"));
    Kess->DialogueNodes =
    {
        [] { FBreakerDialogueNode Node; Node.NodeId = TEXT("Start"); Node.SpeakerLine = TEXT("Hello.");
             FBreakerDialogueChoice Go; Go.Text = TEXT("Continue"); Go.NextNodeId = TEXT("End");
             FBreakerDialogueChoice Leave; Leave.Text = TEXT("[Leave]");
             Node.Choices = { Go, Leave }; return Node; }(),
        [] { FBreakerDialogueNode Node; Node.NodeId = TEXT("End"); Node.SpeakerLine = TEXT("Goodbye.");
             FBreakerDialogueChoice Flagged; Flagged.Text = TEXT("[Leave]"); Flagged.SetsQuestFlag = TEXT("Quest.Test");
             Node.Choices = { Flagged }; return Node; }(),
    };
    FString Error;
    TestTrue(FString::Printf(TEXT("Well-formed dialogue validates (%s)"), *Error), Kess->ValidateDialogue(Error));

    FBreakerDialogueNode Found;
    TestTrue(TEXT("Start node resolves"), Kess->FindDialogueNode(TEXT("Start"), Found));
    TestEqual(TEXT("Node carries its line"), Found.SpeakerLine, FString(TEXT("Hello.")));
    TestEqual(TEXT("Choice links forward"), Found.Choices[0].NextNodeId, FName(TEXT("End")));

    // A dangling link must fail validation.
    ABreakerNPC* Broken = NewObject<ABreakerNPC>();
    Broken->DialogueNodes =
    {
        [] { FBreakerDialogueNode Node; Node.NodeId = TEXT("Start"); Node.SpeakerLine = TEXT("Hm.");
             FBreakerDialogueChoice Bad; Bad.Text = TEXT("Go"); Bad.NextNodeId = TEXT("Missing");
             Node.Choices = { Bad }; return Node; }(),
    };
    TestFalse(TEXT("Dangling link fails validation"), Broken->ValidateDialogue(Error));
    return true;
}

#endif
