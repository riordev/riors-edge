#include "Save/BreakerSaveGame.h"

#include "Progression/BreakerProgressionLibrary.h"

namespace
{
    // MIGRATION LITERALS ARE FROZEN. They deliberately do not reference the
    // named constants in BreakerQuestContent.h: a migration describes what a
    // file written in the PAST contains, and if it followed a constant that
    // gets renamed again the step would quietly stop matching anything.
    const FName V1AcceptedFirstContract(TEXT("Quest.AcceptedFirstContract"));
    const FName V2FirstContractAccepted(TEXT("Quest.FirstContract.Accepted"));
    const FName V2FirstContractOffered(TEXT("Quest.FirstContract.Offered"));

    // THE v5 KIT SNAPSHOT, AND IT IS FROZEN FOREVER. Same rule as the flag
    // literals above, and it bites harder here.
    //
    // A v4 save predates ability unlocks, so every class ability it could reach
    // was free — these lists are what "everything you had" MEANT at v4, and the
    // step exists to hand it back rather than silently take it away.
    //
    // DO NOT READ THESE FROM UBreakerClassDefinition. A migration that reads
    // live content depends on the build date rather than on the file: the day
    // Swift's three missing abilities land, the same v4 save would arrive with
    // four unlocks or one depending on when it was loaded — and the migration
    // test would pass either way, because it reads the same live definition the
    // step does. Frozen literals are what make the step provable, which is the
    // whole property MigrateToCurrent exists to have.
    //
    // ONE DELIBERATE INEXACTNESS: Swift's v4 list held only its two starters,
    // so Swift.CadenceBreak was not "previously available" — it was registered,
    // offered and permanently refused, the defect O100 deletes. It is granted
    // here anyway. Granting is not the failure this step guards against; taking
    // an equipped ability away from a character that has it is.
    struct FV5KitSnapshot { EBreakerClassId ClassId; TArray<FName> AbilityIds; };
    const TArray<FV5KitSnapshot>& V4ToV5UnlockedKits()
    {
        static const TArray<FV5KitSnapshot> Kits = {
            { EBreakerClassId::Swift, { TEXT("Swift.Skim"), TEXT("Swift.Lead"), TEXT("Swift.CadenceBreak") } },
            { EBreakerClassId::Caster, { TEXT("Caster.Cleave"), TEXT("Caster.Rot"), TEXT("Caster.Closequarter"),
                                         TEXT("Caster.Siphon"), TEXT("Caster.Fracture"), TEXT("Caster.Resonance") } },
            { EBreakerClassId::Gunsmith, { TEXT("Gunsmith.SidearmRig"), TEXT("Gunsmith.Turret"), TEXT("Gunsmith.Overhaul"),
                                           TEXT("Gunsmith.AmmoCrate"), TEXT("Gunsmith.MineCluster"), TEXT("Gunsmith.Disruptor") } },
            { EBreakerClassId::Tank, { TEXT("Tank.Rend"), TEXT("Tank.AnchorPoint"), TEXT("Tank.Bloodline"),
                                       TEXT("Tank.Provoke"), TEXT("Tank.BreachCharge"), TEXT("Tank.GroundZero") } },
            { EBreakerClassId::Support, { TEXT("Support.Patch"), TEXT("Support.Mark"), TEXT("Support.Purge"),
                                          TEXT("Support.Cadence"), TEXT("Support.Metronome"), TEXT("Support.Suppress") } },
        };
        return Kits;
    }

    // The unlockable COUNT each class had at v5, frozen for the same reason:
    // the granted counter has to be stamped to what this build would have paid
    // at that level, and reading the live definition would make the stamp move
    // under a content change.
    int32 V5UnlockableCount(EBreakerClassId ClassId)
    {
        return ClassId == EBreakerClassId::Swift ? 1 : 4;
    }
}

bool UBreakerSaveGame::MigrateQuestFlagsV1ToV2(TArray<FName>& Flags)
{
    bool bChanged = false;

    // 1. Rename. v1 spelled the contract flag outside the family it belongs to
    //    (Quest.AcceptedFirstContract vs the Quest.FirstContract.* the quest
    //    object now derives its state from). Without this remap an existing
    //    player who accepted the contract would be offered it again — the save
    //    would load without error and mean the wrong thing, which is the exact
    //    failure an unread version field invites.
    for (FName& Flag : Flags)
    {
        if (Flag == V1AcceptedFirstContract)
        {
            Flag = V2FirstContractAccepted;
            bChanged = true;
        }
    }

    // 2. Backfill the prerequisite. Quest state is DERIVED from flags, and the
    //    derivation reads Offered before Accepted; a v1 save records only the
    //    acceptance. Nobody can accept a contract that was never offered, so
    //    the missing flag is recoverable rather than lost.
    if (Flags.Contains(V2FirstContractAccepted) && !Flags.Contains(V2FirstContractOffered))
    {
        Flags.Add(V2FirstContractOffered);
        bChanged = true;
    }

    // Every other flag is carried through untouched, including any this build
    // does not recognise.
    return bChanged;
}

