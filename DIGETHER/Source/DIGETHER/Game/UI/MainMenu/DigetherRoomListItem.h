#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Game/Online/DigetherRoomTypes.h"
#include "DigetherRoomListItem.generated.h"

// 방 목록(List View) 항목 데이터, 목록 위젯은 UObject 항목만 수용
UCLASS()
class DIGETHER_API UDigetherRoomListItem : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY() FDigetherRoomInfo Room;
};
