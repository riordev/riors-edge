#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Interaction/BreakerRiftDoor.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Game/BreakerZoneBuilder.h"
#include "Save/BreakerQuestJournal.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerRiftEntryEligibilityTest,"RiorsEdge.Campaign.RiftEntryEligibility",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerRiftEntryEligibilityTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* Player=World->SpawnActor<ABreakerCharacter>();
    auto* Door=World->SpawnActor<ABreakerRiftDoor>();
    if (!Player || !Door) return false;
    auto* ASC=Player->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Player,Player); ASC->AddAttributeSetSubobject(Player->GetAttributes());
    Player->GetCombat()->BindAttributes(Player->GetAttributes());
    int32 Requests=0;
    Door->OnRiftEntryRequested.AddLambda([&](const FBreakerRiftDefinition&,APawn* Pawn){ if (Pawn==Player) ++Requests; });
    Door->Rift=UBreakerZoneBuilder::FernhallRiftFor(NAME_None);
    FText Reason=FText::FromString(TEXT("stale"));
    TestTrue(TEXT("ordinary campaign entry remains free without quest flags"),ABreakerRiftDoor::CanEnterRift(Door->Rift,Player,Reason));
    TestTrue(TEXT("success clears previous refusal"),Reason.IsEmpty());
    TestTrue(TEXT("actual ordinary door accepts"),Door->SelectDestination(ABreakerTravelPoint::RiftDestinationId,Player));
    TestEqual(TEXT("one actual entry event"),Requests,1);
    Door->Rift=UBreakerZoneBuilder::FernhallRiftFor(TEXT("breach"));
    TestFalse(TEXT("Breach refuses missing prior mission"),ABreakerRiftDoor::CanEnterRift(Door->Rift,Player,Reason));
    TestTrue(TEXT("refusal names authored mission"),Reason.ToString().Contains(TEXT("A DIFFERENT UNIFORM")));
    TestFalse(TEXT("actual door refuses missing prior mission"),Door->SelectDestination(ABreakerTravelPoint::RiftDestinationId,Player));
    Player->GetQuestJournal()->SetFlag(TEXT("Quest.AlteredContact.TurnedIn"));
    TestFalse(TEXT("prior turn-in alone does not accept next mission"),ABreakerRiftDoor::CanEnterRift(Door->Rift,Player,Reason));
    TestTrue(TEXT("refusal names next assignment"),Reason.ToString().Contains(TEXT("THE BREACH")));
    TestFalse(TEXT("actual door refuses unaccepted Breach"),Door->SelectDestination(ABreakerTravelPoint::RiftDestinationId,Player));
    TestEqual(TEXT("refused doors emit no travel requests"),Requests,1);
    Player->GetQuestJournal()->SetFlag(TEXT("Quest.Breach.Accepted"));
    TestTrue(TEXT("accepted journal permits real door"),Door->SelectDestination(ABreakerTravelPoint::RiftDestinationId,Player));
    TestEqual(TEXT("one accepted Breach request"),Requests,2);
    FBreakerDamageRequest Kill; Kill.BaseDamage=Player->GetAttributes()->GetMaxHealth()+100;
    Kill.DamageFamily=EBreakerDamageFamily::TrueDamage; Kill.bCanCritical=false; Kill.bCanBeAvoided=false;
    Player->GetCombat()->ReceiveDamage(Kill);
    if (!TestTrue(TEXT("actual player death"),Player->GetCombat()->IsDead())) return false;
    TestFalse(TEXT("dead player cannot enter"),Door->SelectDestination(ABreakerTravelPoint::RiftDestinationId,Player));
    TestEqual(TEXT("dead refusal emits no travel"),Requests,2);
    Player->GetCombat()->RestoreVitals();
    TestTrue(TEXT("revival retains legitimate entry eligibility"),ABreakerRiftDoor::CanEnterRift(Door->Rift,Player,Reason));
    TestFalse(TEXT("unset Rift refuses"),ABreakerRiftDoor::CanEnterRift(FBreakerRiftDefinition(),Player,Reason));
    TestFalse(TEXT("null player refuses"),ABreakerRiftDoor::CanEnterRift(Door->Rift,nullptr,Reason));
    return true;
}
#endif
