#include "Misc/AutomationTest.h"
#include "Tests/BreakerCastTestHelpers.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbility_Rot.h"
#include "Abilities/BreakerAbilityTags.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerManaComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerStatusComponent.h"
#include "Combat/BreakerZoneActor.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerRotNodesRuntimeTest, "RiorsEdge.Abilities.RotPurchasedZones",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerRotNodesRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated zone world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
    ABreakerCharacter* Player = World->SpawnActor<ABreakerCharacter>();
    if (!TestNotNull(TEXT("real caster"), Player)) return false;
    UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Player->GetAttributes());
    Player->GetCombat()->BindAttributes(Player->GetAttributes());
    UBreakerProgressionComponent* Progression = Player->GetProgression();
    Progression->BindAttributes(Player->GetAttributes());
    TestTrue(TEXT("actual Caster selection"), Progression->ChoosePermanentClassById(EBreakerClassId::Caster));
    // Full doctrine-budget wiring fixture, not a claim that campaign grants it yet.
    Progression->GrantPlaytestPoints(8, 0);
    UBreakerManaComponent* Mana = Player->GetMana();
    Mana->BindAttributes(Player->GetAttributes()); Mana->PassiveRegenPerSecond = 0;
    Player->GetAttributes()->ApplyClassResource(0);
    const UBreakerProgressionTree* Tree = UBreakerProgressionLibrary::GetCasterVoidWhispererTree();
    auto Buy = [&](const TCHAR* Id)
    {
        FText Reason;
        const bool bBought = Progression->PurchaseNode(Tree, Id, Reason);
        return TestTrue(FString::Printf(TEXT("%s: %s"), Id, *Reason.ToString()), bBought);
    };
    auto Victim = [&](FVector Location)
    {
        AActor* Actor = World->SpawnActor<AActor>();
        USphereComponent* Body = NewObject<USphereComponent>(Actor);
        Actor->AddInstanceComponent(Body); Actor->SetRootComponent(Body);
        Body->SetSphereRadius(35); Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Body->SetCollisionResponseToAllChannels(ECR_Ignore); Body->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);
        Body->RegisterComponent(); Actor->SetActorLocation(Location);
        UBreakerCombatComponent* Combat = NewObject<UBreakerCombatComponent>(Actor);
        Actor->AddInstanceComponent(Combat); Combat->RegisterComponent();
        UBreakerAttributeSet* Attributes = NewObject<UBreakerAttributeSet>(Actor);
        Attributes->ApplyMaxHealth(1000); Attributes->ApplyHealth(1000); Combat->BindAttributes(Attributes);
        UBreakerStatusComponent* Status = NewObject<UBreakerStatusComponent>(Actor);
        Actor->AddInstanceComponent(Status); Status->RegisterComponent();
        return Actor;
    };
    AActor* Enemy = Victim(FVector(300, 0, 0));
    AActor* Second = Victim(FVector(350, 0, 0));
    FBreakerZoneSpec Plain;
    Plain.ZoneTag = BreakerAbilityTags::Zone_Caster_Rot.GetTag(); Plain.Duration = 10;
    Plain.FlatArmorReduction = 40;
    auto Zone = [&](const FBreakerZoneSpec& Spec)
    {
        ABreakerZoneActor* Result = World->SpawnActor<ABreakerZoneActor>();
        Result->ConfigureZone(Spec, Player); Result->AdvanceZone(0);
        return Result;
    };
    ABreakerZoneActor* First = Zone(Plain);
    ABreakerZoneActor* Other = Zone(Plain);
    Mana->AdvanceLoop(1);
    TestEqual(TEXT("unowned Standing Water pays nothing"), Mana->GetMana(), 0.0f);
    if (!Buy(TEXT("Caster.VoidWhisperer.StandingWater"))) return false;
    Mana->AdvanceLoop(1);
    TestEqual(TEXT("two bodies and overlapping zones pay one rank-one stream"), Mana->GetMana(), 2.0f);
    if (!Buy(TEXT("Caster.VoidWhisperer.StandingWater"))) return false;
    Mana->AdvanceLoop(1);
    TestEqual(TEXT("rank two stream"), Mana->GetMana(), 6.0f);
    First->ReleaseAllOccupants(); Other->ReleaseAllOccupants();
    Plain.Duration = 0.25f;
    ABreakerZoneActor* Short = Zone(Plain);
    Mana->AdvanceLoop(1);
    TestEqual(TEXT("income is clipped to remaining zone lifetime"), Mana->GetMana(), 7.0f);
    Short->AdvanceZone(1); Mana->AdvanceLoop(1);
    TestEqual(TEXT("expired zone pays nothing"), Mana->GetMana(), 7.0f);
    Plain.Duration = 10;
    ABreakerZoneActor* Live = Zone(Plain);
    FBreakerDamageRequest Kill; Kill.BaseDamage = 2000; Kill.bCanCritical = false;
    Enemy->FindComponentByClass<UBreakerCombatComponent>()->ReceiveDamage(Kill);
    Second->FindComponentByClass<UBreakerCombatComponent>()->ReceiveDamage(Kill);
    Mana->AdvanceLoop(1);
    TestEqual(TEXT("corpses never occupy income stream"), Mana->GetMana(), 7.0f);
    Live->ReleaseAllOccupants();
    FText RespecReason;
    if (!TestTrue(TEXT("real respec reuses the same eight-point fixture budget"), Progression->RespecAtForge(EBreakerPointCurrency::DoctrinePoints, true, RespecReason))
        || !Buy(TEXT("Caster.VoidWhisperer.StandingWater"))) return false;

    // Actual GAS Rot casts exercise purchased payload and self-placement.
    Enemy = Victim(FVector(100, 0, 0));
    AActor* Floor = World->SpawnActor<AActor>();
    UBoxComponent* Ground = NewObject<UBoxComponent>(Floor);
    Floor->AddInstanceComponent(Ground); Floor->SetRootComponent(Ground);
    Ground->SetBoxExtent(FVector(2000, 2000, 10)); Ground->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Ground->SetCollisionResponseToAllChannels(ECR_Block); Ground->RegisterComponent();
    Floor->SetActorLocation(FVector(0, 0, -110));
    APlayerController* Controller = World->SpawnActor<APlayerController>();
    Controller->Possess(Player); Controller->SetControlRotation(FRotator(-90, 0, 0));
    Player->GetBreakerMovement()->SetMovementMode(MOVE_Walking);
    Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    const FGameplayAbilitySpecHandle RotHandle = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Rot::StaticClass(), 1));
    auto CastRot = [&]()
    {
        Player->GetAttributes()->ApplyClassResource(100);
        TestTrue(TEXT("real Rot activation"), ASC->TryActivateAbility(RotHandle));
        BreakerResolvePendingCast(World, Player);
        ABreakerZoneActor* Found = nullptr;
        for (const TWeakObjectPtr<ABreakerZoneActor>& Held : ABreakerZoneActor::GetLiveZones())
            if (ABreakerZoneActor* Candidate = Held.Get())
                if (Candidate->GetZoneInstigator() == Player && Candidate->GetRemainingDuration() > 0
                    && Candidate != First && Candidate != Other && Candidate != Live && Candidate != Short)
                {
                    if (Candidate->GetFollowActor() == Player) return Candidate;
                    Found = Candidate;
                }
        return Found;
    };
    ABreakerZoneActor* Baseline = CastRot();
    if (!TestNotNull(TEXT("unowned cast zone"), Baseline)) return false;
    TestNull(TEXT("unowned self aim stays ground zone"), Baseline->GetFollowActor());
    TestEqual(TEXT("unowned Zonework adds nothing"), Baseline->GetSpec().AfflictedArmorReduction, 0.0f);
    Baseline->ReleaseAllOccupants(); Baseline->Destroy();
    // Two tier-one investments unlock tier two; four total unlock tier three.
    // Both tier-three nodes cost two, using exactly the same eight-point pool.
    if (!Buy(TEXT("Caster.VoidWhisperer.Seep")) || !Buy(TEXT("Caster.VoidWhisperer.Lingering"))
        || !Buy(TEXT("Caster.VoidWhisperer.Attrition")) || !Buy(TEXT("Caster.VoidWhisperer.Wellspring"))
        || !Buy(TEXT("Caster.VoidWhisperer.Zonework"))) return false;
    ABreakerZoneActor* Mobile = CastRot();
    if (!TestNotNull(TEXT("purchased cast zone"), Mobile)) return false;
    TestEqual(TEXT("Wellspring follows actual caster"), Mobile->GetFollowActor(), static_cast<AActor*>(Player));
    TestEqual(TEXT("Zonework authored flat bonus reaches real zone"), Mobile->GetSpec().AfflictedArmorReduction, 20.0f);
    // Earn the prerequisite through the actual elemental hit seam. Rot no
    // longer gives a free Poison status merely for entering its volume.
    auto* EnemyStatus = Enemy->FindComponentByClass<UBreakerStatusComponent>();
    UBreakerCombatComponent* EnemyCombat = Enemy->FindComponentByClass<UBreakerCombatComponent>();
    Mobile->AdvanceZone(0); // Refresh following-zone membership before reading its base strip.
    TestEqual(TEXT("unafflicted target receives only base strip"), EnemyCombat->GetComposedArmorReduction(), 40.0f);
    FBreakerDamageRequest EntropyHit;
    EntropyHit.BaseDamage = EnemyStatus->GetEntropyThreshold();
    EntropyHit.Element = EBreakerElement::Entropy; EntropyHit.ElementalFraction = 1;
    EntropyHit.bCanCritical = false; EntropyHit.SetInstigator(Player);
    EnemyCombat->ReceiveDamage(EntropyHit);
    TestTrue(TEXT("real accepted hit earns prerequisite Rot"), EnemyStatus->HasStatus(FGameplayTag::RequestGameplayTag(TEXT("Status.Rot"))));
    Mobile->AdvanceZone(0);
    TestEqual(TEXT("target with earned Rot receives extra flat armour strip"), EnemyCombat->GetComposedArmorReduction(), 60.0f);
    Plain.Duration = 10;
    ABreakerZoneActor* Weak = Zone(Plain);
    TestEqual(TEXT("weak overlapping zone cannot overwrite stronger strip"), EnemyCombat->GetComposedArmorReduction(), 60.0f);
    Mobile->AdvanceZone(1);
    ABreakerZoneActor* Refreshed = CastRot();
    TestEqual(TEXT("recast refreshes one following zone"), Refreshed, Mobile);
    const FVector FollowOffset = Mobile->GetActorLocation() - Player->GetActorLocation();
    Player->SetActorLocation(FVector(900, 0, 0)); Mobile->AdvanceZone(0);
    TestTrue(TEXT("following zone keeps ground offset while moving"), Mobile->GetActorLocation().Equals(Player->GetActorLocation() + FollowOffset));
    TestTrue(TEXT("mobile footprint is replicated with zone spec"), Mobile->GetSpec().bMobileFootprint);
    TestEqual(TEXT("leaving strong zone retains only weaker overlap"), EnemyCombat->GetComposedArmorReduction(), 40.0f);
    Weak->ReleaseAllOccupants();
    TestEqual(TEXT("final zone exit releases strip"), EnemyCombat->GetComposedArmorReduction(), 0.0f);
    const UBreakerAbility_Rot* Rot = GetDefault<UBreakerAbility_Rot>();
    TestFalse(TEXT("sky miss is not self-placement"), Rot->ShouldFollowCaster(Player, false, Player->GetActorLocation(), FVector::UpVector));
    TestFalse(TEXT("wall hit is not self-placement"), Rot->ShouldFollowCaster(Player, true, Player->GetActorLocation(), FVector::ForwardVector));
    Player->GetBreakerMovement()->SetMovementMode(MOVE_Falling);
    TestFalse(TEXT("airborne self aim is not self-placement"), Rot->ShouldFollowCaster(Player, true, Player->GetActorLocation(), FVector::UpVector));
    return true;
}
#endif
