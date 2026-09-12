#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerSupportAbilities.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerChargeComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PawnMovementComponent.h"
#include "TimerManager.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionTree.h"
#include "Progression/BreakerExperience.h"
#include "Save/BreakerMissionContent.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerMarkRuntimeTest, "RiorsEdge.Abilities.MarkRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerMarkRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated Mark world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const uint64 InitialFrame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = InitialFrame; };
    auto MakePlayer = [&](FVector Location)
    {
        FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Player = World->SpawnActor<ABreakerCharacter>(Location, FRotator::ZeroRotator, Spawn);
        if (!Player) return Player;
        auto* Controller = World->SpawnActor<APlayerController>();
        if (!TestNotNull(TEXT("actual fixture player controller"), Controller)) return static_cast<ABreakerCharacter*>(nullptr);
        Controller->Player = NewObject<ULocalPlayer>(GEngine);
        Controller->SetAsLocalPlayerController();
        TestTrue(TEXT("fixture controller owns a real local-player camera surface"), Controller->IsLocalPlayerController());
        Controller->Possess(Player);
        Controller->SetInitialLocationAndRotation(Location, FRotator::ZeroRotator);
        Controller->SetViewTarget(Player);
        auto* ASC = Player->GetAbilitySystemComponent();
        ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Player->GetAttributes());
        Player->GetCombat()->BindAttributes(Player->GetAttributes());
        Player->GetProgression()->BindAttributes(Player->GetAttributes());
        TestTrue(TEXT("actual Support selection"), Player->GetProgression()->ChoosePermanentClassById(EBreakerClassId::Support));
        Player->GetBreakerMovement()->SetComponentTickEnabled(false);
        auto* Charge = Player->FindComponentByClass<UBreakerChargeComponent>();
        Charge->BindAttributes(Player->GetAttributes()); Charge->SetComponentTickEnabled(false); Charge->SetInCombat(true);
        Player->GetAttributes()->ApplyClassResource(100);
        return Player;
    };
    auto* Owner = MakePlayer(FVector(0, 0, 100));
    auto* Ally = MakePlayer(FVector(0, 2000, 100));
    if (!Owner || !Ally) return false;
    auto MakeEnemy = [&](FVector Location)
    {
        auto* Enemy = World->SpawnActor<ABreakerEnemy>(Location, FRotator::ZeroRotator);
        if (!Enemy) return Enemy;
        auto* Attributes = Cast<UBreakerAttributeSet>(Enemy->GetDefaultSubobjectByName(TEXT("Attributes")));
        Enemy->GetAbilitySystemComponent()->AddAttributeSetSubobject(Attributes);
        Enemy->DispatchBeginPlay(); Enemy->SetActorTickEnabled(false);
        if (auto* Movement = Enemy->GetMovementComponent()) Movement->SetComponentTickEnabled(false);
        Attributes->ApplyMaxHealth(10000); Attributes->ApplyHealth(10000);
        return Enemy;
    };
    auto* Target = MakeEnemy(FVector(500, 0, 100));
    auto* Other = MakeEnemy(FVector(500, 500, 100));
    if (!Target || !Other) return false;
    auto* TargetCombat = Target->FindComponentByClass<UBreakerCombatComponent>();
    auto* Charge = Owner->FindComponentByClass<UBreakerChargeComponent>();
    auto Advance = [&](int32 Steps)
    {
        for (int32 I = 0; I < Steps; ++I)
        {
            TArray<UBreakerAbilityStateComponent*> States;
            for (TActorIterator<ABreakerCharacter> It(World); It; ++It)
                if (auto* State = It->FindComponentByClass<UBreakerAbilityStateComponent>())
                { State->SetComponentTickEnabled(false); States.Add(State); }
            ++GFrameCounter; World->Tick(LEVELTICK_All, .05f);
            for (auto* State : States) State->AdvanceTime(.05f);
            if (!World->GetTimerManager().HasBeenTickedThisFrame()) World->GetTimerManager().Tick(.05f);
        }
    };
    auto Settle = [&](ABreakerCharacter* Player)
    {
        // Completed authored campaign restoration fixture, not a playthrough claim.
        auto* Progression = Player->GetProgression();
        Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(50, FBreakerExperienceCurve()));
        FBreakerQuestFlagSet Flags;
        for (const auto& Mission : UBreakerMissionLibrary::GetMissions())
            for (const auto& Beat : Mission.Beats)
                for (FName Flag : UBreakerMissionLibrary::BeatCompletionFlags(Beat)) Flags.Add(Flag);
        Flags.Add(TEXT("Quest.Finale.Seal")); Progression->SettleDoctrineEntitlement(Flags);
        TestEqual(TEXT("authored campaign budget"), Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), 8);
    };
    const auto Mark = Owner->GetAbilitySystemComponent()->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Mark::StaticClass(), 1));
    auto AimAt = [&](ABreakerCharacter* Player, ABreakerEnemy* Enemy)
    {
        auto* Controller = Cast<APlayerController>(Player->GetController());
        if (!TestNotNull(TEXT("aim uses actual possessed controller"), Controller)) return;
        Controller->SetInitialLocationAndRotation(Player->GetActorLocation(),
            (Enemy->GetActorLocation() - Player->GetActorLocation()).Rotation());
        // Real camera origin may include the character's eye offset. Resolve
        // control rotation from that origin and refresh the same cached view
        // consumed by Mark after earlier world ticks.
        for (int32 I = 0; I < 3; ++I)
        {
            if (Controller->PlayerCameraManager) Controller->PlayerCameraManager->UpdateCamera(0);
            FVector Origin; FRotator Facing; Controller->GetPlayerViewPoint(Origin, Facing);
            Controller->SetControlRotation((Enemy->GetActorLocation() - Origin).Rotation());
        }
        if (Controller->PlayerCameraManager) Controller->PlayerCameraManager->UpdateCamera(0);
        FVector ViewOrigin; FRotator ViewFacing; Controller->GetPlayerViewPoint(ViewOrigin, ViewFacing);
        TestTrue(TEXT("actual camera points at intended Mark target"), FVector::DotProduct(ViewFacing.Vector(),
            (Enemy->GetActorLocation() - ViewOrigin).GetSafeNormal()) > .99f);
    };
    auto CastMark = [&](ABreakerEnemy* Enemy)
    {
        FGameplayTagContainer Cooldown; Cooldown.AddTag(GetDefault<UBreakerAbility_Mark>()->GetAbilityDefinition()->CooldownTag);
        Owner->GetAbilitySystemComponent()->RemoveActiveEffectsWithGrantedTags(Cooldown);
        AimAt(Owner, Enemy);
        Owner->GetAttributes()->ApplyClassResource(100);
        TestTrue(TEXT("actual aimed Mark cast"), Owner->GetAbilitySystemComponent()->TryActivateAbility(Mark));
        auto* State = Owner->FindComponentByClass<UBreakerAbilityStateComponent>();
        TestTrue(TEXT("weapon-channel aim selected real enemy"), State && State->GetMarkedTarget() == Enemy);
    };
    auto Hit = [&](AActor* Dealer, EBreakerDamageDelivery Delivery, float Proc = 1, bool Dot = false, float Damage = 100)
    {
        FBreakerDamageRequest Request; Request.BaseDamage = Damage; Request.Delivery = Delivery;
        Request.ProcCoefficient = Proc; Request.bIsDamageOverTime = Dot; Request.bCanCritical = false; Request.SetInstigator(Dealer);
        return TargetCombat->ReceiveDamage(Request);
    };
    auto Measure = [&](AActor* Dealer, EBreakerDamageDelivery Delivery, float Proc = 1, bool Dot = false, float Damage = 100)
    {
        Owner->GetAttributes()->ApplyClassResource(0); Charge->BindAttributes(Owner->GetAttributes()); Charge->SetInCombat(true);
        const auto Result = Hit(Dealer, Delivery, Proc, Dot, Damage);
        Charge->AdvanceLoop(1);
        return TPair<FBreakerDamageResult, float>(Result, Owner->GetAttributes()->GetClassResource());
    };
    Advance(1); CastMark(Target);
    const auto Base = Measure(Owner, EBreakerDamageDelivery::Weapon);
    TestTrue(TEXT("own successful weapon damage pays base Mark"), Base.Value > 0);
    TestEqual(TEXT("unowned Painted ability route pays nothing"), Measure(Owner, EBreakerDamageDelivery::Ability).Value, 0.0f);
    TestEqual(TEXT("unowned Painted DoT route pays nothing"), Measure(Owner, EBreakerDamageDelivery::Weapon, 1, true).Value, 0.0f);
    TestEqual(TEXT("unowned Painted ally route pays nothing"), Measure(Ally, EBreakerDamageDelivery::Weapon).Value, 0.0f);
    Settle(Owner); FText Reason;
    const auto* Warden = UBreakerProgressionLibrary::GetSupportWardenTree();
    // Painted is single rank (O272): one buy carries the ability and DoT
    // routes and the allied half yield together.
    if (!TestTrue(TEXT("actual Painted purchase"), Owner->GetProgression()->PurchaseNode(Warden, TEXT("Support.Warden.Painted"), Reason))) return false;
    const auto Ability = Measure(Owner, EBreakerDamageDelivery::Ability);
    TestEqual(TEXT("Painted own ability pays same health-fraction rate"), Ability.Value, Base.Value);
    TestEqual(TEXT("Painted DoT preserves fractional proc"), Measure(Owner, EBreakerDamageDelivery::Ability, .25f, true).Value, Base.Value * .25f);
    TestEqual(TEXT("Painted allies pay authored half yield"), Measure(Ally, EBreakerDamageDelivery::Weapon).Value, Base.Value * .5f);
    TestEqual(TEXT("zero proc cannot pay marked Charge"), Measure(Owner, EBreakerDamageDelivery::Weapon, 0).Value, 0.0f);
    TestEqual(TEXT("zero damage cannot pay marked Charge"), Measure(Owner, EBreakerDamageDelivery::Weapon, 1, false, 0).Value, 0.0f);
    TargetCombat->DodgeChance = 1;
    const auto Dodged = Measure(Owner, EBreakerDamageDelivery::Weapon);
    TestTrue(TEXT("real dodge occurs"), Dodged.Key.bDodged); TestEqual(TEXT("dodge cannot pay Charge"), Dodged.Value, 0.0f);
    TargetCombat->DodgeChance = 0;
    TestEqual(TEXT("nonplayer dealer cannot masquerade as ally"), Measure(Other, EBreakerDamageDelivery::Weapon).Value, 0.0f);
    Owner->GetAbilitySystemComponent()->CancelAbilityHandle(Mark);
    TestNull(TEXT("explicit cancel clears marked-target HUD state"), Owner->FindComponentByClass<UBreakerAbilityStateComponent>()->GetMarkedTarget());
    auto* DebtOwner = MakePlayer(FVector(0, -2000, 100));
    if (!TestNotNull(TEXT("separate full-budget Medic owner"), DebtOwner)) return false;
    Settle(DebtOwner);
    auto* DebtCharge = DebtOwner->FindComponentByClass<UBreakerChargeComponent>();
    AimAt(DebtOwner, Target);
    const auto DebtMark = DebtOwner->GetAbilitySystemComponent()->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Mark::StaticClass(), 1));
    TestTrue(TEXT("Medic's actual aimed mark"), DebtOwner->GetAbilitySystemComponent()->TryActivateAbility(DebtMark));
    TestTrue(TEXT("Medic mark actually selected intended enemy"), DebtOwner->FindComponentByClass<UBreakerAbilityStateComponent>()
        && DebtOwner->FindComponentByClass<UBreakerAbilityStateComponent>()->GetMarkedTarget() == Target);
    auto MeasureDebt = [&](AActor* Dealer, EBreakerDamageDelivery Delivery)
    {
        DebtOwner->GetAttributes()->ApplyClassResource(0); DebtCharge->BindAttributes(DebtOwner->GetAttributes()); DebtCharge->SetInCombat(true);
        const auto Result = Hit(Dealer, Delivery);
        DebtCharge->AdvanceLoop(1);
        return TPair<FBreakerDamageResult, float>(Result, DebtOwner->GetAttributes()->GetClassResource());
    };
    const auto* Medic = UBreakerProgressionLibrary::GetSupportMedicTree();
    // Single-rank nodes (O272): Blood Debt is Attending's impactful and
    // nothing else gates it.
    for (const TCHAR* Node : { TEXT("Support.Medic.FieldDressing"),
        TEXT("Support.Medic.Attending"), TEXT("Support.Medic.CleanHands"), TEXT("Support.Medic.BloodDebt") })
        if (!TestTrue(Node, DebtOwner->GetProgression()->PurchaseNode(Medic, Node, Reason))) return false;
    // Character BeginPlay is intentionally omitted to avoid owner saves; refresh
    // the real Charge node cache after purchases before the actual heal event.
    DebtCharge->BindAttributes(DebtOwner->GetAttributes());
    DebtOwner->GetAttributes()->ApplyHealth(DebtOwner->GetAttributes()->GetMaxHealth() - 20);
    DebtOwner->GetAttributes()->ApplyClassResource(100);
    const auto Patch = DebtOwner->GetAbilitySystemComponent()->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Patch::StaticClass(), 1));
    TestTrue(TEXT("actual Patch banks Blood Debt from real healing"), DebtOwner->GetAbilitySystemComponent()->TryActivateAbility(Patch));
    DebtCharge->AdvanceLoop(1); // Drain the separate actual healing generation before measuring Mark.
    const float Debt = DebtCharge->GetBloodDebtPool();
    if (!TestTrue(TEXT("real heal created a nonempty debt pool"), Debt > 0)) return false;
    MeasureDebt(DebtOwner, EBreakerDamageDelivery::Ability);
    TestEqual(TEXT("own ability damage cannot cash out weapon debt"), DebtCharge->GetBloodDebtPool(), Debt);
    MeasureDebt(Ally, EBreakerDamageDelivery::Weapon);
    TestEqual(TEXT("allied weapon damage cannot cash out owner's debt"), DebtCharge->GetBloodDebtPool(), Debt);
    const float BeforeDebtHit = Target->GetAbilitySystemComponent()->GetSet<UBreakerAttributeSet>()->GetHealth();
    const auto Cashout = MeasureDebt(DebtOwner, EBreakerDamageDelivery::Weapon);
    TestEqual(TEXT("own weapon spends entire debt once"), DebtCharge->GetBloodDebtPool(), 0.0f);
    const float AfterDebtHit = Target->GetAbilitySystemComponent()->GetSet<UBreakerAttributeSet>()->GetHealth();
    TestTrue(TEXT("actual settlement adds damage beyond triggering hit"), BeforeDebtHit - AfterDebtHit > Cashout.Key.HealthDamage);
    TestEqual(TEXT("proc-zero settlement cannot recursively generate Charge"), Cashout.Value, Base.Value);
    DebtOwner->GetAbilitySystemComponent()->CancelAbilityHandle(DebtMark);
    Owner->GetAbilitySystemComponent()->CancelAbilityHandle(Mark);
    TestEqual(TEXT("explicit cancellation detaches damage recipient listener"), Measure(Owner, EBreakerDamageDelivery::Weapon).Value, 0.0f);
    CastMark(Target); CastMark(Other);
    TestEqual(TEXT("actual retarget detaches previous target listener"), Measure(Owner, EBreakerDamageDelivery::Weapon).Value, 0.0f);
    CastMark(Target); Advance(450);
    TestEqual(TEXT("expired Mark detaches actual payout"), Measure(Owner, EBreakerDamageDelivery::Weapon).Value, 0.0f);
    Settle(Ally);
    // Single-rank travel roots (O272): Handoff has no prerequisite.
    for (const TCHAR* Node : { TEXT("Support.Warden.LongWatch"), TEXT("Support.Warden.Handoff") })
        if (!TestTrue(Node, Ally->GetProgression()->PurchaseNode(Warden, Node, Reason))) return false;
    AimAt(Ally, Target);
    Ally->GetAttributes()->ApplyClassResource(100);
    const auto AlliedMark = Ally->GetAbilitySystemComponent()->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Mark::StaticClass(), 1));
    TestTrue(TEXT("second caster actually marks shared target"), Ally->GetAbilitySystemComponent()->TryActivateAbility(AlliedMark));
    TestTrue(TEXT("second caster actually selected shared enemy"), Ally->FindComponentByClass<UBreakerAbilityStateComponent>()
        && Ally->FindComponentByClass<UBreakerAbilityStateComponent>()->GetMarkedTarget() == Target);
    CastMark(Target);
    TestEqual(TEXT("overlapping same Mark vulnerability uses strongest value"), Hit(Owner, EBreakerDamageDelivery::Weapon).HealthDamage, Base.Key.HealthDamage);
    Owner->GetAbilitySystemComponent()->CancelAbilityHandle(Mark);
    TestEqual(TEXT("canceling one actual Mark preserves other vulnerability"), Hit(Owner, EBreakerDamageDelivery::Weapon).HealthDamage, Base.Key.HealthDamage);
    CastMark(Other); // Nearest jump candidate is already marked by another caster.
    auto* Further = MakeEnemy(FVector(500, 900, 100));
    if (!TestNotNull(TEXT("unmarked Handoff destination"), Further)) return false;
    const float BeforeAlliedKill = Ally->GetAttributes()->GetClassResource();
    const auto Lethal = Hit(Owner, EBreakerDamageDelivery::Weapon, 1, false, 20000);
    TestTrue(TEXT("real allied lethal damage kills marked target"), Lethal.bKilled);
    auto* AlliedState = Ally->FindComponentByClass<UBreakerAbilityStateComponent>();
    TestTrue(TEXT("actual lethal Handoff skips nearest already-marked enemy"), AlliedState && AlliedState->GetMarkedTarget() == Further);
    TestEqual(TEXT("allied kill and transfer alone fabricate no owner Charge"), Ally->GetAttributes()->GetClassResource(), BeforeAlliedKill);
    Ally->GetAbilitySystemComponent()->CancelAbilityHandle(AlliedMark);
    Owner->GetAbilitySystemComponent()->CancelAbilityHandle(Mark);
    CastMark(Other);
    FBreakerDamageRequest OwnerDeath; OwnerDeath.BaseDamage = 100000; OwnerDeath.bCanCritical = false;
    Owner->GetCombat()->ReceiveDamage(OwnerDeath);
    TestTrue(TEXT("actual Mark owner dies"), Owner->GetCombat()->IsDead());
    TestNull(TEXT("source death clears marked-target HUD state"), Owner->FindComponentByClass<UBreakerAbilityStateComponent>()->GetMarkedTarget());
    TestFalse(TEXT("source death ends actual Mark instance"), Owner->GetAbilitySystemComponent()->FindAbilitySpecFromHandle(Mark)->IsActive());
    return true;
}
#endif

