#include "DigetherCollapseLab.h"
#include "DigetherCeilingProbe.h"
#include "ProceduralMeshComponent.h"
#include "DigetherExcavationLabCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/MaterialBillboardComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogDigetherCollapseLab, Log, All);

namespace
{
	bool Contains(const UBoxComponent& Box, const FVector& WorldPoint)
	{
		const FVector Extent = Box.GetUnscaledBoxExtent();
		return FBox(-Extent, Extent).IsInsideOrOn(Box.GetComponentTransform().InverseTransformPosition(WorldPoint));
	}
}

ADigetherCollapseLab::ADigetherCollapseLab()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	CeilingRemoval = CreateDefaultSubobject<UBoxComponent>(TEXT("CeilingRemoval"));
	Deposit = CreateDefaultSubobject<UBoxComponent>(TEXT("Deposit"));
	for (UBoxComponent* Box : {CeilingRemoval.Get(), Deposit.Get()})
	{
		Box->SetupAttachment(RootComponent);
		Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Box->SetGenerateOverlapEvents(false);
	}
	CeilingRemoval->SetRelativeLocation(FVector(0, 0, 350));
	CeilingRemoval->SetBoxExtent(FVector(75, 175, 75));
	Deposit->SetRelativeLocation(FVector(0, 0, 130));
	Deposit->SetBoxExtent(FVector(75, 300, 120));
	FallingSoil = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("FallingSoil"));
	FallingSoil->SetupAttachment(RootComponent);
	FallingSoil->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FallingSoil->SetGenerateOverlapEvents(false);
	FallingSoil->SetVisibility(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Game/Developers/Dain/Excavation/Meshes/SM_CollapseClod.SM_CollapseClod"));
	FallingSoil->SetStaticMesh(Mesh.Object);
	Dust = CreateDefaultSubobject<UMaterialBillboardComponent>(TEXT("Dust"));
	Dust->SetupAttachment(RootComponent);
	Dust->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Dust->SetVisibility(false);
}

void ADigetherCollapseLab::BeginPlay()
{
	Super::BeginPlay();
	if (DustMaterial)
	{
		DustInstance = UMaterialInstanceDynamic::Create(DustMaterial, this);
		Dust->AddElement(DustInstance, nullptr, false, 160.f, 120.f, nullptr);
	}
}

void ADigetherCollapseLab::TriggerCollapse()
{
	StartCollapse();
	ShowStatus();
}

void ADigetherCollapseLab::ShowStatus() const
{
	UE_LOG(LogDigetherCollapseLab, Display, TEXT("%s count=%d removed=%d deposited=%d"),
		*LastStatus.ToString(), CollapseCount, RemovedSamples, DepositedSamples);
#if !UE_BUILD_SHIPPING
	if (GEngine && GetWorld() && GetWorld()->IsGameWorld())
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Orange, LastStatus.ToString());
	}
#endif
}

