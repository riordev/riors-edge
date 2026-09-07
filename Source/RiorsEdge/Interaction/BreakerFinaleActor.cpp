#include "Interaction/BreakerFinaleActor.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Game/BreakerGameInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Save/BreakerMissionContent.h"
#include "Save/BreakerQuestJournal.h"

ABreakerFinaleActor::ABreakerFinaleActor()
{
    bReplicates = true;
    // This is an unattended console, not a disguised person or damage target.
    Head->SetVisibility(false);
    Visual->SetRelativeScale3D(FVector(.80f,.60f,.70f));
    Visual->SetRelativeLocation(FVector(0,0,-50));
    Trim->SetRelativeRotation(FRotator::ZeroRotator);
    Trim->SetRelativeScale3D(FVector(.70f,.45f,.06f));
    Trim->SetRelativeLocation(FVector(0,0,-10));
}
void ABreakerFinaleActor::BeginPlay()
{
    Super::BeginPlay();
    RefreshPresentation();
}
void ABreakerFinaleActor::ConfigureFragment(int32 PocketCount)
{
    if (!HasAuthority()) return;
    bDevice = false; ClearedPockets.Init(false, FMath::Max(0, PocketCount));
    if (HasActorBegunPlay()) RefreshPresentation();
}
void ABreakerFinaleActor::ConfigureDevice()
{
    if (!HasAuthority()) return;
    bDevice = true; ClearedPockets.Reset();
    if (HasActorBegunPlay()) RefreshPresentation();
}
void ABreakerFinaleActor::RefreshPresentation()
{
    const FString RowId = bDevice ? TEXT("FinaleDevice") : TEXT("FinaleFragment");
    if (const auto* Row = GetDialogueData().Npcs.FindByPredicate([&](const FBreakerDialogueRow& Entry) { return Entry.Id == RowId; }))
    {
        DisplayName = FText::FromString(Row->DisplayName); StartNodeId = Row->StartNodeId;
        DialogueNodes = Row->Nodes; EntryOverrides = Row->Entries;
    }
    else UE_LOG(LogTemp, Error, TEXT("Finale interaction missing authored row %s"), *RowId);
    Head->SetVisibility(false);
    Visual->SetRelativeScale3D(bDevice ? FVector(1.15f,.85f,1.1f) : FVector(.80f,.60f,.70f));
    Visual->SetRelativeLocation(FVector(0,0,bDevice ? -30 : -50));
    Trim->SetRelativeLocation(FVector(0,0,bDevice ? 30 : -10));
    if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
    {
        UMaterialInstanceDynamic* Shell = UMaterialInstanceDynamic::Create(Base, Visual);
        Shell->SetVectorParameterValue(TEXT("Color"), FLinearColor(.15f,.21f,.23f)); Visual->SetMaterial(0,Shell);
        UMaterialInstanceDynamic* Panel = UMaterialInstanceDynamic::Create(Base, Trim);
        Panel->SetVectorParameterValue(TEXT("Color"), bDevice ? FLinearColor(.25f,.70f,.62f) : FLinearColor(.80f,.58f,.25f)); Trim->SetMaterial(0,Panel);
    }
}
void ABreakerFinaleActor::SetPocketCleared(int32 PocketIndex, bool bCleared)
{
    if (HasAuthority() && ClearedPockets.IsValidIndex(PocketIndex)) ClearedPockets[PocketIndex] = bCleared;
}
bool ABreakerFinaleActor::AreAllPocketsCleared() const { return !ClearedPockets.IsEmpty() && !ClearedPockets.Contains(false); }
bool ABreakerFinaleActor::IsEligibleInteractor(ABreakerCharacter* Player) const
{
    return HasAuthority() && Player && Player->HasAuthority() && Player->GetWorld() == GetWorld()
        && Player->GetCombat() && !Player->GetCombat()->IsDead() && Player->GetQuestJournal()
        && FVector::Dist(Player->GetActorLocation(), GetActorLocation()) <= InteractionRange;
}
bool ABreakerFinaleActor::TryRecoverFragment(ABreakerCharacter* Player)
{
    if (!IsFragment() || !IsEligibleInteractor(Player) || !UBreakerGameInstance::IsStrippedEarthMap(this)
        || !AreAllPocketsCleared()) return false;
    UBreakerQuestJournal* Journal = Player->GetQuestJournal();
    if (!Journal->HasFlag(TEXT("Quest.Survivor.TurnedIn")) || !Journal->HasFlag(TEXT("Quest.Finale.Accepted"))
        || !Journal->HasFlag(TEXT("Mission.Act3.Finale.Arrived")) || Journal->HasFlag(TEXT("Quest.Finale.FragmentRecovered"))) return false;
    const TArray<FName> Flags = UBreakerMissionLibrary::WorldEncounterCompletionFlagsFor(TEXT("earth.rior_fragment"), Journal->GetState());
    if (!Flags.Contains(TEXT("Quest.Finale.FragmentRecovered"))) return false;
    bool bChanged = false;
    for (FName Flag : Flags) bChanged |= Journal->SetFlag(Flag);
    return bChanged;
}
bool ABreakerFinaleActor::TryChooseFinale(ABreakerCharacter* Player, bool bSeal)
{
    if (!IsDevice() || !IsEligibleInteractor(Player) || !UBreakerGameInstance::IsAnchorMap(this)
        || !Player->GetProgression() || Player->GetProgression()->GetCharacterLevel() < 50) return false;
    UBreakerQuestJournal* Journal = Player->GetQuestJournal();
    for (const FName Required : {FName(TEXT("Quest.Survivor.TurnedIn")), FName(TEXT("Quest.Finale.Accepted")),
        FName(TEXT("Quest.Finale.FragmentRecovered")), FName(TEXT("Quest.Finale.ReturnedWithFragment")),
        FName(TEXT("Quest.Finale.Reconstructed")), FName(TEXT("Quest.Finale.ArrivedWon")),
        FName(TEXT("Quest.Finale.MetAlternate")), FName(TEXT("Quest.Finale.ReturnedFromWon"))})
        if (!Journal->HasFlag(Required)) return false;
    return Journal->CommitExclusiveChoice(bSeal ? TEXT("Quest.Finale.Seal") : TEXT("Quest.Finale.Hold"),
        bSeal ? TEXT("Quest.Finale.Hold") : TEXT("Quest.Finale.Seal"), TEXT("Quest.Finale.TurnedIn"));
}

