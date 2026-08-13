#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BreakerNPC.generated.h"

class UCapsuleComponent;
class UStaticMeshComponent;

USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerDialogueChoice
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Text;
    // NAME_None ends the conversation.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName NextNodeId = NAME_None;
    // Optional quest flag set on the character when this choice is picked.
    // The groundwork for vendors and quest states: flags persist in the save.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName SetsQuestFlag = NAME_None;
};

USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerDialogueNode
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName NodeId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString SpeakerLine;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FBreakerDialogueChoice> Choices;
};

// A friendly, talkable actor: the groundwork for vendors, the Forge Keeper,
// and quest givers. Dialogue is a flat node list navigated by id so it can
// later move into Data Assets without changing the runtime.
UCLASS(Blueprintable)
class RIORSEDGE_API ABreakerNPC : public AActor
{
    GENERATED_BODY()

public:
    ABreakerNPC();

    UFUNCTION(BlueprintPure, Category="NPC") FText GetDisplayName() const { return DisplayName; }
    UFUNCTION(BlueprintPure, Category="NPC") FName GetStartNodeId() const { return StartNodeId; }
    UFUNCTION(BlueprintPure, Category="NPC") bool FindDialogueNode(FName NodeId, FBreakerDialogueNode& OutNode) const;
    UFUNCTION(BlueprintPure, Category="NPC") float GetInteractionRange() const { return InteractionRange; }

    // Every NextNodeId must resolve to a node in the list; automation
    // verifies the fallback conversations against this.
    bool ValidateDialogue(FString& OutError) const;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPC") FText DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPC") FName StartNodeId = TEXT("Start");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPC") TArray<FBreakerDialogueNode> DialogueNodes;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPC", meta=(ClampMin="50")) float InteractionRange = 300.0f;

    // Zero-setup placeholder conversations for the gym camp.
    static ABreakerNPC* SpawnForgeKeeper(UWorld* World, const FVector& Location, const FRotator& Rotation);
    static ABreakerNPC* SpawnQuartermaster(UWorld* World, const FVector& Location, const FRotator& Rotation);

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UCapsuleComponent> Body;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Visual;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Head;
};
