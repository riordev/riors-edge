#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponDefinition.h"
#include "Weapons/BreakerRocketProjectile.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerLiteralWeaponRuntimeTest,
    "RiorsEdge.Weapons.LiteralAddedDamageRuntime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerLiteralWeaponRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const uint64 Frame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = Frame; };
    auto* Player = World->SpawnActor<ABreakerCharacter>();
    if (!Player) return false;
    Player->SetActorTickEnabled(false);
    Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    auto* ASC = Player->GetAbilitySystemComponent();
    auto* Attributes = Player->GetAttributes();
    ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Attributes);
    Attributes->SetCriticalChance(0);
    Player->GetCombat()->BindAttributes(Attributes);
    auto* Progression = Player->GetProgression(); Progression->BindAttributes(Attributes);
    // Authored schema fixture until the new Core roster is activated. The
    // actual progression fold discovers this tree through the class asset.
    auto* Tree = NewObject<UBreakerProgressionTree>();
    Tree->TreeId = TEXT("Test.LiteralWeapon"); Tree->Currency = EBreakerPointCurrency::CorePoints;
    auto* Node = NewObject<UBreakerProgressionNode>(Tree);
    Node->NodeId = TEXT("Test.LiteralWeapon.Added"); Node->Currency = Tree->Currency;
    Node->MaxRank = 3; Node->CostPerRank = 1;
    FBreakerNodeEffect Effect; Effect.StatTarget = EBreakerNodeStatTarget::AddedWeaponDamage;
    Effect.StatBucket = EBreakerNodeStatBucket::Flat; Effect.ValuePerRank = 3;
    Node->Effects.Add(Effect); Tree->Nodes.Add(Node);
    auto* Class = NewObject<UBreakerClassDefinition>();
    Class->ClassId = EBreakerClassId::Caster; Class->BranchTrees.Add(Tree);
    if (!TestTrue(TEXT("Actual schema class selection"), Progression->ChoosePermanentClass(Class))) return false;
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(3, Progression->ExperienceCurve));
    auto* Weapon = Player->GetWeapon();
    Weapon->UnequippedItemLevel = 10; // Explicit non-unit item scalar proves the literal is added after scaling.
    Weapon->SetSlotArchetype(1, EBreakerWeaponArchetype::Rifle);
    Weapon->WeaponDefinition = DuplicateObject<UBreakerWeaponDefinition>(Weapon->GetActiveDefinition(), Weapon);
    Weapon->WeaponDefinition->HipSpreadDegrees = 0; Weapon->WeaponDefinition->AimSpreadDegrees = 0;
    Weapon->WeaponDefinition->Recoil.BloomPerShotDegrees = 0; Weapon->WeaponDefinition->BleedChance = 0;
    Weapon->ResetAmmunition();
    const float Base = Weapon->GetScaledBaseDamage();
    FText Reason;
    for (int32 Rank = 0; Rank < 3; ++Rank)
        if (!TestTrue(TEXT("One earned point purchases each actual rank"), Progression->PurchaseNode(Tree, Node->NodeId, Reason))) return false;
    TestEqual(TEXT("Literal ranks aggregate independently of percent damage"), Progression->GetNodeStats().AddedWeaponDamage, 9.0f, .0001f);
    TestEqual(TEXT("Ability coefficient base excludes weapon literal"), Weapon->GetItemLevelBaseDamage(), Base, .0001f);
    TestEqual(TEXT("Displayed/melee base includes literal exactly once"), Weapon->GetScaledBaseDamage(), Base + 9, .0001f);
    const float AuthoredDamage = Weapon->WeaponDefinition->Damage;
    Weapon->WeaponDefinition->Damage = 0;
    TestEqual(TEXT("Zero-base utility does not acquire damage"), Weapon->GetScaledBaseDamage(), 0.0f);
    Weapon->WeaponDefinition->Damage = AuthoredDamage;
    auto* Target = World->SpawnActor<AActor>();
    if (!Target) return false;
    auto* Body = NewObject<USphereComponent>(Target);
    Target->AddInstanceComponent(Body); Target->SetRootComponent(Body);
    Body->SetSphereRadius(80); Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Body->SetCollisionResponseToAllChannels(ECR_Ignore);
    Body->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block); Body->RegisterComponent();
    auto* Victim = NewObject<UBreakerCombatComponent>(Target);
    Target->AddInstanceComponent(Victim); Victim->RegisterComponent();
    auto* Health = NewObject<UBreakerAttributeSet>(Target);
    Health->ApplyMaxHealth(10000); Health->ApplyHealth(10000); Victim->BindAttributes(Health);
    FVector Eye; FRotator Aim; Player->GetActorEyesViewPoint(Eye, Aim);
    Target->SetActorLocation(Eye + Aim.Vector() * 500);
    auto Fire = [&]()
    {
        const int32 Before = Weapon->GetMagazineAmmo();
        Weapon->StartFire(); Weapon->StopFire();
        TestEqual(TEXT("Real trigger consumes one round"), Weapon->GetMagazineAmmo(), Before - 1);
    };
    const float BeforeHit = Health->GetHealth();
    Fire();
    TestEqual(TEXT("Actual hitscan applies literal before ordinary source pools"), BeforeHit - Health->GetHealth(),
        (Base + 9) * Attributes->GetDamageMultiplier(), .002f);
    for (int32 Step = 0; Step < 25; ++Step) { ++GFrameCounter; World->Tick(LEVELTICK_All, .05f); }
    Weapon->WeaponDefinition = nullptr;
    Weapon->SetSlotArchetype(1, EBreakerWeaponArchetype::Rocket);
    Weapon->ResetAmmunition();
    const float RocketBase = Weapon->GetScaledBaseDamage();
    const float SnapshotMultiplier = Attributes->GetDamageMultiplier();
    Fire();
    ABreakerRocketProjectile* Rocket = nullptr;
    for (TActorIterator<ABreakerRocketProjectile> It(World); It; ++It) if (!It->HasExploded()) { Rocket = *It; break; }
    if (!TestNotNull(TEXT("Actual fire creates rocket"), Rocket)) return false;
    if (auto* Flight = Rocket->FindComponentByClass<UProjectileMovementComponent>())
    { Flight->StopMovementImmediately(); Flight->Deactivate(); }
    Rocket->SetActorEnableCollision(false);
    if (!TestTrue(TEXT("Real free respec removes purchased literal"), Progression->RespecCore(Reason))) return false;
    TestEqual(TEXT("Live base loses literal after respec"), Weapon->GetScaledBaseDamage(), RocketBase - 9, .0001f);
    const float BeforeExplosion = Health->GetHealth();
    // Explicit production explosion seam isolates fire-time snapshot, not flight.
    Rocket->Explode(Target->GetActorLocation());
    TestEqual(TEXT("Fired rocket retains literal and original source pools"), BeforeExplosion - Health->GetHealth(),
        RocketBase * SnapshotMultiplier, .002f);
    return true;
}
#endif
