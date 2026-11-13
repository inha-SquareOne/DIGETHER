#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DigetherSoilField.h"
#include "DigetherDiggableSoil.generated.h"
class UProceduralMeshComponent;

UENUM(BlueprintType)
enum class EDigetherSoilEditResult : uint8
{
	Rejected,
	NoChange,
	Applied,
	ApplyFailed
};

// 밀도는 Actor 전체 배열 하나에서 조회, 청크는 셀 범위와 활성 버퍼만 보유
struct FDigetherSoilChunk
{
	// 전역 셀 범위 [Min, Max), Max 제외
	FIntVector Min = FIntVector::ZeroValue;
	FIntVector Max = FIntVector::ZeroValue;
	// ChunkMeshes의 두 버퍼 중 현재 표시·충돌에 사용 중인 쪽
	bool bFrontActive = true;
	// 청크별 연속 두 버퍼의 전역 인덱스, 교체 전 상태로 조회
	int32 GetActiveMeshIndex(int32 ChunkIndex) const { return ChunkIndex * 2 + (bFrontActive ? 0 : 1); }
	int32 GetCandidateMeshIndex(int32 ChunkIndex) const { return ChunkIndex * 2 + (bFrontActive ? 1 : 0); }
	// Solid는 흙 내부의 무표면 상태, 이동 가능한 공기와 구분
	DigetherSoil::EDigetherSoilRangeState State = DigetherSoil::EDigetherSoilRangeState::Failed;
};

// 지형 처리 전용, 장비·거리·사용 간격은 서버 도구 호출부에서 검증
UCLASS()
class DIGETHER_API ADigetherDiggableSoil : public AActor
{
	GENERATED_BODY()
public:
	ADigetherDiggableSoil();
	virtual void OnConstruction(const FTransform& Transform) override;
	// 월드 타격점·반경(cm) 입력 → 거부/무변경/적용/실패, 성공 시에만 Revision 증가
	UFUNCTION(BlueprintCallable, Category = "Digether|Excavation")
	EDigetherSoilEditResult TryExcavate(FVector WorldPoint, float Radius);
	// 현재 규격·모드로 원상 초기화, 실패 시 기존 지형 유지
	UFUNCTION(BlueprintCallable, Category = "Digether|Excavation")
	bool ResetSoil();
	// 0은 기존 구형, 최대 반경 안쪽의 약한 굴곡. 시험값이며 기획 승인값 아님
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digether|Brush", meta = (ClampMin = "0", ClampMax = "0.2"))
	float BrushIrregularity = 0.f;
	// 0은 완화 끔. 새 절삭면의 한 셀 이웃만 밀도 감소, 반경 바깥 가장자리도 소폭 영향
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digether|Brush", meta = (ClampMin = "0", ClampMax = "0.5"))
	float CutEdgeSoftening = 0.f;
	// 마지막 초기화 이후 성공한 굴착 횟수, 네트워크 전송 구현과 별개
	uint32 GetRevision() const { return Revision; }
	// 확정 밀도 CRC32, 회귀 비교용이며 충돌 상태의 완전한 증명은 아님
	uint32 GetDensityHash() const;
	// 개인 비교용 0/8/16 입력 → 성공 여부, 모드 변경 시 굴착 내역도 초기화
	bool SetComparisonChunkCells(int32 Cells);
	int32 GetComparisonChunkCells() const { return AppliedChunkCells; }
	int32 GetLastUpdatedChunks() const { return LastUpdatedChunks; }
	int32 GetLastGeneratedTriangles() const { return LastGeneratedTriangles; }

	// 마지막 성공 요청의 월드 변경 상자, 붕괴·보강 영역 판정과 구분
	FBox GetLastChangedWorldBounds() const { return LastChangedWorldBounds; }