void UBreakerSaveGame::MigrateAbilityUnlocksV4ToV5(FBreakerProgressionState& Progression)
{
    // A v4 file has no UnlockedAbilityIds property, so deserialization left the
    // array empty — and empty means "this character has bought nothing", which
    // for a character that could reach its whole kit is valid data that now
    // means something else. That is the append-only failure exactly, and it is
    // why this step exists rather than trusting the default.
    for (const FV5KitSnapshot& Kit : V4ToV5UnlockedKits())
    {
        if (Kit.ClassId != Progression.PermanentClass) continue;
        for (const FName AbilityId : Kit.AbilityIds)
        {
            Progression.UnlockedAbilityIds.AddUnique(AbilityId);
        }
        break;
    }

    // AND STAMP THE COUNTER. Without this the character is paid the full token
    // entitlement for its level against a kit that is already entirely
    // unlocked, so it holds tokens it can never spend — a number on a screen
    // that does nothing, which is the same defect in a different costume.
    // A character with no class yet has bought nothing and is owed nothing.
    if (Progression.PermanentClass != EBreakerClassId::None)
    {
        Progression.AbilityTokensGranted = UBreakerProgressionLibrary::AbilityTokenEntitlement(
            Progression.CharacterLevel, V5UnlockableCount(Progression.PermanentClass));
    }
}

bool UBreakerSaveGame::MigrateToCurrent(UBreakerSaveGame& Save, FString& OutNote)
{
    OutNote.Reset();

    if (Save.SaveVersion > CurrentSaveVersion)
    {
        OutNote = FString::Printf(
            TEXT("save version %d is newer than this build's %d; this character was saved by a newer version"),
            Save.SaveVersion, CurrentSaveVersion);
        return false;
    }

    // A file written before the field existed deserializes to the default 1,
    // and a hand-edited 0 or negative means the same thing: the oldest shape we
    // know how to read. Clamp rather than refuse — refusing here would strand a
    // player over a number they cannot see.
    if (Save.SaveVersion < 1) Save.SaveVersion = 1;

    int32 Steps = 0;
    while (Save.SaveVersion < CurrentSaveVersion)
    {
        switch (Save.SaveVersion)
        {
        case 1:
            MigrateQuestFlagsV1ToV2(Save.QuestFlags);
            // QuestCounters is additive; an empty map is the correct value for
            // a file written before any objective could be counted.
            break;
        case 2:
            // ForgeWallet is additive, exactly like QuestCounters at the v1 ->
            // v2 step: a v2 file has no such property, so deserialization
            // already left it at the struct's own default-constructed zero
            // wallet, which is exactly correct for a save written before
            // wallet persistence existed. Nothing to transform.
            break;
        case 3:
            // The one-currency consolidation (owner ruling 2026-08-16): a v3
            // file's wallet holds Slag/Flux/Sigil in the legacy Amounts array.
            // Fold them into Riftglass at the conversion stated in
            // CollapseLegacyDenominations (1/6/60 — total value preserved). A
            // v2-or-older file arrives here with an empty legacy array and
            // this is a no-op, exactly as it should be.
            Save.ForgeWallet.CollapseLegacyDenominations();
            break;
        case 4:
            MigrateAbilityUnlocksV4ToV5(Save.Progression);
            break;
        case 5:
            // O111: Class Points are deleted. Nothing is refunded and nothing
            // is read -- see MigrateClassCurrencyV5ToV6.
            MigrateClassCurrencyV5ToV6(Save.Progression);
            break;
        case 6:
            // O111's doctrine pool stopped being paid whole at commitment and
            // became an entitlement settled against a counter. A v6 save was
            // paid its eight AT COMMITMENT and has no counter, so loading it
            // under the new rule would pay the level entitlement again on top.
            // Seeding the counter is what makes the change cost nothing.
            MigrateDoctrineEntitlementV6ToV7(Save.Progression);
            break;
        case 7:
            // Additive, the v2 -> v3 shape: bRiftglassFoldedToAccount
            // deserializes false and CharacterId invalid on a v7 file, and
            // both are the truth for a file written before they existed.
            // The balance is deliberately LEFT IN ForgeWallet here — the
            // fold that moves it writes two files, and a pure step cannot.
            break;
        default:
            // Unreachable while every version below CurrentSaveVersion has a
            // step. Left as a hard stop so ADDING a version without adding its
            // migration hangs nothing and loses nothing.
            OutNote = FString::Printf(TEXT("no migration step from save version %d"), Save.SaveVersion);
            return false;
        }
        ++Save.SaveVersion;
        ++Steps;
    }

    if (Steps > 0)
    {
        OutNote = FString::Printf(TEXT("migrated save to version %d in %d step(s)"), Save.SaveVersion, Steps);
    }
    return true;
}

