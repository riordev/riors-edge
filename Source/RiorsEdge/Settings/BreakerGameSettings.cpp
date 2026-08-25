#include "Settings/BreakerGameSettings.h"

#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "AudioDevice.h"
#include "Input/BreakerInputConfig.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputTriggers.h"
#include "Misc/ConfigCacheIni.h"
#include "UObject/Package.h"

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
// Keybind DEFAULTS — where the Defaults map ResolveActionKey takes comes from.
// ---------------------------------------------------------------------------

const TArray<FName>& UBreakerGameSettingsLibrary::BindableActionNames()
{
    // UBreakerInputConfig's UPROPERTY order (Input/BreakerInputConfig.h:16-34)
    // with DefaultMappingContext (not an action) dropped. Movement first,
    // combat second, the camera/sensitivity nudges third, playtest keys last —
    // the same grouping the asset's Category tags already declare.
    static const TArray<FName> Names = {
        TEXT("Move"), TEXT("Look"), TEXT("Jump"), TEXT("Sprint"), TEXT("Dash"), TEXT("Slide"),
        TEXT("Fire"), TEXT("Aim"), TEXT("Reload"), TEXT("Interact"),
        TEXT("AbilityOne"), TEXT("AbilityTwo"), TEXT("Ultimate"),
        TEXT("FOVUp"), TEXT("FOVDown"), TEXT("SensitivityUp"), TEXT("SensitivityDown"),
        TEXT("PlaytestReset"), TEXT("PlaytestReport"), TEXT("PlaytestDiagnostics")
    };
    return Names;
}

FText UBreakerGameSettingsLibrary::DescribeAction(FName Action)
{
    // An explicit table rather than a de-camel-casing pass. "FOVUp" and
    // "AbilityOne" both come out wrong from any generic splitter, and the two
    // sensitivity nudges want to say what they nudge.
    static const TMap<FName, FString> Labels = {
        { TEXT("Move"),                TEXT("MOVE") },
        { TEXT("Look"),                TEXT("LOOK") },
        { TEXT("Jump"),                TEXT("JUMP") },
        { TEXT("Sprint"),              TEXT("SPRINT") },
        { TEXT("Dash"),                TEXT("DASH") },
        { TEXT("Slide"),               TEXT("SLIDE") },
        { TEXT("Fire"),                TEXT("FIRE") },
        { TEXT("Aim"),                 TEXT("AIM") },
        { TEXT("Reload"),              TEXT("RELOAD") },
        { TEXT("Interact"),            TEXT("INTERACT") },
        { TEXT("AbilityOne"),          TEXT("ABILITY 1") },
        { TEXT("AbilityTwo"),          TEXT("ABILITY 2") },
        { TEXT("Ultimate"),            TEXT("ULTIMATE") },
        { TEXT("FOVUp"),               TEXT("FOV UP") },
        { TEXT("FOVDown"),             TEXT("FOV DOWN") },
        { TEXT("SensitivityUp"),       TEXT("SENSITIVITY UP") },
        { TEXT("SensitivityDown"),     TEXT("SENSITIVITY DOWN") },
        { TEXT("PlaytestReset"),       TEXT("PLAYTEST RESET") },
        { TEXT("PlaytestReport"),      TEXT("PLAYTEST REPORT") },
        { TEXT("PlaytestDiagnostics"), TEXT("PLAYTEST DIAGNOSTICS") }
    };
    if (const FString* Label = Labels.Find(Action))
    {
        return FText::FromString(*Label);
    }
    // An action nobody authored a label for still gets a readable row rather
    // than a blank one.
    return FText::FromString(Action.ToString().ToUpper());
}

void UBreakerGameSettingsLibrary::ResolveDefaultKeysByAction(
    const TArray<TPair<FName, const UInputAction*>>& ActionsByName,
    const TArray<FEnhancedActionKeyMapping>& Mappings,
    TMap<FName, TArray<FKey>>& OutKeysByAction)
{
    OutKeysByAction.Empty();

    // Pointer -> name first, so the mapping list is walked ONCE. A mapping
    // context has a handful of entries today but it is the thing that grows
    // with the game, and the actions list grows with it — the nested-loop
    // version is quadratic in exactly the two dimensions that both move.
    TMap<const UInputAction*, FName> NameByAction;
    for (const TPair<FName, const UInputAction*>& Pair : ActionsByName)
    {
        if (Pair.Value)
        {
            NameByAction.Add(Pair.Value, Pair.Key);
        }
    }

    for (const FEnhancedActionKeyMapping& Mapping : Mappings)
    {
        const FName* Name = NameByAction.Find(Mapping.Action.Get());
        if (!Name || !Mapping.Key.IsValid())
        {
            continue;
        }
        // AddUnique: an axis action commonly appears twice on the same key
        // with different modifiers (a negate pass plus a swizzle), and the
        // screen wants the KEYS a player presses, not the modifier stack.
        OutKeysByAction.FindOrAdd(*Name).AddUnique(Mapping.Key);
    }
}

