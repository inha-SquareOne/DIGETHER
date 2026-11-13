#include "DigetherSoilField.h"
#include "DigetherSoilProfiling.h"

namespace DigetherSoil
{
	// 밀도 경계값: 초과는 흙, 이하는 공기
	static constexpr int32 SurfaceThreshold = 128;

	bool FDigetherSoilGrid::IsValid() const
	{
		return Cells.X >= 4 && Cells.Y >= 4 && Cells.Z >= 4 && Cells.X <= 64 && Cells.Y <= 64 && Cells.Z <= 64 &&
			   int64(Cells.X + 1) * (Cells.Y + 1) * (Cells.Z + 1) <= 32768 && FMath::IsFinite(SpacingCm) &&
			   SpacingCm >= 2.0 && SpacingCm <= 200.0;
	}
	int32 FDigetherSoilGrid::GetSampleCount() const
	{
		return IsValid() ? (Cells.X + 1) * (Cells.Y + 1) * (Cells.Z + 1) : 0;
	}
	int32 FDigetherSoilGrid::GetSampleIndex(int32 X, int32 Y, int32 Z) const
	{
		return (Z * (Cells.Y + 1) + Y) * (Cells.X + 1) + X;
	}
	FVector FDigetherSoilGrid::GetSamplePosition(int32 X, int32 Y, int32 Z) const
	{
		return FVector(X, Y, Z) * SpacingCm;
	}
	static bool IsBoundarySample(const FDigetherSoilGrid& Grid, int32 X, int32 Y, int32 Z)
	{
		return X == 0 || Y == 0 || Z == 0 || X == Grid.Cells.X || Y == Grid.Cells.Y || Z == Grid.Cells.Z;
	}
	// 경계까지 부호 있는 거리(cm) 입력 → 128 기준 포화 밀도 출력, HP가 아닌 표면 보간값
	static uint8 EncodeDistance(double Distance)
	{
		const int32 Magnitude = FMath::RoundToInt(FMath::Min(FMath::Abs(Distance) / 2.0, 127.0));
		return uint8(SurfaceThreshold + (Distance < 0 ? -Magnitude : Magnitude));
	}
	// 배열 길이와 외곽 공기층 검사, 청크 입력도 Actor 전체 밀도 기준
	static bool IsDensityValid(const FDigetherSoilGrid& Grid, const TArray<uint8>& Density)
	{
		if (!Grid.IsValid() || Density.Num() != Grid.GetSampleCount())
		{
			return false;
		}
		for (int32 Z = 0; Z <= Grid.Cells.Z; ++Z)
		{
			for (int32 Y = 0; Y <= Grid.Cells.Y; ++Y)
			{
				for (int32 X = 0; X <= Grid.Cells.X; ++X)
				{
					if (IsBoundarySample(Grid, X, Y, Z) && Density[Grid.GetSampleIndex(X, Y, Z)] >= SurfaceThreshold)
					{
						return false;
					}
				}
			}
		}
		return true;
	}
	bool Initialize(const FDigetherSoilGrid& Grid, TArray<uint8>& OutDensity)
	{
		if (!Grid.IsValid())
		{
			return false;
		}
		TArray<uint8> InitialDensity;
		InitialDensity.SetNumUninitialized(Grid.GetSampleCount());
		for (int32 Z = 0; Z <= Grid.Cells.Z; ++Z)
		{
			for (int32 Y = 0; Y <= Grid.Cells.Y; ++Y)
			{
				for (int32 X = 0; X <= Grid.Cells.X; ++X)
				{
					// 외곽 한 층을 공기로 유지해 여섯 면이 닫힌 표면 생성
					const int32 DistanceInCells =
						FMath::Min3(FMath::Min(X, Grid.Cells.X - X), FMath::Min(Y, Grid.Cells.Y - Y),
									FMath::Min(Z, Grid.Cells.Z - Z));
					InitialDensity[Grid.GetSampleIndex(X, Y, Z)] =
						EncodeDistance((DistanceInCells - 0.5) * Grid.SpacingCm);
				}
			}
		}
		OutDensity = MoveTemp(InitialDensity);
		return true;
	}
	bool WorldToLocal(const FTransform& Transform, const FVector& WorldPoint, FVector& OutLocal)
	{
		if (Transform.ContainsNaN() || WorldPoint.ContainsNaN() || !Transform.GetRotation().IsNormalized() ||
			Transform.GetScale3D().GetMin() <= UE_SMALL_NUMBER)
		{
			return false;
		}
		const FVector Local = Transform.InverseTransformPosition(WorldPoint);
		if (Local.ContainsNaN())
		{
			return false;
		}
		OutLocal = Local;
		return true;
	}
	EDigetherSoilCarveResult Carve(const FDigetherSoilGrid& Grid, TArray<uint8>& Density, const FVector& Point,
								   double Radius, const TArray<FBox>& ProtectedBoxes, FBox& OutChangedBounds,
								   const FVector& WorldScale)
	{
		OutChangedBounds = FBox(ForceInit);
		if (!IsDensityValid(Grid, Density) || Point.ContainsNaN() || !FMath::IsFinite(Radius) ||
			WorldScale.ContainsNaN() || WorldScale.GetMin() <= UE_SMALL_NUMBER)
		{
			return EDigetherSoilCarveResult::InvalidInput;
		}
		const FBox Domain(FVector::ZeroVector, Grid.GetSamplePosition(Grid.Cells.X, Grid.Cells.Y, Grid.Cells.Z));
		if (Radius < Grid.SpacingCm * WorldScale.GetMin() * 0.5 ||
			Radius > (Domain.GetSize() * WorldScale).Size() || !Domain.IsInsideOrOn(Point))
		{
			return EDigetherSoilCarveResult::InvalidInput;
		}
		for (const FBox& ProtectedBox : ProtectedBoxes)
		{
			if (!ProtectedBox.IsValid || ProtectedBox.Min.ContainsNaN() || ProtectedBox.Max.ContainsNaN() ||
				ProtectedBox.Min.X > ProtectedBox.Max.X || ProtectedBox.Min.Y > ProtectedBox.Max.Y ||
				ProtectedBox.Min.Z > ProtectedBox.Max.Z)
			{
				return EDigetherSoilCarveResult::InvalidInput;
			}
		}
		// 타격 중심 1cm 양자화, 반복 요청에서 동일 중심 유지
		const FVector Center(FMath::RoundToDouble(Point.X), FMath::RoundToDouble(Point.Y),
							 FMath::RoundToDouble(Point.Z));
		for (int32 Z = 1; Z < Grid.Cells.Z; ++Z)
		{
			for (int32 Y = 1; Y < Grid.Cells.Y; ++Y)
			{
				for (int32 X = 1; X < Grid.Cells.X; ++X)
				{
					const FVector SamplePosition = Grid.GetSamplePosition(X, Y, Z);
					// 하나라도 포함되면 중단, 한 셀 확장과 경계 포함 규칙 유지
					if (ProtectedBoxes.ContainsByPredicate([&](const FBox& ProtectedBox)
					{
						return ProtectedBox.ExpandBy(Grid.SpacingCm).IsInsideOrOn(SamplePosition);
					}))
					{
						continue;
					}
					// 반경 밖 양수 밀도도 감소 가능, 반경으로 순회 범위 제한 금지
					// 비균일 확대에서도 도구 반경은 월드 cm, 회전은 거리 크기에 영향 없음
					const uint8 CarvedDensity = EncodeDistance(((SamplePosition - Center) * WorldScale).Size() - Radius);
					uint8& CurrentDensity = Density[Grid.GetSampleIndex(X, Y, Z)];
					if (CarvedDensity < CurrentDensity)
					{
						CurrentDensity = CarvedDensity;
						OutChangedBounds += SamplePosition;
					}
				}
			}
		}
		if (!OutChangedBounds.IsValid)
		{
			return EDigetherSoilCarveResult::NoChange;
		}
		OutChangedBounds = OutChangedBounds.ExpandBy(Grid.SpacingCm).Overlap(Domain);
		return EDigetherSoilCarveResult::Changed;
	}
	// 표면 추출·와인딩 출처: 기존 DigetherDainField / prototype
	static void AppendTriangle(FDigetherSoilSurface& Surface, FVector A, FVector B, FVector C,
							   const FVector& AirDirection)
	{
		FVector Normal = FVector::CrossProduct(B - A, C - A);
		if (Normal.SizeSquared() < 0.0001)
		{
			return;
		}
		if (FVector::DotProduct(Normal, AirDirection) < 0)
		{
			Swap(B, C);
			Normal = -Normal;
		}
		Normal.Normalize();
		// 공기 방향 노멀 유지, UE 표면 순서에 맞춰 A,C,B 출력
		for (const FVector& P : {A, C, B})
		{
			Surface.Indices.Add(Surface.Vertices.Num());
			Surface.Vertices.Add(P);
			Surface.Normals.Add(Normal);
			Surface.UVs.Add(FVector2D(P.X / 200.f, P.Y / 200.f));
		}
	}

