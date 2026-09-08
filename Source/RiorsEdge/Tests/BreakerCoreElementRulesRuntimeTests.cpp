#include "Tests/BreakerElementRuleRuntimeObserver.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Combat/BreakerEnemy.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerProgressionTree.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerExperience.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

void UBreakerElementRuleRuntimeObserver::OnHit(const FBreakerHitContext& Hit)
{
    Hits.Add(Hit);
    if (bRepeatRift && Status && Hit.DamageTypeTag == FGameplayTag::RequestGameplayTag(TEXT("Status.Unstable")))
    {
        bRepeatRift = false;
        if (!Status->GetActiveStatuses().IsEmpty()) Status->FlushRiftActivation(Status->GetActiveStatuses()[0].ApplicationSerial);
    }
    if (bReenterVoid && Combat && Status && Hit.DamageTypeTag == FGameplayTag::RequestGameplayTag(TEXT("Status.Erased")))
    {
        bReenterVoid = false;
        FBreakerDamageRequest Nested; Nested.BaseDamage = 20; Nested.bCanCritical = false;
        Combat->ReceiveDamage(Nested);
        for (const auto& Active : Status->GetActiveStatuses())
            if (Active.Spec.StatusTag == FGameplayTag::RequestGameplayTag(TEXT("Status.Erased"))) BudgetAfterReentry = Active.UnpaidDamageBudget;
    }
}
void UBreakerElementRuleRuntimeObserver::OnConsumed(const FBreakerActiveStatus& Active)
{
    if (Active.Spec.StatusTag == FGameplayTag::RequestGameplayTag(TEXT("Status.Erased"))) ConsumedBudget = Active.UnpaidDamageBudget;
}

