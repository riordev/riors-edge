#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerStatusComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerEnemyMeleeObstructionTest, "RiorsEdge.Combat.EnemyMeleeObstruction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerEnemyMeleeObstructionTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const uint64 InitialFrame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = InitialFrame; };
    auto* Player = World->SpawnActor<ABreakerCharacter>(FVector(0, 0, 100), FRotator::ZeroRotator);
    if (!Player) return false;
    auto* ASC = Player->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Player->GetAttributes());
    Player->GetCombat()->BindAttributes(Player->GetAttributes()); Player->GetCombat()->BeginPlay();
    Player->GetProgression()->BindAttributes(Player->GetAttributes());
    Player->SetActorTickEnabled(false); Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    auto* Status = Player->FindComponentByClass<UBreakerStatusComponent>();
    if (!Status) return false;
    Status->SetComponentTickEnabled(false);
    auto* Enemy = World->SpawnActor<ABreakerEnemy>(FVector(200, 0, 100), FRotator::ZeroRotator);
    if (!Enemy) return false;
    Enemy->ConfigureCrowdProbe(); Enemy->DispatchBeginPlay(); Enemy->SetActorTickEnabled(false);
    if (auto* Movement = Enemy->GetMovementComponent()) Movement->SetComponentTickEnabled(false);
    auto* Wall = World->SpawnActor<AActor>();
    if (!Wall) return false;
    auto* Box = NewObject<UBoxComponent>(Wall); Wall->AddInstanceComponent(Box); Wall->SetRootComponent(Box);
    Box->SetBoxExtent(FVector(10, 150, 150)); Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Box->SetCollisionResponseToAllChannels(ECR_Block); Box->RegisterComponent(); Wall->SetActorLocation(FVector(100, 0, 100));
    const float InitialHealth = Player->GetAttributes()->GetHealth();
    Enemy->Tick(.01f);
    TestEqual(TEXT("solid cover blocks actual melee damage"), Player->GetAttributes()->GetHealth(), InitialHealth);
    TestEqual(TEXT("blocked melee cannot build Entropy"), Status->GetEntropyBuildup(), 0.0f);
    Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Enemy->SetActorLocation(FVector(100, 0, 100 + Enemy->GetAttackRange() + 1));
    Enemy->Tick(.01f);
    TestEqual(TEXT("planar proximity cannot strike across floors"), Player->GetAttributes()->GetHealth(), InitialHealth);
    Enemy->SetActorLocation(FVector(200, 0, 100));
    Enemy->Tick(.01f);
    TestTrue(TEXT("unobstructed grounded enemy still deals damage"), Player->GetAttributes()->GetHealth() < InitialHealth);
    TestTrue(TEXT("real open strike still delivers Entropy"), Status->GetEntropyBuildup() > 0
        || Status->HasStatus(FGameplayTag::RequestGameplayTag(TEXT("Status.Rot"))));
    auto Advance = [&](float Seconds)
    {
        for (float Time = 0; Time < Seconds; Time += .05f) { ++GFrameCounter; World->Tick(LEVELTICK_All, .05f); }
    };
    Advance(Enemy->GetAttackCooldown() + .1f);
    // Explicit level entitlement fixture, actual four-point prerequisite purchase.
    auto* Progression = Player->GetProgression();
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(4, Progression->ExperienceCurve));
    FText Failure;
    for (const TCHAR* Node : {TEXT("Core.Bulwark.Read"), TEXT("Core.Bulwark.Guard"), TEXT("Core.Bulwark.Parry")})
        if (!TestTrue(TEXT("actual Core parry purchase"), Progression->PurchaseNode(UBreakerProgressionLibrary::GetCoreSliceTree(), Node, Failure))) return false;
    auto* Combat = Player->GetCombat(); Combat->BlockChance = 0; Combat->DodgeChance = 0;
    const float BeforeParry = Player->GetAttributes()->GetHealth();
    const float BeforeBuildup = Status->GetEntropyBuildup();
    if (!TestTrue(TEXT("purchased frontal parry starts"), Combat->TryParry())) return false;
    Enemy->Tick(.01f);
    TestFalse(TEXT("real enemy source location consumes frontal parry"), Combat->IsParryActive());
    TestEqual(TEXT("frontal enemy melee is negated"), Player->GetAttributes()->GetHealth(), BeforeParry);
    TestEqual(TEXT("parried enemy hit adds no buildup"), Status->GetEntropyBuildup(), BeforeBuildup);
    Advance(Combat->ParryCooldownSeconds + .1f);
    Enemy->SetActorLocation(FVector(-200, 0, 100));
    if (!Combat->TryParry()) return false;
    const float BeforeRear = Player->GetAttributes()->GetHealth();
    Enemy->Tick(.01f);
    TestTrue(TEXT("rear enemy cannot be parried by forward stance"), Player->GetAttributes()->GetHealth() < BeforeRear);
    TestTrue(TEXT("rear hit leaves frontal window unconsumed"), Combat->IsParryActive());
    return true;
}
#endif
