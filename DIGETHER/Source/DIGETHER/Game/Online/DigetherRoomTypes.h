#pragma once

#include "CoreMinimal.h"
#include "DigetherRoomTypes.generated.h"

// 방 목록 한 줄 표시 값, Index는 마지막 검색 결과 내 위치
USTRUCT(BlueprintType)
struct FDigetherRoomInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Digether|Session") int32 Index = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly, Category = "Digether|Session") FString RoomName;
	UPROPERTY(BlueprintReadOnly, Category = "Digether|Session") FString RoomCode;
	UPROPERTY(BlueprintReadOnly, Category = "Digether|Session") FString HostName;
	UPROPERTY(BlueprintReadOnly, Category = "Digether|Session") int32 CurrentPlayers = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Digether|Session") int32 MaxPlayers = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Digether|Session") bool bInGame = false;
	UPROPERTY(BlueprintReadOnly, Category = "Digether|Session") int32 PingMs = 0;
};

UENUM(BlueprintType)
enum class EDigetherSessionOperation : uint8
{
	Idle,
	Creating,
	Finding,
	Waiting,
	Joining,
	Destroying,
	Travelling
};
