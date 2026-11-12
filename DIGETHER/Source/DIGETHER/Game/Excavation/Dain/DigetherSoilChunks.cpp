#include "DigetherDiggableSoil.h"
#include "DigetherSoilProfiling.h"
#include "ProceduralMeshComponent.h"

void ADigetherDiggableSoil::ClearChunks()
{
	for (UProceduralMeshComponent* Mesh : ChunkMeshes)
	{
		if (IsValid(Mesh))
		{
			RemoveInstanceComponent(Mesh);
			Mesh->DestroyComponent();
		}
	}
	ChunkMeshes.Reset();
	Chunks.Reset();
}

bool ADigetherDiggableSoil::SetComparisonChunkCells(int32 Cells)
{
	if (!HasAuthority() || (Cells != 0 && Cells != 8 && Cells != 16))
	{
		return false;
	}
	FVector Local;
	if (!DigetherSoil::WorldToLocal(GetActorTransform(), GetActorLocation(), Local))
	{
		return false;
	}
	DigetherSoil::FDigetherSoilGrid Grid;
	Grid.Cells = GridCells;
	Grid.SpacingCm = CellSpacing;
	TArray<uint8> Initial;
	if (!DigetherSoil::Initialize(Grid, Initial))
	{
		return false;
	}
	if (Cells == 0)
	{
		if (!Apply(Grid, Initial))
		{
			return false;
		}
		ClearChunks();
		AppliedChunkCells = 0;
	}
	else if (!ApplyChunks(Grid, Initial, Cells, true))
	{
		return false;
	}
	// 모드 전환은 기존 굴착 상태의 재분할이 아닌 초기 흙 재생성
	Revision = 0;
	LastChangedWorldBounds = FBox(ForceInit);
	return true;
}

// 검증된 격자·청크 크기 입력 → 비활성 후보 범위와 PMC 쌍 출력
// 후보도 Actor에 등록해 수명 유지, 기존 활성 청크는 호출부의 성공 확정까지 보존
void ADigetherDiggableSoil::CreateChunkBuffers(const DigetherSoil::FDigetherSoilGrid& Grid, int32 ChunkCells,
	TArray<FDigetherSoilChunk>& OutChunks, TArray<TObjectPtr<UProceduralMeshComponent>>& OutMeshes)
{
	for (int32 Z = 0; Z < Grid.Cells.Z; Z += ChunkCells)
	{
		for (int32 Y = 0; Y < Grid.Cells.Y; Y += ChunkCells)
		{
			for (int32 X = 0; X < Grid.Cells.X; X += ChunkCells)
			{
				FDigetherSoilChunk Chunk;
				Chunk.Min = FIntVector(X, Y, Z);
				Chunk.Max =
					FIntVector(FMath::Min(X + ChunkCells, Grid.Cells.X), FMath::Min(Y + ChunkCells, Grid.Cells.Y),
							   FMath::Min(Z + ChunkCells, Grid.Cells.Z));
				OutChunks.Add(Chunk);
				for (int32 Buffer = 0; Buffer < 2; ++Buffer)
				{
					UProceduralMeshComponent* Mesh =
						NewObject<UProceduralMeshComponent>(this, NAME_None, RF_Transient);
					AddInstanceComponent(Mesh);
					Mesh->SetupAttachment(SoilMesh);
					ConfigureMesh(*Mesh);
					Mesh->SetMaterial(0, SoilMesh->GetMaterial(0));
					Mesh->RegisterComponent();
					OutMeshes.Add(Mesh);
				}
			}
		}
	}
}

// 같은 격자의 전후 밀도 입력 → 값이 달라진 전역 표본 좌표 출력
// 반경 밖 밀도 변화와 노멀 이웃을 놓치지 않도록 전체 표본 비교 유지
static void CollectChangedSamples(const DigetherSoil::FDigetherSoilGrid& Grid, const TArray<uint8>& Previous,
	const TArray<uint8>& Candidate, TArray<FIntVector>& OutSamples)
{
	for (int32 Z = 0; Z <= Grid.Cells.Z; ++Z)
	{
		for (int32 Y = 0; Y <= Grid.Cells.Y; ++Y)
		{
			for (int32 X = 0; X <= Grid.Cells.X; ++X)
			{
				const int32 Index = Grid.GetSampleIndex(X, Y, Z);
				if (Previous[Index] != Candidate[Index])
				{
					OutSamples.Add(FIntVector(X, Y, Z));
				}
			}
		}
	}
}

