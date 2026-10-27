// 외부 확장용 공개 계약. 프로젝트 어댑터(예: 별도 DigetherWorkbench 플러그인)가 이 헤더만으로
// 새 도구를 등록할 수 있다. 설정(FWorkbenchToolDescriptor)만으로는 임의 코드를 실행하지 않는다 —
// MakeWidget은 호출자가 명시적으로 만든 위젯 팩토리일 뿐, 문자열·스크립트 실행 경로는 없다.
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Widgets/SWidget.h"

/**
 * 도구 1개(T01..T11 또는 프로젝트 어댑터가 추가하는 도구)를 식별하는 불변 설명자.
 * Id는 도크 탭 내 패널 선택과 메뉴 항목의 안정적인 키로 쓰인다.
 */
struct WORKBENCHEDITOR_API FWorkbenchToolDescriptor
{
	FWorkbenchToolDescriptor() = default;

	FWorkbenchToolDescriptor(
		FName InId,
		FText InTitle,
		FText InDescription,
		TFunction<TSharedRef<SWidget>()> InMakeWidget)
		: Id(InId)
		, Title(MoveTemp(InTitle))
		, Description(MoveTemp(InDescription))
		, MakeWidget(MoveTemp(InMakeWidget))
	{
	}

	bool IsValid() const
	{
		return !Id.IsNone() && MakeWidget != nullptr;
	}

	/** 안정적인 도구 ID. 예: "T01.ItemEditor". */
	FName Id;

	/** 메뉴·탭 제목에 표시할 한국어 설명. */
	FText Title;

	/** 도구가 하는 일과 현재 제한을 사용자에게 설명하는 한국어 문구. */
	FText Description;

	/** 패널 위젯을 만드는 팩토리. 설정 등록 시점에는 호출하지 않고 탭을 열 때만 호출한다. */
	TFunction<TSharedRef<SWidget>()> MakeWidget;
};

/**
 * DevelopmentWorkbench 모듈의 공개 인터페이스.
 * 프로젝트 어댑터 플러그인이 이 모듈에 로드 의존성을 걸고, StartupModule에서
 * IWorkbenchModule::Get().RegisterTool(...)을 호출해 자신의 패널을 추가한다.
 * 외부 등록 도구도 내부 11종과 동일한 도크 탭·메뉴 경로로 노출된다.
 */
class WORKBENCHEDITOR_API IWorkbenchModule : public IModuleInterface
{
public:
	static IWorkbenchModule& Get();
	static bool IsAvailable();

	/**
	 * 도구를 등록한다. 같은 Id가 이미 등록돼 있으면 거부하고 false를 반환한다
	 * (조용히 덮어써서 기존 도구를 잃지 않게 한다).
	 */
	virtual bool RegisterTool(const FWorkbenchToolDescriptor& Descriptor) = 0;

	/** 도구 등록을 해제한다. 등록돼 있지 않으면 아무 일도 하지 않는다. */
	virtual void UnregisterTool(FName ToolId) = 0;

	/** 현재 등록된 모든 도구 설명자를 등록 순서대로 반환한다. */
	virtual TArray<FWorkbenchToolDescriptor> GetRegisteredTools() const = 0;
};