EDigetherSoilEditResult ADigetherCollapseLab::PrepareCollapse(TArray<uint8>& OutCandidate, FBox& OutChanged, int32& OutRemoved, int32& OutAdded, TArray<FVector>* WarningSurface, TArray<FDepositSample>* NewSolids, int32 Stage)
{
	const auto Finish = [this](EDigetherSoilEditResult Result, const FText& Message)
	{
		LastStatus = Message;
		return Result;
	};
#if UE_BUILD_SHIPPING
	return Finish(EDigetherSoilEditResult::Rejected, NSLOCTEXT("DigetherCollapseLab", "Disabled", "개인 붕괴 시험 비활성"));
#else
	if (!GetWorld() || !GetWorld()->IsGameWorld() || !HasAuthority() || !IsValid(CeilingProbe) ||
		!IsValid(CeilingProbe->Soil) || !CeilingProbe->Soil->HasAuthority() || !CeilingProbe->Soil->bReady)
	{
		return Finish(EDigetherSoilEditResult::Rejected, NSLOCTEXT("DigetherCollapseLab", "Invalid", "실행 중인 유효한 흙과 천장 검사가 필요합니다"));
	}
	ADigetherDiggableSoil& Soil = *CeilingProbe->Soil;
	const DigetherSoil::FDigetherSoilGrid& Grid = Soil.AppliedGrid;
	FVector Local;
	if (!DigetherSoil::WorldToLocal(Soil.GetActorTransform(), GetActorLocation(), Local) ||
		CeilingRemoval->Bounds.GetBox().Intersect(Deposit->Bounds.GetBox()) ||
		CeilingRemoval->Bounds.Origin.Z <= Deposit->Bounds.Origin.Z)
	{
		return Finish(EDigetherSoilEditResult::Rejected, NSLOCTEXT("DigetherCollapseLab", "Boxes", "천장 제거와 아래 퇴적 영역을 분리해 배치하세요"));
	}
	// 한 Actor 내부의 유한한 양수 크기만 허용. 영역이 잘려 적용되는 시험을 성공으로 오인하지 않음
	const FBox Domain(FVector::ZeroVector, FVector(Grid.Cells) * Grid.SpacingCm);
	for (const UBoxComponent* Box : {CeilingRemoval.Get(), Deposit.Get()})
	{
		const FTransform Transform = Box->GetComponentTransform();
		const FVector Extent = Box->GetUnscaledBoxExtent();
		if (Transform.ContainsNaN() || Transform.GetScale3D().GetMin() <= UE_SMALL_NUMBER ||
			Extent.ContainsNaN() || Extent.GetMin() <= 0)
		{
			return Finish(EDigetherSoilEditResult::Rejected, NSLOCTEXT("DigetherCollapseLab", "Extent", "붕괴 영역 크기는 유한한 양수여야 합니다"));
		}
		for (int32 Corner = 0; Corner < 8; ++Corner)
		{
			const FVector Point = Transform.TransformPosition(Extent * FVector(
				(Corner & 1) ? 1 : -1, (Corner & 2) ? 1 : -1, (Corner & 4) ? 1 : -1));
			if (!Domain.IsInsideOrOn(Soil.GetActorTransform().InverseTransformPosition(Point)))
			{
				return Finish(EDigetherSoilEditResult::Rejected, NSLOCTEXT("DigetherCollapseLab", "Domain", "제거와 퇴적 영역을 같은 흙 Actor 안에 배치하세요"));
			}
		}
	}
	// 유효한 원본 보호 규칙을 그대로 사용. 잘못된 상자를 무시하고 편집하지 않음
	for (const FBox& Box : Soil.ProtectedRegions)
	{
		if (!Box.IsValid || Box.Min.ContainsNaN() || Box.Max.ContainsNaN() ||
			Box.Min.X > Box.Max.X || Box.Min.Y > Box.Max.Y || Box.Min.Z > Box.Max.Z)
		{
			return Finish(EDigetherSoilEditResult::Rejected, NSLOCTEXT("DigetherCollapseLab", "Protected", "보호 영역 설정이 올바르지 않습니다"));
		}
	}
	const FTransform RemovalTransform = CeilingRemoval->GetComponentTransform();
	const FVector RemovalExtent = CeilingRemoval->GetUnscaledBoxExtent();
	const double RemovalScale = CeilingRemoval->GetScaledBoxExtent().GetMin();
	// 상자는 최대 범위다. 아래로 치우친 타원체로 천장을 깎아 상자 모서리와 윗면을 남긴다.
	const auto RoofDepth = [&](const FVector& WorldPoint)
	{
		const FVector P = RemovalTransform.InverseTransformPosition(WorldPoint) / RemovalExtent;
		const FVector Profile((P.X + .07 * P.Y) / .90, P.Y / .91, (P.Z + .12) / .83);
		return (1.0 - Profile.Size()) * RemovalScale;
	};
	CeilingProbe->RefreshCandidates();
	TArray<FVector> Vertices;
	CeilingProbe->CopyCandidateVertices(Vertices);
	bool bHasCeiling = false;
	for (int32 Index = 0; Index + 2 < Vertices.Num(); Index += 3)
	{
		if (RoofDepth((Vertices[Index] + Vertices[Index + 1] + Vertices[Index + 2]) / 3.0) >= 0.0)
		{
			bHasCeiling = true;
			if (WarningSurface)
			{
				WarningSurface->Append(Vertices.GetData() + Index, 3);
			}
		}
	}
	const bool bRepeat = bCollapsed && MatchesContext(LastContext) && Soil.Revision >= LastCollapseRevision;
	if (bRepeat && Stage <= LastLandedStage && DepositSamples.ContainsByPredicate([&Soil](int32 Index)
		{ return Soil.Density[Index] > 128; }))
	{
		return Finish(EDigetherSoilEditResult::NoChange, NSLOCTEXT("DigetherCollapseLab", "KeepPile", "현재 단계의 잔해를 유지합니다"));
	}
	if (!bHasCeiling && !bRepeat)
	{
		return Finish(EDigetherSoilEditResult::NoChange, NSLOCTEXT("DigetherCollapseLab", "NoCeiling", "제거 영역 안에 굴착으로 생긴 천장이 없습니다"));
	}
	TArray<uint8> Candidate = Soil.Density;
	int32 Removed = 0, Added = 0;
	FBox Changed(ForceInit);
	// 외곽 공기층은 건드리지 않음. 밀도는 질량이 아니므로 두 표본 수를 물리 보존량으로 취급하지 않음
	for (int32 Z = 1; Z < Grid.Cells.Z; ++Z)
	{
		for (int32 Y = 1; Y < Grid.Cells.Y; ++Y)
		{
			for (int32 X = 1; X < Grid.Cells.X; ++X)
			{
				const FVector Position = Grid.GetSamplePosition(X, Y, Z);
				if (Soil.ProtectedRegions.ContainsByPredicate([&](const FBox& Box)
					{ return Box.ExpandBy(Grid.SpacingCm).IsInsideOrOn(Position); }))
				{
					continue;
				}
				const FVector WorldPosition = Soil.GetActorTransform().TransformPosition(Position);
				const int32 SampleIndex = Grid.GetSampleIndex(X, Y, Z);
				uint8& Value = Candidate[SampleIndex];
				if (Contains(*CeilingRemoval, WorldPosition))
				{
					const double Depth = RoofDepth(WorldPosition);
					const uint8 Cut = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(128.0 - 127.0 * Depth / Grid.SpacingCm), 0, 255));
					if (Cut < Value)
					{
						if (Value > 128 && Cut <= 128) ++Removed;
						Value = Cut;
						Changed += Position;
					}
				}
				else if (Value <= 128 && Contains(*Deposit, WorldPosition))
				{
					// 퇴적 상자는 배치 범위이며, 실제 흙은 바닥에서 솟은 완만한 둔덕으로 채운다.
					const FVector Extent = Deposit->GetUnscaledBoxExtent();
					const FVector P = Deposit->GetComponentTransform().InverseTransformPosition(WorldPosition) / Extent;
					// 바닥을 기준으로 1단계는 낮고 넓게, 2단계는 한쪽으로 치우치게, 3단계는 전체 높이로 채운다.
					const double Height = Stage == 1 ? .23 : Stage == 2 ? .57 : 1.;
					const double HeightFromBase = (P.Z + 1.0) / 1.94;
					const FVector Profile(P.X + 0.10 * (1.0 - P.X * P.X) * P.Y + (Stage == 2 ? .36 * HeightFromBase : 0.),
						P.Y, HeightFromBase / Height);
					const double Depth = (1.0 - Profile.Size()) * Deposit->GetScaledBoxExtent().GetMin();
					const uint8 Fill = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(128.0 + 127.0 * Depth / Grid.SpacingCm), 0, 255));
					if (Fill <= Value)
					{
						continue;
					}
					Value = Fill;
					if (Fill > 128)
					{
						++Added;
						if (NewSolids) NewSolids->Add({SampleIndex, WorldPosition});
					}
					Changed += Position;
				}
			}
		}
	}
	if ((!bRepeat && Removed == 0) || Added == 0)
	{
		return Finish(EDigetherSoilEditResult::NoChange, NSLOCTEXT("DigetherCollapseLab", "NoEdit", "제거할 흙과 퇴적할 빈 공간이 모두 필요합니다"));
	}
	OutCandidate = MoveTemp(Candidate);
	OutChanged = Changed;
	OutRemoved = Removed;
	OutAdded = Added;
	return EDigetherSoilEditResult::Applied;