// ---------------------------------------------------------------------------
// O111 - the class-currency migration. REFUNDS NOTHING AND READS NOTHING.
//
// Class Points are deleted and the fifteen branch trees are doctrines now.
// Spent-plus-unspent is derivable from the payload alone (one per level to 30),
// so nothing here needs a cost table to work out what a character paid -- and
// O27 deletes the freed points rather than folding them into another pool, so
// there is nothing to hand back either.
//
// IT SEEDS THE DOCTRINE WALLET WITH ZERO, AND THAT IS THE RULING RATHER THAN AN
// OMISSION. The eight arrive at commitment, and this step clears every
// commitment, so a migrated character re-commits at the Forge and is paid then.
// Seeding eight here would pay a character who has not chosen a doctrine to
// spend them in.
//
// THE FIFTEEN BRANCH IDS ARE FROZEN LITERALS, AND THAT IS THE WHOLE POINT.
// A migration that asked the live node library which branches exist would be
// asking a library that no longer has them: by the time this runs they are
// doctrine trees under different ids. A migration must be readable against the
// build that WROTE the save, not the build reading it, so it carries its own
// copy of the world it is migrating away from. Do not tidy this list into a
// library call. It is not duplication; it is the historical record the live
// library deliberately no longer keeps.
// ---------------------------------------------------------------------------
void UBreakerSaveGame::MigrateDoctrineEntitlementV6ToV7(FBreakerProgressionState& Progression)
{
    // A COMMITTED v6 CHARACTER WAS ALREADY PAID IN FULL. Commitment handed over
    // the whole eight, so the counter is seeded to the whole grant and the
    // level entitlement finds nothing owed. Without this the character is paid
    // a second time at every benchmark they have already passed.
    //
    // AN UNCOMMITTED v6 CHARACTER WAS PAID NOTHING, so its counter is zero and
    // it is paid its benchmarks normally on the next level grant. That is the
    // correct outcome and not a windfall: under the old rule it would have been
    // paid on commitment instead.
    //
    // FROZEN LITERAL, deliberately. This reads DoctrinePointGrant today and
    // would keep reading it if the grant were later retuned -- at which point
    // this step would migrate old saves to a number that was never true for
    // them. The v6 grant was eight; it is written as eight.
    constexpr int32 V6CommitmentGrant = 8;
    Progression.LevelDoctrinePointsGranted =
        (Progression.CommittedBranch != NAME_None) ? V6CommitmentGrant : 0;
}

void UBreakerSaveGame::MigrateClassCurrencyV5ToV6(FBreakerProgressionState& Progression)
{
    static const FName RetiringBranches[] = {
        TEXT("Swift.Kinetic"),         TEXT("Swift.Marksman"),       TEXT("Swift.Frenzy"),
        TEXT("Caster.Spellblade"),     TEXT("Caster.VoidWhisperer"), TEXT("Caster.Multispell"),
        TEXT("Gunsmith.Armory"),       TEXT("Gunsmith.FieldTech"),   TEXT("Gunsmith.Tinkerer"),
        TEXT("Tank.Leech"),            TEXT("Tank.Bastion"),         TEXT("Tank.Demolitionist"),
        TEXT("Support.Medic"),         TEXT("Support.Conductor"),    TEXT("Support.Warden"),
    };

    // The ranks go because the nodes they name are funded from a different pool
    // now. Leaving them would let a character keep a class build they can no
    // longer have paid for, which is the refund this ruling refuses.
    Progression.ClassNodeRanks.Reset();
    Progression.UnspentClassPoints = 0;
    Progression.LevelClassPointsGranted = 0;

    // A v5 save has no doctrine state at all, and the defaults are already
    // correct -- empty ranks, zero wallet. Stated rather than assumed, because
    // "the default is right" is the reasoning a later field addition breaks.
    Progression.DoctrineNodeRanks.Reset();
    Progression.UnspentDoctrinePoints = 0;

    // A commitment naming a branch that no longer exists resolves to nothing,
    // and a commitment that resolves to nothing is worse than none: the save is
    // not corrupt, it points at an id the game cannot answer. Cleared so the
    // player re-commits at the Forge, which is where commitment belongs and
    // where the eight points are paid. A commitment naming anything ELSE is
    // left alone -- this step must not reach past the fifteen ids it knows.
    for (const FName& Retiring : RetiringBranches)
    {
        if (Progression.CommittedBranch == Retiring)
        {
            Progression.CommittedBranch = NAME_None;
            break;
        }
    }
}
