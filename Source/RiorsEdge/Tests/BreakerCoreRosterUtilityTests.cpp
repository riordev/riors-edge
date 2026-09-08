#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerCoreRoster.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
UBreakerProgressionTree* UtilityCandidate(UObject* Outer, FString& Error)
{
    TArray<FBreakerCoreWedgeDefinition> Wedges; BreakerCoreRoster::AppendUtility(Outer, Wedges);
    return BreakerCoreTree::Build(Outer, TEXT("Test.CoreRoster.Utility"), FText::FromString(TEXT("Utility candidate")), Wedges, Error);
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreRosterUtilityAuthoringTest, "RiorsEdge.Progression.CoreRoster.UtilityAuthoring",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreRosterUtilityAuthoringTest::RunTest(const FString& Parameters)
{
    FString Error; auto* Tree = UtilityCandidate(GetTransientPackage(), Error);
    if (!TestNotNull(*Error, Tree)) return false;
    TestEqual(TEXT("Two Utility wedges"), Tree->CoreWedgeOrder.Num(), 2);
    TestEqual(TEXT("Twelve authored nodes"), Tree->Nodes.Num(), 12);
    int32 Offered = 0, Effects = 0, Rules = 0;
    TSet<FName> Ids;
    for (const UBreakerProgressionNode* N : Tree->Nodes)
    {
        Offered += N->MaxRank * N->CostPerRank; Effects += N->Effects.Num(); Rules += N->GrantedTags.Num();
        TestFalse(TEXT("Unique node identity"), Ids.Contains(N->NodeId)); Ids.Add(N->NodeId);
        TestEqual(TEXT("Utility sector metadata"), Tree->CoreWedgeSectors.FindRef(N->Constellation), FName(TEXT("Utility")));
        TestTrue(TEXT("Real lane or rule"), !N->Effects.IsEmpty() || !N->GrantedTags.IsEmpty());
        TestFalse(TEXT("Minor has no keystone"), N->CoreRole == EBreakerCoreNodeRole::Keystone);
        if (N->CoreRole == EBreakerCoreNodeRole::LaneMinor) TestEqual(TEXT("Rank-three minor"), N->MaxRank, 3);
    }
    TestEqual(TEXT("26 offered points"), Offered, 26); TestEqual(TEXT("Six lane effects"), Effects, 6); TestEqual(TEXT("Six rules"), Rules, 6);
    auto Find = [&](const TCHAR* Id) -> const UBreakerProgressionNode*
    { for (const UBreakerProgressionNode* N : Tree->Nodes) if (N->NodeId == FName(Id)) return N; AddError(FString(TEXT("Missing ")) + Id); return nullptr; };
    using T = EBreakerNodeStatTarget;
    auto Effect = [&](const TCHAR* Id, T Target, float Value)
    {
        const auto* N = Find(Id); if (!N || !TestEqual(Id, N->Effects.Num(), 1)) return;
        TestTrue(Id, N->Effects[0].StatTarget == Target && N->Effects[0].StatBucket == EBreakerNodeStatBucket::IncreasedPercent);
        TestEqual(Id, N->Effects[0].ValuePerRank, Value);
    };
    Effect(TEXT("Core.Control.Concussive"), T::StaggerDuration, 15.f);
    Effect(TEXT("Core.Control.Jarring"), T::StaggerDuration, 8.f);
    Effect(TEXT("Core.Control.Insistent"), T::EnemyStaggerResistanceReduction, 6.f);
    Effect(TEXT("Core.Threat.Presence"), T::ThreatGenerated, 20.f);
    Effect(TEXT("Core.Threat.Loud"), T::ThreatGenerated, 10.f);
    Effect(TEXT("Core.Threat.Decoy"), T::DeployableHealth, 15.f);
    const TCHAR* NodeIds[] = {TEXT("Core.Control.Shockwave"), TEXT("Core.Control.Interrupt"), TEXT("Core.Control.Lockstep"), TEXT("Core.Threat.Provocation"), TEXT("Core.Threat.Bait"), TEXT("Core.Threat.IgnoreMe")};
    const TCHAR* Tags[] = {TEXT("Progression.Node.Core.Control.Shockwave"), TEXT("Progression.Node.Core.Control.Interrupt"), TEXT("Progression.Node.Core.Control.Lockstep"), TEXT("Progression.Node.Core.Provocation"), TEXT("Progression.Node.Core.Threat.Bait"), TEXT("Progression.Node.Core.Threat.IgnoreMe")};
    for (int32 I = 0; I < UE_ARRAY_COUNT(NodeIds); ++I)
        if (const auto* N = Find(NodeIds[I])) TestTrue(NodeIds[I], N->GrantedTags.HasTagExact(FGameplayTag::RequestGameplayTag(Tags[I])));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreRosterUtilityRuntimeTest, "RiorsEdge.Progression.CoreRoster.UtilityPurchasedRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreRosterUtilityRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); World->InitializeActorsForPlay(FURL());
    const uint64 Frame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = Frame; };
    auto PlayerAt = [&](FVector Location)
    {
        auto* Player = World->SpawnActor<ABreakerCharacter>(Location, FRotator::ZeroRotator);
        if (!Player) return Player;
        Player->bRefuseSavesForPendingCharacter = true; Player->SetActorTickEnabled(false);
        Player->GetBreakerMovement()->SetComponentTickEnabled(false);
        auto* ASC = Player->GetAbilitySystemComponent(); auto* Attr = Player->GetAttributes();
        ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Attr);
        Player->GetCombat()->BindAttributes(Attr); Player->GetProgression()->BindAttributes(Attr);
        return Player;
    };
    auto* Player = PlayerAt(FVector(900, 0, 100)); auto* Rival = PlayerAt(FVector(400, 0, 100));
    if (!Player || !Rival) return false;
    auto* P = Player->GetProgression();
    if (!TestTrue(TEXT("Real class selected"), P->ChoosePermanentClassById(EBreakerClassId::Gunsmith))) return false;
    auto* Definition = DuplicateObject<UBreakerClassDefinition>(P->ClassDefinition, Player);
    // Candidate IDs deliberately match the replacement roster. The collector
    // searches class trees before global fallbacks, so remove the copied live
    // Core definition to make this candidate the authoritative fixture schema.
    // Retain Doctrine and the actual registered class kit/loadout.
    Definition->BranchTrees.RemoveAll([](const TObjectPtr<UBreakerProgressionTree>& Existing)
    { return Existing && Existing->Currency == EBreakerPointCurrency::CorePoints; });
    FString Error; auto* Tree = UtilityCandidate(Definition, Error); if (!TestNotNull(*Error, Tree)) return false;
    Definition->BranchTrees.Add(Tree); P->ClassDefinition = Definition;
    P->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(20, P->ExperienceCurve));
    const int32 Budget = P->GetUnspentPoints(Tree->Currency); FText Reason;
    auto Buy = [&](const TCHAR* Id, int32 Ranks = 1)
    {
        for (int32 I = 0; I < Ranks; ++I)
            if (!TestTrue(Id, P->PurchaseNode(Tree, Id, Reason))) { AddError(Reason.ToString()); return false; }
        return true;
    };
    auto* Enemy = World->SpawnActor<ABreakerEnemy>(FVector(0, 0, 100), FRotator::ZeroRotator);
    if (!Enemy) return false;
    Enemy->ConfigureCrowdProbe(); Enemy->DispatchBeginPlay(); Enemy->SetActorTickEnabled(false);
    auto* Combat = Enemy->FindComponentByClass<UBreakerCombatComponent>(); if (!Combat) return false;
    Enemy->bStaggerImmune = true;
    TestFalse(TEXT("Unowned player cannot stagger immune enemy"), Combat->ApplyStaggerFrom(Player, 1.f));
    if (!Buy(TEXT("Core.Control.Concussive")) || !Buy(TEXT("Core.Control.Jarring"), 3)
        || !Buy(TEXT("Core.Control.Shockwave")) || !Buy(TEXT("Core.Control.Insistent"), 3)
        || !Buy(TEXT("Core.Control.Interrupt"))) return false;
    TestFalse(TEXT("Lanes alone do not bypass immunity"), Combat->ApplyStaggerFrom(Player, 1.f));
    if (!Buy(TEXT("Core.Control.Lockstep"))) return false;
    TestEqual(TEXT("Full minor costs 13 earned points"), P->GetUnspentPoints(Tree->Currency), Budget - 13);
    if (!TestTrue(TEXT("Paid Lockstep bypasses enemy immunity"), Combat->ApplyStaggerFrom(Player, 1.f))) return false;
    const float Expected = 1.39f * (1.f - FMath::Clamp(Combat->StaggerResistance - .18f, 0.f, 1.f)) * .5f;
    TestEqual(TEXT("Actual stagger consumes ranked duration and resistance with immunity penalty"), Combat->GetStaggerRemaining(), Expected, .001f);
    if (!TestTrue(TEXT("Native Core respec"), P->RespecCore(Reason))) return false;
    TestFalse(TEXT("Respec removes immunity exception"), Combat->ApplyStaggerFrom(Player, 1.f));
    TestEqual(TEXT("Refund restores budget"), P->GetUnspentPoints(Tree->Currency), Budget);

    // A fresh encounter avoids an already-earned stagger blocking target selection.
    Enemy->Destroy();
    Enemy = World->SpawnActor<ABreakerEnemy>(FVector(0, 0, 100), FRotator::ZeroRotator);
    if (!Enemy) return false;
    Enemy->ConfigureCrowdProbe(); Enemy->DispatchBeginPlay(); Enemy->SetActorTickEnabled(false);
    Combat = Enemy->FindComponentByClass<UBreakerCombatComponent>(); if (!Combat) return false;
    auto Hit = [&](AActor* Source, float Damage)
    {
        FBreakerDamageRequest Request; Request.BaseDamage = Damage; Request.DamageFamily = EBreakerDamageFamily::TrueDamage;
        Request.bCanCritical = false; Request.bCanBeAvoided = false; Request.SetInstigator(Source);
        return Combat->ReceiveDamage(Request);
    };
    auto Select = [&]() { Enemy->Tick(0); return Enemy->GetThreatTarget(); };
    Hit(Rival, 12.f); Hit(Player, 10.f);
    TestTrue(TEXT("Higher accepted damage initially holds threat"), Select() == Rival);
    if (!Buy(TEXT("Core.Threat.Presence")) || !Buy(TEXT("Core.Threat.Loud"), 3)) return false;
    Enemy->ClearThreat(); Hit(Rival, 12.f); Hit(Player, 10.f);
    TestTrue(TEXT("Actual paid threat lane changes enemy selection"), Select() == Player);
    TestEqual(TEXT("Threat path costs four earned points"), P->GetUnspentPoints(Tree->Currency), Budget - 4);
    return true;
}
#endif
