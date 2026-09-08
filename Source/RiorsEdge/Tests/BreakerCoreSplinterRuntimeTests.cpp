#include "Tests/BreakerCoreSplinterRuntimeObserver.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "AI/BreakerEnemyMovementComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerStatusComponent.h"
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
#include "Weapons/BreakerRocketProjectile.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponDefinition.h"

void UBreakerCoreSplinterRuntimeObserver::OnHit(const FBreakerHitContext& Hit)
{
    if (Hit.bFromDoT) return;
    Hits.Add(Hit);
    if (bReenter && ReentryRocket && ReentryTarget)
    {
        bReenter = false;
        ReentryRocket->Explode(ReentryTarget->GetActorLocation(), ReentryTarget);
    }
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreSplinterRuntimeTest, "RiorsEdge.Weapons.CoreSplinterRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreSplinterRuntimeTest::RunTest(const FString& Parameters)
{
    FBreakerAttributeContribution TreeMores, GearMores;
    TreeMores.AddDamageMoreSource(TEXT("Splinter"), EBreakerDamageMoreLane::WeaponBeyondFirst, 1.20f);
    TreeMores.AddDamageMoreSource(TEXT("Fixate"), EBreakerDamageMoreLane::WeaponCritical, 1.22f);
    GearMores.AddDamageMoreSource(TEXT("Gear.Weapon"), EBreakerDamageMoreLane::Weapon, 1.24f);
    FBreakerAttributeAggregator MoreFold;
    MoreFold.SetContribution(EBreakerAttributeContributor::Progression, TreeMores);
    MoreFold.SetContribution(EBreakerAttributeContributor::Equipment, GearMores);
    TestEqual(TEXT("Splinter shares the first three selected sources"), MoreFold.GetSelectedDamageMoreSourceCount(), 3);
    TestEqual(TEXT("Selected Splinter keeps its own scope"), MoreFold.GetScopedMoreProduct(false,false,false,false,false,true), 1.20f);
    GearMores.AddDamageMoreSource(TEXT("Gear.Shared"), EBreakerDamageMoreLane::Shared, 1.26f);
    MoreFold.SetContribution(EBreakerAttributeContributor::Equipment, GearMores);
    TestEqual(TEXT("Stronger fourth source displaces Splinter rather than adding a slot"), MoreFold.GetScopedMoreProduct(false,false,false,false,false,true), 1.0f);
    // Separate native loadouts keep channels and ammunition honest: no edited
    // weapon definitions, free reloads, forced crits, or enlarged hitboxes.
    for (int32 Scenario = 0; Scenario < 3; ++Scenario)
    {
        UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
        auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
        if (!World) return false;
        GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); World->InitializeActorsForPlay(FURL());
        const uint64 InitialFrame = GFrameCounter;
        ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = InitialFrame; };
        auto* Player = World->SpawnActor<ABreakerCharacter>(); if (!Player) return false;
        Player->SetActorTickEnabled(false); Player->GetBreakerMovement()->SetComponentTickEnabled(false);
        auto* Attr = Player->GetAttributes(); auto* ASC = Player->GetAbilitySystemComponent();
        ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Attr);
        Player->GetCombat()->BindAttributes(Attr);
        auto* Progression = Player->GetProgression(); Progression->BindAttributes(Attr);
        auto* Class = NewObject<UBreakerClassDefinition>(); Class->ClassId = EBreakerClassId::Caster;
        auto* Tree = NewObject<UBreakerProgressionTree>(Class); Tree->TreeId = TEXT("Test.Core.Splinter"); Tree->Currency = EBreakerPointCurrency::CorePoints; Class->BranchTrees.Add(Tree);
        auto* Splinter = NewObject<UBreakerProgressionNode>(Tree); Splinter->NodeId = TEXT("Test.Core.Splinter.More"); Splinter->Currency = Tree->Currency;
        FBreakerNodeEffect More; More.StatTarget = EBreakerNodeStatTarget::WeaponBeyondFirstDamage; More.StatBucket = EBreakerNodeStatBucket::MorePercent; More.ValuePerRank = 20;
        Splinter->Effects.Add(More); Tree->Nodes.Add(Splinter);
        auto* Channel = NewObject<UBreakerProgressionNode>(Tree); Channel->NodeId = TEXT("Test.Core.Splinter.Channel"); Channel->Currency = Tree->Currency;
        if (Scenario == 2) Channel->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Fan")));
        else { FBreakerNodeEffect E; E.StatTarget = Scenario == 0 ? EBreakerNodeStatTarget::Pierce : EBreakerNodeStatTarget::ChainCount; E.ValuePerRank = 1; Channel->Effects.Add(E); }
        Tree->Nodes.Add(Channel);
        if (!Progression->ChoosePermanentClass(Class)) return false;
        Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(5, Progression->ExperienceCurve));
        FText Reason;
        if (!TestTrue(TEXT("Earned points purchase Splinter"), Progression->PurchaseNode(Tree, Splinter->NodeId, Reason))
            || !TestTrue(TEXT("Earned points purchase native delivery channel"), Progression->PurchaseNode(Tree, Channel->NodeId, Reason))) return false;
        if (Scenario == 0)
        {
            FBreakerDamageRequest Snapshot; Snapshot.BaseDamage = 10; Snapshot.bCanCritical = false; Snapshot.bCanBeAvoided = false;
            UBreakerDamageLibrary::FillSourcePools(Attr, EBreakerDamageDelivery::Weapon, Snapshot);
            TestEqual(TEXT("Purchased Splinter is selected in emitted source snapshot"), Snapshot.WeaponBeyondFirstMoreProduct, 1.2f, .001f);
            Snapshot.bWeaponBeyondFirstTarget = true;
            FBreakerDefenseState Defense; Defense.Health = 1000;
            auto Periodic = Snapshot; Periodic.bIsDamageOverTime = true;
            auto Ability = Snapshot; Ability.Delivery = EBreakerDamageDelivery::Ability;
            auto Baseline = Snapshot; Baseline.WeaponBeyondFirstMoreProduct = 1;
            const float Plain = UBreakerDamageLibrary::ResolveDamage(Baseline, Defense).RawDamage;
            TestEqual(TEXT("Copied secondary-target flag cannot amplify derived DoT"), UBreakerDamageLibrary::ResolveDamage(Periodic, Defense).RawDamage, Plain, .001f);
            TestEqual(TEXT("Copied secondary-target flag cannot amplify ability delivery"), UBreakerDamageLibrary::ResolveDamage(Ability, Defense).RawDamage, Plain, .001f);
        }
        auto* Observer = NewObject<UBreakerCoreSplinterRuntimeObserver>();
        auto SpawnEnemy = [&](const FVector& Position)
        {
            auto* Enemy = World->SpawnActor<ABreakerEnemy>(Position, FRotator::ZeroRotator);
            if (!Enemy) return Enemy;
            Enemy->SetAreaLevel(100); Enemy->ConfigureCrowdProbe(); Enemy->DispatchBeginPlay(); Enemy->SetActorTickEnabled(false);
            if (auto* M = Enemy->FindComponentByClass<UBreakerEnemyMovementComponent>()) M->SetComponentTickEnabled(false);
            if (auto* S = Enemy->FindComponentByClass<UBreakerStatusComponent>()) S->SetComponentTickEnabled(false);
            auto* C = Enemy->FindComponentByClass<UBreakerCombatComponent>(); C->SetComponentTickEnabled(false);
            C->OnDamageTaken.AddDynamic(Observer, &UBreakerCoreSplinterRuntimeObserver::OnHit);
            return Enemy;
        };
        FVector Eye; FRotator Aim; Player->GetActorEyesViewPoint(Eye, Aim);
        auto* A = SpawnEnemy(Scenario == 2 ? FVector(10000,0,0) : Eye + Aim.Vector() * 150);
        auto* B = SpawnEnemy(Scenario == 2 ? FVector(10000,3000,0) : Eye + Aim.Vector() * 300 + (Scenario == 1 ? FVector(0,180,0) : FVector::ZeroVector));
        if (!A || !B) return false;
        auto* Weapon = Player->GetWeapon(); Weapon->WeaponDefinition = nullptr;
        Weapon->SetSlotArchetype(1, Scenario == 0 ? EBreakerWeaponArchetype::Shotgun : Scenario == 1 ? EBreakerWeaponArchetype::Rifle : EBreakerWeaponArchetype::Rocket);
        Weapon->ResetAmmunition();
        auto Clock = [&](float Seconds) { for (int32 I=0; I<FMath::CeilToInt(Seconds*100); ++I) { ++GFrameCounter; World->Tick(LEVELTICK_All,.01f); } };
        auto Fire = [&]()
        {
            if (Weapon->GetMagazineAmmo() == 0) { Weapon->StartReload(); Clock(5); }
            Clock(FMath::Max(1.f, 60.f / Weapon->GetEffectiveRoundsPerMinute(Weapon->GetActiveDefinition()) + .05f));
            const int32 Ammo = Weapon->GetMagazineAmmo(); Weapon->StartFire(); Weapon->StopFire();
            return TestEqual(TEXT("Each actual trigger spends one round"), Weapon->GetMagazineAmmo(), Ammo - 1);
        };
        const float Base = Weapon->GetScaledBaseDamage() * Attr->GetDamageMultiplier();
        const float Critical = Attr->GetCriticalMultiplier();
        auto CheckHit = [&](const FBreakerHitContext& Hit, float Factor)
        {
            TestFalse(TEXT("Native center-body fixture avoids head weakpoint"), Hit.Result.bWeakPoint);
            TestEqual(TEXT("Actual impact pays snapshotted target-scoped More once"), Hit.Result.RawDamage,
                Base * Factor * (Hit.Result.bCritical ? Critical : 1.f), .02f);
        };
        if (Scenario < 2)
        {
            if (!Fire()) return false;
            int32 CountA=0, CountB=0;
            for (const auto& Hit : Observer->Hits)
            {
                if (Hit.Target == A) { ++CountA; CheckHit(Hit, 1); }
                if (Hit.Target == B) { ++CountB; CheckHit(Hit, 1.2f * (Scenario == 0 ? Weapon->PierceDamageFalloff : Weapon->ChainDamageMultiplier)); }
            }
            TestEqual(TEXT("Every native pellet retains first target identity"), CountA, Weapon->GetActiveDefinition()->PelletsPerShot);
            TestEqual(TEXT("Every native pellet reaches the earned secondary channel"), CountB, Weapon->GetActiveDefinition()->PelletsPerShot);
            continue;
        }
        TSet<ABreakerRocketProjectile*> Seen;
        auto Launch = [&]()
        {
            Fire(); TArray<ABreakerRocketProjectile*> Out;
            for (TActorIterator<ABreakerRocketProjectile> It(World); It; ++It)
                if (It->GetOwner() == Player && !Seen.Contains(*It))
                {
                    Seen.Add(*It); Out.Add(*It); It->SetActorEnableCollision(false);
                    if (auto* M = It->FindComponentByClass<UProjectileMovementComponent>()) { M->StopMovementImmediately(); M->Deactivate(); }
                }
            return Out;
        };
        auto First = Launch(); auto Second = Launch();
        if (!TestEqual(TEXT("First paid Fan pull emits three siblings"), First.Num(),3)
            || !TestEqual(TEXT("Second paid Fan pull emits three siblings"), Second.Num(),3)) return false;
        Observer->ReentryRocket = First[1]; Observer->ReentryTarget = B; Observer->bReenter = true;
        First[0]->Explode(A->GetActorLocation(), A);
        if (!TestEqual(TEXT("Damage callback delivers sibling impact synchronously"), Observer->Hits.Num(),2)) return false;
        CheckHit(Observer->Hits[0],1); CheckHit(Observer->Hits[1],1.2f);
        Observer->Hits.Reset(); Second[0]->Explode(B->GetActorLocation(),B);
        if (!TestEqual(TEXT("Overlapping second trigger has its own first target"), Observer->Hits.Num(),1)) return false;
        CheckHit(Observer->Hits[0],1);
        if (!TestTrue(TEXT("Real respec happens while sibling projectiles remain live"), Progression->RespecCore(Reason))) return false;
        Observer->Hits.Reset(); First[2]->Explode(A->GetActorLocation(),A); Second[1]->Explode(A->GetActorLocation(),A);
        if (!TestEqual(TEXT("Both old contexts survive source respec"), Observer->Hits.Num(),2)) return false;
        CheckHit(Observer->Hits[0],1); CheckHit(Observer->Hits[1],1.2f);
        B->Destroy(); Observer->Hits.Reset(); Second[2]->Explode(A->GetActorLocation(),A);
        if (!TestEqual(TEXT("First target destruction preserves the other-target classification"), Observer->Hits.Num(),1)) return false;
        CheckHit(Observer->Hits[0],1.2f);
        // Respec withdraws the two spent-point baseline as well as Splinter.
        // Old siblings above retain Base; this new trigger snapshots live pools.
        const float AfterRespecBase = Weapon->GetScaledBaseDamage() * Attr->GetDamageMultiplier();
        const float AfterRespecCritical = Attr->GetCriticalMultiplier();
        auto After = Launch(); if (!TestEqual(TEXT("Respec restores one native rocket"),After.Num(),1)) return false;
        Observer->Hits.Reset(); After[0]->Explode(A->GetActorLocation(),A);
        if (!TestEqual(TEXT("New trigger after respec still delivers actual damage"), Observer->Hits.Num(),1)) return false;
        TestFalse(TEXT("Post-respec rocket remains a body impact"), Observer->Hits[0].Result.bWeakPoint);
        TestEqual(TEXT("New trigger uses post-respec pools without Splinter"), Observer->Hits[0].Result.RawDamage,
            AfterRespecBase * (Observer->Hits[0].Result.bCritical ? AfterRespecCritical : 1.f), .02f);
    }
    return true;
}
#endif
