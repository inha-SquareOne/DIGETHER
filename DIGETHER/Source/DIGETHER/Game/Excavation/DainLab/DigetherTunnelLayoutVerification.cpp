#include "DigetherTunnelLayoutVerification.h"
#include "DigetherExcavationLabCharacter.h"
#include "Game/Excavation/Dain/DigetherDiggableSoil.h"
#include "Engine/World.h"
#include "Camera/CameraComponent.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"

DEFINE_LOG_CATEGORY_STATIC(LogDigetherTunnelTest, Log, All);

ADigetherTunnelLayoutVerification::ADigetherTunnelLayoutVerification()
{
	PrimaryActorTick.bCanEverTick = true;
}
void ADigetherTunnelLayoutVerification::BeginPlay()
{
	Super::BeginPlay();
#if UE_BUILD_SHIPPING
	SetActorTickEnabled(false);
#else
	SetActorTickEnabled(FParse::Param(FCommandLine::Get(), TEXT("TunnelVerify")));
#endif
}
void ADigetherTunnelLayoutVerification::Advance()
{
	++Stage;
	StageSeconds = 0;
	Waypoint = 0;
	Character->GetCharacterMovement()->StopMovementImmediately();
	UE_LOG(LogDigetherTunnelTest, Display, TEXT("Stage=%d position=%s"), Stage, *Character->GetActorLocation().ToString());
}
bool ADigetherTunnelLayoutVerification::MoveTo(FVector Target, bool bDig)
{
	FVector Direction = Target - Character->GetActorLocation();
	Direction.Z = 0;
	if (Direction.Size() < 35) return true;
	Direction.Normalize();
	FRotator Aim = Direction.Rotation();
	if (bDig)
	{
		// 기존 굴착 시험과 동일하게 첫 흙 타격을 유지하고, 시선이 열린 뒤 캡슐 장애물 조준
		Aim = Character->GetController()->GetControlRotation();
		FHitResult ViewHit, BodyHit;
		if (!Character->TraceTool(ViewHit) || !Cast<ADigetherDiggableSoil>(ViewHit.GetActor()))
		{
			FCollisionQueryParams Params(SCENE_QUERY_STAT(TunnelBody), false, Character.Get());
			const FVector Start = Character->GetActorLocation();
			if (GetWorld()->SweepSingleByChannel(BodyHit, Start, Start + Direction * 100, FQuat::Identity,
				ECC_Pawn, FCollisionShape::MakeCapsule(35, 90), Params) && Cast<ADigetherDiggableSoil>(BodyHit.GetActor()))
			{
				// 모서리를 스치는 시선이 빗나가지 않도록 표면 안쪽을 조준, 실제 타격은 기존 TraceTool 판정
				Aim = (BodyHit.ImpactPoint - BodyHit.ImpactNormal * 10 - Character->FindComponentByClass<UCameraComponent>()->GetComponentLocation()).Rotation();
			}
			// 실제 TraceTool과 같은 카메라·복합 충돌로 머리 위 잔여 흙까지 탐색
			const FVector Eye = Character->FindComponentByClass<UCameraComponent>()->GetComponentLocation();
			Params.bTraceComplex = true;
			for (const double Pitch : {-60.0, -30.0, 0.0, 30.0, 60.0, 80.0, -80.0})
			{
				const FRotator CandidateAim(Pitch, Direction.Rotation().Yaw, 0);
				FHitResult RimHit;
				if (GetWorld()->LineTraceSingleByChannel(RimHit, Eye, Eye + CandidateAim.Vector() * 400, ECC_Visibility, Params) &&
					Cast<ADigetherDiggableSoil>(RimHit.GetActor()))
				{
					Aim = CandidateAim;
					break;
				}
			}
		}
		if (TotalSeconds >= NextProbeSeconds)
		{
			NextProbeSeconds = TotalSeconds + 5;
			UE_LOG(LogDigetherTunnelTest, Display, TEXT("DigProbe stage=%d position=%s hit=%s point=%s aim=%s"),
				Stage, *Character->GetActorLocation().ToString(), *GetNameSafe(ViewHit.GetActor()),
				*ViewHit.ImpactPoint.ToString(), *Aim.ToString());
			UE_LOG(LogDigetherTunnelTest, Display, TEXT("BodyProbe hit=%s point=%s normal=%s penetrating=%d depth=%f"),
				*GetNameSafe(BodyHit.GetActor()), *BodyHit.ImpactPoint.ToString(), *BodyHit.ImpactNormal.ToString(),
				BodyHit.bStartPenetrating, BodyHit.PenetrationDepth);
			if (Stage == 8) FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("QA/stair-probe.png"), false, false);
		}
	}
	Character->GetController()->SetControlRotation(Aim);
	if (bDig) Character->DigOnce();
	Character->AddMovementInput(Direction);
	return false;
}
void ADigetherTunnelLayoutVerification::Finish(bool bPassed, const FString& Reason)
{
	const FString Result = FString::Printf(TEXT("passed=%s stage=%d reason=%s shortcutDigs=%u pitDigs=%u position=%s teleports=0"),
		bPassed ? TEXT("true") : TEXT("false"), Stage, *Reason,
		Shortcut.IsValid() ? Shortcut->GetRevision() : 0, Pit.IsValid() ? Pit->GetRevision() : 0,
		Character.IsValid() ? *Character->GetActorLocation().ToString() : TEXT("none"));
	const FString Directory = FPaths::ProjectSavedDir() / TEXT("QA");
	IFileManager::Get().MakeDirectory(*Directory, true);
	FFileHelper::SaveStringToFile(Result, *(Directory / TEXT("tunnel-result.txt")));
	UE_LOG(LogDigetherTunnelTest, Display, TEXT("RESULT %s"), *Result);
	SetActorTickEnabled(false);
	FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1);
}
void ADigetherTunnelLayoutVerification::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TotalSeconds += DeltaSeconds;
	StageSeconds += DeltaSeconds;
	if (TotalSeconds > 240 || StageSeconds > 90) { Finish(false, TEXT("movement timeout")); return; }
	if (!Character.IsValid()) Character = Cast<ADigetherExcavationLabCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
	if (!Shortcut.IsValid() || !Pit.IsValid())
	{
		for (TActorIterator<ADigetherDiggableSoil> It(GetWorld()); It; ++It)
		{
			if (It->ActorHasTag(TEXT("TunnelShortcut"))) Shortcut = *It;
			if (It->ActorHasTag(TEXT("TunnelPit"))) Pit = *It;
		}
	}
	if (!Character.IsValid() || !Character->GetController() || !Shortcut.IsValid() || !Pit.IsValid()) return;
	if (Character->GetActorLocation().Z < -400) { Finish(false, TEXT("fell outside map")); return; }
	if (Stage == 0)
	{
		if (!Shortcut->SetComparisonChunkCells(8) || !Pit->SetComparisonChunkCells(8))
		{ Finish(false, TEXT("soil initialization")); return; }
		Advance();
	}
	else if (Stage == 1)
	{
		// 지름길을 사용하지 않는 기본 동선 왕복, 실제 이동 입력만 사용
		// 두 암반 코너의 열린 쪽을 교대로 지나고 같은 경로로 귀환
		const FVector Route[] = {{400,0,0},{400,1300,0},{800,1550,0},{1200,1550,0},
			{2400,1150,0},{2900,1150,0},{3200,1300,0},{3200,0,0},
			{3200,1300,0},{2900,1150,0},{2400,1150,0},{1200,1550,0},
			{800,1550,0},{400,1300,0},{400,0,0},{1200,0,0}};
		if (MoveTo(Route[Waypoint]) && ++Waypoint == UE_ARRAY_COUNT(Route)) Advance();
	}
	else if (Stage == 2)
	{
		MoveTo(FVector(2000,0,0));
		if (StageSeconds > 3)
		{
			const float X = Character->GetActorLocation().X;
			if (X < 1400 || X > 1515) { Finish(false, TEXT("intact shortcut did not block")); return; }
			Advance();
		}
	}
	else if (Stage == 3)
	{
		// 사거리 끝의 측벽 대신 가까운 고정 바닥을 조준, 외곽 벽은 별도 이동 검사
		Character->GetController()->SetControlRotation(FRotator(-80,0,0));
		if (StageSeconds > .5)
		{
			FHitResult Hit;
			const uint32 Before = Shortcut->GetDensityHash();
			if (!Character->TraceTool(Hit) || !Hit.GetActor()->ActorHasTag(TEXT("ProtectedRock")) ||
				Character->DigOnce() || Shortcut->GetDensityHash() != Before)
			{ Finish(false, TEXT("rock rejection")); return; }
			Character->GetController()->SetControlRotation(FRotator::ZeroRotator);
			Advance();
		}
	}
	else if (Stage == 4)
	{
		if (MoveTo(FVector(2000,0,0), true))
		{
			if (Shortcut->GetRevision() < 2) { Finish(false, TEXT("shortcut needs repeated digs")); return; }
			Advance();
		}
	}
	else if (Stage == 5)
	{
		// 지름길 개방 뒤 새 입구 문틀을 통과하고 출발 쪽으로 귀환
		const FVector Route[] = {{3350,0,0},{1200,0,0}};
		if (MoveTo(Route[Waypoint]))
		{
			if (Waypoint == 0)
			{
				FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("QA/dungeon-forecourt.png"), false, false);
			}
			if (++Waypoint == UE_ARRAY_COUNT(Route)) Advance();
		}
	}
	else if (Stage == 6)
	{
		const FVector Route[] = {{400,0,0},{400,1300,0},{800,1550,0},{1200,1550,0},
			{2050,1550,0},{2050,1800,0},{2050,2500,0}};
		if (MoveTo(Route[Waypoint]) && ++Waypoint == UE_ARRAY_COUNT(Route))
		{
			if (Character->GetActorLocation().Z < 70) { Finish(false, TEXT("pit top missing before digging")); return; }
			Advance();
		}
	}
	else if (Stage == 7)
	{
		Character->GetController()->SetControlRotation(FRotator(-89,0,0));
		if (StageSeconds > .3) Character->DigOnce();
		if (Character->GetActorLocation().Z < -120)
		{
			if (Pit->GetRevision() < 2) { Finish(false, TEXT("pit needs repeated digs")); return; }
			Character->GetController()->SetControlRotation(FRotator::ZeroRotator);
			Advance();
		}
	}
	else if (Stage == 8)
	{
		// 높은 계단 측면을 가로지르지 않고 북쪽 최하단 계단으로 진입
		const FVector Route[] = {{2050,2550,0},{2350,2550,0}};
		if (MoveTo(Route[Waypoint], true) && ++Waypoint == UE_ARRAY_COUNT(Route)) Advance();
	}
	else if (Stage == 9)
	{
		if (MoveTo(FVector(2350,1500,0)))
		{
			if (Character->GetActorLocation().Z < 70) { Finish(false, TEXT("stairs did not restore height")); return; }
			Advance();
		}
	}
	else if (Stage == 10)
	{
		// 계단 복귀 후 동쪽 코너의 남쪽 개구부를 통해 외곽 검사 위치로 이동
		const FVector Route[] = {{2400,1150,0},{2900,1150,0},{3500,1300,0}};
		if (MoveTo(Route[Waypoint]) && ++Waypoint == UE_ARRAY_COUNT(Route)) Advance();
	}
	else if (Stage == 11)
	{
		MoveTo(FVector(4100,1300,0));
		if (StageSeconds > 3)
		{
			const float X = Character->GetActorLocation().X;
			if (X < 3500 || X > 3570) { Finish(false, TEXT("outer boundary collision")); return; }
			Advance();
		}
	}
	else if (Stage == 12)
	{
		// 외곽 벽 대신 복귀 통로를 촬영, 카메라 갱신 후 화면 기록
		Character->GetController()->SetControlRotation(FRotator(0,180,0));
		if (StageSeconds > .5)
		{
			FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("QA/tunnel-layout.png"), false, false);
			Advance();
		}
	}
	else if (Stage == 13 && StageSeconds > 1)
	{
		Finish(true, TEXT("route roundtrip, shortcut roundtrip, rock rejection, pit fall, stairs return, outer boundary"));
	}
}
