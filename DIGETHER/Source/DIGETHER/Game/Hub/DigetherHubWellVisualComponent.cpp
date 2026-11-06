#include "Game/Hub/DigetherHubWellVisualComponent.h"

#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Game/Framework/DigetherGameplayGameState.h"
#include "Game/Run/DigetherGameOverPublicStateComponent.h"
#include "Game/Run/DigetherGameOverRules.h"

UDigetherHubWellVisualComponent::UDigetherHubWellVisualComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bTickEvenWhenPaused = false;
}

void UDigetherHubWellVisualComponent::FSceneSnapshot::Capture(const USceneComponent& Component)
{
	RelativeTransform = Component.GetRelativeTransform();
	bVisible = Component.GetVisibleFlag();
	bHiddenInGame = Component.bHiddenInGame;
}

void UDigetherHubWellVisualComponent::FSceneSnapshot::Restore(USceneComponent& Component) const
{
	Component.SetRelativeTransform(RelativeTransform);
	Component.SetVisibility(bVisible, false);
	Component.SetHiddenInGame(bHiddenInGame, false);
}

bool UDigetherHubWellVisualComponent::StartFlood(const float FloodSeconds, const float FailMotionSeconds,
	UStaticMeshComponent* FloodSurface, UStaticMeshComponent* SuctionShape, UPointLightComponent* Glow)
{
	// 중복 시작도 최초 배치로 복원한 뒤 캡처, 이전 연출 변환 누적 방지
	StopVisuals();
	if (!FMath::IsFinite(FloodSeconds) || FloodSeconds < 0.f || !FMath::IsFinite(FailMotionSeconds)
		|| FailMotionSeconds < 0.f || (!IsValid(FloodSurface) && !IsValid(SuctionShape) && !IsValid(Glow))
		|| (IsValid(FloodSurface) && FloodSurface == SuctionShape))
	{
		return false;
	}
	ActiveFloodSurface = IsValid(FloodSurface) ? FloodSurface : nullptr;
	ActiveSuctionShape = IsValid(SuctionShape) ? SuctionShape : nullptr;
	ActiveGlow = IsValid(Glow) ? Glow : nullptr;
	if (ActiveFloodSurface)
	{
		FloodSnapshot.Capture(*ActiveFloodSurface);
		ActiveFloodSurface->SetVisibility(true, false);
		ActiveFloodSurface->SetHiddenInGame(false, false);
	}
	if (ActiveSuctionShape)
	{
		SuctionSnapshot.Capture(*ActiveSuctionShape);
		ActiveSuctionShape->SetVisibility(false, false);
		ActiveSuctionShape->SetHiddenInGame(true, false);
	}
	if (ActiveGlow)
	{
		GlowSnapshot.Capture(*ActiveGlow);
		OriginalGlowColor = ActiveGlow->LightColor;
		OriginalGlowIntensity = ActiveGlow->Intensity;
		ActiveGlow->SetVisibility(true, false);
		ActiveGlow->SetHiddenInGame(false, false);
	}
	FloodDurationSeconds = FloodSeconds;
	SuctionDurationSeconds = FMath::Max(0.f, FailMotionSeconds - FloodSeconds);
	Phase = EVisualPhase::Flood;
	if (!RefreshPublicTiming() && bUsingPublicTiming)
	{
		StopVisuals();
		return false;
	}
	ApplyVisuals();
	SetComponentTickEnabled(true);
	return true;
}

bool UDigetherHubWellVisualComponent::StartSuction()
{
	if (Phase == EVisualPhase::Inactive || !HasVisualComponents())
	{
		StopVisuals();
		return false;
	}
	if (Phase == EVisualPhase::Suction)
	{
		return true;
	}
	if (!RefreshPublicTiming() && bUsingPublicTiming)
	{
		StopVisuals();
		return false;
	}
	// 흡입 시작·종료는 서버 이벤트가 결정, 로컬 시간은 외형 보간에만 사용
	Phase = EVisualPhase::Suction;
	FloodElapsedSeconds = FloodDurationSeconds;
	if (!bUsingPublicTiming)
	{
		SuctionElapsedSeconds = 0.0;
	}
	if (IsValid(ActiveSuctionShape))
	{
		ActiveSuctionShape->SetVisibility(true, false);
		ActiveSuctionShape->SetHiddenInGame(false, false);
	}
	ApplyVisuals();
	return true;
}

void UDigetherHubWellVisualComponent::StopVisuals()
{
	if (IsValid(ActiveFloodSurface))
	{
		FloodSnapshot.Restore(*ActiveFloodSurface);
	}
	if (IsValid(ActiveSuctionShape))
	{
		SuctionSnapshot.Restore(*ActiveSuctionShape);
	}
	if (IsValid(ActiveGlow))
	{
		GlowSnapshot.Restore(*ActiveGlow);
		ActiveGlow->SetLightFColor(OriginalGlowColor);
		ActiveGlow->SetIntensity(OriginalGlowIntensity);
	}
	ActiveFloodSurface = nullptr;
	ActiveSuctionShape = nullptr;
	ActiveGlow = nullptr;
	FloodElapsedSeconds = 0.0;
	SuctionElapsedSeconds = 0.0;
	FloodDurationSeconds = 0.f;
	SuctionDurationSeconds = 0.f;
	bUsingPublicTiming = false;
	Phase = EVisualPhase::Inactive;
	SetComponentTickEnabled(false);
}

