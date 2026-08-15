#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BreakerGameSettings.generated.h"

// Window mode the player picks from the settings screen. Kept as our own
// enum rather than engine EWindowMode::Type so the model layer does not leak
// an engine ordering quirk (EWindowMode::WindowedFullscreen IS "borderless",
// a name that reads backwards at the call site) into the settings contract.
// ApplyToEngine() is the only place that translates between the two.
UENUM(BlueprintType)
enum class EBreakerWindowMode : uint8
{
    Fullscreen,
    BorderlessWindowed,
    Windowed
};

// Pure, world-free maths and lookups behind the settings model: clamping and
// keybind override resolution/conflict detection. Follows the split
// UBreakerRangedBehaviorLibrary uses (Combat/BreakerRangedBehavior.h) — the
// DECISIONS are unit-testable even though applying them to the engine or a
// live Enhanced Input subsystem is not.
UCLASS()
class RIORSEDGE_API UBreakerGameSettingsLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // ---- Clamps. One pure function per numeric field so a test (or a UI
    // slider's OnValueChanged) can prove a bound without touching the object
    // that owns the field. ----

    // Matches the range ABreakerCharacter::ApplyMenuSettings and its
    // GConfig load already enforce (Characters/BreakerCharacter.cpp:222,
    // 959) — the range the EXISTING settings screen slider lives in today.
    // Note: the keyboard nudge path (IncreaseSensitivity/DecreaseSensitivity,
    // BreakerCharacter.cpp:946-947) clamps to 0.2-3.0, a wider, pre-existing
    // inconsistency this pass does not resolve — BreakerCharacter.cpp is out
    // of territory. This model follows the settings-screen bound.
    UFUNCTION(BlueprintPure, Category = "Settings|Clamp")
    static float ClampMouseSensitivity(float Value);

    // O2 PLACEHOLDER range — no existing ADS sensitivity value or bound in
    // the project to match. 0.1 floor keeps a multiplier from reaching zero
    // (an unrecoverable "aiming does nothing" state); 3.0 ceiling mirrors the
    // outer edge of the base sensitivity's own keyboard-nudge range above.
    UFUNCTION(BlueprintPure, Category = "Settings|Clamp")
    static float ClampScopedSensitivityMultiplier(float Value);

    // Matches ABreakerCharacter::BaseFieldOfView's existing clamp
    // (Characters/BreakerCharacter.cpp:223, 944-945, 961).
    UFUNCTION(BlueprintPure, Category = "Settings|Clamp")
    static float ClampFOV(float Value);

    // O2 PLACEHOLDER range — no existing frame-rate cap in the project. 0
    // means uncapped and is always in range; a positive cap is bounded to a
    // sane [30, 360] so a fat-fingered ini edit cannot hand the engine a cap
    // of 1 or of a million.
    UFUNCTION(BlueprintPure, Category = "Settings|Clamp")
    static float ClampFrameRateCap(float Value);

    // O2 PLACEHOLDER — shared by master/effects/music, all 0..1.
    UFUNCTION(BlueprintPure, Category = "Settings|Clamp")
    static float ClampVolume(float Value);

    // ---- Keybinds ----

    // An override wins; an action with no override falls through to its
    // default, so an unbound action is never actually unbound.
    UFUNCTION(BlueprintPure, Category = "Settings|Keybinds")
    static FKey ResolveActionKey(FName Action, const TMap<FName, FKey>& Overrides, const TMap<FName, FKey>& Defaults);

    // True if some OTHER action already resolves (through the same
    // override-over-default layering ResolveActionKey uses) to Key.
    // Rebinding an action to the key it ALREADY holds is explicitly not a
    // conflict — a UI can open a "press a key" flow on an action's current
    // binding without an immediate false-positive warning.
    UFUNCTION(BlueprintPure, Category = "Settings|Keybinds")
    static bool FindKeybindConflict(FName Action, FKey Key, const TMap<FName, FKey>& Overrides,
        const TMap<FName, FKey>& Defaults, FName& OutConflictingAction);
};

// The settings MODEL and persistence layer: input (sensitivity, scoped
// sensitivity, invert, keybind overrides), video (FOV, window mode, frame
// cap, vsync) and audio (master/effects/music). No UI lives here.
//
// STORAGE CHOICE: a plain UObject persisted through GConfig into
// GGameUserSettingsIni — not a USaveGame slot. Two reasons:
//  1. It continues the mechanism ABreakerCharacter already uses for
//     sensitivity/FOV/invert (Characters/BreakerCharacter.cpp:218-224,
//     949-964): GConfig->Get/SetFloat/Bool against section
//     "RiorsEdge.Playtest" in GGameUserSettingsIni. This model reads and
//     writes those SAME section/key names for those three fields, so a
//     player's already-persisted values are picked up with nothing to
//     migrate — see LoadOrDefaults()'s comment for the exact keys.
//  2. USaveGame in this project (Save/BreakerSaveGame.h:11-12) is explicitly
//     "everything a CHARACTER carries between sessions" — quest flags,
//     equipped items, the Forge wallet. Window mode, vsync, frame cap and
//     audio volumes are machine/profile preferences: they must survive a
//     character's death, a new save slot, or a wiped BreakerSave0 untouched,
//     which a USaveGame slot does not guarantee. GGameUserSettingsIni is
//     exactly the file UGameUserSettings itself already persists this class
//     of data into.
UCLASS(BlueprintType)
class RIORSEDGE_API UBreakerGameSettings : public UObject
{
    GENERATED_BODY()

public:
    // ---- Input ----
    // Default matches ABreakerCharacter::LookSensitivity's default,
    // Characters/BreakerCharacter.h:279.
    UPROPERTY(BlueprintReadOnly, Category = "Input")
    float MouseSensitivity = 1.0f;

