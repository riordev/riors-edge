#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Items/BreakerItemTypes.h"
#include "Save/BreakerQuestJournal.h"
#include "BreakerQuestContent.generated.h"

// THE FLAG REGISTRY.
//
// Flags are FName literals typed at the call site. They LOOK like GameplayTags
// (Quest.MetForgeKeeper) and are not: they are not registered anywhere, cannot
// be matched with a tag query, and a typo is a silent, permanent no-op —
// Campaign-And-Story.md 6.3 #6. Twenty-six missions' worth of hand-typed
// literals will produce that typo.
//
// Real GameplayTags were the other option and were NOT taken. Tags are a global
// engine registry that wants an .ini and a content pass, they cannot be added
// by a save file, and quest flags must survive a build that no longer declares
// them (the quarantine rule in the migration). A validated central list gets the
// typo caught at test time, which is the whole benefit, at none of that cost.
//
// Names below are FROZEN once shipped. Renaming one is a save migration, not an
// edit — see UBreakerSaveGame::MigrateQuestFlagsV1ToV2 for what that costs.
namespace BreakerQuestFlags
{
    // Anchor camp, shipped in version 1. Spellings preserved exactly so an
    // existing BreakerSave0 keeps meaning what it meant.
    RIORSEDGE_API extern const FName MetForgeKeeper;
    RIORSEDGE_API extern const FName AskedKessAboutRior;
    RIORSEDGE_API extern const FName CheckedVendor;

    // The Quartermaster's first contract. Quest.AcceptedFirstContract from
    // version 1 migrates into FirstContractAccepted.
    RIORSEDGE_API extern const FName FirstContractOffered;
    RIORSEDGE_API extern const FName FirstContractAccepted;
    RIORSEDGE_API extern const FName FirstContractSpillThinned;
    RIORSEDGE_API extern const FName FirstContractEliteDown;
    RIORSEDGE_API extern const FName FirstContractTurnedIn;

    // Progress counters. Not flags — intermediate state that SETS a flag when
    // it reaches its threshold (Campaign-And-Story.md 6.4).
    RIORSEDGE_API extern const FName FirstContractKillCounter;
    RIORSEDGE_API extern const FName FirstContractEliteCounter;

    // ACT I CHAIN, quests 2-4. Gating is dialogue-side: each quest's offer
    // choice requires the PREVIOUS quest's TurnedIn flag, so the chain order
    // lives in exactly one place per link and the quest definitions stay a
    // flat registry (Campaign-And-Story.md 6.4: flags are the state, quests
    // are a lens).
    //
    // Q2 — Kess's salvage. The Forge needs feedstock; the reward is the first
    // thing the Forge ever does FOR the player, which is how the relationship
    // thread opens (roster: Kess has the highest interaction count in the game).
    RIORSEDGE_API extern const FName KessSalvageOffered;
    RIORSEDGE_API extern const FName KessSalvageAccepted;
    RIORSEDGE_API extern const FName KessSalvageFeedstock;
    RIORSEDGE_API extern const FName KessSalvageTurnedIn;
    RIORSEDGE_API extern const FName KessSalvageKillCounter;

    // Q3 — the spill is not random. The Quartermaster's first unease, filed
    // as weather because her sheet has no box for what she actually thinks.
    RIORSEDGE_API extern const FName PatternOffered;
    RIORSEDGE_API extern const FName PatternAccepted;
    RIORSEDGE_API extern const FName PatternMarkedDown;
    RIORSEDGE_API extern const FName PatternTurnedIn;
    RIORSEDGE_API extern const FName PatternEliteCounter;

    // Q4 — the Act I capstone: a rift out past the far ground that "didn't
    // close clean" (Command's words, never hers). Seeds the Breach-to-come
    // without naming anything the Act II turn depends on.
    RIORSEDGE_API extern const FName DeeperOffered;
    RIORSEDGE_API extern const FName DeeperAccepted;
    RIORSEDGE_API extern const FName DeeperSweepDone;
    RIORSEDGE_API extern const FName DeeperTurnedIn;
    RIORSEDGE_API extern const FName DeeperEliteCounter;
}

