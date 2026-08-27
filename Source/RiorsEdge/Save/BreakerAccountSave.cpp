#include "Save/BreakerAccountSave.h"

#include "Kismet/GameplayStatics.h"

UBreakerAccountSave* UBreakerAccountSave::CachedAccount = nullptr;

UBreakerAccountSave* UBreakerAccountSave::LoadOrCreate()
{
    if (CachedAccount) return CachedAccount;
    if (UGameplayStatics::DoesSaveGameExist(AccountSlotName(), 0))
    {
        if (UBreakerAccountSave* Loaded = Cast<UBreakerAccountSave>(
            UGameplayStatics::LoadGameFromSlot(AccountSlotName(), 0)))
        {
            // The roster's own rule, applied to the account: a file from a
            // newer build is refused, not repaired.
            if (Loaded->AccountVersion > CurrentAccountVersion)
            {
                UE_LOG(LogTemp, Error,
                    TEXT("Account save is version %d but this build understands %d. Refusing to load it; ")
                    TEXT("the file has NOT been modified."), Loaded->AccountVersion, CurrentAccountVersion);
                return nullptr;
            }
            Loaded->AddToRoot();
            CachedAccount = Loaded;
            return CachedAccount;
        }
    }
    UBreakerAccountSave* Fresh = Cast<UBreakerAccountSave>(
        UGameplayStatics::CreateSaveGameObject(UBreakerAccountSave::StaticClass()));
    if (Fresh) Fresh->AddToRoot();
    CachedAccount = Fresh;
    return CachedAccount;
}

void UBreakerAccountSave::InjectForTesting(UBreakerAccountSave* Account)
{
    ResetCacheForTesting();
    if (Account)
    {
        Account->AddToRoot();
        CachedAccount = Account;
    }
}

void UBreakerAccountSave::ResetCacheForTesting()
{
    if (CachedAccount) CachedAccount->RemoveFromRoot();
    CachedAccount = nullptr;
}

bool UBreakerAccountSave::SaveAccount() const
{
    if (bNeverPersist) return true;
    return UGameplayStatics::SaveGameToSlot(const_cast<UBreakerAccountSave*>(this), AccountSlotName(), 0);
}
