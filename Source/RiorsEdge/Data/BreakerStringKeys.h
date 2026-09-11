#pragma once

#include "CoreMinimal.h"

// THE STRING TABLE'S KEY LIST (O195): every player-facing string that is not
// a dialogue or quest row is a key here and a value in Data/strings.json, so
// copy is a data edit. Dialogue and quest rows own their own text; nothing in
// this list duplicates one of theirs (one home per string).
//
// One X-macro row per string: the enumerator the code names, the namespaced
// key the file names, and the PLACEHOLDER SIGNATURE — the concatenation of
// every printf token the value must carry, "" for a fixed row. The loader
// refuses a file whose value's tokens differ from the signature, which is
// what stands in for the compile-time format check a literal would have had.
//
// The value never appears in C++. A row is added here and in the file in the
// same change; the RiorsEdge.Data.Strings tests hold the two together.
//
// Keys are namespaced by the surface that draws them:
//   hud.      the combat HUD (UI/BreakerPlaytestHUD.cpp, UI/BreakerHUDResourceRow.h)
//   bar.      the enemy nameplate (Combat/BreakerEnemyHealthBars.cpp)
//   loading.  the deployment screen (UI/BreakerLoadingScreen.cpp)
//   stash.    the stash screen (UI/BreakerStashScreen.cpp)
#define BREAKER_STRING_KEYS(BREAKER_STRING) \
    BREAKER_STRING(HudDeathRedeploying,          "hud.death.redeploying",           "") \
    BREAKER_STRING(HudCalloutEliteDown,          "hud.callout.eliteDown",           "") \
    BREAKER_STRING(HudObjective,                 "hud.objective",                   "") \
    BREAKER_STRING(HudCalloutOverdriveActive,    "hud.callout.overdriveActive",     "") \
    BREAKER_STRING(HudCalloutMarked,             "hud.callout.marked",              "") \
    BREAKER_STRING(HudCalloutDodged,             "hud.callout.dodged",              "") \
    BREAKER_STRING(HudCalloutBlocked,            "hud.callout.blocked",             "") \
    BREAKER_STRING(HudAbilitySkillLevel,         "hud.ability.skillLevel",          "") \
    BREAKER_STRING(HudBannerRiftCleared,         "hud.banner.riftCleared",          "") \
    BREAKER_STRING(HudBannerRunComplete,         "hud.banner.runComplete",          "") \
    BREAKER_STRING(HudBannerLevelUp,             "hud.banner.levelUp",              "") \
    BREAKER_STRING(HudBannerWaveClear,           "hud.banner.waveClear",            "%d") \
    BREAKER_STRING(HudBannerLevel,               "hud.banner.level",                "%d") \
    BREAKER_STRING(HudBannerLevelGained,         "hud.banner.levelGained",          "%d%d") \
    BREAKER_STRING(HudBannerClassPoints,         "hud.banner.classPoints",          "%d") \
    BREAKER_STRING(HudBannerCorePoints,          "hud.banner.corePoints",           "%d") \
    BREAKER_STRING(HudBannerPointCapReached,     "hud.banner.pointCapReached",      "") \
    BREAKER_STRING(HudWalletRiftglass,           "hud.wallet.riftglass",            "%d") \
    BREAKER_STRING(HudPromptTalk,                "hud.prompt.talk",                 "") \
    BREAKER_STRING(HudPromptTalkNamed,           "hud.prompt.talkNamed",            "%s") \
    BREAKER_STRING(HudPromptKeyed,               "hud.prompt.keyed",                "%s") \
    BREAKER_STRING(HudAbilitiesNoKit,            "hud.abilities.noKit",             "") \
    BREAKER_STRING(HudBackpackFull,              "hud.backpack.full",               "%d%d") \
    BREAKER_STRING(HudWeaponRifle,               "hud.weapon.rifle",                "") \
    BREAKER_STRING(HudStatusUnnamed,             "hud.status.unnamed",              "") \
    BREAKER_STRING(HudResourceLabel,             "hud.resource.label",              "") \
    BREAKER_STRING(HudResourceStateNone,         "hud.resource.state.none",         "") \
    BREAKER_STRING(HudResourceMomentumLabel,     "hud.resource.momentum.label",     "") \
    BREAKER_STRING(HudResourceMomentumRedline,   "hud.resource.momentum.redline",   "") \
    BREAKER_STRING(HudResourceMomentumRunning,   "hud.resource.momentum.running",   "") \
    BREAKER_STRING(HudResourceManaLabel,         "hud.resource.mana.label",         "") \
    BREAKER_STRING(HudResourceManaOvercast,      "hud.resource.mana.overcast",      "") \
    BREAKER_STRING(HudResourceScrapLabel,        "hud.resource.scrap.label",        "") \
    BREAKER_STRING(HudResourceScrapSurplus,      "hud.resource.scrap.surplus",      "") \
    BREAKER_STRING(HudResourceScrapStocked,      "hud.resource.scrap.stocked",      "") \
    BREAKER_STRING(HudResourceScrapDry,          "hud.resource.scrap.dry",          "") \
    BREAKER_STRING(HudResourceGritLabel,         "hud.resource.grit.label",         "") \
    BREAKER_STRING(HudResourceGritIronclad,      "hud.resource.grit.ironclad",      "") \
    BREAKER_STRING(HudResourceGritBraced,        "hud.resource.grit.braced",        "") \
    BREAKER_STRING(HudResourceGritWinded,        "hud.resource.grit.winded",        "") \
    BREAKER_STRING(HudResourceChargeLabel,       "hud.resource.charge.label",       "") \
    BREAKER_STRING(HudResourceChargeResonant,    "hud.resource.charge.resonant",    "") \
    BREAKER_STRING(HudResourceChargeAttuned,     "hud.resource.charge.attuned",     "") \
    BREAKER_STRING(HudResourceChargeCold,        "hud.resource.charge.cold",        "") \
    BREAKER_STRING(BarBoss,                      "bar.boss",                        "") \
    BREAKER_STRING(LoadingTitleCampaign,         "loading.title.campaign",          "") \
    BREAKER_STRING(LoadingTitleEndgame,          "loading.title.endgame",           "") \
    BREAKER_STRING(LoadingAreaLevel,             "loading.areaLevel",               "") \
    BREAKER_STRING(LoadingItemLevel,             "loading.itemLevel",               "%d%d") \
    BREAKER_STRING(LoadingStatMonsterHealth,     "loading.stat.monsterHealth",      "") \
    BREAKER_STRING(LoadingStatMonsterDamage,     "loading.stat.monsterDamage",      "") \
    BREAKER_STRING(LoadingStatDeaths,            "loading.stat.deaths",             "") \
    BREAKER_STRING(LoadingStatMultiplier,        "loading.stat.multiplier",         "%.2f") \
    BREAKER_STRING(LoadingInsignia,              "loading.insignia",                "") \
    BREAKER_STRING(LoadingStageOpening,          "loading.stage.opening",           "") \
    BREAKER_STRING(LoadingStageOnSite,           "loading.stage.onSite",            "") \
    BREAKER_STRING(LoadingStageClosing,          "loading.stage.closing",           "") \
    BREAKER_STRING(DebriefContinue,              "debrief.continue",                "") \
    BREAKER_STRING(DebriefChooseOne,             "debrief.chooseOne",               "") \
    BREAKER_STRING(StashMoveToStash,             "stash.moveToStash",               "") \
    BREAKER_STRING(StashTakeToBackpack,          "stash.takeToBackpack",            "") \
    BREAKER_STRING(StashTitle,                   "stash.title",                     "") \
    BREAKER_STRING(EquipmentAuthority, "equipment.authority", "") \
    BREAKER_STRING(EquipmentMissingItem, "equipment.missingItem", "") \
    BREAKER_STRING(EquipmentMissingLevel, "equipment.missingLevel", "") \
    BREAKER_STRING(EquipmentRequiredLevel, "equipment.requiredLevel", "%d%d") \
    BREAKER_STRING(EquipmentInvalidSwap, "equipment.invalidSwap", "") \
    BREAKER_STRING(EquipmentFailed, "equipment.failed", "") \
    BREAKER_STRING(EquipmentUnavailable, "equipment.unavailable", "") \
    BREAKER_STRING(EquipmentEquipped, "equipment.equipped", "") \
    BREAKER_STRING(EquipmentRequirementCard, "equipment.requirementCard", "%d%d") \
    BREAKER_STRING(EquipmentMissingLevelCard, "equipment.missingLevelCard", "") \
    BREAKER_STRING(AbilitiesSwap, "abilities.swap", "") \
    BREAKER_STRING(AbilitiesMove, "abilities.move", "") \
    BREAKER_STRING(SettingsAudioDescription, "settings.audio.description", "") \
    BREAKER_STRING(SettingsAudioTest, "settings.audio.test", "") \
    BREAKER_STRING(SettingsAudioMusicUnavailable, "settings.audio.musicUnavailable", "") \
    BREAKER_STRING(CoreOverviewHint, "core.overviewHint", "") \
    BREAKER_STRING(WeaponBaseDamage, "equipment.weaponBaseDamage", "%.1f") \
    BREAKER_STRING(WeaponBasePellets, "equipment.weaponBasePellets", "%.1f%d") \
    BREAKER_STRING(GearRarityChoices, "equipment.rarityChoices", "") \
    BREAKER_STRING(EnemySkitter, "enemy.skitter", "") \
    BREAKER_STRING(EnemyDrudge, "enemy.drudge", "") \
    BREAKER_STRING(EnemyLattice, "enemy.lattice", "") \
    BREAKER_STRING(EnemySkirmisher, "enemy.skirmisher", "") \
    BREAKER_STRING(EnemyWarden, "enemy.warden", "") \
    BREAKER_STRING(EnemyFieldMarshal, "enemy.fieldMarshal", "") \
    BREAKER_STRING(EnemyHoldfast, "enemy.holdfast", "") \
    BREAKER_STRING(SandboxCharacter, "sandbox.character", "") \
    BREAKER_STRING(SandboxGear, "sandbox.gear", "") \
    BREAKER_STRING(SandboxStats, "sandbox.stats", "") \
    BREAKER_STRING(SandboxTitle, "sandbox.title", "") \
    BREAKER_STRING(SandboxInventory, "sandbox.inventory", "") \
    BREAKER_STRING(SandboxBack, "sandbox.back", "") \
    BREAKER_STRING(SandboxInvalidSeed, "sandbox.invalidSeed", "") \
    BREAKER_STRING(SandboxNotice, "sandbox.notice", "") \
    BREAKER_STRING(SandboxSeedHint, "sandbox.seedHint", "") \
    BREAKER_STRING(SandboxFresh, "sandbox.fresh", "") \
    BREAKER_STRING(SandboxRepeat, "sandbox.repeat", "") \
    BREAKER_STRING(SandboxWallet, "sandbox.wallet", "%d%d%d%d") \
    BREAKER_STRING(SandboxFull, "sandbox.full", "") \
    BREAKER_STRING(CycleBleed, "cycle.bleed", "") \
    BREAKER_STRING(CyclePoison, "cycle.poison", "") \
    BREAKER_STRING(CycleCurrent, "cycle.current", "%s") \
    BREAKER_STRING(CyclePreview, "cycle.preview", "%s%s") \
    BREAKER_STRING(SandboxRarityGates, "sandbox.rarityGates", "") \
    BREAKER_STRING(HudParryLabel, "hud.parry.label", "") \
    BREAKER_STRING(HudParryActive, "hud.parry.active", "") \
    BREAKER_STRING(HudParrySuccess, "hud.parry.success", "") \
    BREAKER_STRING(HudDamageRamp, "hud.weapon.damageRamp", "%d%d") \
    BREAKER_STRING(CycleVoid, "cycle.void", "") \
    BREAKER_STRING(HudEntropy, "hud.entropy", "") \
    BREAKER_STRING(HudRotTimer, "hud.rotTimer", "%.1f") \
    BREAKER_STRING(CycleEntropy, "cycle.entropy", "") \
    BREAKER_STRING(HudRotDamage, "hud.damage.rot", "%s") \
    BREAKER_STRING(HudVoid, "hud.void", "") \
    BREAKER_STRING(HudErasedTimer, "hud.erasedTimer", "%.1f") \
    BREAKER_STRING(HudErasedDamage, "hud.damage.erased", "%s") \
    BREAKER_STRING(HudRift, "hud.rift", "") \
    BREAKER_STRING(HudUnstableTimer, "hud.unstableTimer", "%.1f") \
    BREAKER_STRING(HudUnstableDamage, "hud.damage.unstable", "%s") \
    BREAKER_STRING(HudCollapseDamage, "hud.damage.collapse", "%s") \
    BREAKER_STRING(HudWitherDamage, "hud.damage.wither", "%s") \
    BREAKER_STRING(HudTearDamage, "hud.damage.tear", "%s") \
    BREAKER_STRING(HudRotPersistent, "hud.rotPersistent", "")

// The enumerator is an index into the loaded table and nothing else: it is
// never serialized, so the list may be reordered freely. The key string is
// the stable name.
enum class EBreakerStringKey : uint8
{
#define BREAKER_STRING(Identifier, Key, Signature) Identifier,
    BREAKER_STRING_KEYS(BREAKER_STRING)
#undef BREAKER_STRING
    Count
};

struct FBreakerStringKeyRow
{
    const TCHAR* Key;
    const TCHAR* Signature;
};

// Key and signature per enumerator, in enumerator order.
inline constexpr FBreakerStringKeyRow BreakerStringKeyRows[] =
{
#define BREAKER_STRING(Identifier, Key, Signature) { TEXT(Key), TEXT(Signature) },
    BREAKER_STRING_KEYS(BREAKER_STRING)
#undef BREAKER_STRING
};

inline constexpr int32 BreakerStringKeyCount = static_cast<int32>(EBreakerStringKey::Count);
static_assert(UE_ARRAY_COUNT(BreakerStringKeyRows) == BreakerStringKeyCount, "one row per key");
