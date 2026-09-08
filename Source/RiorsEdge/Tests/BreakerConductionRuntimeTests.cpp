#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbilityComponent.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Abilities/BreakerAbility_HardStop.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerMomentumComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionTree.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerProgressionNode.h"
#include "TimerManager.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerConductionRuntimeTest,
    "RiorsEdge.Abilities.ConductionRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerConductionRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    World->InitializeActorsForPlay(FURL());
    auto* Player = World->SpawnActor<ABreakerCharacter>();
    if (!Player) return false;
    Player->SetActorTickEnabled(false);
    auto* Movement = Player->GetBreakerMovement();
    Movement->SetComponentTickEnabled(false);
    auto* Attributes = Player->GetAttributes();
    auto* ASC = Player->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Player, Player);
    ASC->AddAttributeSetSubobject(Attributes);
    Player->GetCombat()->BindAttributes(Attributes);
    auto* Progression = Player->GetProgression();
    Progression->BindAttributes(Attributes);
    if (!TestTrue(TEXT("Actual Swift class"), Progression->ChoosePermanentClassById(EBreakerClassId::Swift))) return false;
    // The first real token is earned at level five, below the authored paid-respec threshold.
    // This fixture needs one token and one Core point; no campaign/Doctrine entitlement.
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(
        UBreakerProgressionLibrary::FirstAbilityTokenLevel, Progression->ExperienceCurve));
    FText Reason;
    const int32 Tokens = Progression->GetUnspentAbilityTokens();
    if (!TestTrue(TEXT("Earned token unlocks actual Hard Stop"), Progression->SpendAbilityToken(TEXT("Swift.HardStop"), Reason))) return false;
    TestEqual(TEXT("Unlock spends a token"), Progression->GetUnspentAbilityTokens(), Tokens - 1);
    const auto Slot = EBreakerAbilitySlot::ClassAbilityOne;
    if (!TestTrue(TEXT("Actual loadout equips Hard Stop"), Progression->EquipAbility(Slot, TEXT("Swift.HardStop"), Reason))) return false;
    auto* Abilities = Player->GetAbilities();
    Abilities->RefreshGrants();
    if (!TestTrue(TEXT("Native ability is granted"), Abilities->IsSlotGranted(Slot))) return false;
    TestEqual(TEXT("Base cost before first activation"), Abilities->GetCost(Slot), 30.0f, .0001f);
    // Separate rule schema, purchased with earned Core points; no shipped roster activation.
    auto* Definition = DuplicateObject<UBreakerClassDefinition>(Progression->ClassDefinition, Player);
    auto* RuleTree = NewObject<UBreakerProgressionTree>(Definition);
    RuleTree->TreeId = TEXT("Test.Core.Conduction"); RuleTree->Currency = EBreakerPointCurrency::CorePoints;
    auto* Rule = NewObject<UBreakerProgressionNode>(RuleTree); Rule->NodeId = TEXT("Test.Conduction.Rule");
    Rule->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Conduction")));
    RuleTree->Nodes.Add(Rule); Definition->BranchTrees.Add(RuleTree); Progression->ClassDefinition = Definition;
    TestTrue(TEXT("Unowned Hard Stop has authored cooldown"), Abilities->SlotHasCooldown(Slot));
    if (!TestTrue(TEXT("Actual earned rule purchase"), Progression->PurchaseNode(RuleTree, Rule->NodeId, Reason))) return false;
    TestFalse(TEXT("Owned rule removes cooldown presentation"), Abilities->SlotHasCooldown(Slot));
    auto* Momentum = Player->GetMomentum(); Momentum->BindAttributes(Attributes); Momentum->SetComponentTickEnabled(false);
    Movement->SetMovementMode(MOVE_Walking);
    for (int32 Step = 0; Step < 80 && Momentum->GetMomentum() < 100; ++Step)
    {
        Movement->Velocity = FVector(Movement->WalkSpeed, 0, 0);
        Player->SetActorLocation(Player->GetActorLocation() + Movement->Velocity);
        Momentum->AdvanceLoop(1);
    }
    Movement->StopMovementImmediately();
    if (!TestTrue(TEXT("Normal movement funds two casts"), Momentum->GetMomentum() >= 100)) return false;
    bool bReentrantAccepted = false; int32 CallbackCount = 0;
    const FDelegateHandle Callback = ASC->GetGameplayAttributeValueChangeDelegate(UBreakerAttributeSet::GetClassResourceAttribute()).AddLambda(
        [&](const FOnAttributeChangeData& Change)
        {
            if (Change.NewValue >= Change.OldValue) return;
            ++CallbackCount;
            TestEqual(TEXT("Reentrant quote retains committed price"), Abilities->GetCost(Slot), Change.OldValue - Change.NewValue, .001f);
            bReentrantAccepted |= Abilities->TryActivateSlot(Slot);
        });
    const float Before = Momentum->GetMomentum();
    if (!TestTrue(TEXT("First actual Conduction cast"), Abilities->TryActivateSlot(Slot))) return false;
    TestEqual(TEXT("First cast pays original price"), Before - Momentum->GetMomentum(), 30.0f, .001f);
    TestEqual(TEXT("Next quote adds forty percent"), Abilities->GetCost(Slot), 42.0f, .001f);
    const float BeforeSecond = Momentum->GetMomentum();
    if (!TestTrue(TEXT("Actual immediate second cast bypasses cooldown"), Abilities->TryActivateSlot(Slot))) return false;
    TestEqual(TEXT("Second cast pays quoted surcharge once"), BeforeSecond - Momentum->GetMomentum(), 42.0f, .001f);
    ASC->GetGameplayAttributeValueChangeDelegate(UBreakerAttributeSet::GetClassResourceAttribute()).Remove(Callback);
    TestTrue(TEXT("Real payment callbacks attempted reentry"), CallbackCount >= 2);
    TestFalse(TEXT("Payment callback cannot recursively cast"), bReentrantAccepted);
    const auto* PaidInstance = Cast<UBreakerGameplayAbility>(ASC->FindAbilitySpecFromClass(UBreakerAbility_HardStop::StaticClass())->GetPrimaryInstance());
    TestEqual(TEXT("Refund basis remains actual second payment, not future quote"), PaidInstance->GetLastPaidResourceCost(), 42.0f, .001f);
    TestEqual(TEXT("Independent additive contributions quote eighty percent"), Abilities->GetCost(Slot), 54.0f, .001f);
    TestEqual(TEXT("No real cooldown effect remains"), Abilities->GetCooldownRemaining(Slot), 0.0f);
    const float BeforeFailed = Momentum->GetMomentum();
    TestFalse(TEXT("Unaffordable third cast refused"), Abilities->TryActivateSlot(Slot));
    TestEqual(TEXT("Failure spends nothing"), Momentum->GetMomentum(), BeforeFailed);
    TestEqual(TEXT("Failure creates no contribution"), Abilities->GetCost(Slot), 54.0f, .001f);
    const uint64 InitialFrame = GFrameCounter;
    for (int32 Step = 0; Step < 50; ++Step) { ++GFrameCounter; World->Tick(LEVELTICK_All, .05f); }
    TestEqual(TEXT("Both contributions fade linearly at half duration"), Abilities->GetCost(Slot), 42.0f, .02f);
    for (int32 Step = 0; Step < 52; ++Step) { ++GFrameCounter; World->Tick(LEVELTICK_All, .05f); }
    GFrameCounter = InitialFrame;
    TestEqual(TEXT("Five seconds restores original cost without combat reset"), Abilities->GetCost(Slot), 30.0f, .001f);
    for (int32 Step = 0; Step < 80 && Momentum->GetMomentum() < 100; ++Step)
    {
        Movement->Velocity = FVector(Movement->WalkSpeed, 0, 0);
        Player->SetActorLocation(Player->GetActorLocation() + Movement->Velocity); Momentum->AdvanceLoop(1);
    }
    Movement->StopMovementImmediately();
    if (!TestTrue(TEXT("New real cast creates fresh contribution before respec"), Abilities->TryActivateSlot(Slot))) return false;
    TestEqual(TEXT("Fresh contribution exists"), Abilities->GetCost(Slot), 42.0f, .001f);
    if (!TestTrue(TEXT("Real respec removes Conduction"), Progression->RespecCore(Reason))) return false;
    TestFalse(TEXT("Respec clears rule"), Abilities->IsConductionActive());
    TestTrue(TEXT("Respec restores authored cooldown presentation"), Abilities->SlotHasCooldown(Slot));
    if (!Progression->PurchaseNode(RuleTree, Rule->NodeId, Reason)) return false;
    TestEqual(TEXT("Rebuy never resurrects cleared contributions"), Abilities->GetCost(Slot), 30.0f, .001f);
    if (!TestTrue(TEXT("Paid cast creates contribution before real death"), Abilities->TryActivateSlot(Slot))) return false;
    FBreakerDamageRequest Lethal; Lethal.BaseDamage = 100000; Lethal.DamageFamily = EBreakerDamageFamily::TrueDamage; Lethal.bCanCritical = false; Lethal.bCanBeAvoided = false;
    Player->GetCombat()->ReceiveDamage(Lethal);
    if (!TestTrue(TEXT("Actual source death"), Player->GetCombat()->IsDead())) return false;
    Player->GetCombat()->RestoreVitals();
    TestEqual(TEXT("Revive cannot restore pre-death surcharge"), Abilities->GetCost(Slot), 30.0f, .001f);
    return true;
}
#endif
