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
#include "Save/BreakerMissionContent.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerHardStopAffordabilityRuntimeTest,
    "RiorsEdge.Abilities.HardStopPaidAffordability",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerHardStopAffordabilityRuntimeTest::RunTest(const FString& Parameters)
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
    // Restored benchmark entitlement fixture, not a claim of a campaign run.
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(50, Progression->ExperienceCurve));
    FBreakerQuestFlagSet Flags;
    for (const auto& Mission : UBreakerMissionLibrary::GetMissions())
        for (const auto& Beat : Mission.Beats)
            for (FName Flag : UBreakerMissionLibrary::BeatCompletionFlags(Beat)) Flags.Add(Flag);
    Flags.Add(TEXT("Quest.Finale.Seal"));
    Progression->SettleDoctrineEntitlement(Flags);
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
    const auto* Tree = UBreakerProgressionLibrary::GetSwiftKineticTree();
    for (const TCHAR* Node : {TEXT("Swift.Kinetic.ReadTheRoom"), TEXT("Swift.Kinetic.ReadTheRoom"),
        TEXT("Swift.Kinetic.Redirect"), TEXT("Swift.Kinetic.Redirect"),
        TEXT("Swift.Kinetic.SkimDiscipline"), TEXT("Swift.Kinetic.SpendToLive")})
    {
        const bool Bought = Progression->PurchaseNode(Tree, Node, Reason);
        if (!TestTrue(FString::Printf(TEXT("Legal purchase %s: %s"), Node, *Reason.ToString()), Bought)) return false;
    }
    TestEqual(TEXT("Legal path spends exactly eight Doctrine"), Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), 0);
    TestEqual(TEXT("Live instance quotes doubled cost before first cast"), Abilities->GetCost(Slot), 60.0f, .0001f);
    auto* Momentum = Player->GetMomentum();
    Momentum->BindAttributes(Attributes);
    Momentum->SetComponentTickEnabled(false);
    Movement->SetMovementMode(MOVE_Walking);
    auto EarnStep = [&]()
    {
        Movement->Velocity = FVector(Movement->WalkSpeed, 0, 0);
        Player->SetActorLocation(Player->GetActorLocation() + Movement->Velocity);
        Momentum->AdvanceLoop(1);
    };
    for (int32 Step = 0; Step < 40 && Momentum->GetMomentum() < 35; ++Step) EarnStep();
    Movement->StopMovementImmediately();
    const float Below = Momentum->GetMomentum();
    if (!TestTrue(TEXT("Normal movement income lies between old and live costs"), Below >= 30 && Below < 60)) return false;
    TestFalse(TEXT("HUD refuses insufficient live cost"), Abilities->CanAffordSlot(Slot));
    TestFalse(TEXT("GAS slot activation also refuses"), Abilities->TryActivateSlot(Slot));
    TestEqual(TEXT("Refusal spends nothing"), Momentum->GetMomentum(), Below, .0001f);
    for (int32 Step = 0; Step < 40 && Momentum->GetMomentum() < 65; ++Step) EarnStep();
    Movement->StopMovementImmediately();
    const float Before = Momentum->GetMomentum();
    if (!TestTrue(TEXT("Normal income funds doubled payment"), Before >= 60)) return false;
    TestTrue(TEXT("HUD agrees payment is affordable"), Abilities->CanAffordSlot(Slot));
    if (!TestTrue(TEXT("Real paid slot cast succeeds"), Abilities->TryActivateSlot(Slot))) return false;
    TestEqual(TEXT("Actual debit equals the displayed live cost"), Before - Momentum->GetMomentum(), 60.0f, .001f);
    TestTrue(TEXT("Actual Hard Stop window opened"), UBreakerAbilityStateComponent::FindOrAdd(Player)->IsWindowActive(UBreakerAbility_HardStop::WindowKey()));
    return true;
}
#endif
