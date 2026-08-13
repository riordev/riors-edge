#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Abilities/BreakerAbilityComponent.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Abilities/BreakerAbilityTags.h"
#include "Abilities/BreakerAbility_Lead.h"
#include "Abilities/BreakerAbility_Overdrive.h"
#include "Abilities/BreakerAbility_Skim.h"
#include "Abilities/BreakerGameplayAbility.h"
#include "GameFramework/Actor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAbilityRegistryTest,
    "RiorsEdge.Abilities.FallbackRegistry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAbilityRegistryTest::RunTest(const FString& Parameters)
{
    const TArray<UBreakerAbilityDefinition*>& Registry = UBreakerAbilityDefinition::GetFallbackRegistry();
    TestTrue(TEXT("The fallback registry is populated"), Registry.Num() >= 3);

    TSet<FName> SeenIds;
    for (const UBreakerAbilityDefinition* Definition : Registry)
    {
        if (!Definition)
        {
            AddError(TEXT("A null entry is in the fallback registry"));
            continue;
        }
        TestFalse(TEXT("Every fallback entry has an id"), Definition->AbilityId.IsNone());
        TestFalse(TEXT("Ids are unique"), SeenIds.Contains(Definition->AbilityId));
        SeenIds.Add(Definition->AbilityId);
        TestTrue(TEXT("Every entry has a class"), Definition->ClassId != EBreakerClassId::None);
        TestTrue(TEXT("Every entry has an ability tag"), Definition->AbilityTag.IsValid());
        TestTrue(TEXT("Costs are non-negative"), Definition->ResourceCost >= 0.0f);
        TestTrue(TEXT("Cooldowns are non-negative"), Definition->CooldownSeconds >= 0.0f);
        // A cooldown without a tag cannot be queried by the HUD, and a tag
        // without a cooldown is dead data.
        TestEqual(TEXT("Cooldown seconds and cooldown tag agree"), Definition->HasCooldown(), Definition->CooldownTag.IsValid());
        TestTrue(TEXT("Every registry entry resolves by id"), UBreakerAbilityDefinition::FindFallback(Definition->AbilityId) == Definition);
    }

    TestNull(TEXT("An unknown id resolves to nothing"), UBreakerAbilityDefinition::FindFallback(TEXT("Swift.NotAnAbility")));
    TestNull(TEXT("None resolves to nothing"), UBreakerAbilityDefinition::FindFallback(NAME_None));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAbilityDefinitionValuesTest,
    "RiorsEdge.Abilities.DefinitionValues",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAbilityDefinitionValuesTest::RunTest(const FString& Parameters)
{
    // Values quoted from Docs/Design/Class-Kits.md §1.2.
    const UBreakerAbilityDefinition* Skim = UBreakerAbilityDefinition::FindFallback(TEXT("Swift.Skim"));
    if (!Skim)
    {
        AddError(TEXT("Skim is missing from the fallback registry"));
        return false;
    }
    TestEqual(TEXT("Skim costs 15 Momentum"), Skim->GetResourceCost(), 15.0f);
    TestEqual(TEXT("Skim has a 3s cooldown"), Skim->GetCooldownSeconds(), 3.0f);
    TestTrue(TEXT("Skim is the one implemented proof ability"), Skim->IsImplemented());
    TestTrue(TEXT("Skim's ability class derives from the Breaker base"), Skim->AbilityClass->IsChildOf(UBreakerGameplayAbility::StaticClass()));

    const UBreakerAbilityDefinition* Lead = UBreakerAbilityDefinition::FindFallback(TEXT("Swift.Lead"));
    if (!Lead)
    {
        AddError(TEXT("Lead is missing from the fallback registry"));
        return false;
    }
    TestEqual(TEXT("Lead costs 40 Momentum"), Lead->GetResourceCost(), 40.0f);
    TestEqual(TEXT("Lead has a 10s cooldown"), Lead->GetCooldownSeconds(), 10.0f);
    TestTrue(TEXT("Lead is implemented"), Lead->IsImplemented());
    TestEqual(TEXT("Lead's mark lasts 6s"), Lead->WindowDuration, 6.0f);

    const UBreakerAbilityDefinition* Overdrive = UBreakerAbilityDefinition::FindFallback(TEXT("Swift.Overdrive"));
    if (!Overdrive)
    {
        AddError(TEXT("Overdrive is missing from the fallback registry"));
        return false;
    }
    TestEqual(TEXT("Overdrive costs a full bar"), Overdrive->GetResourceCost(), 100.0f);
    TestFalse(TEXT("Overdrive is cost-gated, not cooldown-gated"), Overdrive->HasCooldown());
    TestTrue(TEXT("Overdrive is the ultimate"), Overdrive->IsUltimate());
    TestTrue(TEXT("Overdrive is implemented"), Overdrive->IsImplemented());
    TestEqual(TEXT("Overdrive's base window is 8s"), Overdrive->WindowDuration, 8.0f);

    // The whole Swift kit is now activatable: E, T and G all resolve to an
    // ability class rather than a designed-but-unbuilt slot.
    for (const EBreakerAbilitySlot Slot : { EBreakerAbilitySlot::ClassAbilityOne, EBreakerAbilitySlot::ClassAbilityTwo, EBreakerAbilitySlot::Ultimate })
    {
        const UBreakerAbilityDefinition* Definition = UBreakerAbilityComponent::ResolveDefinition(EBreakerClassId::Swift, Slot, NAME_None);
        TestNotNull(TEXT("Every Swift slot resolves"), Definition);
        if (Definition)
        {
            TestTrue(TEXT("Every Swift slot is implemented"), Definition->IsImplemented());
            TestTrue(TEXT("Every Swift ability derives from the Breaker base"), Definition->AbilityClass->IsChildOf(UBreakerGameplayAbility::StaticClass()));
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAbilityKeystoneVariantTest,
    "RiorsEdge.Abilities.KeystoneVariants",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAbilityKeystoneVariantTest::RunTest(const FString& Parameters)
{
    const UBreakerAbilityDefinition* Overdrive = UBreakerAbilityDefinition::FindFallback(TEXT("Swift.Overdrive"));
    if (!Overdrive)
    {
        AddError(TEXT("Overdrive is missing from the fallback registry"));
        return false;
    }
    TestEqual(TEXT("Overdrive authors a base row and three keystone rows"), Overdrive->Variants.Num(), 4);

    // No keystone: the base row.
    const FGameplayTagContainer NoTags;
    const FBreakerAbilityVariant Base = Overdrive->ResolveVariant(NoTags);
    TestFalse(TEXT("The base row carries no keystone tag"), Base.KeystoneTag.IsValid());
    TestEqual(TEXT("The base row is the 8s window"), Base.WindowDuration, 8.0f);

    // One keystone at a time: Class-Kits §0.2 caps a character at one, so
    // resolution is a lookup rather than a merge.
    FGameplayTagContainer Bloodrhythm;
    Bloodrhythm.AddTag(BreakerAbilityTags::Keystone_Swift_Bloodrhythm);
    const FBreakerAbilityVariant BloodrhythmRow = Overdrive->ResolveVariant(Bloodrhythm);
    TestEqual(TEXT("Bloodrhythm resolves to its own row"), BloodrhythmRow.KeystoneTag, BreakerAbilityTags::Keystone_Swift_Bloodrhythm.GetTag());
    TestEqual(TEXT("Bloodrhythm carries the 1.5s no-hit exit"), BloodrhythmRow.HitTimeoutSeconds, 1.5f);

    FGameplayTagContainer TerminalVelocity;
    TerminalVelocity.AddTag(BreakerAbilityTags::Keystone_Swift_TerminalVelocity);
    const FBreakerAbilityVariant TerminalRow = Overdrive->ResolveVariant(TerminalVelocity);
    TestEqual(TEXT("Terminal Velocity resolves to its own row"), TerminalRow.KeystoneTag, BreakerAbilityTags::Keystone_Swift_TerminalVelocity.GetTag());
    // Master 5.4: Terminal Velocity is an availability rewrite, never a speed one.
    TestEqual(TEXT("Terminal Velocity grants no speed"), TerminalRow.SpeedMultiplier, 1.0f);

    FGameplayTagContainer StandingWave;
    StandingWave.AddTag(BreakerAbilityTags::Keystone_Swift_StandingWave);
    TestEqual(TEXT("Standing Wave resolves to its own row"),
        Overdrive->ResolveVariant(StandingWave).KeystoneTag, BreakerAbilityTags::Keystone_Swift_StandingWave.GetTag());

    // An unrelated tag must not select a keystone row.
    FGameplayTagContainer Unrelated;
    Unrelated.AddTag(BreakerAbilityTags::State_Ability_Skim);
    TestFalse(TEXT("An unrelated tag falls back to the base row"), Overdrive->ResolveVariant(Unrelated).KeystoneTag.IsValid());

    // A definition with no rows authored still answers with the definition's
    // own window, so an unauthored ability cannot resolve to a zero duration.
    UBreakerAbilityDefinition* Bare = NewObject<UBreakerAbilityDefinition>();
    Bare->WindowDuration = 4.0f;
    TestEqual(TEXT("An unauthored definition inherits its own window"), Bare->ResolveVariant(NoTags).WindowDuration, 4.0f);

    TestTrue(TEXT("A full bar meets the ultimate threshold"), UBreakerAbility_Overdrive::MeetsUltimateThreshold(100.0f, 100.0f));
    TestFalse(TEXT("A near-full bar does not"), UBreakerAbility_Overdrive::MeetsUltimateThreshold(99.9f, 100.0f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAbilityLeadRangeGateTest,
    "RiorsEdge.Abilities.LeadRangeGate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAbilityLeadRangeGateTest::RunTest(const FString& Parameters)
{
    // Class-Kits §1.2 S6: the 25 m gate is what stops Lead being a free crit
    // engine at close quarters.
    const float Gate = 2500.0f;
    TestTrue(TEXT("A marked target beyond 25 m is a weak point"), UBreakerAbility_Lead::ShouldTreatAsWeakPoint(true, 2500.1f, Gate));
    TestFalse(TEXT("Exactly at the gate does not qualify"), UBreakerAbility_Lead::ShouldTreatAsWeakPoint(true, Gate, Gate));
    TestFalse(TEXT("Point-blank on a mark does not qualify"), UBreakerAbility_Lead::ShouldTreatAsWeakPoint(true, 500.0f, Gate));
    TestFalse(TEXT("An unmarked target never qualifies"), UBreakerAbility_Lead::ShouldTreatAsWeakPoint(false, 9000.0f, Gate));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAbilityStateWindowTest,
    "RiorsEdge.Abilities.StateWindows",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAbilityStateWindowTest::RunTest(const FString& Parameters)
{
    UBreakerAbilityStateComponent* State = NewObject<UBreakerAbilityStateComponent>();
    const FName Key = TEXT("Window.Test");

    TestFalse(TEXT("Nothing is open on a fresh component"), State->IsWindowActive(Key));
    TestEqual(TEXT("An unopened window has no remaining time"), State->GetWindowRemaining(Key), 0.0f);
    State->StartWindow(Key, 0.0f);
    TestFalse(TEXT("A zero-length window never opens"), State->IsWindowActive(Key));

    State->StartWindow(Key, 3.0f);
    TestTrue(TEXT("The window opens"), State->IsWindowActive(Key));
    TestEqual(TEXT("Remaining time starts at the full duration"), State->GetWindowRemaining(Key), 3.0f);

    State->AdvanceTime(1.0f);
    TestEqual(TEXT("Remaining time counts down"), State->GetWindowRemaining(Key), 2.0f);

    // Re-starting refreshes rather than stacking.
    State->StartWindow(Key, 3.0f);
    TestEqual(TEXT("A re-cast refreshes the window"), State->GetWindowRemaining(Key), 3.0f);
    State->ExtendWindow(Key, 1.0f);
    TestEqual(TEXT("Extending adds to the remainder"), State->GetWindowRemaining(Key), 4.0f);

    // OnWindowEnded is a dynamic delegate and an automation test cannot declare
    // a UFUNCTION to bind to it, so expiry is asserted through the component's
    // own queries.
    State->AdvanceTime(4.0f);
    TestFalse(TEXT("The window closes when its time elapses"), State->IsWindowActive(Key));
    TestEqual(TEXT("A closed window reports no remaining time"), State->GetWindowRemaining(Key), 0.0f);
    TestEqual(TEXT("No windows remain active"), State->GetActiveWindowCount(), 0);

    // Explicit close is the same teardown path.
    State->StartWindow(Key, 5.0f);
    State->CloseWindow(Key);
    TestFalse(TEXT("Closing ends the window immediately"), State->IsWindowActive(Key));

    // Windows are independent.
    State->StartWindow(TEXT("Window.A"), 1.0f);
    State->StartWindow(TEXT("Window.B"), 5.0f);
    State->AdvanceTime(2.0f);
    TestFalse(TEXT("The short window expired"), State->IsWindowActive(TEXT("Window.A")));
    TestTrue(TEXT("The long window survived"), State->IsWindowActive(TEXT("Window.B")));
    TestEqual(TEXT("Exactly one window is active"), State->GetActiveWindowCount(), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAbilityStateStreakTest,
    "RiorsEdge.Abilities.StateStreaks",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAbilityStateStreakTest::RunTest(const FString& Parameters)
{
    // The rule first, world-free.
    TestTrue(TEXT("A same-target hit inside the gap continues the streak"), UBreakerAbilityStateComponent::ShouldContinueStreak(true, 1.0f, 3.0f));
    TestTrue(TEXT("Exactly at the gap still continues"), UBreakerAbilityStateComponent::ShouldContinueStreak(true, 3.0f, 3.0f));
    TestFalse(TEXT("Past the gap resets"), UBreakerAbilityStateComponent::ShouldContinueStreak(true, 3.01f, 3.0f));
    TestFalse(TEXT("A target switch resets regardless of timing"), UBreakerAbilityStateComponent::ShouldContinueStreak(false, 0.1f, 3.0f));

    UBreakerAbilityStateComponent* State = NewObject<UBreakerAbilityStateComponent>();
    AActor* TargetA = NewObject<AActor>();
    AActor* TargetB = NewObject<AActor>();

    TestEqual(TEXT("An untouched target has no streak"), State->GetStreak(TargetA), 0);
    TestEqual(TEXT("A null target records nothing"), State->RecordHit(nullptr), 0);

    TestEqual(TEXT("The first hit starts the streak at one"), State->RecordHit(TargetA), 1);
    TestEqual(TEXT("The second consecutive hit stacks"), State->RecordHit(TargetA), 2);
    TestEqual(TEXT("The streak reads back"), State->GetStreak(TargetA), 2);

    // Target switch resets.
    TestEqual(TEXT("Switching targets restarts the streak"), State->RecordHit(TargetB), 1);
    TestEqual(TEXT("The abandoned target has no streak"), State->GetStreak(TargetA), 0);

    // A gap resets even on the same target.
    State->AdvanceTime(3.5f);
    TestEqual(TEXT("A stale streak reads as zero without a tick to clear it"), State->GetStreak(TargetB), 0);
    TestEqual(TEXT("A hit after the gap restarts the streak"), State->RecordHit(TargetB), 1);

    State->ResetStreak();
    TestEqual(TEXT("An explicit reset clears the streak"), State->GetStreak(TargetB), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAbilitySlotAffinityTest,
    "RiorsEdge.Abilities.SlotAffinity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAbilitySlotAffinityTest::RunTest(const FString& Parameters)
{
    UBreakerAbilityDefinition* ClassAbility = NewObject<UBreakerAbilityDefinition>();
    ClassAbility->SlotAffinity = EBreakerAbilitySlot::ClassAbilityOne;
    TestTrue(TEXT("A class ability fits slot one"), ClassAbility->CanOccupySlot(EBreakerAbilitySlot::ClassAbilityOne));
    TestTrue(TEXT("A class ability fits slot two"), ClassAbility->CanOccupySlot(EBreakerAbilitySlot::ClassAbilityTwo));
    TestFalse(TEXT("A class ability may not sit in the ultimate slot"), ClassAbility->CanOccupySlot(EBreakerAbilitySlot::Ultimate));

    UBreakerAbilityDefinition* Ultimate = NewObject<UBreakerAbilityDefinition>();
    Ultimate->SlotAffinity = EBreakerAbilitySlot::Ultimate;
    TestTrue(TEXT("An ultimate fits the ultimate slot"), Ultimate->CanOccupySlot(EBreakerAbilitySlot::Ultimate));
    TestFalse(TEXT("An ultimate may not sit in a class slot"), Ultimate->CanOccupySlot(EBreakerAbilitySlot::ClassAbilityOne));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAbilityResolutionTest,
    "RiorsEdge.Abilities.SlotResolution",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAbilityResolutionTest::RunTest(const FString& Parameters)
{
    // An empty loadout falls back to the class default so the slice is
    // playable before the loadout UI writes anything.
    const UBreakerAbilityDefinition* SlotOne = UBreakerAbilityComponent::ResolveDefinition(EBreakerClassId::Swift, EBreakerAbilitySlot::ClassAbilityOne, NAME_None);
    TestNotNull(TEXT("An empty Swift slot one defaults to Skim"), SlotOne);
    if (SlotOne)
    {
        TestEqual(TEXT("The default is Skim"), SlotOne->AbilityId, FName(TEXT("Swift.Skim")));
    }

    const UBreakerAbilityDefinition* Ultimate = UBreakerAbilityComponent::ResolveDefinition(EBreakerClassId::Swift, EBreakerAbilitySlot::Ultimate, NAME_None);
    TestNotNull(TEXT("An empty Swift ultimate defaults to Overdrive"), Ultimate);

    // A class with no kit authored yet grants nothing rather than borrowing Swift's.
    TestNull(TEXT("Caster has no fallback kit yet"), UBreakerAbilityComponent::ResolveDefinition(EBreakerClassId::Caster, EBreakerAbilitySlot::ClassAbilityOne, NAME_None));
    // An explicitly equipped ability that does not fit the slot is refused.
    TestNull(TEXT("Overdrive cannot be equipped into a class slot"), UBreakerAbilityComponent::ResolveDefinition(EBreakerClassId::Swift, EBreakerAbilitySlot::ClassAbilityOne, TEXT("Swift.Overdrive")));
    TestNull(TEXT("An unknown equipped id resolves to nothing"), UBreakerAbilityComponent::ResolveDefinition(EBreakerClassId::Swift, EBreakerAbilitySlot::ClassAbilityOne, TEXT("Swift.Nonsense")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAbilityCostRuleTest,
    "RiorsEdge.Abilities.CostRule",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAbilityCostRuleTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("Exactly enough resource affords the ability"), UBreakerGameplayAbility::IsAffordable(15.0f, 15.0f));
    TestFalse(TEXT("One short does not afford it"), UBreakerGameplayAbility::IsAffordable(14.99f, 15.0f));
    TestTrue(TEXT("A free ability is always affordable"), UBreakerGameplayAbility::IsAffordable(0.0f, 0.0f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAbilitySkimDirectionTest,
    "RiorsEdge.Abilities.SkimDirection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAbilitySkimDirectionTest::RunTest(const FString& Parameters)
{
    // Skim is a horizontal verb: looking straight down must not aim it downward.
    const FVector LookingDown = UBreakerAbility_Skim::HorizontalDirectionForView(FRotator(-89.0, 0.0, 0.0));
    TestTrue(TEXT("The impulse is strictly horizontal"), FMath::IsNearlyZero(LookingDown.Z));
    TestTrue(TEXT("The impulse is normalized"), FMath::IsNearlyEqual(LookingDown.Size(), 1.0f, KINDA_SMALL_NUMBER));
    TestTrue(TEXT("Pitch does not change the heading"), LookingDown.Equals(FVector::ForwardVector, KINDA_SMALL_NUMBER));

    const FVector Yawed = UBreakerAbility_Skim::HorizontalDirectionForView(FRotator(0.0, 90.0, 0.0));
    TestTrue(TEXT("Yaw drives the heading"), Yawed.Equals(FVector::RightVector, KINDA_SMALL_NUMBER));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAbilityComponentBookkeepingTest,
    "RiorsEdge.Abilities.ComponentBookkeeping",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAbilityComponentBookkeepingTest::RunTest(const FString& Parameters)
{
    // No owner, no ability system: every query must answer safely rather than
    // crash, because the HUD reads these before BeginPlay on a fresh pawn.
    UBreakerAbilityComponent* Component = NewObject<UBreakerAbilityComponent>();
    Component->RefreshGrants();
    TestEqual(TEXT("Nothing is granted without an owner"), Component->GetGrantedCount(), 0);
    TestEqual(TEXT("No active cooldowns without an owner"), Component->GetActiveCooldownCount(), 0);
    TestEqual(TEXT("An ungranted slot has no id"), Component->GetAbilityIdForSlot(EBreakerAbilitySlot::ClassAbilityOne), FName(NAME_None));
    TestNull(TEXT("An ungranted slot has no definition"), Component->GetDefinitionForSlot(EBreakerAbilitySlot::ClassAbilityOne));
    TestFalse(TEXT("An ungranted slot is not implemented"), Component->IsSlotImplemented(EBreakerAbilitySlot::ClassAbilityOne));
    TestFalse(TEXT("An ungranted slot is not granted"), Component->IsSlotGranted(EBreakerAbilitySlot::ClassAbilityOne));
    TestEqual(TEXT("An ungranted slot costs nothing"), Component->GetCost(EBreakerAbilitySlot::Ultimate), 0.0f);
    TestEqual(TEXT("An ungranted slot has no cooldown remaining"), Component->GetCooldownRemaining(EBreakerAbilitySlot::Ultimate), 0.0f);
    TestEqual(TEXT("An ungranted slot has no cooldown duration"), Component->GetCooldownDuration(EBreakerAbilitySlot::Ultimate), 0.0f);
    TestFalse(TEXT("An ungranted slot reports no cooldown at all"), Component->SlotHasCooldown(EBreakerAbilitySlot::Ultimate));
    TestFalse(TEXT("Activating an ungranted slot fails cleanly"), Component->TryActivateSlot(EBreakerAbilitySlot::ClassAbilityOne));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAbilityImpactRulesTest,
    "RiorsEdge.Abilities.ImpactRules",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAbilityImpactRulesTest::RunTest(const FString& Parameters)
{
    // Lead's gate as the weapon reads it: one authored value, not a literal
    // copied into the consumer.
    TestEqual(TEXT("The weapon reads the ability's own 25 m gate"), UBreakerAbility_Lead::DefaultMinimumRangeCm(), 2500.0f);

    // Hard Stop (K7 Skim Discipline). Without the node the verb never fires,
    // whatever the player is looking at.
    TestFalse(TEXT("Without the node, looking down is still a redirect"),
        UBreakerAbility_Skim::ShouldHardStop(false, -80.0f, UBreakerAbility_Skim::HardStopPitchDegrees));
    TestFalse(TEXT("With the node, a level view is still a redirect"),
        UBreakerAbility_Skim::ShouldHardStop(true, 0.0f, UBreakerAbility_Skim::HardStopPitchDegrees));
    TestFalse(TEXT("With the node, looking up is still a redirect"),
        UBreakerAbility_Skim::ShouldHardStop(true, 60.0f, UBreakerAbility_Skim::HardStopPitchDegrees));
    TestTrue(TEXT("With the node, a steep down-aim stops dead"),
        UBreakerAbility_Skim::ShouldHardStop(true, -80.0f, UBreakerAbility_Skim::HardStopPitchDegrees));
    TestTrue(TEXT("The threshold itself qualifies"),
        UBreakerAbility_Skim::ShouldHardStop(true, UBreakerAbility_Skim::HardStopPitchDegrees, UBreakerAbility_Skim::HardStopPitchDegrees));
    // Control rotations arrive unwound (0-360), so the rule must normalize.
    TestTrue(TEXT("An unwound pitch normalizes before comparison"),
        UBreakerAbility_Skim::ShouldHardStop(true, 280.0f, UBreakerAbility_Skim::HardStopPitchDegrees));

    // Skim's burst must remain a burst, not a state.
    TestTrue(TEXT("Skim's burst adds speed"), UBreakerAbility_Skim::BurstSpeedMultiplier > 1.0f);
    TestTrue(TEXT("Skim's burst is shorter than its cooldown"), UBreakerAbility_Skim::BurstSeconds < 3.0f);

    // Overdrive is a power state: doubled generation and a real More.
    TestEqual(TEXT("Overdrive doubles Momentum generation"), UBreakerAbility_Overdrive::LoopGenerationMultiplier, 2.0f);
    TestTrue(TEXT("Overdrive raises outgoing damage"), UBreakerAbility_Overdrive::OutgoingMoreMultiplier > 1.0f);
    // Damage-Pipeline §4 caps any single More at 1.30x.
    TestTrue(TEXT("Overdrive's More respects the per-modifier ceiling"), UBreakerAbility_Overdrive::OutgoingMoreMultiplier <= 1.30f);
    TestFalse(TEXT("Overdrive's modifier key is real"), UBreakerAbility_Overdrive::OutgoingModifierKey().IsNone());
    TestNotEqual(TEXT("The modifier key is distinct from the window key"), UBreakerAbility_Overdrive::OutgoingModifierKey(), UBreakerAbility_Overdrive::WindowKey());
    return true;
}

#endif
