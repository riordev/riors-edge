#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbility_Cleave.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerManaComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerStatusComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerExperience.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCleaveAcceptedHitRuntimeTest,
    "RiorsEdge.Abilities.CleaveAcceptedHitRuntime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCleaveAcceptedHitRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    const auto Bleed = FGameplayTag::RequestGameplayTag(TEXT("Status.Bleed"));
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    // Independent real casts avoid cooldown/resource resets. Native component
    // initialization deliberately omits Character BeginPlay and its owner saves.
    for (int32 Scenario = 0; Scenario < 5; ++Scenario)
    {
        const FVector Origin(Scenario * 5000, 0, 200);
        auto* Caster = World->SpawnActor<ABreakerCharacter>(Origin, FRotator::ZeroRotator, Spawn);
        auto* Front = World->SpawnActor<ABreakerCharacter>(Origin + FVector(110, 0, 0), FRotator(0, 180, 0), Spawn);
        auto* Rear = World->SpawnActor<ABreakerCharacter>(Origin + FVector(240, 0, 0), FRotator(0, 180, 0), Spawn);
        if (!Caster || !Front || !Rear) return false;
        for (auto* Player : { Caster, Front, Rear })
        {
            Player->SetActorTickEnabled(false);
            Player->GetBreakerMovement()->SetComponentTickEnabled(false);
            auto* ASC = Player->GetAbilitySystemComponent();
            ASC->InitAbilityActorInfo(Player, Player);
            ASC->AddAttributeSetSubobject(Player->GetAttributes());
            Player->GetCombat()->BindAttributes(Player->GetAttributes());
            Player->GetCombat()->BeginPlay();
            Player->GetProgression()->BindAttributes(Player->GetAttributes());
            Player->GetCombat()->DodgeChance = 0;
            Player->GetCombat()->BlockChance = 0;
            Player->FindComponentByClass<UBreakerStatusComponent>()->BeginPlay();
        }
        if (!Caster->GetProgression()->ChoosePermanentClassById(EBreakerClassId::Caster)) return false;
        auto* Mana = Caster->GetMana();
        Mana->BindAttributes(Caster->GetAttributes());
        Mana->AdvanceLoop(20); // Ordinary passive recovery funds the cast.
        Mana->PassiveRegenPerSecond = 0; // Isolate status income after payment.
        AActor* Wall = nullptr;
        if (Scenario == 1)
        {
            Wall = World->SpawnActor<AActor>();
            auto* Box = NewObject<UBoxComponent>(Wall);
            Wall->AddInstanceComponent(Box); Wall->SetRootComponent(Box);
            Box->SetBoxExtent(FVector(10, 150, 150));
            Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            Box->SetCollisionObjectType(ECC_WorldStatic);
            Box->SetCollisionResponseToAllChannels(ECR_Block);
            Box->RegisterComponent(); Wall->SetActorLocation(Origin + FVector(55, 0, 0));
        }
        if (Scenario >= 2) Rear->SetActorLocation(Origin + FVector(2000, 0, 0));
        if (Scenario == 2) Front->GetCombat()->DodgeChance = 1;
        if (Scenario == 3)
        {
            auto* Progression = Front->GetProgression();
            Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(4, Progression->ExperienceCurve));
            FText Reason;
            for (const TCHAR* Node : { TEXT("Core.Bulwark.SetStance"), TEXT("Core.Bulwark.Read"), TEXT("Core.Bulwark.Parry") })
                if (!TestTrue(TEXT("actual earned Core parry purchase"), Progression->PurchaseNode(UBreakerProgressionLibrary::GetCoreSliceTree(), Node, Reason))) return false;
            if (!TestTrue(TEXT("real frontal parry starts"), Front->GetCombat()->TryParry())) return false;
        }
        if (Scenario == 4) Front->GetAttributes()->ApplyHealth(1);
        auto* ASC = Caster->GetAbilitySystemComponent();
        const auto Handle = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Cleave::StaticClass(), 1));
        const float BeforeMana = Mana->GetMana();
        const float FrontHealth = Front->GetAttributes()->GetHealth();
        const float RearHealth = Rear->GetAttributes()->GetHealth();
        if (!TestTrue(TEXT("real paid Cleave activates"), ASC->TryActivateAbility(Handle))) return false;
        const float PaidMana = Mana->GetMana();
        TestTrue(TEXT("Cleave spends ordinary Mana"), PaidMana < BeforeMana);
        Mana->AdvanceLoop(1);
        auto* FrontStatus = Front->FindComponentByClass<UBreakerStatusComponent>();
        auto* RearStatus = Rear->FindComponentByClass<UBreakerStatusComponent>();
        if (Scenario == 0)
        {
            TestTrue(TEXT("front open target takes actual swing"), Front->GetAttributes()->GetHealth() < FrontHealth);
            TestTrue(TEXT("aligned rear target is not occluded by Pawn"), Rear->GetAttributes()->GetHealth() < RearHealth);
            TestTrue(TEXT("both accepted hits carry Bleed"), FrontStatus->HasStatus(Bleed) && RearStatus->HasStatus(Bleed));
            TestTrue(TEXT("accepted Bleed earns normal status income"), Mana->GetMana() > PaidMana);
        }
        else
        {
            TestFalse(TEXT("blocked avoided or lethal hit cannot carry Bleed"), FrontStatus->HasStatus(Bleed));
            // A lethal swing is still an accepted melee hit. Its ordinary hit
            // income remains valid; only the nonexistent Bleed must pay nothing.
            if (Scenario == 4)
            {
                TestEqual(TEXT("shipped baseline melee income"), Mana->WeaponHitGain, 1.5f);
                TestEqual(TEXT("lethal swing pays only ordinary melee income, no Bleed refund"),
                    Mana->GetMana(), PaidMana + Mana->WeaponHitGain);
            }
            else TestEqual(TEXT("refused payload gives no status refund"), Mana->GetMana(), PaidMana);
            if (Scenario != 4) TestEqual(TEXT("wall dodge or parry prevents health damage"), Front->GetAttributes()->GetHealth(), FrontHealth);
            else TestTrue(TEXT("lethal control actually kills"), Front->GetCombat()->IsDead());
            if (Scenario == 1)
            {
                TestEqual(TEXT("real wall also shields rear target"), Rear->GetAttributes()->GetHealth(), RearHealth);
                TestFalse(TEXT("wall blocks rear Bleed"), RearStatus->HasStatus(Bleed));
            }
            if (Scenario == 3) TestFalse(TEXT("swing consumes real parry window"), Front->GetCombat()->IsParryActive());
        }
        ASC->CancelAbilityHandle(Handle);
        if (Wall) Wall->Destroy();
        Rear->Destroy(); Front->Destroy(); Caster->Destroy();
    }
    return true;
}
#endif
