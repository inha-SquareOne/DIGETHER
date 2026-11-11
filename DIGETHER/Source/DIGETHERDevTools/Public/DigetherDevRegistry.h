#pragma once

#include "CoreMinimal.h"

class APlayerController;
class ADigetherGameplayGameMode;
class ADigetherGameplayPlayerController;
class ADigetherGameplayPlayerState;
class UWorld;

// 개발 지원 전용 로그 카테고리, 이름 표·사용 규칙은 docs/design/LOG_CATEGORIES.md
DIGETHERDEVTOOLS_API DECLARE_LOG_CATEGORY_EXTERN(LogDigetherDev, Log, All);

// 개발 명령 등록부: 명령 하나를 등록하면 콘솔 명령·웹 버튼·명령 문서가 함께 생김
// 이 모듈은 Shipping에서 빌드하지 않음(DIGETHER.uproject TargetConfigurationDenyList)
namespace DigetherDev
{
	enum class EParamType : uint8
	{
		Int,
		Float,
		Bool,
		// Options 중 하나(대소문자 무시)
		Enum,
		// self·all·PlayerId 숫자, 호스트에서 참가자로 해석
		Player,
		Text
	};

	enum class EScope : uint8
	{
		// 호스트 판정 변경, 참가자 프로세스에서 실행하면 거절
		Host,
		// 실행한 프로세스의 화면·요청만 다룸(참가자도 가능, 판정은 각 서버 RPC가 검사)
		Local
	};

	enum class ESource : uint8
	{
		Console,
		Web,
		Test
	};

	struct FParam
	{
		FString Name;
		EParamType Type = EParamType::Text;
		bool bOptional = false;
		FString Default;
		TArray<FString> Options;
		TOptional<double> Min;
		TOptional<double> Max;
		FString Hint;
	};

	struct FCommandDesc
	{
		FName Name;
		// 웹 화면 탭 이름(요청서 4·5절 분류)
		FString Category;
		// 요청서 항목 ID, 여러 개면 쉼표
		FString RequestIds;
		FString Summary;
		TArray<FParam> Params;
		EScope Scope = EScope::Host;
		// 저장 슬롯 초기화처럼 되돌리기 어려운 명령, 웹 화면이 확인 창 표시
		bool bDestructive = false;
		// 게임 오버(실패 사건·크레딧) 중 허용, 기본은 거절
		bool bAllowDuringGameOver = false;
		// 게임 맵(Gameplay GameMode·컨트롤러) 필요
		bool bRequiresGameplayWorld = true;
		// 판정 상태를 바꾸는 명령, 성공 시 "치트 사용됨" 표시
		bool bChangesState = true;
		// 비어 있지 않으면 실행 거절하고 이 이유를 응답(선행 시스템 대기)
		FString UnavailableReason;

		DIGETHERDEVTOOLS_API FString MakeUsage() const;
	};

	struct FContext
	{
		UWorld* World = nullptr;
		// 지금 화면을 가진 로컬 컨트롤러, 자유 카메라 중에는 디버그 카메라 컨트롤러
		APlayerController* ViewController = nullptr;
		// 원래 게임 컨트롤러, 자유 카메라 중에도 게임 Pawn·권한 판정 대상
		ADigetherGameplayPlayerController* GameController = nullptr;
		ESource Source = ESource::Console;

		DIGETHERDEVTOOLS_API bool IsHost() const;
		DIGETHERDEVTOOLS_API ADigetherGameplayGameMode* GetGameMode() const;
	};

	struct FResult
	{
		bool bOk = false;
		FString Message;

		static FResult Ok(FString InMessage) { return FResult{ true, MoveTemp(InMessage) }; }
		static FResult Fail(FString InMessage) { return FResult{ false, MoveTemp(InMessage) }; }
	};

	using FHandler = TFunction<FResult(const FContext& Context, const TArray<FString>& Args)>;

	// 같은 이름은 교체, 콘솔 명령도 함께 등록, 이름은 Digether 접두사 필수
	DIGETHERDEVTOOLS_API bool Register(FCommandDesc Desc, FHandler Handler);
	DIGETHERDEVTOOLS_API void Unregister(FName Name);
	DIGETHERDEVTOOLS_API void UnregisterAll();
	DIGETHERDEVTOOLS_API const FCommandDesc* Find(FName Name);
	// 분류 등록 순서 → 이름 순
	DIGETHERDEVTOOLS_API TArray<const FCommandDesc*> GetCommands();

	// 월드의 로컬 플레이어 기준 문맥, 로컬 플레이어가 없으면 컨트롤러 비어 있음
	DIGETHERDEVTOOLS_API FContext MakeContext(UWorld* World, ESource Source);
	// 웹 요청 대상 월드: 게임·PIE 월드 중 호스트(단독·리슨) 우선
	DIGETHERDEVTOOLS_API UWorld* FindPrimaryGameWorld();

	// 빈 문자열이면 통과, 아니면 사용자에게 보여 줄 거절 사유
	DIGETHERDEVTOOLS_API FString ValidateArgs(const FCommandDesc& Desc, const TArray<FString>& Args);
	DIGETHERDEVTOOLS_API FResult Execute(FName Name, const TArray<FString>& Args, const FContext& Context);

	// Player 인자 해석, 호스트 문맥에서만 성공
	DIGETHERDEVTOOLS_API bool ResolvePlayers(const FContext& Context, const FString& Token,
		TArray<ADigetherGameplayPlayerState*>& OutPlayers, FString& OutError);

	DIGETHERDEVTOOLS_API FString DescribeCommandsJson();
	DIGETHERDEVTOOLS_API FString DescribeCommandsMarkdown();

	// 이 프로세스에서 판정 변경 명령이 한 번이라도 성공했는지
	DIGETHERDEVTOOLS_API bool WasCheatUsed();
}
