#include "Game/Monster/DigetherMonsterCameraSampleComponent.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Game/Monster/DigetherMonsterSenseRules.h"
#include "Game/Monster/DigetherMonsterSenseSubsystem.h"
#include "Game/Monster/DigetherMonsterSettings.h"
#include "GameFramework/PlayerController.h"

UDigetherMonsterCameraSampleComponent::UDigetherMonsterCameraSampleComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	// Server RPC 경로용, 복제할 상태는 없음
	SetIsReplicatedByDefault(true);
}

void UDigetherMonsterCameraSampleComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const APlayerController* Controller = Cast<APlayerController>(GetOwner());
	if (!Controller || !Controller->IsLocalController())
	{
		// 원격 Controller 사본은 전송하지 않음, 서버는 수신만
		SetComponentTickEnabled(false);
		return;
	}
	const UDigetherMonsterSettings& Settings = UDigetherMonsterSettings::Get();
	SinceLastSend += DeltaTime;
	const FRotator Current = Controller->PlayerCameraManager ? Controller->PlayerCameraManager->GetCameraRotation() : FRotator::ZeroRotator;
	if (!bHasSent || SinceLastSend >= Settings.CameraSampleIntervalSeconds
		|| ShouldResend(LastSentRotation, Current, Settings.CameraSampleResendDegrees))
	{
		SendSample();
	}
}

bool UDigetherMonsterCameraSampleComponent::ShouldResend(const FRotator& Previous, const FRotator& Current, const float ThresholdDegrees)
{
	if (ThresholdDegrees <= 0.f)
	{
		return false;
	}
	const float Dot = FVector::DotProduct(Previous.Vector(), Current.Vector());
	return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Dot, -1.f, 1.f))) >= ThresholdDegrees;
}

bool UDigetherMonsterCameraSampleComponent::BuildLocalSample(FDigetherCameraSample& OutSample) const
{
	const APlayerController* Controller = Cast<APlayerController>(GetOwner());
	const APlayerCameraManager* Camera = Controller ? Controller->PlayerCameraManager.Get() : nullptr;
	if (!Camera)
	{
		return false;
	}
	const FMinimalViewInfo& View = Camera->GetCameraCacheView();
	OutSample.Location = Camera->GetCameraLocation();
	OutSample.Rotation = Camera->GetCameraRotation();

	float Aspect = 16.f / 9.f;
	bool bMaintainX = true;
	if (View.bConstrainAspectRatio && View.AspectRatio > 0.f)
	{
		Aspect = View.AspectRatio;
	}
	else
	{
		const ULocalPlayer* LocalPlayer = Controller->GetLocalPlayer();
		if (LocalPlayer && LocalPlayer->ViewportClient)
		{
			FVector2D Size;
			LocalPlayer->ViewportClient->GetViewportSize(Size);
			if (Size.X > 0.0 && Size.Y > 0.0)
			{
				Aspect = static_cast<float>(Size.X / Size.Y);
			}
		}
		// 실제 투영과 같은 축 규칙(CameraStackTypes.cpp), 뷰 지정값 우선
		const EAspectRatioAxisConstraint Constraint = View.AspectRatioAxisConstraint.Get(
			LocalPlayer ? LocalPlayer->AspectRatioAxisConstraint.GetValue() : AspectRatio_MaintainXFOV);
		bMaintainX = Constraint == AspectRatio_MaintainXFOV || (Constraint == AspectRatio_MajorAxisFOV && Aspect >= 1.f);
	}
	DigetherMonsterSenseRules::ComputeEffectiveFov(Camera->GetFOVAngle(), Aspect, bMaintainX, OutSample.HorizontalFov, OutSample.VerticalFov);
	return true;
}

void UDigetherMonsterCameraSampleComponent::SendSample()
{
	FDigetherCameraSample Sample;
	if (!BuildLocalSample(Sample))
	{
		return;
	}
	Sample.Sequence = NextSequence++;
	LastSentRotation = Sample.Rotation;
	SinceLastSend = 0.f;
	bHasSent = true;
	// 리슨 호스트 로컬 플레이어는 RPC가 즉시 로컬 실행
	ServerSubmitCameraSample(Sample);
}

void UDigetherMonsterCameraSampleComponent::ServerSubmitCameraSample_Implementation(const FDigetherCameraSample& Sample)
{
	APlayerController* Controller = Cast<APlayerController>(GetOwner());
	UWorld* World = GetWorld();
	if (!Controller || !World)
	{
		return;
	}
	if (UDigetherMonsterSenseSubsystem* Sense = World->GetSubsystem<UDigetherMonsterSenseSubsystem>())
	{
		Sense->SubmitCameraSample(Controller, Sample);
	}
}
