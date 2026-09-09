#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerModifierComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerVolatileFuseFeedbackRuntimeTest,"RiorsEdge.Enemies.VolatileFuseFeedback",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBreakerVolatileFuseFeedbackRuntimeTest::RunTest(const FString&)
{
 UWorld::InitializationValues Init;Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
 auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);if(!World)return false;
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);World->InitializeActorsForPlay(FURL());
 ON_SCOPE_EXIT{World->DestroyWorld(false);GEngine->DestroyWorldContext(World);};
 auto Spawn=[&](){auto* E=World->SpawnActor<ABreakerEnemy>();if(E){E->ConfigureCrowdProbe();E->SetAreaLevel(1);E->DispatchBeginPlay();E->SetActorTickEnabled(false);}return E;};
 auto* Bomb=Spawn();if(!TestNotNull(TEXT("Native Volatile enemy"),Bomb))return false;
 auto* Mods=Bomb->GetModifierComponent();if(!TestNotNull(TEXT("Modifier component"),Mods))return false;
 TestTrue(TEXT("Legal Volatile assignment"),Mods->SetModifiers({EBreakerEnemyModifier::Volatile}));
 TestFalse(TEXT("Living enemy does not show corpse countdown"),Mods->IsFuseLit());
 TestEqual(TEXT("Unlit countdown has no negative presentation value"),Mods->GetFuseRemainingSeconds(),0.0f);
 const FProperty* FuseProperty=FindFProperty<FProperty>(UBreakerEnemyModifierComponent::StaticClass(),TEXT("FuseRemaining"));
 TestTrue(TEXT("Client cue reads replicated authoritative fuse"),FuseProperty&&FuseProperty->HasAnyPropertyFlags(CPF_Net));
 FBreakerDamageRequest Lethal;Lethal.BaseDamage=Bomb->FindComponentByClass<UBreakerCombatComponent>()->GetMaxHealth()*2;
 Lethal.DamageFamily=EBreakerDamageFamily::TrueDamage;Lethal.bCanCritical=false;Lethal.bCanBeAvoided=false;
 Bomb->FindComponentByClass<UBreakerCombatComponent>()->ReceiveDamage(Lethal);
 TestTrue(TEXT("Real lethal hit leaves a dead corpse"),Bomb->IsDeadEnemy());
 TestTrue(TEXT("Cue remains visible after health bar dies"),Mods->IsFuseLit());
 const float Fuse=Mods->Params.VolatileFuseSeconds;
 TestEqual(TEXT("Countdown starts at actual authored fuse"),Mods->GetFuseRemainingSeconds(),Fuse);
 Bomb->Tick(Fuse*.5f);
 TestTrue(TEXT("Dead actor continues advancing warning"),Mods->IsFuseLit());
 TestTrue(TEXT("Countdown halves on native corpse tick"),FMath::IsNearlyEqual(Mods->GetFuseRemainingSeconds(),Fuse*.5f));
 Bomb->Tick(Fuse*.5f+.01f);
 TestFalse(TEXT("Detonation removes countdown immediately"),Mods->IsFuseLit());
 TestEqual(TEXT("Finished warning exposes zero seconds"),Mods->GetFuseRemainingSeconds(),0.0f);
 // Re-dressing is the existing pool/clear boundary. An interrupted fuse
 // must not leave a stale warning or detonate in a future pooled life.
 auto* ResetBomb=Spawn();if(!ResetBomb)return false;
 auto* ResetMods=ResetBomb->GetModifierComponent();ResetMods->SetModifiers({EBreakerEnemyModifier::Volatile});
 Lethal.BaseDamage=ResetBomb->FindComponentByClass<UBreakerCombatComponent>()->GetMaxHealth()*2;ResetBomb->FindComponentByClass<UBreakerCombatComponent>()->ReceiveDamage(Lethal);
 TestTrue(TEXT("Second corpse has real fuse before reset"),ResetMods->IsFuseLit());
 ResetMods->SetModifiers({});
 TestFalse(TEXT("Existing modifier reset clears corpse warning"),ResetMods->IsFuseLit());
 TestEqual(TEXT("Reset leaves no old countdown"),ResetMods->GetFuseRemainingSeconds(),0.0f);
 return true;
}
#endif
