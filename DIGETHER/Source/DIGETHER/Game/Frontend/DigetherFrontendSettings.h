#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "DigetherFrontendSettings.generated.h"

class UCommonActivatableWidget;
class UDigetherFrontendLayoutWidget;

// 메인 메뉴·대기실·게임 시작 맵과 화면 위젯 블루프린트. 프로젝트 설정 > Game > DIGETHER Frontend에서 바꾼다.
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "DIGETHER Frontend"))
class DIGETHER_API UDigetherFrontendSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const UDigetherFrontendSettings* Get() { return GetDefault<UDigetherFrontendSettings>(); }

	FString GetMainMenuMapName() const { return MainMenuMap.GetLongPackageName(); }
	FString GetLobbyMapName() const { return LobbyMap.GetLongPackageName(); }
	FString GetGameMapName() const { return GameMap.GetLongPackageName(); }

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	UPROPERTY(Config, EditAnywhere, Category = "Maps", meta = (AllowedClasses = "/Script/Engine.World"))
	FSoftObjectPath MainMenuMap;

	UPROPERTY(Config, EditAnywhere, Category = "Maps", meta = (AllowedClasses = "/Script/Engine.World"))
	FSoftObjectPath LobbyMap;

	// 대기실에서 '게임 시작'을 누르면 모든 참가자가 이동하는 맵.
	UPROPERTY(Config, EditAnywhere, Category = "Maps", meta = (AllowedClasses = "/Script/Engine.World"))
	FSoftObjectPath GameMap;

	// 화면 스택을 담는 최상위 레이아웃(WBP).
	UPROPERTY(Config, EditAnywhere, Category = "UI")
	TSoftClassPtr<UDigetherFrontendLayoutWidget> FrontendLayoutClass;

	UPROPERTY(Config, EditAnywhere, Category = "UI")
	TSoftClassPtr<UCommonActivatableWidget> MainMenuScreenClass;

	UPROPERTY(Config, EditAnywhere, Category = "UI")
	TSoftClassPtr<UCommonActivatableWidget> LobbyScreenClass;
};
