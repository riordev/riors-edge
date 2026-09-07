#include "UI/BreakerMenu.h"

#include "Characters/BreakerCharacter.h"
#include "Interaction/BreakerNPC.h"
#include "Interaction/BreakerSurvivor.h"
#include "Save/BreakerQuestJournal.h"
#include "InputCoreTypes.h"

FReply SBreakerMenu::HandleDialogueChoiceKey(const FKeyEvent& KeyEvent)
{
    if (CurrentScreen != EBreakerMenuScreen::Dialogue) return FReply::Unhandled();
    static const FKey NumberKeys[] = { EKeys::One, EKeys::Two, EKeys::Three,
        EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
    static const FKey PadKeys[] = { EKeys::NumPadOne, EKeys::NumPadTwo, EKeys::NumPadThree,
        EKeys::NumPadFour, EKeys::NumPadFive, EKeys::NumPadSix, EKeys::NumPadSeven, EKeys::NumPadEight, EKeys::NumPadNine };
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(NumberKeys); ++Index)
    {
        if (KeyEvent.GetKey() == NumberKeys[Index] || KeyEvent.GetKey() == PadKeys[Index])
        {
            // Holding a choice must not choose again on the next dialogue node.
            return KeyEvent.IsRepeat() ? FReply::Handled() : SelectDialogueChoice(Index);
        }
    }
    return FReply::Unhandled();
}

FReply SBreakerMenu::SelectDialogueChoice(int32 ChoiceIndex)
{
    ABreakerNPC* NPC = DialogueNPC.Get();
    FBreakerDialogueNode Node;
    if (CurrentScreen != EBreakerMenuScreen::Dialogue || !Character.IsValid()
        || !NPC || !NPC->FindDialogueNode(DialogueNodeId, Node)) return FReply::Handled();

    const UBreakerQuestJournal* Journal = Character->GetQuestJournal();
    static const FBreakerQuestFlagSet EmptyFlags;
    TArray<FBreakerDialogueChoice> Choices;
    NPC->GetVisibleChoices(Node, Journal ? Journal->GetState() : EmptyFlags, Choices);
    if (!Choices.IsValidIndex(ChoiceIndex)) return FReply::Handled();
    const FBreakerDialogueChoice Choice = Choices[ChoiceIndex];
    if (Choice.Action == EBreakerDialogueAction::StartSurvivorEscort)
    {
        ABreakerSurvivor* Survivor = Cast<ABreakerSurvivor>(NPC);
        if (!Survivor || !Survivor->TryBeginEscort(Character.Get())) return FReply::Handled();
        Character->AddQuestFlag(Choice.SetsQuestFlag);
        Character->ResumeFromMenu();
        return FReply::Handled();
    }
    Character->AddQuestFlag(Choice.SetsQuestFlag);
    DialogueLeavePressedAt = 0.0;
    if (Choice.Action == EBreakerDialogueAction::OpenQuartermaster)
    {
        QuartermasterStatus = FText::GetEmpty();
        Rebuild(EBreakerMenuScreen::Quartermaster);
    }
    else if (Choice.Action == EBreakerDialogueAction::OpenForge)
    {
        bAtForge = true;
        ForgeStatus = FText::GetEmpty();
        Rebuild(EBreakerMenuScreen::Forge);
    }
    else if (Choice.NextNodeId == NAME_None)
    {
        Character->ResumeFromMenu();
    }
    else
    {
        DialogueNodeId = Choice.NextNodeId;
        Rebuild(EBreakerMenuScreen::Dialogue);
    }
    return FReply::Handled();
}
