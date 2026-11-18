#include "DigetherExcavationLabCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"

void ADigetherExcavationLabCharacter::FinishBurialRescue()
{
	if (!bIsBuriedForRescue) return;
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(LabRescueExit), false, this);
	const auto Channel = Capsule->GetCollisionObjectType();
	const FCollisionResponseParams Responses(Capsule->GetCollisionResponseToChannels());
	const double Radius = Capsule->GetScaledCapsuleRadius();
	const double Standing = Capsule->GetScaledCapsuleHalfHeight();
	const double Crouched = FMath::Max(Radius, double(Movement->GetCrouchedHalfHeight() * Capsule->GetShapeScale()));

	auto FindExit = [&](double HalfHeight, FVector& OutLocation)
	{
		const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight);
		for (int32 Ring = 0; Ring <= 10; ++Ring)
		{
			const int32 Directions = FMath::Max(1, Ring * 6);
			for (int32 I = 0; I < Directions; ++I)
			{
				const double Angle = UE_TWO_PI * I / Directions;
				const FVector Foot = BurialFeet + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0) * (30. * Ring);
				FHitResult Floor;
				for (const double Height : {45., 155., 265.})
				{
					if (!GetWorld()->LineTraceSingleByChannel(Floor, Foot + FVector::UpVector * Height,
						Foot - FVector::UpVector * 85., Channel, Params, Responses) || !Movement->IsWalkable(Floor)) continue;
					const FVector Center = Floor.ImpactPoint + FVector::UpVector * (HalfHeight + 2.5);
					if (FVector::DistSquared(BurialFeet, Floor.ImpactPoint) > FMath::Square(300.) ||
						GetWorld()->OverlapBlockingTestByChannel(Center, FQuat::Identity, Channel, Shape, Params, Responses)) continue;
					// 몸 위에 열린 공간에서 목적지까지 막힘이 있으면 벽 너머로 옮기지 않는다.
					if (GetWorld()->SweepTestByChannel(BurialFeet + FVector::UpVector * 40., Center,
						FQuat::Identity, Channel, FCollisionShape::MakeSphere(8.f), Params, Responses)) continue;
					OutLocation = Center;
					return true;
				}
			}
		}
		return false;
	};

	FVector Exit = GetActorLocation();
	const bool bStandingExit = FindExit(Standing, Exit);
	const bool bCrouchedExit = !bStandingExit && FindExit(Crouched, Exit);
	bIsBuriedForRescue = false;
	LastReceivedBurialStage = 0;
	BurialSamples.Reset();
	BurialSite.Reset();
	BurialSoil.Reset();
	GetMesh()->SetSkeletalMeshAsset(SavedBurialMeshAsset);
	GetMesh()->SetRelativeTransform(SavedBurialMeshTransform);
	GetMesh()->SetVisibility(bSavedBurialMeshVisible);
	GetMesh()->SetOwnerNoSee(bSavedBurialOwnerNoSee);
	SavedBurialMeshAsset = nullptr;
	Camera->SetRelativeLocation(SavedBurialCameraLocation);
	bUseControllerRotationYaw = bSavedBurialYawControl;
	Movement->SetMovementMode(MOVE_Walking);
	if (bCrouchedExit)
	{
		Movement->GetNavAgentPropertiesRef().bCanCrouch = true;
		Crouch();
		Movement->Crouch(false);
	}
	if (bStandingExit || bCrouchedExit)
	{
		SetActorLocation(Exit, false, nullptr, ETeleportType::TeleportPhysics);
	}
}