#endif
}

bool ADigetherCollapseLab::GetWarningSurface(TArray<FVector>& OutVertices, int32 Stage)
{
	OutVertices.Reset();
	TArray<uint8> Candidate;
	FBox Changed(ForceInit);
	int32 Removed = 0, Added = 0;
	return !bFalling && PrepareCollapse(Candidate, Changed, Removed, Added, &OutVertices, nullptr, Stage) == EDigetherSoilEditResult::Applied;
}

EDigetherSoilEditResult ADigetherCollapseLab::TryCollapse()
{
	return ApplyLanding(FMath::Clamp(PreviewStage, 1, 3));
}

EDigetherSoilEditResult ADigetherCollapseLab::ApplyLanding(int32 Stage)
{
	if (bFalling)
	{
		return EDigetherSoilEditResult::Rejected;
	}
	TArray<uint8> Candidate;
	FBox Changed(ForceInit);
	int32 Removed = 0, Added = 0;
	TArray<FDepositSample> NewSolids;
	const EDigetherSoilEditResult Result = PrepareCollapse(Candidate, Changed, Removed, Added, nullptr, &NewSolids, Stage);
	if (Result != EDigetherSoilEditResult::Applied)
	{
		return Result;
	}
	ADigetherDiggableSoil& Soil = *CeilingProbe->Soil;
	// 새 충돌이 캡슐을 밀어내기 전에 수집하고, 지형 적용에 성공한 경우에만 결과를 공개한다.
	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(Overlaps, Deposit->GetComponentLocation(), Deposit->GetComponentQuat(),
		FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeBox(Deposit->GetScaledBoxExtent()));
	TArray<TWeakObjectPtr<APawn>> Targets;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		APawn* Pawn = Cast<APawn>(Overlap.GetActor());
		UPrimitiveComponent* Body = IsValid(Pawn) ? Cast<UPrimitiveComponent>(Pawn->GetRootComponent()) : nullptr;
		if (Body && Pawn->IsPlayerControlled() && NewSolids.ContainsByPredicate([Body](const FDepositSample& Sample)
		{
			FVector Closest;
			return Body->GetClosestPointOnCollision(Sample.WorldPosition, Closest) == 0.f;
		}))
		{
			Targets.AddUnique(TWeakObjectPtr<APawn>(Pawn));
		}
	}
	TArray<int32> RemainingSamples;
	if (bCollapsed && MatchesContext(LastContext) && Soil.Revision >= LastCollapseRevision)
	{
		for (const int32 Index : DepositSamples)
		{
			if (Soil.Density[Index] > 128) RemainingSamples.Add(Index);
		}
	}
	// 구간 잔해와 한 밀도장에 합성하되, 몸을 덮은 퇴적 표본의 소유권만 매몰자별로 기록한다.
	TMap<ADigetherExcavationLabCharacter*, TArray<int32>> BodyCovers;
	if (Stage == 3 && !Targets.IsEmpty())
	{
		TSet<int32> Deposited;
		Deposited.Append(RemainingSamples);
		for (const FDepositSample& Sample : NewSolids) Deposited.Add(Sample.Index);
		const auto& Grid = Soil.AppliedGrid;
		for (const auto& Hit : Targets)
		{
			ADigetherExcavationLabCharacter* Player = Cast<ADigetherExcavationLabCharacter>(Hit.Get());
			if (!Player || Player->bIsBuriedForRescue) continue;
			TArray<int32>& Cover = BodyCovers.Add(Player);
			const FVector Feet = Player->GetActorLocation() - FVector::UpVector * Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			const FQuat Rotation = FRotator(0., Player->GetActorRotation().Yaw, 0.).Quaternion();
			const FTransform SoilTransform = Soil.GetActorTransform();
			const FBox Local = FBox(Feet - FVector(145.,145.,10.), Feet + FVector(145.,145.,550.)).TransformBy(SoilTransform.Inverse());
			const FIntVector Lo = FIntVector(1).ComponentMax(FIntVector(Local.Min / Grid.SpacingCm));
			const FIntVector Hi = (Grid.Cells - FIntVector(1)).ComponentMin(FIntVector(Local.Max / Grid.SpacingCm) + FIntVector(1));
			for (int32 Z = Lo.Z; Z <= Hi.Z; ++Z)
			for (int32 Y = Lo.Y; Y <= Hi.Y; ++Y)
			for (int32 X = Lo.X; X <= Hi.X; ++X)
			{
				const int32 I = Grid.GetSampleIndex(X,Y,Z);
				const FVector Pos = Grid.GetSamplePosition(X,Y,Z);
				const FVector World = SoilTransform.TransformPosition(Pos);
				const FVector P = Rotation.UnrotateVector(World - Feet);
				if (P.Z < 0. || FMath::Abs(P.X) > 122. || FMath::Abs(P.Y) > 64.) continue;
				const double Depth = (1. - FVector(P.X / 122., P.Y / 64., P.Z / 84.).Size()) * 64.;
				if (Depth > -Grid.SpacingCm && Candidate[I] <= 128 && !Soil.ProtectedRegions.ContainsByPredicate(
					[&](const FBox& Box) { return Box.ExpandBy(Grid.SpacingCm).IsInsideOrOn(Pos); }))
				{
					const uint8 Fill = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(128. + 127. * Depth / Grid.SpacingCm), 0, 255));
					if (Fill > Candidate[I])
					{
						Candidate[I] = Fill;
						Changed += Pos;
						if (Fill > 128)
						{
							NewSolids.Add({I,World});
							Deposited.Add(I);
							++Added;
						}
					}
				}
				if (Deposited.Contains(I)) Cover.Add(I);
			}
		}
	}
	if (!(Soil.AppliedChunkCells == 0 ? Soil.Apply(Soil.AppliedGrid, Candidate)
		: Soil.ApplyChunks(Soil.AppliedGrid, Candidate, Soil.AppliedChunkCells, false)))
	{
		LastStatus = NSLOCTEXT("DigetherCollapseLab", "Failed", "붕괴 적용 실패: 기존 지형과 충돌을 유지합니다");
		return EDigetherSoilEditResult::ApplyFailed;
	}
	for (const FDepositSample& Sample : NewSolids) RemainingSamples.Add(Sample.Index);
	DepositSamples = MoveTemp(RemainingSamples);
	++Soil.Revision;
	Soil.LastChangedWorldBounds = Changed.ExpandBy(Soil.AppliedGrid.SpacingCm).TransformBy(Soil.GetActorTransform());
	RemovedSamples = Removed;
	DepositedSamples = Added;
	bCollapsed = true;
	++CollapseCount;
	LastLandedStage = Stage;
	LastBurialTargets = MoveTemp(Targets);
	for (const TWeakObjectPtr<APawn>& Hit : LastBurialTargets)
	{
		if (ADigetherExcavationLabCharacter* Player = Cast<ADigetherExcavationLabCharacter>(Hit.Get()))
		{
			if (!Player->bIsBuriedForRescue)
			{
				Player->LastReceivedBurialStage = LastLandedStage;
				if (TArray<int32>* Cover = BodyCovers.Find(Player))
				{
					Player->BeginBurial(*this, Soil, MoveTemp(*Cover));
					UpdateBurialExposure(*Player, Soil, false);
				}
			}
		}
	}
	UE_LOG(LogDigetherCollapseLab, Display, TEXT("Collapse %d on %s: burial candidates=%d"),
		CollapseCount, *GetName(), LastBurialTargets.Num());
	LastCollapseRevision = Soil.Revision;
	LastContext = CaptureContext();
	LastStatus = NSLOCTEXT("DigetherCollapseLab", "Applied", "붕괴 완료: 잔해를 다시 굴착할 수 있습니다");
	return EDigetherSoilEditResult::Applied;
}

