#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbility_Fracture.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerManaComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerProjectileBase.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerLootLibrary.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponDefinition.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreVelocityRuntimeTest, "RiorsEdge.Movement.CoreVelocityRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreVelocityRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); World->InitializeActorsForPlay(FURL());
    const uint64 Frame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = Frame; };
    auto* Player = World->SpawnActor<ABreakerCharacter>(); if (!Player) return false;
    Player->SetActorTickEnabled(false); auto* Move = Player->GetBreakerMovement(); Move->SetComponentTickEnabled(false);
    auto* Attr = Player->GetAttributes(); auto* ASC = Player->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Attr);
    // Keep the incoming-damage fixture above the 130-point hit, so the
    // assertion measures the forfeit rather than clamped lethal damage.
    Attr->SetAggregatedAttributeBase(EBreakerAggregatedAttribute::MaxHealth, 500);
    auto* Combat = Player->GetCombat(); Combat->BindAttributes(Attr); Combat->SetComponentTickEnabled(false);
    auto* Progression = Player->GetProgression(); Progression->BindAttributes(Attr);
    if (!Progression->ChoosePermanentClassById(EBreakerClassId::Caster)) return false;
    Attr->SetAggregatedAttributeBase(EBreakerAggregatedAttribute::MoveSpeed, Move->WalkSpeed);
    auto* Equipment = Player->GetEquipment(); Equipment->BindAttributes(Attr);
    FBreakerItemInstance Boots; bool bRolled = false;
    for (int32 Seed = 1; Seed <= 4096; ++Seed)
    {
        Boots = UBreakerLootLibrary::RollItem(TEXT("Test.Velocity.Boots"), EBreakerEquipSlot::Boots, EBreakerItemRarity::Standard, 5, Seed);
        if (Boots.Affixes.ContainsByPredicate([](const FBreakerRolledAffix& A) { return A.AffixId == FName(TEXT("Core.MoveSpeed")); }))
        { bRolled = true; break; }
    }
    if (!TestTrue(TEXT("Actual movement boots roll"), bRolled) || !Equipment->EquipItem(Boots)) return false;
    const float GearSpeed = Equipment->GetStats().MoveSpeedMultiplier - 1;
    const float BaseSpeed = Attr->GetAttributeBase(EBreakerAggregatedAttribute::MoveSpeed);
    auto* Definition = DuplicateObject<UBreakerClassDefinition>(Progression->ClassDefinition, Player);
    auto* Tree = NewObject<UBreakerProgressionTree>(Definition); Tree->TreeId = TEXT("Test.Core.Velocity");
    Tree->Currency = EBreakerPointCurrency::CorePoints; Definition->BranchTrees.Add(Tree); Progression->ClassDefinition = Definition;
    auto Node = [&](const TCHAR* Id, const TCHAR* Tag)
    {
        auto* N = NewObject<UBreakerProgressionNode>(Tree); N->NodeId = Id; N->Currency = Tree->Currency;
        if (Tag) N->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(Tag)); Tree->Nodes.Add(N); return N;
    };
    auto* Speed = Node(TEXT("Test.Velocity.Speed"), nullptr);
    for (float Value : {20.0f, -5.0f})
    {
        FBreakerNodeEffect E; E.StatTarget = EBreakerNodeStatTarget::MoveSpeed;
        E.StatBucket = EBreakerNodeStatBucket::IncreasedPercent; E.ValuePerRank = Value; Speed->Effects.Add(E);
    }
    auto* WeaponRule = Node(TEXT("Test.Velocity.Momentum"), TEXT("Progression.Node.Core.Velocity.Momentum"));
    auto* AbilityRule = Node(TEXT("Test.Velocity.Downforce"), TEXT("Progression.Node.Core.Velocity.Downforce"));
    auto* NoGround = Node(TEXT("Test.Velocity.NoGround"), TEXT("Progression.Node.Core.Velocity.NoGround"));
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(6, Progression->ExperienceCurve));
    FText Reason;
    if (!Progression->PurchaseNode(Tree, Speed->NodeId, Reason)) return false;
    TestEqual(TEXT("Ordinary gear and signed tree speed share bucket"), Attr->GetMoveSpeed(), BaseSpeed * (1.15f + GearSpeed), .01f);
    if (!Progression->PurchaseNode(Tree, WeaponRule->NodeId, Reason) || !Progression->PurchaseNode(Tree, AbilityRule->NodeId, Reason)
        || !Progression->PurchaseNode(Tree, NoGround->NodeId, Reason)) return false;
    const float SpeedRatio = 1.35f + 2 * GearSpeed;
    TestEqual(TEXT("No Ground doubles positive bonuses and preserves negative five"), Attr->GetMoveSpeed(), BaseSpeed * SpeedRatio, .01f);
    TestEqual(TEXT("Real walking cap uses composed rewrite"), Move->GetWalkSpeedCap(), Move->WalkSpeed * SpeedRatio, .01f);
    Move->PushSpeedMultiplier(TEXT("Test.Buff"), 1.2f, 2);
    Move->PushSpeedMultiplier(TEXT("Test.Slow"), .8f, 2);
    TestEqual(TEXT("Temporary speed bonus doubles; slow does not"), Move->GetSpeedMultiplier(), 1.4f * .8f, .001f);
    Move->PopSpeedMultiplier(TEXT("Test.Buff")); Move->PopSpeedMultiplier(TEXT("Test.Slow"));
    auto* Target = World->SpawnActor<AActor>(); auto* Body = NewObject<USphereComponent>(Target);
    Target->AddInstanceComponent(Body); Target->SetRootComponent(Body); Body->SetSphereRadius(60);
    Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Body->SetCollisionResponseToAllChannels(ECR_Ignore);
    Body->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block); Body->RegisterComponent();
    FVector Eye; FRotator Aim; Player->GetActorEyesViewPoint(Eye, Aim); Target->SetActorLocation(Eye + Aim.Vector() * 500);
    auto* Sink = NewObject<UBreakerCombatComponent>(Target); Target->AddInstanceComponent(Sink); Sink->RegisterComponent();
    auto* Health = NewObject<UBreakerAttributeSet>(Target); Health->ApplyMaxHealth(10000); Health->ApplyHealth(10000); Sink->BindAttributes(Health);
    auto* Weapon = Player->GetWeapon(); Weapon->WeaponDefinition = DuplicateObject<UBreakerWeaponDefinition>(Weapon->GetActiveDefinition(), Weapon);
    Weapon->WeaponDefinition->HipSpreadDegrees = Weapon->WeaponDefinition->AimSpreadDegrees = 0; Weapon->WeaponDefinition->BleedChance = 0;
    Weapon->ResetAmmunition(); ASC->SetNumericAttributeBase(UBreakerAttributeSet::GetCriticalChanceAttribute(), 0);
    const float WeaponBase = Weapon->GetItemLevelBaseDamage();
    Weapon->StartFire(); Weapon->StopFire();
    TestEqual(TEXT("Actual rifle receives composed converted source power"), Weapon->GetLastShot().DamageResult.HealthDamage,
        WeaponBase * Attr->GetDamageMultiplier(), .02f);
    const float Converted = (SpeedRatio - 1) * 50;
    const auto& Fold = Attr->GetAttributeAggregator();
    const float OrdinaryWeapon = Fold.GetContribution(EBreakerAttributeContributor::Equipment).GetIncreasedPercent(EBreakerAggregatedAttribute::DamageMultiplier)
        + Fold.GetContribution(EBreakerAttributeContributor::Progression).GetIncreasedPercent(EBreakerAggregatedAttribute::DamageMultiplier);
    TestEqual(TEXT("Converter joins Increased rather than multiplying it"), Fold.ComposedIncreasedPercent(EBreakerAggregatedAttribute::DamageMultiplier), OrdinaryWeapon + Converted, .001f);
    Player->GetMana()->BindAttributes(Attr); Player->GetMana()->AdvanceLoop(20);
    const auto Handle = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Fracture::StaticClass(), 1));
    const float Resource = Attr->GetClassResource();
    if (!ASC->TryActivateAbility(Handle)) return false;
    TestTrue(TEXT("Native ability actually pays"), Attr->GetClassResource() < Resource);
    ABreakerProjectileBase* Projectile = nullptr;
    for (TActorIterator<ABreakerProjectileBase> It(World); It; ++It) if (It->GetOwner() == Player && !It->HasImpacted()) { Projectile = *It; break; }
    if (!TestNotNull(TEXT("Paid Fracture emitted"), Projectile)) return false;
    const auto Request = Projectile->GetProjectileDamage();
    const float AbilityPower = Attr->GetAbilityDamageMultiplier();
    TestEqual(TEXT("Ability snapshot contains converted lane"), Request.SourceDamageMultiplier, AbilityPower, .001f);
    FBreakerDamageRequest Incoming; Incoming.BaseDamage = 100; Incoming.DamageFamily = EBreakerDamageFamily::TrueDamage; Incoming.bCanCritical = false; Incoming.SetInstigator(Target);
    Attr->ApplyHealth(Attr->GetMaxHealth());
    TestEqual(TEXT("No Ground charges thirty percent against actual true hit"), Combat->ReceiveDamage(Incoming).HealthDamage, 130.0f, .01f);
    Incoming.SetInstigator(Player); Attr->ApplyHealth(Attr->GetMaxHealth());
    TestEqual(TEXT("Explicit self cost retains authored amount"), Combat->ReceiveDamage(Incoming).HealthDamage, 100.0f, .01f);
    if (!Progression->RespecCore(Reason)) return false;
    TestEqual(TEXT("Respec keeps only real gear speed"), Attr->GetMoveSpeed(), BaseSpeed * (1 + GearSpeed), .01f);
    const float Before = Health->GetHealth(); Projectile->Impact(Target, Target->GetActorLocation());
    TestTrue(TEXT("Emitted ability still deals converted damage after respec"), Before - Health->GetHealth() >= Request.BaseDamage * AbilityPower - .02f);
    Incoming.SetInstigator(Target); Attr->ApplyHealth(Attr->GetMaxHealth());
    TestEqual(TEXT("Respec removes incoming forfeit"), Combat->ReceiveDamage(Incoming).HealthDamage, 100.0f, .01f);
    return true;
}
#endif
