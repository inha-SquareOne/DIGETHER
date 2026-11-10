#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

class APlayerController;
class UInputAction;

// 안내 문구용 키 이름 조회, 고정 키 대신 실제 적용 매핑(사용자 재설정 포함) 기준
// G0: 키보드·마우스 키만 표시(D20). 패드 지원 시 현재 입력 장치 기준 선택으로 확장
namespace DigetherInputPrompt
{
	// 표시 대상(키보드·마우스) 중 첫 키, 없으면 EKeys::Invalid
	DIGETHER_API FKey ChooseDisplayKey(TConstArrayView<FKey> Keys);

	// 키 재설정 화면과 같은 짧은 키 이름, 빈 키는 "키 미지정"
	DIGETHER_API FText MakeKeyText(const FKey& Key);

	// 로컬 플레이어의 현재 매핑에서 액션 키 이름, 액션·매핑이 없으면 "키 미지정"
	DIGETHER_API FText GetActionKeyText(const APlayerController* Controller, const UInputAction* Action);
}
