#include "Settings/BreakerGameSettings.h"

#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "AudioDevice.h"
#include "Misc/ConfigCacheIni.h"

// ---------------------------------------------------------------------------
// UBreakerGameSettingsLibrary — pure clamps and keybind resolution.
// ---------------------------------------------------------------------------

float UBreakerGameSettingsLibrary::ClampMouseSensitivity(float Value)
{
    return FMath::Clamp(Value, 0.2f, 2.0f);
}

float UBreakerGameSettingsLibrary::ClampScopedSensitivityMultiplier(float Value)
{
    return FMath::Clamp(Value, 0.1f, 3.0f);
}

float UBreakerGameSettingsLibrary::ClampFOV(float Value)
{
    return FMath::Clamp(Value, 70.0f, 120.0f);
}

float UBreakerGameSettingsLibrary::ClampFrameRateCap(float Value)
{
    // 0 is the uncapped sentinel and is always valid; only a positive
    // request gets bounded.
    if (Value <= 0.0f) return 0.0f;
    return FMath::Clamp(Value, 30.0f, 360.0f);
}

float UBreakerGameSettingsLibrary::ClampVolume(float Value)
{
    return FMath::Clamp(Value, 0.0f, 1.0f);
}

FKey UBreakerGameSettingsLibrary::ResolveActionKey(FName Action, const TMap<FName, FKey>& Overrides, const TMap<FName, FKey>& Defaults)
{
    if (const FKey* Override = Overrides.Find(Action))
    {
        return *Override;
    }
    if (const FKey* Default = Defaults.Find(Action))
    {
        return *Default;
    }
    return EKeys::Invalid;
}

