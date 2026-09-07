#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UBreakerAbilityDefinition;
class UBreakerProgressionTree;
struct FBreakerAffixLibraryData;
struct FBreakerDialogueData;
struct FBreakerMissionDefinition;
struct FBreakerMissionRift;
struct FBreakerQuestDefinition;

// The census: every built tree and the progression vocabulary, as JSON, and
// the affix library re-serialised from what the game loaded.
//
// Docs/STATE.md used to be measured by regex over the node library's C++
// authoring shape: a MakeNode( pattern, a sixty-line forward walk, a helper
// read by name. This is the same census read off the objects the game
// actually builds, so a magnitude that moves in data moves in the report
// without a compile (O186). The commandlet writes it; the freshness test
// pins the committed file to a live export; status.py --from-data reads it.
//
// World-free: takes the trees or the loaded library, returns text. Nothing
// here writes a file.
namespace BreakerCensus
{
    // The repo-relative path of the export. One place, so the commandlet, the
    // test and the reporter cannot disagree about where the census lives.
    RIORSEDGE_API FString RelativePath();

    RIORSEDGE_API TSharedRef<FJsonObject> Export(const TArray<UBreakerProgressionTree*>& Trees);

    // Pretty-printed, "\n" line endings whatever the platform, no BOM: the
    // file is committed and diffed, so it must serialise byte-identically on
    // every seat.
    RIORSEDGE_API FString Serialize(const TSharedRef<FJsonObject>& Census);

    // Data/affixes.json, the path the library loads from.
    RIORSEDGE_API FString AffixesRelativePath();

    // The affix library as its data file: rows in pool order (slice, aberrant,
    // anomalous, downside, elemental), enums by name, the display name as the
    // FText's source string, then the "leans" and "caps" objects. Returns the
    // text directly rather than an FJsonObject because FJsonObject holds
    // every number as a double and prints seventeen significant digits, which
    // turns an authored 2.2 into 2.2000000000000002; the writer's float
    // overload prints the shortest form, which is what a data file a human
    // edits has to contain. Same printer, same line-ending rule as Serialize.
    RIORSEDGE_API FString ExportAffixes(const FBreakerAffixLibraryData& Data);

    // Data/quests.json, the path the quest library loads from.
    RIORSEDGE_API FString QuestsRelativePath();

    // The flag registry and the quest chain as their data file: "flags" in
    // registry order, then "quests" in chain order with each quest's
    // objectives and reward. NAME_None prints as "", integers through the
    // integer writer, the reward rarity by enum name. Same printer, same
    // line-ending rule as ExportAffixes.
    RIORSEDGE_API FString ExportQuests(const TArray<FBreakerQuestDefinition>& Quests, const TArray<FName>& Flags);

    // Data/dialogue.json, the path the NPC dialogue loads from.
    RIORSEDGE_API FString DialogueRelativePath();

    // Both NPCs' conversations as their data file: one "npcs" row per NPC
    // with its nodes, each node's choices, and its entry overrides, all in
    // authoring order. The file carries em dashes; the caller writes it UTF-8
    // without BOM and the writer leaves characters above 0x7F unescaped.
    RIORSEDGE_API FString ExportDialogue(const FBreakerDialogueData& Data);

    // Data/missions.json, the path the mission library loads from.
    RIORSEDGE_API FString MissionsRelativePath();

    // The rift list and the missions as their data file: "version", then
    // "rifts" by id and yard, then "missions" in act order with each
    // mission's quests and beats. A beat writes "id" and "kind" first and
    // then exactly the fields its kind has; an absent field is omitted, never
    // written empty. NAME_None prints as "". Same printer, same line-ending
    // rule as ExportQuests.
    RIORSEDGE_API FString ExportMissions(const TArray<FBreakerMissionRift>& Rifts, const TArray<FBreakerMissionDefinition>& Missions);

    // Data/abilities.json, the path the ability registry loads its numerics from.
    RIORSEDGE_API FString AbilitiesRelativePath();

    // The ability numerics as their data file: one row per registry entry in
    // registry order, its cost, cooldown and window, then "variants" in
    // authoring order with the keystone tag's name ("" for the base row) and
    // the four variant numerics. Non-ultimates write an empty "variants".
    // Same printer, same line-ending rule as ExportAffixes.
    RIORSEDGE_API FString ExportAbilities(const TArray<UBreakerAbilityDefinition*>& Definitions);
}
