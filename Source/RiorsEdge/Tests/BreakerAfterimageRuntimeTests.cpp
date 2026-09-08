#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbility_Slipcut.h"
#include "Abilities/BreakerAbility_Overdrive.h"
#include "Abilities/BreakerAbility_Skim.h"
#include "Abilities/BreakerAbility_Sightline.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Abilities/BreakerWindowLaneMath.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerMomentumComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"
#include "Weapons/BreakerWeaponDefinition.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerAfterimageRuntimeTest, "RiorsEdge.Abilities.CoreAfterimageRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerAfterimageRuntimeTest::RunTest(const FString&)
{
    TestEqual(TEXT("Fractional lane contribution remains fractional"), .5f * FBreakerWindowLaneMath::Scale(1, 1, true), .25f);
    TestEqual(TEXT("Hitch beyond both deadlines cannot create late tail"), FBreakerWindowLaneMath::Scale(4,1,true), 0.0f);
    for (int32 Scenario = 0; Scenario < 6; ++Scenario)
    {
        UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
        auto* World = UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
        if (!World) return false;
        GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
        World->InitializeActorsForPlay(FURL());
        const uint64 SavedFrame = GFrameCounter;
        ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = SavedFrame; };
        auto* Player = World->SpawnActor<ABreakerCharacter>();
        Player->SetActorTickEnabled(false); Player->bRefuseSavesForPendingCharacter = true;
        auto* Move = Player->GetBreakerMovement(); Move->SetComponentTickEnabled(false); Move->SetMovementMode(MOVE_Walking);
        auto* ASC = Player->GetAbilitySystemComponent(); auto* Attr = Player->GetAttributes();
        ASC->InitAbilityActorInfo(Player,Player); ASC->AddAttributeSetSubobject(Attr);
        Player->GetCombat()->BindAttributes(Attr);
        auto* Progression = Player->GetProgression(); Progression->BindAttributes(Attr);
        if (!Progression->ChoosePermanentClassById(EBreakerClassId::Swift)) return false;
        auto* Definition = DuplicateObject<UBreakerClassDefinition>(Progression->ClassDefinition,Player);
        auto* Tree = NewObject<UBreakerProgressionTree>(Definition); Tree->TreeId = TEXT("Test.Afterimage");
        Tree->Currency = EBreakerPointCurrency::CorePoints; Definition->BranchTrees.Add(Tree); Progression->ClassDefinition = Definition;
        auto* Node = NewObject<UBreakerProgressionNode>(Tree); Node->NodeId = TEXT("Test.Afterimage.Rule"); Node->Currency = Tree->Currency;
        Node->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Afterimage"))); Tree->Nodes.Add(Node);
        Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(3,Progression->ExperienceCurve));
        FText Reason; if (!Progression->PurchaseNode(Tree,Node->NodeId,Reason)) return false;
        if (Scenario == 5)
        {
            auto* Recast = NewObject<UBreakerProgressionNode>(Tree); Recast->NodeId = TEXT("Test.Afterimage.Conduction");
            Recast->Currency = Tree->Currency;
            Recast->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Conduction"))); Tree->Nodes.Add(Recast);
            if (!Progression->PurchaseNode(Tree,Recast->NodeId,Reason)) return false;
        }
        auto* Momentum = Player->GetMomentum(); Momentum->BindAttributes(Attr); Momentum->SetComponentTickEnabled(false);
        Move->Velocity = FVector(Move->WalkSpeed,0,0);
        for (int32 I=0; I<40; ++I) { Player->SetActorLocation(Player->GetActorLocation()+Move->Velocity); Momentum->AdvanceLoop(1); }
        Move->StopMovementImmediately();
        auto* Weapon = Player->GetWeapon(); const float BaseRate = Weapon->GetFireRateMultiplier();
        const auto Handle = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Slipcut::StaticClass(),1));
        const float Before = Momentum->GetMomentum();
        if (!TestTrue(TEXT("Real paid Slipcut activates"), ASC->TryActivateAbility(Handle))) return false;
        TestTrue(TEXT("Cast spends normally earned Momentum"), Momentum->GetMomentum() < Before);
        TestEqual(TEXT("Paid window receives full rate"), Weapon->GetFireRateMultiplier(),BaseRate*2,.001f);
        auto* State = UBreakerAbilityStateComponent::FindOrAdd(Player);
        // Character BeginPlay is deliberately omitted to avoid loading owner
        // saves. Its state component therefore has no registered actor tick;
        // drive the same component tick explicitly beside the world's clocks.
        State->SetComponentTickEnabled(false);
        auto Tick = [&](float Seconds)
        {
            for (int32 I=0; I<FMath::CeilToInt(Seconds/.01f); ++I)
            {
                ++GFrameCounter; World->Tick(LEVELTICK_All,.01f);
                State->TickComponent(.01f, LEVELTICK_All, nullptr);
            }
        };
        const float Duration = State->GetWindowRemaining(UBreakerAbility_Slipcut::WindowKey());
        if (Scenario == 1)
        {
            State->CloseWindow(UBreakerAbility_Slipcut::WindowKey());
            TestEqual(TEXT("Explicit cancellation refuses tail"),Weapon->GetFireRateMultiplier(),BaseRate,.001f);
        }
        else
        {
            Tick(Duration + .03f);
            TestFalse(TEXT("Normal window permission ends before tail"),State->IsWindowActive(UBreakerAbility_Slipcut::WindowKey()));
            TestEqual(TEXT("Tail halves rate contribution, not total rate"),Weapon->GetFireRateMultiplier(),BaseRate*1.5f,.001f);
            if (Scenario == 2)
            {
                if (!Progression->RespecCore(Reason)) return false;
                TestEqual(TEXT("Respec immediately removes tail"),Weapon->GetFireRateMultiplier(),BaseRate,.001f);
                if (!Progression->PurchaseNode(Tree,Node->NodeId,Reason)) return false;
                TestEqual(TEXT("Rebuy cannot resurrect tail"),Weapon->GetFireRateMultiplier(),BaseRate,.001f);
            }
            else if (Scenario == 3)
            {
                FBreakerDamageRequest Hit; Hit.BaseDamage=100000; Hit.bCanCritical=false; Hit.bCanBeAvoided=false;
                Hit.DamageFamily=EBreakerDamageFamily::TrueDamage;
                Player->GetCombat()->ReceiveDamage(Hit); Player->GetCombat()->RestoreVitals();
                TestEqual(TEXT("Death and revival cannot resurrect tail"),Weapon->GetFireRateMultiplier(),BaseRate,.001f);
            }
            else if (Scenario == 4)
            {
                Weapon->OnReloadChanged.Broadcast(true);
                TestEqual(TEXT("Reload cancels numerical tail"),Weapon->GetFireRateMultiplier(),BaseRate,.001f);
            }
            else if (Scenario == 5)
            {
                const float BeforeRecast = Momentum->GetMomentum();
                if (!TestTrue(TEXT("Purchased Conduction allows actual paid recast during tail"),ASC->TryActivateAbility(Handle))) return false;
                TestTrue(TEXT("Recast spends resource again"),Momentum->GetMomentum() < BeforeRecast);
                TestEqual(TEXT("Paid recast replaces tail instead of multiplying it"),Weapon->GetFireRateMultiplier(),BaseRate*2,.001f);
                const float RecastDuration = State->GetWindowRemaining(UBreakerAbility_Slipcut::WindowKey());
                Tick(RecastDuration + .03f);
                TestEqual(TEXT("Recast creates one new half-strength tail"),Weapon->GetFireRateMultiplier(),BaseRate*1.5f,.001f);
            }
            Tick(2.05f);
            TestEqual(TEXT("Original deadline ends tail permanently"),Weapon->GetFireRateMultiplier(),BaseRate,.001f);
            if (Scenario == 0)
            {
                auto Fund = [&]()
                {
                    Move->Velocity=FVector(Move->WalkSpeed,0,0);
                    for (int32 I=0; I<40; ++I) { Player->SetActorLocation(Player->GetActorLocation()+Move->Velocity); Momentum->AdvanceLoop(1); }
                    Move->StopMovementImmediately();
                };
                Fund();
                const auto Skim = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Skim::StaticClass(),1));
                // Skim's paid speed window requires a successful moving
                // redirect; casting while stationary legitimately grants none.
                const float RedirectSpeed = Move->GetWalkSpeedCap();
                Move->Velocity = FVector(0, RedirectSpeed, 0);
                if (!TestTrue(TEXT("Paid Skim opens real speed lane"),ASC->TryActivateAbility(Skim))) return false;
                TestTrue(TEXT("Paid Skim actually redirects existing momentum"), Move->Velocity.X > 0
                    && FMath::IsNearlyEqual(Move->Velocity.Size2D(), RedirectSpeed, .01f));
                Move->StopMovementImmediately();
                TestEqual(TEXT("Skim full speed contribution"),Move->GetSpeedMultiplier(),1.25f,.001f);
                Tick(UBreakerAbility_Skim::BurstSeconds+.03f);
                TestEqual(TEXT("Skim half speed contribution"),Move->GetSpeedMultiplier(),1.125f,.001f);
                Tick(2.05f);
                TestEqual(TEXT("Skim speed tail ends"),Move->GetSpeedMultiplier(),1.0f,.001f);
                Fund();
                auto* Target=World->SpawnActor<AActor>(); auto* Body=NewObject<USphereComponent>(Target);
                Target->AddInstanceComponent(Body); Target->SetRootComponent(Body); Body->SetSphereRadius(60);
                Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Body->SetCollisionResponseToAllChannels(ECR_Ignore);
                Body->SetCollisionResponseToChannel(ECC_GameTraceChannel2,ECR_Block); Body->RegisterComponent();
                FVector Eye; FRotator Aim; Player->GetActorEyesViewPoint(Eye,Aim); Target->SetActorLocation(Eye+Aim.Vector()*500);
                auto* Sink=NewObject<UBreakerCombatComponent>(Target); Target->AddInstanceComponent(Sink); Sink->RegisterComponent();
                auto* Health=NewObject<UBreakerAttributeSet>(Target); Health->ApplyMaxHealth(100000); Health->ApplyHealth(100000); Sink->BindAttributes(Health);
                Weapon->WeaponDefinition=DuplicateObject<UBreakerWeaponDefinition>(Weapon->GetActiveDefinition(),Weapon);
                Weapon->WeaponDefinition->HipSpreadDegrees=Weapon->WeaponDefinition->AimSpreadDegrees=0; Weapon->WeaponDefinition->BleedChance=0;
                Weapon->ResetAmmunition(); ASC->SetNumericAttributeBase(UBreakerAttributeSet::GetCriticalChanceAttribute(),0);
                auto Shot=[&]() { Weapon->StartFire(); Weapon->StopFire(); return Weapon->GetLastShot().DamageResult.HealthDamage; };
                const float BaseDamage=Shot(); Tick(.3f);
                const auto Ultimate=ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Overdrive::StaticClass(),1));
                if (!TestTrue(TEXT("Actual earned full bar pays Overdrive"),ASC->TryActivateAbility(Ultimate))) return false;
                TestEqual(TEXT("Paid outgoing More reaches actual rifle hit"),Shot(),BaseDamage*1.25f,.02f);
                const float UltimateDuration=State->GetWindowRemaining(UBreakerAbility_Overdrive::WindowKey());
                Tick(UltimateDuration+.03f);
                TestFalse(TEXT("Ultimate permission ends before numerical tail"),State->IsWindowActive(UBreakerAbility_Overdrive::WindowKey()));
                TestEqual(TEXT("Paid outgoing More tail reaches actual rifle hit"),Shot(),BaseDamage*1.125f,.02f);
                Tick(2.05f);
                TestEqual(TEXT("Actual rifle returns to baseline after outgoing tail"),Shot(),BaseDamage,.02f);
                // The same native channel consumer combines half contributions
                // before flooring; projectile count stays fractional for fire.
                const auto BaseChannels=Weapon->GetShotChannels();
                Weapon->PushWindowShotChannelBonus(TEXT("Test.ChannelA"),.5f,1,1,1,.2f);
                Weapon->PushWindowShotChannelBonus(TEXT("Test.ChannelB"),.5f,1,1,1,.2f);
                Tick(.23f);
                const auto TailChannels=Weapon->GetShotChannels();
                TestEqual(TEXT("Two half pierces combine before floor"),TailChannels.PierceCount,BaseChannels.PierceCount+1);
                TestEqual(TEXT("Two half chains combine before floor"),TailChannels.ChainCount,BaseChannels.ChainCount+1);
                TestEqual(TEXT("Two half ricochets combine before floor"),TailChannels.RicochetCount,BaseChannels.RicochetCount+1);
                TestEqual(TEXT("Fractional projectile tails stay fractional"),TailChannels.AdditionalProjectiles,BaseChannels.AdditionalProjectiles+.5f,.001f);
                Tick(2.05f);
                TestEqual(TEXT("Channel lease expires"),Weapon->GetShotChannels().PierceCount,BaseChannels.PierceCount);
                Fund();
                const int32 BeforeSightline=Weapon->GetShotChannels().PierceCount;
                const auto Sightline=ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Sightline::StaticClass(),1));
                if (!TestTrue(TEXT("Actual paid Sightline opens pierce window"),ASC->TryActivateAbility(Sightline))) return false;
                const int32 FullPierce=Weapon->GetShotChannels().PierceCount-BeforeSightline;
                TestTrue(TEXT("Paid Sightline grants actual pierce"),FullPierce>0);
                Tick(State->GetWindowRemaining(UBreakerAbility_Sightline::WindowKey())+.03f);
                TestEqual(TEXT("Paid Sightline tail halves discrete contribution"),Weapon->GetShotChannels().PierceCount,
                    BeforeSightline+FMath::FloorToInt(FullPierce*.5f));
                Weapon->StartFire(); Weapon->StopFire();
                TestEqual(TEXT("Actual first tail shot consumes Sightline"),Weapon->GetShotChannels().PierceCount,BeforeSightline);
            }
        }
    }
    return true;
}
#endif