bool UBreakerGameSettingsLibrary::FindKeybindConflict(FName Action, FKey Key, const TMap<FName, FKey>& Overrides,
    const TMap<FName, FKey>& Defaults, FName& OutConflictingAction)
{
    OutConflictingAction = NAME_None;
    if (!Key.IsValid())
    {
        return false;
    }

    // Every action name either side has an entry for, deduplicated, is a
    // candidate — an action can appear only in Defaults (never rebound), or
    // only in Overrides (should not happen in practice but costs nothing to
    // also check), or in both.
    TSet<FName> Candidates;
    Defaults.GetKeys(Candidates);
    for (const TPair<FName, FKey>& Pair : Overrides)
    {
        Candidates.Add(Pair.Key);
    }

    for (const FName& Candidate : Candidates)
    {
        // Rebinding an action to the key it already holds is not a conflict
        // with itself.
        if (Candidate == Action)
        {
            continue;
        }
        if (ResolveActionKey(Candidate, Overrides, Defaults) == Key)
        {
            OutConflictingAction = Candidate;
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// UBreakerGameSettings — model + persistence.
// ---------------------------------------------------------------------------

namespace BreakerGameSettingsIni
{
    // Section/keys ABreakerCharacter already owns (Characters/BreakerCharacter
    // .cpp:218-224, 949-955). Reused verbatim so a pre-existing player's FOV,
    // sensitivity and invert survive this model's introduction untouched —
    // both loaders read the same three lines, neither one migrates the other.
    static const TCHAR* LegacySection = TEXT("RiorsEdge.Playtest");
    static const TCHAR* LegacyFOVKey = TEXT("FOV");
    static const TCHAR* LegacySensitivityKey = TEXT("Sensitivity");
    static const TCHAR* LegacyInvertKey = TEXT("InvertLookY");

    // New fields this pass adds. A separate section so a pre-existing
    // "RiorsEdge.Playtest" block (which has exactly the three legacy keys
    // above and nothing else) is never mistaken for a full settings block.
    static const TCHAR* Section = TEXT("RiorsEdge.Settings");
    static const TCHAR* ScopedSensitivityKey = TEXT("ScopedSensitivityMultiplier");
    static const TCHAR* WindowModeKey = TEXT("WindowMode");
    static const TCHAR* FrameRateCapKey = TEXT("FrameRateCapFPS");
    static const TCHAR* VSyncKey = TEXT("VSyncEnabled");
    static const TCHAR* MasterVolumeKey = TEXT("MasterVolume");
    static const TCHAR* EffectsVolumeKey = TEXT("EffectsVolume");
    static const TCHAR* MusicVolumeKey = TEXT("MusicVolume");

    // Keybind overrides live in their own section, one key=value line per
    // overridden action, so loading them does not require knowing the
    // action names in advance — GConfig->GetSection enumerates whatever is
    // there, which is exactly the "layered override" model: an empty
    // section is a completely valid "no overrides" state.
    static const TCHAR* KeybindSection = TEXT("RiorsEdge.Settings.Keybinds");
}

void UBreakerGameSettings::LoadOrDefaults()
{
    using namespace BreakerGameSettingsIni;

    // ---- The three fields ABreakerCharacter already persists. Defaults
    // here match this object's own field initializers, which in turn match
    // ABreakerCharacter's — see the UPROPERTY comments in the header. ----
    float LoadedFOV = FieldOfView;
    GConfig->GetFloat(LegacySection, LegacyFOVKey, LoadedFOV, GGameUserSettingsIni);
    FieldOfView = UBreakerGameSettingsLibrary::ClampFOV(LoadedFOV);

    float LoadedSensitivity = MouseSensitivity;
    GConfig->GetFloat(LegacySection, LegacySensitivityKey, LoadedSensitivity, GGameUserSettingsIni);
    MouseSensitivity = UBreakerGameSettingsLibrary::ClampMouseSensitivity(LoadedSensitivity);

    bool LoadedInvert = bInvertVerticalLook;
    GConfig->GetBool(LegacySection, LegacyInvertKey, LoadedInvert, GGameUserSettingsIni);
    bInvertVerticalLook = LoadedInvert;

    // ---- Everything this pass adds. Missing keys keep the field
    // initializer's default, so an ini written before this pass loads
    // cleanly. ----
    float LoadedScoped = ScopedSensitivityMultiplier;
    GConfig->GetFloat(Section, ScopedSensitivityKey, LoadedScoped, GGameUserSettingsIni);
    ScopedSensitivityMultiplier = UBreakerGameSettingsLibrary::ClampScopedSensitivityMultiplier(LoadedScoped);

    int32 LoadedWindowMode = static_cast<int32>(WindowMode);
    GConfig->GetInt(Section, WindowModeKey, LoadedWindowMode, GGameUserSettingsIni);
    LoadedWindowMode = FMath::Clamp(LoadedWindowMode, 0, static_cast<int32>(EBreakerWindowMode::Windowed));
    WindowMode = static_cast<EBreakerWindowMode>(LoadedWindowMode);

    float LoadedFrameCap = FrameRateCapFPS;
    GConfig->GetFloat(Section, FrameRateCapKey, LoadedFrameCap, GGameUserSettingsIni);
    FrameRateCapFPS = UBreakerGameSettingsLibrary::ClampFrameRateCap(LoadedFrameCap);

    bool LoadedVSync = bVSyncEnabled;
    GConfig->GetBool(Section, VSyncKey, LoadedVSync, GGameUserSettingsIni);
    bVSyncEnabled = LoadedVSync;

    float LoadedMaster = MasterVolume;
    GConfig->GetFloat(Section, MasterVolumeKey, LoadedMaster, GGameUserSettingsIni);
    MasterVolume = UBreakerGameSettingsLibrary::ClampVolume(LoadedMaster);

    float LoadedEffects = EffectsVolume;
    GConfig->GetFloat(Section, EffectsVolumeKey, LoadedEffects, GGameUserSettingsIni);
    EffectsVolume = UBreakerGameSettingsLibrary::ClampVolume(LoadedEffects);

    float LoadedMusic = MusicVolume;
    GConfig->GetFloat(Section, MusicVolumeKey, LoadedMusic, GGameUserSettingsIni);
    MusicVolume = UBreakerGameSettingsLibrary::ClampVolume(LoadedMusic);

    // ---- Keybind overrides ----
    KeybindOverrides.Empty();
    if (const FConfigSection* ConfigSection = GConfig->GetSection(KeybindSection, false, GGameUserSettingsIni))
    {
        for (const TPair<FName, FConfigValue>& Entry : *ConfigSection)
        {
            FKey Key(FName(*Entry.Value.GetValue()));
            if (Key.IsValid())
            {
                KeybindOverrides.Add(Entry.Key, Key);
            }
        }
    }
}

void UBreakerGameSettings::Save() const
{
    using namespace BreakerGameSettingsIni;

    // Same section/keys ABreakerCharacter::SavePlaytestSettings writes
    // (Characters/BreakerCharacter.cpp:949-955) — this object and that
    // function are two writers of the SAME lines, not competing ones.
    GConfig->SetFloat(LegacySection, LegacyFOVKey, FieldOfView, GGameUserSettingsIni);
    GConfig->SetFloat(LegacySection, LegacySensitivityKey, MouseSensitivity, GGameUserSettingsIni);
    GConfig->SetBool(LegacySection, LegacyInvertKey, bInvertVerticalLook, GGameUserSettingsIni);

    GConfig->SetFloat(Section, ScopedSensitivityKey, ScopedSensitivityMultiplier, GGameUserSettingsIni);
    GConfig->SetInt(Section, WindowModeKey, static_cast<int32>(WindowMode), GGameUserSettingsIni);
    GConfig->SetFloat(Section, FrameRateCapKey, FrameRateCapFPS, GGameUserSettingsIni);
    GConfig->SetBool(Section, VSyncKey, bVSyncEnabled, GGameUserSettingsIni);
    GConfig->SetFloat(Section, MasterVolumeKey, MasterVolume, GGameUserSettingsIni);
    GConfig->SetFloat(Section, EffectsVolumeKey, EffectsVolume, GGameUserSettingsIni);
    GConfig->SetFloat(Section, MusicVolumeKey, MusicVolume, GGameUserSettingsIni);

    // Keybinds: clear the whole section and rewrite it so a cleared override
    // (ClearKeybindOverride/ResetKeybindsToDefault) actually disappears from
    // disk instead of lingering as a stale line GConfig never removes on its
    // own.
    GConfig->EmptySection(KeybindSection, GGameUserSettingsIni);
    for (const TPair<FName, FKey>& Pair : KeybindOverrides)
    {
        GConfig->SetString(KeybindSection, *Pair.Key.ToString(), *Pair.Value.ToString(), GGameUserSettingsIni);
    }

    GConfig->Flush(false, GGameUserSettingsIni);
}

void UBreakerGameSettings::ApplyToEngine() const
{
    if (UGameUserSettings* EngineSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
    {
        // EBreakerWindowMode -> EWindowMode::Type. WindowedFullscreen is the
        // engine's name for "borderless" — see the header comment on
        // EBreakerWindowMode for why this model does not use that enum
        // directly.
        EWindowMode::Type EngineMode = EWindowMode::Windowed;
        switch (WindowMode)
        {
            case EBreakerWindowMode::Fullscreen: EngineMode = EWindowMode::Fullscreen; break;
            case EBreakerWindowMode::BorderlessWindowed: EngineMode = EWindowMode::WindowedFullscreen; break;
            case EBreakerWindowMode::Windowed: default: EngineMode = EWindowMode::Windowed; break;
        }
        EngineSettings->SetFullscreenMode(EngineMode);
        EngineSettings->SetVSyncEnabled(bVSyncEnabled);
        EngineSettings->SetFrameRateLimit(FrameRateCapFPS);
        EngineSettings->ApplyNonResolutionSettings();
        EngineSettings->SaveSettings();
    }

    // Master volume only: the engine has a direct, content-free path for
    // this (the main audio device's transient master volume). Effects/Music
    // do not — routing them needs SoundClass/SoundMix content assets this
    // project does not yet have, so those two fields are stored and clamped
    // but not pushed anywhere yet. See the header comment on ApplyToEngine.
    // AUDIO IS STORED AND CLAMPED, AND ROUTED NOWHERE. Recorded as a gap
    // rather than faked: FAudioDevice::SetTransientMasterVolume lives in a
    // module this target does not link, and adding an audio module dependency
    // to move one float would be the wrong trade while the project has no
    // SoundClass/SoundMix assets for Effects and Music to route through
    // either — two of the three sliders would still do nothing.
    //
    // This matters more than it looks: the project has NO AUDIO AT ALL yet
    // (CONTEXT's asset list calls it the largest single gap between what is
    // built and what is felt), so a volume slider has nothing to make quieter.
    // All three values persist correctly and take effect the day sound exists
    // and this function learns to route them.
    if (GEngine && !FMath::IsNearlyEqual(MasterVolume, 1.0f))
    {
        UE_LOG(LogTemp, Verbose,
            TEXT("BreakerGameSettings: master volume %.2f is stored but not routed — no audio pipeline exists yet."),
            MasterVolume);
    }
}
