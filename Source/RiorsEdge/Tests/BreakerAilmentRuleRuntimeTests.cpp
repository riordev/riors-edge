#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbility_Fracture.h"
#include "Combat/BreakerProjectileBase.h"
#include "Combat/BreakerElementReactions.h"
#include "EngineUtils.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerManaComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Combat/BreakerStatusComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerProgressionTree.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerExperience.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerAilmentRuleRuntimeTest, "RiorsEdge.Combat.AilmentRulesRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerAilmentRuleRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated ability world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const uint64 InitialFrame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = InitialFrame; };
    ABreakerCharacter* Caster = World->SpawnActor<ABreakerCharacter>();
    if (!TestNotNull(TEXT("real caster without save-loading BeginPlay"), Caster)) return false;
    UAbilitySystemComponent* ASC = Caster->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Caster, Caster); ASC->AddAttributeSetSubobject(Caster->GetAttributes());
    Caster->GetAttributes()->SetCriticalChance(0);
    Caster->GetCombat()->BindAttributes(Caster->GetAttributes());
    Caster->GetProgression()->BindAttributes(Caster->GetAttributes());
    if (!TestTrue(TEXT("actual permanent Caster selection"), Caster->GetProgression()->ChoosePermanentClassById(EBreakerClassId::Caster))) return false;
    Caster->GetMana()->BindAttributes(Caster->GetAttributes());
    Caster->GetBreakerMovement()->SetComponentTickEnabled(false);
    // Recover through the class's normal passive loop before the paid cast.
    Caster->GetMana()->AdvanceLoop(20.0f);
    ASC->SetNumericAttributeBase(UBreakerAttributeSet::GetCriticalChanceAttribute(), 0.0f);
    AActor* Target = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("real target"), Target)) return false;
    USphereComponent* Body = NewObject<USphereComponent>(Target);
    Target->AddInstanceComponent(Body); Target->SetRootComponent(Body);
    Body->SetSphereRadius(40); Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Body->SetCollisionResponseToAllChannels(ECR_Ignore); Body->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);
    Body->RegisterComponent(); Target->SetActorLocation(FVector(500, 0, 0));
    UBreakerCombatComponent* Combat = NewObject<UBreakerCombatComponent>(Target);
    Target->AddInstanceComponent(Combat); Combat->RegisterComponent();
    UBreakerAttributeSet* Health = NewObject<UBreakerAttributeSet>(Target);
    Health->ApplyMaxHealth(10000); Health->ApplyHealth(10000); Combat->BindAttributes(Health);
    UBreakerStatusComponent* Status = NewObject<UBreakerStatusComponent>(Target);
    Target->AddInstanceComponent(Status); Status->RegisterComponent();
    auto Advance = [&](int32 Steps) { for (int32 I = 0; I < Steps; ++I) { ++GFrameCounter; World->Tick(LEVELTICK_All, .05f); } };
    auto* Definition = DuplicateObject<UBreakerClassDefinition>(Caster->GetProgression()->ClassDefinition, Caster);
    auto* Tree = NewObject<UBreakerProgressionTree>(Definition);
    Tree->TreeId = TEXT("Test.Core.AilmentRules"); Tree->Currency = EBreakerPointCurrency::CorePoints;
    for (const TCHAR* Rule : { TEXT("Density"), TEXT("Hemorrhage"), TEXT("Deepen") })
    {
        auto* Node = NewObject<UBreakerProgressionNode>(Tree);
        Node->NodeId = FName(Rule);
        Node->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(*FString::Printf(TEXT("Progression.Node.Core.%s"), Rule))));
        Tree->Nodes.Add(Node);
    }
    Definition->BranchTrees.Add(Tree); Caster->GetProgression()->ClassDefinition = Definition;
    Caster->GetProgression()->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(5, Caster->GetProgression()->ExperienceCurve));
    FText Reason;
    const auto Handle = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Fracture::StaticClass(), 1));
    const auto Poison = FGameplayTag::RequestGameplayTag(TEXT("Status.Poison"));
    const auto Rot = FGameplayTag::RequestGameplayTag(TEXT("Status.Rot"));
    auto Find = [&](FGameplayTag Tag) -> const FBreakerActiveStatus*
    { return Status->GetActiveStatuses().FindByPredicate([&](const auto& Active) { return Active.Spec.StatusTag == Tag; }); };
    auto PaidPoison = [&]()
    {
        for (int32 Cast = 0; Cast < 4 && !Find(Poison); ++Cast)
        {
            Caster->GetMana()->AdvanceLoop(20);
            TSet<ABreakerProjectileBase*> Existing;
            for (TActorIterator<ABreakerProjectileBase> It(World); It; ++It) Existing.Add(*It);
            const float BeforeMana = Caster->GetMana()->GetMana();
            if (!TestTrue(TEXT("Actual paid Fracture"), ASC->TryActivateAbility(Handle))) return false;
            TestTrue(TEXT("Fracture paid normal Mana"), Caster->GetMana()->GetMana() < BeforeMana);
            ABreakerProjectileBase* Shot = nullptr;
            for (TActorIterator<ABreakerProjectileBase> It(World); It; ++It) if (!Existing.Contains(*It)) { Shot = *It; break; }
            if (!TestNotNull(TEXT("Native projectile"), Shot)) return false;
            Shot->Impact(Target, Target->GetActorLocation());
        }
        return TestNotNull(TEXT("Actual cycle earns physical Poison"), Find(Poison));
    };
    if (!PaidPoison()) return false;
    const FBreakerActiveStatus Baseline = *Find(Poison);
    for (int32 I = 0; I < 15; ++I) Status->ApplyStatus(Baseline.Spec, EBreakerDamageFamily::Physical, Caster);
    TestEqual(TEXT("Ordinary source retains ten stack cap"), Find(Poison)->Stacks, 10);
    Status->ConsumeAllStatuses();
    if (!TestTrue(TEXT("Earned Deepen schema purchase"), Caster->GetProgression()->PurchaseNode(Tree, TEXT("Deepen"), Reason))) return false;
    if (!PaidPoison()) return false;
    const FBreakerStatusApplicationSpec DeepSpec = Find(Poison)->Spec;
    for (int32 I = 0; I < 15; ++I) Status->ApplyStatus(DeepSpec, EBreakerDamageFamily::Physical, Caster);
    TestEqual(TEXT("Source Deepen adds exactly one to ordinary cap"), Find(Poison)->Stacks, 11);
    Status->ConsumeAllStatuses();
    if (!TestTrue(TEXT("Earned Hemorrhage schema purchase"), Caster->GetProgression()->PurchaseNode(Tree, TEXT("Hemorrhage"), Reason))) return false;
    if (!PaidPoison()) return false;
    const FBreakerActiveStatus Fast = *Find(Poison);
    TestEqual(TEXT("Physical ailment half duration"), Fast.RemainingDuration, Baseline.RemainingDuration * .5f);
    TestEqual(TEXT("Physical ailment half interval"), Fast.Spec.TickInterval, Baseline.Spec.TickInterval * .5f);
    Status->AdvanceStatuses(.1f);
    const float Remaining = Find(Poison)->RemainingDuration;
    Status->ApplyStatus(Fast.Spec, EBreakerDamageFamily::Physical, Caster);
    TestEqual(TEXT("Active Hemorrhage cannot refresh"), Find(Poison)->RemainingDuration, Remaining);
    TestEqual(TEXT("Refused refresh cannot add a stack"), Find(Poison)->Stacks, 1);
    Status->ConsumeAllStatuses();
    auto Copy = Fast.Spec; Copy.Duration = Remaining; Copy.ProcCoefficient = 0;
    Status->ApplyPierceSpread(Copy, EBreakerDamageFamily::Physical, Caster);
    TestEqual(TEXT("Spread does not halve interval again"), Find(Poison)->Spec.TickInterval, Fast.Spec.TickInterval);
    TestEqual(TEXT("Spread retains remaining duration exactly"), Find(Poison)->RemainingDuration, Remaining);
    Status->ConsumeAllStatuses();
    if (!TestTrue(TEXT("Earned Density schema purchase"), Caster->GetProgression()->PurchaseNode(Tree, TEXT("Density"), Reason))) return false;
    FBreakerDamageRequest Hit;
    Hit.SetInstigator(Caster); Hit.BaseDamage = 1000; Hit.Element = EBreakerElement::Entropy;
    Hit.ElementalFraction = 1; Hit.bCanCritical = false;
    UBreakerDamageLibrary::SnapshotElementSource(Caster, Hit);
    Combat->ReceiveDamage(Hit);
    if (!TestNotNull(TEXT("Accepted Entropy threshold earns Rot"), Find(Rot))) return false;
    TestTrue(TEXT("Density ten ticks over four seconds"), FMath::IsNearlyEqual(Find(Rot)->Spec.TickInterval, .4f));
    TestEqual(TEXT("Same earned finite Rot budget"), Find(Rot)->UnpaidDamageBudget, 500.0f);
    Status->AdvanceStatuses(1.21f);
    TestEqual(TEXT("Three faster ticks paid"), Find(Rot)->TicksDelivered, 3);
    bool bConsumed = false;
    const auto Consumed = Status->ConsumeStatus(Rot, bConsumed);
    TestTrue(TEXT("Partial Rot consumed"), bConsumed);
    TestTrue(TEXT("Consumed budget excludes three delivered ticks"), FMath::IsNearlyEqual(Consumed.UnpaidDamageBudget, 350.0f, .01f));
    const float AfterConsume = Health->GetHealth();
    Status->AdvanceStatuses(5);
    TestEqual(TEXT("Consumed Rot cannot pay again"), Health->GetHealth(), AfterConsume);
    Combat->ReceiveDamage(Hit);
    if (!TestNotNull(TEXT("A fresh earned Rot exists"), Find(Rot))) return false;
    const float BeforeFullRot = Health->GetHealth();
    Status->AdvanceStatuses(4.01f);
    TestFalse(TEXT("Density retains original four-second expiry"), Status->HasStatus(Rot));
    TestTrue(TEXT("All ten ticks pay the same total budget"), FMath::IsNearlyEqual(BeforeFullRot - Health->GetHealth(), 500.0f, .01f));
    return true;
}
#endif