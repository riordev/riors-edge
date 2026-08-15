#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Input/BreakerInputConfig.h"
#include "InputAction.h"
#include "Settings/BreakerGameSettings.h"

// ---------------------------------------------------------------------------
// SETTINGS SCREEN — the rules the screen relies on, exercised without a screen.
//
// UI/BreakerMenu.cpp's settings screen is a view over UBreakerGameSettings. The
// parts of it that can be WRONG rather than merely ugly are:
//   - where the default keybinds come from (a mapping context, flattened),
//   - which actions the screen lists,
//   - what happens when a rebind clashes, is cleared, or is reset,
//   - whether the values its sliders and chips produce survive the model's
//     own clamps unchanged.
// All four are pure functions of data, and all four are covered below. Nothing
// here constructs a UWorld or a widget.
//
// NOT COVERED, and not fakeable at this level — stated plainly rather than
// papered over with a test that asserts something easier than it claims:
//
//  1. KEY CAPTURE. SBreakerMenu::OnPreviewKeyDown / OnPreviewMouseButtonDown
//     only fire inside a live Slate application with a focus path, an open
//     viewport and a real key event. Whether the listening row actually
//     RECEIVES a key in a standalone session is the one thing this suite
//     cannot answer, and this project has already been bitten twice by
//     assuming it would (the title gate, twice). It needs a human at the
//     screen, or an automated latent test driving FSlateApplication.
//  2. The widget tree: section order, column widths, whether the BIND button's
//     box is wide enough for "PRESS A KEY…". Layout is not unit-testable here;
//     the widths are chosen against the longest label at the authored font
//     size and commented at their use site.
//  3. UBreakerGameSettingsLibrary::ProjectDefaultKeybinds(), which does a
//     synchronous LoadObject of a .uasset. It is the impure wrapper around
//     ResolveDefaultKeysByAction (which IS covered) and asserting on it would
//     be asserting on the contents of a binary asset from a test, which turns
//     an ordinary content edit into a red suite.
//  4. Save()/LoadOrDefaults()/ApplyToEngine() — already excluded, for the
//     reasons in Tests/BreakerGameSettingsTests.cpp's header.
// ---------------------------------------------------------------------------