TMap<FName, FKey> UBreakerGameSettingsLibrary::FirstKeyPerAction(const TMap<FName, TArray<FKey>>& KeysByAction)
{
    TMap<FName, FKey> Flat;
    for (const TPair<FName, TArray<FKey>>& Pair : KeysByAction)
    {
        if (Pair.Value.Num() > 0)
        {
            Flat.Add(Pair.Key, Pair.Value[0]);
        }
    }
    return Flat;
}

void UBreakerGameSettingsLibrary::ListConfigActions(const UBreakerInputConfig* Config,
    TArray<TPair<FName, const UInputAction*>>& OutActionsByName)
{
    OutActionsByName.Empty();
    if (!Config)
    {
        return;
    }
    // Written out rather than walked reflectively. The reflection version
    // would be shorter and would silently pick up any future UInputAction
    // property under a name nobody chose for display; this one fails loudly
    // (a missing row) when the asset gains an action, which is the failure
    // mode that gets noticed.
    auto Add = [&OutActionsByName](const TCHAR* Name, const UInputAction* Action)
    {
        OutActionsByName.Add(TPair<FName, const UInputAction*>(FName(Name), Action));
    };
    Add(TEXT("Move"), Config->Move);
    Add(TEXT("Look"), Config->Look);
    Add(TEXT("Jump"), Config->Jump);
    Add(TEXT("Sprint"), Config->Sprint);
    Add(TEXT("Dash"), Config->Dash);
    Add(TEXT("Slide"), Config->Slide);
    Add(TEXT("Fire"), Config->Fire);
    Add(TEXT("Aim"), Config->Aim);
    Add(TEXT("Reload"), Config->Reload);
    // The contextual verb the interact prompt names. Added the day the config
    // gained the field: this list also feeds BuildRuntimeMappingContext, so
    // an action missing here is not merely rowless — its rebinds silently
    // never apply.
    Add(TEXT("Interact"), Config->Interact);
    Add(TEXT("AbilityOne"), Config->AbilityOne);
    Add(TEXT("AbilityTwo"), Config->AbilityTwo);
    Add(TEXT("Ultimate"), Config->Ultimate);
    Add(TEXT("FOVUp"), Config->FOVUp);
    Add(TEXT("FOVDown"), Config->FOVDown);
    Add(TEXT("SensitivityUp"), Config->SensitivityUp);
    Add(TEXT("SensitivityDown"), Config->SensitivityDown);
    Add(TEXT("PlaytestReset"), Config->PlaytestReset);
    Add(TEXT("PlaytestReport"), Config->PlaytestReport);
    Add(TEXT("PlaytestDiagnostics"), Config->PlaytestDiagnostics);
}

TMap<FName, TArray<FKey>> UBreakerGameSettingsLibrary::ProjectDefaultKeybinds()
{
    // ABreakerCharacter::InputConfig is protected (Characters/BreakerCharacter
    // .h:163, inside the protected block that opens at :97), so the settings
    // screen cannot read the config off the pawn it already holds. Loading the
    // asset by path is the alternative that does not require editing
    // BreakerCharacter.h. The path is the one the Content tree actually has:
    // Content/ProjectBreaker/Input/DA_PlayerInputConfig.uasset.
    static const TCHAR* ConfigPath = TEXT("/Game/ProjectBreaker/Input/DA_PlayerInputConfig.DA_PlayerInputConfig");

    TMap<FName, TArray<FKey>> Empty;
    const UBreakerInputConfig* Config = LoadObject<UBreakerInputConfig>(nullptr, ConfigPath);
    if (!Config || !Config->DefaultMappingContext)
    {
        return Empty;
    }

    TArray<TPair<FName, const UInputAction*>> ActionsByName;
    ListConfigActions(Config, ActionsByName);

    TMap<FName, TArray<FKey>> KeysByAction;
    ResolveDefaultKeysByAction(ActionsByName, Config->DefaultMappingContext->GetMappings(), KeysByAction);
    return KeysByAction;
}

