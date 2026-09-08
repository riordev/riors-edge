#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerGunsmithAbilities.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerScrapComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDeployable.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "TimerManager.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerTempoDeployRuntimeTest, "RiorsEdge.Abilities.TempoDeployRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerTempoDeployRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    // AActor::ProcessEvent refuses reflected callbacks until actors are initialized.
    // Initialize world dispatch without beginning the player (which would load owner saves).
    World->InitializeActorsForPlay(FURL());
    const uint64 InitialFrame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = InitialFrame; };
    auto* Player = World->SpawnActor<ABreakerCharacter>(FVector(0, 0, 200), FRotator::ZeroRotator);
    auto* Controller = World->SpawnActor<APlayerController>();
    if (!Player || !Controller) return false;
    Player->SetActorTickEnabled(false); Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    Controller->SetInitialLocationAndRotation(Player->GetActorLocation(), FRotator(-20, 0, 0));
    Controller->Possess(Player); Controller->SetControlRotation(FRotator(-20, 0, 0));
    auto* ASC = Player->GetAbilitySystemComponent(); auto* Attributes = Player->GetAttributes();
    ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Attributes);
    Attributes->SetCriticalChance(0);
    Player->GetCombat()->BindAttributes(Attributes);
    auto* Progression = Player->GetProgression(); Progression->BindAttributes(Attributes);
    // Explicit separate schema asset, not a modified shipping node or free rank.
    auto* Definition = NewObject<UBreakerClassDefinition>(); Definition->ClassId = EBreakerClassId::Gunsmith;
    auto* Tree = NewObject<UBreakerProgressionTree>(Definition);
    Tree->TreeId = TEXT("Test.Core.DeployCastRate"); Tree->Currency = EBreakerPointCurrency::CorePoints;
    Definition->BranchTrees.Add(Tree);
    for (const auto Target : { EBreakerNodeStatTarget::AbilityCastRate })
    {
        auto* Node = NewObject<UBreakerProgressionNode>(Tree);
        Node->NodeId = TEXT("Test.Deployable.CastRate");
        FBreakerNodeEffect Effect; Effect.StatTarget = Target;
        Effect.ValuePerRank = 100; Effect.StatBucket = EBreakerNodeStatBucket::IncreasedPercent;
        Node->Effects.Add(Effect); Tree->Nodes.Add(Node);
    }
    if (!Progression->ChoosePermanentClass(Definition)) return false;
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(2, Progression->ExperienceCurve));
    FText Reason;

    auto* Scrap = Player->FindComponentByClass<UBreakerScrapComponent>(); Scrap->BindAttributes(Attributes);
    auto* Weapon = Player->GetWeapon(); Weapon->EquipArchetype(EBreakerWeaponArchetype::Rifle);
    // Equipping the default Rifle is a no-op before weapon BeginPlay; initialize its authored magazine/reserve.
    Weapon->ResetAmmunition();
    FScriptDelegate Reload; Reload.BindUFunction(Player, TEXT("HandleClassResourceReloadCompleted"));
    Weapon->OnReloadCompleted.Add(Reload); // The real callback normally bound by save-loading Character.BeginPlay.
    TestTrue(TEXT("Native reload callback resolves"), Reload.IsBound());
    TestEqual(TEXT("Actual permanent Gunsmith is active"), Progression->GetProgressionState().PermanentClass, EBreakerClassId::Gunsmith);
    TestTrue(TEXT("Ordinary resource capacity can fund turret"), Attributes->GetMaxClassResource() >= 50);
    auto Advance = [&](float Seconds)
    {
        for (float Time = 0; Time < Seconds; Time += .05f)
        { ++GFrameCounter; World->Tick(LEVELTICK_All, .05f); if (!World->GetTimerManager().HasBeenTickedThisFrame()) World->GetTimerManager().Tick(.05f); Scrap->AdvanceLoop(.05f); }
    };
    for (int32 Cycle = 0; Cycle < 20 && Scrap->GetScrap() < 50; ++Cycle)
        {
        const int32 BeforeRounds = Weapon->GetMagazineAmmo();
        const float BeforeScrap = Scrap->GetScrap();
        Weapon->StartFire(); Weapon->StopFire();
        if (!TestEqual(TEXT("Funding shot spends an actual round"), Weapon->GetMagazineAmmo(), BeforeRounds - 1)) return false;
        Weapon->StartReload(); Advance(4);
        if (!TestEqual(TEXT("Funding reload replaces the spent round"), Weapon->GetMagazineAmmo(), BeforeRounds)) return false;
        AddInfo(FString::Printf(TEXT("Reload %d Scrap %.3f -> %.3f max %.3f authored grant %.3f generation %.3f authority %d class %d world %.3f"), Cycle, BeforeScrap, Scrap->GetScrap(), Attributes->GetMaxClassResource(), Scrap->ReloadGrant, Scrap->GetGenerationMultiplier(), Player->HasAuthority(), static_cast<int32>(Progression->GetProgressionState().PermanentClass), World->GetTimeSeconds()));
        if (!TestEqual(TEXT("Actual completed reload pays authored Scrap once"), Scrap->GetScrap(), BeforeScrap + Scrap->ReloadGrant, .001f)) return false;
    }
    if (!TestTrue(TEXT("Real spent-round reloads fund deployment"), Scrap->GetScrap() >= 50)) return false;
    auto* Floor = World->SpawnActor<AActor>(); auto* Surface = NewObject<UBoxComponent>(Floor);
    Floor->AddInstanceComponent(Surface); Floor->SetRootComponent(Surface);
    Surface->SetBoxExtent(FVector(2000, 2000, 10)); Surface->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Surface->SetCollisionResponseToAllChannels(ECR_Block); Surface->RegisterComponent(); Floor->SetActorLocation(FVector(0, 0, -10));
        // With no camera startup the viewpoint may use the possessed actor's transform.
    Player->SetActorRotation(FRotator(-20, 0, 0));
    Controller->SetInitialLocationAndRotation(Player->GetActorLocation(), FRotator(-20, 0, 0));
    Controller->SetControlRotation(FRotator(-20, 0, 0));
    FVector Eye, Placement; FRotator View;
    Controller->GetPlayerViewPoint(Eye, View);
    if (!TestTrue(TEXT("Native downward viewpoint resolves real floor placement"),
        ABreakerDeployable::ResolvePlacement(World, Player, Eye, View.Vector(), 800, Placement))) return false;
    const auto Handle = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Turret::StaticClass(), 1));
    auto Live = [&]() -> ABreakerDeployable*
    {
        for (const auto& Entry : ABreakerDeployable::GetLiveDeployables())
            if (Entry.IsValid() && Entry->GetOwningCharacter() == Player) return Entry.Get();
        return nullptr;
    };
    auto Place = [&](bool bRespec, float& Elapsed, float& Cost)
    {
        // Continue the same real reload economy for the second paid placement.
        for (int32 Cycle = 0; Cycle < 20 && Scrap->GetScrap() < 50; ++Cycle)
        { Weapon->StartFire(); Weapon->StopFire(); Weapon->StartReload(); Advance(4); }
        if (!TestTrue(TEXT("Normal reload economy funds this cast"), Scrap->GetScrap() >= 50)) return false;
        Controller->SetControlRotation(FRotator(-20, 0, 0));
        const float Before = Scrap->GetScrap();
        if (!TestTrue(TEXT("Native paid turret cast begins"), ASC->TryActivateAbility(Handle))) return false;
        Elapsed = 0;
        bool bRespecced = false;
        while (!Live() && Elapsed < 4)
        {
            Advance(.05f); Elapsed += .05f;
            if (bRespec && !bRespecced && Elapsed >= .2f)
            {
                if (!TestTrue(TEXT("Actual mid-placement respec"), Progression->RespecCore(Reason))) return false;
                bRespecced = true;
                TestEqual(TEXT("Live cast rate resets"), UBreakerGameplayAbility::AbilityCastRateMultiplierFor(Player), 1.0f);
            }
        }
        auto* Deployed = Live();
        if (!TestNotNull(TEXT("Real pending placement completes"), Deployed)) return false;
        Cost = Before - Scrap->GetScrap();
        TestTrue(TEXT("Placement actually spends resource"), Cost > 0);
        Deployed->Destroy();
        return true;
    };
    float Baseline = 0, Faster = 0, BaseCost = 0, FastCost = 0;
    if (!Place(false, Baseline, BaseCost)) return false;
    if (!TestTrue(TEXT("Earned point buys isolated cast-rate schema"), Progression->PurchaseNode(Tree, Tree->Nodes[0]->NodeId, Reason))) return false;
    if (!Place(true, Faster, FastCost)) return false;
    TestEqual(TEXT("Rate leaves actual deployment cost unchanged"), FastCost, BaseCost, .001f);
    TestTrue(TEXT("Faster placement completes sooner despite mid-cast respec"), Faster < Baseline * .75f);
    TestTrue(TEXT("Faster placement remains a real pending cast"), Faster >= Baseline * .4f);
    return true;
}
#endif