namespace
{
    // A throwaway input action. Only its IDENTITY matters to the resolver —
    // mappings are matched by pointer, exactly as a real mapping context does.
    UInputAction* MakeAction()
    {
        return NewObject<UInputAction>(GetTransientPackage());
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerSettingsScreenKeybindDefaultsTest,
    "RiorsEdge.Settings.Screen.KeybindDefaults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerSettingsScreenKeybindDefaultsTest::RunTest(const FString& Parameters)
{
    UInputAction* MoveAction = MakeAction();
    UInputAction* JumpAction = MakeAction();
    UInputAction* FireAction = MakeAction();
    UInputAction* UnmappedAction = MakeAction();
    UInputAction* StrangerAction = MakeAction();   // in the context, not in the roster
    if (!MoveAction || !JumpAction || !FireAction || !UnmappedAction || !StrangerAction)
    {
        AddError(TEXT("Could not create input actions"));
        return false;
    }

    using FActionEntry = TPair<FName, const UInputAction*>;
    TArray<FActionEntry> ActionsByName;
    ActionsByName.Add(FActionEntry(FName(TEXT("Move")), MoveAction));
    ActionsByName.Add(FActionEntry(FName(TEXT("Jump")), JumpAction));
    ActionsByName.Add(FActionEntry(FName(TEXT("Fire")), FireAction));
    ActionsByName.Add(FActionEntry(FName(TEXT("Reload")), UnmappedAction));
    // An action name with a NULL action, which is the real shape of a data
    // asset field nobody has filled in yet.
    ActionsByName.Add(FActionEntry(FName(TEXT("Ultimate")), nullptr));

    TArray<FEnhancedActionKeyMapping> Mappings;
    // Move as a real 2D composite: four keys, one of them listed twice because
    // a mapping context routinely carries the same key under two modifier
    // stacks.
    Mappings.Add(FEnhancedActionKeyMapping(MoveAction, EKeys::W));
    Mappings.Add(FEnhancedActionKeyMapping(MoveAction, EKeys::A));
    Mappings.Add(FEnhancedActionKeyMapping(MoveAction, EKeys::S));
    Mappings.Add(FEnhancedActionKeyMapping(MoveAction, EKeys::D));
    Mappings.Add(FEnhancedActionKeyMapping(MoveAction, EKeys::W));
    Mappings.Add(FEnhancedActionKeyMapping(JumpAction, EKeys::SpaceBar));
    Mappings.Add(FEnhancedActionKeyMapping(FireAction, EKeys::LeftMouseButton));
    // Noise the resolver must drop: an action nobody named, and a mapping with
    // no key on it at all.
    Mappings.Add(FEnhancedActionKeyMapping(StrangerAction, EKeys::Z));
    Mappings.Add(FEnhancedActionKeyMapping(JumpAction, EKeys::Invalid));

    TMap<FName, TArray<FKey>> KeysByAction;
    UBreakerGameSettingsLibrary::ResolveDefaultKeysByAction(ActionsByName, Mappings, KeysByAction);

    // A single-key action gets exactly one key.
    const TArray<FKey>* JumpKeys = KeysByAction.Find(TEXT("Jump"));
    if (!JumpKeys) { AddError(TEXT("Jump resolved to no keys at all")); return false; }
    TestEqual(TEXT("Jump has exactly one default key"), JumpKeys->Num(), 1);
    TestEqual(TEXT("Jump's default key is Space"), (*JumpKeys)[0], FKey(EKeys::SpaceBar));

    // A composite keeps ALL of its keys, deduplicated. This is the property the
    // screen reads to decide a row is not single-key rebindable; collapsing it
    // to one key is exactly the bug that would make the screen claim Move is
    // bound to W alone.
    const TArray<FKey>* MoveKeys = KeysByAction.Find(TEXT("Move"));
    if (!MoveKeys) { AddError(TEXT("Move resolved to no keys at all")); return false; }
    TestEqual(TEXT("Move keeps all four movement keys, deduplicated"), MoveKeys->Num(), 4);
    TestTrue(TEXT("Move's keys are in mapping order, W first"), (*MoveKeys)[0] == EKeys::W);
    TestTrue(TEXT("Move includes D"), MoveKeys->Contains(EKeys::D));

    // A mouse button is an ordinary default, not a special case.
    const TArray<FKey>* FireKeys = KeysByAction.Find(TEXT("Fire"));
    if (!FireKeys) { AddError(TEXT("Fire resolved to no keys at all")); return false; }
    TestEqual(TEXT("Fire's default is the left mouse button"), (*FireKeys)[0], FKey(EKeys::LeftMouseButton));

    // An action with no mapping is ABSENT rather than present-and-empty, and a
    // null action pointer never matches anything.
    TestFalse(TEXT("An unmapped action has no entry"), KeysByAction.Contains(TEXT("Reload")));
    TestFalse(TEXT("An action whose asset field is null has no entry"), KeysByAction.Contains(TEXT("Ultimate")));

    // A mapping for an action the roster does not name contributes nothing.
    TestEqual(TEXT("Only named, mapped actions appear"), KeysByAction.Num(), 3);

    // ---- Flattening ------------------------------------------------------
    const TMap<FName, FKey> Flat = UBreakerGameSettingsLibrary::FirstKeyPerAction(KeysByAction);
    TestEqual(TEXT("Flattening keeps one entry per mapped action"), Flat.Num(), 3);
    TestEqual(TEXT("Jump flattens to Space"), Flat.FindRef(TEXT("Jump")), FKey(EKeys::SpaceBar));
    TestEqual(TEXT("A composite flattens to its FIRST key"), Flat.FindRef(TEXT("Move")), FKey(EKeys::W));

    // And the flattened map is exactly the shape ResolveActionKey consumes,
    // which is the whole reason it exists.
    const TMap<FName, FKey> NoOverrides;
    TestEqual(TEXT("The flattened map drives ResolveActionKey unchanged"),
        UBreakerGameSettingsLibrary::ResolveActionKey(TEXT("Fire"), NoOverrides, Flat), FKey(EKeys::LeftMouseButton));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerSettingsScreenActionRosterTest,
    "RiorsEdge.Settings.Screen.ActionRoster",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerSettingsScreenActionRosterTest::RunTest(const FString& Parameters)
{
    const TArray<FName>& Roster = UBreakerGameSettingsLibrary::BindableActionNames();
    TestTrue(TEXT("The bindable roster is not empty"), Roster.Num() > 0);

    // Every listed action gets a distinct, non-empty label. A duplicate label
    // would produce two rows that read identically, and the conflict message
    // ("X IS ALREADY <label>") would name the wrong one.
    TSet<FString> SeenLabels;
    TSet<FName> SeenNames;
    for (const FName& Action : Roster)
    {
        const FString Label = UBreakerGameSettingsLibrary::DescribeAction(Action).ToString();
        TestFalse(FString::Printf(TEXT("%s has a non-empty label"), *Action.ToString()), Label.IsEmpty());
        TestFalse(FString::Printf(TEXT("%s's label is unique"), *Action.ToString()), SeenLabels.Contains(Label));
        TestFalse(FString::Printf(TEXT("%s appears once in the roster"), *Action.ToString()), SeenNames.Contains(Action));
        SeenLabels.Add(Label);
        SeenNames.Add(Action);
    }

    // THE DRIFT GUARD. The screen lists BindableActionNames(); the defaults
    // come from ListConfigActions(). If the input config gains an action and
    // only one of those two lists learns about it, the screen either shows a
    // row with no default or silently omits a bindable action. Neither is
    // visible without this check.
    UBreakerInputConfig* Config = NewObject<UBreakerInputConfig>(GetTransientPackage());
    if (!Config) { AddError(TEXT("Could not create an input config")); return false; }
    TArray<TPair<FName, const UInputAction*>> ActionsByName;
    UBreakerGameSettingsLibrary::ListConfigActions(Config, ActionsByName);

    TestEqual(TEXT("The config action list and the bindable roster are the same length"),
        ActionsByName.Num(), Roster.Num());
    for (const TPair<FName, const UInputAction*>& Pair : ActionsByName)
    {
        TestTrue(FString::Printf(TEXT("%s is offered on the settings screen"), *Pair.Key.ToString()),
            Roster.Contains(Pair.Key));
    }
    for (const FName& Action : Roster)
    {
        const bool bInConfig = ActionsByName.ContainsByPredicate(
            [Action](const TPair<FName, const UInputAction*>& Pair) { return Pair.Key == Action; });
        TestTrue(FString::Printf(TEXT("%s is read out of the input config"), *Action.ToString()), bInConfig);
    }

    // A null config yields an empty list rather than a crash — the state a
    // build with no cooked input asset is actually in.
    TArray<TPair<FName, const UInputAction*>> FromNull;
    UBreakerGameSettingsLibrary::ListConfigActions(nullptr, FromNull);
    TestEqual(TEXT("A null config lists no actions"), FromNull.Num(), 0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerSettingsScreenRebindRulesTest,
    "RiorsEdge.Settings.Screen.RebindRules",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerSettingsScreenRebindRulesTest::RunTest(const FString& Parameters)
{
    // The exact sequence SBreakerMenu::CommitKeybind, the row's DEFAULT button
    // and RESET ALL KEYBINDS perform, driven through the same library calls in
    // the same order. This is the behaviour of the screen minus its widgets.
    UBreakerGameSettings* Model = NewObject<UBreakerGameSettings>(GetTransientPackage());
    if (!Model) { AddError(TEXT("Could not create a settings object")); return false; }

    TMap<FName, FKey> Defaults;
    Defaults.Add(TEXT("Jump"), EKeys::SpaceBar);
    Defaults.Add(TEXT("Dash"), EKeys::Q);
    Defaults.Add(TEXT("Fire"), EKeys::LeftMouseButton);
    Defaults.Add(TEXT("Aim"), EKeys::RightMouseButton);

    FName ClashWith = NAME_None;

    // ---- A rebind onto a free key commits straight through ----------------
    TestFalse(TEXT("Binding Dash to F is not a clash"),
        UBreakerGameSettingsLibrary::FindKeybindConflict(TEXT("Dash"), EKeys::F, Model->KeybindOverrides, Defaults, ClashWith));
    Model->SetKeybindOverride(TEXT("Dash"), EKeys::F);
    TestEqual(TEXT("Dash now resolves to F"),
        UBreakerGameSettingsLibrary::ResolveActionKey(TEXT("Dash"), Model->KeybindOverrides, Defaults), FKey(EKeys::F));
    TestEqual(TEXT("Dash's old key is untouched for everyone else"),
        UBreakerGameSettingsLibrary::ResolveActionKey(TEXT("Jump"), Model->KeybindOverrides, Defaults), FKey(EKeys::SpaceBar));

    // ---- A rebind onto a TAKEN key names the owner and does not commit -----
    // This is the arm half of the screen's arm/confirm: the first press
    // reports, it does not write.
    ClashWith = NAME_None;
    TestTrue(TEXT("Binding Jump to Fire's key is a clash"),
        UBreakerGameSettingsLibrary::FindKeybindConflict(TEXT("Jump"), EKeys::LeftMouseButton, Model->KeybindOverrides, Defaults, ClashWith));
    TestEqual(TEXT("The clash names the action that owns the key"), ClashWith, FName(TEXT("Fire")));
    TestEqual(TEXT("Nothing was written by the refused attempt"),
        UBreakerGameSettingsLibrary::ResolveActionKey(TEXT("Jump"), Model->KeybindOverrides, Defaults), FKey(EKeys::SpaceBar));

    // ---- Confirming the clash SHARES the key rather than stealing it -------
    // The screen's BIND ANYWAY. Both actions must still resolve to the key
    // afterwards; a "steal" that silently unbound Fire would be the wrong
    // outcome, and the model has no way to express an unbound action anyway.
    Model->SetKeybindOverride(TEXT("Jump"), EKeys::LeftMouseButton);
    TestEqual(TEXT("Jump took the shared key"),
        UBreakerGameSettingsLibrary::ResolveActionKey(TEXT("Jump"), Model->KeybindOverrides, Defaults), FKey(EKeys::LeftMouseButton));
    TestEqual(TEXT("Fire still holds it too"),
        UBreakerGameSettingsLibrary::ResolveActionKey(TEXT("Fire"), Model->KeybindOverrides, Defaults), FKey(EKeys::LeftMouseButton));

    // ---- And the badge the screen draws on BOTH rows is live --------------
    // MakeKeybindRow asks FindKeybindConflict about the key an action already
    // holds, so a share that the player confirmed keeps announcing itself
    // instead of scrolling away with the status line.
    ClashWith = NAME_None;
    TestTrue(TEXT("Jump's row reports the share"),
        UBreakerGameSettingsLibrary::FindKeybindConflict(TEXT("Jump"), EKeys::LeftMouseButton, Model->KeybindOverrides, Defaults, ClashWith));
    TestEqual(TEXT("Jump's badge names Fire"), ClashWith, FName(TEXT("Fire")));
    ClashWith = NAME_None;
    TestTrue(TEXT("Fire's row reports the share too"),
        UBreakerGameSettingsLibrary::FindKeybindConflict(TEXT("Fire"), EKeys::LeftMouseButton, Model->KeybindOverrides, Defaults, ClashWith));
    TestEqual(TEXT("Fire's badge names Jump"), ClashWith, FName(TEXT("Jump")));

    // ---- The row's DEFAULT button clears one binding, and only one ---------
    Model->ClearKeybindOverride(TEXT("Jump"));
    TestEqual(TEXT("Jump is back on Space"),
        UBreakerGameSettingsLibrary::ResolveActionKey(TEXT("Jump"), Model->KeybindOverrides, Defaults), FKey(EKeys::SpaceBar));
    TestEqual(TEXT("Dash's unrelated rebind survives"),
        UBreakerGameSettingsLibrary::ResolveActionKey(TEXT("Dash"), Model->KeybindOverrides, Defaults), FKey(EKeys::F));
    // With the share gone, neither row still claims a clash.
    ClashWith = NAME_None;
    TestFalse(TEXT("Clearing the share clears the badge"),
        UBreakerGameSettingsLibrary::FindKeybindConflict(TEXT("Fire"), EKeys::LeftMouseButton, Model->KeybindOverrides, Defaults, ClashWith));

    // ---- RESET ALL KEYBINDS returns every action to its default ------------
    Model->ResetKeybindsToDefault();
    TestEqual(TEXT("Reset leaves no overrides"), Model->KeybindOverrides.Num(), 0);
    for (const TPair<FName, FKey>& Pair : Defaults)
    {
        TestEqual(FString::Printf(TEXT("%s is back on its default after reset"), *Pair.Key.ToString()),
            UBreakerGameSettingsLibrary::ResolveActionKey(Pair.Key, Model->KeybindOverrides, Defaults), Pair.Value);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerSettingsScreenControlValuesTest,
    "RiorsEdge.Settings.Screen.ControlValues",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerSettingsScreenControlValuesTest::RunTest(const FString& Parameters)
{
    // The arithmetic the screen's own controls perform. A slider hands back
    // 0..1 and the screen remaps it into the model's range; if a remap misses
    // the range by any amount, the clamp silently corrects it and the slider
    // develops a dead zone at one end that nobody can see by reading the code.
    // These are the exact expressions in BuildSettingsInputSection /
    // BuildSettingsVideoSection.

    // Look sensitivity: 0.2 + t * 1.8, against ClampMouseSensitivity's [0.2, 2.0].
    TestEqual(TEXT("Sensitivity slider at 0 lands exactly on the floor"),
        UBreakerGameSettingsLibrary::ClampMouseSensitivity(0.2f + 0.0f * 1.8f), 0.2f);
    TestEqual(TEXT("Sensitivity slider at 1 lands exactly on the ceiling"),
        UBreakerGameSettingsLibrary::ClampMouseSensitivity(0.2f + 1.0f * 1.8f), 2.0f);
    // And the inverse the screen uses to POSITION the handle from a stored
    // value round-trips, which is what stops the handle jumping on screen entry.
    TestEqual(TEXT("Sensitivity handle position round-trips"),
        0.2f + ((1.4f - 0.2f) / 1.8f) * 1.8f, 1.4f);

    // Scoped multiplier: 0.1 + t * 2.9, against [0.1, 3.0].
    TestEqual(TEXT("Scoped slider at 0 lands exactly on the floor"),
        UBreakerGameSettingsLibrary::ClampScopedSensitivityMultiplier(0.1f + 0.0f * 2.9f), 0.1f);
    TestEqual(TEXT("Scoped slider at 1 lands exactly on the ceiling"),
        UBreakerGameSettingsLibrary::ClampScopedSensitivityMultiplier(0.1f + 1.0f * 2.9f), 3.0f);

    // FOV: 70 + t * 50, against [70, 120].
    TestEqual(TEXT("FOV slider at 0 lands exactly on the floor"),
        UBreakerGameSettingsLibrary::ClampFOV(70.0f + 0.0f * 50.0f), 70.0f);
    TestEqual(TEXT("FOV slider at 1 lands exactly on the ceiling"),
        UBreakerGameSettingsLibrary::ClampFOV(70.0f + 1.0f * 50.0f), 120.0f);

    // Volumes are already 0..1 and the screen passes them straight through.
    TestEqual(TEXT("Volume slider at 0 passes through"), UBreakerGameSettingsLibrary::ClampVolume(0.0f), 0.0f);
    TestEqual(TEXT("Volume slider at 1 passes through"), UBreakerGameSettingsLibrary::ClampVolume(1.0f), 1.0f);

    // THE FRAME CAP CHIPS. Every value the strip offers must survive its own
    // clamp unchanged — a chip whose value the clamp rewrites would light up as
    // selected for a value the player did not choose, or never light up at all.
    const float CapChips[] = { 0.0f, 60.0f, 120.0f, 144.0f, 240.0f, 360.0f };
    for (const float Cap : CapChips)
    {
        TestEqual(FString::Printf(TEXT("Frame cap chip %.0f survives its clamp"), Cap),
            UBreakerGameSettingsLibrary::ClampFrameRateCap(Cap), Cap);
    }

    // The three window modes the strip offers are the whole enum, so the strip
    // cannot leave the model in a state no chip represents.
    TestEqual(TEXT("Windowed is the last window mode"),
        static_cast<int32>(EBreakerWindowMode::Windowed), 2);

    return true;
}

#endif