#if WITH_DEV_AUTOMATION_TESTS
namespace BreakerCoreElementRulesFixture
{
    struct FTarget { AActor* Actor = nullptr; UBreakerCombatComponent* Combat = nullptr; UBreakerStatusComponent* Status = nullptr; UBreakerAttributeSet* Health = nullptr; };
    struct FFixture
    {
        UWorld* World = nullptr;
        ABreakerCharacter* Player = nullptr;
        UBreakerProgressionTree* Tree = nullptr;
        FFixture()
        {
            UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
            World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
            if (!World) return;
            GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); World->InitializeActorsForPlay(FURL());
            Player = World->SpawnActor<ABreakerCharacter>(); if (!Player) return;
            auto* ASC = Player->GetAbilitySystemComponent(); ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Player->GetAttributes());
            Player->GetCombat()->BindAttributes(Player->GetAttributes()); Player->GetProgression()->BindAttributes(Player->GetAttributes());
            if (!Player->GetProgression()->ChoosePermanentClassById(EBreakerClassId::Caster)) return;
            auto* Definition = DuplicateObject<UBreakerClassDefinition>(Player->GetProgression()->ClassDefinition, Player);
            Tree = NewObject<UBreakerProgressionTree>(Definition); Tree->TreeId = TEXT("Test.Core.ElementRules"); Tree->Currency = EBreakerPointCurrency::CorePoints;
            Definition->BranchTrees.Add(Tree); Player->GetProgression()->ClassDefinition = Definition;
            Player->GetProgression()->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(4, Player->GetProgression()->ExperienceCurve));
        }
        ~FFixture() { if (World) { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); } }
        bool Purchase(const TCHAR* Name)
        {
            const FName Id(*FString::Printf(TEXT("Test.Core.%s"), Name));
            if (!Tree->Nodes.ContainsByPredicate([Id](const UBreakerProgressionNode* Node) { return Node->NodeId == Id; }))
            {
                auto* Node = NewObject<UBreakerProgressionNode>(Tree); Node->NodeId = Id;
                Node->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(*FString::Printf(TEXT("Progression.Node.Core.%s"), Name))); Tree->Nodes.Add(Node);
            }
            FText Reason; return Player->GetProgression()->PurchaseNode(Tree, Id, Reason);
        }
        bool Respec() { FText Reason; return Player->GetProgression()->RespecCore(Reason); }
        FBreakerDamageRequest Emit(EBreakerElement Element, FVector Source = FVector(-500, 0, 92))
        {
            FBreakerDamageRequest Hit; Hit.BaseDamage = 100; Hit.Element = Element; Hit.ElementalFraction = 1; Hit.DamageFamily = EBreakerDamageFamily::Elemental;
            Hit.bCanCritical = false; Hit.SetInstigator(Player); Hit.SourceLocation = Source; Hit.bHasSourceLocation = true;
            UBreakerDamageLibrary::FillSourcePools(Player->GetAttributes(), EBreakerDamageDelivery::Ability, Hit); return Hit;
        }
        FTarget MakeTarget()
        {
            FTarget T; T.Actor = World->SpawnActor<AActor>();
            auto* Root = NewObject<USceneComponent>(T.Actor); T.Actor->AddInstanceComponent(Root); T.Actor->SetRootComponent(Root); Root->RegisterComponent();
            T.Combat = NewObject<UBreakerCombatComponent>(T.Actor); T.Actor->AddInstanceComponent(T.Combat); T.Combat->RegisterComponent();
            T.Health = NewObject<UBreakerAttributeSet>(T.Actor); T.Health->ApplyMaxHealth(1000); T.Health->ApplyHealth(1000); T.Combat->BindAttributes(T.Health);
            T.Status = NewObject<UBreakerStatusComponent>(T.Actor); T.Actor->AddInstanceComponent(T.Status); T.Status->RegisterComponent(); T.Status->SetComponentTickEnabled(false); return T;
        }
        FTarget MakeEnemy(FVector Origin)
        {
            FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            auto* Enemy = World->SpawnActor<ABreakerEnemy>(Origin + FVector(0,0,200), FRotator::ZeroRotator, Spawn);
            FTarget T; T.Actor = Enemy; T.Combat = Enemy->FindComponentByClass<UBreakerCombatComponent>(); T.Status = Enemy->FindComponentByClass<UBreakerStatusComponent>();
            T.Health = FindObject<UBreakerAttributeSet>(Enemy, TEXT("Attributes"));
            auto* ASC = Enemy->GetAbilitySystemComponent(); ASC->InitAbilityActorInfo(Enemy, Enemy); ASC->AddAttributeSetSubobject(T.Health);
            T.Health->ApplyMaxHealth(1000); T.Health->ApplyHealth(1000); T.Combat->BindAttributes(T.Health); T.Status->SetComponentTickEnabled(false);
            const auto* Capsule = Cast<UCapsuleComponent>(Enemy->GetRootComponent()); Enemy->SetActorLocation(Origin + FVector(0,0,Capsule->GetScaledCapsuleHalfHeight()+2)); return T;
        }
        AActor* Box(FVector Position, FVector Extent)
        {
            auto* Actor = World->SpawnActor<AActor>(); auto* Shape = NewObject<UBoxComponent>(Actor); Actor->AddInstanceComponent(Shape); Actor->SetRootComponent(Shape);
            Shape->SetBoxExtent(Extent); Shape->SetCollisionObjectType(ECC_WorldStatic); Shape->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            Shape->SetCollisionResponseToAllChannels(ECR_Block); Shape->RegisterComponent(); Actor->SetActorLocation(Position); return Actor;
        }
        UBreakerElementRuleRuntimeObserver* Observe(const FTarget& T)
        {
            auto* Observer = NewObject<UBreakerElementRuleRuntimeObserver>(T.Actor); Observer->Combat = T.Combat; Observer->Status = T.Status;
            T.Combat->OnDamageTaken.AddDynamic(Observer, &UBreakerElementRuleRuntimeObserver::OnHit);
            T.Status->OnStatusConsumed.AddDynamic(Observer, &UBreakerElementRuleRuntimeObserver::OnConsumed); return Observer;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreVoidRulesRuntimeTest, "RiorsEdge.Combat.Void.CoreRulesRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreVoidRulesRuntimeTest::RunTest(const FString& Parameters)
{
    using namespace BreakerCoreElementRulesFixture;
    FFixture F; if (!TestNotNull(TEXT("native schema world"), F.Tree)) return false;
    const FTarget T = F.MakeTarget(); auto* Observer = F.Observe(T);
    auto Reset = [&]() { T.Status->ConsumeAllStatuses(); T.Status->AdvanceStatuses(20); T.Health->ApplyMaxShield(0); T.Combat->RestoreVitals(); Observer->Hits.Reset(); };
    const auto Erased = FGameplayTag::RequestGameplayTag(TEXT("Status.Erased"));
    auto Active = [&]() { return T.Status->GetActiveStatuses().FindByPredicate([Erased](const FBreakerActiveStatus& E) { return E.Spec.StatusTag == Erased; }); };
    T.Combat->ReceiveDamage(F.Emit(EBreakerElement::Void)); T.Status->AdvanceStatuses(2);
    TestEqual(TEXT("baseline delay and coefficient unchanged"), T.Health->GetHealth(), 850.0f, .001f);
    Reset(); if (!TestTrue(TEXT("earned Patience purchase"), F.Purchase(TEXT("Patience")))) return false;
    T.Combat->ReceiveDamage(F.Emit(EBreakerElement::Void));
    if (!TestNotNull(TEXT("Patience earns delayed mark"), Active())) return false;
    TestEqual(TEXT("Patience changes authored delay to three seconds"), Active()->RemainingDuration, 3.0f);
    TestEqual(TEXT("Patience coefficient includes single spent-point floor"), Active()->UnpaidDamageBudget, 70.175f, .001f);
    TestEqual(TEXT("rule rewrite creates no selected More slot"), F.Player->GetAttributes()->GetAttributeAggregator().GetSelectedDamageMoreSourceCount(), 0);
    T.Status->AdvanceStatuses(2); TestEqual(TEXT("Patience does not pay baseline expiry"), T.Health->GetHealth(), 899.75f, .001f);
    if (!TestTrue(TEXT("actual respec during delay"), F.Respec())) return false;
    T.Status->AdvanceStatuses(1); TestEqual(TEXT("snapshotted Patience pays after respec"), T.Health->GetHealth(), 829.575f, .001f);
    Reset(); if (!F.Purchase(TEXT("Patience")) || !F.Purchase(TEXT("Debt"))) return false;
    const auto Stored = F.Emit(EBreakerElement::Void); T.Combat->ReceiveDamage(Stored);
    if (!TestNotNull(TEXT("combined rules create Erased"), Active())) return false;
    TestEqual(TEXT("applying hit is never added as Debt"), Active()->UnpaidDamageBudget, 70.35f, .001f);
    T.Health->ApplyMaxShield(50); T.Health->ApplyShield(50);
    FBreakerDamageRequest Other; Other.BaseDamage = 100; Other.bCanCritical = false; Other.SetInstigator(F.World->SpawnActor<AActor>());
    const auto Paid = T.Combat->ReceiveDamage(Other);
    TestEqual(TEXT("other attacker really pays shield"), Paid.ShieldDamage, 50.0f);
    TestEqual(TEXT("other attacker really pays health"), Paid.HealthDamage, 50.0f);
    TestEqual(TEXT("Debt uses accepted total only once"), Active()->UnpaidDamageBudget, 85.35f, .001f);
    FBreakerStatusApplicationSpec Poison; Poison.StatusTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Poison"));
    Poison.Duration = .5f; Poison.TickInterval = .5f; Poison.BaseDamagePerTick = 10; Poison.ProcCoefficient = 0; Poison.Snapshot.SourcePower = 1;
    T.Status->ApplyStatus(Poison, EBreakerDamageFamily::Physical, Other.Instigator.Get()); T.Status->AdvanceStatuses(.5f);
    TestEqual(TEXT("other status damage earns finite Debt"), Active()->UnpaidDamageBudget, 86.85f, .001f);
    if (!TestTrue(TEXT("removal of owned Debt is a real respec"), F.Respec())) return false;
    Other.BaseDamage = 20; T.Combat->ReceiveDamage(Other);
    TestEqual(TEXT("existing delay retains Debt snapshot"), Active()->UnpaidDamageBudget, 89.85f, .001f);
    FBreakerDamageRequest Tear; Tear.BaseDamage = 1; Tear.Element = EBreakerElement::Rift; Tear.ElementalFraction = 1; Tear.bCanCritical = false;
    Observer->Hits.Reset(); T.Combat->ReceiveDamage(Tear);
    const auto* Reaction = Observer->Hits.FindByPredicate([](const FBreakerHitContext& H) { return H.DamageTypeTag == FGameplayTag::RequestGameplayTag(TEXT("Reaction.Tear")); });
    if (!TestNotNull(TEXT("Tear settles existing Erased"), Reaction)) return false;
    TestEqual(TEXT("Tear transfers funded original plus accepted Debt including trigger"), Reaction->Result.RawDamage, 90.0f, .001f);
    const float Settled = T.Health->GetHealth(); T.Status->AdvanceStatuses(10); TestEqual(TEXT("transferred debt cannot pay later"), T.Health->GetHealth(), Settled);
    Reset(); T.Combat->ReceiveDamage(Stored); Observer->bReenterVoid = true; T.Status->AdvanceStatuses(3);
    TestEqual(TEXT("expiry claims budget before nested hit"), Observer->BudgetAfterReentry, 0.0f);
    TestEqual(TEXT("expiry plus nested hit contains no refunded Debt"), T.Health->GetHealth(), 809.15f, .001f);
    TestFalse(TEXT("expiry removes mark"), T.Status->HasStatus(Erased));
    Reset(); T.Combat->ReceiveDamage(Stored); T.Combat->DodgeChance = 1; Other.BaseDamage = 100;
    TestTrue(TEXT("actual dodge refuses incoming debt damage"), T.Combat->ReceiveDamage(Other).bDodged);
    TestEqual(TEXT("avoided damage adds no Debt"), Active()->UnpaidDamageBudget, 70.35f, .001f); T.Combat->DodgeChance = 0;
    Other.BaseDamage = 10000; T.Combat->ReceiveDamage(Other);
    TestEqual(TEXT("lethal overkill cannot mint debt before death cleanup"), Observer->ConsumedBudget, 70.35f, .001f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreRiftRulesRuntimeTest, "RiorsEdge.Combat.Rift.CoreRulesRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreRiftRulesRuntimeTest::RunTest(const FString& Parameters)
{
    using namespace BreakerCoreElementRulesFixture;
    FFixture F; if (!TestNotNull(TEXT("native Rift schema world"), F.Tree)) return false;
    auto Lane = [&](int32 Index, float FloorHalf = 500.0f) { const FVector Origin(0, Index*2000, 0); F.Box(Origin-FVector(0,0,25), FVector(FloorHalf,500,25)); return Origin; };
    const FVector Open = Lane(1); const FTarget Base = F.MakeEnemy(Open);
    FBreakerDamageRequest Request = F.Emit(EBreakerElement::Rift, Open+FVector(-500,0,92)); Base.Combat->ReceiveDamage(Request);
    TestEqual(TEXT("baseline native enemy moves away"), Base.Actor->GetActorLocation().X, 200.0, .01);
    if (!TestTrue(TEXT("earned Vector Field purchase"), F.Purchase(TEXT("VectorField")))) return false;
    Base.Status->ConsumeAllStatuses(); Base.Combat->RestoreVitals(); Base.Actor->SetActorLocation(Open+FVector(0,0,92));
    Base.Combat->ReceiveDamage(F.Emit(EBreakerElement::Rift, Open+FVector(-500,0,92)));
    TestEqual(TEXT("Vector Field reverses actual swept movement toward snapshot"), Base.Actor->GetActorLocation().X, -200.0, .01);
    if (!F.Respec() || !F.Purchase(TEXT("Impact")) || !F.Purchase(TEXT("HardLanding"))) return false;
    Request = F.Emit(EBreakerElement::Rift);
    if (!TestTrue(TEXT("actual respec after projectile source snapshot"), F.Respec())) return false;
    const FVector WallLane = Lane(2); F.Box(WallLane+FVector(120,0,150), FVector(10,500,150)); const FTarget Wall = F.MakeEnemy(WallLane);
    Request.SourceLocation = WallLane+FVector(-500,0,92); auto* Observer = F.Observe(Wall); Observer->bRepeatRift = true; Wall.Combat->ReceiveDamage(Request);
    TestTrue(TEXT("wall approached without tunneling"), Wall.Actor->GetActorLocation().X > 0 && Wall.Actor->GetActorLocation().X < 70);
    TestEqual(TEXT("Hard Landing pays quarter of original burst exactly once after snapshot respec"), Wall.Health->GetHealth(), 836.6875f, .001f);
    int32 BurstCount = 0; for (const auto& H : Observer->Hits) if (H.DamageTypeTag == FGameplayTag::RequestGameplayTag(TEXT("Status.Unstable"))) ++BurstCount;
    TestEqual(TEXT("reentrant flush cannot repeat main or wall payment"), BurstCount, 2);
    const FVector LedgeLane = Lane(3,100); F.Box(LedgeLane+FVector(180,0,150), FVector(10,500,150)); const FTarget Ledge = F.MakeEnemy(LedgeLane); Request.SourceLocation = LedgeLane+FVector(-500,0,92); Ledge.Combat->ReceiveDamage(Request);
    TestTrue(TEXT("ledge truncates native movement"), Ledge.Actor->GetActorLocation().X < 100);
    TestEqual(TEXT("support ends before the wall so Hard Landing pays no bonus"), Ledge.Health->GetHealth(), 849.25f, .001f);
    const FVector ImpactLane = Lane(4); const FTarget First = F.MakeEnemy(ImpactLane), Second = F.MakeEnemy(ImpactLane+FVector(150,0,0)), Third = F.MakeEnemy(ImpactLane+FVector(300,0,0));
    auto* ImpactObserver = F.Observe(Second); Request.SourceLocation = ImpactLane+FVector(-500,0,92); First.Combat->ReceiveDamage(Request);
    TestTrue(TEXT("actual displacement reaches or crosses the first enemy"), First.Actor->GetActorLocation().X > 0 && First.Actor->GetActorLocation().X <= 200.01);
    TestEqual(TEXT("blocking enemy receives one original burst"), Second.Health->GetHealth(), 949.75f, .001f);
    TestEqual(TEXT("further enemy receives no chain"), Third.Health->GetHealth(), 1000.0f, .001f);
    TestEqual(TEXT("pawn obstruction is not Hard Landing wall"), First.Health->GetHealth(), 849.25f, .001f);
    if (!TestEqual(TEXT("exactly one secondary payment"), ImpactObserver->Hits.Num(), 1)) return false;
    TestEqual(TEXT("secondary payment cannot proc"), ImpactObserver->Hits[0].ProcCoefficient, 0.0f);
    TestFalse(TEXT("secondary payment cannot crit"), ImpactObserver->Hits[0].Result.bCritical);
    TestEqual(TEXT("secondary payment creates no buildup"), Second.Status->GetRiftBuildup(), 0.0f);
    const FVector TieLane = Lane(6); const FTarget TieMain = F.MakeEnemy(TieLane);
    const FTarget TieA = F.MakeEnemy(TieLane+FVector(150,50,0)), TieB = F.MakeEnemy(TieLane+FVector(150,-50,0));
    Request.SourceLocation = TieLane+FVector(-500,0,92); TieMain.Combat->ReceiveDamage(Request);
    const bool bAFirst = TieA.Actor->GetUniqueID() < TieB.Actor->GetUniqueID();
    TestEqual(TEXT("equal-distance Impact uses stable actor identity"), (bAFirst ? TieA : TieB).Health->GetHealth(), 949.75f, .001f);
    TestEqual(TEXT("equal-distance alternative receives no second Impact"), (bAFirst ? TieB : TieA).Health->GetHealth(), 1000.0f, .001f);
    const FVector ImmuneLane = Lane(5); const FTarget Immune = F.MakeEnemy(ImmuneLane), NearImmune = F.MakeEnemy(ImmuneLane+FVector(150,0,0));
    Immune.Combat->GrantStaggerImmunity(10); Request.SourceLocation = ImmuneLane+FVector(-500,0,92); Immune.Combat->ReceiveDamage(Request);
    TestEqual(TEXT("immunity suppresses displacement"), Immune.Actor->GetActorLocation().X, 0.0, .01);
    TestEqual(TEXT("immunity still pays ordinary earned burst"), Immune.Health->GetHealth(), 849.25f, .001f);
    TestEqual(TEXT("no displacement means no Impact victim"), NearImmune.Health->GetHealth(), 1000.0f, .001f);
    return true;
}
#endif
