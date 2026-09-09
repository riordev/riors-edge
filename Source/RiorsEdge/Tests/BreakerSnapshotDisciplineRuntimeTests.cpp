#include "Misc/AutomationTest.h"
#include "Combat/BreakerStatusComponent.h"
#include "Combat/BreakerZoneActor.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbilityComponent.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Abilities/BreakerAbility_Cleave.h"
#include "Tests/BreakerCastTestHelpers.h"
#include "Abilities/BreakerAbility_Unmake.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerManaComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"
#include "Progression/BreakerExperience.h"
#include "Save/BreakerMissionContent.h"
#include "Save/BreakerQuestContent.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerSnapshotDisciplineRuntimeTest,"RiorsEdge.Abilities.Caster.SnapshotDisciplineRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerSnapshotDisciplineRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);if(!World)return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);World->InitializeActorsForPlay(FURL());
    const uint64 Frame=GFrameCounter;
    ON_SCOPE_EXIT{World->DestroyWorld(false);GEngine->DestroyWorldContext(World);GFrameCounter=Frame;};
    auto Clock=[&](float Seconds){for(float T=0;T<Seconds;T+=.01f){++GFrameCounter;World->Tick(LEVELTICK_All,.01f);}};
    auto* Player=World->SpawnActor<ABreakerCharacter>();if(!Player)return false;
    Player->SetActorTickEnabled(false);Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    auto* ASC=Player->GetAbilitySystemComponent();auto* Attributes=Player->GetAttributes();
    ASC->InitAbilityActorInfo(Player,Player);ASC->AddAttributeSetSubobject(Attributes);
    auto* Combat=Player->GetCombat();Combat->BindAttributes(Attributes);
    auto* Progression=Player->GetProgression();Progression->BindAttributes(Attributes);
    if(!Progression->ChoosePermanentClassById(EBreakerClassId::Caster))return false;
    Player->GetMana()->BindAttributes(Attributes);
    FBreakerQuestFlagSet Flags;for(const auto& Mission:UBreakerMissionLibrary::GetMissions())for(const auto& Beat:Mission.Beats)
        for(const auto& Flag:UBreakerMissionLibrary::BeatCompletionFlags(Beat))Flags.Add(Flag);
    Progression->SettleDoctrineEntitlement(Flags);FText Reason;
    const auto* Tree=UBreakerProgressionLibrary::GetCasterVoidWhispererTree();
    for(const TCHAR* Id:{TEXT("Caster.VoidWhisperer.Seep"),TEXT("Caster.VoidWhisperer.StandingWater"),TEXT("Caster.VoidWhisperer.Patience"),TEXT("Caster.VoidWhisperer.Attrition"),TEXT("Caster.VoidWhisperer.Zonework"),TEXT("Caster.VoidWhisperer.SnapshotDiscipline")})
        if(!TestTrue(TEXT("Actual eight-point Snapshot Discipline route"),Progression->PurchaseNode(Tree,Id,Reason)))return false;
    TestEqual(TEXT("No free doctrine entitlement"),Progression->GetUnspentPoints(Tree->Currency),0);
    auto* Zone=World->SpawnActor<ABreakerZoneActor>();if(!Zone)return false;
    FBreakerZoneSpec ZoneSpec;ZoneSpec.RadiusCm=200;ZoneSpec.Duration=10; // O2 PLACEHOLDER, geometry fixture.
    Zone->SetActorLocation(Player->GetActorLocation());Zone->ConfigureZone(ZoneSpec,Player);
    auto Apply=[&](const FBreakerStatusApplicationSpec& Spec)
    {
        auto* Target=World->SpawnActor<ABreakerCharacter>();if(!Target)return static_cast<UBreakerStatusComponent*>(nullptr);
        Target->SetActorTickEnabled(false);Target->GetBreakerMovement()->SetComponentTickEnabled(false);
        auto* TargetASC=Target->GetAbilitySystemComponent();TargetASC->InitAbilityActorInfo(Target,Target);TargetASC->AddAttributeSetSubobject(Target->GetAttributes());
        Target->GetCombat()->BindAttributes(Target->GetAttributes());
        auto* Status=Target->FindComponentByClass<UBreakerStatusComponent>();if(!Status)return Status;
        if(!Status->HasBegunPlay())Status->BeginPlay();Status->SetComponentTickEnabled(false);
        Status->ApplyStatus(Spec,EBreakerDamageFamily::Physical,Player);return Status;
    };
    FBreakerStatusApplicationSpec Spec;Spec.StatusTag=FGameplayTag::RequestGameplayTag(TEXT("Status.Bleed"));
    Spec.Duration=4;Spec.TickInterval=1;Spec.BaseDamagePerTick=1; // O2 PLACEHOLDER, deterministic physical delivery probe.
    Spec.Snapshot.CriticalChance=.1f;Spec.Snapshot.CriticalRollSample=.2f;Spec.Snapshot.bHasCriticalRollSample=true; // O2 PLACEHOLDER, fixed original roll.
    auto* Inside=Apply(Spec);if(!Inside||Inside->GetActiveStatuses().IsEmpty())return false;
    const auto Captured=Inside->GetActiveStatuses()[0].Spec;
    TestEqual(TEXT("Adds percentage points, not relative chance"),Captured.Snapshot.CriticalChance,.35f,.001f);
    TestTrue(TEXT("Same original roll now critical"),Captured.Snapshot.bRolledCritical);
    TestEqual(TEXT("Critical multiplier is untouched"),Captured.Snapshot.CriticalMultiplier,Spec.Snapshot.CriticalMultiplier);
    auto* InsideCharacter=Cast<ABreakerCharacter>(Inside->GetOwner());
    const float BeforeHealth=InsideCharacter->GetAttributes()->GetHealth();
    Inside->AdvanceStatuses(1.f);
    TestEqual(TEXT("Native periodic hit uses single critical multiplier"),BeforeHealth-InsideCharacter->GetAttributes()->GetHealth(),Spec.BaseDamagePerTick*Spec.Snapshot.CriticalMultiplier,.001f);
    Player->SetActorLocation(Zone->GetActorLocation()+FVector(201,0,0));
    auto* Outside=Apply(Spec);if(!Outside||Outside->GetActiveStatuses().IsEmpty())return false;
    TestFalse(TEXT("Outside at application retains original result"),Outside->GetActiveStatuses()[0].Spec.Snapshot.bRolledCritical);
    TestEqual(TEXT("Earlier inside snapshot does not change after leaving"),Inside->GetActiveStatuses()[0].Spec.Snapshot.CriticalChance,.35f,.001f);
    auto* Copied=Apply(Captured);if(!Copied||Copied->GetActiveStatuses().IsEmpty())return false;
    TestEqual(TEXT("Copy neither loses nor doubles original bonus"),Copied->GetActiveStatuses()[0].Spec.Snapshot.CriticalChance,.35f,.001f);
    // Real equipped starter Cleave proves the production roll reaches application.
    // The owned zone above is a geometry fixture, not a claimed paid Rot cast.
    Player->SetActorLocation(Zone->GetActorLocation());Player->SetActorRotation(FRotator::ZeroRotator);
    FActorSpawnParameters Spawn;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Victim=World->SpawnActor<ABreakerCharacter>(Player->GetActorLocation()+FVector(110,0,0),FRotator(0,180,0),Spawn);
    if(!Victim)return false;
    Victim->SetActorTickEnabled(false);Victim->GetBreakerMovement()->SetComponentTickEnabled(false);
    auto* VictimASC=Victim->GetAbilitySystemComponent();VictimASC->InitAbilityActorInfo(Victim,Victim);VictimASC->AddAttributeSetSubobject(Victim->GetAttributes());
    Victim->GetCombat()->BindAttributes(Victim->GetAttributes());Victim->GetCombat()->BeginPlay();
    auto* VictimStatus=Victim->FindComponentByClass<UBreakerStatusComponent>();if(!VictimStatus)return false;
    VictimStatus->BeginPlay();VictimStatus->SetComponentTickEnabled(false);
    auto* Abilities=Player->GetAbilities();
    if(!Abilities->TryEquipAbility(EBreakerAbilitySlot::ClassAbilityOne,TEXT("Caster.Cleave"),Reason))return false;
    Abilities->RefreshGrants();
    const float OriginalChance=Attributes->GetCriticalChance();
    const float BeforeMana=Player->GetMana()->GetMana();
    const float QuotedCost=Abilities->GetResourceCostForSlot(EBreakerAbilitySlot::ClassAbilityOne);
    if(!TestTrue(TEXT("Normal equipped starter Cleave activates"),Abilities->TryActivateSlot(EBreakerAbilitySlot::ClassAbilityOne)))return false;
        // O266: the swing lands at the END of its wind-up, not on the press.
        Clock(BreakerAuthoredCastSeconds(TEXT("Caster.Cleave"))+.05f);
    TestEqual(TEXT("Cleave pays its actual quoted Mana"),BeforeMana-Player->GetMana()->GetMana(),QuotedCost,.001f);
    const auto* NativeBleed=VictimStatus->GetActiveStatuses().FindByPredicate([](const FBreakerActiveStatus& Entry)
        {return Entry.Spec.StatusTag==FGameplayTag::RequestGameplayTag(TEXT("Status.Bleed"));});
    if(!TestNotNull(TEXT("Actual paid Cleave applies physical Bleed"),NativeBleed))return false;
    TestTrue(TEXT("Native producer records its original single sample"),NativeBleed->Spec.Snapshot.bHasCriticalRollSample);
    TestTrue(TEXT("Recorded original sample is a valid random draw"),NativeBleed->Spec.Snapshot.CriticalRollSample>=0 && NativeBleed->Spec.Snapshot.CriticalRollSample<1);
    TestEqual(TEXT("Real producer gains exactly twenty-five percentage points once"),NativeBleed->Spec.Snapshot.CriticalChance,FMath::Clamp(OriginalChance+.25f,0.f,1.f),.001f);
    TestEqual(TEXT("Applied result uses that same producer sample"),NativeBleed->Spec.Snapshot.bRolledCritical,
        NativeBleed->Spec.Snapshot.CriticalRollSample<NativeBleed->Spec.Snapshot.CriticalChance);
    if(!Progression->RespecAtForge(Tree->Currency,true,Reason))return false;
    Player->SetActorLocation(Zone->GetActorLocation());
    auto* Removed=Apply(Spec);if(!Removed||Removed->GetActiveStatuses().IsEmpty())return false;
    TestFalse(TEXT("Respec stops new in-zone bonus"),Removed->GetActiveStatuses()[0].Spec.Snapshot.bRolledCritical);
    return true;
}
#endif
