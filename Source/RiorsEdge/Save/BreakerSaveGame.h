#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Items/BreakerForgeLibrary.h"
#include "Items/BreakerItemTypes.h"
#include "Progression/BreakerProgressionTypes.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "BreakerSaveGame.generated.h"

// THE PLAYER'S BODY (O14): both models are real and a character picks one at
// creation. Serialized BY VALUE in the character save, so this enum is
// APPEND-ONLY FOREVER — rename is safe; insert, reorder or reuse a value and
// every existing character silently wears the wrong body. Human = 0, Effigy
// = 1 are pinned by the appearance round-trip test.
UENUM(BlueprintType)
enum class EBreakerPlayerModel : uint8
{
    Human,
    Effigy
};

// THE PLAYER'S VOICE. Same append-only rule as the model. The three-word
// vocabulary is O2 PLACEHOLDER until a voice line exists to be heard; Mid is
// the default because it is the one a migrated character never chose.
// Low = 0, Mid = 1, Dry = 2, pinned by the same test.
UENUM(BlueprintType)
enum class EBreakerPlayerVoice : uint8
{
    Low,
    Mid,
    Dry
};

// Everything a character carries between sessions. Stable ids and rolled
// numbers only — never pointers or calculated attribute totals.
UCLASS()
class RIORSEDGE_API UBreakerSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    static const TCHAR* DefaultSlotName() { return TEXT("BreakerSave0"); }

    UPROPERTY() FBreakerProgressionState Progression;
    UPROPERTY() TArray<FBreakerItemInstance> EquippedItems;
    UPROPERTY() TArray<FBreakerItemInstance> BackpackItems;
    UPROPERTY() EBreakerWeaponArchetype SlotOneArchetype = EBreakerWeaponArchetype::Rifle;
    UPROPERTY() EBreakerWeaponArchetype SlotTwoArchetype = EBreakerWeaponArchetype::Shotgun;
    UPROPERTY() TArray<FName> QuestFlags;
    // Objective progress counters. Additive since version 1: a v1 file has no
    // such property, so deserialization leaves it empty, which is exactly the
    // correct value for a save written before any objective could be counted.
    UPROPERTY() TMap<FName, int32> QuestCounters;
    // Additive exploration state: older saves begin with no discovered sites.
    UPROPERTY() TArray<FName> DiscoveredMapSites;
    UPROPERTY() FName TrackedMapSite;
    // The Forge's crafting wallet (Items/BreakerForgeLibrary.h). Additive
    // since version 2, the same discipline QuestCounters used going into
    // version 2: a v2 file has no such property, so it deserializes to the
    // struct's own default-constructed zero wallet, which is exactly correct
    // for a save written before wallet persistence existed. A v3 file carries
    // the old three-denomination Slag/Flux/Sigil array; the v3 -> v4 step
    // folds it into the one Riftglass balance (owner ruling 2026-08-16, one
    // crafting currency) at the conversion stated in
    // FBreakerForgeWallet::CollapseLegacyDenominations.
    UPROPERTY() FBreakerForgeWallet ForgeWallet;
    // THE FOLD RECEIPT (O51, Save/BreakerRiftglassFold.h). True once this
    // file's balance has been zeroed on the way to the account; ForgeWallet
    // is dead weight from then on and is written as zero. Additive since
    // version 7: a v7 file deserializes false, which is the truth — it has
    // not been folded. A FIELD rather than the version number, because
    // MigrateToCurrent bumps the version in memory before the fold reads
    // the balance, so the version cannot double as the receipt.
    UPROPERTY() bool bRiftglassFoldedToAccount = false;
    // The character's own id, carried in the payload (approved with One-AD's
    // fold ruling). The slot name held it alone before; a payload that knows
    // whose it is lets the stash journal's claim release name its claimant.
    // Additive since version 7: an unset guid is a pre-v8 file. Invalid on
    // the legacy single slot, which has no roster row.
    UPROPERTY() FGuid CharacterId;
    // APPEARANCE (O14): the model, the voice and which of the five face
    // tiles the character wears. Chosen once at creation; never derived.
    // Additive since version 8: a v8 file has none of the three properties,
    // so it deserializes to Human / Mid / tile 0, which is the body every
    // pre-v9 character was drawn with. FaceIndex is 0..4 — five tiles, the
    // count O2 PLACEHOLDER until the tiles are drawn; nothing here clamps
    // it, because the creation screen is the only writer and the save
    // records what it was handed.
    UPROPERTY() EBreakerPlayerModel Model = EBreakerPlayerModel::Human;
    UPROPERTY() EBreakerPlayerVoice Voice = EBreakerPlayerVoice::Mid;
    UPROPERTY() uint8 FaceIndex = 0;
    // Version 1 shipped. Version 2 renamed the first-contract flag into the
    // Quest.FirstContract.* family and added QuestCounters. Version 3 added
    // ForgeWallet. Version 4 collapsed the wallet's three denominations into
    // the single Riftglass balance.
    //
    // Before this pass the field was DECORATION: declared here and in two other
    // structs, read by nothing, with no migration branch anywhere. That is
    // worse than having no version at all, because it implies a guarantee that
    // does not exist — the first additive change would have been misread in
    // silence. MigrateToCurrent is what makes the number mean something.
    // Version 5 split the class kit into free starters and token-bought
    // unlockables (O100); the step hands every pre-v5 character the kit it
    // could already reach and stamps its token counter so it is not paid
    // retroactively for abilities it already has. Version 8 added the fold
    // receipt and the character id, both additive; the Riftglass fold itself
    // is NOT a migration step, because it writes two files and a pure
    // in-memory step cannot — see Save/BreakerRiftglassFold.h. Version 9
    // added Model, Voice and FaceIndex, additive.
    static constexpr int32 CurrentSaveVersion = 9;

    UPROPERTY() int32 SaveVersion = 1;
    // Missing fields must deserialize as legacy even after activation. Writers
    // must stamp ActiveCoreLayoutVersion when the new roster is enabled.
    UPROPERTY() int32 CoreLayoutVersion = 1;
    static constexpr int32 ActiveCoreLayoutVersion = 2;
    static bool MigrateCoreLayout(UBreakerSaveGame& Save, int32 TargetVersion, FString& OutNote);
    static int32 LegacyCoreRankCost(FName NodeId);

    // The v4 -> v5 step, exposed so the suite can prove it in isolation like
    // MigrateQuestFlagsV1ToV2. Pure on the struct: no world, no slot, no class
    // definition read at load time.
    static void MigrateAbilityUnlocksV4ToV5(FBreakerProgressionState& Progression);
    // O111. Refunds nothing and reads nothing -- see the definition for why the
    // fifteen retiring branch ids are frozen literals rather than a library call.
    static void MigrateClassCurrencyV5ToV6(FBreakerProgressionState& Progression);
    static void MigrateDoctrineEntitlementV6ToV7(FBreakerProgressionState& Progression);

    // Brings a deserialized payload up to CurrentSaveVersion IN MEMORY, in
    // version order, one step at a time (Save-Architecture 5.2 — never a switch
    // on "old vs new"). Returns false only for a file from a NEWER build, which
    // is refused rather than repaired or overwritten. Pure on the struct: no
    // world, no slot, no engine state, so the automation suite can prove every
    // step. The migrated payload is written back on the next normal save, not
    // eagerly.
    static bool MigrateToCurrent(UBreakerSaveGame& Save, FString& OutNote);

    // The rename step, exposed so it can be tested and reused: applies the
    // v1 -> v2 flag remap in place. Unknown flags are PRESERVED VERBATIM, never
    // dropped — a save touched by a branch build must survive coming back.
    static bool MigrateQuestFlagsV1ToV2(TArray<FName>& Flags);
};