TArray<FEnhancedActionKeyMapping> UBreakerGameSettingsLibrary::BuildOverriddenMappings(
    const TArray<TPair<FName, const UInputAction*>>& ActionsByName,
    const TArray<FEnhancedActionKeyMapping>& DefaultMappings,
    const TMap<FName, FKey>& Overrides)
{
    // Pointer -> name, same single-walk shape ResolveDefaultKeysByAction uses.
    TMap<const UInputAction*, FName> NameByAction;
    for (const TPair<FName, const UInputAction*>& Pair : ActionsByName)
    {
        if (Pair.Value)
        {
            NameByAction.Add(Pair.Value, Pair.Key);
        }
    }

    // The key each action's row on the settings screen SHOWS: its first valid
    // key in mapping order. That is the key an override replaces — and only
    // that key, so WASD's A/S/D and any gamepad alternate keep their defaults
    // when the displayed key moves.
    TMap<FName, FKey> FirstDefaultKey;
    for (const FEnhancedActionKeyMapping& Mapping : DefaultMappings)
    {
        const FName* Name = NameByAction.Find(Mapping.Action.Get());
        if (Name && Mapping.Key.IsValid() && !FirstDefaultKey.Contains(*Name))
        {
            FirstDefaultKey.Add(*Name, Mapping.Key);
        }
    }

    TArray<FEnhancedActionKeyMapping> Result = DefaultMappings;
    for (FEnhancedActionKeyMapping& Mapping : Result)
    {
        const FName* Name = NameByAction.Find(Mapping.Action.Get());
        if (!Name)
        {
            continue;
        }
        const FKey* Override = Overrides.Find(*Name);
        const FKey* Shown = FirstDefaultKey.Find(*Name);
        // A 2D-axis action can carry its shown key on more than one row (the
        // same key under different modifier stacks), so EVERY row holding the
        // shown key moves — replacing only the first row would leave the old
        // key half-bound, pressing both the old and the new key at once.
        if (Override && Override->IsValid() && Shown && Mapping.Key == *Shown)
        {
            Mapping.Key = *Override;
        }
    }
    return Result;
}

UInputMappingContext* UBreakerGameSettingsLibrary::BuildRuntimeMappingContext(
    const UBreakerInputConfig* Config, const TMap<FName, FKey>& Overrides, UObject* Outer)
{
    // Nothing to override means "use the default asset itself" — signalled as
    // nullptr so the caller registers the authored context and no clone
    // exists to drift from it.
    if (!Config || !Config->DefaultMappingContext || Overrides.Num() == 0)
    {
        return nullptr;
    }

    TArray<TPair<FName, const UInputAction*>> ActionsByName;
    ListConfigActions(Config, ActionsByName);
    const TArray<FEnhancedActionKeyMapping> Rewritten =
        BuildOverriddenMappings(ActionsByName, Config->DefaultMappingContext->GetMappings(), Overrides);

    UInputMappingContext* Context = NewObject<UInputMappingContext>(
        Outer ? Outer : GetTransientPackage(), NAME_None, RF_Transient);
    for (const FEnhancedActionKeyMapping& Source : Rewritten)
    {
        // MapKey appends the row; the struct copy then carries over the whole
        // mapping (modifier/trigger stacks, mappable settings) on top of the
        // action+key MapKey set — including Source's key, which is already
        // the overridden one.
        FEnhancedActionKeyMapping& Row = Context->MapKey(Source.Action.Get(), Source.Key);
        Row = Source;
        // Modifiers and triggers are INSTANCED objects. The copied pointers
        // still aim at the asset's instances; stateful ones (smoothing,
        // holds) must not be shared between the asset and this clone, so
        // each gets its own copy outered to the clone.
        for (TObjectPtr<UInputModifier>& Modifier : Row.Modifiers)
        {
            if (Modifier)
            {
                Modifier = DuplicateObject<UInputModifier>(Modifier, Context);
            }
        }
        for (TObjectPtr<UInputTrigger>& Trigger : Row.Triggers)
        {
            if (Trigger)
            {
                Trigger = DuplicateObject<UInputTrigger>(Trigger, Context);
            }
        }
    }
    return Context;
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

FOnBreakerKeybindOverridesChanged& UBreakerGameSettings::OnKeybindOverridesChanged()
{
    static FOnBreakerKeybindOverridesChanged Delegate;
    return Delegate;
}

void UBreakerGameSettings::SetKeybindOverride(FName Action, FKey Key)
{
    KeybindOverrides.Add(Action, Key);
    OnKeybindOverridesChanged().Broadcast(KeybindOverrides);
}

void UBreakerGameSettings::ClearKeybindOverride(FName Action)
{
    // Only a REMOVAL is a change worth rebuilding a live mapping context
    // over — clearing an action that was never overridden is a no-op.
    if (KeybindOverrides.Remove(Action) > 0)
    {
        OnKeybindOverridesChanged().Broadcast(KeybindOverrides);
    }
}

void UBreakerGameSettings::ResetKeybindsToDefault()
{
    if (KeybindOverrides.Num() > 0)
    {
        KeybindOverrides.Empty();
        OnKeybindOverridesChanged().Broadcast(KeybindOverrides);
    }
}
