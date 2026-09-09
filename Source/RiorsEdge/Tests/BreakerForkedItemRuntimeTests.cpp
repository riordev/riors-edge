#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerStatusComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerItemRules.h"
#include "Items/BreakerLootLibrary.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponDefinition.h"
#include "Movement/BreakerCharacterMovementComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace BreakerForkedItemTest
{
    struct FVictim
    {
        AActor* Actor;
        UBreakerAttributeSet* Attributes;
        UBreakerCombatComponent* Combat;
        UBreakerStatusComponent* Status;
    };
    FVictim MakeVictim(UWorld* World, FVector Location)
    {
        AActor* Actor = World->SpawnActor<AActor>();
        USphereComponent* Body = NewObject<USphereComponent>(Actor);
        Actor->AddInstanceComponent(Body);
        Actor->SetRootComponent(Body);
        Body->SetSphereRadius(55);
        Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Body->SetCollisionResponseToAllChannels(ECR_Ignore);
        Body->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);
        Body->RegisterComponent();
        Actor->SetActorLocation(Location);
        UBreakerAttributeSet* Attributes = NewObject<UBreakerAttributeSet>(Actor);
        Attributes->ApplyMaxHealth(10000);
        Attributes->ApplyHealth(10000);
        UBreakerCombatComponent* Combat = NewObject<UBreakerCombatComponent>(Actor);
        Actor->AddInstanceComponent(Combat);
        Combat->RegisterComponent();
        Combat->BindAttributes(Attributes);
        UBreakerStatusComponent* Status = NewObject<UBreakerStatusComponent>(Actor);
        Actor->AddInstanceComponent(Status);
        Status->RegisterComponent();
        return {Actor, Attributes, Combat, Status};
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerRefractorRollTest,
    "RiorsEdge.Items.Refractor.RollAndSeed", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerRefractorRollTest::RunTest(const FString& Parameters)
{
    bool bReachedFromOrdinaryLoot = false;
    for (int32 Seed = 0; Seed < 512; ++Seed)
    {
        const FBreakerItemInstance Item = UBreakerLootLibrary::RollItem(TEXT("Test.Necklace"), EBreakerEquipSlot::Necklace,
            EBreakerItemRarity::Unwritten, 40, Seed);
        if (Item.LegendaryId == TEXT("Legendary.Refractor"))
        {
            bReachedFromOrdinaryLoot = true;
            TestTrue(TEXT("Real loot retains the fork rule"), Item.Rule == EBreakerItemRule::Refractor);
            break;
        }
    }
    TestTrue(TEXT("Named exotic is reachable from the production loot roll"), bReachedFromOrdinaryLoot);
    int32 Paid = 0;
    for (int32 Seed = 0; Seed < 128; ++Seed)
    {
        const bool First = UBreakerItemRuleLibrary::RollCriticalFork(0.5f, Seed);
        TestEqual(TEXT("Seeded replacement roll repeats"), First, UBreakerItemRuleLibrary::RollCriticalFork(0.5f, Seed));
        Paid += First ? 1 : 0;
        TestFalse(TEXT("Zero critical chance never forks"), UBreakerItemRuleLibrary::RollCriticalFork(0, Seed));
        TestTrue(TEXT("Certain critical chance always forks"), UBreakerItemRuleLibrary::RollCriticalFork(1, Seed));
    }
    TestTrue(TEXT("Intermediate chance has both successes and misses"), Paid > 0 && Paid < 128);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerRefractorDamageTest,
    "RiorsEdge.Items.Refractor.HitscanRuntime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerRefractorDamageTest::RunTest(const FString& Parameters)
{
    using namespace BreakerForkedItemTest;
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("Isolated world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    ABreakerCharacter* Player = World->SpawnActor<ABreakerCharacter>();
    if (!TestNotNull(TEXT("Real player"), Player)) return false;
    // No Character BeginPlay: no save loading or persistence listeners.
    Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    Player->GetAbilitySystemComponent()->InitAbilityActorInfo(Player, Player);
    Player->GetAbilitySystemComponent()->AddAttributeSetSubobject(Player->GetAttributes());
    Player->GetCombat()->BindAttributes(Player->GetAttributes());
    Player->GetEquipment()->BindAttributes(Player->GetAttributes());
    // Low-level explicit legendary is a purchased-item wiring fixture only;
    // the separate ordinary-roll test exercises the real level-40 drop gate.
    const FBreakerItemInstance Item = UBreakerLootLibrary::RollLegendary(TEXT("Legendary.Refractor"), 1, 4242);
    TestTrue(TEXT("Real rolled necklace equips"), Player->GetEquipment()->EquipItem(Item));
    TestTrue(TEXT("Equipped rule has a live nonidentity result"), UBreakerItemRuleLibrary::ResolveRules(Player->GetEquipment()->GetEquipped()).bHitscanCriticalForks);
    FVector Eye;
    FRotator Rotation;
    Player->GetActorEyesViewPoint(Eye, Rotation);
    FVictim Primary = MakeVictim(World, Eye + FVector(500, 0, 0));
    FVictim Left = MakeVictim(World, Eye + FVector(500, -230, 0));
    FVictim Right = MakeVictim(World, Eye + FVector(500, 230, 0));
    FVictim Third = MakeVictim(World, Eye + FVector(500, 450, 0));
    UBreakerWeaponComponent* Weapon = Player->GetWeapon();
    Weapon->EquipArchetype(EBreakerWeaponArchetype::Rifle);
    Weapon->WeaponDefinition = DuplicateObject<UBreakerWeaponDefinition>(Weapon->GetActiveDefinition(), Weapon);
    Weapon->WeaponDefinition->HipSpreadDegrees = 0;
    Weapon->WeaponDefinition->AimSpreadDegrees = 0;
    Weapon->WeaponDefinition->BleedChance = 1;
    Weapon->WeaponDefinition->BleedDamagePerTick = 1;
    Weapon->WeaponDefinition->BleedDuration = 5;
    auto Fire = [&]()
    {
        Player->GetAttributes()->InitCriticalChance(1);
        Weapon->ResetAmmunition();
        Weapon->StartFire();
        Weapon->StopFire();
    };
    Fire();
    const float PrimaryDamage = 10000 - Primary.Attributes->GetHealth();
    TestTrue(TEXT("Actual traced primary receives damage"), PrimaryDamage > 0);
    TestFalse(TEXT("Equipped hitscan forfeits critical damage"), Weapon->GetLastShot().DamageResult.bCritical);
    TestEqual(TEXT("Exactly two visible fork beams, no recursion"), Weapon->GetLastShot().SecondaryImpacts.Num(), 2);
    TestTrue(TEXT("Left fork delivers sixty percent through combat"), FMath::IsNearlyEqual(10000 - Left.Attributes->GetHealth(), PrimaryDamage * 0.6f, 0.01f));
    TestTrue(TEXT("Right fork delivers sixty percent through combat"), FMath::IsNearlyEqual(10000 - Right.Attributes->GetHealth(), PrimaryDamage * 0.6f, 0.01f));
    TestEqual(TEXT("Third body cannot receive a recursive fork"), Third.Attributes->GetHealth(), 10000.0f);
    TestFalse(TEXT("Certain weapon bleed actually applies on the primary"), Primary.Status->GetActiveStatuses().IsEmpty());
    TestTrue(TEXT("Forks do not apply even a certain weapon status"), Left.Status->GetActiveStatuses().IsEmpty() && Right.Status->GetActiveStatuses().IsEmpty());
    TestTrue(TEXT("Real unequip succeeds"), Player->GetEquipment()->UnequipSlot(EBreakerEquipSlot::Necklace));
    TestFalse(TEXT("Unequip removes the delivery rewrite"), UBreakerItemRuleLibrary::ResolveRules(Player->GetEquipment()->GetEquipped()).bHitscanCriticalForks);
    World->Tick(LEVELTICK_All, 0.5f);
    Fire();
    TestTrue(TEXT("Unequipped hitscan critical damage returns"), Weapon->GetLastShot().DamageResult.bCritical);
    TestEqual(TEXT("Unequipped shot has no forks"), Weapon->GetLastShot().SecondaryImpacts.Num(), 0);
    TestTrue(TEXT("Reequip succeeds"), Player->GetEquipment()->EquipItem(Item));
    Primary.Combat->DodgeChance = 1;
    World->Tick(LEVELTICK_All, 0.5f);
    Fire();
    TestEqual(TEXT("Dodged primary cannot generate forks"), Weapon->GetLastShot().SecondaryImpacts.Num(), 0);
    Primary.Combat->DodgeChance = 0;
    // The previously close bodies leave the radius; a dead nearer body and a
    // solid obstruction must not steal a fork or allow one through the wall.
    Left.Actor->SetActorLocation(Eye + FVector(500, -1500, 0));
    Right.Actor->SetActorLocation(Eye + FVector(500, 1500, 0));
    Third.Actor->SetActorLocation(Eye + FVector(500, 1600, 0));
    FVictim Dead = MakeVictim(World, Eye + FVector(500, -120, 0));
    FBreakerDamageRequest Kill;
    Kill.BaseDamage = 20000;
    Kill.bCanCritical = false;
    Dead.Combat->ReceiveDamage(Kill);
    TestTrue(TEXT("Excluded fixture is actually dead"), Dead.Combat->IsDead());
    FVictim Blocked = MakeVictim(World, Eye + FVector(500, 350, 0));
    AActor* Wall = World->SpawnActor<AActor>();
    USphereComponent* WallBody = NewObject<USphereComponent>(Wall);
    Wall->AddInstanceComponent(WallBody);
    Wall->SetRootComponent(WallBody);
    WallBody->SetSphereRadius(90);
    WallBody->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    WallBody->SetCollisionResponseToAllChannels(ECR_Ignore);
    WallBody->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);
    WallBody->RegisterComponent();
    Wall->SetActorLocation(Eye + FVector(470, 170, 0));
    World->Tick(LEVELTICK_All, 0.5f);
    Fire();
    TestEqual(TEXT("No forks to dead, out-of-range or occluded actors"), Weapon->GetLastShot().SecondaryImpacts.Num(), 0);
    TestEqual(TEXT("Occluded target stays unharmed"), Blocked.Attributes->GetHealth(), 10000.0f);
    Wall->Destroy();
    Primary.Actor->SetActorLocation(Eye + FVector(3000, 0, 0));
    CastChecked<USphereComponent>(Primary.Actor->GetRootComponent())->SetSphereRadius(200);
    Left.Actor->SetActorLocation(Eye + FVector(3000, -350, 0));
    Right.Actor->SetActorLocation(Eye + FVector(3000, 350, 0));
    World->Tick(LEVELTICK_All, 1.0f);
    UBreakerAbilityStateComponent* State = UBreakerAbilityStateComponent::FindOrAdd(Player);
    if (!TestNotNull(TEXT("Real mark state"), State)) return false;
    State->SetMark(Primary.Actor, 10);
    Fire();
    TestTrue(TEXT("Production Lead range grants a weak point"), Weapon->GetLastShot().bWeakPoint);
    TestEqual(TEXT("O104 granted weak point cannot also pay a critical fork"), Weapon->GetLastShot().SecondaryImpacts.Num(), 0);
    return true;
}
#endif