bool UDigetherHubWellVisualComponent::HasVisualComponents() const
{
	return IsValid(ActiveFloodSurface) || IsValid(ActiveSuctionShape) || IsValid(ActiveGlow);
}

bool UDigetherHubWellVisualComponent::RefreshPublicTiming()
{
	const UWorld* World = GetWorld();
	const ADigetherGameplayGameState* State = World && !World->IsPreviewWorld()
		? World->GetGameState<ADigetherGameplayGameState>() : nullptr;
	const UDigetherGameOverPublicStateComponent* Public = IsValid(State) ? State->GetGameOverPublicStateComponent() : nullptr;
	if (!IsValid(Public))
	{
		return false;
	}
	bUsingPublicTiming = true;
	const FDigetherGameOverPublicState Snapshot = Public->GetPublicState();
	const double ServerNow = State->GetServerWorldTimeSeconds();
	if (Snapshot.Phase != EDigetherGameOverPhase::FailureEvent || !DigetherGameOverRules::IsValidTuning(Snapshot.Tuning)
		|| !FMath::IsFinite(Snapshot.PhaseStartServerSeconds) || !FMath::IsFinite(ServerNow))
	{
		return false;
	}
	FloodDurationSeconds = Snapshot.Tuning.FailMotionSeconds * Snapshot.Tuning.FloodRatio;
	SuctionDurationSeconds = Snapshot.Tuning.FailMotionSeconds - FloodDurationSeconds;
	// 부모의 FloodSeconds는 늦은 연결에서 남은 시간, 원래 범람 길이는 공개 튜닝 사용
	FloodElapsedSeconds = FMath::Clamp(static_cast<double>(FloodDurationSeconds) - Public->GetSecondsUntilSuction(),
		0.0, static_cast<double>(FloodDurationSeconds));
	SuctionElapsedSeconds = FMath::Max(0.0, ServerNow - Snapshot.PhaseStartServerSeconds - FloodDurationSeconds);
	return true;
}

void UDigetherHubWellVisualComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (Phase == EVisualPhase::Inactive)
	{
		return;
	}
	if (!HasVisualComponents())
	{
		StopVisuals();
		return;
	}
	if (!FMath::IsFinite(DeltaTime) || DeltaTime <= 0.f || TickType == LEVELTICK_PauseTick
		|| (GetWorld() && GetWorld()->IsPaused()))
	{
		return;
	}
	if (!RefreshPublicTiming())
	{
		if (bUsingPublicTiming)
		{
			StopVisuals();
			return;
		}
		FloodElapsedSeconds = FMath::Min(static_cast<double>(FloodDurationSeconds), FloodElapsedSeconds + DeltaTime);
		if (Phase == EVisualPhase::Suction)
		{
			SuctionElapsedSeconds += DeltaTime;
		}
	}
	ApplyVisuals();
}

void UDigetherHubWellVisualComponent::ApplyVisuals()
{
	const float FloodAlpha = FloodDurationSeconds > 0.f
		? static_cast<float>(FMath::Clamp(FloodElapsedSeconds / FloodDurationSeconds, 0.0, 1.0)) : 1.f;
	if (IsValid(ActiveFloodSurface))
	{
		FTransform Transform = FloodSnapshot.RelativeTransform;
		const float Scale = FMath::Lerp(FloodStartScaleMultiplier, FloodEndScaleMultiplier, FloodAlpha);
		Transform.SetScale3D(Transform.GetScale3D() * FVector(Scale, Scale, 1.f));
		Transform.AddToTranslation(FVector(0.0, 0.0, FloodRiseCm * FloodAlpha));
		ActiveFloodSurface->SetRelativeTransform(Transform);
	}
	float Pulse = 1.f;
	if (Phase == EVisualPhase::Suction)
	{
		Pulse += SuctionPulseScale * static_cast<float>(FMath::Sin(2.0 * UE_DOUBLE_PI * SuctionPulseHz * SuctionElapsedSeconds));
		if (IsValid(ActiveSuctionShape))
		{
			const float SuctionAlpha = SuctionDurationSeconds > 0.f
				? static_cast<float>(FMath::Clamp(SuctionElapsedSeconds / SuctionDurationSeconds, 0.0, 1.0)) : 1.f;
			FTransform Transform = SuctionSnapshot.RelativeTransform;
			const FQuat Rotation(FVector::UpVector, FMath::DegreesToRadians(
				FMath::Fmod(SuctionRotationDegreesPerSecond * SuctionElapsedSeconds, 360.0)));
			Transform.SetRotation(Transform.GetRotation() * Rotation);
			const float Scale = FMath::Lerp(1.f, SuctionEndScaleMultiplier, SuctionAlpha) * Pulse;
			Transform.SetScale3D(Transform.GetScale3D() * FVector(Scale, Scale * SuctionEllipseRatio, Pulse));
			ActiveSuctionShape->SetRelativeTransform(Transform);
		}
	}
	if (IsValid(ActiveGlow))
	{
		const bool bSuction = Phase == EVisualPhase::Suction;
		ActiveGlow->SetLightColor(bSuction ? SuctionGlowColor : FloodGlowColor);
		ActiveGlow->SetIntensity(bSuction ? SuctionGlowIntensity * Pulse : FloodGlowIntensity * FMath::Lerp(0.25f, 1.f, FloodAlpha));
	}
}

void UDigetherHubWellVisualComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopVisuals();
	Super::EndPlay(EndPlayReason);
}
