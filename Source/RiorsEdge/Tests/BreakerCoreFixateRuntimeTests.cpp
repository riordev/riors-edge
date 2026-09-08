#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponDefinition.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreFixateRuntimeTest, "RiorsEdge.Combat.CoreFixateRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreFixateRuntimeTest::RunTest(const FString& Parameters)
{
    using Lane = EBreakerDamageMoreLane;
    FBreakerAttributeContribution TreeContribution, Gear;
    TreeContribution.AddDamageMoreSource(TEXT("Fixate"), Lane::WeaponCritical, 1.22f);
    TreeContribution.AddDamageMoreSource(TEXT("Weapon"), Lane::Weapon, 1.24f);
    Gear.AddDamageMoreSource(TEXT("Gear.High"), Lane::Shared, 1.26f);
    Gear.AddDamageMoreSource(TEXT("Gear.Low"), Lane::Ability, 1.20f);
    FBreakerAttributeAggregator Fold;
    Fold.SetContribution(EBreakerAttributeContributor::Progression, TreeContribution);
    Fold.SetContribution(EBreakerAttributeContributor::Equipment, Gear);
    TestEqual(TEXT("Conditional More competes for the same three slots"), Fold.GetSelectedDamageMoreSourceCount(), 3);
    TestEqual(TEXT("Selected critical scope is separate from ordinary weapon product"), Fold.GetScopedMoreProduct(false,false,false,false,true), 1.22f, .0001f);
    TestEqual(TEXT("Unconditional weapon product excludes Fixate"), Fold.ComposedMoreProduct(EBreakerAggregatedAttribute::DamageMultiplier), 1.24f*1.26f, .0001f);
    Gear.AddDamageMoreSource(TEXT("Gear.Third"), Lane::Void, 1.25f);
    Fold.SetContribution(EBreakerAttributeContributor::Equipment, Gear);
    TestEqual(TEXT("Stronger gear displaces Fixate instead of granting a fourth slot"), Fold.GetScopedMoreProduct(false,false,false,false,true), 1.f);

    UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); World->InitializeActorsForPlay(FURL());
    const uint64 Frame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = Frame; };
    auto* Player = World->SpawnActor<ABreakerCharacter>(); if (!Player) return false;
    Player->SetActorTickEnabled(false); Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    auto* Attributes = Player->GetAttributes(); auto* ASC = Player->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Player,Player); ASC->AddAttributeSetSubobject(Attributes);
    Player->GetCombat()->BindAttributes(Attributes); Player->GetEquipment()->BindAttributes(Attributes);
    auto* Progression = Player->GetProgression(); Progression->BindAttributes(Attributes);
    auto* Class = NewObject<UBreakerClassDefinition>(); Class->ClassId = EBreakerClassId::Caster;
    auto* Tree = NewObject<UBreakerProgressionTree>(Class); Tree->TreeId = TEXT("Test.Core.Fixate"); Tree->Currency = EBreakerPointCurrency::CorePoints;
    Class->BranchTrees.Add(Tree);
    auto* Fixate = NewObject<UBreakerProgressionNode>(Tree); Fixate->NodeId = TEXT("Test.Core.Fixate.More"); Fixate->Currency = Tree->Currency;
    FBreakerNodeEffect Effect; Effect.StatTarget = EBreakerNodeStatTarget::WeaponCriticalDamage;
    Effect.StatBucket = EBreakerNodeStatBucket::MorePercent; Effect.ValuePerRank = 22;
    Fixate->Effects.Add(Effect); Tree->Nodes.Add(Fixate);
    auto* Deadeye = NewObject<UBreakerProgressionNode>(Tree); Deadeye->NodeId = TEXT("Test.Core.Fixate.Deadeye"); Deadeye->Currency = Tree->Currency;
    Deadeye->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Deadeye"))); Tree->Nodes.Add(Deadeye);
    if (!Progression->ChoosePermanentClass(Class)) return false;
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(5, Progression->ExperienceCurve));
    FText Reason;
    if (!TestTrue(TEXT("Fixate bought with earned point"), Progression->PurchaseNode(Tree, Fixate->NodeId, Reason))) return false;
    auto* Target = World->SpawnActor<AActor>(); auto* Body = NewObject<USphereComponent>(Target);
    Target->AddInstanceComponent(Body); Target->SetRootComponent(Body); Body->SetSphereRadius(100);
    Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Body->SetCollisionResponseToAllChannels(ECR_Ignore);
    Body->SetCollisionResponseToChannel(ECC_GameTraceChannel2,ECR_Block); Body->RegisterComponent();
    FVector Eye; FRotator Aim; Player->GetActorEyesViewPoint(Eye,Aim); Target->SetActorLocation(Eye+Aim.Vector()*500);
    auto* Victim = NewObject<UBreakerCombatComponent>(Target); Target->AddInstanceComponent(Victim); Victim->RegisterComponent();
    auto* Health = NewObject<UBreakerAttributeSet>(Target); Health->ApplyMaxHealth(100000); Health->ApplyHealth(100000); Victim->BindAttributes(Health);
    auto* Weapon = Player->GetWeapon(); Weapon->ResetAmmunition();
    auto Fire = [&]()
    {
        for (int32 I=0; I<20; ++I) { ++GFrameCounter; World->Tick(LEVELTICK_All,.05f); }
        const int32 Ammo = Weapon->GetMagazineAmmo(); Weapon->StartFire(); Weapon->StopFire();
        TestEqual(TEXT("Actual rifle trigger pays ammunition"),Weapon->GetMagazineAmmo(),Ammo-1);
        return Weapon->GetLastShot().DamageResult;
    };
    bool bNoncritical = false;
    for (int32 I=0; I<8 && !bNoncritical; ++I)
    {
        const auto Result = Fire(); if (!TestTrue(TEXT("Shipped rifle hits real collision target"),Result.HealthDamage>0)) return false;
        if (!Result.bCritical)
        {
            bNoncritical = true;
            TestEqual(TEXT("Noncritical rifle does not acquire Fixate"),Result.RawDamage,Weapon->GetScaledBaseDamage()*Attributes->GetDamageMultiplier(),.001f);
        }
    }
    if (!TestTrue(TEXT("Native crit sequence supplies noncritical coverage"),bNoncritical)) return false;
    if (!TestTrue(TEXT("Deadeye also uses earned purchase"),Progression->PurchaseNode(Tree,Deadeye->NodeId,Reason))) return false;
    const auto Critical = Fire(); TestTrue(TEXT("Deadeye guarantees real rifle crit"),Critical.bCritical);
    TestEqual(TEXT("Critical rifle pays selected Fixate once"),Critical.RawDamage,
        Weapon->GetScaledBaseDamage()*Attributes->GetDamageMultiplier()*Attributes->GetCriticalMultiplier()*1.22f,.001f);
    FBreakerDamageRequest Stored; Stored.BaseDamage=10; Stored.SetInstigator(Player); Stored.CriticalMultiplier=Attributes->GetCriticalMultiplier();
    UBreakerDamageLibrary::FillSourcePools(Attributes,EBreakerDamageDelivery::Weapon,Stored);
    const float StoredExpected=10*Stored.SourceDamageMultiplier*Stored.CriticalMultiplier*1.22f;
    if (!Progression->RespecCore(Reason)) return false;
    TestEqual(TEXT("Emitted source retains Fixate and Deadeye through respec"),Victim->ReceiveDamage(Stored).RawDamage,StoredExpected,.001f);
    Stored.bIsDamageOverTime=true; Stored.bCanCritical=false;
    TestEqual(TEXT("Derived tick does not acquire direct conditional More"),Victim->ReceiveDamage(Stored).RawDamage,10*Stored.SourceDamageMultiplier,.001f);
    Stored.bIsDamageOverTime=false; Stored.Delivery=EBreakerDamageDelivery::Ability; Stored.bForceCriticalStrike=false;
    TestEqual(TEXT("Ability delivery cannot borrow weapon conditional scope"),Victim->ReceiveDamage(Stored).RawDamage,10*Stored.SourceDamageMultiplier,.001f);
    FBreakerDamageRequest Converted; Converted.BaseDamage=10; Converted.bHasSourceSplit=true;
    Converted.CriticalChance=1; Converted.CriticalMultiplier=1.5f; Converted.WeaponCriticalMoreProduct=1.22f;
    Converted.Element=EBreakerElement::Void; Converted.ElementalFraction=1;
    Converted.ElementSource.ElementalMoreProduct=1.26f; Converted.ElementSource.VoidMoreProduct=1.25f;
    Player->GetCombat()->PushOutgoingModifier(TEXT("Test.Fixate.Window"),0,1.30f,0);
    Player->GetCombat()->ApplyOutgoingModifiers(Converted);
    const auto Payout=UBreakerDamageLibrary::ResolveDamage(Converted,{});
    TestEqual(TEXT("Converted crit and temporary window share one ceiling"),Payout.RawDamage,15*FBreakerAttributeAggregator::ComposedMoreCeiling(),.001f);
    return true;
}
#endif