bool ABreakerFinaleActor::TryMeetAlternate(ABreakerNPC* NPC, ABreakerCharacter* Player)
{
    if (!NPC || !Player || !NPC->HasAuthority() || !Player->HasAuthority() || NPC->GetWorld()!=Player->GetWorld()
        || !UBreakerGameInstance::IsWinningEarthMap(NPC) || !NPC->ActorHasTag(TEXT("AlternateSelf"))
        || NPC->FindComponentByClass<UBreakerCombatComponent>() || !Player->GetCombat() || Player->GetCombat()->IsDead()
        || !Player->GetQuestJournal() || FVector::Dist(NPC->GetActorLocation(),Player->GetActorLocation())>NPC->GetInteractionRange()) return false;
    const FName Met(TEXT("Quest.Finale.MetAlternate"));
    for (const auto& Mission : UBreakerMissionLibrary::GetMissions())
        if (Mission.MissionId==TEXT("Act3.Finale"))
        {
            const auto* Beat=UBreakerMissionLibrary::CurrentBeat(Mission,Player->GetQuestJournal()->GetState());
            if (!Beat || Beat->CompletesOn!=Met) return false;
            Player->AddQuestFlag(Met);
            return Player->GetQuestJournal()->HasFlag(Met);
        }
    return false;
}
