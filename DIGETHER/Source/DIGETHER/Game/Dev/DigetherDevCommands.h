#pragma once

#include "CoreMinimal.h"

// 개발 치트 명령 등록 틀, 각 패키지가 자기 명령 처리기 등록
// 처리기가 없으면 "아직 연결 안 됨" 응답, Shipping 빌드에서는 어떤 명령도 실행하지 않음
namespace DigetherDev
{
	// 인자 문자열을 받아 사용자에게 보여 줄 응답 반환
	using FCommandHandler = TFunction<FString(const TArray<FString>& Args)>;

	struct FCommandInfo
	{
		FName Name;
		// 처리기를 붙일 패키지
		const TCHAR* OwnerPackage;
		const TCHAR* Summary;
	};

	// 예약 명령 목록, 추가 시 DigetherDevCommands.cpp의 PlannedCommands와 UDigetherCheatManager Exec 함수를 함께 추가
	DIGETHER_API TConstArrayView<FCommandInfo> GetPlannedCommands();

	// Shipping이 아닐 때만 참
	DIGETHER_API bool IsAvailable();

	// 같은 이름은 교체, 예약 목록 밖 이름·빈 처리기는 거절
	DIGETHER_API bool Register(FName Command, FCommandHandler Handler);
	DIGETHER_API void Unregister(FName Command);
	DIGETHER_API bool IsConnected(FName Command);

	// 실행 후 응답 반환, Shipping·미등록·예약 외 이름은 각각 다른 응답
	DIGETHER_API FString Execute(FName Command, const TArray<FString>& Args);

	DIGETHER_API FString MakeNotConnectedMessage(FName Command);
}
