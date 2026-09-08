#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Progression/BreakerCoreWheelMath.h"
#include "Progression/BreakerCoreRoster.h"
#include "Progression/BreakerCoreTree.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreRosterMovementTest, "RiorsEdge.Progression.CoreRoster.Movement", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreRosterMovementTest::RunTest(const FString&)
{
    TArray<FBreakerCoreWedgeDefinition> Wedges; BreakerCoreRoster::AppendMovement(GetTransientPackage(), Wedges);
    TestEqual(TEXT("Two authored Movement wedges"), Wedges.Num(), 2);
    FString Error;
    auto* Tree = BreakerCoreTree::Build(GetTransientPackage(), TEXT("Test.Core.Movement"), FText::FromString(TEXT("Movement")), Wedges, Error);
    if (!TestNotNull(TEXT("Authored Movement builds"), Tree)) return false;
    TestEqual(TEXT("Movement contains seventeen nodes"), Tree->Nodes.Num(), 17);
    int32 Offered=0; for (const UBreakerProgressionNode* N : Tree->Nodes) Offered += N->CostPerRank*N->MaxRank;
    TestEqual(TEXT("Movement offers thirty-nine points"), Offered, 39);
    auto Check = [&](const TCHAR* Id, const TCHAR* Name, int32 Cost, int32 Ranks, std::initializer_list<FBreakerNodeEffect> Effects, const TCHAR* Tag)
    {
        const auto* Found = Tree->Nodes.FindByPredicate([&](const UBreakerProgressionNode* N) { return N->NodeId == FName(Id); });
        if (!TestTrue(Id, Found != nullptr)) return;
        const UBreakerProgressionNode* N=Found->Get();
        TestEqual(TEXT("Literal display name"), N->DisplayName.ToString(), FString(Name));
        TestFalse(TEXT("Authored description is present"), N->Description.IsEmpty());
        TestEqual(TEXT("Shape price"),N->CostPerRank,Cost); TestEqual(TEXT("Shape ranks"),N->MaxRank,Ranks);
        TestEqual(TEXT("Exact effect count"),N->Effects.Num(),static_cast<int32>(Effects.size()));
        int32 Index=0; for (const auto& E : Effects)
        {
            if (N->Effects.IsValidIndex(Index))
            {
                TestTrue(TEXT("Exact stat"),N->Effects[Index].StatTarget==E.StatTarget);
                TestTrue(TEXT("Exact bucket"),N->Effects[Index].StatBucket==E.StatBucket);
                TestEqual(TEXT("Exact authored magnitude"),N->Effects[Index].ValuePerRank,E.ValuePerRank);
                TestTrue(TEXT("No legacy conditional"),N->Effects[Index].Condition==E.Condition);
            }
            ++Index;
        }
        TestEqual(TEXT("No stray rule tags"),N->GrantedTags.Num(),Tag?1:0);
        if (Tag) TestTrue(TEXT("Actual runtime consumer tag"),N->GrantedTags.HasTagExact(FGameplayTag::RequestGameplayTag(Tag)));
    };
    using BreakerCoreRoster::Effect;
    Check(TEXT("Core.Velocity.Grind"), TEXT("Grind"), 1, 1, {Effect(EBreakerNodeStatTarget::MoveSpeed, EBreakerNodeStatBucket::IncreasedPercent, 8.0f)}, nullptr);
    Check(TEXT("Core.Velocity.Stride"), TEXT("Stride"), 1, 3, {Effect(EBreakerNodeStatTarget::MoveSpeed, EBreakerNodeStatBucket::IncreasedPercent, 4.0f)}, nullptr);
    Check(TEXT("Core.Velocity.Momentum"), TEXT("Momentum"), 2, 1, {}, TEXT("Progression.Node.Core.Velocity.Momentum"));
    Check(TEXT("Core.Velocity.Slide"), TEXT("Slide"), 1, 3, {Effect(EBreakerNodeStatTarget::SlideSpeed, EBreakerNodeStatBucket::IncreasedPercent, 8.0f)}, nullptr);
    Check(TEXT("Core.Velocity.Carry"), TEXT("Carry"), 2, 1, {Effect(EBreakerNodeStatTarget::MoveSpeed, EBreakerNodeStatBucket::IncreasedPercent, 12.0f), Effect(EBreakerNodeStatTarget::SlideSpeed, EBreakerNodeStatBucket::IncreasedPercent, 12.0f)}, nullptr);
    Check(TEXT("Core.Velocity.Sprint"), TEXT("Sprint"), 1, 3, {Effect(EBreakerNodeStatTarget::SprintSpeed, EBreakerNodeStatBucket::IncreasedPercent, 5.0f)}, nullptr);
    Check(TEXT("Core.Velocity.Downforce"), TEXT("Downforce"), 2, 1, {}, TEXT("Progression.Node.Core.Velocity.Downforce"));
    Check(TEXT("Core.Velocity.Traction"), TEXT("Traction"), 1, 1, {Effect(EBreakerNodeStatTarget::MoveSpeed, EBreakerNodeStatBucket::IncreasedPercent, 5.0f)}, nullptr);
    Check(TEXT("Core.Velocity.Afterburn"), TEXT("Afterburn"), 1, 1, {Effect(EBreakerNodeStatTarget::Acceleration, EBreakerNodeStatBucket::IncreasedPercent, 8.0f)}, nullptr);
    Check(TEXT("Core.Velocity.TerminalVelocity"), TEXT("Terminal Velocity"), 3, 1, {Effect(EBreakerNodeStatTarget::Damage, EBreakerNodeStatBucket::MorePercent, 22.0f)}, nullptr);
    Check(TEXT("Core.Velocity.NoGround"), TEXT("No Ground"), 5, 1, {}, TEXT("Progression.Node.Core.Velocity.NoGround"));
    Check(TEXT("Core.Kinesis.LightFooting"), TEXT("Light Footing"), 1, 1, {Effect(EBreakerNodeStatTarget::LedgeSpeed, EBreakerNodeStatBucket::IncreasedPercent, 10.0f)}, nullptr);
    Check(TEXT("Core.Kinesis.Loft"), TEXT("Loft"), 1, 3, {Effect(EBreakerNodeStatTarget::JumpHeight, EBreakerNodeStatBucket::IncreasedPercent, 6.0f)}, nullptr);
    Check(TEXT("Core.Kinesis.AirWork"), TEXT("Air Work"), 2, 1, {Effect(EBreakerNodeStatTarget::AirControl, EBreakerNodeStatBucket::IncreasedPercent, 25.0f)}, nullptr);
    Check(TEXT("Core.Kinesis.Landing"), TEXT("Landing"), 1, 3, {Effect(EBreakerNodeStatTarget::SafeFallDistance, EBreakerNodeStatBucket::Flat, 2.0f)}, nullptr);
    Check(TEXT("Core.Kinesis.AirJump"), TEXT("Air Jump"), 2, 1, {Effect(EBreakerNodeStatTarget::AirJumpCount, EBreakerNodeStatBucket::Flat, 1.0f)}, nullptr);
    Check(TEXT("Core.Kinesis.PhantomStep"), TEXT("Phantom Step"), 2, 1, {}, TEXT("Progression.Node.Core.PhantomStep"));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreRosterMovementRuntimeTest,"RiorsEdge.Progression.CoreRoster.MovementRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreRosterMovementRuntimeTest::RunTest(const FString&)
{
    UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); World->InitializeActorsForPlay(FURL());
    const uint64 Frame=GFrameCounter;
    ON_SCOPE_EXIT {World->DestroyWorld(false);GEngine->DestroyWorldContext(World);GFrameCounter=Frame;};
    auto* Player=World->SpawnActor<ABreakerCharacter>(); if (!Player) return false;
    auto* Move=Player->GetBreakerMovement(); Player->SetActorTickEnabled(false); Move->SetComponentTickEnabled(false);
    auto* Attr=Player->GetAttributes(); auto* ASC=Player->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Player,Player); ASC->AddAttributeSetSubobject(Attr);
    auto* Progression=Player->GetProgression(); Progression->BindAttributes(Attr);
    auto* Combat=Player->GetCombat(); Combat->BeginPlay(); Combat->SetComponentTickEnabled(false);
    auto* Class=NewObject<UBreakerClassDefinition>(); Class->ClassId=EBreakerClassId::Caster;
    TArray<FBreakerCoreWedgeDefinition> Wedges; BreakerCoreRoster::AppendMovement(Class,Wedges);
    FString Error; auto* Tree=BreakerCoreTree::Build(Class,TEXT("Test.Core.Movement.Runtime"),FText::FromString(TEXT("Movement")),Wedges,Error);
    if (!Tree) return false; Class->BranchTrees.Add(Tree);
    if (!Progression->ChoosePermanentClass(Class)) return false;
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(30,Progression->ExperienceCurve));
    const float BaseSpeed=Attr->GetMoveSpeed();
    const int32 Wallet=Progression->GetUnspentPoints(Tree->Currency);
    FText Reason;
    auto Buy=[&](const TCHAR* Id) { return TestTrue(Id,Progression->PurchaseNode(Tree,FName(Id),Reason)); };
    if (!Buy(TEXT("Core.Velocity.Grind"))) return false;
    for (const TCHAR* Id : {TEXT("Core.Velocity.Stride"),TEXT("Core.Velocity.Slide"),TEXT("Core.Velocity.Sprint")})
        if (!Buy(Id)) return false;
    for (const TCHAR* Id : {TEXT("Core.Velocity.Momentum"),TEXT("Core.Velocity.Carry"),TEXT("Core.Velocity.Downforce"),TEXT("Core.Velocity.TerminalVelocity")})
        if (!Buy(Id)) return false;
    TestFalse(TEXT("Cheap convergence cannot bypass eighteen-point gate"),Progression->CanPurchaseNode(Tree,TEXT("Core.Velocity.NoGround"),Reason));
    for (const TCHAR* Id : {TEXT("Core.Velocity.Stride"),TEXT("Core.Velocity.Stride"),TEXT("Core.Velocity.Slide"),TEXT("Core.Velocity.Slide"),TEXT("Core.Velocity.Sprint")})
        if (!Buy(Id)) return false;
    TestEqual(TEXT("Optional ranks earn exact keystone gate"),Progression->GetConstellationInvestment(Tree,TEXT("Velocity")),18);
    TestEqual(TEXT("Literal movement effects compose before keystone"),Attr->GetMoveSpeed(),BaseSpeed*1.32f,.01f);
    if (!Buy(TEXT("Core.Velocity.NoGround"))) return false;
    TestEqual(TEXT("Full earned route pays twenty-three points"),Wallet-Progression->GetUnspentPoints(Tree->Currency),23);
    TestEqual(TEXT("No Ground doubles authored positive movement bonuses"),Attr->GetMoveSpeed(),BaseSpeed*1.64f,.01f);
    TestEqual(TEXT("Native walking reads composed movement"),Move->GetWalkSpeedCap(),Move->WalkSpeed*1.64f,.01f);
    const auto& Fold=Attr->GetAttributeAggregator();
    const auto& Contribution=Fold.GetContribution(EBreakerAttributeContributor::Progression);
    TestEqual(TEXT("Momentum adds half composed movement percent to weapon lane"),
        Fold.ComposedIncreasedPercent(EBreakerAggregatedAttribute::DamageMultiplier),
        Contribution.GetIncreasedPercent(EBreakerAggregatedAttribute::DamageMultiplier)+32.f,.001f);
    TestEqual(TEXT("Downforce adds half composed movement percent to ability lane"),
        Fold.ComposedIncreasedPercent(EBreakerAggregatedAttribute::AbilityDamageMultiplier),
        Contribution.GetIncreasedPercent(EBreakerAggregatedAttribute::AbilityDamageMultiplier)+32.f,.001f);
    FBreakerDamageRequest Incoming; Incoming.BaseDamage=10; Incoming.DamageFamily=EBreakerDamageFamily::TrueDamage;
    Incoming.bCanCritical=false; Incoming.bCanBeAvoided=false; Incoming.SetInstigator(World->SpawnActor<AActor>());
    Combat->RestoreVitals();
    TestEqual(TEXT("Earned No Ground charges its real incoming forfeit"),Combat->ReceiveDamage(Incoming).HealthDamage,13.f,.01f);
    const auto RespecCost=BreakerCoreRespecCost(Progression->GetCharacterLevel());
    auto* Equipment=Player->GetEquipment();
    Equipment->GrantForgeCurrency(RespecCost.Amount);
    const int32 CurrencyBefore=Equipment->GetForgeWallet().Get();
    if (!TestTrue(TEXT("Funded actual Core respec succeeds"),Progression->RespecCore(Reason))) return false;
    TestEqual(TEXT("Respec debits exact authored Riftglass price"),Equipment->GetForgeWallet().Get(),CurrencyBefore-RespecCost.Amount);
    TestEqual(TEXT("Respec restores movement base"),Attr->GetMoveSpeed(),BaseSpeed,.01f);
    Combat->RestoreVitals();
    TestEqual(TEXT("Respec removes incoming forfeit"),Combat->ReceiveDamage(Incoming).HealthDamage,10.f,.01f);
    for (const TCHAR* Id : {TEXT("Core.Kinesis.LightFooting"),TEXT("Core.Kinesis.Loft"),TEXT("Core.Kinesis.AirWork"),TEXT("Core.Kinesis.Landing"),TEXT("Core.Kinesis.AirJump"),TEXT("Core.Kinesis.PhantomStep")})
        if (!Buy(Id)) return false;
    TestEqual(TEXT("Rank-one Kinesis route costs nine"),Progression->GetConstellationInvestment(Tree,TEXT("Kinesis")),9);
    TestEqual(TEXT("Landing preserves metre units"),Progression->GetNodeStats().BonusSafeFallDistanceMeters,2.f);
    TestEqual(TEXT("Air Jump grants one count"),Progression->GetNodeStats().BonusAirJumpCount,1.f);
    TestEqual(TEXT("Loft authors height, not impulse percent"),Progression->GetNodeStats().JumpHeightMultiplier,1.06f,.001f);
    TestTrue(TEXT("Earned Phantom Step uses existing traversal permission"),Progression->HasNodeTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.PhantomStep"))));
    return true;
}
#endif
