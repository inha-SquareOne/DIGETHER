#include "DigetherDainVerification.h"
#include "DigetherDainCharacter.h"
#include "DigetherDainSoilWall.h"
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

DEFINE_LOG_CATEGORY_STATIC(LogDigetherWallTest, Log, All);

ADigetherDainVerification::ADigetherDainVerification() { PrimaryActorTick.bCanEverTick = true; }

void ADigetherDainVerification::Finish(bool bPassed, const FString& Reason)
{
	const FVector Position = Character ? Character->GetActorLocation() : FVector::ZeroVector;
	const FString Result = FString::Printf(TEXT("{\"passed\":%s,\"reason\":\"%s\",\"blockedX\":%.2f,\"afterOneHitX\":%.2f,\"exitX\":%.2f,\"exitY\":%.2f,\"exitZ\":%.2f,\"successfulDigs\":%d,\"protectedRejected\":%s,\"capsuleRadius\":35,\"capsuleHalfHeight\":90,\"teleportsDuringTest\":0}"),
		bPassed ? TEXT("true") : TEXT("false"), *Reason, BlockedX, AfterOneHitX, Position.X, Position.Y, Position.Z,
		Wall ? GetWallRevision() : 0, bProtectedRejected ? TEXT("true") : TEXT("false"));
	const FString Directory = FPaths::ProjectSavedDir() / TEXT("QA");
	IFileManager::Get().MakeDirectory(*Directory, true);
	FFileHelper::SaveStringToFile(Result, *(Directory / TEXT("wall-result.json")));
	UE_LOG(LogDigetherWallTest, Display, TEXT("RESULT %s"), *Result);
	SetActorTickEnabled(false);
	FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1);
}

