#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "InputCoreTypes.h"
#include "DigetherOptionsSettings.generated.h"

class UInputMappingContext;
class USoundClass;
class USoundMix;

// 설정 화면이 쓰는 에셋 연결, 프로젝트 설정 > Game > DIGETHER Options에서 지정
// 비어 있으면 해당 항목은 저장만 하고 적용 생략
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "DIGETHER Options"))
class DIGETHER_API UDigetherOptionsSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const UDigetherOptionsSettings* Get() { return GetDefault<UDigetherOptionsSettings>(); }

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	// 효과음·음악 볼륨을 싣는 SoundMix, 항상 적용 상태로 유지
	UPROPERTY(Config, EditAnywhere, Category = "Audio")
	TSoftObjectPtr<USoundMix> VolumeSoundMix;

	UPROPERTY(Config, EditAnywhere, Category = "Audio")
	TSoftObjectPtr<USoundClass> EffectsSoundClass;

	UPROPERTY(Config, EditAnywhere, Category = "Audio")
	TSoftObjectPtr<USoundClass> MusicSoundClass;

	// 키 재설정 목록에 올릴 입력 컨텍스트, 액션의 Player Mappable Key Settings가 있어야 행으로 표시
	UPROPERTY(Config, EditAnywhere, Category = "Input")
	TArray<TSoftObjectPtr<UInputMappingContext>> RebindableMappingContexts;

	// 음성 컴포넌트에 누르고 말하기 액션 에셋이 없을 때 쓰는 고정 키(2026-11-06 임시값 V), 키 재설정 대상 아님
	UPROPERTY(Config, EditAnywhere, Category = "Voice")
	FKey FallbackPushToTalkKey = EKeys::V;
};
