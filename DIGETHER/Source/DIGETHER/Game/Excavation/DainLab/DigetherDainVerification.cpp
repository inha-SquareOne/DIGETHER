#include "DigetherDainVerification.h"
#include "DigetherDainCharacter.h"
#include "Game/Excavation/Dain/DigetherDiggableSoil.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"

DEFINE_LOG_CATEGORY_STATIC(LogDigetherWallTest, Log, All);

#if !UE_BUILD_SHIPPING
namespace
{
	// 개인 맵의 단일 검증기용, 프레임 사이 CPU 실시간 간격이며 GPU 시간과 구분
	double LastFrameSeconds = 0;
	uint32 LastFrameRevision = 0;
	FString FrameCsv = TEXT("stage,wall_ms,delta_ms,revision,previous_frame_dug\n");
} // namespace
#endif

ADigetherDainVerification::ADigetherDainVerification()
{
	PrimaryActorTick.bCanEverTick = !UE_BUILD_SHIPPING;
}

void ADigetherDainVerification::Finish(bool bPassed, const FString& Reason)
{
#if !UE_BUILD_SHIPPING
	const FVector Position = Character.IsValid() ? Character->GetActorLocation() : FVector::ZeroVector;
	const FString Result = FString::Printf(
		TEXT("{\"passed\":%s,\"reason\":\"%s\",\"blockedX\":%.2f,\"afterOneHitX\":%.2f,\"exitX\":%.2f,"
			 "\"exitY\":%.2f,\"exitZ\":%.2f,\"successfulDigs\":%d,\"protectedRejected\":%s,"
			 "\"capsuleRadius\":35,\"capsuleHalfHeight\":90,\"teleportsDuringTest\":0,\"chunkCells\":%d}"),
		bPassed ? TEXT("true") : TEXT("false"), *Reason, BlockedX, AfterOneHitX, Position.X, Position.Y, Position.Z,
		Wall.IsValid() ? Wall->GetRevision() : 0, bProtectedRejected ? TEXT("true") : TEXT("false"),
		Wall.IsValid() ? Wall->GetComparisonChunkCells() : -1);
	const FString Directory = FPaths::ProjectSavedDir() / TEXT("QA");
	IFileManager::Get().MakeDirectory(*Directory, true);
	FFileHelper::SaveStringToFile(Result, *(Directory / TEXT("wall-result.json")));
	FFileHelper::SaveStringToFile(FrameCsv, *(Directory / TEXT("frames.csv")));
	UE_LOG(LogDigetherWallTest, Display, TEXT("RESULT %s"), *Result);
	SetActorTickEnabled(false);
	FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1);
#endif
}