	static void AppendTetrahedron(const FVector* CornerPositions, const int32* CornerDensities,
								  const int32 (&Tetrahedron)[4], FDigetherSoilSurface& Surface)
	{
		// Solid/Air는 네 꼭짓점의 인덱스, 위치 합은 공기 방향 판정용
		int32 Solid[4], Air[4], SolidCount = 0, AirCount = 0;
		FVector SolidPositionSum = FVector::ZeroVector, AirPositionSum = FVector::ZeroVector;
		for (const int32 CornerIndex : Tetrahedron)
		{
			if (CornerDensities[CornerIndex] > 0)
			{
				Solid[SolidCount++] = CornerIndex;
				SolidPositionSum += CornerPositions[CornerIndex];
			}
			else
			{
				Air[AirCount++] = CornerIndex;
				AirPositionSum += CornerPositions[CornerIndex];
			}
		}
		if (SolidCount == 0 || AirCount == 0)
		{
			return;
		}
		const FVector TowardsAir = AirPositionSum / AirCount - SolidPositionSum / SolidCount;
		// 흙/공기 변의 128 교차점, 분모는 서로 다른 분류로 0 제외
		const auto InterpolateBoundary = [&CornerPositions, &CornerDensities](int32 SolidCorner, int32 AirCorner)
		{
			return FMath::Lerp(CornerPositions[SolidCorner], CornerPositions[AirCorner],
							   static_cast<double>(CornerDensities[SolidCorner]) /
								   (CornerDensities[SolidCorner] - CornerDensities[AirCorner]));
		};
		if (SolidCount == 1)
		{
			AppendTriangle(Surface, InterpolateBoundary(Solid[0], Air[0]), InterpolateBoundary(Solid[0], Air[1]),
						   InterpolateBoundary(Solid[0], Air[2]), TowardsAir);
		}
		else if (AirCount == 1)
		{
			AppendTriangle(Surface, InterpolateBoundary(Solid[0], Air[0]), InterpolateBoundary(Solid[1], Air[0]),
						   InterpolateBoundary(Solid[2], Air[0]), TowardsAir);
		}
		else
		{
			const FVector A = InterpolateBoundary(Solid[0], Air[0]), B = InterpolateBoundary(Solid[0], Air[1]);
			const FVector C = InterpolateBoundary(Solid[1], Air[0]), D = InterpolateBoundary(Solid[1], Air[1]);
			AppendTriangle(Surface, A, B, C, TowardsAir);
			AppendTriangle(Surface, B, D, C, TowardsAir);
		}
	}

