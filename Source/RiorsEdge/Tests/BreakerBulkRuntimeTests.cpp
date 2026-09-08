#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerTankAbilities.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerGritComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDeployable.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Weapons/BreakerRocketProjectile.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionTree.h"
#include "Save/BreakerMissionContent.h"
#include "Weapons/BreakerWeaponComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerBulkRuntimeTest,
    "RiorsEdge.Abilities.BulkPaidAnchor", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerBulkRuntimeTest::RunTest(const FString& Parameters)
{
    for (int32 Rank = 0; Rank <= 2; ++Rank)
    {
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated Anchor world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* Floor = World->SpawnActor<AActor>();
    auto* Surface = NewObject<UBoxComponent>(Floor);
    Floor->AddInstanceComponent(Surface); Floor->SetRootComponent(Surface);
    Surface->SetBoxExtent(FVector(3000, 3000, 20));
    Surface->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Surface->SetCollisionResponseToAllChannels(ECR_Block);
    Surface->RegisterComponent(); Floor->SetActorLocation(FVector(0, 0, -20));
    auto* Tank = World->SpawnActor<ABreakerCharacter>(FVector(0, 0, 100), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("native Tank"), Tank)) return false;
    Tank->SetActorTickEnabled(false); Tank->GetBreakerMovement()->SetComponentTickEnabled(false);
    const float HalfHeight = Tank->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    Tank->SetActorLocation(FVector(0, 0, HalfHeight));
    auto* ASC = Tank->GetAbilitySystemComponent();
    auto* Attributes = Tank->GetAttributes();
    auto* Progression = Tank->GetProgression();
    ASC->InitAbilityActorInfo(Tank, Tank); ASC->AddAttributeSetSubobject(Attributes);
    Tank->GetCombat()->BindAttributes(Attributes); Progression->BindAttributes(Attributes);
    if (!TestTrue(TEXT("actual Tank class"), Progression->ChoosePermanentClassById(EBreakerClassId::Tank))) return false;
    // Restored benchmark entitlement fixture, not a claimed campaign run.
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(50, Progression->ExperienceCurve));
    FBreakerQuestFlagSet Flags;
    for (const auto& Mission : UBreakerMissionLibrary::GetMissions())
        for (const auto& Beat : Mission.Beats)
            for (FName Flag : UBreakerMissionLibrary::BeatCompletionFlags(Beat)) Flags.Add(Flag);
    Flags.Add(TEXT("Quest.Finale.Seal")); Progression->SettleDoctrineEntitlement(Flags);
    TestEqual(TEXT("actual entitlement is eight"), Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), 8);
    const auto* Tree = UBreakerProgressionLibrary::GetTankBastionTree();
    FText Reason;
    for (int32 N = 0; N < 2; ++N)
        if (!TestTrue(TEXT("legal Line of Sight entry"), Progression->PurchaseNode(Tree, TEXT("Tank.Bastion.LineOfSight"), Reason))) return false;
    for (int32 N = 0; N < Rank; ++N)
        if (!TestTrue(TEXT("legal Bulk purchase"), Progression->PurchaseNode(Tree, TEXT("Tank.Bastion.Bulk"), Reason))) return false;
    TestEqual(TEXT("Bulk stays within actual budget"), Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), 6 - Rank);
    if (!Progression->IsAbilityUnlocked(TEXT("Tank.AnchorPoint")))
        if (!TestTrue(TEXT("earned token unlocks Anchor"), Progression->SpendAbilityToken(TEXT("Tank.AnchorPoint"), Reason))) return false;
    auto* Grit = Tank->GetGrit(); Grit->BindAttributes(Attributes); Grit->SetComponentTickEnabled(false);
    Grit->SetInCombat(true);
    for (int32 Second = 0; Second < 70; ++Second) { Grit->SetEnemyInProximity(true); Grit->AdvanceLoop(1); }
    TestEqual(TEXT("ordinary combat proximity funds Anchor"), Attributes->GetClassResource(), 100.0f);
    auto* Weapon = Tank->GetWeapon(); Weapon->BeginPlay(); Weapon->EquipArchetype(EBreakerWeaponArchetype::Rifle);
    auto* Movement = Tank->GetBreakerMovement(); Movement->SetMovementMode(MOVE_Walking);
    const auto Handle = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_AnchorPoint::StaticClass(), 1));
    if (!TestTrue(TEXT("actual paid Anchor cast"), ASC->TryActivateAbility(Handle))) return false;
    TestEqual(TEXT("Anchor spends thirty Grit"), Attributes->GetClassResource(), 70.0f);
    ABreakerDeployable* Anchor = nullptr;
    for (const auto& Weak : ABreakerDeployable::GetLiveDeployables())
        if (auto* Candidate = Weak.Get(); Candidate && Candidate->GetOwningCharacter() == Tank
            && Candidate->GetDeployableType() == EBreakerDeployableType::AnchorPoint) Anchor = Candidate;
    if (!TestNotNull(TEXT("paid Anchor exists"), Anchor)) return false;
    Anchor->DispatchBeginPlay(); // Bind native panel damage/death without owner save startup.
    auto* PanelCombat = Anchor->FindComponentByClass<UBreakerCombatComponent>();
    auto* PanelHealth = FindObject<UBreakerAttributeSet>(Anchor, TEXT("Attributes"));
    if (!TestNotNull(TEXT("panel combat"), PanelCombat) || !TestNotNull(TEXT("panel attributes"), PanelHealth)) return false;
    const float ExpectedFraction = .2f * (Rank == 2 ? 2.0f : Rank == 1 ? 1.5f : 1.0f);
    TestEqual(TEXT("existing paid Bulk health scaling remains"), PanelHealth->GetMaxHealth(), Attributes->GetMaxHealth() * ExpectedFraction, .01f);
    auto Rocket = [&](bool bDirect)
    {
        auto* Shot = World->SpawnActor<ABreakerRocketProjectile>(Anchor->GetActorLocation(), FRotator::ZeroRotator);
        if (!TestNotNull(TEXT("actual rocket"), Shot)) return;
        FBreakerDamageRequest Damage;
        Damage.BaseDamage = 1;
        Damage.bCanCritical = false;
        Damage.bCanBeAvoided = false;
        Shot->InitializeRocket(Damage, 1000, 100);
        Shot->DispatchBeginPlay();
        if (bDirect)
        {
            auto* Collision = Shot->FindComponentByClass<USphereComponent>();
            FHitResult Hit;
            Hit.ImpactPoint = Anchor->GetActorLocation();
            Collision->OnComponentHit.Broadcast(Collision, Anchor, Anchor->GetRootComponent()->IsA<UPrimitiveComponent>()
                ? Cast<UPrimitiveComponent>(Anchor->GetRootComponent()) : nullptr, FVector::ZeroVector, Hit);
        }
        else Shot->Explode(Anchor->GetActorLocation());
        Shot->Destroy();
    };
    float Before = PanelHealth->GetHealth();
    Rocket(false);
    TestEqual(TEXT("only owned Bulk rejects incidental real rocket explosion"), PanelHealth->GetHealth(), Before - (Rank > 0 ? 0 : 1), .001f);
    Before = PanelHealth->GetHealth();
    Rocket(true);
    TestEqual(TEXT("actual panel impact remains vulnerable"), PanelHealth->GetHealth(), Before - 1, .001f);
    FBreakerDamageRequest Direct;
    Direct.BaseDamage = 1; Direct.bCanCritical = false; Direct.bCanBeAvoided = false;
    TestEqual(TEXT("ordinary direct damage is unaffected"), PanelCombat->ReceiveDamage(Direct).HealthDamage, 1.0f, .001f);
    if (Rank > 0)
    {
        // Receiver-type isolation: authored native turret is not granted Bulk.
        auto* Other = World->SpawnActor<ABreakerDeployable>(FVector(4000, 0, 100), FRotator::ZeroRotator);
        if (!TestNotNull(TEXT("unrelated receiver"), Other)) return false;
        Other->InitializeDeployable(EBreakerDeployableType::Turret, Tank, 0);
        Other->DispatchBeginPlay();
        FBreakerDamageRequest Splash = Direct; Splash.bRadialDamage = true;
        TestEqual(TEXT("Bulk never protects other deployable types"),
            Other->FindComponentByClass<UBreakerCombatComponent>()->ReceiveDamage(Splash).HealthDamage, 1.0f, .001f);
        Other->Destroy();
        if (!TestTrue(TEXT("actual Forge respec removes Bulk"), Progression->RespecAtForge(EBreakerPointCurrency::DoctrinePoints, true, Reason))) return false;
        Before = PanelHealth->GetHealth();
        Rocket(false);
        TestEqual(TEXT("existing panel loses immunity immediately on respec"), PanelHealth->GetHealth(), Before - 1, .001f);
    }
    }
    return true;
}
#endif