void ADigetherCollapseLab::UpdateBurialExposure(ADigetherExcavationLabCharacter& Victim, ADigetherDiggableSoil& Soil, bool bAllowRescue)
{
	if (!Victim.bIsBuriedForRescue || Victim.BurialSoil.Get() != &Soil || !Soil.bReady) return;
	if (bAllowRescue && !Soil.LastChangedWorldBounds.Intersect(FBox(Victim.BurialFeet - FVector(145.,145.,10.),
		Victim.BurialFeet + FVector(145.,145.,550.)))) return;
	const auto& Grid = Soil.AppliedGrid;
	const FIntVector Size = Grid.Cells + FIntVector(1);
	FIntVector Lo = Grid.Cells;
	FIntVector Hi(0);
	// 공기 쪽 보간값을 보존해야 같은 타격에서 화면의 경계와 노출 판정 표면이 일치한다.
	TArray<uint8> CoverDensity = Soil.Density;
	for (uint8& Value : CoverDensity)
	{
		if (Value > 128) Value = 0;
	}
	Victim.BurialSamples.RemoveAll([&](int32 Index)
	{
		if (!Soil.Density.IsValidIndex(Index) || Soil.Density[Index] <= 128) return true;
		const FIntVector Sample(Index % Size.X, (Index / Size.X) % Size.Y, Index / (Size.X * Size.Y));
		Lo = Lo.ComponentMin(Sample);
		Hi = Hi.ComponentMax(Sample);
		CoverDensity[Index] = Soil.Density[Index];
		return false;
	});
	DigetherSoil::FDigetherSoilSurface Cover;
	if (!Victim.BurialSamples.IsEmpty())
	{
		const auto State = DigetherSoil::BuildSurfaceRange(Grid, CoverDensity, FIntVector(0).ComponentMax(Lo - FIntVector(1)),
			Grid.Cells.ComponentMin(Hi + FIntVector(1)), Cover, false);
		if (State == DigetherSoil::EDigetherSoilRangeState::Failed) return;
	}
	const FTransform Transform = Soil.GetActorTransform();
	int32 Exposed = 0;
	for (int32 Ray = 0; Ray < 40; ++Ray)
	{
		const FVector Top = Victim.GetBurialTopPoint(Ray);
		const FVector Start = Transform.InverseTransformPosition(Top);
		const FVector End = Transform.InverseTransformPosition(Top + FVector::UpVector * 600.);
		bool bCovered = false;
		for (int32 T = 0; T < Cover.Indices.Num(); T += 3)
		{
			FVector Point, Normal;
			if (FMath::SegmentTriangleIntersection(Start, End, Cover.Vertices[Cover.Indices[T]],
				Cover.Vertices[Cover.Indices[T+1]], Cover.Vertices[Cover.Indices[T+2]], Point, Normal))
			{
				bCovered = true;
				break;
			}
		}
		if (!bCovered) ++Exposed;
	}
	const bool bUncovered = Exposed > Victim.ExposedTopSamples;
	Victim.ExposedTopSamples = Exposed;
	if (bAllowRescue && bUncovered && Exposed >= 28) Victim.FinishBurialRescue();
}