	static void UpdateSmoothNormals(const FDigetherSoilGrid& Grid, const TArray<uint8>& Density,
									FDigetherSoilSurface& Surface)
	{
		// 음영용 밀도 기울기 보간, 정점 위치와 충돌은 유지
		if (Surface.Vertices.IsEmpty())
		{
			return;
		}
		// 전역 표본별 공기 방향 기울기, 청크 경계에서도 같은 이웃 참조
		// 현재는 갱신 청크마다 전체 격자 기울기 재계산, 청크 범위만 계산하는 최적화는 미적용
		TArray<FVector> Gradients;
		Gradients.SetNumUninitialized(Grid.GetSampleCount());
		for (int32 Z = 0; Z <= Grid.Cells.Z; ++Z)
		{
			for (int32 Y = 0; Y <= Grid.Cells.Y; ++Y)
			{
				for (int32 X = 0; X <= Grid.Cells.X; ++X)
				{
					const int32 X0 = FMath::Max(0, X - 1), X1 = FMath::Min(Grid.Cells.X, X + 1);
					const int32 Y0 = FMath::Max(0, Y - 1), Y1 = FMath::Min(Grid.Cells.Y, Y + 1);
					const int32 Z0 = FMath::Max(0, Z - 1), Z1 = FMath::Min(Grid.Cells.Z, Z + 1);
					Gradients[Grid.GetSampleIndex(X, Y, Z)] =
						FVector(double(int32(Density[Grid.GetSampleIndex(X0, Y, Z)]) -
									   Density[Grid.GetSampleIndex(X1, Y, Z)]) /
									(X1 - X0),
								double(int32(Density[Grid.GetSampleIndex(X, Y0, Z)]) -
									   Density[Grid.GetSampleIndex(X, Y1, Z)]) /
									(Y1 - Y0),
								double(int32(Density[Grid.GetSampleIndex(X, Y, Z0)]) -
									   Density[Grid.GetSampleIndex(X, Y, Z1)]) /
									(Z1 - Z0));
				}
			}
		}
		for (int32 VertexIndex = 0; VertexIndex < Surface.Vertices.Num(); ++VertexIndex)
		{
			const FVector GridPosition = Surface.Vertices[VertexIndex] / Grid.SpacingCm;
			const FIntVector Cell(FMath::Clamp(FMath::FloorToInt(GridPosition.X), 0, Grid.Cells.X - 1),
								  FMath::Clamp(FMath::FloorToInt(GridPosition.Y), 0, Grid.Cells.Y - 1),
								  FMath::Clamp(FMath::FloorToInt(GridPosition.Z), 0, Grid.Cells.Z - 1));
			// 셀 내부 상대 위치, 이웃 여덟 기울기의 보간 가중치
			const FVector Weight = GridPosition - FVector(Cell);
			FVector Normal = FVector::ZeroVector;
			for (int32 Z = 0; Z < 2; ++Z)
			{
				for (int32 Y = 0; Y < 2; ++Y)
				{
					for (int32 X = 0; X < 2; ++X)
					{
						Normal += Gradients[Grid.GetSampleIndex(Cell.X + X, Cell.Y + Y, Cell.Z + Z)] *
								  (X ? Weight.X : 1 - Weight.X) * (Y ? Weight.Y : 1 - Weight.Y) *
								  (Z ? Weight.Z : 1 - Weight.Z);
					}
				}
			}
			if (Normal.Normalize())
			{
				Surface.Normals[VertexIndex] = Normal;
			}
		}

	}

