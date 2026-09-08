#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Game/BreakerFernhallCourtyardBuilder.h"
#include "Game/BreakerFernhallCourtyardEncounter.h"
#include "Game/BreakerZoneBuilder.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerRangedEnemy.h"
#include "Combat/BreakerCombatComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Components/CapsuleComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCourtyardEncounterTest, "RiorsEdge.Zone.Fernhall.CourtyardEncounter",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCourtyardEncounterTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    World->InitializeActorsForPlay(FURL());
    TArray<FBreakerZonePiece> Pieces;
    UBreakerZoneBuilder::CollectZonePieces(UBreakerZoneBuilder::FernhallMeshFolder(), Pieces);
    BreakerFernhallCourtyard::FPlan Plan; FString Error;
    if (!TestTrue(TEXT("Actual authored courtyard plan"), BreakerFernhallCourtyard::MakePlan(Pieces,Plan,Error))) return false;
    if (!TestTrue(TEXT("Actual courtyard geometry"), BreakerFernhallCourtyard::Build(World,Plan))) return false;
    const auto Enemies = BreakerSpawnFernhallCourtyardEncounter(World,Plan);
    if (!TestEqual(TEXT("Complete finite quartet"),Enemies.Num(),4)) return false;
    int32 Melee=0, Ranged=0;
    for (auto* Enemy : Enemies)
    {
        Melee += Enemy->GetClass() == ABreakerEnemy::StaticClass();
        Ranged += Enemy->GetClass() == ABreakerRangedEnemy::StaticClass();
        TestEqual(TEXT("Fixed entry level"),Enemy->GetAreaLevel(),5);
        TestEqual(TEXT("Ordinary native rank"),Enemy->GetMonsterRank(),EBreakerMonsterRank::Trash);
        TestEqual(TEXT("Vestige family keeps ordinary feedstock qualification"),Enemy->GetFamily(),EBreakerEnemyFamily::Vestige);
        auto* Capsule=Enemy->FindComponentByClass<UCapsuleComponent>();
        FCollisionQueryParams Query(SCENE_QUERY_STAT(CourtyardBodyTest),false,Enemy);
        TestFalse(TEXT("Actual capsule clears built walls and other bodies"),World->OverlapBlockingTestByChannel(
            Enemy->GetActorLocation(),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(
                Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()),Query));
    }
    TestEqual(TEXT("Three melee"),Melee,3); TestEqual(TEXT("One Lattice"),Ranged,1);
    auto* Attacker=Enemies[0]; Attacker->DispatchBeginPlay(); Attacker->SetActorTickEnabled(false);
    const FVector At=Attacker->GetActorLocation()+Plan.Right*100;
    auto* Player=World->SpawnActor<ABreakerCharacter>(At,FRotator::ZeroRotator);
    auto* Controller=World->SpawnActor<APlayerController>();
    if (!Player || !Controller) return false;
    Controller->Possess(Player);
    auto* ASC=Player->GetAbilitySystemComponent(); ASC->InitAbilityActorInfo(Player,Player); ASC->AddAttributeSetSubobject(Player->GetAttributes());
    Player->GetCombat()->BindAttributes(Player->GetAttributes());
    const float Before=Player->GetAttributes()->GetHealth();
    // A single ordinary native attack, no fabricated player defenses or offense.
    Attacker->Tick(1.0f);
    TestTrue(TEXT("Courtyard melee actually damages ordinary player"),Player->GetAttributes()->GetHealth()<Before);
    TestFalse(TEXT("Ordinary player survives one attack"),Player->GetCombat()->IsDead());
    return true;
}
#endif
