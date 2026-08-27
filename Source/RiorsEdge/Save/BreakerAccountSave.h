#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Items/BreakerItemTypes.h"
#include "BreakerAccountSave.generated.h"

// ---------------------------------------------------------------------------
// THE ACCOUNT SAVE — the third save object, sibling to the roster and the
// per-character saves, in its own slot (Part One-X's architecture: not inside
// the roster, whose own header forbids deserializing inventories to list
// characters; not inside a character save, which is what account-wide
// excludes). O17 is the philosophy: characters are builds, the account is
// the player.
//
// What lives here and why:
//  * HighestClearedAreaLevel (Part One-Z item 2) — the ladder's memory. An
//    ACCOUNT record because it decides the alt question, and because a
//    character record would make every alt re-earn what the player already
//    reached. Written by the first-clear rule (One-AA: a first clear pays
//    the LADDER, a re-clear pays the LOOT), read by everything that gates
//    on how far the account has been.
//  * The stash and the account Riftglass join in One-X's commit, beside the
//    transfer journal. The object exists first so the record does not wait
//    on the stash's larger surface.
//
// LOADING mirrors the roster exactly, including the newer-version refusal:
// silently downgrading a file this build does not understand is how a player
// loses an account.
// ---------------------------------------------------------------------------
UCLASS()
class RIORSEDGE_API UBreakerAccountSave : public USaveGame
{
    GENERATED_BODY()

public:
    static FString AccountSlotName() { return TEXT("BreakerAccount"); }
    static constexpr int32 CurrentAccountVersion = 1;

    // Loads the account save, creating a fresh one when none exists; returns
    // null only for the newer-version refusal. Cached per process so every
    // reader sees one object — two loaded copies of the account is the
    // two-owners-one-question failure as data.
    static UBreakerAccountSave* LoadOrCreate();
    // Drops the process cache. Tests use it so one test's account cannot
    // leak into the next; nothing in the game calls it.
    static void ResetCacheForTesting();
    // Installs a transient account as the process's one account. Tests pair
    // it with bNeverPersist so the suite can exercise the record and the
    // first-clear rule WITHOUT touching the machine's real account slot —
    // the never-touch-a-save discipline, applied to the account file.
    static void InjectForTesting(UBreakerAccountSave* Account);

    bool SaveAccount() const;

    UPROPERTY() int32 AccountVersion = CurrentAccountVersion;

    // Test-injected accounts set this so SaveAccount is a loud no-op: a
    // transient account that wrote the real slot would be the probe-save bug
    // wearing a test harness.
    UPROPERTY(Transient) bool bNeverPersist = false;

    // The ladder's memory (Part One-Z item 2). 0 = nothing cleared.
    UPROPERTY() int32 HighestClearedAreaLevel = 0;

    // ---- THE STASH (Part One-X) ------------------------------------------
    // A TRANSFER POINT, NOT A WAREHOUSE — the backpack is unbounded, so an
    // uncapped stash would be a second pile with a screen between them. The
    // cap is what makes putting something in a decision. RULED at 70
    // (One-AB, from the owner's own number): 8.75 sets of eight slots —
    // 1.75 per character across five, a transfer point rather than a
    // warehouse. O2 PLACEHOLDER still, like every tunable.
    static constexpr int32 StashCapacity = 70;

    UPROPERTY() TArray<FBreakerItemInstance> StashItems;

    // THE TRANSFER JOURNAL — the design, not a detail (One-X). Two save
    // files, no atomic write across them; the account file is the commit
    // point and every crash window resolves at the next RestoreState:
    //  * DEPOSIT: the item enters StashItems AND its guid enters
    //    PendingRemovals, one write; the runtime backpack drops it; the
    //    character save writes whenever it writes. A crash in between
    //    leaves the guid journaled, and any later restore whose backpack
    //    still carries it DROPS that copy — the stash is authoritative —
    //    and clears the entry. Never duplicates.
    //  * WITHDRAWAL: the item never leaves StashItems at withdraw time — it
    //    is CLAIM-MARKED in PendingWithdrawals (refusing display/second
    //    withdrawal) while the runtime backpack takes a copy. A restore
    //    whose backpack carries the guid proves the withdrawal reached a
    //    save: the stash copy is then removed and the claim cleared. A
    //    crash before the character save leaves the claim standing and the
    //    item locked-but-present — never lost, never duplicated; the
    //    residual is a stuck claim, and the clean release (knowing WHICH
    //    character restored empty-handed) needs the character id in the
    //    save payload, which joins the SaveVersion bump the Riftglass fold
    //    already owes. Recorded, not hidden.
    //  * BOTH MARKS AT ONCE (deposited then withdrawn before any restore):
    //    without ids a sighting cannot say whose save it is, so the
    //    reconcile consumes deposit-first — the first sighting drops the
    //    possibly-stale copy and clears the removal mark while claim and
    //    stash copy stand; only the next sighting proves the withdrawal.
    //    Conservative on purpose: the alternative re-opens the duplication
    //    window, and the worst case here is one extra withdrawal.
    UPROPERTY() TArray<FGuid> PendingRemovals;
    UPROPERTY() TArray<FGuid> PendingWithdrawals;

private:
    static UBreakerAccountSave* CachedAccount;
};