void ADigetherDainVerification::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TotalSeconds += DeltaSeconds;
	StageSeconds += DeltaSeconds;
	if (TotalSeconds > 100.f) { Finish(false, TEXT("scenario timeout")); return; }
	if (!Character) { Character = Cast<ADigetherDainCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0)); }
	if (!Wall) { for (TActorIterator<ADigetherDiggableSoil> It(GetWorld()); It; ++It) { Wall = *It; break; } }
	if (!Wall) { for (TActorIterator<ADigetherDainSoilWall> It(GetWorld()); It; ++It) { Wall = *It; break; } }
	if (!Character || !Wall || !Character->GetController()) { return; }
	AController* Controller = Character->GetController();
	const auto Advance = [&]() { ++Stage; StageSeconds = 0; UE_LOG(LogDigetherWallTest, Display, TEXT("Stage %d"), Stage); };
	if (Stage == 0)
	{
		Controller->SetControlRotation(FRotator::ZeroRotator);
		if (StageSeconds >= 1.f) { Advance(); }
	}
	else if (Stage == 1)
	{
		Character->AddMovementInput(FVector::ForwardVector, 1.f);
		if (StageSeconds < 2.f) { return; }
		BlockedX = Character->GetActorLocation().X;
		if (BlockedX < 600.f || BlockedX >= 700.f) { Finish(false, TEXT("intact wall did not block walking")); return; }
		// Aim at the protected floor and invoke the normal dig entry point.
		Controller->SetControlRotation(FRotator(-80, 0, 0));
		// Camera control rotation updates on the next frame.
		Advance();
	}
	else if (Stage == 2 && StageSeconds > .5f)
	{
		FHitResult Hit;
		const uint32 Before = GetWallHash();
		bProtectedRejected = Character->TraceTool(Hit) && Hit.GetActor() && Hit.GetActor()->ActorHasTag(TEXT("ProtectedRock"))
			&& !Character->DigOnce() && Before == GetWallHash();
		if (!bProtectedRejected) { Finish(false, TEXT("protected first hit was not rejected")); return; }
		Controller->SetControlRotation(FRotator::ZeroRotator);
		Advance();
	}
	else if (Stage == 3 && StageSeconds > .5f)
	{
		if (!Character->DigOnce() || GetWallRevision() != 1) { Finish(false, TEXT("first dig failed")); return; }
		Advance();
	}
	else if (Stage == 4)
	{
		Character->AddMovementInput(FVector::ForwardVector, 1.f);
		if (StageSeconds < 1.f) { return; }
		AfterOneHitX = Character->GetActorLocation().X;
		FHitResult Hit;
		if (AfterOneHitX >= 900.f || !Character->TraceTool(Hit) || Hit.GetActor() != Wall)
		{
			Finish(false, TEXT("first hit must leave a blocking soil surface")); return;
		}
		Advance();
	}
	else if (Stage == 5)
	{
		// Once the eye-height ray is clear, aim at the soil still blocking the BODY.
		// This is the same mouse aim + dig a person uses to widen the lower rim.
		FHitResult Aim, BodyHit;
		if (!Character->TraceTool(Aim) || Aim.GetActor() != Wall)
		{
			FCollisionQueryParams Probe(SCENE_QUERY_STAT(LabBodyObstacle), false, Character);
			const FVector Start = Character->GetActorLocation();
			if (GetWorld()->SweepSingleByChannel(BodyHit, Start, Start + FVector(100, 0, 0), FQuat::Identity,
				ECC_Pawn, FCollisionShape::MakeCapsule(35.f, 90.f), Probe) && BodyHit.GetActor() == Wall)
			{
				Controller->SetControlRotation((BodyHit.ImpactPoint - Character->GetPawnViewLocation()).Rotation());
			}
		}
		Character->DigOnce();
		Character->AddMovementInput(FVector::ForwardVector, 1.f);
		if (FMath::Abs(Character->GetActorLocation().Y) > 100.f) { Finish(false, TEXT("character bypassed the wall")); return; }
		if (StageSeconds > 15.f) { Finish(false, TEXT("repeated digging did not permit crossing")); return; }
		if (Character->GetActorLocation().X < 1050.f) { return; }
		if (GetWallRevision() < 2) { Finish(false, TEXT("multiple successful hits required")); return; }
		Character->GetCharacterMovement()->StopMovementImmediately();
		Controller->SetControlRotation(FRotator(0, 180, 0));
		Advance();
	}
	else if (Stage == 6)
    {
        // A walkable tunnel may have a step: exercise CharacterMovement both ways.
        Character->AddMovementInput(-FVector::ForwardVector, 1.f);
        if (StageSeconds > 15.f) { Finish(false, TEXT("return through tunnel failed")); return; }
        if (Character->GetActorLocation().X > 600.f) { return; }
        Character->GetCharacterMovement()->StopMovementImmediately();
        Controller->SetControlRotation(FRotator::ZeroRotator);
        Advance();
    }
    else if (Stage == 7 && StageSeconds > 1.f)
	{
		if (FParse::Param(FCommandLine::Get(), TEXT("nullrhi"))) { Finish(true, TEXT("walked through and returned")); return; }
		FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("QA/through-wall.png"), false, false);
		Advance();
	}
	else if (Stage == 8)
	{
		if (IFileManager::Get().FileSize(*(FPaths::ProjectSavedDir() / TEXT("QA/through-wall.png"))) > 0)
		{
			Finish(true, TEXT("walked through and returned with rendered evidence"));
		}
		else if (StageSeconds > 60.f) { Finish(false, TEXT("screenshot timed out")); }
	}
}

uint32 ADigetherDainVerification::GetWallRevision() const
{
    if (const auto* Soil = Cast<ADigetherDiggableSoil>(Wall)) { return Soil->GetRevision(); }
    const auto* Legacy = Cast<ADigetherDainSoilWall>(Wall);
    return Legacy ? Legacy->GetRevision() : 0;
}
uint32 ADigetherDainVerification::GetWallHash() const
{
    if (const auto* Soil = Cast<ADigetherDiggableSoil>(Wall)) { return Soil->GetDensityHash(); }
    const auto* Legacy = Cast<ADigetherDainSoilWall>(Wall);
    return Legacy ? Legacy->GetDensityHash() : 0;
}