ADigetherCollapseLab::FCollapseContext ADigetherCollapseLab::CaptureContext() const
{
	FCollapseContext Result;
	Result.Soil = CeilingProbe->Soil;
	Result.ChunkBuffer = Result.Soil->ChunkMeshes.IsEmpty() ? nullptr : Result.Soil->ChunkMeshes[0].Get();
	Result.SoilTransform = Result.Soil->GetActorTransform();
	Result.RemovalTransform = CeilingRemoval->GetComponentTransform();
	Result.DepositTransform = Deposit->GetComponentTransform();
	Result.RemovalExtent = CeilingRemoval->GetUnscaledBoxExtent();
	Result.DepositExtent = Deposit->GetUnscaledBoxExtent();
	Result.Cells = Result.Soil->AppliedGrid.Cells;
	Result.Spacing = Result.Soil->AppliedGrid.SpacingCm;
	return Result;
}

bool ADigetherCollapseLab::MatchesContext(const FCollapseContext& Context) const
{
	return IsValid(CeilingProbe) && IsValid(CeilingProbe->Soil) && Context.Soil == CeilingProbe->Soil &&
		Context.ChunkBuffer == (CeilingProbe->Soil->ChunkMeshes.IsEmpty() ? nullptr : CeilingProbe->Soil->ChunkMeshes[0].Get()) &&
		Context.SoilTransform.Equals(CeilingProbe->Soil->GetActorTransform()) &&
		Context.RemovalTransform.Equals(CeilingRemoval->GetComponentTransform()) &&
		Context.DepositTransform.Equals(Deposit->GetComponentTransform()) &&
		Context.RemovalExtent.Equals(CeilingRemoval->GetUnscaledBoxExtent()) &&
		Context.DepositExtent.Equals(Deposit->GetUnscaledBoxExtent()) &&
		Context.Cells == CeilingProbe->Soil->AppliedGrid.Cells && Context.Spacing == CeilingProbe->Soil->AppliedGrid.SpacingCm;
}

