#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Abilities/BreakerAbilityComponent.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Abilities/BreakerAbilityTags.h"
#include "Abilities/BreakerAbility_Cleave.h"
#include "Abilities/BreakerAbility_Closequarter.h"
#include "Abilities/BreakerAbility_Lead.h"
#include "Abilities/BreakerAbility_Overdrive.h"
#include "Abilities/BreakerAbility_Skim.h"
#include "Abilities/BreakerAbility_Unmake.h"
#include "Abilities/BreakerCasterAbility.h"
#include "Abilities/BreakerGameplayAbility.h"
#include "Abilities/BreakerMeleeSweep.h"
#include "Classes/BreakerManaComponent.h"
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

    // Caster is now reachable through the same path: an empty loadout (which is
    // exactly what ChoosePermanentClassById leaves for a class with no authored
    // class definition) resolves to the Caster kit, not to nothing and not to
    // Swift's.
    const UBreakerAbilityDefinition* CasterOne = UBreakerAbilityComponent::ResolveDefinition(EBreakerClassId::Caster, EBreakerAbilitySlot::ClassAbilityOne, NAME_None);
    TestNotNull(TEXT("An empty Caster slot one defaults to Cleave"), CasterOne);
    if (CasterOne)
    {
        TestEqual(TEXT("The Caster default is Cleave"), CasterOne->AbilityId, FName(TEXT("Caster.Cleave")));
        TestEqual(TEXT("Cleave is a Caster ability"), CasterOne->ClassId, EBreakerClassId::Caster);
    }
    // A class that genuinely has no kit still grants nothing rather than
    // borrowing another class's.
    TestNull(TEXT("Tank has no fallback kit yet"), UBreakerAbilityComponent::ResolveDefinition(EBreakerClassId::Tank, EBreakerAbilitySlot::ClassAbilityOne, NAME_None));
    // An explicitly equipped ability that does not fit the slot is refused.
    TestNull(TEXT("Overdrive cannot be equipped into a class slot"), UBreakerAbilityComponent::ResolveDefinition(EBreakerClassId::Swift, EBreakerAbilitySlot::ClassAbilityOne, TEXT("Swift.Overdrive")));
    // Rule changed after the owner hit dead abilities from a stale loadout:
    // an unknown equipped id falls back to the class default instead of
    // silently granting nothing.
    const UBreakerAbilityDefinition* UnknownFallback = UBreakerAbilityComponent::ResolveDefinition(EBreakerClassId::Swift, EBreakerAbilitySlot::ClassAbilityOne, TEXT("Swift.Nonsense"));
    TestNotNull(TEXT("An unknown equipped id falls back to the class default"), UnknownFallback);
    if (UnknownFallback) TestEqual(TEXT("Fallback is the slot default"), UnknownFallback->AbilityId, FName(TEXT("Swift.Skim")));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerCasterKitRegistryTest,
    "RiorsEdge.Abilities.CasterKit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerCasterKitRegistryTest::RunTest(const FString& Parameters)
{
    // Values quoted from Docs/Design/Class-Kits.md §2.2.
    const UBreakerAbilityDefinition* Cleave = UBreakerAbilityDefinition::FindFallback(TEXT("Caster.Cleave"));
    if (!Cleave)
    {
        AddError(TEXT("Cleave is missing from the fallback registry"));
        return false;
    }
    TestEqual(TEXT("Cleave costs 20 Mana"), Cleave->GetResourceCost(), 20.0f);
    TestTrue(TEXT("Cleave is implemented"), Cleave->IsImplemented());
    TestTrue(TEXT("Cleave derives from the Caster base"), Cleave->AbilityClass->IsChildOf(UBreakerCasterAbility::StaticClass()));

    const UBreakerAbilityDefinition* Closequarter = UBreakerAbilityDefinition::FindFallback(TEXT("Caster.Closequarter"));
    if (!Closequarter)
    {
        AddError(TEXT("Closequarter is missing from the fallback registry"));
        return false;
    }
    TestEqual(TEXT("Closequarter costs 35 Mana"), Closequarter->GetResourceCost(), 35.0f);
    TestTrue(TEXT("Closequarter is implemented"), Closequarter->IsImplemented());

    const UBreakerAbilityDefinition* Unmake = UBreakerAbilityDefinition::FindFallback(TEXT("Caster.Unmake"));
    if (!Unmake)
    {
        AddError(TEXT("Unmake is missing from the fallback registry"));
        return false;
    }
    TestEqual(TEXT("Unmake costs 80 Mana"), Unmake->GetResourceCost(), 80.0f);
    TestTrue(TEXT("Unmake is the ultimate"), Unmake->IsUltimate());
    TestEqual(TEXT("Unmake's base window is 6s"), Unmake->WindowDuration, 6.0f);

    // Class-Kits §2.1: Mana IS the cooldown. A Caster ability that grows a
    // cooldown has stopped being a Caster ability.
    for (const UBreakerAbilityDefinition* Definition : UBreakerAbilityDefinition::GetFallbackRegistry())
    {
        if (!Definition || Definition->ClassId != EBreakerClassId::Caster) continue;
        TestFalse(TEXT("No Caster ability has a cooldown"), Definition->HasCooldown());
        TestFalse(TEXT("No Caster ability authors a cooldown tag"), Definition->CooldownTag.IsValid());
        TestTrue(TEXT("Every Caster ability is implemented"), Definition->IsImplemented());
    }

    // Every Caster slot resolves and is activatable: E, T and G all reach an
    // ability class rather than a designed-but-unbuilt slot.
    for (const EBreakerAbilitySlot Slot : { EBreakerAbilitySlot::ClassAbilityOne, EBreakerAbilitySlot::ClassAbilityTwo, EBreakerAbilitySlot::Ultimate })
    {
        const UBreakerAbilityDefinition* Definition = UBreakerAbilityComponent::ResolveDefinition(EBreakerClassId::Caster, Slot, NAME_None);
        TestNotNull(TEXT("Every Caster slot resolves"), Definition);
        if (Definition)
        {
            TestTrue(TEXT("Every Caster slot is implemented"), Definition->IsImplemented());
            TestTrue(TEXT("Every Caster ability derives from the Caster base"), Definition->AbilityClass->IsChildOf(UBreakerCasterAbility::StaticClass()));
        }
    }

    // A stale save carrying an unknown Caster id still lands on a live ability
    // rather than a dead key (the addf85 rule, now applied to Caster).
    const UBreakerAbilityDefinition* Unknown = UBreakerAbilityComponent::ResolveDefinition(EBreakerClassId::Caster, EBreakerAbilitySlot::ClassAbilityOne, TEXT("Caster.Nonsense"));
    TestNotNull(TEXT("An unknown Caster id falls back to the class default"), Unknown);
    if (Unknown) TestEqual(TEXT("The Caster fallback is Cleave"), Unknown->AbilityId, FName(TEXT("Caster.Cleave")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerCasterUnmakeVariantTest,
    "RiorsEdge.Abilities.UnmakeVariants",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerCasterUnmakeVariantTest::RunTest(const FString& Parameters)
{
    const UBreakerAbilityDefinition* Unmake = UBreakerAbilityDefinition::FindFallback(TEXT("Caster.Unmake"));
    if (!Unmake)
    {
        AddError(TEXT("Unmake is missing from the fallback registry"));
        return false;
    }
    TestEqual(TEXT("Unmake authors a base row and three keystone rows"), Unmake->Variants.Num(), 4);

    const FGameplayTagContainer NoTags;
    const FBreakerAbilityVariant Base = Unmake->ResolveVariant(NoTags);
    TestFalse(TEXT("The base row carries no keystone tag"), Base.KeystoneTag.IsValid());
    TestEqual(TEXT("The base window is 6s"), Base.WindowDuration, 6.0f);
    TestEqual(TEXT("Base Unmake makes Caster abilities free"), Base.AbilityCostMultiplier, 0.0f);

    // Long Dark is the only fully parametric rewrite: 12s at 50% cost.
    FGameplayTagContainer LongDark;
    LongDark.AddTag(BreakerAbilityTags::Keystone_Caster_LongDark);
    const FBreakerAbilityVariant LongDarkRow = Unmake->ResolveVariant(LongDark);
    TestEqual(TEXT("Long Dark resolves to its own row"), LongDarkRow.KeystoneTag, BreakerAbilityTags::Keystone_Caster_LongDark.GetTag());
    TestEqual(TEXT("Long Dark doubles the window"), LongDarkRow.WindowDuration, 12.0f);
    TestEqual(TEXT("Long Dark halves the cost instead of removing it"), LongDarkRow.AbilityCostMultiplier, 0.5f);

    FGameplayTagContainer Edgework;
    Edgework.AddTag(BreakerAbilityTags::Keystone_Caster_Edgework);
    TestEqual(TEXT("Edgework resolves to its own row"),
        Unmake->ResolveVariant(Edgework).KeystoneTag, BreakerAbilityTags::Keystone_Caster_Edgework.GetTag());

    FGameplayTagContainer Cascade;
    Cascade.AddTag(BreakerAbilityTags::Keystone_Caster_Cascade);
    TestEqual(TEXT("Cascade resolves to its own row"),
        Unmake->ResolveVariant(Cascade).KeystoneTag, BreakerAbilityTags::Keystone_Caster_Cascade.GetTag());

    // A Swift keystone must never select a Caster row.
    FGameplayTagContainer WrongClass;
    WrongClass.AddTag(BreakerAbilityTags::Keystone_Swift_Bloodrhythm);
    TestFalse(TEXT("A foreign keystone falls back to the base row"), Unmake->ResolveVariant(WrongClass).KeystoneTag.IsValid());

    // Duration and cost-scalar resolution rules.
    TestEqual(TEXT("An authored row duration wins"), UBreakerAbility_Unmake::ResolveDuration(12.0f, 6.0f), 12.0f);
    TestEqual(TEXT("An unauthored row inherits the definition window"), UBreakerAbility_Unmake::ResolveDuration(0.0f, 6.0f), 6.0f);
    TestEqual(TEXT("Free casts resolve to zero"), UBreakerAbility_Unmake::ResolveCostScalar(0.0f), 0.0f);
    TestEqual(TEXT("Half cost passes through"), UBreakerAbility_Unmake::ResolveCostScalar(0.5f), 0.5f);
    TestEqual(TEXT("A scalar above one is clamped: Unmake never raises costs"), UBreakerAbility_Unmake::ResolveCostScalar(3.0f), 1.0f);
    TestEqual(TEXT("A negative scalar is clamped to free, never inverted"), UBreakerAbility_Unmake::ResolveCostScalar(-1.0f), 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerCasterCostWindowTest,
    "RiorsEdge.Abilities.CasterCostWindow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerCasterCostWindowTest::RunTest(const FString& Parameters)
{
    // The price rule, world-free.
    TestEqual(TEXT("With no window a cast pays full price"), UBreakerCasterAbility::CostUnderWindow(20.0f, 1.0f), 20.0f);
    TestEqual(TEXT("Base Unmake makes a cast free"), UBreakerCasterAbility::CostUnderWindow(20.0f, 0.0f), 0.0f);
    TestEqual(TEXT("Long Dark halves a cast"), UBreakerCasterAbility::CostUnderWindow(35.0f, 0.5f), 17.5f);
    TestEqual(TEXT("A negative scalar never refunds"), UBreakerCasterAbility::CostUnderWindow(35.0f, -2.0f), 0.0f);

    // The affordability rule with a CLOSED floor: identical to the strict rule
    // it replaced, which is what guarantees no other class changed.
    TestTrue(TEXT("Exactly enough Mana affords the cast"), UBreakerCasterAbility::CanCastAt(20.0f, 20.0f));
    TestFalse(TEXT("One short is refused"), UBreakerCasterAbility::CanCastAt(19.99f, 20.0f));
    TestTrue(TEXT("A free cast is always allowed"), UBreakerCasterAbility::CanCastAt(0.0f, 0.0f));
    TestTrue(TEXT("An empty bank still casts a freed ability"), UBreakerCasterAbility::CanCastAt(0.0f, UBreakerCasterAbility::CostUnderWindow(80.0f, 0.0f)));

    // ...and with the floor OPEN (spec D8, Class-Kits §2.1). Overcast is a
    // debt, not a spiral, and a cast that would breach the floor is REFUSED
    // rather than truncated — a partial spend would be a silent discount.
    TestTrue(TEXT("A cast may drive the bank into Overcast"), UBreakerCasterAbility::CanCastAt(10.0f, 30.0f, -20.0f));
    TestTrue(TEXT("A cast may land exactly on the floor"), UBreakerCasterAbility::CanCastAt(10.0f, 30.0f, -20.0f));
    TestFalse(TEXT("A cast that would breach the floor is refused, not truncated"), UBreakerCasterAbility::CanCastAt(10.0f, 31.0f, -20.0f));
    TestFalse(TEXT("Nothing may be cast while already Overcast"), UBreakerCasterAbility::CanCastAt(-0.5f, 5.0f, -20.0f));
    TestTrue(TEXT("A freed cast still works in debt: Overcast into Unmake"), UBreakerCasterAbility::CanCastAt(-15.0f, 0.0f, -20.0f));
    TestTrue(TEXT("A deeper SB4 floor allows a deeper overdraft"), UBreakerCasterAbility::CanCastAt(10.0f, 44.0f, -35.0f));
    TestFalse(TEXT("A positive floor is treated as closed"), UBreakerCasterAbility::CanCastAt(10.0f, 30.0f, 10.0f));

    // The ability layer's rule and the Mana component's rule must not drift:
    // one refuses the activation, the other refuses the spend, and a
    // disagreement is either a free cast or a dead button.
    const float Banks[] = {-30.0f, -0.01f, 0.0f, 10.0f, 20.0f, 100.0f};
    const float Costs[] = {0.0f, 5.0f, 20.0f, 35.0f, 80.0f};
    const float Floors[] = {0.0f, -20.0f, -35.0f};
    for (const float Bank : Banks)
    {
        for (const float Cost : Costs)
        {
            for (const float Floor : Floors)
            {
                TestEqual(*FString::Printf(TEXT("Ability and Mana rules agree at bank %.2f cost %.2f floor %.2f"), Bank, Cost, Floor),
                    UBreakerCasterAbility::CanCastAt(Bank, Cost, Floor),
                    UBreakerManaComponent::CanSpendFrom(Bank, Cost, Floor));
            }
        }
    }

    // The generic rule underneath, which the HUD's CanAffordSlot also uses.
    TestTrue(TEXT("A closed floor leaves the base affordability rule untouched"), UBreakerGameplayAbility::IsAffordable(15.0f, 15.0f));
    TestEqual(TEXT("IsAffordable is IsAffordableWithFloor at zero"),
        UBreakerGameplayAbility::IsAffordable(14.99f, 15.0f), UBreakerGameplayAbility::IsAffordableWithFloor(14.99f, 15.0f, 0.0f));

    // The window carries the scalar, so the ultimate and the abilities it
    // discounts cannot drift apart.
    UBreakerAbilityStateComponent* State = NewObject<UBreakerAbilityStateComponent>();
    const FName Key = UBreakerCasterAbility::UnmakeWindowKey();
    TestEqual(TEXT("With no window open the payload default is returned"), State->GetWindowPayload(Key, 1.0f), 1.0f);

    State->StartWindowWithPayload(Key, 6.0f, 0.0f);
    TestTrue(TEXT("Unmake's window opens"), State->IsWindowActive(Key));
    TestEqual(TEXT("The window carries the free-cast scalar"), State->GetWindowPayload(Key, 1.0f), 0.0f);

    State->AdvanceTime(5.0f);
    TestTrue(TEXT("The window is still open before its time"), State->IsWindowActive(Key));
    State->AdvanceTime(1.5f);
    TestFalse(TEXT("The window closes on time"), State->IsWindowActive(Key));
    TestEqual(TEXT("A closed window stops discounting"), State->GetWindowPayload(Key, 1.0f), 1.0f);

    // Long Dark's longer window with its own scalar.
    State->StartWindowWithPayload(Key, 12.0f, 0.5f);
    TestEqual(TEXT("Long Dark's window carries the half-cost scalar"), State->GetWindowPayload(Key, 1.0f), 0.5f);
    State->CloseWindow(Key);
    TestEqual(TEXT("An explicitly closed window stops discounting"), State->GetWindowPayload(Key, 1.0f), 1.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerCleaveRulesTest,
    "RiorsEdge.Abilities.CleaveRules",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerCleaveRulesTest::RunTest(const FString& Parameters)
{
    // Damage scales off the weapon; an unarmed Caster still has a melee verb.
    TestEqual(TEXT("Cleave scales off weapon damage"), UBreakerAbility_Cleave::SwingDamage(30.0f, 1.5f, 20.0f), 45.0f);
    TestEqual(TEXT("An unarmed swing uses the fallback"), UBreakerAbility_Cleave::SwingDamage(0.0f, 1.5f, 20.0f), 30.0f);
    TestEqual(TEXT("A zero coefficient still resolves"), UBreakerAbility_Cleave::SwingDamage(30.0f, 0.0f, 20.0f), 0.0f);
    TestEqual(TEXT("Damage is never negative"), UBreakerAbility_Cleave::SwingDamage(30.0f, -2.0f, 20.0f), 0.0f);

    // Class-Kits §2.2: the Edgework keystone removes the animation lock.
    TestEqual(TEXT("Without Edgework the lock is the authored value"), UBreakerAbility_Cleave::AnimationLockFor(false, 0.45f), 0.45f);
    TestEqual(TEXT("Edgework removes the lock entirely"), UBreakerAbility_Cleave::AnimationLockFor(true, 0.45f), 0.0f);
    TestEqual(TEXT("A negative authored lock is treated as none"), UBreakerAbility_Cleave::AnimationLockFor(false, -1.0f), 0.0f);

    // The arc, world-free. 3 m forward, 120 degrees included.
    const FVector Origin = FVector::ZeroVector;
    const FVector Forward = FVector::ForwardVector;
    const float Range = 300.0f;
    const float Arc = 120.0f;
    TestTrue(TEXT("Straight ahead and in range is a hit"), UBreakerMeleeSweep::IsInsideArc(Origin, Forward, FVector(200.0, 0.0, 0.0), Range, Arc));
    TestFalse(TEXT("Straight ahead and out of range is a miss"), UBreakerMeleeSweep::IsInsideArc(Origin, Forward, FVector(400.0, 0.0, 0.0), Range, Arc));
    TestTrue(TEXT("Exactly at the range boundary connects"), UBreakerMeleeSweep::IsInsideArc(Origin, Forward, FVector(300.0, 0.0, 0.0), Range, Arc));
    TestFalse(TEXT("Directly behind is never a hit"), UBreakerMeleeSweep::IsInsideArc(Origin, Forward, FVector(-100.0, 0.0, 0.0), Range, Arc));
    // 45 degrees off centre is inside a 120-degree arc; 70 is not.
    TestTrue(TEXT("45 degrees off centre is inside a 120 degree arc"),
        UBreakerMeleeSweep::IsInsideArc(Origin, Forward, FVector(100.0, 100.0, 0.0), Range, Arc));
    TestFalse(TEXT("70 degrees off centre is outside a 120 degree arc"),
        UBreakerMeleeSweep::IsInsideArc(Origin, Forward, FVector(FMath::Cos(FMath::DegreesToRadians(70.0)) * 100.0, FMath::Sin(FMath::DegreesToRadians(70.0)) * 100.0, 0.0), Range, Arc));
    // SB8 Edge widens the arc to 180: the same 70-degree target now connects.
    TestTrue(TEXT("A 180 degree arc reaches 70 degrees off centre"),
        UBreakerMeleeSweep::IsInsideArc(Origin, Forward, FVector(FMath::Cos(FMath::DegreesToRadians(70.0)) * 100.0, FMath::Sin(FMath::DegreesToRadians(70.0)) * 100.0, 0.0), Range, 180.0f));
    // Height must not decide reach: an enemy on a crate is still in front of you.
    TestTrue(TEXT("Range is measured horizontally"), UBreakerMeleeSweep::IsInsideArc(Origin, Forward, FVector(200.0, 0.0, 250.0), Range, Arc));
    // A degenerate arc hits nothing rather than everything.
    TestFalse(TEXT("A zero arc hits nothing"), UBreakerMeleeSweep::IsInsideArc(Origin, Forward, FVector(100.0, 0.0, 0.0), Range, 0.0f));
    TestFalse(TEXT("A zero range hits nothing"), UBreakerMeleeSweep::IsInsideArc(Origin, Forward, FVector(1.0, 0.0, 0.0), 0.0f, Arc));

    // Deterministic ordering: centre-most first, nearest breaks the tie.
    const float Centre = UBreakerMeleeSweep::SortKey(Origin, Forward, FVector(250.0, 0.0, 0.0));
    const float OffAxis = UBreakerMeleeSweep::SortKey(Origin, Forward, FVector(100.0, 100.0, 0.0));
    TestTrue(TEXT("A centred far target sorts before an off-axis near one"), Centre < OffAxis);
    const float NearCentre = UBreakerMeleeSweep::SortKey(Origin, Forward, FVector(100.0, 0.0, 0.0));
    TestTrue(TEXT("Distance breaks ties on the centre line"), NearCentre < Centre);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerClosequarterRulesTest,
    "RiorsEdge.Abilities.ClosequarterRules",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerClosequarterRulesTest::RunTest(const FString& Parameters)
{
    // Class-Kits §2.2 C2: arrive 2 m short of the target.
    const FVector Caster = FVector::ZeroVector;
    const FVector Target(1000.0, 0.0, 0.0);
    const FVector Arrival = UBreakerAbility_Closequarter::ArrivalPoint(Caster, Target, 200.0f);
    TestTrue(TEXT("The arrival point is 2 m short of the target"), Arrival.Equals(FVector(800.0, 0.0, 0.0), 0.01));
    TestTrue(TEXT("The blink never overshoots the target"), FVector::Dist(Arrival, Target) >= 199.0);

    // Already inside the standoff: do not step backwards.
    const FVector Close(150.0, 0.0, 0.0);
    TestTrue(TEXT("A target inside the standoff produces no movement"),
        UBreakerAbility_Closequarter::ArrivalPoint(Caster, Close, 200.0f).Equals(Caster, 0.01));
    TestTrue(TEXT("A target exactly at the standoff produces no movement"),
        UBreakerAbility_Closequarter::ArrivalPoint(Caster, FVector(200.0, 0.0, 0.0), 200.0f).Equals(Caster, 0.01));

    // The refund gate: "at or below 40% health".
    TestTrue(TEXT("A target below the threshold refunds"), UBreakerAbility_Closequarter::ShouldRefund(0.2f, 0.4f));
    TestTrue(TEXT("A target exactly at the threshold refunds"), UBreakerAbility_Closequarter::ShouldRefund(0.4f, 0.4f));
    TestFalse(TEXT("A healthy target does not refund"), UBreakerAbility_Closequarter::ShouldRefund(0.41f, 0.4f));
    TestFalse(TEXT("A full-health target does not refund"), UBreakerAbility_Closequarter::ShouldRefund(1.0f, 0.4f));
    // SB10 No Distance moves the threshold to 100% — a data change, not a branch.
    TestTrue(TEXT("A 100% threshold refunds on anything"), UBreakerAbility_Closequarter::ShouldRefund(1.0f, 1.0f));
    return true;
}

#endif