// Derived, never stored. A quest's state is a pure function of the flag set,
// which is the promise that keeps the save format from forking: adding quests
// adds flags, never a second serialized state machine.
UENUM(BlueprintType)
enum class EBreakerQuestState : uint8
{
    NotOffered,
    Offered,
    Active,
    ReadyToTurnIn,
    Complete
};

// One objective. Completion is a FLAG; the counter is how a "kill 8" objective
// gets there. An objective with no counter is completed directly by whatever
// sets its flag (a conversation, a zone entry, a pickup).
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerQuestObjective
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ObjectiveId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Text;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName CompletionFlag = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ProgressCounter = NAME_None;
    // O2 PLACEHOLDER. Zero means the objective is not counted.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) int32 RequiredCount = 0;
    // Only kills of this rank advance the counter, when the objective is one
    // the kill tracker feeds. Trash counts every rank at or above it.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bRequiresEliteKill = false;
};

// What turning a quest in pays. Deliberately thin: gear is the entire endgame
// (Save-Architecture 1), so a contract pays gear rather than inventing a
// quest-only currency.
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerQuestReward
{
    GENERATED_BODY()

    // O2 PLACEHOLDER.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) int32 ItemCount = 1;
    // O2 PLACEHOLDER.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EBreakerItemRarity MinimumRarity = EBreakerItemRarity::Exceptional;
    // O2 PLACEHOLDER. Quests are camp content at area level 1 today; when the
    // campaign authors real areas this should follow the area's level the way
    // GetDropItemLevel does.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1")) int32 ItemLevel = 1;
};

// A QUEST IS A LAYER OVER FLAGS, NOT A PARALLEL SYSTEM.
//
// Every field below is either content (text, ids) or a reference to a flag.
// Nothing here is serialized into the save: the save stores flags and counters,
// and this definition is the lens that reads them back as a quest. That is
// Campaign-And-Story.md 6.3 #3's explicit requirement and 8.5's ruling.
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerQuestDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName QuestId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Title;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Giver;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName OfferedFlag = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName AcceptedFlag = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName TurnedInFlag = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FBreakerQuestObjective> Objectives;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FBreakerQuestReward Reward;
};

UCLASS()
class RIORSEDGE_API UBreakerQuestLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // Zero-setup convention: the slice plays with no content assets, so the
    // quest list is a C++ fallback registry exactly like the tree, affix,
    // ability and class registries. Data Assets replace it one-for-one later.
    static const TArray<FBreakerQuestDefinition>& GetFallbackQuests();
    static bool FindQuest(FName QuestId, FBreakerQuestDefinition& OutQuest);

    // Pure. No world, no character, no journal object — just flags in, state
    // out, so the whole quest layer is unit-testable.
    UFUNCTION(BlueprintPure, Category="Quest")
    static EBreakerQuestState ComputeQuestState(const FBreakerQuestDefinition& Quest, const FBreakerQuestFlagSet& Flags);

    UFUNCTION(BlueprintPure, Category="Quest")
    static bool AreAllObjectivesComplete(const FBreakerQuestDefinition& Quest, const FBreakerQuestFlagSet& Flags);

    // The gate every dialogue condition runs through: required flags are ALL,
    // blocked flags are ANY. Empty lists pass.
    UFUNCTION(BlueprintPure, Category="Quest")
    static bool PassesFlagConditions(const TArray<FName>& RequiredFlags, const TArray<FName>& BlockedByFlags, const FBreakerQuestFlagSet& Flags);

    // Every flag this build knows about. The registry from the header comment.
    static const TArray<FName>& GetRegisteredFlags();
    static bool IsRegisteredFlag(FName Flag);

    // Fails when quest content or fallback dialogue references a flag that is
    // not registered — i.e. when someone typed one. Automation calls it; that
    // is what turns a silent no-op into a red test.
    static bool ValidateQuestContent(FString& OutError);

    // Advances every counted objective of every ACTIVE quest for one kill.
    // Returns the number of objectives this kill completed. The one place that
    // turns a combat event into campaign state (Campaign-And-Story.md 6.3 #4).
    static int32 NotifyEnemyKilled(UBreakerQuestJournal& Journal, bool bEliteOrAbove);
};
