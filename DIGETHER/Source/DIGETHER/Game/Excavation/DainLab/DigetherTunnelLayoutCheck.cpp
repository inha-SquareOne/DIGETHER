#include "DigetherTunnelLayoutCheck.h"
#include "Engine/Engine.h"
#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"

DEFINE_LOG_CATEGORY_STATIC(LogDigetherTunnelLayout, Log, All);

const FName ADigetherTunnelLayoutCheck::EntranceTag(TEXT("Tunnel.DungeonEntrance"));

ADigetherTunnelLayoutCheck::ADigetherTunnelLayoutCheck()
{
	PrimaryActorTick.bCanEverTick = false;
}

bool ADigetherTunnelLayoutCheck::CheckLayout()
{
	StartPoint = nullptr;
	DungeonEntrance = nullptr;
	int32 StartCount = 0;
	int32 EntranceCount = 0;
	APlayerStart* FoundStart = nullptr;
	ATargetPoint* FoundEntrance = nullptr;
	if (IsValid(GetWorld()))
	{
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			// 다른 하위 레벨·거점의 시작점은 이 맵의 중복으로 계산하지 않음
			if (!IsValid(*It) || It->GetLevel() != GetLevel()) continue;
			if (APlayerStart* Point = Cast<APlayerStart>(*It))
			{
				FoundStart = Point;
				++StartCount;
			}
			if (ATargetPoint* Point = Cast<ATargetPoint>(*It); Point && Point->ActorHasTag(EntranceTag))
			{
				FoundEntrance = Point;
				++EntranceCount;
			}
		}
	}

	const bool bValid = StartCount == 1 && EntranceCount == 1;
	CheckMessage = bValid
		? NSLOCTEXT("DigetherTunnel", "PointsReady", "필수 지점 정상: 시작점 1개, 던전 입구 1개 (통행·충돌 검사는 별도)")
		: FText::Format(NSLOCTEXT("DigetherTunnel", "InvalidPoints",
			"필수 지점 오류: 시작점 {0}개, 던전 입구 {1}개 — 각각 1개 필요 (0: 누락, 2 이상: 중복). 입구는 Tunnel.DungeonEntrance 태그가 있는 TargetPoint"),
			FText::AsNumber(StartCount), FText::AsNumber(EntranceCount));
	if (bValid)
	{
		StartPoint = FoundStart;
		DungeonEntrance = FoundEntrance;
		UE_LOG(LogDigetherTunnelLayout, Display, TEXT("%s: %s"), *GetName(), *CheckMessage.ToString());
	}
	else
	{
		UE_LOG(LogDigetherTunnelLayout, Warning, TEXT("%s: %s"), *GetName(), *CheckMessage.ToString());
	}
	return bValid;
}

void ADigetherTunnelLayoutCheck::BeginPlay()
{
	Super::BeginPlay();
	const bool bValid = CheckLayout();
#if !UE_BUILD_SHIPPING
	// 개인 시험 안내만 표시, 실패해도 공통 런 상태나 플레이어를 변경하지 않음
	if (GEngine)
	{
		constexpr float MessageSeconds = 12.f;
		GEngine->AddOnScreenDebugMessage(INDEX_NONE, MessageSeconds,
			bValid ? FColor::Green : FColor::Red, CheckMessage.ToString());
	}
#endif
}
