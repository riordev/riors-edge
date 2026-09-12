#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerMomentumComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerLootLibrary.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerSwiftMovementNodesRuntimeTest, "RiorsEdge.Movement.SwiftPurchasedFallCredit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerSwiftMovementNodesRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated movement world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    ABreakerCharacter* Player = World->SpawnActor<ABreakerCharacter>();
    if (!TestNotNull(TEXT("real Swift body"), Player)) return false;
    Player->GetAbilitySystemComponent()->InitAbilityActorInfo(Player, Player);
    Player->GetAbilitySystemComponent()->AddAttributeSetSubobject(Player->GetAttributes());
    Player->GetCombat()->BindAttributes(Player->GetAttributes());
    UBreakerProgressionComponent* Progression = Player->GetProgression();
    Progression->BindAttributes(Player->GetAttributes());
    TestTrue(TEXT("actual Swift class selection"), Progression->ChoosePermanentClassById(EBreakerClassId::Swift));
    // Full-pool wiring fixture; the campaign's current two-point award is not
    // claimed to fund both nodes exercised in this movement test.
    Progression->GrantPlaytestPoints(8, 0);
    UBreakerMomentumComponent* Momentum = Player->FindComponentByClass<UBreakerMomentumComponent>();
    if (!TestNotNull(TEXT("real Momentum component"), Momentum)) return false;
    Momentum->BindAttributes(Player->GetAttributes());
    UBreakerCharacterMovementComponent* Movement = Player->GetBreakerMovement();
    Movement->bRunPhysicsWithNoController = true;
    const UBreakerProgressionTree* Tree = UBreakerProgressionLibrary::GetSwiftKineticTree();
    auto Buy = [&](const TCHAR* Id)
    {
        FText Reason;
        const bool bResult = Progression->PurchaseNode(Tree, Id, Reason);
        return TestTrue(FString::Printf(TEXT("%s: %s"), Id, *Reason.ToString()), bResult);
    };
    Movement->SetMovementMode(MOVE_Walking); Momentum->AdvanceLoop(0.01f);
    TestEqual(TEXT("unowned window is shipped three seconds"), Momentum->GetAirborneCreditRemaining(), 3.0f);
    Movement->SetMovementMode(MOVE_Falling); Momentum->AdvanceLoop(1.0f);
    // O272: Read the Room is a single rank and its one buy reads 6.0 s.
    if (!Buy(TEXT("Swift.Kinetic.ReadTheRoom"))) return false;
    TestEqual(TEXT("one Read the Room buy spends one of eight"), Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), 7);
    Momentum->AdvanceLoop(0.5f);
    TestEqual(TEXT("buying midair does not refill credit"), Momentum->GetAirborneCreditRemaining(), 1.5f);
    Movement->SetMovementMode(MOVE_Walking); Momentum->AdvanceLoop(0.01f);
    TestEqual(TEXT("owned Read the Room refills six seconds on actual grounded mode"), Momentum->GetAirborneCreditRemaining(), 6.0f);
    {
        FText Refusal;
        TestFalse(TEXT("owned Read the Room refuses a second purchase"), Progression->PurchaseNode(Tree, TEXT("Swift.Kinetic.ReadTheRoom"), Refusal));
        TestEqual(TEXT("refused purchase spends nothing"), Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), 7);
    }
    Momentum->AdvanceLoop(0.01f);
    TestEqual(TEXT("refusal leaves the six-second window"), Momentum->GetAirborneCreditRemaining(), 6.0f);
    Player->GetAttributes()->ApplyClassResource(0);
    Movement->SetMovementMode(MOVE_Falling); Momentum->AdvanceLoop(6.5f);
    TestTrue(TEXT("last partial frame pays exactly six seconds of airborne income"), FMath::IsNearlyEqual(Momentum->GetMomentum(), 6.0f * Momentum->AirborneRate, 0.01f));
    TestEqual(TEXT("airborne credit exhausted"), Momentum->GetAirborneCreditRemaining(), 0.0f);

    AActor* Floor = World->SpawnActor<AActor>();
    UBoxComponent* Box = NewObject<UBoxComponent>(Floor);
    Floor->AddInstanceComponent(Box); Floor->SetRootComponent(Box);
    Box->SetBoxExtent(FVector(2000, 2000, 10)); Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Box->SetCollisionResponseToAllChannels(ECR_Block); Box->RegisterComponent();
    Floor->SetActorLocation(FVector(0, 0, -10));
    const float HalfHeight = Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    FBreakerItemInstance ShieldItem;
    for (int32 Seed = 1; Seed <= 256; ++Seed)
    {
        ShieldItem = UBreakerLootLibrary::RollItem(TEXT("Swift.LandingShield"), EBreakerEquipSlot::BodyArmour, EBreakerItemRarity::Standard, 1, Seed);
        if (ShieldItem.ArmourArchetype == EBreakerArmourArchetype::Shield) break;
    }
    // A real level-one shield base lets the cap case survive lethal-height
    // environmental damage. Unrelated affix bonuses are excluded from income.
    for (auto& Affix : ShieldItem.Affixes) Affix.Value = 0;
    Player->GetEquipment()->BindAttributes(Player->GetAttributes());
    if (!TestTrue(TEXT("actual rolled Shield body equips"), Player->GetEquipment()->EquipItem(ShieldItem))) return false;
    if (!TestTrue(TEXT("gear supplies positive shield before fall"), Player->GetAttributes()->GetMaxShield() > 0)) return false;
    auto Fall = [&](float Distance, bool bTeleportNearGround)
    {
        // Each living fall starts with restored real pools; the explicit corpse
        // case below remains dead and must still receive zero Momentum.
        if (!Player->GetCombat()->IsDead()) Player->GetCombat()->RestoreVitals();
        Movement->SetMovementMode(MOVE_None);
        Player->SetActorLocation(FVector(0, 0, HalfHeight + Distance), false, nullptr, ETeleportType::TeleportPhysics);
        Movement->Velocity = FVector::ZeroVector;
        Player->GetAttributes()->ApplyClassResource(20);
        Movement->SetMovementMode(MOVE_Falling);
        bool bRepositioned = false;
        for (int32 Step = 0; Step < 1500 && Movement->IsFalling(); ++Step)
        {
            if (bTeleportNearGround && !bRepositioned && Step == 10)
            {
                Player->SetActorLocation(FVector(0, 0, HalfHeight + 100), false, nullptr, ETeleportType::TeleportPhysics);
                bRepositioned = true;
            }
            // Real engine swept falling/landing physics, not a direct call to
            // ProcessLanded or the resource helper. Resource ticks stay out so
            // only the landing conversion changes the bank in these assertions.
            Movement->PerformMovement(0.02f);
        }
        TestTrue(TEXT("fall actually lands against floor collision"), Movement->IsMovingOnGround());
        return Momentum->GetMomentum() - 20.0f;
    };
    TestEqual(TEXT("unowned long fall earns nothing"), Fall(1000, false), 0.0f);
    // O272: Landing is a single rank reading 3 Momentum per metre past the
    // 6 m minimum, capped at 30 per landing. A 10 m fall is 4 credited
    // metres, so 12; a 40 m fall is 34 credited metres, so 102 capped to 30.
    if (!Buy(TEXT("Swift.Kinetic.Landing"))) return false;
    TestEqual(TEXT("one Landing buy spends the second of eight"), Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), 6);
    TestTrue(TEXT("owned Landing converts actual ten-metre fall at three per metre"), FMath::IsNearlyEqual(Fall(1000, false), 12.0f, 0.2f));
    const float Paid = Momentum->GetMomentum();
    Movement->PerformMovement(0.02f); Movement->PerformMovement(0.02f);
    TestEqual(TEXT("remaining grounded cannot replay landing credit"), Momentum->GetMomentum(), Paid);
    TestEqual(TEXT("short jump-height fall does not qualify"), Fall(300, false), 0.0f);
    TestEqual(TEXT("direct teleport cannot counterfeit long falling distance"), Fall(1800, true), 0.0f);
    {
        FText Refusal;
        TestFalse(TEXT("owned Landing refuses a second purchase"), Progression->PurchaseNode(Tree, TEXT("Swift.Kinetic.Landing"), Refusal));
        TestEqual(TEXT("refused Landing purchase spends nothing"), Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), 6);
    }
    TestTrue(TEXT("refusal leaves the same ten-metre conversion"), FMath::IsNearlyEqual(Fall(1000, false), 12.0f, 0.2f));
    TestEqual(TEXT("very long fall respects the per-landing cap of thirty"), Fall(4000, false), 30.0f);
    TestFalse(TEXT("shielded cap case survives its actual landing"), Player->GetCombat()->IsDead());
    TestEqual(TEXT("very long fall consumes the real shield pool"), Player->GetAttributes()->GetShield(), 0.0f);
    FBreakerDamageRequest Kill; Kill.BaseDamage = 100000; Kill.bCanCritical = false;
    Player->GetCombat()->ReceiveDamage(Kill);
    TestTrue(TEXT("corpse fixture actually died"), Player->GetCombat()->IsDead());
    TestEqual(TEXT("dead Swift receives no landing resource"), Fall(1000, false), 0.0f);
    Player->GetCombat()->RestoreVitals();
    Progression->DevForceClass(EBreakerClassId::Tank);
    Momentum->BindAttributes(Player->GetAttributes());
    TestFalse(TEXT("other-class fixture is not Swift"), Momentum->IsActiveForOwner());
    TestEqual(TEXT("living non-Swift cannot earn landing Momentum"), Fall(1000, false), 0.0f);
    return true;
}
#endif
