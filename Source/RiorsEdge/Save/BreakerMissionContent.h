#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Save/BreakerQuestJournal.h"
#include "BreakerMissionContent.generated.h"

struct FBreakerRiftDefinition;

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
    // Encounter only: a dedicated field encounter, mutually exclusive with Rift.
    FName WorldEncounter = NAME_None;
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
    // Stable main-story benchmark identity, required only for Doctrine grants.
    FName Benchmark = NAME_None;
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

    // ---- PROGRESS, AS PURE FUNCTIONS OVER THE JOURNAL'S FLAGS -------------
    // A mission's position is derived from the flag set every time it is
    // asked, never stored: the save holds flags and counters, and these are
    // the lens that reads them back as a place in the story. World-free so
    // the rule is tested on a bare FBreakerQuestFlagSet.

    // The flags a beat completes on. Dialogue, Travel, Boss and Return carry
    // theirs; an Encounter completes on every named objective's completion
    // flag; a Reward completes on its quest's turn-in flag (the Return before
    // it sets the same flag, so a Reward is done the moment its Return is); an
    // Unlock carries none and is complete the moment it is reached.
    static TArray<FName> BeatCompletionFlags(const FBreakerMissionBeat& Beat);

    // The length of the leading run of beats whose flags are all held. A beat
    // completed out of order does not count until every beat before it is
    // done -- order is the gate (O186), and a flag set early waits for it.
    static int32 BeatsCompleted(const FBreakerMissionDefinition& Mission, const FBreakerQuestFlagSet& Flags);
    // The first beat not yet complete, or null once the mission is done.
    static const FBreakerMissionBeat* CurrentBeat(const FBreakerMissionDefinition& Mission, const FBreakerQuestFlagSet& Flags);

    // THE TRACKER LINE: the one string the HUD's quest corner draws for a
    // beat, built from the rows and nothing else (O195: one home per string).
    // The two verbs are the tracker's own words and live here so the HUD
    // reads them rather than owning a copy. TCHAR arrays, not pointers,
    // because FString::Printf holds its format to an array type.
    //   Dialogue  -> SpeakToVerb over the beat's NPC when it accepts a quest.
    //   Return    -> ReturnToVerb over the beat's NPC when it turns in a quest.
    //                Acceptance and turn-in can happen at different people.
    //   Encounter -> the first of the beat's objectives (beat order) the flag
    //                set does not hold: its text, with "  n/N" when counted.
    //   Boss      -> the objective whose CompletionFlag the beat completes on,
    //                the same way; else the rift's area name.
    //   Travel    -> the destination's display name.
    //   Reward, Unlock -> empty: nothing is asked of the player.
    // Every name is upper-cased here, once. A Dialogue or Return matching no
    // quest by flag answers the dialogue row's display name with no verb.
    static constexpr TCHAR SpeakToVerb[] = TEXT("SPEAK TO THE %s");
    static constexpr TCHAR ReturnToVerb[] = TEXT("RETURN TO THE %s");
    static FString TrackerLine(const FBreakerMissionBeat& Beat, const FBreakerQuestFlagSet& Flags);

    // O43: doctrine points are granted by mission beats, not by level. The
    // sum of doctrinePoints over every reached Unlock beat across every
    // mission; the component settles it against LevelDoctrinePointsGranted.
    static int32 DoctrinePointEntitlement(const FBreakerQuestFlagSet& Flags);

    // THE TWO SEAMS. Both gate on the beat being CURRENT -- the same rule
    // UBreakerQuestLibrary::NotifyEnemyKilled applies to an objective: work
    // done before the story asks for it does not pre-complete the ask.
    // Arriving at a destination completes a Travel beat to it that is current
    // now; finishing a rift completes a Boss beat in that rift that is current
    // now. Each returns the flags to set, possibly several when more than one
    // mission is waiting on the same event, and the caller sets them.
    static TArray<FName> ArrivalFlagsFor(FName DestinationId, const FBreakerQuestFlagSet& Flags);
    static TArray<FName> WorldEncounterCompletionFlagsFor(FName EncounterId, const FBreakerQuestFlagSet& Flags);
    // Match the authored yard's stable EncounterId. Display names are copy,
    // and unnamed/dev definitions cannot satisfy a story encounter.
    static TArray<FName> RiftCompletionFlagsFor(const FBreakerRiftDefinition& Rift, const FBreakerQuestFlagSet& Flags);
    // Authored identity of a rift's boss, independent of whether the player
    // has reached its story beat. Re-clears fight the same boss.
    static FName BossForRift(const FBreakerRiftDefinition& Rift);
};
