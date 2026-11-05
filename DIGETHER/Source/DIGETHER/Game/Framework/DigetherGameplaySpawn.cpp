#include "Game/Framework/DigetherGameplayGameMode.h"

#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerStart.h"

namespace
{
	const UPrimitiveComponent* ResolveSpawnPrimitive(const ADigetherGameplayGameMode& Mode, const APawn* Pawn)
	{
		const APawn* ShapePawn = Pawn;
		if (!IsValid(ShapePawn) && Mode.DefaultPawnClass != nullptr)
		{
			// Pawn 생성 전 기본 클래스의 충돌 크기 사용
			ShapePawn = Mode.DefaultPawnClass->GetDefaultObject<APawn>();
		}
		return IsValid(ShapePawn) ? Cast<UPrimitiveComponent>(ShapePawn->GetRootComponent()) : nullptr;
	}
}

bool ADigetherGameplayGameMode::IsUsableHubSpawnTransform(const APawn* Pawn, const FTransform& Transform,
	const TArray<FTransform>& ReservedTransforms) const
{
	const UWorld* World = GetWorld();
	const UPrimitiveComponent* Primitive = ResolveSpawnPrimitive(*this, Pawn);
	if (!IsValid(World) || !IsValid(Primitive) || Transform.ContainsNaN()
		|| !Transform.GetRotation().IsNormalized() || !HubSpawnAllowedBounds.IsValid
		|| !HubSpawnAllowedBounds.IsInsideOrOn(Transform.GetLocation()))
	{
		return false;
	}
	// 저장 스케일 변경에 따른 충돌 검증·실제 배치 불일치 방지
	const AActor* ShapeOwner = Primitive->GetOwner();
	if (!IsValid(ShapeOwner) || !Transform.GetScale3D().Equals(ShapeOwner->GetActorScale3D(), KINDA_SMALL_NUMBER)
		|| (IsValid(Pawn) && Primitive->Mobility != EComponentMobility::Movable))
	{
		return false;
	}
	// 바닥 접촉 오차를 침투로 오인하지 않도록 1cm 축소 질의
	const FCollisionShape Shape = Primitive->GetCollisionShape(-1.0f);
	if (Shape.IsNearlyZero())
	{
		return false;
	}
	const FVector Extent = Shape.GetExtent();
	const double MinimumSpacing = 2.0 * FMath::Max(Extent.X, Extent.Y) + 2.0;
	// 배치된 플레이어와 복원 예정 위치의 중복 점유 방지
	for (const FTransform& Reserved : ReservedTransforms)
	{
		const FVector Delta = Reserved.GetLocation() - Transform.GetLocation();
		if (FMath::Abs(Delta.Z) < 2.0 * Extent.Z
			&& FVector2D(Delta.X, Delta.Y).SizeSquared() < FMath::Square(MinimumSpacing))
		{
			return false;
		}
	}
	// 현재 Pawn을 막는 정적 지형만 장애물 판정
	FCollisionQueryParams Query(SCENE_QUERY_STAT(DigetherSavedHubSpawn), false, Pawn);
	const FCollisionObjectQueryParams Objects(ECC_WorldStatic);
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Transform.GetLocation(), Transform.GetRotation(), Objects, Shape, Query);
	for (const FOverlapResult& Overlap : Overlaps)
	{
		const UPrimitiveComponent* Obstacle = Overlap.GetComponent();
		if (IsValid(Obstacle) && Obstacle->GetCollisionResponseToChannel(Primitive->GetCollisionObjectType()) == ECR_Block)
		{
			return false;
		}
	}
	FHitResult Floor;
	// 바닥 없는 저장 위치 복원 방지용 200cm 지지면 검사
	if (!World->LineTraceSingleByObjectType(Floor, Transform.GetLocation(),
		Transform.GetLocation() - FVector(0.0, 0.0, 200.0), Objects, Query)
		|| !Floor.bBlockingHit || Floor.ImpactNormal.Z < 0.5)
	{
		return false;
	}
	const UPrimitiveComponent* FloorComponent = Floor.GetComponent();
	return IsValid(FloorComponent)
		&& FloorComponent->GetCollisionResponseToChannel(Primitive->GetCollisionObjectType()) == ECR_Block;
}

bool ADigetherGameplayGameMode::ResolveHubSpawnTransform(const APawn* Pawn, const FTransform* SavedTransform,
	const TArray<FTransform>& ReservedTransforms, FTransform& OutTransform, bool& bOutUsedFallback) const
{
	if (SavedTransform != nullptr && (!bValidateSavedHubSpawn
		|| IsUsableHubSpawnTransform(Pawn, *SavedTransform, ReservedTransforms)))
	{
		OutTransform = *SavedTransform;
		bOutUsedFallback = false;
		return true;
	}
	if (GetWorld() == nullptr)
	{
		return false;
	}
	TArray<APlayerStart*> Starts;
	for (TActorIterator<APlayerStart> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		Starts.Add(*Iterator);
	}
	// 기존 맵의 첫 시작점 선택 유지
	// 새 거점만 이름순으로 빈 시작점 선택
	if (bValidateSavedHubSpawn)
	{
		Starts.Sort([](const APlayerStart& Left, const APlayerStart& Right) { return Left.GetName() < Right.GetName(); });
	}
	for (const APlayerStart* Start : Starts)
	{
		const FTransform Candidate = Start->GetActorTransform();
		if (!bValidateSavedHubSpawn || IsUsableHubSpawnTransform(Pawn, Candidate, ReservedTransforms))
		{
			OutTransform = Candidate;
			bOutUsedFallback = SavedTransform != nullptr;
			return true;
		}
	}
	// 안전한 대체 위치 부재 시 호출자의 출력·진행 상태 보존
	return false;
}