bool ADigetherCollapseLab::StartCollapse(int32 Stage)
{
	if (Stage < 0 || Stage > 3 || bFalling || !FMath::IsFinite(FallSeconds) || FallSeconds < .1f || FallSeconds > 5.f ||
		!FMath::IsFinite(DustSeconds) || DustSeconds < .1f || DustSeconds > 5.f)
	{
		LastStatus = NSLOCTEXT("DigetherCollapseLab", "Busy", "이미 낙하 중이거나 연출 시간 설정이 잘못되었습니다");
		return false;
	}
	TArray<uint8> Candidate;
	FBox Changed(ForceInit);
	int32 Removed = 0, Added = 0;
	const int32 SelectedStage = Stage == 0 ? FMath::Clamp(PreviewStage, 1, 3) : Stage;
	TArray<FVector> CeilingVertices;
	if (PrepareCollapse(Candidate, Changed, Removed, Added, &CeilingVertices, nullptr, SelectedStage) != EDigetherSoilEditResult::Applied)
	{
		return false;
	}
	FallingStage = SelectedStage;
	FallingContext = CaptureContext();
	FallingRevision = FallingContext.Soil->Revision;
	ActiveFallSeconds = FallSeconds;
	ActiveDustSeconds = DustSeconds;
	FallElapsed = 0;
	DustElapsed = 0;
	bFalling = true;
	bDustActive = true;
	FallingSoil->ClearInstances();
	if (UPrimitiveComponent* Surface = Cast<UPrimitiveComponent>(FallingContext.Soil->GetRootComponent()))
	{
		FallingSoil->SetMaterial(0, Surface->GetMaterial(0));
	}
	SpawnFallingClods(CeilingVertices);
	FallingSoil->SetVisibility(true);
	Dust->SetVisibility(DustInstance != nullptr);
	LastStatus = NSLOCTEXT("DigetherCollapseLab", "Falling", "천장 흙 낙하 중 — 도착 후 굴착할 수 있습니다");
	SetActorTickEnabled(true);
	Tick(0.f);
	return true;
}

