#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerSupportAbilities.h"
#include "Classes/BreakerChargeComponent.h"
#include "Combat/BreakerStatusComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionTree.h"
#include "Save/BreakerMissionContent.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerPurgePriorityRuntimeTest,
    "RiorsEdge.Abilities.PurgePriorityPaidImmunity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerPurgePriorityRuntimeTest::RunTest(const FString& Parameters)
{
    // Three cast-time target health fractions. Triage Priority is single rank
    // (O272) and Field Kit follows it, so every paid immunity window is the
    // scaled one; there is no unscaled Field Kit to walk.
    for (int32 Scenario = 0; Scenario < 3; ++Scenario)
    {
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated Purge world"), World)) return false;
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
    auto* Support = World->SpawnActor<ABreakerCharacter>(FVector(0, 0, 100), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("native Support"), Support)) return false;
    Support->SetActorTickEnabled(false); Support->GetBreakerMovement()->SetComponentTickEnabled(false);
    const float HalfHeight = Support->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    Support->SetActorLocation(FVector(0, 0, HalfHeight));
    auto* ASC = Support->GetAbilitySystemComponent();
    auto* Attributes = Support->GetAttributes();
    auto* Progression = Support->GetProgression();
    ASC->InitAbilityActorInfo(Support, Support); ASC->AddAttributeSetSubobject(Attributes);
    Support->GetCombat()->BindAttributes(Attributes); Progression->BindAttributes(Attributes);
    if (!TestTrue(TEXT("actual Support class"), Progression->ChoosePermanentClassById(EBreakerClassId::Support))) return false;
    // Restored benchmark entitlement fixture, not a claimed campaign run.
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(50, Progression->ExperienceCurve));
    FBreakerQuestFlagSet Flags;
    for (const auto& Mission : UBreakerMissionLibrary::GetMissions())
        for (const auto& Beat : Mission.Beats)
            for (FName Flag : UBreakerMissionLibrary::BeatCompletionFlags(Beat)) Flags.Add(Flag);
    Flags.Add(TEXT("Quest.Finale.Seal")); Progression->SettleDoctrineEntitlement(Flags);
    TestEqual(TEXT("actual entitlement is eight"), Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), 8);
    const auto* Tree = UBreakerProgressionLibrary::GetSupportMedicTree();
    FText Reason;
    // Single-rank nodes (O272): Field Dressing and Steady Hands are travel
    // roots; Field Kit is Triage Priority's impactful and buys behind it.
    for (const TCHAR* Node : { TEXT("Support.Medic.FieldDressing"), TEXT("Support.Medic.SteadyHands"),
        TEXT("Support.Medic.TriagePriority"), TEXT("Support.Medic.FieldKit") })
    {
        const bool bBought = Progression->PurchaseNode(Tree, Node, Reason);
        if (!TestTrue(FString::Printf(TEXT("purchase %s: %s"), Node, *Reason.ToString()), bBought)) return false;
    }
    TestEqual(TEXT("four nodes leave four of the eight"), Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), UBreakerProgressionLibrary::DoctrinePointGrant - 4);
    if (!Progression->IsAbilityUnlocked(TEXT("Support.Purge")))
        if (!TestTrue(TEXT("earned ability token buys Purge"), Progression->SpendAbilityToken(TEXT("Support.Purge"), Reason))) return false;
    auto* Charge = Support->GetCharge(); Charge->BindAttributes(Attributes); Charge->BeginPlay(); Charge->SetComponentTickEnabled(false);
    Charge->SetInCombat(true);
    auto* Combat = Support->GetCombat();
    // Actual damage and effective self healing fund bought Field Dressing.
    // No resource grant or fabricated healing notification.
    for (int32 Heal = 0; Heal < 12; ++Heal)
    {
        Charge->AdvanceLoop(1);
        FBreakerDamageRequest Hurt; Hurt.BaseDamage = Attributes->GetMaxHealth() * .25f;
        Hurt.DamageFamily = EBreakerDamageFamily::TrueDamage; Hurt.bCanCritical = false; Hurt.bCanBeAvoided = false;
        Combat->ReceiveDamage(Hurt);
        Combat->ApplyHealingAmount(Hurt.BaseDamage, Support, FGameplayTag());
        Charge->AdvanceLoop(1);
    }
    const float Fraction = Scenario == 0 ? 1.0f : Scenario == 1 ? .5f : .2f;
    if (Fraction < 1)
    {
        FBreakerDamageRequest Hurt; Hurt.BaseDamage = Attributes->GetMaxHealth() * (1 - Fraction);
        Hurt.DamageFamily = EBreakerDamageFamily::TrueDamage; Hurt.bCanCritical = false; Hurt.bCanBeAvoided = false;
        Combat->ReceiveDamage(Hurt);
    }
    const float BeforeCharge = Attributes->GetClassResource();
    TestTrue(TEXT("actual healing income funds thirty Charge"), BeforeCharge >= 30);
    const auto Handle = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Purge::StaticClass(), 1));
    if (!TestTrue(TEXT("actual paid Purge cast"), ASC->TryActivateAbility(Handle))) return false;
    TestEqual(TEXT("Purge pays authored thirty Charge"), Attributes->GetClassResource(), BeforeCharge - 30, .001f);
    auto* Status = Support->FindComponentByClass<UBreakerStatusComponent>();
    if (!TestNotNull(TEXT("native target status component"), Status)) return false;
    const float Duration = 3.0f * FMath::Lerp(.6f, 1.4f, 1 - Fraction);
    TestTrue(TEXT("paid Field Kit grants actual immunity"), Status->IsStatusImmune());
    FBreakerDamageRequest Entropy; Entropy.BaseDamage = 1; Entropy.DamageFamily = EBreakerDamageFamily::Elemental;
    Entropy.Element = EBreakerElement::Entropy; Entropy.ElementalFraction = 1; Entropy.bCanCritical = false; Entropy.bCanBeAvoided = false;
    Combat->ReceiveDamage(Entropy);
    TestEqual(TEXT("new elemental buildup refused during immunity"), Status->GetEntropyBuildup(), 0.0f);
    Status->AdvanceStatuses(Duration - .01f);
    TestTrue(TEXT("cast-time target health determines duration"), Status->IsStatusImmune());
    Status->AdvanceStatuses(.02f);
    TestFalse(TEXT("same window expires at scaled deadline"), Status->IsStatusImmune());
    Combat->ReceiveDamage(Entropy);
    TestTrue(TEXT("actual incoming buildup resumes after deadline"), Status->GetEntropyBuildup() > 0);
    }
    return true;
}
#endif
