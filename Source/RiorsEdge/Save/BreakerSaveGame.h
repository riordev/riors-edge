#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Items/BreakerForgeLibrary.h"
#include "Items/BreakerItemTypes.h"
#include "Progression/BreakerProgressionTypes.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "BreakerSaveGame.generated.h"

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
    // The Forge's scalar crafting currencies (Items/BreakerForgeLibrary.h;
    // O12). Additive since version 2, the same discipline QuestCounters used
    // going into version 2: a v2 file has no such property, so it
    // deserializes to the struct's own default-constructed zero wallet, which
    // is exactly correct for a save written before wallet persistence
    // existed. Replicated on UBreakerEquipmentComponent but never round-
    // tripped through this save before version 3 — every Slag/Flux/Sigil
    // balance a player had earned was gone the moment the process restarted.
    UPROPERTY() FBreakerForgeWallet ForgeWallet;
    // Version 1 shipped. Version 2 renamed the first-contract flag into the
    // Quest.FirstContract.* family and added QuestCounters. Version 3 added
    // ForgeWallet.
    //
    // Before this pass the field was DECORATION: declared here and in two other
    // structs, read by nothing, with no migration branch anywhere. That is
    // worse than having no version at all, because it implies a guarantee that
    // does not exist — the first additive change would have been misread in
    // silence. MigrateToCurrent is what makes the number mean something.
    static constexpr int32 CurrentSaveVersion = 3;

    UPROPERTY() int32 SaveVersion = 1;

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
