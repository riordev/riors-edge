#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreRecoveryRuntimeTest,"RiorsEdge.Progression.CoreRecoveryRuntime",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreRecoveryRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    World->InitializeActorsForPlay(FURL());
    auto Advance = [&](int32 Frames) { for (int32 Frame=0; Frame<Frames; ++Frame) World->Tick(LEVELTICK_All,.05f); };
    auto* Player=World->SpawnActor<ABreakerCharacter>();
    // Recovery integration fixture: advance clocks without floorless movement or actor lifecycle work.
    Player->SetActorTickEnabled(false); Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    auto* Controller=World->SpawnActor<APlayerController>(); Controller->Possess(Player);
    // This isolated world has no GameMode to allocate the controller/player state.
    // Match the existing PlayerRecoveryTick fixture and the native player eligibility gate.
    Player->SetPlayerState(World->SpawnActor<APlayerState>());
    if (!TestTrue(TEXT("Native recovery subject is player controlled"),Player->IsPlayerControlled())) return false;
    auto* Attr=Player->GetAttributes(); auto* Combat=Player->GetCombat(); auto* Progression=Player->GetProgression();
    auto* ASC=Player->GetAbilitySystemComponent(); ASC->InitAbilityActorInfo(Player,Player); ASC->AddAttributeSetSubobject(Attr);
    Combat->BindAttributes(Attr); Progression->BindAttributes(Attr);
    auto* Tree=NewObject<UBreakerProgressionTree>(); Tree->TreeId=TEXT("Test.Recovery.Schema"); Tree->Currency=EBreakerPointCurrency::CorePoints;
    auto* Node=NewObject<UBreakerProgressionNode>(Tree); Node->NodeId=TEXT("Test.Recovery.Numeric"); Node->Currency=Tree->Currency;
    auto Add=[&](EBreakerNodeStatTarget Target,float Value,EBreakerNodeStatBucket Bucket)
    { FBreakerNodeEffect E; E.StatTarget=Target; E.StatBucket=Bucket; E.ValuePerRank=Value; Node->Effects.Add(E); };
    Add(EBreakerNodeStatTarget::HealingReceived,45,EBreakerNodeStatBucket::IncreasedPercent);
    Add(EBreakerNodeStatTarget::HealthRegenPercentMaxHealth,1.2f,EBreakerNodeStatBucket::Flat);
    Add(EBreakerNodeStatTarget::ShieldRechargeDelayReduction,1.2f,EBreakerNodeStatBucket::Flat);
    Tree->Nodes.Add(Node);
    auto* Rule=NewObject<UBreakerProgressionNode>(Tree); Rule->NodeId=TEXT("Test.Recovery.SecondLife"); Rule->Currency=Tree->Currency;
    Rule->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.SecondLife"))); Tree->Nodes.Add(Rule);
    auto* Class=NewObject<UBreakerClassDefinition>(); Class->ClassId=EBreakerClassId::Caster; Class->BranchTrees.Add(Tree);
    if (!Progression->ChoosePermanentClass(Class)) return false;
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(3,Progression->ExperienceCurve));
    FText Reason; if (!TestTrue(TEXT("Earned schema point purchases Recovery effects"),Progression->PurchaseNode(Tree,Node->NodeId,Reason))) return false;
    Attr->ApplyHealth(Attr->GetMaxHealth()*.25f);
    FBreakerHealRequest Heal; Heal.Amount=10;
    TestEqual(TEXT("Received healing amplifies once"),Combat->ApplyHealing(Heal).HealthHealed,14.5f,.001f);
    // Conversion is an already-earned amount; even if a callback injured the
    // recipient meanwhile, it cannot become another heal or receive amplification.
    Attr->ApplyMaxShield(100); Attr->ApplyShield(0);
    Heal.bConversionOnly=true; Heal.bOverhealToShield=true;
    const auto Conversion=Combat->ApplyHealing(Heal);
    TestEqual(TEXT("Conversion cannot heal again"),Conversion.HealthHealed,0.0f);
    TestEqual(TEXT("Conversion does not amplify twice"),Conversion.ShieldGranted,10.0f,.001f);
    Attr->ApplyHealth(Attr->GetMaxHealth()*.25f);
    const float Before=Attr->GetHealth();
    // Advance world clock without beginning the character (no owner save load).
    Advance(100);
    if (!TestTrue(TEXT("Clock advance retains live player eligibility"),Player->IsPlayerControlled() && Player->GetController()==Controller && !Combat->IsDead())) return false;
    const float AtTick=Attr->GetHealth();
    Combat->TickComponent(1,LEVELTICK_All,nullptr);
    TestEqual(TEXT("Existing passive payout includes max-health rate"),Attr->GetHealth()-AtTick,Attr->GetMaxHealth()*(.02f+.012f),.001f);
    TestTrue(TEXT("Passive recovery never reduces health"),Attr->GetHealth()>=Before);
    FBreakerDamageRequest Hit; Hit.BaseDamage=1; Hit.bCanBeAvoided=false; Hit.bCanCritical=false; Hit.bBypassShield=true;
    Combat->ReceiveDamage(Hit); const float CombatHealth=Attr->GetHealth();
    Combat->TickComponent(1,LEVELTICK_All,nullptr);
    TestEqual(TEXT("Ordinary regeneration stops in combat"),Attr->GetHealth(),CombatHealth);
    if (!TestTrue(TEXT("Second earned point purchases Second Life"),Progression->PurchaseNode(Tree,Rule->NodeId,Reason))) return false;
    const float SecondBefore=Attr->GetHealth(); Combat->TickComponent(1,LEVELTICK_All,nullptr);
    TestEqual(TEXT("Second Life pays half the same composed regeneration"),Attr->GetHealth()-SecondBefore,(Attr->GetMaxHealth()*(.02f+.012f))*.5f,.001f);
    Attr->ApplyMaxShield(100); Attr->ApplyShield(0); Advance(58);
    const float ShieldBefore=Attr->GetShield(); Combat->TickComponent(.1f,LEVELTICK_All,nullptr);
    TestTrue(TEXT("Purchased delay allows existing shield recharge before four seconds"),Attr->GetShield()>ShieldBefore);
    if (!TestTrue(TEXT("Actual Core respec succeeds"),Progression->RespecCore(Reason))) return false;
    Combat->ReceiveDamage(Hit); Attr->ApplyMaxShield(100); Attr->ApplyShield(0); Advance(58);
    Combat->TickComponent(.1f,LEVELTICK_All,nullptr);
    TestEqual(TEXT("Respec restores four-second shield delay"),Attr->GetShield(),0.0f);
    Advance(100);
    Attr->ApplyHealth(Attr->GetMaxHealth()*.25f);
    const float RespecBefore=Attr->GetHealth(); Combat->TickComponent(1,LEVELTICK_All,nullptr);
    TestEqual(TEXT("Respec restores baseline two percent"),Attr->GetHealth()-RespecBefore,Attr->GetMaxHealth()*.02f,.001f);
    return true;
}
#endif
