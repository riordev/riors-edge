#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerAlteredEnemy.h"
#include "Combat/BreakerWardenEnemy.h"
#include "Combat/BreakerHoldfastEnemy.h"
#include "Combat/BreakerRangedEnemy.h"
#include "Combat/BreakerSkirmisherEnemy.h"
#include "Combat/BreakerEnemyProjectile.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerStatusComponent.h"
#include "Combat/BreakerVoid.h"
#include "Components/SphereComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerEnemyFamilyElementTest, "RiorsEdge.Combat.EnemyFamilyElements",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerEnemyFamilyElementTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const uint64 InitialFrame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = InitialFrame; };
    auto* Player = World->SpawnActor<ABreakerCharacter>(FVector(0, 0, 100), FRotator::ZeroRotator);
    if (!Player) return false;
    Player->GetAbilitySystemComponent()->InitAbilityActorInfo(Player, Player);
    Player->GetAbilitySystemComponent()->AddAttributeSetSubobject(Player->GetAttributes());
    Player->GetCombat()->BindAttributes(Player->GetAttributes());
    Player->SetActorTickEnabled(false); Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    auto* Status = Player->FindComponentByClass<UBreakerStatusComponent>();
    if (!Status) return false;
    Status->SetComponentTickEnabled(false);
    // Real late-spawn BeginPlay for projectiles; deliberately do not dispatch
    // the existing player's BeginPlay, which loads the owner's save.
    World->SetBegunPlay(true);
    TestEqual(TEXT("authored Altered share"), BreakerVoid::AlteredAttackFraction(), .5f);
    TestEqual(TEXT("authored Altered resistance"), BreakerVoid::AlteredResistancePercent(), 25.0f);
    for (UClass* Class : {ABreakerAlteredEnemy::StaticClass(), ABreakerWardenEnemy::StaticClass(),
        ABreakerHoldfastEnemy::StaticClass(), ABreakerRangedEnemy::StaticClass(), ABreakerSkirmisherEnemy::StaticClass()})
    {
        auto Label = [&](const TCHAR* Message) { return FString::Printf(TEXT("%s: %s"), *Class->GetName(), Message); };
        const bool Ranged = Class == ABreakerRangedEnemy::StaticClass() || Class == ABreakerSkirmisherEnemy::StaticClass();
        Player->GetAttributes()->ApplyMaxHealth(10000); Player->GetAttributes()->ApplyHealth(10000);
        Player->GetAttributes()->ApplyShield(0);
        Status->ConsumeAllStatuses(); Status->AdvanceStatuses(5.0f);
        auto* Enemy = World->SpawnActor<ABreakerEnemy>(Class, FVector(Ranged ? 1000 : 200, 0, 100), FRotator(0, 180, 0));
        if (!TestNotNull(Label(TEXT("real authored enemy")), Enemy)) return false;
        Enemy->SetActorTickEnabled(false);
        if (auto* Movement = Enemy->GetMovementComponent()) Movement->SetComponentTickEnabled(false);
        const bool Altered = Enemy->GetFamily() == EBreakerEnemyFamily::Altered;
        auto* EnemyStatus = Enemy->FindComponentByClass<UBreakerStatusComponent>();
        if (!TestNotNull(Label(TEXT("enemy status receiver")), EnemyStatus)) return false;
        TestEqual(Label(TEXT("family resistance uses actual chassis")), EnemyStatus->GetVoidResistancePercent(), Altered ? 25.0f : 0.0f);
        if (auto* Skirmisher = Cast<ABreakerSkirmisherEnemy>(Enemy))
        {
            Skirmisher->Tick(.01f);
            TestEqual(Label(TEXT("the player's capsule is not world cover in an empty arena")),
                Skirmisher->GetCoverState(), EBreakerCoverState::Exposed);
        }
        if (auto* Lattice = Cast<ABreakerRangedEnemy>(Enemy))
        {
            auto* Wall = World->SpawnActor<AActor>();
            if (!Wall) return false;
            auto* Box = NewObject<UBoxComponent>(Wall); Wall->AddInstanceComponent(Box); Wall->SetRootComponent(Box);
            Box->SetBoxExtent(FVector(20, 300, 300));
            Box->SetCollisionProfileName(TEXT("BlockAll")); Box->RegisterComponent();
            Wall->SetActorLocation(FVector(500, 0, 150));
            Lattice->Tick(.01f);
            TestFalse(Label(TEXT("actual static wall still prevents an aimed volley")), Lattice->IsWindingUp());
            Wall->Destroy();
        }
        if (Class == ABreakerAlteredEnemy::StaticClass())
        {
            Player->GetCombat()->DodgeChance = 1;
            Enemy->Tick(.01f);
            TestEqual(Label(TEXT("dodged real Altered melee cannot build Void")), Status->GetVoidBuildup(), 0.0f);
            TestEqual(Label(TEXT("dodged real Altered melee deals no damage")), Player->GetAttributes()->GetHealth(), 10000.0f);
            Player->GetCombat()->DodgeChance = 0;
        }
        bool SawRound = false;
        for (int32 Step = 0; Step < 100 && Player->GetAttributes()->GetHealth() == 10000; ++Step)
        {
            ++GFrameCounter; World->Tick(LEVELTICK_All, .05f); Enemy->Tick(.05f);
            for (TActorIterator<ABreakerEnemyProjectile> It(World); It; ++It)
            {
                auto* Round = *It;
                if (Round->GetInstigator() != Enemy) continue;
                SawRound = true;
                TestTrue(Label(TEXT("actual shot has begun play")), Round->HasActorBegunPlay());
                TestEqual(Label(TEXT("actual shot snapshots family element")), Round->GetProjectileDamage().Element,
                    Altered ? EBreakerElement::Void : EBreakerElement::Entropy);
                TestEqual(Label(TEXT("actual shot snapshots half conversion")), Round->GetProjectileDamage().ElementalFraction, .5f);
                if (Class == ABreakerSkirmisherEnemy::StaticClass())
                {
                    const auto* Sphere = Round->FindComponentByClass<USphereComponent>();
                    const auto* Mesh = Round->FindComponentByClass<UStaticMeshComponent>();
                    if (!Sphere || !Mesh) return false;
                    TestEqual(Label(TEXT("real begun Skirmisher round uses authored small collision")), Sphere->GetUnscaledSphereRadius(), 14.0f);
                    TestEqual(Label(TEXT("real begun Skirmisher round uses small visual")), Mesh->GetRelativeScale3D().X, .30, .0001);
                }
                if (auto* Movement = Round->FindComponentByClass<UProjectileMovementComponent>()) { Movement->StopMovementImmediately(); Movement->Deactivate(); }
                // Delivery isolation: actual fired projectile resolves through
                // production Impact; travel precision is covered independently.
                Round->Impact(Player, Player->GetActorLocation() + FVector(40, 0, 0));
            }
        }
        const float Lost = 10000 - Player->GetAttributes()->GetHealth();
        TestTrue(Label(TEXT("actual authored attack lands positive damage")), Lost > 0);
        if (Ranged) TestTrue(Label(TEXT("real AI emitted a projectile")), SawRound);
        TestEqual(Label(TEXT("accepted damage produces half-share family buildup")),
            Altered ? Status->GetVoidBuildup() : Status->GetEntropyBuildup(), Lost * .5f, .01f);
        TestEqual(Label(TEXT("other family does not silently accumulate")),
            Altered ? Status->GetEntropyBuildup() : Status->GetVoidBuildup(), 0.0f);
        Enemy->Destroy();
        TArray<ABreakerEnemyProjectile*> Left;
        for (TActorIterator<ABreakerEnemyProjectile> It(World); It; ++It) Left.Add(*It);
        for (auto* Round : Left) Round->Destroy();
    }
    return true;
}
#endif
