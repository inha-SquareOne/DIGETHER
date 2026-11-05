#include "Game/Economy/DigetherFloorCoinActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// 임시 표시 크기, 정식 금화 메시·머티리얼은 아트 담당이 BP 또는 에셋으로 교체
	const FVector FloorCoinMeshScale(0.4, 0.4, 0.05);
}

ADigetherFloorCoinActor::ADigetherFloorCoinActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetCanBeDamaged(false);

	// 저장·복원하는 액터 변환 크기를 1로 유지하려고 표시 크기는 자식 메시에만 적용
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	CoinMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoinMesh"));
	CoinMesh->SetupAttachment(SceneRoot);
	CoinMesh->SetRelativeScale3D(FloorCoinMeshScale);
	// 상호작용 시선 검사(ECC_Visibility)를 가리지 않도록 충돌 끔, 줍기 판정은 서버 거리 검사
	CoinMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CoinMesh->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		CoinMesh->SetStaticMesh(CylinderMesh.Object);
	}
}

bool ADigetherFloorCoinActor::InitializeServerState(const FGuid& InCoinId, const FName InStationId,
	const int32 InAmountCredits)
{
	if (!HasAuthority() || CoinId.IsValid() || !InCoinId.IsValid() || InStationId.IsNone() || InAmountCredits < 1)
	{
		return false;
	}
	CoinId = InCoinId;
	StationId = InStationId;
	AmountCredits = InAmountCredits;
	bConsumed = false;
	return true;
}

bool ADigetherFloorCoinActor::AddAmount(const int32 Credits)
{
	// 포화 시 더한 돈 일부가 사라지므로 넘치는 합치기 거절, 호출자가 차감을 줄여 돈을 지갑에 남김
	if (!HasAuthority() || bConsumed || Credits < 1
		|| static_cast<int64>(AmountCredits) + Credits > MAX_int32)
	{
		return false;
	}
	AmountCredits += Credits;
	return true;
}

bool ADigetherFloorCoinActor::RemoveAmount(const int32 Credits)
{
	if (!HasAuthority() || Credits < 1 || AmountCredits - Credits < 1)
	{
		return false;
	}
	AmountCredits -= Credits;
	return true;
}
