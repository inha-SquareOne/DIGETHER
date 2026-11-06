#include "DigetherDainGameMode.h"
#include "DigetherDainCharacter.h"
#include "DigetherDainHud.h"
#include "DigetherDainVerification.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
ADigetherDainGameMode::ADigetherDainGameMode()
{
	DefaultPawnClass = ADigetherDainCharacter::StaticClass();
	HUDClass = ADigetherDainHud::StaticClass();
}
void ADigetherDainGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (FParse::Param(FCommandLine::Get(), TEXT("LabVerify")))
	{
		GetWorld()->SpawnActor<ADigetherDainVerification>();
	}
}
