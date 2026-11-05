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
		// 서브시스템 초기화(GameInstance Init) 때 처음 쓰이면 기본 정책(FindOrLoad)은 아직 로드 전이라 빈 항목을 돌려준다.
		// 상태 문구는 게임 스레드에서만 쓰고 표도 작으므로 처음 한 번 동기로 불러온다.
		return FText::FromStringTable(FName(TableId), GetKey(Key), EStringTableLoadingPolicy::FindOrFullyLoad);
	}
}