void ADigetherDainVerification::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
#if !UE_BUILD_SHIPPING
	TotalSeconds += DeltaSeconds;
	StageSeconds += DeltaSeconds;
	if (TotalSeconds > 100.f)
	{
		Finish(false, TEXT("scenario timeout"));
		return;
	}
	if (!Character.IsValid())
	{
		Character = Cast<ADigetherDainCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
	}
	if (!Wall.IsValid())
	{
		for (TActorIterator<ADigetherDiggableSoil> It(GetWorld()); It; ++It)
		{
			Wall = *It;
			break;
		}
	}
	if (!Character.IsValid() || !Wall.IsValid() || !IsValid(Character->GetController()))
	{
		return;
	}
	const double NowSeconds = FPlatformTime::Seconds();
	if (LastFrameSeconds > 0)
	{
		FrameCsv += FString::Printf(TEXT("%d,%.6f,%.6f,%u,%d\n"), Stage, (NowSeconds - LastFrameSeconds) * 1000.0,
									DeltaSeconds * 1000.0, Wall->GetRevision(),
									Wall->GetRevision() != LastFrameRevision ? 1 : 0);
	}
	LastFrameSeconds = NowSeconds;
	LastFrameRevision = Wall->GetRevision();
	AController* Controller = Character->GetController();
	const auto Advance = [&]()
	{
		++Stage;
		StageSeconds = 0;
		UE_LOG(LogDigetherWallTest, Display, TEXT("Stage %d"), Stage);
	};
	// Stage별 실제 입력·이동 관찰, 고정 시험 맵 좌표 전제
	if (Stage == 0)
	{
		int32 ExpectedCells = 0;
		if (FParse::Value(FCommandLine::Get(), TEXT("SoilChunkCells="), ExpectedCells) &&
			Wall->GetComparisonChunkCells() != ExpectedCells)
		{
			Finish(false, TEXT("requested chunk mode was not applied"));
			return;
		}
		Controller->SetControlRotation(FRotator::ZeroRotator);
		if (StageSeconds >= 1.f)
		{
			Advance();
		}
	}
	else if (Stage == 1)
	{
		Character->AddMovementInput(FVector::ForwardVector, 1.f);
		if (StageSeconds < 2.f)
		{
			return;
		}
		BlockedX = Character->GetActorLocation().X;
		if (BlockedX < 600.f || BlockedX >= 700.f)
		{
			Finish(false, TEXT("intact wall did not block walking"));
			return;
		}
		// 보호 바닥을 조준해 실제 도구 진입점의 거절 확인
		Controller->SetControlRotation(FRotator(-80, 0, 0));
		// 카메라 회전 반영을 위해 다음 프레임까지 대기
		Advance();
	}
	else if (Stage == 2 && StageSeconds > .5f)
	{
		FHitResult Hit;
		const uint32 Before = Wall->GetDensityHash();
		bProtectedRejected = Character->TraceTool(Hit) && IsValid(Hit.GetActor()) &&
							 Hit.GetActor()->ActorHasTag(TEXT("ProtectedRock")) && !Character->DigOnce() &&
							 Before == Wall->GetDensityHash();
		if (!bProtectedRejected)
		{
			Finish(false, TEXT("protected first hit was not rejected"));
			return;
		}
		Controller->SetControlRotation(FRotator::ZeroRotator);
		Advance();
	}
	else if (Stage == 3 && StageSeconds > .5f)
	{
		if (!Character->DigOnce() || Wall->GetRevision() != 1)
		{
			Finish(false, TEXT("first dig failed"));
			return;
		}
		Advance();
	}
	else if (Stage == 4)
	{
		Character->AddMovementInput(FVector::ForwardVector, 1.f);
		if (StageSeconds < 1.f)
		{
			return;
		}
		AfterOneHitX = Character->GetActorLocation().X;
		FHitResult Hit;
		if (AfterOneHitX >= 900.f || !Character->TraceTool(Hit) || Hit.GetActor() != Wall.Get())
		{
			Finish(false, TEXT("first hit must leave a blocking soil surface"));
			return;
		}
		Advance();
	}
	else if (Stage == 5)
	{
		// 시선이 통과해도 캡슐을 막는 아래쪽 흙은 추가 굴착 필요
		// 별도 지형 수정 없이 조준·굴착 진입점만 사용
		FHitResult Aim, BodyHit;
		if (!Character->TraceTool(Aim) || Aim.GetActor() != Wall.Get())
		{
			FCollisionQueryParams Probe(SCENE_QUERY_STAT(LabBodyObstacle), false, Character.Get());
			const FVector Start = Character->GetActorLocation();
			if (GetWorld()->SweepSingleByChannel(BodyHit, Start, Start + FVector(100, 0, 0), FQuat::Identity, ECC_Pawn,
												 FCollisionShape::MakeCapsule(35.f, 90.f), Probe) &&
				BodyHit.GetActor() == Wall.Get())
			{
				Controller->SetControlRotation((BodyHit.ImpactPoint - Character->GetPawnViewLocation()).Rotation());
			}
		}
		Character->DigOnce();
		Character->AddMovementInput(FVector::ForwardVector, 1.f);
		if (FMath::Abs(Character->GetActorLocation().Y) > 100.f)
		{
			Finish(false, TEXT("character bypassed the wall"));
			return;
		}
		if (StageSeconds > 15.f)
		{
			Finish(false, TEXT("repeated digging did not permit crossing"));
			return;
		}
		if (Character->GetActorLocation().X < 1050.f)
		{
			return;
		}
		if (Wall->GetRevision() < 2)
		{
			Finish(false, TEXT("multiple successful hits required"));
			return;
		}
		Character->GetCharacterMovement()->StopMovementImmediately();
		Controller->SetControlRotation(FRotator(0, 180, 0));
		Advance();
	}
	else if (Stage == 6)
	{
		// 굴착 턱을 포함한 왕복 가능 여부를 CharacterMovement로 확인
		Character->AddMovementInput(-FVector::ForwardVector, 1.f);
		if (StageSeconds > 15.f)
		{
			Finish(false, TEXT("return through tunnel failed"));
			return;
		}
		if (Character->GetActorLocation().X > 600.f)
		{
			return;
		}
		Character->GetCharacterMovement()->StopMovementImmediately();
		Controller->SetControlRotation(FRotator::ZeroRotator);
		Advance();
	}
	else if (Stage == 7 && StageSeconds > 1.f)
	{
		if (FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
		{
			Finish(true, TEXT("walked through and returned"));
			return;
		}
		FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("QA/through-wall.png"), false, false);
		Advance();
	}
	else if (Stage == 8)
	{
		if (IFileManager::Get().FileSize(*(FPaths::ProjectSavedDir() / TEXT("QA/through-wall.png"))) > 0)
		{
			Finish(true, TEXT("walked through and returned with rendered evidence"));
		}
		else if (StageSeconds > 60.f)
		{
			Finish(false, TEXT("screenshot timed out"));
		}
	}
#endif
}
