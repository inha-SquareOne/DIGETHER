#include "DigetherExcavationLabGameMode.h"
#include "DigetherExcavationLabCharacter.h"
#include "DigetherExcavationLabHud.h"
#include "DigetherExcavationLabVerification.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
ADigetherExcavationLabGameMode::ADigetherExcavationLabGameMode()
{
	DefaultPawnClass = ADigetherExcavationLabCharacter::StaticClass();
	HUDClass = ADigetherExcavationLabHud::StaticClass();
}
// -LabVerify 명시 실행에서만 검증 Actor 생성
void ADigetherExcavationLabGameMode::BeginPlay()
{
	Super::BeginPlay();
#if !UE_BUILD_SHIPPING
	// 자동 조작·프로세스 종료는 개발 검증에서만 허용
	if (FParse::Param(FCommandLine::Get(), TEXT("LabVerify")))
	{
		GetWorld()->SpawnActor<ADigetherExcavationLabVerification>();
	}
#endif
}
