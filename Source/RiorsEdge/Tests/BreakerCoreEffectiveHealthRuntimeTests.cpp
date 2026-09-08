#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreEffectiveHealthRuntimeTest,
    "RiorsEdge.Progression.CoreEffectiveHealthRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerCoreEffectiveHealthRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
        ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    World->InitializeActorsForPlay(FURL());
    auto* Player = World->SpawnActor<ABreakerCharacter>();
    if (!Player) return false;
    auto* Attr = Player->GetAttributes();
    auto* ASC = Player->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Player, Player);
    ASC->AddAttributeSetSubobject(Attr);
    auto* Combat = Player->GetCombat();
    Combat->BindAttributes(Attr);
    auto* Progression = Player->GetProgression();
    Progression->BindAttributes(Attr);
    auto* Tree = NewObject<UBreakerProgressionTree>();
    Tree->TreeId = TEXT("Test.EffectiveHealth.Schema");
    Tree->Currency = EBreakerPointCurrency::CorePoints;
    for (int32 Index = 0; Index < 4; ++Index)
    {
        auto* Node = NewObject<UBreakerProgressionNode>(Tree);
        Node->NodeId = FName(*FString::Printf(TEXT("Test.EffectiveHealth.Source%d"), Index));
        Node->Currency = Tree->Currency;
        FBreakerNodeEffect Effect;
        Effect.StatTarget = Index == 0 ? EBreakerNodeStatTarget::EffectiveHealth : EBreakerNodeStatTarget::Damage;
        Effect.StatBucket = EBreakerNodeStatBucket::MorePercent;
        Effect.ValuePerRank = 20.0f + Index;
        Node->Effects.Add(Effect);
        Tree->Nodes.Add(Node);
    }
    auto* Class = NewObject<UBreakerClassDefinition>();
    Class->ClassId = EBreakerClassId::Caster;
    Class->BranchTrees.Add(Tree);
    if (!TestTrue(TEXT("Temporary primitive schema selected"), Progression->ChoosePermanentClass(Class))) return false;
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(5, Progression->ExperienceCurve));
    Combat->RestoreVitals();
    FBreakerDamageRequest Injury;
    Injury.BaseDamage = 10.0f;
    Injury.DamageFamily = EBreakerDamageFamily::TrueDamage;
    Injury.bBypassShield = true;
    Injury.bCanBeAvoided = false;
    Injury.bCanCritical = false;
    Combat->ReceiveDamage(Injury);
    const float InitialHealth = Attr->GetHealth();
    const float InitialMaxHealth = Attr->GetMaxHealth();
    const float InitialMaxShield = Attr->GetMaxShield();
    FText Reason;
    if (!TestTrue(TEXT("Earned point buys effective-health source"), Progression->PurchaseNode(Tree, Tree->Nodes[0]->NodeId, Reason))) return false;
    TestEqual(TEXT("Purchase creates no health capacity"), Attr->GetMaxHealth(), InitialMaxHealth);
    TestEqual(TEXT("Purchase creates no shield capacity"), Attr->GetMaxShield(), InitialMaxShield);
    TestEqual(TEXT("Purchase grants no healing"), Attr->GetHealth(), InitialHealth);
    TestEqual(TEXT("Purchase creates no front pool"), Combat->GetFrontShield(), 0.0f);

    // Explicit observation pools exercise the real front -> ward -> health
    // resolver. They are fixture capacity, never granted by the More itself.
    auto HitPools = [&](EBreakerDamageFamily Family)
    {
        Combat->RestoreVitals();
        ASC->SetNumericAttributeBase(UBreakerAttributeSet::GetMaxShieldAttribute(), 10.0f);
        ASC->SetNumericAttributeBase(UBreakerAttributeSet::GetShieldAttribute(), 10.0f);
        Combat->ArmFrontShield(10.0f);
        FBreakerDamageRequest Hit;
        Hit.BaseDamage = 60.0f;
        Hit.DamageFamily = Family;
        Hit.ArmorPenetration = 10000.0f;
        Hit.bCanBeAvoided = false;
        Hit.bCanCritical = false;
        Hit.bCanApplyElementBuildup = false;
        Hit.bHasSourceLocation = true;
        Hit.SourceLocation = Player->GetActorLocation() + Player->GetActorForwardVector() * 100.0f;
        return Combat->ReceiveDamage(Hit);
    };
    for (auto Family : { EBreakerDamageFamily::Physical, EBreakerDamageFamily::Elemental, EBreakerDamageFamily::TrueDamage })
    {
        const auto Result = HitPools(Family);
        TestEqual(TEXT("Effective-health More divides actual accepted damage"), Result.HealthDamage + Result.ShieldDamage, 50.0f, .001f);
        TestEqual(TEXT("Both observation shield pools are spent before health"), Result.ShieldDamage, 20.0f, .001f);
        TestEqual(TEXT("Reduced spill reaches health"), Result.HealthDamage, 30.0f, .001f);
        TestEqual(TEXT("Front pool actually spent"), Combat->GetFrontShield(), 0.0f);
        TestEqual(TEXT("Ward actually spent"), Attr->GetShield(), 0.0f);
    }
    for (int32 Index = 1; Index < 4; ++Index)
        if (!TestTrue(TEXT("Earned points buy competing offensive sources"), Progression->PurchaseNode(Tree, Tree->Nodes[Index]->NodeId, Reason))) return false;
    TestEqual(TEXT("Three stronger offensive sources displace defensive More"),
        HitPools(EBreakerDamageFamily::TrueDamage).HealthDamage, 40.0f, .001f);
    if (!TestTrue(TEXT("Actual Core respec succeeds"), Progression->RespecCore(Reason))) return false;
    TestEqual(TEXT("Respec leaves original pool durability"), HitPools(EBreakerDamageFamily::Physical).HealthDamage, 40.0f, .001f);
    if (!TestTrue(TEXT("Refunded earned point can repurchase defensive source"), Progression->PurchaseNode(Tree, Tree->Nodes[0]->NodeId, Reason))) return false;
    TestEqual(TEXT("Defensive source pays again once slot is available"), HitPools(EBreakerDamageFamily::TrueDamage).HealthDamage, 30.0f, .001f);
    return true;
}
#endif
