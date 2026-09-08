#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerTankAbilities.h"
#include "Abilities/BreakerAbility_CadenceBreak.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerGritComponent.h"
#include "Classes/BreakerMomentumComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"
#include "Save/BreakerMissionContent.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponDefinition.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
    struct FBreakerAfterimageDeliveryFixture
    {
        UWorld* World = nullptr;
        ABreakerCharacter* Player = nullptr;
        AActor* Target = nullptr;
        uint64 SavedFrame = GFrameCounter;
        FBreakerAfterimageDeliveryFixture(EBreakerClassId Class)
        {
            UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
            World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
            if (!World) return;
            GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); World->InitializeActorsForPlay(FURL());
            Player=World->SpawnActor<ABreakerCharacter>(); Player->bRefuseSavesForPendingCharacter=true; Player->SetActorTickEnabled(false);
            Player->GetBreakerMovement()->SetComponentTickEnabled(false); Player->GetBreakerMovement()->SetMovementMode(MOVE_Walking);
            auto* ASC=Player->GetAbilitySystemComponent(); auto* Attr=Player->GetAttributes();
            ASC->InitAbilityActorInfo(Player,Player); ASC->AddAttributeSetSubobject(Attr); Player->GetCombat()->BindAttributes(Attr);
            auto* Progression=Player->GetProgression(); Progression->BindAttributes(Attr); Progression->ChoosePermanentClassById(Class);
            auto* Definition=DuplicateObject<UBreakerClassDefinition>(Progression->ClassDefinition,Player);
            auto* Tree=NewObject<UBreakerProgressionTree>(Definition); Tree->TreeId=TEXT("Test.Afterimage.Delivery"); Tree->Currency=EBreakerPointCurrency::CorePoints;
            Definition->BranchTrees.Add(Tree); Progression->ClassDefinition=Definition;
            auto* Node=NewObject<UBreakerProgressionNode>(Tree); Node->NodeId=TEXT("Test.Afterimage.Delivery.Rule"); Node->Currency=Tree->Currency;
            Node->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Afterimage"))); Tree->Nodes.Add(Node);
            Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(Class==EBreakerClassId::Tank ? 20 : 3,Progression->ExperienceCurve));
            FText Reason; Progression->PurchaseNode(Tree,Node->NodeId,Reason);
            Player->GetGrit()->BindAttributes(Attr); Player->GetGrit()->SetComponentTickEnabled(false);
            Player->GetMomentum()->BindAttributes(Attr); Player->GetMomentum()->SetComponentTickEnabled(false);
            UBreakerAbilityStateComponent::FindOrAdd(Player)->SetComponentTickEnabled(false);
            auto* Weapon=Player->GetWeapon(); Weapon->WeaponDefinition=DuplicateObject<UBreakerWeaponDefinition>(Weapon->GetActiveDefinition(),Weapon);
            Weapon->WeaponDefinition->HipSpreadDegrees=Weapon->WeaponDefinition->AimSpreadDegrees=0; Weapon->WeaponDefinition->BleedChance=0;
            Weapon->ResetAmmunition();
            Target=World->SpawnActor<AActor>(); auto* Body=NewObject<USphereComponent>(Target); Target->AddInstanceComponent(Body); Target->SetRootComponent(Body);
            Body->SetSphereRadius(60); Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Body->SetCollisionResponseToAllChannels(ECR_Ignore);
            Body->SetCollisionResponseToChannel(ECC_GameTraceChannel2,ECR_Block); Body->RegisterComponent(); AimTarget();
            auto* Sink=NewObject<UBreakerCombatComponent>(Target); Target->AddInstanceComponent(Sink); Sink->RegisterComponent();
            auto* Health=NewObject<UBreakerAttributeSet>(Target); Health->ApplyMaxHealth(100000); Health->ApplyHealth(100000); Sink->BindAttributes(Health);
        }
        ~FBreakerAfterimageDeliveryFixture() { if (World) { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); } GFrameCounter=SavedFrame; }
        void AimTarget() { FVector Eye; FRotator Aim; Player->GetActorEyesViewPoint(Eye,Aim); Target->SetActorLocation(Eye+Aim.Vector()*800); }
        void Tick(float Seconds)
        {
            auto* State=UBreakerAbilityStateComponent::FindOrAdd(Player);
            for (int32 I=0; I<FMath::CeilToInt(Seconds/.01f); ++I)
            { ++GFrameCounter; World->Tick(LEVELTICK_All,.01f); State->TickComponent(.01f,LEVELTICK_All,nullptr); }
        }
        float Shot()
        {
            auto* W=Player->GetWeapon();
            const float CriticalMultiplier=Player->GetAttributes()->GetCriticalMultiplier();
            W->StartFire(); W->StopFire();
            const auto& Result=W->GetLastShot().DamageResult;
            // These real hits retain their ordinary critical rolls. Compare
            // the flat lane on the same pre-critical basis for each result;
            // progression recomposition must not change what is measured.
            return Result.HealthDamage / (Result.bCritical ? CriticalMultiplier : 1.0f);
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerAfterimageProvokeTest,"RiorsEdge.Abilities.AfterimageProvokeDelivery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerAfterimageProvokeTest::RunTest(const FString&)
{
    FBreakerAfterimageDeliveryFixture F(EBreakerClassId::Tank); if (!F.Player) return false;
    auto* Player=F.Player; auto* Grit=Player->GetGrit(); auto* Progression=Player->GetProgression();
    FBreakerQuestFlagSet Flags;
    // Restored Act I/II completion entitlement at their authored campaign
    // band, not a claim that this focused fixture plays through both acts.
    for (const auto& Mission:UBreakerMissionLibrary::GetMissions()) if (Mission.Act <= 2) for (const auto& Beat:Mission.Beats)
        for (FName Flag:UBreakerMissionLibrary::BeatCompletionFlags(Beat)) Flags.Add(Flag);
    Progression->SettleDoctrineEntitlement(Flags);
    FText Reason;
    for (const TCHAR* Id:{TEXT("Tank.Bastion.Loud"),TEXT("Tank.Bastion.Loud"),TEXT("Tank.Bastion.AnsweringFire"),TEXT("Tank.Bastion.AnsweringFire")})
        if (!TestTrue(TEXT("Legal Answering Fire path"),Progression->PurchaseNode(UBreakerProgressionLibrary::GetTankBastionTree(),Id,Reason))) return false;
    auto* Enemy=F.World->SpawnActor<ABreakerEnemy>(FVector(0,300,0),FRotator::ZeroRotator);
    Enemy->ConfigureWave(5); Enemy->DispatchBeginPlay();
    // BeginPlay registers/enables the enemy's ticks. Freeze the observation
    // target afterward: this test measures the paid buff and real proximity
    // source, not survival against an un-evaded melee assault.
    Enemy->SetActorTickEnabled(false);
    if (auto* Movement=Enemy->FindComponentByClass<UPawnMovementComponent>())
    { Movement->StopMovementImmediately(); Movement->SetComponentTickEnabled(false); }
    auto Contact=[&]()
    {
        FBreakerDamageRequest Hit; Hit.BaseDamage=.01f; Hit.bCanCritical=false; Hit.bCanBeAvoided=false; Hit.SetInstigator(Enemy);
        Player->GetCombat()->ReceiveDamage(Hit);
        // Invoke the real character proximity scan; do not hand-author its bool.
        Player->Tick(.3f);
    };
    Contact();
    for (int32 I=0; I<70; ++I) { Contact(); Grit->AdvanceLoop(1); }
    TestTrue(TEXT("Purchased Afterimage survives native resource polling"),Progression->HasNodeTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Afterimage"))));
    TestEqual(TEXT("Actual nearby enemy funds the ordinary loop"),Grit->GetGrit(),100.0f);
    const float BaseDamage=F.Shot(); F.Tick(.3f);
    const auto Handle=Player->GetAbilitySystemComponent()->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Provoke::StaticClass(),1));
    const auto* Defaults=GetDefault<UBreakerAbility_Provoke>();
    if (!TestTrue(TEXT("Paid Provoke"),Player->GetAbilitySystemComponent()->TryActivateAbility(Handle))) return false;
    TestTrue(TEXT("Provoke spent funded Grit"),Grit->GetGrit()<100);
    const float Full=F.Shot(); TestTrue(TEXT("Actual Provoke flat rider increases rifle hit"),Full>BaseDamage);
    auto Income=[&]() { Contact(); const float Before=Grit->GetGrit(); Grit->AdvanceLoop(.1f); return Grit->GetGrit()-Before; };
    TestEqual(TEXT("Full rank-two proximity income"),Income(),Grit->ProximityRate*2*.1f,.002f);
    F.Tick(Defaults->BonusDurationSeconds+.03f);
    if (!TestFalse(TEXT("Player survives until the numerical tail is measured"),Player->GetCombat()->IsDead())) return false;
    TestTrue(TEXT("Observation enemy stays at its actual proximity site"),Enemy->GetActorLocation().Equals(FVector(0,300,0),.01f));
    TestEqual(TEXT("Flat damage tail halves captured contribution"),F.Shot(),BaseDamage+(Full-BaseDamage)*.5f,.03f);
    TestEqual(TEXT("Tail rank-two proximity income"),Income(),Grit->ProximityRate*1.5f*.1f,.002f);
    Enemy->SetActorLocation(FVector(0,2000,0)); Contact(); const float BeforeAbsent=Grit->GetGrit(); Grit->AdvanceLoop(.1f);
    TestEqual(TEXT("Tail cannot generate without a nearby enemy"),Grit->GetGrit(),BeforeAbsent,.002f);
    Enemy->SetActorLocation(FVector(0,300,0));
    if (!Progression->RespecCore(Reason)) return false;
    TestEqual(TEXT("Respec revokes proximity tail"),Grit->GetProximityRateMultiplier(),1.0f,.001f);
    const auto* CoreTree=Progression->ClassDefinition->BranchTrees.Last().Get();
    if (!Progression->PurchaseNode(CoreTree,TEXT("Test.Afterimage.Delivery.Rule"),Reason)) return false;
    TestEqual(TEXT("Rebuy does not resurrect old proximity tail"),Grit->GetProximityRateMultiplier(),1.0f,.001f);
    F.Tick(2.05f);
    TestEqual(TEXT("Expired proximity returns to ordinary income"),Income(),Grit->ProximityRate*.1f,.002f);
    F.Tick(20);
    for (int32 I=0; I<70; ++I) { Contact(); Grit->AdvanceLoop(1); }
    if (!TestTrue(TEXT("Second ordinarily funded Provoke"),Player->GetAbilitySystemComponent()->TryActivateAbility(Handle))) return false;
    TestEqual(TEXT("Fresh paid window after rebuy"),Grit->GetProximityRateMultiplier(),2.0f,.001f);
    if (!TestFalse(TEXT("Death cleanup test starts with a living player"),Player->GetCombat()->IsDead())) return false;
    FBreakerDamageRequest Lethal; Lethal.BaseDamage=100000; Lethal.bCanCritical=false; Lethal.bCanBeAvoided=false;
    Lethal.DamageFamily=EBreakerDamageFamily::TrueDamage; Lethal.SetInstigator(Enemy);
    Player->GetCombat()->ReceiveDamage(Lethal);
    TestTrue(TEXT("Actual death occurs"),Player->GetCombat()->IsDead()); Player->GetCombat()->RestoreVitals();
    TestEqual(TEXT("Death and revival cancel proximity window and tail"),Grit->GetProximityRateMultiplier(),1.0f,.001f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerAfterimageCadenceTest,"RiorsEdge.Abilities.AfterimageCadenceStackDelivery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerAfterimageCadenceTest::RunTest(const FString&)
{
    FBreakerAfterimageDeliveryFixture F(EBreakerClassId::Swift); if (!F.Player) return false;
    auto* Player=F.Player; auto* Move=Player->GetBreakerMovement(); auto* Momentum=Player->GetMomentum();
    Move->Velocity=FVector(Move->WalkSpeed,0,0);
    for (int32 I=0; I<40; ++I) { Player->SetActorLocation(Player->GetActorLocation()+Move->Velocity); Momentum->AdvanceLoop(1); }
    Move->StopMovementImmediately(); F.AimTarget();
    const float BaseDamage=F.Shot(); F.Tick(.3f);
    const auto Handle=Player->GetAbilitySystemComponent()->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_CadenceBreak::StaticClass(),1));
    const float Before=Momentum->GetMomentum();
    if (!TestTrue(TEXT("Actual paid Cadence Break"),Player->GetAbilitySystemComponent()->TryActivateAbility(Handle))) return false;
    TestTrue(TEXT("Cadence spends normally earned Momentum"),Momentum->GetMomentum()<Before);
    const float First=F.Shot(); F.Tick(.3f); const float Second=F.Shot();
    TestEqual(TEXT("First accepted shot earns rather than pre-applies stack"),First,BaseDamage,.02f);
    TestTrue(TEXT("Second accepted shot receives earned flat stack"),Second>First);
    // Two shots have now earned two stacks; the expiry snapshot must keep
    // those two at half, exactly the one full stack delivered on shot two.
    auto* State=UBreakerAbilityStateComponent::FindOrAdd(Player);
    F.Tick(State->GetWindowRemaining(UBreakerAbility_CadenceBreak::WindowKey())+.03f);
    TestFalse(TEXT("Stack acquisition window is closed"),State->IsWindowActive(UBreakerAbility_CadenceBreak::WindowKey()));
    TestEqual(TEXT("Tail pays half final two-stack contribution"),F.Shot(),Second,.03f);
    F.Tick(.3f); TestEqual(TEXT("Tail hits cannot acquire more stacks"),F.Shot(),Second,.03f);
    F.Tick(2.05f); TestEqual(TEXT("Stack tail expires"),F.Shot(),BaseDamage,.03f);
    return true;
}
#endif
