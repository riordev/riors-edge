#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbility_Siphon.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerManaComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerStatusComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerSiphonVoidDeliveryTest, "RiorsEdge.Abilities.SiphonVoidDelivery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerSiphonVoidDeliveryTest::RunTest(const FString& Parameters)
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
    // Native ability grant isolates actual paid delivery, not campaign acquisition.
    const auto Handle = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Siphon::StaticClass(), 1));
    Combat->DodgeChance = 1;
    const float ManaBefore = Caster->GetAttributes()->GetClassResource();
    if (!TestTrue(TEXT("actual Siphon channel activates"), ASC->TryActivateAbility(Handle))) return false;
    Advance(12);
    TestEqual(TEXT("dodged channel cannot add Void buildup"), Status->GetVoidBuildup(), 0.0f);
    TestEqual(TEXT("dodged channel cannot deal damage"), Health->GetHealth(), 10000.0f);
    TestTrue(TEXT("real channel pays Mana"), Caster->GetAttributes()->GetClassResource() < ManaBefore);
    Combat->DodgeChance = 0;
    Advance(12);
    TestTrue(TEXT("accepted live channel deals damage"), Health->GetHealth() < 10000);
    TestTrue(TEXT("accepted live channel builds Void"), Status->GetVoidBuildup() > 0);
    TestEqual(TEXT("Siphon does not secretly build Entropy"), Status->GetEntropyBuildup(), 0.0f);
    ASC->CancelAbilityHandle(Handle);
    const float AfterCancel = Health->GetHealth();
    Advance(12);
    TestEqual(TEXT("cancel stops paid channel damage"), Health->GetHealth(), AfterCancel);
    return true;
}
#endif
