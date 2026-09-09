#include "Misc/AutomationTest.h"
#include "Tests/BreakerCastTestHelpers.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbility_Siphon.h"
#include "Abilities/BreakerAbility_Unmake.h"
#include "Abilities/BreakerTankAbilities.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerManaComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerExperience.h"
#include "Save/BreakerMissionContent.h"
#include "TimerManager.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerAbilityInterruptRuntimeTest, "RiorsEdge.Abilities.InterruptRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerAbilityInterruptRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated interrupt world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const uint64 InitialFrame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = InitialFrame; };
    ABreakerCharacter* Player = World->SpawnActor<ABreakerCharacter>();
    if (!TestNotNull(TEXT("real character without save-loading BeginPlay"), Player)) return false;
    auto* ASC = Player->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Player->GetAttributes());
    Player->GetCombat()->BindAttributes(Player->GetAttributes());
    Player->GetProgression()->BindAttributes(Player->GetAttributes());
    auto* Movement = Player->GetBreakerMovement();
    Movement->SetComponentTickEnabled(false); Movement->bRunPhysicsWithNoController = true;
    auto SetClass = [&](EBreakerClassId Class)
    {
        FBreakerProgressionState State; State.PermanentClass = Class;
        Player->GetProgression()->LoadProgressionState(State);
        Player->GetAttributes()->ApplyClassResource(100);
    };
    SetClass(EBreakerClassId::Caster);
    Player->GetMana()->BindAttributes(Player->GetAttributes());
    auto Advance = [&](int32 Steps, bool bPhysics = false)
    {
        for (int32 I = 0; I < Steps; ++I)
        {
            ++GFrameCounter; World->Tick(LEVELTICK_All, .05f);
            if (!World->GetTimerManager().HasBeenTickedThisFrame()) World->GetTimerManager().Tick(.05f);
            if (bPhysics) Movement->PerformMovement(.05f);
        }
    };
    AActor* Target = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("channel target"), Target)) return false;
    auto* Body = NewObject<USphereComponent>(Target);
    Target->AddInstanceComponent(Body); Target->SetRootComponent(Body);
    Body->SetSphereRadius(40); Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Body->SetCollisionResponseToAllChannels(ECR_Ignore); Body->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);
    Body->RegisterComponent(); Target->SetActorLocation(FVector(500, 0, 0));
    auto* TargetCombat = NewObject<UBreakerCombatComponent>(Target);
    Target->AddInstanceComponent(TargetCombat); TargetCombat->RegisterComponent();
    auto* Health = NewObject<UBreakerAttributeSet>(Target);
    Health->ApplyMaxHealth(10000); Health->ApplyHealth(10000); TargetCombat->BindAttributes(Health);
    const auto Siphon = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Siphon::StaticClass(), 1));
    if (!TestTrue(TEXT("real Siphon starts"), ASC->TryActivateAbility(Siphon))) return false;
    BreakerResolvePendingCast(World, Player);
    Advance(12);
    if (!TestTrue(TEXT("Siphon actually ticks before interruption"), Health->GetHealth() < 10000)) return false;
    if (!TestTrue(TEXT("accepted stagger interrupts channel"), Player->GetCombat()->ApplyStagger(.5f))) return false;
    TestFalse(TEXT("Siphon GAS instance ends"), ASC->FindAbilitySpecFromHandle(Siphon)->IsActive());
    const float AfterInterrupt = Health->GetHealth();
    const float ManaBefore = Player->GetAttributes()->GetClassResource();
    TestFalse(TEXT("native GAS activation also refuses while staggered"), ASC->TryActivateAbility(Siphon));
    BreakerResolvePendingCast(World, Player);
    TestEqual(TEXT("refused activation spends nothing"), Player->GetAttributes()->GetClassResource(), ManaBefore);
    Advance(12);
    TestEqual(TEXT("canceled Siphon timer cannot keep damaging"), Health->GetHealth(), AfterInterrupt);
    const auto Unmake = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Unmake::StaticClass(), 1));
    Player->GetAttributes()->ApplyClassResource(100);
    if (!TestTrue(TEXT("real Unmake starts"), ASC->TryActivateAbility(Unmake))) return false;
    BreakerResolvePendingCast(World, Player);
    if (!TestTrue(TEXT("Unmake owns generation suspension"), Player->GetMana()->IsGenerationSuspended())) return false;
    Player->GetCombat()->ApplyStagger(.25f);
    TestFalse(TEXT("Unmake channel is canceled"), ASC->FindAbilitySpecFromHandle(Unmake)->IsActive());
    TestFalse(TEXT("Unmake cancellation restores generation"), Player->GetMana()->IsGenerationSuspended());
    Advance(6);
    SetClass(EBreakerClassId::Tank);
    const auto Hold = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Hold::StaticClass(), 1));
    if (!TestTrue(TEXT("real timed Hold starts"), ASC->TryActivateAbility(Hold))) return false;
    BreakerResolvePendingCast(World, Player);
    Player->GetCombat()->ApplyStagger(.25f);
    TestTrue(TEXT("already-applied timed buff survives stagger"), ASC->FindAbilitySpecFromHandle(Hold)->IsActive());
    ASC->CancelAbilityHandle(Hold); Advance(6);

    AActor* Floor = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("physical landing floor"), Floor)) return false;
    auto* Box = NewObject<UBoxComponent>(Floor);
    Floor->AddInstanceComponent(Box); Floor->SetRootComponent(Box);
    Box->SetBoxExtent(FVector(2000, 2000, 10)); Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Box->SetCollisionResponseToAllChannels(ECR_Block); Box->RegisterComponent(); Floor->SetActorLocation(FVector(0, 0, -10));
    ABreakerEnemy* Enemy = World->SpawnActor<ABreakerEnemy>(FVector(200, 0, 100), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("actual Ground Zero enemy"), Enemy)) return false;
    Enemy->SetActorTickEnabled(false);
    auto* EnemyCombat = Enemy->FindComponentByClass<UBreakerCombatComponent>();
    if (!TestNotNull(TEXT("enemy combat"), EnemyCombat)) return false;
    auto* EnemyHealth = Cast<UBreakerAttributeSet>(Enemy->GetDefaultSubobjectByName(TEXT("Attributes")));
    auto* EnemyASC = Enemy->GetAbilitySystemComponent();
    if (!TestNotNull(TEXT("enemy's actual attribute set"), EnemyHealth) || !TestNotNull(TEXT("enemy ASC"), EnemyASC)) return false;
    EnemyASC->InitAbilityActorInfo(Enemy, Enemy);
    EnemyASC->AddAttributeSetSubobject(EnemyHealth);
    EnemyASC->SetNumericAttributeBase(UBreakerAttributeSet::GetMaxHealthAttribute(), 10000);
    EnemyASC->SetNumericAttributeBase(UBreakerAttributeSet::GetHealthAttribute(), 10000);
    EnemyCombat->BindAttributes(EnemyHealth);
    if (!TestEqual(TEXT("actual enemy fixture maximum is ten thousand"), EnemyHealth->GetMaxHealth(), 10000.0f)
        || !TestEqual(TEXT("actual enemy fixture starts at full health"), EnemyHealth->GetHealth(), 10000.0f)) return false;
    const auto Plunge = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_GroundZero::StaticClass(), 1));
    auto StartPlunge = [&](float Height = 400.0f)
    {
        Player->TeleportTo(FVector(0, 0, Height), FRotator::ZeroRotator, false, true);
        Movement->SetMovementMode(MOVE_Falling); Movement->Velocity = FVector(0, 0, -100);
        Player->GetAttributes()->ApplyClassResource(100);
        ASC->SetNumericAttributeBase(UBreakerAttributeSet::GetCriticalChanceAttribute(), 0);
        return ASC->TryActivateAbility(Plunge);
    };
    if (!TestTrue(TEXT("actual airborne Ground Zero starts"), StartPlunge())) return false;
    TestEqual(TEXT("casting in air cannot manufacture floor damage"), EnemyHealth->GetHealth(), 10000.0f);
    TestTrue(TEXT("plunge remains an interruptible active action"), ASC->FindAbilitySpecFromHandle(Plunge)->IsActive());
    Player->GetCombat()->ApplyStagger(.25f);
    Advance(24, true);
    TestTrue(TEXT("interrupted character still physically lands"), Movement->IsMovingOnGround());
    TestEqual(TEXT("interrupted plunge never detonates later"), EnemyHealth->GetHealth(), 10000.0f);
    // Reset only the fixture's cooldown, preserving a second actual GAS cast.
    FGameplayTagContainer PlungeCooldown;
    PlungeCooldown.AddTag(GetDefault<UBreakerAbility_GroundZero>()->GetAbilityDefinition()->CooldownTag);
    ASC->RemoveActiveEffectsWithGrantedTags(PlungeCooldown);
    if (!TestTrue(TEXT("second real plunge starts"), StartPlunge())) return false;
    Advance(12, true);
    TestTrue(TEXT("successful plunge lands before paying damage"), Movement->IsMovingOnGround());
    TestTrue(TEXT("actual landing damages enemy"), EnemyHealth->GetHealth() < 10000);
    TestTrue(TEXT("actual landing applies binary stagger"), EnemyCombat->IsStaggered());
    const float ShortFallDamage = 10000.0f - EnemyHealth->GetHealth();
    float CappedDamage = 0;
    for (const float Height : {1600.0f, 2000.0f})
    {
        ASC->RemoveActiveEffectsWithGrantedTags(PlungeCooldown);
        Player->GetAttributes()->ApplyHealth(Player->GetAttributes()->GetMaxHealth());
        const float BeforeFall = EnemyHealth->GetHealth();
        if (!TestTrue(TEXT("deep actual Ground Zero starts"), StartPlunge(Height))) return false;
        for (int32 I = 0; I < 60 && ASC->FindAbilitySpecFromHandle(Plunge)->IsActive(); ++I) Advance(1, true);
        if (!TestTrue(TEXT("deep plunge physically lands"), Movement->IsMovingOnGround())) return false;
        const float Damage = BeforeFall - EnemyHealth->GetHealth();
        TestTrue(TEXT("actual longer descent deals more than short-drop minimum"), Damage > ShortFallDamage);
        if (CappedDamage > 0) TestEqual(TEXT("different real drops beyond twelve metres share baseline cap"), Damage, CappedDamage, .01f);
        CappedDamage = Damage;
    }
    Player->GetAttributes()->ApplyHealth(Player->GetAttributes()->GetMaxHealth());
    // Full-budget wiring fixture, distinct from earned campaign tests. The
    // actual charge actor/fuse and physical landing prove launch attribution.
    Player->GetProgression()->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(50, FBreakerExperienceCurve()));
    FBreakerQuestFlagSet CompletedFlags;
    for (const auto& Mission : UBreakerMissionLibrary::GetMissions())
        for (const auto& Beat : Mission.Beats)
            for (FName Flag : UBreakerMissionLibrary::BeatCompletionFlags(Beat)) CompletedFlags.Add(Flag);
    CompletedFlags.Add(TEXT("Quest.Finale.Seal"));
    Player->GetProgression()->SettleDoctrineEntitlement(CompletedFlags);
    if (!TestEqual(TEXT("authored completed benchmark settlement supplies eight points"),
        Player->GetProgression()->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), 8)) return false;
    const auto* Demolition = UBreakerProgressionLibrary::GetTankDemolitionistTree();
    FText Reason;
    for (const TCHAR* Node : { TEXT("Tank.Demolitionist.ShapedCharge"), TEXT("Tank.Demolitionist.ShapedCharge"),
        TEXT("Tank.Demolitionist.Bootstraps"), TEXT("Tank.Demolitionist.Bootstraps"),
        TEXT("Tank.Demolitionist.BracedForImpact"), TEXT("Tank.Demolitionist.BracedForImpact"),
        TEXT("Tank.Demolitionist.KineticRecovery") })
        if (!TestTrue(Node, Player->GetProgression()->PurchaseNode(Demolition, Node, Reason))) return false;
    APlayerController* Controller = World->SpawnActor<APlayerController>();
    if (!TestNotNull(TEXT("real downward aim controller"), Controller)) return false;
    Controller->Possess(Player);
    Controller->SetInitialLocationAndRotation(FVector(0, 0, 200), FRotator(-90, 0, 0));
    Controller->SetControlRotation(FRotator(-90, 0, 0));
    const auto Breach = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_BreachCharge::StaticClass(), 1));
    Player->GetAttributes()->ApplyClassResource(100);
    const float BeforeTakeoff = Player->GetAttributes()->GetHealth();
    if (!TestTrue(TEXT("real Breach charge placement"), ASC->TryActivateAbility(Breach))) return false;
    BreakerResolvePendingCast(World, Player);
    Advance(26, true);
    TestTrue(TEXT("owned blast still pays actual self-damage"), Player->GetAttributes()->GetHealth() < BeforeTakeoff);
    if (!TestTrue(TEXT("actual Breach fuse launches player airborne"), Movement->IsFalling())) return false;
    for (int32 I = 0; I < 70 && Movement->IsFalling(); ++I) Advance(1, true);
    TestTrue(TEXT("owned blast returns to a physical landing"), Movement->IsMovingOnGround());
    TestTrue(TEXT("purchased Kinetic grants real immunity after actual own blast landing"), Player->GetCombat()->IsStaggerImmune());
    return true;
}
#endif
