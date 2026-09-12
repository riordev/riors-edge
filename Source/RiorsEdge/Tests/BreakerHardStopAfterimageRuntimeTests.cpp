#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbilityComponent.h"
#include "Abilities/BreakerAbilityDefinition.h"
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
#include "Items/BreakerEquipmentComponent.h"
#include "Progression/BreakerCoreWheelMath.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerHardStopAfterimageRuntimeTest,
    "RiorsEdge.Abilities.HardStopAfterimageRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerHardStopAfterimageRuntimeTest::RunTest(const FString& Parameters)
{
    for (int32 Case = 0; Case < 6; ++Case)
    {
    const bool bSpendToLive = Case == 4;
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    World->InitializeActorsForPlay(FURL());
    auto* Player = World->SpawnActor<ABreakerCharacter>();
    if (!Player) return false;
    Player->bRefuseSavesForPendingCharacter = true;
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
    if (bSpendToLive)
    {
    // O272: Redirect -> SpendToLive is the whole route, one point each, no gate.
    const auto* Tree = UBreakerProgressionLibrary::GetSwiftKineticTree();
    TestEqual(TEXT("Fixture ships the eight-point doctrine wallet"), Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), 8);
    for (const TCHAR* Node : {TEXT("Swift.Kinetic.Redirect"), TEXT("Swift.Kinetic.SpendToLive")})
    {
        const bool Bought = Progression->PurchaseNode(Tree, Node, Reason);
        if (!TestTrue(FString::Printf(TEXT("Legal purchase %s: %s"), Node, *Reason.ToString()), Bought)) return false;
    }
    TestEqual(TEXT("Legal path spends two of eight Doctrine, six unspent"), Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), 6);
    TestEqual(TEXT("Live instance quotes doubled cost before first cast"), Abilities->GetCost(Slot), 60.0f, .0001f);
    }
    const auto* Core = UBreakerProgressionLibrary::GetCoreSliceTree();
    for (const TCHAR* Id : {TEXT("Core.Duration.Hold"), TEXT("Core.Duration.Extend"), TEXT("Core.Duration.Uptime"),
        TEXT("Core.Duration.Settle"), TEXT("Core.Duration.Standing"), TEXT("Core.Duration.Afterimage")})
        if (!TestTrue(FString::Printf(TEXT("Actual paid Afterimage route %s"), Id), Progression->PurchaseNode(Core, Id, Reason))) return false;
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
    for (int32 Step = 0; Step < 40 && Momentum->GetMomentum() < 90; ++Step) EarnStep();
    Movement->StopMovementImmediately();
    const float Cost = bSpendToLive ? 60.0f : 30.0f;
    const float Before = Momentum->GetMomentum();
    if (!TestTrue(TEXT("Normal movement funds paid protection"), Before >= Cost)) return false;
    if (!TestTrue(TEXT("Actual paid Hard Stop"), Abilities->TryActivateSlot(Slot))) return false;
    TestEqual(TEXT("Actual authored payment"), Before - Momentum->GetMomentum(), Cost, .001f);
    auto* Combat = Player->GetCombat();
    auto Hit = [&]()
    {
        FBreakerDamageRequest Request;
        Request.BaseDamage = 20;
        Request.DamageFamily = EBreakerDamageFamily::Elemental;
        Request.bCanCritical = false;
        Request.bCanBeAvoided = false;
        Request.bBypassShield = true;
        return Combat->ReceiveDamage(Request).HealthDamage;
    };
    TestEqual(TEXT("Paid window applies its actual damage protection"), Hit(),
        20.0f * UBreakerAbility_HardStop::IncomingMultiplier(bSpendToLive,
            UBreakerAbility_HardStop::DamageReductionFraction), .001f);
    auto* State = UBreakerAbilityStateComponent::FindOrAdd(Player);
    // This isolated pawn intentionally never enters BeginPlay/save loading.
    // Drive the real State clock once per world tick without auto double ticks.
    State->SetComponentTickEnabled(false);
    const uint64 Frame = GFrameCounter;
    ON_SCOPE_EXIT { GFrameCounter = Frame; };
    auto AdvanceFrames = [&](int32 Count)
    {
        for (int32 I=0; I<Count; ++I) { ++GFrameCounter; World->Tick(LEVELTICK_All,.05f); State->TickComponent(.05f, LEVELTICK_All, nullptr); }
    };
    if (Case == 1)
    {
        auto* Spec = ASC->FindAbilitySpecFromClass(UBreakerAbility_HardStop::StaticClass());
        if (!TestNotNull(TEXT("Actual equipped spec"), Spec)) return false;
        ASC->ClearAbility(Spec->Handle);
        TestEqual(TEXT("Inactive grant removal revokes full protection"), Hit(), 20.0f, .001f);
    }
    AdvanceFrames(16);
    TestFalse(TEXT("Permission window ends before numerical tail"), State->IsWindowActive(UBreakerAbility_HardStop::WindowKey()));
    const bool bTail = Case != 1 && !bSpendToLive;
    TestEqual(TEXT("Half contribution only, no immunity tail"), Hit(), bTail ? 17.0f : 20.0f, .001f);
    if (Case == 2)
    {
        auto* Spec = ASC->FindAbilitySpecFromClass(UBreakerAbility_HardStop::StaticClass());
        if (!TestNotNull(TEXT("Actual inactive spec during tail"), Spec)) return false;
        ASC->ClearAbility(Spec->Handle);
        TestEqual(TEXT("Grant removal revokes existing tail"), Combat->GetComposedIncomingDamageMultiplier(), 1.0f, .001f);
    }
    if (Case == 3)
    {
        const auto Price = BreakerCoreRespecCost(Progression->GetCharacterLevel());
        auto* Equipment = Player->GetEquipment();
        const int32 Wallet = Equipment->GetForgeWallet().Get();
        Equipment->GrantForgeCurrency(Price.Amount + 1); // Respec debit is the fixture subject.
        if (!TestTrue(TEXT("Paid Core respec during protection tail"), Progression->RespecCore(Reason))) return false;
        TestEqual(TEXT("Real respec currency debit"), Equipment->GetForgeWallet().Get(), Wallet + 1);
        TestEqual(TEXT("Respec revokes tail immediately"), Combat->GetComposedIncomingDamageMultiplier(), 1.0f, .001f);
        for (const TCHAR* Id : {TEXT("Core.Duration.Hold"), TEXT("Core.Duration.Extend"), TEXT("Core.Duration.Uptime"),
            TEXT("Core.Duration.Settle"), TEXT("Core.Duration.Standing"), TEXT("Core.Duration.Afterimage")})
            if (!Progression->PurchaseNode(Core, Id, Reason)) return false;
        TestEqual(TEXT("Rebuy cannot restore old tail"), Combat->GetComposedIncomingDamageMultiplier(), 1.0f, .001f);
    }
    if (Case != 0) AdvanceFrames(40);
    if (Case != 0) TestEqual(TEXT("Expired lease is neutral"), Combat->GetComposedIncomingDamageMultiplier(), 1.0f, .001f);
    FBreakerDamageRequest Lethal;
    Lethal.BaseDamage = (Attributes->GetHealth() + Attributes->GetShield() + 1) / Combat->GetComposedIncomingDamageMultiplier();
    Lethal.DamageFamily = EBreakerDamageFamily::TrueDamage;
    Lethal.bCanCritical = false; Lethal.bCanBeAvoided = false;
    Combat->ReceiveDamage(Lethal);
    if (!TestTrue(TEXT("Actual lethal damage during tail or after expiry"), Combat->IsDead())) return false;
    Combat->RestoreVitals();
    TestEqual(TEXT("Revive does not restore protection"),Hit(),20.0f,.001f);
    }
    return true;
}
#endif
