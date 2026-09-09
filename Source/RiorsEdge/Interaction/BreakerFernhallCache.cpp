#include "Interaction/BreakerFernhallCache.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Items/BreakerDropTable.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerLootLibrary.h"
#include "Items/BreakerLootPickup.h"
#include "Net/UnrealNetwork.h"
ABreakerFernhallCache::ABreakerFernhallCache()
{
    bReplicates = true; ConfigureConsoleBody();
    DisplayName = FText::FromString(TEXT("Fernhall cache"));
    DialogueId = NAME_None; DialogueNodes.Reset(); EntryOverrides.Reset();
    Tags.Add(TEXT("Fernhall.Cache"));
}
void ABreakerFernhallCache::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ABreakerFernhallCache, bOpened);
    DOREPLIFETIME(ABreakerFernhallCache, bCleared);
}
void ABreakerFernhallCache::Configure(int32 AreaLevel, const TArray<ABreakerEnemy*>& Pocket, int32 ExpectedMembers)
{
    if (!HasAuthority() || bConfigured) return;
    bConfigured = true; ItemLevel = FMath::Max(1, AreaLevel);
    bRosterValid = ExpectedMembers > 0 && Pocket.Num() == ExpectedMembers;
    for (ABreakerEnemy* Guard : Pocket)
    {
        auto* Combat = IsValid(Guard) ? Guard->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
        if (!Combat || Guard->GetWorld() != GetWorld() || Guards.Contains(Guard)) { bRosterValid = false; continue; }
        Guards.Add(Guard); ObservedDeaths.Add(false);
        Combat->OnDeath.AddUniqueDynamic(this, &ABreakerFernhallCache::ObserveGuardDeaths);
    }
    ObserveGuardDeaths();
}
void ABreakerFernhallCache::ObserveGuardDeaths()
{
    if (!HasAuthority() || bOpened) return;
    for (int32 Index = 0; Index < Guards.Num(); ++Index)
    {
        if (ObservedDeaths[Index]) continue;
        ABreakerEnemy* Guard = Guards[Index].Get();
        const auto* Combat = IsValid(Guard) ? Guard->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
        if (Combat && Combat->IsDead()) ObservedDeaths[Index] = true;
        else if (!Combat || Guard->IsActorBeingDestroyed()) bRosterValid = false;
    }
    bCleared = bRosterValid && !Guards.IsEmpty() && !ObservedDeaths.Contains(false);
}
FText ABreakerFernhallCache::GetCachePrompt() const
{
    if (bOpened) return FText::GetEmpty();
    return FText::FromString(bCleared ? TEXT("OPEN CACHE") : TEXT("CLEAR NEARBY HOSTILES"));
}
bool ABreakerFernhallCache::TryOpen(ABreakerCharacter* Player)
{
    if (!HasAuthority() || IsActorBeingDestroyed() || bOpened || !IsValid(Player) || !Player->HasAuthority()
        || Player->GetWorld() != GetWorld() || !Player->GetCombat() || Player->GetCombat()->IsDead()
        || !Player->GetEquipment() || FVector::DistSquared(Player->GetActorLocation(), GetActorLocation()) > FMath::Square(InteractionRange)) return false;
    FCollisionQueryParams Visibility(SCENE_QUERY_STAT(FernhallCacheVisibility), false, Player);
    Visibility.AddIgnoredActor(this);
    FHitResult Obstruction;
    if (GetWorld()->LineTraceSingleByObjectType(Obstruction, Player->GetActorLocation(), GetActorLocation(),
        FCollisionObjectQueryParams(ECC_WorldStatic), Visibility)) return false;
    ObserveGuardDeaths(); if (!bCleared) return false;
    // Claim before spawn callbacks; only a failed physical spawn releases it.
    bOpened = true;
    const int32 Seed = FMath::Rand();
    const auto Rarity = UBreakerDropTableLibrary::RollGatedRarity(Seed, ItemLevel, EBreakerMonsterRank::Trash,
        Player->GetEquipment()->GetStats().DropChancePercent, FBreakerDropTableParams());
    const auto Item = UBreakerLootLibrary::RollItem(TEXT("FernhallCache"), UBreakerLootLibrary::RollDropSlot(Seed), Rarity, ItemLevel, Seed);
    // O2 PLACEHOLDER: reward rests on the console's approach side.
    const FVector At = GetActorLocation() + GetActorForwardVector() * 120.f - FVector(0,0,40.f);
    auto* Pickup = GetWorld()->SpawnActor<ABreakerLootPickup>(ABreakerLootPickup::StaticClass(), At, FRotator::ZeroRotator);
    if (!Pickup) { bOpened = false; return false; }
    Pickup->SetItem(Item); return true;
}
