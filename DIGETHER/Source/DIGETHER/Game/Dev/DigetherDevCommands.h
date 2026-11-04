#pragma once

#include "CoreMinimal.h"

// 개발 치트 명령의 등록 틀. 각 패키지가 끝날 때 자기 명령의 처리기를 등록한다.
// 처리기가 없으면 "아직 연결 안 됨"으로 응답한다. Shipping 빌드에서는 어떤 명령도 실행하지 않는다.
namespace DigetherDev
{
	// 명령 처리기. 인자 문자열을 받아 사용자에게 보여 줄 응답을 돌려준다.
	using FCommandHandler = TFunction<FString(const TArray<FString>& Args)>;

	struct FCommandInfo
	{
		FName Name;
		// 처리기를 붙일 패키지.
		const TCHAR* OwnerPackage;
		const TCHAR* Summary;
	};

	// 이 틀이 예약한 명령 목록. 명령을 추가하려면 여기와 UDigetherCheatManager의 Exec 함수를 함께 늘린다.
	DIGETHER_API TConstArrayView<FCommandInfo> GetPlannedCommands();

	// Shipping이 아닐 때만 참이다.
	DIGETHER_API bool IsAvailable();

	// 처리기를 등록한다. 같은 이름이면 교체한다. 예약 목록에 없는 이름·빈 처리기는 거절한다.
	DIGETHER_API bool Register(FName Command, FCommandHandler Handler);
	DIGETHER_API void Unregister(FName Command);
	DIGETHER_API bool IsConnected(FName Command);

	// 명령을 실행하고 응답을 돌려준다. Shipping·미등록·예약 외 이름은 각각 다른 응답이다.
	DIGETHER_API FString Execute(FName Command, const TArray<FString>& Args);

	DIGETHER_API FString MakeNotConnectedMessage(FName Command);
	DIGETHER_API FString MakeUnavailableMessage();
}
