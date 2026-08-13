#include "Interaction/BreakerNPC.h"

#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"

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
        for (const FBreakerDialogueChoice& Choice : Node.Choices)
        {
            if (Choice.NextNodeId != NAME_None && !FindDialogueNode(Choice.NextNodeId, Unused))
            {
                OutError = FString::Printf(TEXT("Node '%s' links to missing node '%s'"), *Node.NodeId.ToString(), *Choice.NextNodeId.ToString());
                return false;
            }
        }
    }
    return true;
}

namespace
{
    FBreakerDialogueChoice MakeChoice(const TCHAR* Text, FName NextNodeId = NAME_None, FName SetsQuestFlag = NAME_None)
    {
        FBreakerDialogueChoice Choice;
        Choice.Text = Text;
        Choice.NextNodeId = NextNodeId;
        Choice.SetsQuestFlag = SetsQuestFlag;
        return Choice;
    }

    FBreakerDialogueNode MakeNode(FName NodeId, const TCHAR* Line, std::initializer_list<FBreakerDialogueChoice> Choices)
    {
        FBreakerDialogueNode Node;
        Node.NodeId = NodeId;
        Node.SpeakerLine = Line;
        Node.Choices = Choices;
        return Node;
    }

    ABreakerNPC* SpawnNPC(UWorld* World, const FVector& Location, const FRotator& Rotation)
    {
        if (!World) return nullptr;
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        return World->SpawnActor<ABreakerNPC>(ABreakerNPC::StaticClass(), Location + FVector(0, 0, 88.0f), Rotation, Params);
    }
}

ABreakerNPC* ABreakerNPC::SpawnForgeKeeper(UWorld* World, const FVector& Location, const FRotator& Rotation)
{
    ABreakerNPC* NPC = SpawnNPC(World, Location, Rotation);
    if (!NPC) return nullptr;
    NPC->DisplayName = FText::FromString(TEXT("KESS — FORGE KEEPER"));
    NPC->DialogueNodes =
    {
        MakeNode(TEXT("Start"), TEXT("The Forge is cold today. Bring me something worth heating."),
        {
            MakeChoice(TEXT("What do you do here?"), TEXT("Role")),
            MakeChoice(TEXT("You're an Effigy, aren't you?"), TEXT("Effigy")),
            MakeChoice(TEXT("[Leave] Another time.")),
        }),
        MakeNode(TEXT("Role"), TEXT("Respecs. Crafting. Tier work, when you find gear worth the risk. The Forge handles every change you'll ever make to yourself — remember that."),
        {
            MakeChoice(TEXT("I'll bring you something."), TEXT("Start"), TEXT("Quest.MetForgeKeeper")),
            MakeChoice(TEXT("[Leave] Good to know.")),
        }),
        MakeNode(TEXT("Effigy"), TEXT("...Yes. Built before the militia captured the shipment. I remember being manufactured. Ask me what you actually came to ask, or don't."),
        {
            MakeChoice(TEXT("Did you ever meet Rior?"), TEXT("Rior")),
            MakeChoice(TEXT("[Leave] Sorry. None of my business.")),
        }),
        MakeNode(TEXT("Rior"), TEXT("(A long pause. The Forge hums.) ...Bring me something worth heating, Breaker."),
        {
            MakeChoice(TEXT("[Leave] ...Understood."), NAME_None, TEXT("Quest.AskedKessAboutRior")),
        }),
    };
    return NPC;
}

ABreakerNPC* ABreakerNPC::SpawnQuartermaster(UWorld* World, const FVector& Location, const FRotator& Rotation)
{
    ABreakerNPC* NPC = SpawnNPC(World, Location, Rotation);
    if (!NPC) return nullptr;
    NPC->DisplayName = FText::FromString(TEXT("QUARTERMASTER"));
    NPC->DialogueNodes =
    {
        MakeNode(TEXT("Start"), TEXT("Ammo's on the shelf, gear's in the crates. You breaking things or buying things?"),
        {
            MakeChoice(TEXT("Show me what you've got. (Vendor — coming soon)"), TEXT("Vendor")),
            MakeChoice(TEXT("Anything need doing around here?"), TEXT("Job")),
            MakeChoice(TEXT("[Leave] Just passing through.")),
        }),
        MakeNode(TEXT("Vendor"), TEXT("Storefront's not built yet, Breaker. The rift schedule says soon. Everything comes through the rift schedule eventually."),
        {
            MakeChoice(TEXT("[Leave] I'll check back."), NAME_None, TEXT("Quest.CheckedVendor")),
        }),
        MakeNode(TEXT("Job"), TEXT("The spill out past the pad keeps regrouping. Thin it out, and put that elite down while you're at it. I count what comes back — that's the job."),
        {
            MakeChoice(TEXT("Consider it done."), NAME_None, TEXT("Quest.AcceptedFirstContract")),
            MakeChoice(TEXT("[Leave] Not my problem yet.")),
        }),
    };
    return NPC;
}
