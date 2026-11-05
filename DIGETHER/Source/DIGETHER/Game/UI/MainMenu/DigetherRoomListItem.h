#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Game/Online/DigetherRoomTypes.h"
#include "DigetherRoomListItem.generated.h"

// 방 목록(List View)의 항목 데이터. 목록 위젯은 UObject 항목을 받는다.
UCLASS()
class DIGETHER_API UDigetherRoomListItem : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY() FDigetherRoomInfo Room;
};
