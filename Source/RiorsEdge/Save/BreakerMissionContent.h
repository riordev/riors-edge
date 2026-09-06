#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BreakerMissionContent.generated.h"

// THE STORY MISSIONS, AS DATA (O186).
//
// A mission is a chapter of the main story: an ordered list of beats under
// one id, tied to the quests it advances. No beat carries text — the title
// lives on the quest row and the line on the dialogue node (O195). A beat
// completes when its flag is set, and every gate is derived from beat order,
// never authored on the beat.
//
// Missions, quests and dialogue share ONE flag registry: Data/quests.json's
// "flags", plus the arrival flag this loader registers per mission
// (Mission.<missionId>.Arrived). A beat's completesOn must be in one of the
// two, or the file does not load.

// The closed beat vocabulary. Written to the data file BY NAME, so the order
// here is not serialized; it is still append-only so a kind never changes
// meaning under a file that names it.
UENUM()
enum class EBreakerMissionBeatKind : uint8
{
    // An NPC and an entry node; completes on the choice's flag.
    Dialogue,
    // A destination in the travel registry; completes on arrival.
    Travel,
    // A rift by id, with the quest objectives it counts; completes on their
    // flags.
    Encounter,
    // A rift and a boss; completes on the rift's completion flag.
    Boss,
    // A dialogue whose flag is the quest's turn-in.
    Return,
    // The quest's reward, paid at turn-in.
    Reward,
    // Doctrine points, a Core world point by source id, or an ability token.
    Unlock
};

// One beat. Every beat carries an id and a kind; the rest is the kind's own
// fields and nothing else — the exporter writes exactly the fields the kind
// has, and the loader refuses a field the kind does not own.
struct FBreakerMissionBeat
{
    FName BeatId = NAME_None;
    EBreakerMissionBeatKind Kind = EBreakerMissionBeatKind::Dialogue;

    // Dialogue, Return.
    FName Npc = NAME_None;
    FName Node = NAME_None;
    // Dialogue, Travel, Boss, Return.
    FName CompletesOn = NAME_None;
    // Travel.
    FName Destination = NAME_None;
    // Encounter, Boss.
    FName Rift = NAME_None;
    // Encounter, Reward.
    FName Quest = NAME_None;
    // Encounter.
    TArray<FName> Objectives;
    // Boss. "" until the owner names it; the loader counts an unnamed boss as
    // a warning, not an error.
    FName Boss = NAME_None;
    // Unlock: exactly one of the three.
    int32 DoctrinePoints = 0;
    FName CorePoint = NAME_None;
    FName AbilityToken = NAME_None;
};

// A rift by id, resolving to the yard's rift definition
// (UBreakerZoneBuilder::FernhallRiftFor). NAME_None is the entry yard.
struct FBreakerMissionRift
{
    FName RiftId = NAME_None;
    FName Yard = NAME_None;
};

struct FBreakerMissionDefinition
{
    FName MissionId = NAME_None;
    int32 Act = 1;
    // Owner input; "" until written.
    FString Title;
    TArray<FName> Quests;
    TArray<FBreakerMissionBeat> Beats;
};

// Everything Data/missions.json holds, as the loader read it, plus what the
// loader derived from it: the arrival flags it registers and the warnings it
// counted. The census re-exports Rifts and Missions and
// RiorsEdge.Data.Missions.Fresh pins the committed file to that export.
struct FBreakerMissionData
{
    TArray<FBreakerMissionRift> Rifts;
    TArray<FBreakerMissionDefinition> Missions;
    // Mission.<missionId>.Arrived, one per mission, in file order.
    TArray<FName> Flags;
    // Gaps the file records rather than fakes: a Boss beat whose boss is not
    // yet named. Zero of these is the shipped configuration's goal, not its
    // present state.
    TArray<FString> Warnings;
};

UCLASS()
class RIORSEDGE_API UBreakerMissionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    static FString DataRelativePath();

    // Every complaint the loader raised, or empty. A file that fails
    // validation loads as EMPTY behind an ensure, never as a nearest fit.
    static const TArray<FString>& GetDataErrors();
    static const TArray<FString>& GetDataWarnings();

    static const TArray<FBreakerMissionDefinition>& GetMissions();
    static const TArray<FBreakerMissionRift>& GetRifts();

    // The arrival flags this loader registers. A Travel beat completes on
    // Mission.<missionId>.Arrived; the seam that sets it is the travel point's.
    static const TArray<FName>& GetRegisteredMissionFlags();
    static bool IsRegisteredMissionFlag(FName Flag);
    static FName ArrivedFlagFor(FName MissionId);

    // The whole read: text in, data out, every complaint collected. Structural
    // checks (fields present, kinds known, ids unique) and the cross-file
    // checks (flags registered, npc/node/rift/quest/objective ids resolve,
    // beats ordered, the point budgets) run in one pass so a broken file
    // names all of its breaks. Public so a test can feed a bad file through
    // the same door the loader uses. Returns true when OutErrors is empty.
    static bool ParseMissionsJson(const FString& Json, FBreakerMissionData& Out, TArray<FString>& OutErrors);

    // Fails when this file, the quest registry or the dialogue did not load
    // clean. Automation calls it.
    static bool ValidateMissionContent(FString& OutError);
};
