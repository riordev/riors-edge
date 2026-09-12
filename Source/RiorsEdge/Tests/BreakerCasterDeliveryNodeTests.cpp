#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbility_Closequarter.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerManaComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerStatusComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace BreakerCasterDeliveryTest
{
    ABreakerCharacter* Caster(UWorld* World)
    {
        ABreakerCharacter* Player = World->SpawnActor<ABreakerCharacter>();
        Player->GetBreakerMovement()->SetComponentTickEnabled(false);
        Player->GetAbilitySystemComponent()->InitAbilityActorInfo(Player, Player);
        Player->GetAbilitySystemComponent()->AddAttributeSetSubobject(Player->GetAttributes());
        Player->GetCombat()->BindAttributes(Player->GetAttributes());
        Player->GetProgression()->BindAttributes(Player->GetAttributes());
        FBreakerProgressionState State;
        State.PermanentClass = EBreakerClassId::Caster;
        State.UnspentDoctrinePoints = 4; // Future benchmark wiring fixture; shipped campaign still grants two.
        State.UnspentCorePoints = 5;
        Player->GetProgression()->LoadProgressionState(State);
        Player->GetMana()->BindAttributes(Player->GetAttributes());
        Player->GetMana()->PassiveRegenPerSecond = 0;
        Player->GetAttributes()->ApplyClassResource(0);
        return Player;
    }
    AActor* Target(UWorld* World, FVector Location)
    {
        AActor* Actor = World->SpawnActor<AActor>();
        USphereComponent* Body = NewObject<USphereComponent>(Actor);
        Actor->AddInstanceComponent(Body); Actor->SetRootComponent(Body);
        Body->SetSphereRadius(40); Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Body->SetCollisionResponseToAllChannels(ECR_Ignore); Body->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);
        Body->RegisterComponent(); Actor->SetActorLocation(Location);
        UBreakerCombatComponent* Combat = NewObject<UBreakerCombatComponent>(Actor);
        Actor->AddInstanceComponent(Combat); Combat->RegisterComponent();
        UBreakerAttributeSet* Attr = NewObject<UBreakerAttributeSet>(Actor);
        Attr->ApplyMaxHealth(10000); Attr->ApplyHealth(10000); Combat->BindAttributes(Attr);
        UBreakerStatusComponent* Status = NewObject<UBreakerStatusComponent>(Actor);
        Actor->AddInstanceComponent(Status); Status->RegisterComponent();
        return Actor;
    }
    FBreakerStatusApplicationSpec Status(const TCHAR* Tag)
    {
        FBreakerStatusApplicationSpec Spec;
        Spec.StatusTag = FGameplayTag::RequestGameplayTag(Tag);
        Spec.BaseDamagePerTick = 1; Spec.Duration = 10; Spec.TickInterval = 1;
        return Spec;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerChainDeliveryRuntimeTest, "RiorsEdge.Status.ChainPurchasedRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerChainDeliveryRuntimeTest::RunTest(const FString& Parameters)
{
    using namespace BreakerCasterDeliveryTest;
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("Isolated world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    ABreakerCharacter* Player = Caster(World);
    const UBreakerProgressionTree* Tree = UBreakerProgressionLibrary::GetCasterMultispellTree();
    FText Error;
    for (const TCHAR* Entry : { TEXT("Core.Affliction.OpenWound") })
        if (!TestTrue(TEXT("Actual Core adjacency path to Linger"), Player->GetProgression()->PurchaseNode(
            UBreakerProgressionLibrary::GetCoreSliceTree(), Entry, Error))) return false;
    if (!TestTrue(TEXT("Actual Linger purchase supplies nonunit duration scaling"), Player->GetProgression()->PurchaseNode(
        UBreakerProgressionLibrary::GetCoreSliceTree(), TEXT("Core.Affliction.Linger"), Error))) return false;
    // O272: Chain is a travel node with no prerequisite; it is bought alone below.
    AActor* First = Target(World, FVector(500, 0, 0));
    AActor* Second = Target(World, FVector(500, 300, 0));
    AActor* Third = Target(World, FVector(500, 500, 0));
    ABreakerCharacter* Teammate = World->SpawnActor<ABreakerCharacter>(ABreakerCharacter::StaticClass(), FVector(600, 0, 0), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Nearer actual player teammate"), Teammate)) return false;
    UBreakerStatusComponent* TeammateStatus = NewObject<UBreakerStatusComponent>(Teammate);
    Teammate->AddInstanceComponent(TeammateStatus);
    TeammateStatus->RegisterComponent();
    auto* A = First->FindComponentByClass<UBreakerStatusComponent>();
    auto* B = Second->FindComponentByClass<UBreakerStatusComponent>();
    auto* C = Third->FindComponentByClass<UBreakerStatusComponent>();
    const FBreakerStatusApplicationSpec Poison = Status(TEXT("Status.Poison"));
    const FBreakerStatusApplicationSpec Bleed = Status(TEXT("Status.Bleed"));
    A->ApplyStatus(Poison, EBreakerDamageFamily::Physical, Player);
    A->ApplyStatus(Bleed, EBreakerDamageFamily::Physical, Player);
    TestTrue(TEXT("Without purchase two types do not spread"), B->GetActiveStatuses().IsEmpty());
    if (!TestTrue(TEXT("Actual Chain purchase"), Player->GetProgression()->PurchaseNode(Tree, TEXT("Caster.Multispell.Chain"), Error))) return false;
    A->ConsumeAllStatuses();
    A->ApplyStatus(Poison, EBreakerDamageFamily::Physical, Player);
    TestTrue(TEXT("One distinct type cannot spread"), B->GetActiveStatuses().IsEmpty());
    B->ApplyStatus(Poison, EBreakerDamageFamily::Physical, Player);
    A->ApplyStatus(Bleed, EBreakerDamageFamily::Physical, Player);
    TestTrue(TEXT("Newest accepted type reaches nearest target"), B->HasStatus(Bleed.StatusTag));
    TestFalse(TEXT("A nearer player teammate cannot receive hostile spread"), TeammateStatus->HasStatus(Bleed.StatusTag));
    TestFalse(TEXT("Copy does not recursively spread despite recipient having two types"), C->HasStatus(Bleed.StatusTag));
    const FBreakerActiveStatus* Copy = B->GetActiveStatuses().FindByPredicate([&](const FBreakerActiveStatus& Active) { return Active.Spec.StatusTag == Bleed.StatusTag; });
    if (!TestNotNull(TEXT("Actual spread payload"), Copy)) return false;
    TestEqual(TEXT("Spread has zero proc coefficient"), Copy->Spec.ProcCoefficient, 0.0f);
    TestEqual(TEXT("Spread duration is not scaled twice"), Copy->RemainingDuration, A->GetActiveStatuses().Last().RemainingDuration);
    TestTrue(TEXT("Duration fixture actually has scaling"), Copy->RemainingDuration > Bleed.Duration);
    B->ConsumeAllStatuses(); C->ConsumeAllStatuses();
    Second->SetActorLocation(FVector(500, 750, 0)); Third->SetActorLocation(FVector(500, 1500, 0));
    A->ApplyStatus(Bleed, EBreakerDamageFamily::Physical, Player);
    // O272: Chain has one rank and its reach is 900 cm from that purchase.
    // The former "rank one cannot reach 7.5 m, rank two can" pair is gone
    // with the second rank.
    TestTrue(TEXT("Single-rank Chain reaches 7.5 m"), B->HasStatus(Bleed.StatusTag));
    B->ConsumeAllStatuses();
    AActor* Wall = Target(World, FVector(500, 350, 0));
    Wall->FindComponentByClass<UBreakerStatusComponent>()->DestroyComponent();
    Wall->FindComponentByClass<UBreakerCombatComponent>()->DestroyComponent();
    A->ApplyStatus(Bleed, EBreakerDamageFamily::Physical, Player);
    TestFalse(TEXT("Wall blocks an otherwise in-range spread"), B->HasStatus(Bleed.StatusTag));
    Wall->Destroy();
    FBreakerDamageRequest Kill;
    Kill.BaseDamage = 20000; Kill.bCanCritical = false;
    Second->FindComponentByClass<UBreakerCombatComponent>()->ReceiveDamage(Kill);
    A->ApplyStatus(Bleed, EBreakerDamageFamily::Physical, Player);
    TestFalse(TEXT("Dead target never receives spread"), B->HasStatus(Bleed.StatusTag));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerMomentumTransferRuntimeTest, "RiorsEdge.Combat.MomentumTransferPurchasedRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerMomentumTransferRuntimeTest::RunTest(const FString& Parameters)
{
    using namespace BreakerCasterDeliveryTest;
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("Isolated world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    ABreakerCharacter* Player = Caster(World);
    AActor* Victim = Target(World, FVector(500, 0, 0));
    UBreakerCombatComponent* Combat = Victim->FindComponentByClass<UBreakerCombatComponent>();
    Combat->DodgeChance = 1; Combat->BlockChance = 1;
    const UBreakerProgressionTree* Tree = UBreakerProgressionLibrary::GetCasterSpellbladeTree();
    FText Error;
    // O272: Momentum Transfer is a travel node with no prerequisite and one
    // rank; its 3 s window is what the single purchase grants.
    if (!TestTrue(TEXT("Actual Momentum Transfer purchase"), Player->GetProgression()->PurchaseNode(Tree, TEXT("Caster.Spellblade.MomentumTransfer"), Error))) return false;
    UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
    const FGameplayAbilitySpecHandle Ability = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Closequarter::StaticClass(), 1));
    // This fixture tests arrival and defense suppression, so fund the real
    // authored cast rather than attempting it from an empty Mana bank.
    Player->GetAttributes()->ApplyClassResource(100);
    TestTrue(TEXT("Actual Closequarter GAS activation"), ASC->TryActivateAbility(Ability));
    TestTrue(TEXT("Actual swept arrival reaches standoff"), Player->GetActorLocation().Equals(FVector(300, 0, 0), 5));
    FBreakerDamageRequest Hit;
    Hit.BaseDamage = 10; Hit.bCanCritical = false; Hit.SetInstigator(Player);
    Hit.SourceTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Damage.Melee")));
    FBreakerDamageRequest Other = Hit; Other.SetInstigator(World->SpawnActor<AActor>());
    TestTrue(TEXT("Another attacker cannot spend suppression"), Combat->ReceiveDamage(Other).bDodged);
    FBreakerDamageRequest Copy = Hit; Copy.ProcCoefficient = 0;
    TestTrue(TEXT("Zero proc cannot spend suppression"), Combat->ReceiveDamage(Copy).bDodged);
    FBreakerDamageRequest Tick = Hit; Tick.bIsDamageOverTime = true;
    Combat->ReceiveDamage(Tick);
    const FBreakerDamageResult Paid = Combat->ReceiveDamage(Hit);
    TestFalse(TEXT("Matching next melee bypasses dodge"), Paid.bDodged);
    TestFalse(TEXT("Matching next melee bypasses block"), Paid.bBlocked);
    TestTrue(TEXT("Actual combat damage pays"), Paid.HealthDamage > 0);
    TestTrue(TEXT("Suppression is spent exactly once"), Combat->ReceiveDamage(Hit).bDodged);
    TestEqual(TEXT("Passive dodge property never changes"), Combat->DodgeChance, 1.0f);
    TestEqual(TEXT("Passive block property never changes"), Combat->BlockChance, 1.0f);
    Combat->ArmMeleeDefenseSuppression(Player, 0.1f);
    World->Tick(LEVELTICK_All, 0.2f);
    TestTrue(TEXT("Expired window cannot suppress"), Combat->ReceiveDamage(Hit).bDodged);
    return true;
}
#endif
