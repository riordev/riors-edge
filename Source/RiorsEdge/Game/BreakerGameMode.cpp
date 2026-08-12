#include "Game/BreakerGameMode.h"

#include "Characters/BreakerCharacter.h"

ABreakerGameMode::ABreakerGameMode()
{
    DefaultPawnClass = ABreakerCharacter::StaticClass();
}
