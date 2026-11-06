#include "Game/Online/DigetherSessionText.h"

#include "Internationalization/Text.h"
#include "UObject/Class.h"

namespace DigetherSessionText
{
	const TCHAR* const TableId = TEXT("/Game/Frontend/Localization/ST_Session.ST_Session");

	FString GetKey(EDigetherSessionText Key)
	{
		return StaticEnum<EDigetherSessionText>()->GetNameStringByValue(static_cast<int64>(Key));
	}

	FText Get(EDigetherSessionText Key)
	{
		// 서브시스템 초기화(GameInstance Init) 때 첫 사용이면 기본 정책(FindOrLoad)은 로드 전이라 빈 항목 반환
		// 게임 스레드 전용·작은 표라 처음 한 번 동기 로드
		static const FName TableName(TableId);
		return FText::FromStringTable(TableName, GetKey(Key), EStringTableLoadingPolicy::FindOrFullyLoad);
	}
}
