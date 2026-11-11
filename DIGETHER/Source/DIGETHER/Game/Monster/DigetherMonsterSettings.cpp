#include "Game/Monster/DigetherMonsterSettings.h"

const UDigetherMonsterSettings& UDigetherMonsterSettings::Get()
{
	return *GetDefault<UDigetherMonsterSettings>();
}
