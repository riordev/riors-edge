#include "Interaction/BreakerNPC.h"

#include "Animation/AnimSequence.h"
#include "Combat/BreakerEnemyBodyMath.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Data/BreakerDataFile.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Save/BreakerQuestContent.h"

namespace
{
    // The person palette. Warm on purpose: every hostile in Combat/ tints
    // cool (grey-violet chassis, harm-red bars), so warmth alone says
    // "not a target" before range, name or prompt can. O2 PLACEHOLDER values —
    // presentation, judged by screenshot, no gameplay meaning.
    const FLinearColor NPCCoat  (0.42f, 0.30f, 0.18f); // waxed-canvas coat
    const FLinearColor NPCFace  (0.78f, 0.62f, 0.46f); // a face, not a sensor
    const FLinearColor NPCSash  (1.00f, 0.68f, 0.22f); // the bright amber trim
    const FLinearColor NPCLight (1.00f, 0.72f, 0.35f); // campfire-warm glow

    // Same stock-material-plus-dynamic-instance idiom as the hub and gym
    // builders: BasicShapeMaterial exposes one "Color" vector param, so the
    // whole palette costs zero assets.
    void ApplyPersonColor(UStaticMeshComponent* Mesh, const FLinearColor& Color)
    {
        if (!Mesh) return;
        UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(
            nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
        if (!BaseMaterial) return;
        if (UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(BaseMaterial, Mesh))
        {
            Dynamic->SetVectorParameterValue(TEXT("Color"), Color);
            Mesh->SetMaterial(0, Dynamic);
        }
    }
}

ABreakerNPC::ABreakerNPC()
{
    PrimaryActorTick.bCanEverTick = false;

    Body = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Body"));
    Body->InitCapsuleSize(34.0f, 88.0f);
    Body->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    SetRootComponent(Body);

    Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
    Visual->SetupAttachment(Body);
    Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Visual->SetRelativeScale3D(FVector(0.55f, 0.45f, 1.5f));
    Visual->SetRelativeLocation(FVector(0.0f, 0.0f, -12.0f));
    if (UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
    {
        Visual->SetStaticMesh(Cube);
    }

    Head = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head"));
    Head->SetupAttachment(Body);
    Head->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Head->SetRelativeScale3D(FVector(0.32f, 0.32f, 0.32f));
    Head->SetRelativeLocation(FVector(0.0f, 0.0f, 92.0f));
    if (UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")))
    {
        Head->SetStaticMesh(Sphere);
    }

    // The sash: a thin bright band worn diagonally across the torso. Its job
    // is silhouette-breaking colour — enemies are unbroken slabs, a person
    // wears KIT — and it reads at the same distance the body shape does.
    Trim = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Trim"));
    Trim->SetupAttachment(Body);
    Trim->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Trim->SetRelativeScale3D(FVector(0.58f, 0.14f, 0.62f));
    Trim->SetRelativeLocation(FVector(0.0f, 0.0f, 30.0f));
    Trim->SetRelativeRotation(FRotator(0.0f, 0.0f, 28.0f));
    if (UStaticMesh* TrimCube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
    {
        Trim->SetStaticMesh(TrimCube);
    }

    // The named body (see BodyMeshAsset): created hidden; BeginPlay shows it
    // and hides the primitives when the asset resolves.
    BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
    BodyMesh->SetupAttachment(Body);
    BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BodyMesh->SetVisibility(false);
    SkeletalBody = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalBody"));
    SkeletalBody->SetupAttachment(Body);
    SkeletalBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SkeletalBody->SetVisibility(false);

    // The idle glow: a soft warm pool, same idiom as the hub's prop lights
    // (HubAttachPropLight) but owned by the NPC so it travels with them.
    // Deliberately dimmer than the forge/crate props — a person is lit, not a
    // beacon. No shadows: it is a read, not a light source that costs.
    Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
    Glow->SetupAttachment(Body);
    Glow->SetRelativeLocation(FVector(0.0f, 0.0f, 60.0f));
    Glow->SetLightColor(NPCLight);
    Glow->SetIntensity(650.0f);
    Glow->SetAttenuationRadius(520.0f);
    Glow->SetCastShadows(false);
}

void ABreakerNPC::BeginPlay()
{
    Super::BeginPlay();
    ApplyPersonColor(Visual, NPCCoat);
    ApplyPersonColor(Head, NPCFace);
    ApplyPersonColor(Trim, NPCSash);

    // Editor-placed NPCs carry BodyMeshAsset as a property; runtime spawners
    // set it AFTER SpawnActor (BeginPlay has already run by then) and call
    // ApplyBodyMesh themselves.
    ApplyBodyMesh();
}

void ABreakerNPC::ApplyBodyMesh()
{
    // THE NAMED BODY. When the designer's mesh resolves, it replaces the
    // cubes-and-a-sphere assembly whole; the warm glow stays either way (a
    // person is lit, whatever they are made of). A clone without the
    // imported blockout keeps the primitives — the same "the floor still
    // works" shape as the audio samples over the synth.
    if (!BodyMesh || !BodyMeshAsset.IsValid()) return;
    UObject* Loaded = BodyMeshAsset.TryLoad();
    // The SKELETAL branch: an intake base character. Fitted to the capsule by
    // the enemy's own pure rule (one fit, two wearers), given its idle so a
    // person stands rather than T-poses, and the same primitives fallback.
    if (USkeletalMesh* Person = Cast<USkeletalMesh>(Loaded))
    {
        if (!SkeletalBody) return;
        SkeletalBody->SetSkeletalMesh(Person);
        const FBoxSphereBounds MeshBounds = Person->GetBounds();
        const BreakerEnemyBody::FBreakerBodyFit Fit = BreakerEnemyBody::FitBodyToCapsule(
            MeshBounds.Origin, MeshBounds.BoxExtent, Body->GetScaledCapsuleHalfHeight());
        SkeletalBody->SetRelativeScale3D(FVector(Fit.Scale));
        SkeletalBody->SetRelativeLocation(Fit.RelativeLocation + BodyMeshOffset);
        SkeletalBody->SetRelativeRotation(BodyMeshRotation);
        if (UAnimSequence* Idle = Cast<UAnimSequence>(BodyIdleAnimation.TryLoad()))
        {
            SkeletalBody->SetAnimationMode(EAnimationMode::AnimationSingleNode);
            SkeletalBody->PlayAnimation(Idle, /*bLooping=*/true);
        }
        SkeletalBody->SetVisibility(true);
        BodyMesh->SetVisibility(false);
        Visual->SetVisibility(false);
        Head->SetVisibility(false);
        Trim->SetVisibility(false);
        return;
    }
    if (UStaticMesh* Named = Cast<UStaticMesh>(Loaded))
    {
        BodyMesh->SetStaticMesh(Named);
        // MEASURED, NOT ASSUMED: the blockout GLB imported at 100x (an NPC
        // 18,000 cm tall) with its geometry baked at SCENE position and the
        // pivot at the scene origin — npc_kess's bounds centre is exactly
        // the hub layout's vendor spot at 100x. So the placement is derived
        // from the mesh's own bounds: scale it to author intent, cancel the
        // baked offset so the bounds centre lands on the actor, and drop it
        // so the feet touch the capsule bottom. Generic — any re-export
        // with a different bake keeps working. The first wiring hard-coded
        // an identity transform and the capture showed a mesh the size of a
        // hill filling the plaza.
        constexpr float BreakerBlockoutScale = 0.01f;   // the 100x undone
        const FBoxSphereBounds MeshBounds = Named->GetBounds();
        const FVector Centred = -MeshBounds.Origin * BreakerBlockoutScale;
        // After centring, the feet sit at -scaledHalfZ; lift by the surplus
        // over the capsule's half height so they touch its bottom instead.
        const float FeetLift = static_cast<float>(MeshBounds.BoxExtent.Z) * BreakerBlockoutScale
            - Body->GetScaledCapsuleHalfHeight();
        BodyMesh->SetRelativeScale3D(FVector(BreakerBlockoutScale));
        BodyMesh->SetRelativeLocation(Centred + FVector(0.0f, 0.0f, FeetLift) + BodyMeshOffset);
        BodyMesh->SetRelativeRotation(BodyMeshRotation);
        BodyMesh->SetVisibility(true);
        Visual->SetVisibility(false);
        Head->SetVisibility(false);
        Trim->SetVisibility(false);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[BreakerNPC] %s: body mesh %s did not resolve — primitive fallback."),
            *DisplayName.ToString(), *BodyMeshAsset.ToString());
    }
}

bool ABreakerNPC::FindDialogueNode(FName NodeId, FBreakerDialogueNode& OutNode) const
{
    if (const FBreakerDialogueNode* Found = DialogueNodes.FindByPredicate([NodeId](const FBreakerDialogueNode& Node) { return Node.NodeId == NodeId; }))
    {
        OutNode = *Found;
        return true;
    }
    return false;
}

FName ABreakerNPC::ResolveStartNodeId(const FBreakerQuestFlagSet& Flags) const
{
    // First match wins, so the most-progressed entry is authored first.
    for (const FBreakerDialogueEntry& Entry : EntryOverrides)
    {
        if (Entry.StartNodeId == NAME_None) continue;
        if (!UBreakerQuestLibrary::PassesFlagConditions(Entry.RequiredFlags, Entry.BlockedByFlags, Flags)) continue;
        FBreakerDialogueNode Unused;
        // A dangling override must not silently swallow the conversation; fall
        // through to the ordinary start instead. ValidateDialogue catches the
        // authoring error separately.
        if (FindDialogueNode(Entry.StartNodeId, Unused)) return Entry.StartNodeId;
    }
    return StartNodeId;
}

void ABreakerNPC::GetVisibleChoices(const FBreakerDialogueNode& Node, const FBreakerQuestFlagSet& Flags, TArray<FBreakerDialogueChoice>& OutChoices) const
{
    OutChoices.Reset();
    for (const FBreakerDialogueChoice& Choice : Node.Choices)
    {
        if (!UBreakerQuestLibrary::PassesFlagConditions(Choice.RequiredFlags, Choice.BlockedByFlags, Flags)) continue;
        // A choice pointing at a node the player cannot enter is a dead end
        // dressed as an option. Hide it with the node it leads to.
        if (Choice.NextNodeId != NAME_None)
        {
            FBreakerDialogueNode Next;
            if (FindDialogueNode(Choice.NextNodeId, Next)
                && !UBreakerQuestLibrary::PassesFlagConditions(Next.RequiredFlags, Next.BlockedByFlags, Flags))
            {
                continue;
            }
        }
        OutChoices.Add(Choice);
    }
}

bool ABreakerNPC::ValidateDialogue(FString& OutError) const
{
    FBreakerDialogueNode Unused;
    if (!FindDialogueNode(StartNodeId, Unused))
    {
        OutError = FString::Printf(TEXT("Start node '%s' missing"), *StartNodeId.ToString());
        return false;
    }
    for (const FBreakerDialogueNode& Node : DialogueNodes)
    {
        if (Node.Choices.IsEmpty())
        {
            OutError = FString::Printf(TEXT("Node '%s' has no choices (needs at least an exit)"), *Node.NodeId.ToString());
            return false;
        }
        // Conditions introduced a new way to strand a player: every choice on a
        // node could be gated off, leaving a conversation with no exit but Esc.
        // Every node must therefore carry at least one UNCONDITIONAL choice.
        const bool bHasUnconditional = Node.Choices.ContainsByPredicate([](const FBreakerDialogueChoice& Choice)
        {
            return Choice.RequiredFlags.IsEmpty() && Choice.BlockedByFlags.IsEmpty();
        });
        if (!bHasUnconditional)
        {
            OutError = FString::Printf(TEXT("Node '%s' has no unconditional choice: a player without the flags would be stranded"), *Node.NodeId.ToString());
            return false;
        }
        for (const FBreakerDialogueChoice& Choice : Node.Choices)
        {
            if (Choice.NextNodeId != NAME_None && !FindDialogueNode(Choice.NextNodeId, Unused))
            {
                OutError = FString::Printf(TEXT("Node '%s' links to missing node '%s'"), *Node.NodeId.ToString(), *Choice.NextNodeId.ToString());
                return false;
            }
        }
    }
    for (const FBreakerDialogueEntry& Entry : EntryOverrides)
    {
        if (Entry.StartNodeId == NAME_None || !FindDialogueNode(Entry.StartNodeId, Unused))
        {
            OutError = FString::Printf(TEXT("Entry override points at missing node '%s'"), *Entry.StartNodeId.ToString());
            return false;
        }
        if (Entry.RequiredFlags.IsEmpty() && Entry.BlockedByFlags.IsEmpty())
        {
            OutError = FString::Printf(TEXT("Entry override to '%s' has no conditions, so it would shadow every later entry"), *Entry.StartNodeId.ToString());
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// THE LOADER — Data/dialogue.json into one row per NPC.
// ---------------------------------------------------------------------------
// The file is the conversation. Every node, choice and entry override of both
// Anchor NPCs is a row here, and the flags they gate on are the same strings
// Data/quests.json registers; ValidateQuestContent checks that the two files
// agree. A row that fails any check below fails the WHOLE load: the data
// comes back empty behind an ensure, because a conversation that dropped one
// choice and served the rest could strand a player on a node with no exit.
namespace
{
    using BreakerDataFile::FBreakerDataErrors;

    const TCHAR* const BreakerDialogueForgeKeeperId = TEXT("ForgeKeeper");
    const TCHAR* const BreakerDialogueQuartermasterId = TEXT("Quartermaster");

    struct FBreakerDialogueLoad
    {
        FBreakerDialogueData Data;
        TArray<FString> Errors;
    };

    bool BreakerDialogueReadString(const FJsonObject& Row, const TCHAR* Field, const FString& Where, FString& Out, FBreakerDataErrors& Errors)
    {
        if (!Row.TryGetStringField(Field, Out))
        {
            Errors.Add(FString::Printf(TEXT("%s: \"%s\" is missing or not a string"), *Where, Field));
            return false;
        }
        return true;
    }

    // A name field; "" reads as NAME_None.
    bool BreakerDialogueReadName(const FJsonObject& Row, const TCHAR* Field, const FString& Where, FName& Out, FBreakerDataErrors& Errors)
    {
        FString Name;
        if (!BreakerDialogueReadString(Row, Field, Where, Name, Errors))
        {
            return false;
        }
        Out = Name.IsEmpty() ? NAME_None : FName(*Name);
        return true;
    }

    bool BreakerDialogueReadNames(const FJsonObject& Row, const TCHAR* Field, const FString& Where, TArray<FName>& Out, FBreakerDataErrors& Errors)
    {
        Out.Reset();
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        if (!Row.TryGetArrayField(Field, Values))
        {
            Errors.Add(FString::Printf(TEXT("%s: \"%s\" is missing or not an array"), *Where, Field));
            return false;
        }
        bool bOk = true;
        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            FString Name;
            if (!Value.IsValid() || !Value->TryGetString(Name) || Name.IsEmpty())
            {
                Errors.Add(FString::Printf(TEXT("%s: a \"%s\" entry is not a flag name"), *Where, Field));
                bOk = false;
                continue;
            }
            Out.Add(FName(*Name));
        }
        return bOk;
    }

    bool BreakerDialogueReadChoice(const FJsonObject& Row, const FString& Where, FBreakerDialogueChoice& Out, FBreakerDataErrors& Errors)
    {
        bool bOk = BreakerDialogueReadString(Row, TEXT("text"), Where, Out.Text, Errors);
        if (bOk && Out.Text.IsEmpty())
        {
            Errors.Add(FString::Printf(TEXT("%s: a choice has empty \"text\""), *Where));
            bOk = false;
        }
        bOk = BreakerDialogueReadName(Row, TEXT("nextNodeId"), Where, Out.NextNodeId, Errors) && bOk;
        bOk = BreakerDialogueReadName(Row, TEXT("setsQuestFlag"), Where, Out.SetsQuestFlag, Errors) && bOk;
        bOk = BreakerDialogueReadNames(Row, TEXT("requiredFlags"), Where, Out.RequiredFlags, Errors) && bOk;
        bOk = BreakerDialogueReadNames(Row, TEXT("blockedByFlags"), Where, Out.BlockedByFlags, Errors) && bOk;

        FString Action;
        if (!Row.TryGetStringField(TEXT("action"), Action) || !BreakerDataFile::ParseEnum(Action, Out.Action))
        {
            Errors.Add(FString::Printf(TEXT("%s: \"action\" is \"%s\", not an EBreakerDialogueAction"), *Where, *Action));
            bOk = false;
        }
        return bOk;
    }

    bool BreakerDialogueReadNode(const FJsonObject& Row, const FString& NpcId, FBreakerDialogueNode& Out, FBreakerDataErrors& Errors)
    {
        FString Id;
        if (!Row.TryGetStringField(TEXT("nodeId"), Id) || Id.IsEmpty())
        {
            Errors.Add(FString::Printf(TEXT("%s: a node has no \"nodeId\""), *NpcId));
            return false;
        }
        Out.NodeId = FName(*Id);
        const FString Where = NpcId + TEXT(".") + Id;

        bool bOk = BreakerDialogueReadString(Row, TEXT("speakerLine"), Where, Out.SpeakerLine, Errors);
        bOk = BreakerDialogueReadNames(Row, TEXT("requiredFlags"), Where, Out.RequiredFlags, Errors) && bOk;
        bOk = BreakerDialogueReadNames(Row, TEXT("blockedByFlags"), Where, Out.BlockedByFlags, Errors) && bOk;

        Out.Choices.Reset();
        const TArray<TSharedPtr<FJsonValue>>* Choices = nullptr;
        if (!Row.TryGetArrayField(TEXT("choices"), Choices))
        {
            Errors.Add(FString::Printf(TEXT("%s: \"choices\" is missing or not an array"), *Where));
            return false;
        }
        for (const TSharedPtr<FJsonValue>& Value : *Choices)
        {
            const TSharedPtr<FJsonObject>* ChoiceObject = nullptr;
            if (!Value.IsValid() || !Value->TryGetObject(ChoiceObject))
            {
                Errors.Add(FString::Printf(TEXT("%s: a \"choices\" entry is not an object"), *Where));
                bOk = false;
                continue;
            }
            FBreakerDialogueChoice Choice;
            if (!BreakerDialogueReadChoice(**ChoiceObject, Where, Choice, Errors))
            {
                bOk = false;
                continue;
            }
            Out.Choices.Add(Choice);
        }
        return bOk;
    }

    bool BreakerDialogueReadEntry(const FJsonObject& Row, const FString& NpcId, FBreakerDialogueEntry& Out, FBreakerDataErrors& Errors)
    {
        const FString Where = NpcId + TEXT(".entries");
        bool bOk = BreakerDialogueReadName(Row, TEXT("startNodeId"), Where, Out.StartNodeId, Errors);
        if (bOk && Out.StartNodeId.IsNone())
        {
            Errors.Add(FString::Printf(TEXT("%s: an entry has empty \"startNodeId\""), *Where));
            bOk = false;
        }
        bOk = BreakerDialogueReadNames(Row, TEXT("requiredFlags"), Where, Out.RequiredFlags, Errors) && bOk;
        bOk = BreakerDialogueReadNames(Row, TEXT("blockedByFlags"), Where, Out.BlockedByFlags, Errors) && bOk;
        return bOk;
    }

    // One npc row. Returns false with every field-level complaint recorded,
    // not just the first.
    bool BreakerDialogueReadNpc(const FJsonObject& Row, FBreakerDialogueRow& Out, FBreakerDataErrors& Errors)
    {
        FString Id;
        if (!Row.TryGetStringField(TEXT("id"), Id) || Id.IsEmpty())
        {
            Errors.Add(TEXT("an npc row has no \"id\""));
            return false;
        }
        Out.Id = FName(*Id);

        bool bOk = BreakerDialogueReadString(Row, TEXT("displayName"), Id, Out.DisplayName, Errors);
        if (bOk && Out.DisplayName.IsEmpty())
        {
            Errors.Add(FString::Printf(TEXT("%s: \"displayName\" is empty"), *Id));
            bOk = false;
        }
        bOk = BreakerDialogueReadName(Row, TEXT("startNodeId"), Id, Out.StartNodeId, Errors) && bOk;
        if (Out.StartNodeId.IsNone())
        {
            Errors.Add(FString::Printf(TEXT("%s: \"startNodeId\" is empty"), *Id));
            bOk = false;
        }

        Out.Nodes.Reset();
        const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
        if (!Row.TryGetArrayField(TEXT("nodes"), Nodes))
        {
            Errors.Add(FString::Printf(TEXT("%s: \"nodes\" is missing or not an array"), *Id));
            bOk = false;
        }
        else
        {
            for (const TSharedPtr<FJsonValue>& Value : *Nodes)
            {
                const TSharedPtr<FJsonObject>* NodeObject = nullptr;
                if (!Value.IsValid() || !Value->TryGetObject(NodeObject))
                {
                    Errors.Add(FString::Printf(TEXT("%s: a \"nodes\" entry is not an object"), *Id));
                    bOk = false;
                    continue;
                }
                FBreakerDialogueNode Node;
                if (!BreakerDialogueReadNode(**NodeObject, Id, Node, Errors))
                {
                    bOk = false;
                    continue;
                }
                if (Out.Nodes.ContainsByPredicate([&Node](const FBreakerDialogueNode& Other) { return Other.NodeId == Node.NodeId; }))
                {
                    Errors.Add(FString::Printf(TEXT("%s: node \"%s\" appears twice"), *Id, *Node.NodeId.ToString()));
                    bOk = false;
                    continue;
                }
                Out.Nodes.Add(Node);
            }
        }

        Out.Entries.Reset();
        const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
        if (!Row.TryGetArrayField(TEXT("entries"), Entries))
        {
            Errors.Add(FString::Printf(TEXT("%s: \"entries\" is missing or not an array"), *Id));
            bOk = false;
        }
        else
        {
            for (const TSharedPtr<FJsonValue>& Value : *Entries)
            {
                const TSharedPtr<FJsonObject>* EntryObject = nullptr;
                if (!Value.IsValid() || !Value->TryGetObject(EntryObject))
                {
                    Errors.Add(FString::Printf(TEXT("%s: an \"entries\" entry is not an object"), *Id));
                    bOk = false;
                    continue;
                }
                FBreakerDialogueEntry Entry;
                if (!BreakerDialogueReadEntry(**EntryObject, Id, Entry, Errors))
                {
                    bOk = false;
                    continue;
                }
                Out.Entries.Add(Entry);
            }
        }
        return bOk;
    }

    FBreakerDialogueLoad BreakerDialogueLoadData()
    {
        FBreakerDialogueLoad Load;
        FBreakerDataErrors Errors;
        FBreakerDialogueData Data;
        const FString File = ABreakerNPC::DialogueRelativePath();

        const TSharedPtr<FJsonObject> Root = BreakerDataFile::Load(File, Errors);
        if (Root.IsValid())
        {
            const TArray<TSharedPtr<FJsonValue>>* RowValues = nullptr;
            if (!Root->TryGetArrayField(TEXT("npcs"), RowValues))
            {
                Errors.Add(FString::Printf(TEXT("%s: no \"npcs\" array"), *File));
            }
            else
            {
                for (const TSharedPtr<FJsonValue>& Value : *RowValues)
                {
                    const TSharedPtr<FJsonObject>* RowObject = nullptr;
                    if (!Value.IsValid() || !Value->TryGetObject(RowObject))
                    {
                        Errors.Add(FString::Printf(TEXT("%s: an \"npcs\" entry is not an object"), *File));
                        continue;
                    }
                    FBreakerDialogueRow Row;
                    if (!BreakerDialogueReadNpc(**RowObject, Row, Errors))
                    {
                        continue;
                    }
                    if (Data.Npcs.ContainsByPredicate([&Row](const FBreakerDialogueRow& Other) { return Other.Id == Row.Id; }))
                    {
                        Errors.Add(FString::Printf(TEXT("%s: id appears twice"), *Row.Id.ToString()));
                        continue;
                    }
                    Data.Npcs.Add(Row);
                }
            }
        }

        if (!Errors.IsClean())
        {
            Load.Errors = Errors.Messages;
            ensureMsgf(false, TEXT("%s failed to load; the dialogue is EMPTY.\n%s"), *File, *Errors.Join());
            return Load;
        }
        Load.Data = MoveTemp(Data);
        return Load;
    }

    const FBreakerDialogueLoad& BreakerDialogueLoaded()
    {
        static const FBreakerDialogueLoad Load = BreakerDialogueLoadData();
        return Load;
    }

    // The row by id, or an empty row: a dirty load serves nothing, and the
    // callers' emptiness is what the tests then report.
    const FBreakerDialogueRow& BreakerDialogueRowById(const TCHAR* Id)
    {
        static const FBreakerDialogueRow Empty;
        const FName Key(Id);
        const FBreakerDialogueRow* Row = BreakerDialogueLoaded().Data.Npcs.FindByPredicate(
            [Key](const FBreakerDialogueRow& Candidate) { return Candidate.Id == Key; });
        return Row ? *Row : Empty;
    }

    ABreakerNPC* SpawnNPC(UWorld* World, const FVector& Location, const FRotator& Rotation)
    {
        if (!World) return nullptr;
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        return World->SpawnActor<ABreakerNPC>(ABreakerNPC::StaticClass(), Location + FVector(0, 0, 88.0f), Rotation, Params);
    }
}

FString ABreakerNPC::DialogueRelativePath()
{
    return TEXT("Data/dialogue.json");
}

const FBreakerDialogueData& ABreakerNPC::GetDialogueData()
{
    return BreakerDialogueLoaded().Data;
}

const TArray<FString>& ABreakerNPC::GetDialogueErrors()
{
    return BreakerDialogueLoaded().Errors;
}

TArray<FBreakerDialogueNode> ABreakerNPC::MakeForgeKeeperDialogue()
{
    return BreakerDialogueRowById(BreakerDialogueForgeKeeperId).Nodes;
}

TArray<FBreakerDialogueEntry> ABreakerNPC::MakeForgeKeeperEntries()
{
    return BreakerDialogueRowById(BreakerDialogueForgeKeeperId).Entries;
}

TArray<FBreakerDialogueNode> ABreakerNPC::MakeQuartermasterDialogue()
{
    return BreakerDialogueRowById(BreakerDialogueQuartermasterId).Nodes;
}

TArray<FBreakerDialogueEntry> ABreakerNPC::MakeQuartermasterEntries()
{
    return BreakerDialogueRowById(BreakerDialogueQuartermasterId).Entries;
}

ABreakerNPC* ABreakerNPC::SpawnForgeKeeper(UWorld* World, const FVector& Location, const FRotator& Rotation)
{
    ABreakerNPC* NPC = SpawnNPC(World, Location, Rotation);
    if (!NPC) return nullptr;
    const FBreakerDialogueRow& Row = BreakerDialogueRowById(BreakerDialogueForgeKeeperId);
    NPC->DisplayName = FText::FromString(Row.DisplayName);
    NPC->StartNodeId = Row.StartNodeId;
    NPC->DialogueNodes = Row.Nodes;
    NPC->EntryOverrides = Row.Entries;
    // The blockout's own Kess, by name (ruled).
    // KESS = Superhero_Female_FullBody, recorded in Assets/npcs/
    // LICENSE-NOTE.txt with the one-body-family ruling; her blockout statue
    // (npc_kess) stands down for the rigged base at a talking idle. Note: the
    // Anchor has TWO NPCs — "Forge Keeper" is Kess's own title, not a third
    // person.
    NPC->BodyMeshAsset = FSoftObjectPath(TEXT("/Game/Breaker/Meshes/npcs/Superhero_Female_FullBody/SkeletalMeshes/Superhero_Female.Superhero_Female"));
    NPC->BodyIdleAnimation = FSoftObjectPath(TEXT("/Game/Breaker/Meshes/npcs/Superhero_Female_FullBody/Anims/UAL1_Standard/SkeletalMeshes/UAL1_StandardIdle_Talking_Loop.UAL1_StandardIdle_Talking_Loop"));
    NPC->ApplyBodyMesh();
    return NPC;
}

ABreakerNPC* ABreakerNPC::SpawnQuartermaster(UWorld* World, const FVector& Location, const FRotator& Rotation)
{
    ABreakerNPC* NPC = SpawnNPC(World, Location, Rotation);
    if (!NPC) return nullptr;
    const FBreakerDialogueRow& Row = BreakerDialogueRowById(BreakerDialogueQuartermasterId);
    NPC->DisplayName = FText::FromString(Row.DisplayName);
    NPC->StartNodeId = Row.StartNodeId;
    NPC->DialogueNodes = Row.Nodes;
    NPC->EntryOverrides = Row.Entries;
    // The Quartermaster wears the male base at a plain idle — the second of
    // the pack's two bodies, distinct from Kess at a glance.
    NPC->BodyMeshAsset = FSoftObjectPath(TEXT("/Game/Breaker/Meshes/npcs/Superhero_Male_FullBody/SkeletalMeshes/SuperHero_Male.SuperHero_Male"));
    NPC->BodyIdleAnimation = FSoftObjectPath(TEXT("/Game/Breaker/Meshes/npcs/Superhero_Male_FullBody/Anims/UAL1_Standard/SkeletalMeshes/UAL1_StandardIdle_Loop.UAL1_StandardIdle_Loop"));
    NPC->ApplyBodyMesh();
    return NPC;
}
