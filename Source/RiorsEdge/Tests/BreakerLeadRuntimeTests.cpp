#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbility_Lead.h"
#include "Abilities/BreakerAbilityTags.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Characters/BreakerCharacter.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Classes/BreakerMomentumComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Weapons/BreakerWeaponComponent.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerLeadTwoTargetsRuntimeTest, "RiorsEdge.Abilities.LeadTwoTargetsRuntime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerLeadTwoTargetsRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("Transient world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    ABreakerCharacter* Player = World->SpawnActor<ABreakerCharacter>();
    APlayerController* Controller = World->SpawnActor<APlayerController>();
    if (!TestNotNull(TEXT("Player"), Player) || !TestNotNull(TEXT("Controller"), Controller)) return false;
    Controller->Possess(Player);
    UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Player->GetAttributes());
    Player->GetCombat()->BindAttributes(Player->GetAttributes());
    Player->GetProgression()->BindAttributes(Player->GetAttributes());
    FBreakerProgressionState Setup;
    Setup.PermanentClass = EBreakerClassId::Swift; Setup.UnspentDoctrinePoints = 8;
    Player->GetProgression()->LoadProgressionState(Setup); // Isolated purchased-effect fixture; no owner saves.
    ASC->SetNumericAttributeBase(UBreakerAttributeSet::GetCriticalChanceAttribute(), 0.0f);
    TArray<AActor*> Targets;
    for (int32 Index = 0; Index < 3; ++Index)
    {
        AActor* Target = World->SpawnActor<AActor>();
        USphereComponent* Body = NewObject<USphereComponent>(Target);
        Target->AddInstanceComponent(Body); Target->SetRootComponent(Body);
        Body->SetSphereRadius(200); Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Body->SetCollisionResponseToAllChannels(ECR_Ignore);
        // Shipped enemy hitboxes ignore Visibility and block the weapon channel.
        Body->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);
        Body->RegisterComponent(); Target->SetActorLocation(FVector(3500, Index * 1000, 64));
        UBreakerCombatComponent* Combat = NewObject<UBreakerCombatComponent>(Target);
        Target->AddInstanceComponent(Combat); Combat->RegisterComponent();
        UBreakerAttributeSet* Health = NewObject<UBreakerAttributeSet>(Target);
        Health->ApplyMaxHealth(10000); Health->ApplyHealth(10000); Combat->BindAttributes(Health);
        Targets.Add(Target);
    }
    UBreakerAbilityStateComponent* State = UBreakerAbilityStateComponent::FindOrAdd(Player);
    if (!TestNotNull(TEXT("Ability state"), State)) return false;
    const FGameplayAbilitySpecHandle Lead = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Lead::StaticClass(), 1));
    auto Aim = [&](AActor* Target) { FVector Eye; FRotator Rotation; Controller->GetPlayerViewPoint(Eye, Rotation); Controller->SetControlRotation((Target->GetActorLocation() - Eye).Rotation()); };
    auto Mark = [&](AActor* Target)
    {
        Aim(Target); Player->GetAttributes()->ApplyClassResource(100);
        FGameplayTagContainer Cooldown; Cooldown.AddTag(BreakerAbilityTags::Cooldown_Class_Swift_Lead.GetTag());
        ASC->RemoveActiveEffectsWithGrantedTags(Cooldown);
        return TestTrue(TEXT("Actual Lead GAS activation"), ASC->TryActivateAbility(Lead));
    };
    if (!Mark(Targets[0]) || !Mark(Targets[1])) return false;
    TestEqual(TEXT("Without node only latest mark survives"), State->GetMarkedTargets().Num(), 1);
    TestFalse(TEXT("Baseline replaces first target"), State->IsMarked(Targets[0]));
    const UBreakerProgressionTree* Tree = UBreakerProgressionLibrary::GetSwiftMarksmanTree();
    for (const TCHAR* Id : { TEXT("Swift.Marksman.Steady"), TEXT("Swift.Marksman.Steady"), TEXT("Swift.Marksman.Ledger"), TEXT("Swift.Marksman.MarkEconomy"), TEXT("Swift.Marksman.Lead") })
    { FText Reason; if (!TestTrue(Id, Player->GetProgression()->PurchaseNode(Tree, Id, Reason))) return false; }
    if (!Mark(Targets[0]) || !Mark(Targets[1])) return false;
    TestEqual(TEXT("Purchased Lead retains two independent targets"), State->GetMarkedTargets().Num(), 2);
    UBreakerMomentumComponent* Momentum = Player->FindComponentByClass<UBreakerMomentumComponent>();
    if (!TestNotNull(TEXT("Momentum loop"), Momentum)) return false;
    Momentum->BindAttributes(Player->GetAttributes());
    int32 PullIndex = 0;
    for (AActor* Target : { Targets[0], Targets[1], Targets[0], Targets[1] })
    {
        Aim(Target); World->Tick(LEVELTICK_All, 0.2f);
        Player->GetAttributes()->ApplyClassResource(20);
        Player->GetWeapon()->ResetAmmunition(); Player->GetWeapon()->StartFire(); Player->GetWeapon()->StopFire();
        const FBreakerShotResult& Shot = Player->GetWeapon()->GetLastShot();
        TestTrue(TEXT("Actual weapon hit both marked targets"), Shot.bFired && Shot.HitActor == Target && Shot.DamageResult.HealthDamage > 0);
        TestTrue(TEXT("Both marks grant range-qualified weak points"), Shot.bWeakPoint);
        TestEqual(TEXT("Actual weapon refunds once per independent cast"), Player->GetAttributes()->GetClassResource(), PullIndex < 2 ? 30.0f : 20.0f);
        ++PullIndex;
    }
    if (!Mark(Targets[2])) return false;
    TestFalse(TEXT("Third cast evicts oldest target"), State->IsMarked(Targets[0]));
    TestTrue(TEXT("Third cast preserves the other mark"), State->IsMarked(Targets[1]));
    TestTrue(TEXT("First refund belongs to this mark"), State->ConsumeMarkRefund(Targets[2]));
    TestFalse(TEXT("Alternating targets cannot replay a refund"), State->ConsumeMarkRefund(Targets[2]));
    if (!Mark(Targets[2])) return false;
    TestEqual(TEXT("Refreshing same target does not duplicate it"), State->GetMarkedTargets().Num(), 2);
    TestTrue(TEXT("A new cast resets only its own refund"), State->ConsumeMarkRefund(Targets[2]));
    const float RemainingBeforeTransfer = State->GetMarkRemainingFor(Targets[2]);
    State->TransferMark(Targets[2], Targets[0]);
    TestEqual(TEXT("Transfer preserves both marks"), State->GetMarkedTargets().Num(), 2);
    TestEqual(TEXT("Transfer preserves remaining lifetime"), State->GetMarkRemainingFor(Targets[0]), RemainingBeforeTransfer);
    TestFalse(TEXT("Transfer cannot replay its consumed refund"), State->ConsumeMarkRefund(Targets[0]));
    FBreakerDamageRequest Kill; Kill.BaseDamage = 100000; Kill.bCanCritical = false; Kill.bBypassShield = true;
    Targets[1]->FindComponentByClass<UBreakerCombatComponent>()->ReceiveDamage(Kill);
    TestEqual(TEXT("Dead targets disappear from visible marks"), State->GetMarkedTargets().Num(), 1);
    State->AdvanceTime(100);
    TestEqual(TEXT("All marks expire independently of a recast"), State->GetMarkedTargets().Num(), 0);
    State->SetMark(Targets[0], 1); State->AddMark(Targets[2], 3, 2); State->AdvanceTime(1.5f);
    TestFalse(TEXT("Short mark expired"), State->IsMarked(Targets[0]));
    TestTrue(TEXT("Longer mark still exists"), State->IsMarked(Targets[2]));
    State->SetMark(Targets[0], 2);
    TestEqual(TEXT("Legacy setter still replaces every mark"), State->GetMarkedTargets().Num(), 1);
    State->ClearMark();
    AActor* Scenery = World->SpawnActor<AActor>();
    USphereComponent* SceneryBody = NewObject<USphereComponent>(Scenery);
    Scenery->AddInstanceComponent(SceneryBody); Scenery->SetRootComponent(SceneryBody);
    SceneryBody->SetSphereRadius(200); SceneryBody->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    SceneryBody->SetCollisionResponseToAllChannels(ECR_Block); SceneryBody->RegisterComponent();
    Scenery->SetActorLocation(FVector(3500, -1500, 64));
    if (!Mark(Scenery)) return false;
    TestEqual(TEXT("A cast on blocking scenery does not mark it"), State->GetMarkedTargets().Num(), 0);
    if (!Mark(Targets[1])) return false;
    TestEqual(TEXT("A cast on a dead combat target does not mark it"), State->GetMarkedTargets().Num(), 0);
    ABreakerEnemy* ActualEnemy = World->SpawnActor<ABreakerEnemy>(FVector(3500, -3000, 100), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Shipped enemy fixture"), ActualEnemy)) return false;
    UBreakerAttributeSet* EnemyAttributes = Cast<UBreakerAttributeSet>(ActualEnemy->GetDefaultSubobjectByName(TEXT("Attributes")));
    if (!TestNotNull(TEXT("Shipped enemy attribute subobject"), EnemyAttributes)) return false;
    ActualEnemy->GetAbilitySystemComponent()->AddAttributeSetSubobject(EnemyAttributes);
    ActualEnemy->DispatchBeginPlay();
    if (!Mark(ActualEnemy)) return false;
    TestTrue(TEXT("Actual Lead trace marks the shipped enemy collision geometry"), State->IsMarked(ActualEnemy));
    return true;
}
#endif
