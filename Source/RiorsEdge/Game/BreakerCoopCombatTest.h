#pragma once
#include "CoreMinimal.h"
class UWorld;
namespace BreakerCoopCombatTest
{
    // Explicit opt-in only. Never imports or writes a roster/account character.
    bool IsEnabled(const UWorld* World);
    // Geometry and lighting only. Character invokes this on remote clients;
    // authority continues to build its environment through GameMode.
    bool EnsureLocalEnvironment(UWorld* World);
}
