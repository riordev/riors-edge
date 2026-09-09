#include "Game/BreakerContainmentHunt.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerCombatComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Save/BreakerQuestJournal.h"

UBreakerContainmentHunt::UBreakerContainmentHunt() { PrimaryComponentTick.bCanEverTick=false; }
FName UBreakerContainmentHunt::TargetTag() { return TEXT("StationZero.ContainmentCustodian"); }
FName UBreakerContainmentHunt::CompletionFlag() { return TEXT("Quest.StationZero.CustodianDown"); }
void UBreakerContainmentHunt::AttachTo(ABreakerEnemy* Target)
{
    if (!Target || !Target->HasAuthority() || Target->FindComponentByClass<UBreakerContainmentHunt>()) return;
    Target->Tags.AddUnique(TargetTag());
    auto* Component=NewObject<UBreakerContainmentHunt>(Target);
    Target->AddInstanceComponent(Component); Component->RegisterComponent();
}
void UBreakerContainmentHunt::BeginPlay()
{
    Super::BeginPlay();
    if (GetOwner() && GetOwner()->HasAuthority())
        if (auto* Combat=GetOwner()->FindComponentByClass<UBreakerCombatComponent>())
            Combat->OnDeath.AddUniqueDynamic(this,&UBreakerContainmentHunt::OnTargetDeath);
}
void UBreakerContainmentHunt::OnTargetDeath()
{
    const auto* Combat=GetOwner() ? GetOwner()->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
    if (bClaimed || !GetOwner() || !GetOwner()->HasAuthority() || !Combat || !Combat->IsDead()) return;
    // Claim before journal persistence/reward observers can reenter. This
    // solo destination uses the same local recipient as native enemy XP/loot.
    bClaimed=true;
    auto* PC=GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    auto* Player=PC ? Cast<ABreakerCharacter>(PC->GetPawn()) : nullptr;
    if (Player && Player->GetQuestJournal()) Player->GetQuestJournal()->SetFlag(CompletionFlag());
    // The actual enemy already pays its ordinary fixed-region XP/loot once.
    // No second scripted drop or repeated journal reward is minted here.
}