    // Multiplier of MouseSensitivity applied while
    // UBreakerWeaponComponent::IsAiming() (Weapons/BreakerWeaponComponent.h:125)
    // is true — NOT an absolute sensitivity. An absolute value would fight
    // the base slider: raising base sensitivity would silently change ADS
    // feel too, or the UI would have to keep the two sliders in lockstep. A
    // multiplier lets "scoped sensitivity" mean exactly what the name says —
    // how aiming down sights feels RELATIVE to hip fire — independent of
    // wherever the base slider sits.
    // O2 PLACEHOLDER default — no existing ADS sensitivity value in the
    // project to match; 1.0 means "identical to hip fire" until a value is
    // authored.
    UPROPERTY(BlueprintReadOnly, Category = "Input")
    float ScopedSensitivityMultiplier = 1.0f;

    // Default matches ABreakerCharacter::bInvertLookY's default,
    // Characters/BreakerCharacter.h:280.
    UPROPERTY(BlueprintReadOnly, Category = "Input")
    bool bInvertVerticalLook = false;

    // ---- Keybinds ----
    // Overrides layered over UBreakerInputConfig's (Input/BreakerInputConfig.h)
    // default mapping context. An action with no entry here falls through to
    // its default via UBreakerGameSettingsLibrary::ResolveActionKey, so a
    // fresh install — or an action nobody has ever rebound — is never
    // actually unbound. Keyed by the same action names the data asset's
    // UPROPERTYs use (e.g. "Move", "Look", "Jump", "Sprint", "Dash", "Slide",
    // "Fire", "Aim", "Reload") so a caller can look an override up by the
    // same name it already has from UBreakerInputConfig.
    UPROPERTY(BlueprintReadOnly, Category = "Input|Keybinds")
    TMap<FName, FKey> KeybindOverrides;

    // ---- Video ----
    // Default matches ABreakerCharacter::BaseFieldOfView's default,
    // Characters/BreakerCharacter.h:294.
    UPROPERTY(BlueprintReadOnly, Category = "Video")
    float FieldOfView = 90.0f;

    // O2 PLACEHOLDER default — no existing project default for window mode.
    UPROPERTY(BlueprintReadOnly, Category = "Video")
    EBreakerWindowMode WindowMode = EBreakerWindowMode::Windowed;

    // O2 PLACEHOLDER default. 0 = uncapped.
    UPROPERTY(BlueprintReadOnly, Category = "Video")
    float FrameRateCapFPS = 0.0f;

    // O2 PLACEHOLDER default.
    UPROPERTY(BlueprintReadOnly, Category = "Video")
    bool bVSyncEnabled = false;

    // ---- Audio ----
    // O2 PLACEHOLDER defaults — no existing project audio volume values.
    UPROPERTY(BlueprintReadOnly, Category = "Audio")
    float MasterVolume = 1.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Audio")
    float EffectsVolume = 1.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Audio")
    float MusicVolume = 1.0f;

    // Loads every field from GGameUserSettingsIni, clamping each on the way
    // in so a hand-edited or stale ini can never hand a live consumer an
    // out-of-range value. A missing key falls back to the default above —
    // this is what makes a pre-this-pass ini (which has FOV/Sensitivity/
    // InvertLookY and nothing else) load cleanly with sane values for
    // everything new.
    void LoadOrDefaults();

    // Writes every field back to GGameUserSettingsIni and flushes. Writes
    // the SAME section/keys ABreakerCharacter::SavePlaytestSettings already
    // owns for FOV/Sensitivity/InvertLookY (Characters/BreakerCharacter.cpp
    // :949-955), so the two writers stay compatible instead of racing each
    // other onto divergent keys.
    void Save() const;

    // The only function that touches engine state: pushes window mode,
    // vsync and the frame cap through UGameUserSettings, and master volume
    // through the main audio device's transient master volume. Deliberately
    // isolated so LoadOrDefaults, Save and every clamp/keybind function
    // above stay testable with no engine subsystems spun up.
    //
    // NOT covered by RiorsEdge.Settings.* — see Tests/BreakerGameSettingsTests
    // .cpp's header comment. Effects/Music volumes are stored and clamped
    // but NOT applied here: routing them requires SoundClass/SoundMix
    // content assets this project does not yet have, which is content
    // authoring outside this model-and-persistence pass.
    void ApplyToEngine() const;

    // Sets (or replaces) the override for one action. Does not itself reject
    // conflicts — UBreakerGameSettingsLibrary::FindKeybindConflict is
    // offered separately so a UI can warn and let the player confirm the
    // rebind anyway (a valid choice: two actions sharing a key is sometimes
    // intentional, e.g. context-sensitive binds).
    void SetKeybindOverride(FName Action, FKey Key) { KeybindOverrides.Add(Action, Key); }

    // Removes a single override so Action falls back through to its default.
    void ClearKeybindOverride(FName Action) { KeybindOverrides.Remove(Action); }

    // Drops every override. Defaults live on the UBreakerInputConfig data
    // asset, not here, so there is nothing to reset TO — resetting is just
    // "no overrides remain", which is exactly what falling through to
    // ResolveActionKey's Defaults argument already means.
    void ResetKeybindsToDefault() { KeybindOverrides.Empty(); }
};