	// 축별 셀 개수, 밀도 표본 수는 각 축 +1의 곱
	UPROPERTY(EditAnywhere, Category = "Digether|Excavation", meta = (ClampMin = "4", ClampMax = "64"))
	FIntVector GridCells = FIntVector(8, 8, 8);
	UPROPERTY(EditAnywhere, Category = "Digether|Excavation", meta = (ClampMin = "2", ClampMax = "200", Units = "cm"))
	// cm 단위, 기존 맵 직렬화 이름 유지
	double CellSpacing = 50.0;
	// 로컬 보호 영역, 계산 코어에서 한 셀 여유 적용
	UPROPERTY(EditAnywhere, Category = "Digether|Excavation")
	TArray<FBox> ProtectedRegions;

protected:
	virtual void BeginPlay() override;

private:
	friend class FDigetherSoilActorTestAccess;
	// 공유 후보 밀도·청크 크기 입력 → 전 후보 충돌 준비 성공 시에만 확정
	bool ApplyChunks(const DigetherSoil::FDigetherSoilGrid& Grid, const TArray<uint8>& Candidate, int32 ChunkCells,
					 bool bReset);
	// 빈 출력 배열에 초기화 후보 구성, 적용·실패 시 폐기는 ApplyChunks 책임
	void CreateChunkBuffers(const DigetherSoil::FDigetherSoilGrid& Grid, int32 ChunkCells,
		TArray<FDigetherSoilChunk>& OutChunks, TArray<TObjectPtr<UProceduralMeshComponent>>& OutMeshes);
	// 등록된 청크 PMC 소유 해제·파괴, 전체 모드 PMC는 유지
	void ClearChunks();
	// 미등록/비활성 PMC 입력 → 동기 삼각형 충돌 설정, 객체 소유권 변경 없음
	static void ConfigureMesh(UProceduralMeshComponent& Mesh);
	// 표시·충돌 동시 전환, 활성화는 후보 준비 성공 후 호출
	static void SetMeshEnabled(UProceduralMeshComponent& Mesh, bool bEnabled);
	// 비활성 PMC·표면 입력 → 정상 빈 표면 또는 충돌 생성 성공 여부, 활성 지형 불변
	static bool PrepareMesh(UProceduralMeshComponent& Mesh, const DigetherSoil::FDigetherSoilSurface& Surface);
	// 청크당 PMC 두 개의 GC 참조, 후보도 AddInstanceComponent로 Actor에 등록
	UPROPERTY(Transient)
	TArray<TObjectPtr<UProceduralMeshComponent>> ChunkMeshes;
	// 밀도 복제 없이 셀 범위·버퍼·표면 분류만 보관
	TArray<FDigetherSoilChunk> Chunks;
	// 실제 적용된 모드, 0 전체 / 8·16 청크
	int32 AppliedChunkCells = 0;
	// 최근 요청의 재생성 청크·삼각형 수, 거부 요청도 0으로 갱신
	int32 LastUpdatedChunks = 0;
	int32 LastGeneratedTriangles = 0;
#if WITH_DEV_AUTOMATION_TESTS
	// 테스트 전용 후보 실패 순서, -1은 주입 안 함
	int32 FailPreparedChunkForTest = -1;
#endif
	// 후보 밀도 입력 → 전체 표면·충돌 준비 성공 시 확정, 실패 시 기존 상태 유지
	bool Apply(const DigetherSoil::FDigetherSoilGrid& Grid, const TArray<uint8>& Candidate);
	// 전체 모드의 두 버퍼, 기존 BP 서브오브젝트 이름 유지
	UPROPERTY(VisibleAnywhere, Category = "Digether|Excavation")
	TObjectPtr<UProceduralMeshComponent> SoilMesh;
	UPROPERTY(VisibleAnywhere, Category = "Digether|Excavation")
	TObjectPtr<UProceduralMeshComponent> SoilBack;
	// 성공한 지형의 규격·전역 밀도, 편집 중인 GridCells/CellSpacing과 구분
	DigetherSoil::FDigetherSoilGrid AppliedGrid;
	TArray<uint8> Density;
	FBox LastChangedWorldBounds = FBox(ForceInit);
	uint32 Revision = 0;
	bool bFrontActive = false;
	// 확정 지형 준비 여부, 초기 준비 실패 시 굴착 요청 거부
	bool bReady = false;
};
