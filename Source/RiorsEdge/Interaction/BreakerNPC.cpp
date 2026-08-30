#include "Interaction/BreakerNPC.h"

#include "Animation/AnimSequence.h"
#include "Combat/BreakerEnemyBodyMath.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
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

namespace
{
    FBreakerDialogueChoice MakeChoice(const TCHAR* Text, FName NextNodeId = NAME_None, FName SetsQuestFlag = NAME_None,
        std::initializer_list<FName> RequiredFlags = {}, std::initializer_list<FName> BlockedByFlags = {},
        EBreakerDialogueAction Action = EBreakerDialogueAction::None)
    {
        FBreakerDialogueChoice Choice;
        Choice.Text = Text;
        Choice.NextNodeId = NextNodeId;
        Choice.SetsQuestFlag = SetsQuestFlag;
        Choice.Action = Action;
        Choice.RequiredFlags = RequiredFlags;
        Choice.BlockedByFlags = BlockedByFlags;
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

    FBreakerDialogueEntry MakeEntry(FName StartNodeId, std::initializer_list<FName> RequiredFlags, std::initializer_list<FName> BlockedByFlags = {})
    {
        FBreakerDialogueEntry Entry;
        Entry.StartNodeId = StartNodeId;
        Entry.RequiredFlags = RequiredFlags;
        Entry.BlockedByFlags = BlockedByFlags;
        return Entry;
    }

    ABreakerNPC* SpawnNPC(UWorld* World, const FVector& Location, const FRotator& Rotation)
    {
        if (!World) return nullptr;
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        return World->SpawnActor<ABreakerNPC>(ABreakerNPC::StaticClass(), Location + FVector(0, 0, 88.0f), Rotation, Params);
    }
}

TArray<FBreakerDialogueNode> ABreakerNPC::MakeForgeKeeperDialogue()
{
    using namespace BreakerQuestFlags;
    return
    {
        MakeNode(TEXT("Start"), TEXT("The Forge is cold today. Bring me something worth heating."),
        {
            MakeChoice(TEXT("What do you do here?"), TEXT("Role")),
            // O42/content-and-modes: the Forge is an Anchor interaction and this
            // is now the only way to it, on every entry state Kess has. No node
            // id or quest flag moves — BreakerQuestLoopTests walks this dialogue
            // and the salvage chain reads its flags.
            MakeChoice(TEXT("Open the Forge."), NAME_None, NAME_None, {}, {},
                EBreakerDialogueAction::OpenForge),
            // Gated: Kess only acknowledges the contract once it is closed, and
            // this is the smallest possible proof that a flag changes what an
            // NPC says.
            MakeChoice(TEXT("The Quartermaster's contract is closed."), TEXT("Contract"), NAME_None, { FirstContractTurnedIn }),
            // The chain's second link: Kess only asks once the first contract
            // proved the player comes back. Blocks on her own acceptance.
            MakeChoice(TEXT("What would heat the Forge?"), TEXT("Salvage"), KessSalvageOffered, { FirstContractTurnedIn }, { KessSalvageAccepted }),
            MakeChoice(TEXT("Still gathering your feedstock."), TEXT("SalvageProgress"), NAME_None, { KessSalvageAccepted }, { KessSalvageTurnedIn }),
            MakeChoice(TEXT("You're an Effigy, aren't you?"), TEXT("Effigy"), NAME_None, {}, { AskedKessAboutRior }),
            MakeChoice(TEXT("[Leave] Another time.")),
        }),
        MakeNode(TEXT("Role"), TEXT("Respecs. Crafting. Tier work, when you find gear worth the risk. The Forge handles every change you'll ever make to yourself — remember that."),
        {
            MakeChoice(TEXT("I'll bring you something."), TEXT("Start"), MetForgeKeeper),
            MakeChoice(TEXT("[Leave] Good to know.")),
        }),
        MakeNode(TEXT("Contract"), TEXT("So I heard. The Quartermaster counts what comes back, and this time the count was right. That buys you a hearing here, when you have something to bring."),
        {
            MakeChoice(TEXT("[Leave] I'll remember that.")),
        }),
        MakeNode(TEXT("Effigy"), TEXT("...Yes. Built before the militia captured the shipment. I remember being manufactured. Ask me what you actually came to ask, or don't."),
        {
            MakeChoice(TEXT("Did you ever meet Rior?"), TEXT("Rior")),
            MakeChoice(TEXT("[Leave] Sorry. None of my business.")),
        }),
        MakeNode(TEXT("Rior"), TEXT("(A long pause. The Forge hums.) ...Bring me something worth heating, Breaker."),
        {
            MakeChoice(TEXT("[Leave] ...Understood."), NAME_None, AskedKessAboutRior),
        }),
        // Entry target: what Kess opens with once the question has been asked.
        MakeNode(TEXT("Returned"), TEXT("(The Forge is still cold. Kess does not look up.) You came back. That's more than most."),
        {
            MakeChoice(TEXT("What do you do here?"), TEXT("Role")),
            // O42/content-and-modes: the Forge is an Anchor interaction and this
            // is now the only way to it, on every entry state Kess has. No node
            // id or quest flag moves — BreakerQuestLoopTests walks this dialogue
            // and the salvage chain reads its flags.
            MakeChoice(TEXT("Open the Forge."), NAME_None, NAME_None, {}, {},
                EBreakerDialogueAction::OpenForge),
            MakeChoice(TEXT("The Quartermaster's contract is closed."), TEXT("Contract"), NAME_None, { FirstContractTurnedIn }),
            MakeChoice(TEXT("What would heat the Forge?"), TEXT("Salvage"), KessSalvageOffered, { FirstContractTurnedIn }, { KessSalvageAccepted }),
            MakeChoice(TEXT("Still gathering your feedstock."), TEXT("SalvageProgress"), NAME_None, { KessSalvageAccepted }, { KessSalvageTurnedIn }),
            MakeChoice(TEXT("[Leave] Another time.")),
        }),

        // ---- Q2: FEED THE FORGE --------------------------------------------
        MakeNode(TEXT("Salvage"), TEXT("(Kess turns a cold crucible with one hand.) Feedstock. Vestige residue burns, and the spill carries more of it than the ground can rot. Six of them. Bring me what's left when they drop — I'll know the weight when you walk in."),
        {
            MakeChoice(TEXT("You'll have it."), NAME_None, KessSalvageAccepted),
            MakeChoice(TEXT("[Leave] Not today.")),
        }),
        MakeNode(TEXT("SalvageProgress"), TEXT("Not the weight yet. Six carcasses' worth. The Forge has waited years — it can wait an afternoon."),
        {
            MakeChoice(TEXT("[Leave] Working on it.")),
        }),
        // Entry target: the turn-in. She said she'd know the weight; she does.
        MakeNode(TEXT("SalvageTurnIn"), TEXT("(Kess does not weigh anything. Kess already knows.) That's the weight. The Forge takes it. ...It's warmer in here now. Remember that, next time you find something worth heating."),
        {
            MakeChoice(TEXT("The Forge eats first."), NAME_None, KessSalvageTurnedIn),
            MakeChoice(TEXT("[Leave] Later.")),
        }),
        // Entry target after the salvage closes: her opening line stops being
        // "the Forge is cold", because it stopped being true and she is the
        // one character who would never say a false thing about the Forge.
        MakeNode(TEXT("Warm"), TEXT("(The Forge is lit. Low, but lit.) You did that. Now find me something worth it."),
        {
            MakeChoice(TEXT("What do you do here?"), TEXT("Role")),
            // O42/content-and-modes: the Forge is an Anchor interaction and this
            // is now the only way to it, on every entry state Kess has. No node
            // id or quest flag moves — BreakerQuestLoopTests walks this dialogue
            // and the salvage chain reads its flags.
            MakeChoice(TEXT("Open the Forge."), NAME_None, NAME_None, {}, {},
                EBreakerDialogueAction::OpenForge),
            MakeChoice(TEXT("You're an Effigy, aren't you?"), TEXT("Effigy"), NAME_None, {}, { AskedKessAboutRior }),
            MakeChoice(TEXT("[Leave] Another time.")),
        }),
    };
}

TArray<FBreakerDialogueEntry> ABreakerNPC::MakeForgeKeeperEntries()
{
    using namespace BreakerQuestFlags;
    return
    {
        // Most progressed first, same rule as the Quartermaster's.
        MakeEntry(TEXT("SalvageTurnIn"), { KessSalvageAccepted, KessSalvageFeedstock }, { KessSalvageTurnedIn }),
        MakeEntry(TEXT("Warm"), { KessSalvageTurnedIn }),
        MakeEntry(TEXT("Returned"), { AskedKessAboutRior }),
    };
}

TArray<FBreakerDialogueNode> ABreakerNPC::MakeQuartermasterDialogue()
{
    using namespace BreakerQuestFlags;
    return
    {
        MakeNode(TEXT("Start"), TEXT("Ammo's on the shelf, gear's in the crates. You breaking things or buying things?"),
        {
            MakeChoice(TEXT("Show me what you've got."), TEXT("Vendor")),
            // Offering and accepting are two different flags on purpose: a
            // contract the player heard and walked away from is a different
            // state from one never mentioned, and the quest object reads both.
            MakeChoice(TEXT("Anything need doing around here?"), TEXT("Job"), FirstContractOffered, {}, { FirstContractAccepted }),
            MakeChoice(TEXT("Still working on that spill."), TEXT("Progress"), NAME_None, { FirstContractAccepted }, { FirstContractTurnedIn }),
            // The chain gates, each one line: an offer requires the PREVIOUS
            // quest's turn-in and blocks on its own acceptance, so the chain
            // order is authored exactly once per link.
            MakeChoice(TEXT("Got another contract?"), TEXT("Pattern"), PatternOffered, { KessSalvageTurnedIn }, { PatternAccepted }),
            MakeChoice(TEXT("Still counting your marked."), TEXT("PatternProgress"), NAME_None, { PatternAccepted }, { PatternTurnedIn }),
            MakeChoice(TEXT("What's this about a bad rift?"), TEXT("Deeper"), DeeperOffered, { PatternTurnedIn }, { DeeperAccepted }),
            MakeChoice(TEXT("Still sweeping the source."), TEXT("DeeperProgress"), NAME_None, { DeeperAccepted }, { DeeperTurnedIn }),
            MakeChoice(TEXT("[Leave] Just passing through.")),
        }),
        // O100. THE NODE ID AND THE FLAG ARE LOAD-BEARING and are deliberately
        // unchanged: BreakerQuestLoopTests walks this dialogue through
        // ValidateDialogue / GetVisibleChoices / ResolveStartNodeId, and
        // CheckedVendor is a registered quest flag two contracts read. The body
        // text and the first choice's destination are what moved.
        MakeNode(TEXT("Vendor"), TEXT("Requisitions, then. Anything your class is cleared for, I can sign out — one token a piece, and I don't take Riftglass for it. Rift work earns the tokens; I just hold the keys."),
        {
            MakeChoice(TEXT("Show me the requisition list."), NAME_None, CheckedVendor, {}, {},
                EBreakerDialogueAction::OpenQuartermaster),
            MakeChoice(TEXT("[Leave] I'll check back."), NAME_None, CheckedVendor),
        }),
        MakeNode(TEXT("Job"), TEXT("The spill out past the pad keeps regrouping. Thin it out, and put that elite down while you're at it. I count what comes back — that's the job."),
        {
            MakeChoice(TEXT("Consider it done."), NAME_None, FirstContractAccepted),
            MakeChoice(TEXT("[Leave] Not my problem yet.")),
        }),
        MakeNode(TEXT("Progress"), TEXT("Count's not right yet. The spill's still moving and that elite's still upright. Come back when both of those stop being true."),
        {
            MakeChoice(TEXT("[Leave] Working on it.")),
        }),
        // Entry target: the turn-in. Reached by walking up, not by hunting for
        // a menu option — that is what per-NPC entry state buys.
        MakeNode(TEXT("ReadyTurnIn"), TEXT("Count's right. Spill's thinned and the elite's down. You did the job and you came back to say so, which is rarer than the first part."),
        {
            MakeChoice(TEXT("That's the job."), NAME_None, FirstContractTurnedIn),
            MakeChoice(TEXT("[Leave] Later.")),
        }),
        MakeNode(TEXT("Done"), TEXT("Contract's closed and you've been paid. There'll be more — the spill always comes back. Ammo's still on the shelf."),
        {
            // Every closed-contract node routes back to Start, because the
            // entry overrides OPEN on these nodes: without the route, a player
            // who lands here could never reach the next offer.
            MakeChoice(TEXT("Anything else need doing?"), TEXT("Start")),
            MakeChoice(TEXT("[Leave] I'll be around.")),
        }),

        // ---- Q3: THE PATTERN ------------------------------------------------
        MakeNode(TEXT("Pattern"), TEXT("Second contract. The spill keeps regrouping — same ground, same hours — and the marked ones are up front now, every time. Three of them, down. I want to see if the next count sheet reads different."),
        {
            MakeChoice(TEXT("Consider it done."), NAME_None, PatternAccepted),
            MakeChoice(TEXT("[Leave] Not yet.")),
        }),
        MakeNode(TEXT("PatternProgress"), TEXT("Count's short. The marked don't fall easy — that's why the contract says three and not thirty. Come back when the sheet's full."),
        {
            MakeChoice(TEXT("[Leave] Working on it.")),
        }),
        // Her first unease. In HER register: no theory, no rift talk — a count
        // sheet that will not add up, filed under the only box it fits.
        MakeNode(TEXT("PatternTurnIn"), TEXT("Count's right. Three marked, three down. ...Off the record: spills drift. This one doesn't. Same ground, every time, like something's taking a measurement. The sheet doesn't have a box for that, so it's going down as weather."),
        {
            MakeChoice(TEXT("That's the job."), NAME_None, PatternTurnedIn),
            MakeChoice(TEXT("[Leave] Later.")),
        }),
        MakeNode(TEXT("PatternDone"), TEXT("Contract's closed, paid in full. Sheet still says weather. Weather doesn't hold formation."),
        {
            MakeChoice(TEXT("Anything else need doing?"), TEXT("Start")),
            MakeChoice(TEXT("[Leave] I'll be around.")),
        }),

        // ---- Q4: DEEPER -----------------------------------------------------
        // Seeds Act II in the only register that doesn't overreach: a rift
        // that "didn't close clean" — Command's words, which she repeats and
        // declines to interpret. Nothing is named.
        MakeNode(TEXT("Deeper"), TEXT("Last one on my sheet, and I don't love writing it. Command flagged a rift out past the far ground — didn't close clean, their words. Everything it lets through comes up marked. Sweep it: five of the marked, and whatever's biggest goes down first."),
        {
            MakeChoice(TEXT("Consider it done."), NAME_None, DeeperAccepted),
            MakeChoice(TEXT("[Leave] Not yet.")),
        }),
        MakeNode(TEXT("DeeperProgress"), TEXT("Sweep's not done. Five of the marked, and the count's honest or it's nothing. Same as always — I count what comes back."),
        {
            MakeChoice(TEXT("[Leave] Working on it.")),
        }),
        MakeNode(TEXT("DeeperTurnIn"), TEXT("Five down, and you walked back in to say so. Good count. ...That rift's still out there. Didn't close clean — Command's words, not mine. It's gone up the chain, and what goes up the chain comes back down with a name and a Breaker attached. Sleep while you can."),
        {
            MakeChoice(TEXT("That's the job."), NAME_None, DeeperTurnedIn),
            MakeChoice(TEXT("[Leave] Later.")),
        }),
        MakeNode(TEXT("DeeperDone"), TEXT("Sheet's clear. First time since you signed in. Restock while it lasts — when that name comes back down the chain, it'll be yours."),
        {
            MakeChoice(TEXT("Anything else need doing?"), TEXT("Start")),
            MakeChoice(TEXT("[Leave] I'll be around.")),
        }),
    };
}

TArray<FBreakerDialogueEntry> ABreakerNPC::MakeQuartermasterEntries()
{
    using namespace BreakerQuestFlags;
    return
    {
        // Most progressed first. The closed-contract greetings additionally
        // block on the NEXT quest being in play, so a mid-chain player gets
        // the ordinary Start (where the live progress line is) rather than a
        // stale closure line.
        MakeEntry(TEXT("DeeperDone"), { DeeperTurnedIn }),
        MakeEntry(TEXT("DeeperTurnIn"), { DeeperAccepted, DeeperSweepDone }, { DeeperTurnedIn }),
        MakeEntry(TEXT("PatternDone"), { PatternTurnedIn }, { DeeperOffered }),
        MakeEntry(TEXT("PatternTurnIn"), { PatternAccepted, PatternMarkedDown }, { PatternTurnedIn }),
        MakeEntry(TEXT("Done"), { FirstContractTurnedIn }, { PatternOffered }),
        MakeEntry(TEXT("ReadyTurnIn"), { FirstContractSpillThinned, FirstContractEliteDown }, { FirstContractTurnedIn }),
    };
}

ABreakerNPC* ABreakerNPC::SpawnForgeKeeper(UWorld* World, const FVector& Location, const FRotator& Rotation)
{
    ABreakerNPC* NPC = SpawnNPC(World, Location, Rotation);
    if (!NPC) return nullptr;
    NPC->DisplayName = FText::FromString(TEXT("KESS — FORGE KEEPER"));
    NPC->DialogueNodes = MakeForgeKeeperDialogue();
    NPC->EntryOverrides = MakeForgeKeeperEntries();
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
    NPC->DisplayName = FText::FromString(TEXT("QUARTERMASTER"));
    NPC->DialogueNodes = MakeQuartermasterDialogue();
    NPC->EntryOverrides = MakeQuartermasterEntries();
    // The Quartermaster wears the male base at a plain idle — the second of
    // the pack's two bodies, distinct from Kess at a glance.
    NPC->BodyMeshAsset = FSoftObjectPath(TEXT("/Game/Breaker/Meshes/npcs/Superhero_Male_FullBody/SkeletalMeshes/SuperHero_Male.SuperHero_Male"));
    NPC->BodyIdleAnimation = FSoftObjectPath(TEXT("/Game/Breaker/Meshes/npcs/Superhero_Male_FullBody/Anims/UAL1_Standard/SkeletalMeshes/UAL1_StandardIdle_Loop.UAL1_StandardIdle_Loop"));
    NPC->ApplyBodyMesh();
    return NPC;
}
