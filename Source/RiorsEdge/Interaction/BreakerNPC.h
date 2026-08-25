#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Save/BreakerQuestJournal.h"
#include "BreakerNPC.generated.h"

class UCapsuleComponent;
class UPointLightComponent;
class UStaticMeshComponent;

// What a choice does BESIDES routing and setting a flag. A typed field rather
// than a screen enum, so Interaction/ takes no dependency on UI/, and rather
// than the menu string-matching a node id — which would make an authored node
// id load-bearing content nobody could ever rename.
//
// APPEND ONLY: dialogue is authorable content and this serializes by value.
UENUM(BlueprintType)
enum class EBreakerDialogueAction : uint8
{
    None,
    // O100: the quartermaster's unlock screen. Unlocking is an Anchor
    // interaction, so this is the ONLY way that screen opens — it has no
    // tab-strip entry and no pause-menu path.
    OpenQuartermaster,
    // The Forge, for the same reason and by the same route. content-and-modes
    // rules Forge and vendor Anchor interactions that never appear in a pause
    // menu; the Forge was in the shared tab strip, which the pause menu's
    // INVENTORY button opens two clicks deep.
    OpenForge
};

USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerDialogueChoice
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Text;
    // NAME_None ends the conversation.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName NextNodeId = NAME_None;
    // Optional quest flag set on the character when this choice is picked.
    // The character's journal persists it write-through.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName SetsQuestFlag = NAME_None;
    // Conditions. Required is ALL, blocked is ANY, empty passes. This is what
    // makes a flag matter to a player: before it, flags could be written and
    // never read, so an NPC said the same thing forever no matter what the
    // player had done (Campaign-And-Story.md 6.3 #1).
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> RequiredFlags;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> BlockedByFlags;
    // Runs when the choice is picked, after the flag is written. None for every
    // authored choice but one.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EBreakerDialogueAction Action = EBreakerDialogueAction::None;
};

USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerDialogueNode
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName NodeId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString SpeakerLine;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FBreakerDialogueChoice> Choices;
    // Same conditions at node scope, for gating a whole branch rather than one
    // line. A node that fails its conditions is never entered.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> RequiredFlags;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> BlockedByFlags;
};

// Per-NPC entry state. ShowDialogue used to always open on StartNodeId, so an
// NPC greeted the player identically forever and no returning-visit beat could
// exist (Campaign-And-Story.md 6.3 #2). Evaluated FIRST MATCH WINS, so the
// most-progressed entry is authored first.
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerDialogueEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> RequiredFlags;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> BlockedByFlags;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName StartNodeId = NAME_None;
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
    // Presentation only: paints the person palette onto the basic shapes.
    // Dynamic material instances need a live world, so this cannot happen in
    // the constructor (which also runs on the CDO).
    virtual void BeginPlay() override;
    // Swaps the primitive assembly for BodyMeshAsset when it resolves.
    // Called by BeginPlay (editor-placed NPCs) and by the runtime spawners
    // after they set the path — BeginPlay has already run inside SpawnActor
    // by the time a spawner can set anything.
    UFUNCTION(BlueprintCallable, Category="NPC") void ApplyBodyMesh();

    UFUNCTION(BlueprintPure, Category="NPC") FText GetDisplayName() const { return DisplayName; }
    UFUNCTION(BlueprintPure, Category="NPC") FName GetStartNodeId() const { return StartNodeId; }
    UFUNCTION(BlueprintPure, Category="NPC") bool FindDialogueNode(FName NodeId, FBreakerDialogueNode& OutNode) const;
    UFUNCTION(BlueprintPure, Category="NPC") float GetInteractionRange() const { return InteractionRange; }

    // Which node this NPC opens on for a given player state. Falls back to
    // StartNodeId when no override matches, so an NPC with no entries behaves
    // exactly as it did before entries existed.
    FName ResolveStartNodeId(const FBreakerQuestFlagSet& Flags) const;
    // The choices this player may actually see on a node. The UI iterates this
    // instead of Node.Choices.
    void GetVisibleChoices(const FBreakerDialogueNode& Node, const FBreakerQuestFlagSet& Flags, TArray<FBreakerDialogueChoice>& OutChoices) const;

    // Every NextNodeId must resolve to a node in the list; automation
    // verifies the fallback conversations against this.
    bool ValidateDialogue(FString& OutError) const;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPC") FText DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPC") FName StartNodeId = TEXT("Start");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPC") TArray<FBreakerDialogueNode> DialogueNodes;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPC") TArray<FBreakerDialogueEntry> EntryOverrides;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPC", meta=(ClampMin="50")) float InteractionRange = 300.0f;

    // THE NAMED BODY (ruled): the designer's blockout shipped npc_kess and
    // npc_quartermaster meshes and nothing referenced them, so Kess and the
    // Quartermaster were visually identical objects distinguished only by
    // DisplayName. The spawners set this path; BeginPlay loads it and, on
    // success, swaps the primitive assembly for the named mesh — on failure
    // (a clone without the imported blockout) the primitives remain, same
    // fallback shape as the audio samples. Offset/rotation exist because a
    // GLB import's pivot is the artist's, not ours; corrected from capture.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPC") FSoftObjectPath BodyMeshAsset;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPC") FVector BodyMeshOffset = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPC") FRotator BodyMeshRotation = FRotator::ZeroRotator;

    // Zero-setup placeholder conversations for the gym camp. The content is
    // split out of the spawners so automation can validate and walk it with no
    // world — a conversation that needs an actor cannot be unit-tested.
    static TArray<FBreakerDialogueNode> MakeForgeKeeperDialogue();
    static TArray<FBreakerDialogueEntry> MakeForgeKeeperEntries();
    static TArray<FBreakerDialogueNode> MakeQuartermasterDialogue();
    static TArray<FBreakerDialogueEntry> MakeQuartermasterEntries();

    static ABreakerNPC* SpawnForgeKeeper(UWorld* World, const FVector& Location, const FRotator& Rotation);
    static ABreakerNPC* SpawnQuartermaster(UWorld* World, const FVector& Location, const FRotator& Rotation);

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UCapsuleComponent> Body;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Visual;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Head;
    // The named-mesh body; hidden until BodyMeshAsset resolves in BeginPlay.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> BodyMesh;
    // NPC READABILITY (owner playtest 2026-08-17: "no visual indicator that
    // the npcs are people"). Enemies read as cool grey-violet silhouettes with
    // harm-red bars; a person reads WARM: a bright amber sash across the
    // chest, a warm palette on body and head, and a soft warm glow so the two
    // populations separate at a glance and at distance. The overhead floating
    // name label rides in the HUD (ABreakerPlaytestHUD::DrawInteractableLabels)
    // in the same projected-canvas idiom the enemy labels use.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Trim;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UPointLightComponent> Glow;
};
