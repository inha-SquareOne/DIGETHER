#include "Game/Settings/DigetherUserOptionsSubsystem.h"

#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "Game/Settings/DigetherGameUserSettings.h"
#include "Game/Settings/DigetherKeyBindings.h"
#include "UObject/UObjectGlobals.h"

void UDigetherUserOptionsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	// PlayerControllerChanged 순서상 Enhanced Input이 먼저 사용자 설정(저장된 키)을 만든 뒤 이 서브시스템이 등록
	Collection.InitializeDependency<UEnhancedInputLocalPlayerSubsystem>();
	Super::Initialize(Collection);
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostLoadMap);
}

void UDigetherUserOptionsSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	Super::Deinitialize();
}

void UDigetherUserOptionsSubsystem::PlayerControllerChanged(APlayerController* NewPlayerController)
{
	Super::PlayerControllerChanged(NewPlayerController);
	UDigetherKeyBindingLibrary::RegisterRebindableContexts(GetLocalPlayer());
	// PIE는 맵 로드 방송 없이 시작해 여기서도 저장값 적용
	HandlePostLoadMap(nullptr);
}

void UDigetherUserOptionsSubsystem::HandlePostLoadMap(UWorld* World)
{
	if (UDigetherGameUserSettings* Settings = UDigetherGameUserSettings::Get())
	{
		Settings->ApplyRuntimeSettings();
	}
}