bool ADigetherDiggableSoil::ApplyChunks(const DigetherSoil::FDigetherSoilGrid& Grid, const TArray<uint8>& Candidate,
										int32 ChunkCells, bool bReset)
{
	if (!Grid.IsValid() || Candidate.Num() != Grid.GetSampleCount())
	{
		return false;
	}
	if (!bReset && (AppliedGrid.Cells != Grid.Cells || AppliedGrid.SpacingCm != Grid.SpacingCm ||
					Density.Num() != Candidate.Num()))
	{
		return false;
	}
	// 재초기화 후보, 전체 준비 성공 전 기존 Chunks/ChunkMeshes 유지
	TArray<FDigetherSoilChunk> NewChunks;
	TArray<TObjectPtr<UProceduralMeshComponent>> NewMeshes;
	if (bReset)
	{
		CreateChunkBuffers(Grid, ChunkCells, NewChunks, NewMeshes);
	}
	TArray<FDigetherSoilChunk>& WorkingChunks = bReset ? NewChunks : Chunks;
	TArray<TObjectPtr<UProceduralMeshComponent>>& WorkingMeshes = bReset ? NewMeshes : ChunkMeshes;
	const auto Reject = [&]()
	{
		// 기존 활성 버퍼·밀도 보존, 초기화 후보 컴포넌트만 폐기
		if (bReset)
		{
			for (UProceduralMeshComponent* Mesh : NewMeshes)
			{
				RemoveInstanceComponent(Mesh);
				Mesh->DestroyComponent();
			}
		}
		// 일반 굴착 실패의 후보 데이터는 비활성 버퍼에 잔류, 다음 준비에서 제거
		return false;
	};
	// 반경 추정 대신 전후 밀도가 실제 달라진 전역 표본 좌표
	TArray<FIntVector> ChangedSamples;
	if (!bReset)
	{
		CollectChangedSamples(Grid, Density, Candidate, ChangedSamples);
	}
	// Prepared와 States는 같은 인덱스의 후보 목록, 전체 성공 후 일괄 활성화
	TArray<int32> Prepared;
	TArray<DigetherSoil::EDigetherSoilRangeState> States;
	int32 GeneratedTriangles = 0;
	for (int32 Index = 0; Index < WorkingChunks.Num(); ++Index)
	{
		FDigetherSoilChunk& Chunk = WorkingChunks[Index];
		bool bDirty = bReset;
		for (const FIntVector& Sample : ChangedSamples)
		{
			// 경계 정점의 기울기가 참조하는 이웃 표본까지 포함
			if (Sample.X >= Chunk.Min.X - 1 && Sample.Y >= Chunk.Min.Y - 1 && Sample.Z >= Chunk.Min.Z - 1 &&
				Sample.X <= Chunk.Max.X + 1 && Sample.Y <= Chunk.Max.Y + 1 && Sample.Z <= Chunk.Max.Z + 1)
			{
				bDirty = true;
				break;
			}
		}
		if (!bDirty)
		{
			continue;
		}
		DigetherSoil::FDigetherSoilSurface Surface;
		const DigetherSoil::EDigetherSoilRangeState State =
			DigetherSoil::BuildSurfaceRange(Grid, Candidate, Chunk.Min, Chunk.Max, Surface);
		if (State == DigetherSoil::EDigetherSoilRangeState::Failed)
		{
			return Reject();
		}
		// 청크마다 연속한 PMC 두 개, 현재 활성 반대편에서만 후보 준비
		UProceduralMeshComponent* Next = WorkingMeshes[Chunk.GetCandidateMeshIndex(Index)];
		if (!PrepareMesh(*Next, Surface))
		{
			return Reject();
		}
#if WITH_DEV_AUTOMATION_TESTS
		if (Prepared.Num() == FailPreparedChunkForTest)
		{
			return Reject();
		}
#endif
		Prepared.Add(Index);
		States.Add(State);
		GeneratedTriangles += Surface.Indices.Num() / 3;
	}
	// 모든 후보 성공 후 동일 게임 스레드 처리에서 표시·충돌·밀도 확정
	DIGETHER_SOIL_TIMER(CommitTimer, Commit);
	for (int32 PreparedIndex = 0; PreparedIndex < Prepared.Num(); ++PreparedIndex)
	{
		const int32 Index = Prepared[PreparedIndex];
		FDigetherSoilChunk& Chunk = WorkingChunks[Index];
		UProceduralMeshComponent* Previous = WorkingMeshes[Chunk.GetActiveMeshIndex(Index)];
		UProceduralMeshComponent* Next = WorkingMeshes[Chunk.GetCandidateMeshIndex(Index)];
		// Solid도 무표면이므로 충돌 비활성, 내부 스폰·순간이동 차단용 부피 충돌은 없음
		const bool bSurface = States[PreparedIndex] == DigetherSoil::EDigetherSoilRangeState::Surface;
		SetMeshEnabled(*Previous, false);
		SetMeshEnabled(*Next, bSurface);
		Chunk.bFrontActive = !Chunk.bFrontActive;
		Chunk.State = States[PreparedIndex];
	}
	if (bReset)
	{
		ClearChunks();
		Chunks = MoveTemp(NewChunks);
		ChunkMeshes = MoveTemp(NewMeshes);
		SetMeshEnabled(*SoilMesh, false);
		SetMeshEnabled(*SoilBack, false);
		SoilMesh->ClearAllMeshSections();
		SoilBack->ClearAllMeshSections();
	}
	Density = Candidate;
	AppliedGrid = Grid;
	AppliedChunkCells = ChunkCells;
	bReady = true;
	LastUpdatedChunks = Prepared.Num();
	LastGeneratedTriangles = GeneratedTriangles;
	return true;
}
