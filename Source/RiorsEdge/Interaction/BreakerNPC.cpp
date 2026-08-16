#include "Interaction/BreakerNPC.h"

#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Save/BreakerQuestContent.h"

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
        std::initializer_list<FName> RequiredFlags = {}, std::initializer_list<FName> BlockedByFlags = {})
    {
        FBreakerDialogueChoice Choice;
        Choice.Text = Text;
        Choice.NextNodeId = NextNodeId;
        Choice.SetsQuestFlag = SetsQuestFlag;
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
            MakeChoice(TEXT("Show me what you've got. (Vendor — coming soon)"), TEXT("Vendor")),
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
        MakeNode(TEXT("Vendor"), TEXT("Storefront's not built yet, Breaker. The rift schedule says soon. Everything comes through the rift schedule eventually."),
        {
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
    return NPC;
}

ABreakerNPC* ABreakerNPC::SpawnQuartermaster(UWorld* World, const FVector& Location, const FRotator& Rotation)
{
    ABreakerNPC* NPC = SpawnNPC(World, Location, Rotation);
    if (!NPC) return nullptr;
    NPC->DisplayName = FText::FromString(TEXT("QUARTERMASTER"));
    NPC->DialogueNodes = MakeQuartermasterDialogue();
    NPC->EntryOverrides = MakeQuartermasterEntries();
    return NPC;
}
