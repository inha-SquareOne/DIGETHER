// 'DIGETHER 개발 도구' 허브의 값 모델. 엔진 접근 없이 판정하는 순수 함수와, 실제 엔진 상태를 읽는 조회 함수를 나눈다.
// 조회 값은 읽은 시점의 스냅샷이며 기능 검증 결과가 아니다.
#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

enum class EDigetherPluginState : uint8
{
	NotFound,
	Disabled,
	Enabled
};

/** 표시 색상용 등급. 설정값이 있다는 사실만으로는 정상 등급을 만들지 않는다. */
enum class EDigetherDevToolsSeverity : uint8
{
	Neutral,
	Warning,
	Problem
};

struct FDigetherDevToolsRow
{
	FText Label;
	FText Value;
	FText Detail;
	EDigetherDevToolsSeverity Severity = EDigetherDevToolsSeverity::Neutral;
};

struct FDigetherAutoSaveValues
{
	bool bAvailable = false;
	bool bEnabled = false;
	bool bSaveMaps = false;
	bool bSaveContent = false;
	int32 IntervalMinutes = 0;
};

struct FDigetherAssetManagerValues
{
	bool bAvailable = false;
	bool bHasGameFeatureDataRule = false;
	int32 PrimaryAssetTypeCount = 0;
};

enum class EDigetherShortcutTarget : uint8
{
	GlobalTab,
	LevelEditorTab,
	SettingsSection
};

struct FDigetherShortcut
{
	FName Id;
	FText Label;
	FText Hint;
	EDigetherShortcutTarget Target = EDigetherShortcutTarget::GlobalTab;
	/** 이 바로가기가 요구하는 플러그인. 엔진 기본 기능이면 NAME_None. */
	FName PluginName;
	FName TabId;
	FName SettingsContainer;
	FName SettingsCategory;
	FName SettingsSection;
};

struct FDigetherShortcutAvailability
{
	bool bAvailable = false;
	FText FailureReason;
};

/** 바로가기 가용성 판정에 쓰는 엔진 상태 조회 수단. 테스트에서는 가짜로 교체한다. */
struct FDigetherDevToolsEnvironment
{
	TFunction<EDigetherPluginState(FName PluginName)> GetPluginState;
	TFunction<bool(FName TabId)> HasGlobalTab;
	TFunction<bool(FName TabId)> HasLevelEditorTab;
	TFunction<bool(FName Container, FName Category, FName Section)> HasSettingsSection;
};

/** 허브가 안내하는 유용 플러그인 이름. */
TArray<FName> GetDigetherDevToolsPluginNames();

FDigetherDevToolsRow DescribeDigetherEngineVersion(uint32 Major, uint32 Minor, uint32 Patch);
FDigetherDevToolsRow DescribeDigetherPlugin(FName PluginName, EDigetherPluginState State);
FDigetherDevToolsRow DescribeDigetherAutoSave(const FDigetherAutoSaveValues& Values);
FDigetherDevToolsRow DescribeDigetherAssetManagerRule(const FDigetherAssetManagerValues& Values);

TArray<FDigetherShortcut> GetDigetherShortcuts();

/** 플러그인 → 탭/설정 구획 순으로 확인하고, 실패하면 이유를 채운다. */
FDigetherShortcutAvailability EvaluateDigetherShortcut(const FDigetherShortcut& Shortcut, const FDigetherDevToolsEnvironment& Environment);

/** 정확한 엔진 메뉴 경로 안내 문구. 이 도구가 해당 기능을 실행하지 않는다. */
TArray<FText> GetDigetherMenuGuidance();

// ---- 실제 엔진 상태 조회 (Editor 전용, 버튼/생성 시에만 호출) ----

FDigetherDevToolsEnvironment MakeDigetherLiveEnvironment();
FDigetherAutoSaveValues ReadDigetherAutoSaveValues();
FDigetherAssetManagerValues ReadDigetherAssetManagerValues();

/** 클릭 직전에 가용성을 다시 확인하고 엔진 UI를 연다. 저장·설정 변경·검사 실행은 하지 않는다. */
bool TryOpenDigetherShortcut(const FDigetherShortcut& Shortcut, FText& OutFailureReason);