void ADigetherCollapseLab::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bFalling)
	{
		// 착지 시 최신 밀도에서 붕괴를 계산하므로 낙하 중 다른 위치의 굴착도 보존한다.
		if (!MatchesContext(FallingContext) || FallingContext.Soil->Revision < FallingRevision)
		{
			bFalling = false;
			LastStatus = NSLOCTEXT("DigetherCollapseLab", "Cancelled", "낙하 중 지형/배치가 변경되어 취소했습니다. C로 다시 요청하세요");
			ShowStatus();
		}
		else
		{
			FallElapsed += DeltaSeconds;
			const float Alpha = FMath::Clamp(FallElapsed / ActiveFallSeconds, 0.f, 1.f);
			const FVector Start = CeilingRemoval->GetComponentLocation();
			const FVector End = Deposit->GetComponentLocation();
			const FVector Extent = Deposit->GetScaledBoxExtent();
			AnimateFallingClods(Alpha);
			// 카메라 쪽 퇴적 표면 바깥에 놓아 착지한 흙 내부에 스프라이트가 묻히지 않도록 함
			FVector DustPosition = FMath::Lerp(Start, End, Alpha);
			if (APlayerController* Controller = GetWorld()->GetFirstPlayerController())
			{
				FVector ViewLocation;
				FRotator ViewRotation;
				Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
				const FVector Direction = (ViewLocation - DustPosition).GetSafeNormal2D();
				const FVector LocalDirection = Deposit->GetComponentQuat().UnrotateVector(Direction).GetAbs();
				DustPosition += Direction * (FVector::DotProduct(LocalDirection, Extent) + 25.0);
			}
			Dust->SetWorldLocation(DustPosition);
			if (Alpha >= 1.f)
			{
				bFalling = false;
				ApplyLanding(FallingStage);
				ShowStatus();
			}
		}
		if (!bFalling)
		{
			FallingSoil->SetVisibility(false);
			FallingSoil->ClearInstances();
			Clods.Reset();
			DustElapsed = 0;
		}
	}
	if (bDustActive)
	{
		if (!bFalling)
		{
			DustElapsed += DeltaSeconds;
		}
		const float Opacity = .32f * (1.f - FMath::Clamp(DustElapsed / ActiveDustSeconds, 0.f, 1.f));
		if (DustInstance)
		{
			DustInstance->SetScalarParameterValue(TEXT("DustOpacity"), Opacity);
		}
		Dust->SetWorldScale3D(FVector(1.f + DustElapsed * .4f));
		if (DustElapsed >= ActiveDustSeconds)
		{
			bDustActive = false;
			Dust->SetVisibility(false);
		}
	}
	if (!bFalling && !bDustActive)
	{
		SetActorTickEnabled(false);
	}
}
