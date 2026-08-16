#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Settings/BreakerGameSettings.h"
#include "Input/BreakerInputConfig.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "UObject/Package.h"

// ---------------------------------------------------------------------------
// SETTINGS MODEL — clamps, keybind override layering/conflict detection, and
// the object's own accessors. Nothing here constructs a UWorld: the object
// under test is a plain UObject (NewObject<UBreakerGameSettings>(GetTransientPackage())),
// and every static function on UBreakerGameSettingsLibrary is pure.
//
// NOT covered by this suite:
//  - ApplyToEngine(): touches UGameUserSettings and the main audio device,
//    neither of which exist in an automation-test process the way this
//    suite runs (no engine subsystems spun up). Would need a full editor/
//    game world to exercise honestly.
//  - LoadOrDefaults()/Save(): both read/write GGameUserSettingsIni, the
//    developer's REAL GameUserSettings.ini on disk. Exercising them here
//    would mean either mutating that real file during a test run or
//    threading a fake ini path through GConfig, neither of which this pass
//    attempts. The migration behaviour they implement (reading the legacy
//    "RiorsEdge.Playtest" section) is documented in BreakerGameSettings.cpp
//    instead and left for an integration-level check.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerGameSettingsClampTest,
    "RiorsEdge.Settings.Clamp",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerGameSettingsClampTest::RunTest(const FString& Parameters)
{
    // Mouse sensitivity: [0.2, 2.0], matching the existing settings-screen
    // slider's range (Characters/BreakerCharacter.cpp:222, 959).
    TestEqual(TEXT("Sensitivity floors at 0.2"), UBreakerGameSettingsLibrary::ClampMouseSensitivity(-5.0f), 0.2f);
    TestEqual(TEXT("Sensitivity ceilings at 2.0"), UBreakerGameSettingsLibrary::ClampMouseSensitivity(50.0f), 2.0f);
    TestEqual(TEXT("Sensitivity mid-range passes through"), UBreakerGameSettingsLibrary::ClampMouseSensitivity(1.0f), 1.0f);

    // Scoped/ADS sensitivity multiplier: [0.1, 3.0].
    TestEqual(TEXT("Scoped multiplier floors at 0.1"), UBreakerGameSettingsLibrary::ClampScopedSensitivityMultiplier(0.0f), 0.1f);
    TestEqual(TEXT("Scoped multiplier ceilings at 3.0"), UBreakerGameSettingsLibrary::ClampScopedSensitivityMultiplier(999.0f), 3.0f);

    // FOV: [70, 120], matching ABreakerCharacter::BaseFieldOfView
    // (Characters/BreakerCharacter.cpp:223, 944-945, 961).
    TestEqual(TEXT("FOV floors at 70"), UBreakerGameSettingsLibrary::ClampFOV(10.0f), 70.0f);
    TestEqual(TEXT("FOV ceilings at 120"), UBreakerGameSettingsLibrary::ClampFOV(400.0f), 120.0f);
    TestEqual(TEXT("FOV mid-range passes through"), UBreakerGameSettingsLibrary::ClampFOV(90.0f), 90.0f);

    // Frame-rate cap: 0 is the uncapped sentinel and always valid; a
    // positive request is bounded to [30, 360].
    TestEqual(TEXT("A zero frame cap stays uncapped"), UBreakerGameSettingsLibrary::ClampFrameRateCap(0.0f), 0.0f);
    TestEqual(TEXT("A negative frame cap collapses to uncapped"), UBreakerGameSettingsLibrary::ClampFrameRateCap(-30.0f), 0.0f);
    TestEqual(TEXT("Frame cap floors at 30"), UBreakerGameSettingsLibrary::ClampFrameRateCap(5.0f), 30.0f);
    TestEqual(TEXT("Frame cap ceilings at 360"), UBreakerGameSettingsLibrary::ClampFrameRateCap(1000.0f), 360.0f);

    // Volumes: [0, 1], shared by master/effects/music.
    TestEqual(TEXT("Volume floors at 0"), UBreakerGameSettingsLibrary::ClampVolume(-1.0f), 0.0f);
    TestEqual(TEXT("Volume ceilings at 1"), UBreakerGameSettingsLibrary::ClampVolume(5.0f), 1.0f);
    TestEqual(TEXT("Volume mid-range passes through"), UBreakerGameSettingsLibrary::ClampVolume(0.5f), 0.5f);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerGameSettingsKeybindLayeringTest,
    "RiorsEdge.Settings.KeybindLayering",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerGameSettingsKeybindLayeringTest::RunTest(const FString& Parameters)
{
    TMap<FName, FKey> Defaults;
    Defaults.Add(TEXT("Fire"), EKeys::LeftMouseButton);
    Defaults.Add(TEXT("Aim"), EKeys::RightMouseButton);
    Defaults.Add(TEXT("Jump"), EKeys::SpaceBar);

    TMap<FName, FKey> Overrides;

    // An action with no override falls through to its default.
    TestEqual(TEXT("Jump falls through to its default with no override"),
        UBreakerGameSettingsLibrary::ResolveActionKey(TEXT("Jump"), Overrides, Defaults), FKey(EKeys::SpaceBar));

    // An override wins over the default.
    Overrides.Add(TEXT("Jump"), EKeys::F);
    TestEqual(TEXT("An override wins over the default"),
        UBreakerGameSettingsLibrary::ResolveActionKey(TEXT("Jump"), Overrides, Defaults), FKey(EKeys::F));

    // Other actions are unaffected by an unrelated override.
    TestEqual(TEXT("Fire is untouched by Jump's override"),
        UBreakerGameSettingsLibrary::ResolveActionKey(TEXT("Fire"), Overrides, Defaults), FKey(EKeys::LeftMouseButton));

    // An action absent from BOTH maps resolves to an invalid key rather than
    // asserting or defaulting to something arbitrary.
    TestFalse(TEXT("An unknown action resolves to an invalid key"),
        UBreakerGameSettingsLibrary::ResolveActionKey(TEXT("Nonexistent"), Overrides, Defaults).IsValid());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerGameSettingsKeybindConflictTest,
    "RiorsEdge.Settings.KeybindConflict",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerGameSettingsKeybindConflictTest::RunTest(const FString& Parameters)
{
    TMap<FName, FKey> Defaults;
    Defaults.Add(TEXT("Fire"), EKeys::LeftMouseButton);
    Defaults.Add(TEXT("Aim"), EKeys::RightMouseButton);
    Defaults.Add(TEXT("Jump"), EKeys::SpaceBar);

    TMap<FName, FKey> Overrides;
    FName OutConflict;

    // Rebinding an action to a key already used by a DIFFERENT action is a
    // conflict, and the offending action is reported.
    TestTrue(TEXT("Binding Jump to Fire's default key is a conflict"),
        UBreakerGameSettingsLibrary::FindKeybindConflict(TEXT("Jump"), EKeys::LeftMouseButton, Overrides, Defaults, OutConflict));
    TestEqual(TEXT("The conflicting action is reported"), OutConflict, FName(TEXT("Fire")));

    // Rebinding an action to the key it ALREADY holds is explicitly not a
    // conflict with itself.
    OutConflict = NAME_None;
    TestFalse(TEXT("Rebinding Jump to its own current key is not a conflict"),
        UBreakerGameSettingsLibrary::FindKeybindConflict(TEXT("Jump"), EKeys::SpaceBar, Overrides, Defaults, OutConflict));
    TestEqual(TEXT("No conflicting action is reported"), OutConflict, NAME_None);

    // A key nobody uses is not a conflict.
    TestFalse(TEXT("An unused key is not a conflict"),
        UBreakerGameSettingsLibrary::FindKeybindConflict(TEXT("Jump"), EKeys::Q, Overrides, Defaults, OutConflict));

    // The conflict check sees OVERRIDES too, not just defaults: give Aim an
    // override onto Q, then binding Jump to Q must conflict with Aim, not
    // silently pass because Aim's DEFAULT (RMB) doesn't clash.
    Overrides.Add(TEXT("Aim"), EKeys::Q);
    TestTrue(TEXT("An overridden action's key is still detected as a conflict"),
        UBreakerGameSettingsLibrary::FindKeybindConflict(TEXT("Jump"), EKeys::Q, Overrides, Defaults, OutConflict));
    TestEqual(TEXT("The overridden action is reported, not its stale default"), OutConflict, FName(TEXT("Aim")));

    // And once Aim has moved off its default (RMB) via that same override,
    // RMB is free again — nothing else conflicts with it.
    TestFalse(TEXT("A default vacated by an override is free"),
        UBreakerGameSettingsLibrary::FindKeybindConflict(TEXT("Jump"), EKeys::RightMouseButton, Overrides, Defaults, OutConflict));

    // An invalid key can never conflict.
    TestFalse(TEXT("An invalid key is never a conflict"),
        UBreakerGameSettingsLibrary::FindKeybindConflict(TEXT("Jump"), EKeys::Invalid, Overrides, Defaults, OutConflict));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerGameSettingsAccessorRoundTripTest,
    "RiorsEdge.Settings.AccessorRoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerGameSettingsAccessorRoundTripTest::RunTest(const FString& Parameters)
{
    UBreakerGameSettings* Settings = NewObject<UBreakerGameSettings>(GetTransientPackage());
    if (!Settings) { AddError(TEXT("Could not create a settings object")); return false; }

    TMap<FName, FKey> Defaults;
    Defaults.Add(TEXT("Fire"), EKeys::LeftMouseButton);
    Defaults.Add(TEXT("Sprint"), EKeys::LeftShift);

    // Freshly constructed: no overrides, so every action falls through.
    TestEqual(TEXT("A fresh object has no keybind overrides"), Settings->KeybindOverrides.Num(), 0);
    TestEqual(TEXT("Fire resolves to its default before any override"),
        UBreakerGameSettingsLibrary::ResolveActionKey(TEXT("Fire"), Settings->KeybindOverrides, Defaults), FKey(EKeys::LeftMouseButton));

    // Set, then read back through the same accessor path a UI would use.
    Settings->SetKeybindOverride(TEXT("Fire"), EKeys::G);
    TestEqual(TEXT("SetKeybindOverride round-trips through KeybindOverrides"),
        UBreakerGameSettingsLibrary::ResolveActionKey(TEXT("Fire"), Settings->KeybindOverrides, Defaults), FKey(EKeys::G));

    // Replacing an existing override overwrites rather than duplicating.
    Settings->SetKeybindOverride(TEXT("Fire"), EKeys::H);
    TestEqual(TEXT("A second SetKeybindOverride call replaces the first"),
        UBreakerGameSettingsLibrary::ResolveActionKey(TEXT("Fire"), Settings->KeybindOverrides, Defaults), FKey(EKeys::H));
    TestEqual(TEXT("Replacing an override does not add a second entry"), Settings->KeybindOverrides.Num(), 1);

    // Clearing one action's override falls it back to default without
    // touching any other action's override.
    Settings->SetKeybindOverride(TEXT("Sprint"), EKeys::LeftControl);
    Settings->ClearKeybindOverride(TEXT("Fire"));
    TestEqual(TEXT("A cleared override falls back to default"),
        UBreakerGameSettingsLibrary::ResolveActionKey(TEXT("Fire"), Settings->KeybindOverrides, Defaults), FKey(EKeys::LeftMouseButton));
    TestEqual(TEXT("An unrelated override survives ClearKeybindOverride"),
        UBreakerGameSettingsLibrary::ResolveActionKey(TEXT("Sprint"), Settings->KeybindOverrides, Defaults), FKey(EKeys::LeftControl));

    // Reset-to-default drops every override; every action falls back to its
    // default in one call, with no per-action bookkeeping required by the
    // caller (Sprint had a live override going into this call, Fire did
    // not — both end up at their defaults).
    Settings->ResetKeybindsToDefault();
    TestEqual(TEXT("ResetKeybindsToDefault empties the override map"), Settings->KeybindOverrides.Num(), 0);
    TestEqual(TEXT("Fire is back at its default after reset"),
        UBreakerGameSettingsLibrary::ResolveActionKey(TEXT("Fire"), Settings->KeybindOverrides, Defaults), FKey(EKeys::LeftMouseButton));
    TestEqual(TEXT("Sprint is back at its default after reset"),
        UBreakerGameSettingsLibrary::ResolveActionKey(TEXT("Sprint"), Settings->KeybindOverrides, Defaults), FKey(EKeys::LeftShift));

    // Non-keybind fields round-trip as plain data too: nothing under this
    // model computes them, so a direct write must read back unchanged.
    Settings->MouseSensitivity = UBreakerGameSettingsLibrary::ClampMouseSensitivity(1.4f);
    Settings->FieldOfView = UBreakerGameSettingsLibrary::ClampFOV(100.0f);
    Settings->bInvertVerticalLook = true;
    TestEqual(TEXT("MouseSensitivity round-trips"), Settings->MouseSensitivity, 1.4f);
    TestEqual(TEXT("FieldOfView round-trips"), Settings->FieldOfView, 100.0f);
    TestTrue(TEXT("bInvertVerticalLook round-trips"), Settings->bInvertVerticalLook);

    return true;
}

// ---------------------------------------------------------------------------
// R10 — overrides applied to the LIVE mapping list. BuildOverriddenMappings
// is the pure decision the character's runtime context is rebuilt from: the
// override moves exactly the rows carrying the action's displayed (first
// default) key, and every other row keeps its default byte-for-byte.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerKeybindOverrideMappingTest,
    "RiorsEdge.Settings.KeybindOverrideMapping",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerKeybindOverrideMappingTest::RunTest(const FString& Parameters)
{
    UInputAction* JumpAction = NewObject<UInputAction>(GetTransientPackage());
    UInputAction* MoveAction = NewObject<UInputAction>(GetTransientPackage());
    UInputAction* FireAction = NewObject<UInputAction>(GetTransientPackage());
    UInputAction* ReloadAction = NewObject<UInputAction>(GetTransientPackage());

    TArray<TPair<FName, const UInputAction*>> Actions;
    Actions.Add(TPair<FName, const UInputAction*>(TEXT("Jump"), JumpAction));
    Actions.Add(TPair<FName, const UInputAction*>(TEXT("Move"), MoveAction));
    Actions.Add(TPair<FName, const UInputAction*>(TEXT("Fire"), FireAction));
    Actions.Add(TPair<FName, const UInputAction*>(TEXT("Reload"), ReloadAction));

    // The shape a real context has: Move's displayed key (W) appears on TWO
    // rows (different modifier stacks), Jump carries a keyboard key plus a
    // gamepad alternate, Reload has no rows at all.
    TArray<FEnhancedActionKeyMapping> Defaults;
    Defaults.Add(FEnhancedActionKeyMapping(MoveAction, EKeys::W));
    Defaults.Add(FEnhancedActionKeyMapping(MoveAction, EKeys::W));
    Defaults.Add(FEnhancedActionKeyMapping(MoveAction, EKeys::S));
    Defaults.Add(FEnhancedActionKeyMapping(MoveAction, EKeys::A));
    Defaults.Add(FEnhancedActionKeyMapping(MoveAction, EKeys::D));
    Defaults.Add(FEnhancedActionKeyMapping(JumpAction, EKeys::SpaceBar));
    Defaults.Add(FEnhancedActionKeyMapping(JumpAction, EKeys::Gamepad_FaceButton_Bottom));
    Defaults.Add(FEnhancedActionKeyMapping(FireAction, EKeys::LeftMouseButton));

    // One override: Jump onto F.
    TMap<FName, FKey> Overrides;
    Overrides.Add(TEXT("Jump"), EKeys::F);
    TArray<FEnhancedActionKeyMapping> Result =
        UBreakerGameSettingsLibrary::BuildOverriddenMappings(Actions, Defaults, Overrides);

    TestEqual(TEXT("The rebuilt list keeps every row"), Result.Num(), Defaults.Num());
    TestEqual(TEXT("Jump's displayed key moved to the override"), Result[5].Key, FKey(EKeys::F));
    TestEqual(TEXT("Jump's gamepad alternate keeps its default"), Result[6].Key, FKey(EKeys::Gamepad_FaceButton_Bottom));
    TestEqual(TEXT("Fire is untouched by Jump's override"), Result[7].Key, FKey(EKeys::LeftMouseButton));
    TestEqual(TEXT("Move's W keeps its default"), Result[0].Key, FKey(EKeys::W));

    // A multi-row displayed key moves on EVERY row it holds — a half-moved
    // W would leave forward bound to both the old and the new key.
    Overrides.Empty();
    Overrides.Add(TEXT("Move"), EKeys::Up);
    Result = UBreakerGameSettingsLibrary::BuildOverriddenMappings(Actions, Defaults, Overrides);
    TestEqual(TEXT("Move's first W row moved"), Result[0].Key, FKey(EKeys::Up));
    TestEqual(TEXT("Move's second W row moved with it"), Result[1].Key, FKey(EKeys::Up));
    TestEqual(TEXT("Move's S keeps its default"), Result[2].Key, FKey(EKeys::S));
    TestEqual(TEXT("Move's A keeps its default"), Result[3].Key, FKey(EKeys::A));
    TestEqual(TEXT("Move's D keeps its default"), Result[4].Key, FKey(EKeys::D));
    TestEqual(TEXT("Jump keeps its default with only Move overridden"), Result[5].Key, FKey(EKeys::SpaceBar));

    // An override for an action with no rows changes nothing.
    Overrides.Empty();
    Overrides.Add(TEXT("Reload"), EKeys::V);
    Result = UBreakerGameSettingsLibrary::BuildOverriddenMappings(Actions, Defaults, Overrides);
    for (int32 Index = 0; Index < Result.Num(); ++Index)
    {
        TestEqual(TEXT("An unmapped action's override moves nothing"), Result[Index].Key, Defaults[Index].Key);
    }

    // RESET: an empty override map reproduces the defaults exactly.
    Overrides.Empty();
    Result = UBreakerGameSettingsLibrary::BuildOverriddenMappings(Actions, Defaults, Overrides);
    TestEqual(TEXT("No overrides keeps every row"), Result.Num(), Defaults.Num());
    for (int32 Index = 0; Index < Result.Num(); ++Index)
    {
        TestEqual(TEXT("No overrides reproduces every default key"), Result[Index].Key, Defaults[Index].Key);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerKeybindRuntimeContextTest,
    "RiorsEdge.Settings.KeybindRuntimeContext",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerKeybindRuntimeContextTest::RunTest(const FString& Parameters)
{
    // A minimal config: two actions, one context, authored the way
    // DA_PlayerInputConfig is.
    UInputAction* JumpAction = NewObject<UInputAction>(GetTransientPackage());
    UInputAction* FireAction = NewObject<UInputAction>(GetTransientPackage());
    UInputMappingContext* Authored = NewObject<UInputMappingContext>(GetTransientPackage());
    Authored->MapKey(JumpAction, EKeys::SpaceBar);
    Authored->MapKey(FireAction, EKeys::LeftMouseButton);
    UBreakerInputConfig* Config = NewObject<UBreakerInputConfig>(GetTransientPackage());
    Config->DefaultMappingContext = Authored;
    Config->Jump = JumpAction;
    Config->Fire = FireAction;

    // No overrides: nullptr, meaning "register the authored asset itself" —
    // no clone exists to drift from the asset, and RESET ALL lands here.
    TMap<FName, FKey> Overrides;
    TestNull(TEXT("No overrides builds no clone"),
        UBreakerGameSettingsLibrary::BuildRuntimeMappingContext(Config, Overrides, GetTransientPackage()));

    // One override: a clone whose Jump row carries the override, whose Fire
    // row keeps its default — and the authored asset is untouched.
    Overrides.Add(TEXT("Jump"), EKeys::F);
    UInputMappingContext* Clone =
        UBreakerGameSettingsLibrary::BuildRuntimeMappingContext(Config, Overrides, GetTransientPackage());
    if (!Clone) { AddError(TEXT("An override must build a clone")); return false; }
    TestNotEqual(TEXT("The clone is a different object from the asset"),
        static_cast<UObject*>(Clone), static_cast<UObject*>(Authored));
    TestEqual(TEXT("The clone keeps every row"), Clone->GetMappings().Num(), Authored->GetMappings().Num());
    TestEqual(TEXT("The clone's Jump row carries the override"), Clone->GetMappings()[0].Key, FKey(EKeys::F));
    TestEqual(TEXT("The clone's Jump row still drives the Jump action"),
        Clone->GetMappings()[0].Action.Get(), static_cast<const UInputAction*>(JumpAction));
    TestEqual(TEXT("The clone's Fire row keeps its default"), Clone->GetMappings()[1].Key, FKey(EKeys::LeftMouseButton));
    TestEqual(TEXT("The AUTHORED asset's Jump row is untouched"), Authored->GetMappings()[0].Key, FKey(EKeys::SpaceBar));

    // Degenerate inputs answer nullptr rather than asserting.
    TestNull(TEXT("A null config builds nothing"),
        UBreakerGameSettingsLibrary::BuildRuntimeMappingContext(nullptr, Overrides, GetTransientPackage()));
    Config->DefaultMappingContext = nullptr;
    TestNull(TEXT("A config with no context builds nothing"),
        UBreakerGameSettingsLibrary::BuildRuntimeMappingContext(Config, Overrides, GetTransientPackage()));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerKeybindChangeBroadcastTest,
    "RiorsEdge.Settings.KeybindChangeBroadcast",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerKeybindChangeBroadcastTest::RunTest(const FString& Parameters)
{
    UBreakerGameSettings* Settings = NewObject<UBreakerGameSettings>(GetTransientPackage());
    if (!Settings) { AddError(TEXT("Could not create a settings object")); return false; }

    int32 Broadcasts = 0;
    TMap<FName, FKey> LastSeen;
    FDelegateHandle Handle = UBreakerGameSettings::OnKeybindOverridesChanged().AddLambda(
        [&Broadcasts, &LastSeen](const TMap<FName, FKey>& Overrides)
        {
            ++Broadcasts;
            LastSeen = Overrides;
        });

    // Set fires, carrying the new map.
    Settings->SetKeybindOverride(TEXT("Jump"), EKeys::F);
    TestEqual(TEXT("SetKeybindOverride broadcasts"), Broadcasts, 1);
    TestEqual(TEXT("The broadcast carries the new override"),
        LastSeen.FindRef(TEXT("Jump")), FKey(EKeys::F));

    // Clearing an action that was never overridden is not a change.
    Settings->ClearKeybindOverride(TEXT("Fire"));
    TestEqual(TEXT("Clearing a non-override does not broadcast"), Broadcasts, 1);

    // Clearing a real override fires, and the map it carries has moved on.
    Settings->ClearKeybindOverride(TEXT("Jump"));
    TestEqual(TEXT("Clearing a live override broadcasts"), Broadcasts, 2);
    TestEqual(TEXT("The broadcast map no longer holds the cleared action"), LastSeen.Num(), 0);

    // Reset on an already-empty map is not a change; reset with overrides is.
    Settings->ResetKeybindsToDefault();
    TestEqual(TEXT("Resetting an empty map does not broadcast"), Broadcasts, 2);
    Settings->SetKeybindOverride(TEXT("Aim"), EKeys::Q);
    Settings->ResetKeybindsToDefault();
    TestEqual(TEXT("Reset with live overrides broadcasts"), Broadcasts, 4);
    TestEqual(TEXT("Reset broadcasts an empty map"), LastSeen.Num(), 0);

    // The delegate is static and outlives this test — the lambda captures
    // stack locals, so it MUST come off before they die.
    UBreakerGameSettings::OnKeybindOverridesChanged().Remove(Handle);
    return true;
}

#endif
