#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerCoreRoster.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"
#include "Weapons/BreakerWeaponComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreRosterWeaponTest,"RiorsEdge.Progression.CoreRoster.WeaponAuthoring",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreRosterWeaponTest::RunTest(const FString&)
{
    TArray<FBreakerCoreWedgeDefinition> Wedges; BreakerCoreRoster::AppendWeapon(GetTransientPackage(),Wedges);
    FString Error; auto* Tree=BreakerCoreTree::Build(GetTransientPackage(),TEXT("Core.WeaponCandidate"),{},Wedges,Error);
    if(!TestNotNull(TEXT("Authored Weapon sector builds"),Tree))return false;
    TestEqual(TEXT("Three majors and one minor"),Tree->Nodes.Num(),39);
    const TArray<FName> Order={TEXT("Precision"),TEXT("Vector"),TEXT("Ballistics"),TEXT("Loadout")};
    TestTrue(TEXT("Exact sector order"),Tree->CoreWedgeOrder==Order);
    int32 Offered=0; TSet<FName> Ids; TArray<const UBreakerProgressionNode*> Nodes; TArray<FBreakerNodeRank> Ranks;
    for(const UBreakerProgressionNode* N:Tree->Nodes)
    {
        Offered+=N->MaxRank*N->CostPerRank; Ids.Add(N->NodeId); Nodes.Add(N); Ranks.Add({N->NodeId,N->MaxRank});
        TestFalse(TEXT("Every node has player-facing explanation"),N->Description.IsEmpty());
        TestTrue(TEXT("Every Weapon node has a stat or an explicit rule"),!N->Effects.IsEmpty()||!N->GrantedTags.IsEmpty());
        TestEqual(TEXT("Only ranked lanes have three ranks"),N->MaxRank,N->CoreRole==EBreakerCoreNodeRole::LaneMinor?3:1);
    }
    TestEqual(TEXT("No repeated IDs"),Ids.Num(),39); TestEqual(TEXT("Authored offered rank cost"),Offered,91);
    const auto Stats=UBreakerProgressionComponent::AggregateStats(Nodes,Ranks);
    TestEqual(TEXT("Literal added damage totals all ranked bases"),Stats.AddedWeaponDamage,41.f,.001f);
    TestEqual(TEXT("Weapon Increased excludes shared damage"),Stats.DamageMultiplier,1.80f,.001f);
    TestEqual(TEXT("Spread reduction is a flat percentage-point lane"),Stats.WeaponBaseSpreadReductionPercent,10.f,.001f);
    TestEqual(TEXT("Pierce loss reduction is a flat percentage-point lane"),Stats.PierceLossReductionPercent,15.f,.001f);
    TestEqual(TEXT("Ranked critical chance"),Stats.CriticalChanceBonus,.06f,.001f);
    TestEqual(TEXT("Authored critical bonus before Deadeye composition"),Stats.CriticalMultiplierBonus,.31f,.001f);
    TestEqual(TEXT("Reserve has three ranks"),Stats.WeaponReserveAmmoMultiplier,1.45f,.001f);
    const auto* Deadeye=Tree->FindNode(TEXT("Core.Precision.Deadeye"));
    TestTrue(TEXT("Deadeye grants actual always-critical rewrite"),Deadeye&&Deadeye->GrantedTags.HasTagExact(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Deadeye"))));
    TestFalse(TEXT("New Deadeye does not inherit old weakpoint-only identity"),Deadeye->GrantedTags.HasTagExact(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Precision.Deadeye"))));

    UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);if(!World)return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);World->InitializeActorsForPlay(FURL());
    const uint64 Frame=GFrameCounter;
    ON_SCOPE_EXIT{World->DestroyWorld(false);GEngine->DestroyWorldContext(World);GFrameCounter=Frame;};
    auto* Player=World->SpawnActor<ABreakerCharacter>();if(!Player)return false;
    Player->bRefuseSavesForPendingCharacter=true;Player->SetActorTickEnabled(false);Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    auto* ASC=Player->GetAbilitySystemComponent();auto* Attr=Player->GetAttributes();ASC->InitAbilityActorInfo(Player,Player);ASC->AddAttributeSetSubobject(Attr);
    Player->GetCombat()->BindAttributes(Attr);auto* P=Player->GetProgression();P->BindAttributes(Attr);
    auto* Class=NewObject<UBreakerClassDefinition>(Player);Class->ClassId=EBreakerClassId::Caster;Class->BranchTrees.Add(Tree);
    if(!P->ChoosePermanentClass(Class))return false;
    P->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(23,P->ExperienceCurve));FText Reason;
    // Actual authored IDs, prices and prerequisites: optional ranks supply
    // eighteen local points before the five-point keystone, not free tags.
    for(const TCHAR* Id:{TEXT("Sightline"),TEXT("Angle"),TEXT("Angle"),TEXT("Angle"),TEXT("CalledShot"),
        TEXT("Cadence"),TEXT("Cadence"),TEXT("Cadence"),TEXT("TriggerDiscipline"),TEXT("Fixate"),
        TEXT("Ledger"),TEXT("Ledger"),TEXT("Ledger"),TEXT("Steady")})
        if(!TestTrue(FString(TEXT("Earned Precision purchase "))+Id,P->PurchaseNode(Tree,FName(*(FString(TEXT("Core.Precision."))+Id)),Reason)))return false;
    TestEqual(TEXT("Exact local eighteen-point gate"),P->GetConstellationInvestment(Tree,TEXT("Precision")),18);
    if(!TestTrue(TEXT("Actual five-point Deadeye purchase"),P->PurchaseNode(Tree,Deadeye->NodeId,Reason)))return false;
    TestEqual(TEXT("Minimum keystone route spends twenty-three"),P->GetUnspentPoints(Tree->Currency),0);
    auto* Target=World->SpawnActor<AActor>();auto* Body=NewObject<USphereComponent>(Target);Target->AddInstanceComponent(Body);Target->SetRootComponent(Body);
    Body->SetSphereRadius(120);Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Body->SetCollisionResponseToAllChannels(ECR_Ignore);
    Body->SetCollisionResponseToChannel(ECC_GameTraceChannel2,ECR_Block);Body->RegisterComponent();
    FVector Eye;FRotator Aim;Player->GetActorEyesViewPoint(Eye,Aim);Target->SetActorLocation(Eye+Aim.Vector()*600);
    auto* Combat=NewObject<UBreakerCombatComponent>(Target);Target->AddInstanceComponent(Combat);Combat->RegisterComponent();
    auto* Health=NewObject<UBreakerAttributeSet>(Target);Health->ApplyMaxHealth(10000);Health->ApplyHealth(10000);Combat->BindAttributes(Health);
    auto* Weapon=Player->GetWeapon();Weapon->ResetAmmunition();const int32 Before=Weapon->GetMagazineAmmo();
    Weapon->StartFire();Weapon->StopFire();const auto& Hit=Weapon->GetLastShot();
    TestEqual(TEXT("Native shot spends ammunition"),Weapon->GetMagazineAmmo(),Before-1);
    TestTrue(TEXT("Authored purchased Deadeye reaches actual shot"),Hit.bHit&&Hit.DamageResult.bCritical);
    TestEqual(TEXT("Actual critical shot includes selected Fixate once"),Hit.DamageResult.RawDamage,
        Weapon->GetScaledBaseDamage()*Attr->GetDamageMultiplier()*Attr->GetCriticalMultiplier()*1.22f,.02f);
    return true;
}
#endif
