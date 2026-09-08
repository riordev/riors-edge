#include "Tests/BreakerWardRuntimeObserver.h"
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
void UBreakerWardRuntimeObserver::OnAvoided(const FBreakerActiveStatus& Active)
{
    ++Refusals;
    if (bReenter && Status)
    {
        bReenter=false;
        FBreakerDamageRequest Hit; Hit.BaseDamage=1; Hit.bCanCritical=false; Hit.bCanBeAvoided=false;
        if (Combat) Combat->ReceiveDamage(Hit);
        Status->ApplyStatus(Active.Spec,Active.DamageFamily,nullptr);
    }
}
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreWardRuntimeTest,"RiorsEdge.Progression.CoreWardRuntime",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreWardRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    World->InitializeActorsForPlay(FURL());
    auto Advance=[&](int32 Frames) { for(int32 I=0; I<Frames; ++I) World->Tick(LEVELTICK_All,.05f); };
    auto* Player=World->SpawnActor<ABreakerCharacter>();
    auto* Attr=Player->GetAttributes(); auto* Combat=Player->GetCombat(); auto* Progression=Player->GetProgression();
    auto* ASC=Player->GetAbilitySystemComponent(); ASC->InitAbilityActorInfo(Player,Player); ASC->AddAttributeSetSubobject(Attr);
    Combat->BindAttributes(Attr); Progression->BindAttributes(Attr);
    auto* Status=Player->FindComponentByClass<UBreakerStatusComponent>(); if (!Status) return false;
    Status->SetComponentTickEnabled(false); // Explicit status advancement; world still supplies genuine combat timestamps.
    auto* Tree=NewObject<UBreakerProgressionTree>(); Tree->TreeId=TEXT("Test.Ward.Schema"); Tree->Currency=EBreakerPointCurrency::CorePoints;
    for (const TCHAR* Id : {TEXT("Null"),TEXT("Insulation")})
    {
        auto* Node=NewObject<UBreakerProgressionNode>(Tree); Node->NodeId=FName(Id); Node->Currency=Tree->Currency;
        Node->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(*FString::Printf(TEXT("Progression.Node.Core.%s"),Id)))); Tree->Nodes.Add(Node);
    }
    auto* Class=NewObject<UBreakerClassDefinition>(); Class->ClassId=EBreakerClassId::Caster; Class->BranchTrees.Add(Tree);
    if (!Progression->ChoosePermanentClass(Class)) return false;
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(3,Progression->ExperienceCurve));
    FText Reason; if (!TestTrue(TEXT("Earned Null purchase"),Progression->PurchaseNode(Tree,TEXT("Null"),Reason))) return false;
    auto* Observer=NewObject<UBreakerWardRuntimeObserver>(); Observer->Status=Status; Observer->Combat=Combat;
    Status->OnStatusAvoided.AddDynamic(Observer,&UBreakerWardRuntimeObserver::OnAvoided);
    FBreakerStatusApplicationSpec Poison; Poison.StatusTag=FGameplayTag::RequestGameplayTag(TEXT("Status.Poison"));
    Poison.Duration=3; Poison.TickInterval=.5f; Poison.BaseDamagePerTick=1;
    auto Invalid=Poison; Invalid.Duration=0; Status->ApplyStatus(Invalid,EBreakerDamageFamily::Physical,nullptr);
    Status->GrantStatusImmunity(.1f); Status->ApplyStatus(Poison,EBreakerDamageFamily::Physical,nullptr); Status->AdvanceStatuses(.2f);
    TestEqual(TEXT("Invalid and immune applications do not consume Null"),Observer->Refusals,0);
    Observer->bReenter=true; Status->ApplyStatus(Poison,EBreakerDamageFamily::Physical,nullptr);
    TestEqual(TEXT("Refusal is claimed before callback reentry"),Observer->Refusals,1);
    TestTrue(TEXT("Nested second application can land rather than receive another refusal"),Status->HasStatus(Poison.StatusTag));
    Status->ConsumeAllStatuses(); Status->ApplyStatus(Poison,EBreakerDamageFamily::Physical,nullptr);
    TestEqual(TEXT("Consumption does not rearm Null"),Observer->Refusals,1); Status->ConsumeAllStatuses();
    // No Status tick during the quiet gap: incoming clock hook must retain the fight boundary.
    Advance(130);
    FBreakerDamageRequest Hit; Hit.BaseDamage=1; Hit.bCanCritical=false; Hit.bCanBeAvoided=false;
    Hit.bBypassShield=true; Combat->ReceiveDamage(Hit);
    Status->ApplyStatus(Poison,EBreakerDamageFamily::Physical,nullptr);
    TestEqual(TEXT("Real incoming hit after quiet gap rearms without a status tick"),Observer->Refusals,2);
    TestFalse(TEXT("New fight first ailment refused"),Status->HasStatus(Poison.StatusTag));
    Advance(130);
    Hit.Element=EBreakerElement::Void; Hit.ElementalFraction=1; Hit.BaseDamage=Combat->GetMaxHealth()*.11f;
    const auto Direct=Combat->ReceiveDamage(Hit);
    const float AfterDirect=Attr->GetHealth();
    TestTrue(TEXT("Element threshold direct hit still lands"),Direct.HealthDamage>0);
    TestEqual(TEXT("Threshold ailment consumes the new fight refusal"),Observer->Refusals,3);
    TestFalse(TEXT("Refused Erased creates no payload"),Status->HasStatus(FGameplayTag::RequestGameplayTag(TEXT("Status.Erased"))));
    Status->AdvanceStatuses(5); TestEqual(TEXT("Refused Erased never pays delayed damage"),Attr->GetHealth(),AfterDirect);
    if (!TestTrue(TEXT("Earned Insulation purchase"),Progression->PurchaseNode(Tree,TEXT("Insulation"),Reason))) return false;
    Hit.BaseDamage=1; Combat->ReceiveDamage(Hit);
    TestTrue(TEXT("Accepted partial buildup exists"),Status->GetVoidBuildup()>0);
    Status->AdvanceStatuses(1.9f); TestTrue(TEXT("Insulation retains buildup before doubled grace ends"),Status->GetVoidBuildup()>0);
    Status->AdvanceStatuses(.2f); TestEqual(TEXT("Insulation doubles buildup grace elapsed only"),Status->GetVoidBuildup(),0.0f);
    Hit.ElementBuildupFadeSeconds=2; Combat->ReceiveDamage(Hit);
    const float Protected=Status->GetVoidBuildup(); Status->AdvanceStatuses(2.5f);
    TestEqual(TEXT("Insulation doubles protected grace and fade without changing earned amount"),Status->GetVoidBuildup(),Protected*.5f,.001f);
    Status->AdvanceStatuses(.6f);
    Status->ApplyStatus(Poison,EBreakerDamageFamily::Physical,nullptr);
    Status->AdvanceStatuses(.5f);
    const auto* Active=Status->GetActiveStatuses().FindByPredicate([&](const auto& S){return S.Spec.StatusTag==Poison.StatusTag;});
    if (!TestNotNull(TEXT("Active physical ailment survives"),Active)) return false;
    TestEqual(TEXT("Insulation does not double active duration aging"),Active->RemainingDuration,2.5f,.001f);
    return true;
}
#endif