	// 검증된 전역 밀도·셀 범위 입력 → 표면과 흙/공기 존재 여부 출력
	// 전체·청크의 분할·보간·노멀 순서 일치, 빈 결과 정책은 호출부 책임
	static void ExtractSurface(const FDigetherSoilGrid& Grid, const TArray<uint8>& Density,
							   const FIntVector& CellMin, const FIntVector& CellMax,
							   FDigetherSoilSurface& OutSurface, bool& bOutHasAir, bool& bOutHasSolid)
	{
		DIGETHER_SOIL_TIMER(SurfaceTimer, Surface);
		bOutHasAir = false;
		bOutHasSolid = false;
		// 인접 셀의 경계가 맞도록 모든 셀에서 동일한 분할 사용
		static constexpr int32 Corners[8][3] = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
												{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}};
		static constexpr int32 Tetrahedra[6][4] = {{0, 1, 2, 6}, {0, 2, 3, 6}, {0, 3, 7, 6},
												   {0, 7, 4, 6}, {0, 4, 5, 6}, {0, 5, 1, 6}};
		for (int32 Z = CellMin.Z; Z < CellMax.Z; ++Z)
		{
			for (int32 Y = CellMin.Y; Y < CellMax.Y; ++Y)
			{
				for (int32 X = CellMin.X; X < CellMax.X; ++X)
				{
					FVector CornerPositions[8];
					int32 CornerDensities[8];
					for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
					{
						const int32 SampleX = X + Corners[CornerIndex][0], SampleY = Y + Corners[CornerIndex][1],
									SampleZ = Z + Corners[CornerIndex][2];
						CornerPositions[CornerIndex] = Grid.GetSamplePosition(SampleX, SampleY, SampleZ);
						CornerDensities[CornerIndex] =
							static_cast<int32>(Density[Grid.GetSampleIndex(SampleX, SampleY, SampleZ)]) -
							SurfaceThreshold;
						bOutHasAir |= CornerDensities[CornerIndex] <= 0;
						bOutHasSolid |= CornerDensities[CornerIndex] > 0;
					}
					for (const auto& Tetrahedron : Tetrahedra)
					{
						AppendTetrahedron(CornerPositions, CornerDensities, Tetrahedron, OutSurface);
					}
				}
			}
		}
		DIGETHER_SOIL_STOP(SurfaceTimer);
		DIGETHER_SOIL_TIMER(NormalTimer, Normals);
		UpdateSmoothNormals(Grid, Density, OutSurface);
		DIGETHER_SOIL_STOP(NormalTimer);
	}

	bool BuildSurface(const FDigetherSoilGrid& Grid, const TArray<uint8>& Density, FDigetherSoilSurface& OutSurface)
	{
		if (!IsDensityValid(Grid, Density))
		{
			return false;
		}
		FDigetherSoilSurface Result;
		bool bHasAir, bHasSolid;
		ExtractSurface(Grid, Density, FIntVector::ZeroValue, Grid.Cells, Result, bHasAir, bHasSolid);
		// 전체 경로는 혼합 밀도의 무표면 결과도 허용, 청크 경로의 Failed 정책과 차이
		OutSurface = MoveTemp(Result);
		return true;
	}
	EDigetherSoilRangeState BuildSurfaceRange(const FDigetherSoilGrid& Grid, const TArray<uint8>& Density,
											  const FIntVector& CellMin, const FIntVector& CellMax,
											  FDigetherSoilSurface& OutSurface)
	{
		if (!IsDensityValid(Grid, Density) || CellMin.X < 0 || CellMin.Y < 0 || CellMin.Z < 0 ||
			CellMax.X > Grid.Cells.X || CellMax.Y > Grid.Cells.Y || CellMax.Z > Grid.Cells.Z ||
			CellMin.X >= CellMax.X || CellMin.Y >= CellMax.Y || CellMin.Z >= CellMax.Z)
		{
			return EDigetherSoilRangeState::Failed;
		}
		FDigetherSoilSurface Result;
		bool bHasAir, bHasSolid;
		ExtractSurface(Grid, Density, CellMin, CellMax, Result, bHasAir, bHasSolid);
		// 표면이 없는 내부 흙과 완전 공기를 구분
		if (Result.Indices.IsEmpty() && bHasAir && bHasSolid)
		{
			// 혼합 밀도인데 표면이 없으면 기존 상태 보존, 퇴화 원인 별도 조사
			return EDigetherSoilRangeState::Failed;
		}
		const EDigetherSoilRangeState State =
			!Result.Indices.IsEmpty() ? EDigetherSoilRangeState::Surface
									  : (bHasSolid ? EDigetherSoilRangeState::Solid : EDigetherSoilRangeState::Air);
		OutSurface = MoveTemp(Result);
		return State;
	}

} // namespace DigetherSoil
