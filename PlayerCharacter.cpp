// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerCharacter.h"

#include <string>


#include "DrawDebugHelpers.h"
#include "RachWeapon.h"
#include "Camera/CameraComponent.h"
#include "Chaos/Utilities.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"

// Sets default values
APlayerCharacter::APlayerCharacter()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	InvertCamera = -1.0f;
	CameraFOV = 90.0f;
	RunningFOV = 100.0f;
	FOVTransitionTime = 1.0f;
	CrouchTransitionTime = 1.0f;
	CrouchInterpolationTime = 1.0f;
	DefaultStrafeMultiplier = 0.85f;
	RunStrafeMultiplier = 0.6f;
	DefaultMoveBackMultiplier = 0.7f;
	RunMoveBackMultiplier = 0.6f;
	DefaultSpeed = 400;
	RunSpeed = 800;
	CrouchSpeed = 300;
	bWantsToRun = false;
	bWantsToCrouch = false;
	MaxCapsuleHalfHeight = 100.0f;
	MinCapsuleHalfHeight = 10.0f;
	StandingCapsuleHalfHeight = 88.0f;
	CrouchingCapsuleHalfHeight = 40.0f;
	DefaultBrakingDeceleration = 256.0f;
	DefaultGroundFriction = 4.0f;
	SlidingBrakingDeceleration = 128.0f;
	SlidingGroundFriction = 0.5f;
	SlideForceMultiplier = 800000.0f;
	MinSlideSpeed = 10.0f;
	MaxSlideSpeed = 800.0f;
	MaxSlideTime = 5.0f;
	JumpZVelocity = 300.0f;
	MaxMovementAcceleration = 1024.0f;
	MoveState = EMovementState::MS_Standing;

	CapsuleComp = Cast<UCapsuleComponent>(GetCapsuleComponent());
	MovementComp = Cast<UCharacterMovementComponent>(GetCharacterMovement());
	MovementComp->GetNavAgentPropertiesRef().bCanCrouch = true;
	MovementComp->JumpZVelocity = JumpZVelocity;
	MovementComp->MaxAcceleration = MaxMovementAcceleration;
	CameraComp = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComp"));
	CameraComp->bUsePawnControlRotation = true;
	CameraComp->SetFieldOfView(CameraFOV);
	GetMesh()->SetupAttachment(CameraComp);
}

// Called when the game starts or when spawned
void APlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	MovementComp->MaxWalkSpeed = DefaultSpeed;
	MovementComp->BrakingDecelerationWalking = DefaultBrakingDeceleration;
	MovementComp->GroundFriction = DefaultGroundFriction;
	CapsuleComp->SetCapsuleHalfHeight(MaxCapsuleHalfHeight);
	FVector CameraLocation = CameraComp->GetRelativeLocation();
	CameraLocation.Z = BaseEyeHeight;
	TargetFOV = CameraComp->FieldOfView;
	CrouchingTargetCapsuleHalfHeight = CrouchingCapsuleHalfHeight;
	CameraComp->SetRelativeLocation(CameraLocation);

	FOnTimelineFloat ProgressFunctionCrouchEyeHeight;
	ProgressFunctionCrouchEyeHeight.BindUFunction(this, FName("HandleProgressCrouchEyeHeight"));
	TimelineCrouchEyeHeight.AddInterpFloat(CurveFloatCrouchEyeHeight, ProgressFunctionCrouchEyeHeight);
	FOnTimelineFloat ProgressFunctionCrouchHalfHeight;
	ProgressFunctionCrouchHalfHeight.BindUFunction(this, FName("HandleProgressCrouchHalfHeight"));
	TimelineCrouchHalfHeight.AddInterpFloat(CurveFloatCrouchHalfHeight, ProgressFunctionCrouchHalfHeight);
}

void APlayerCharacter::ChangeMovementState(EMovementState TargetState)
{
	MoveState = TargetState;
}

void APlayerCharacter::UpdateMovementState()
{
	FVector Velocity = MovementComp->Velocity;
	switch (MoveState)
	{
		case EMovementState::MS_Sliding:
			MovementComp->AddForce(GetFloorSurfaceVector());
			
			if (Velocity.Size() < MinSlideSpeed) EndSlide();
			if (Velocity.Size() > MaxSlideSpeed)
			{
				Velocity.Normalize();
				MovementComp->Velocity = Velocity * RunSpeed;
			}
			return;

		case EMovementState::MS_Running:
			if (bWantsToCrouch)
			{
				if (!MovementComp->IsFalling()) BeginSlide();
				else
				{
					EndRun();
					BeginCrouch();
				}
			}
		
			else if (!bWantsToRun)
			{
				EndRun();
			}
			return;

		case EMovementState::MS_Crouching:
			if (!CanUncrouch()) return;
			
			if (bWantsToRun)
			{
				if (MovementComp->IsMovingOnGround())
				{
					EndCrouch();
					BeginRun();
				}
			}
		
			else if (!bWantsToCrouch)
			{
				EndCrouch();
			}
			return;

		case EMovementState::MS_Standing:
			if (bWantsToCrouch && bWantsToRun)
			{
				EndCrouch();
				BeginRun();
            }
		
			else if (bWantsToCrouch)
			{
				BeginCrouch();
			}
		
			else if (bWantsToRun)
			{
				BeginRun();
			}
			return;
	}
}

bool APlayerCharacter::CanUncrouch()
{
	if (MoveState == EMovementState::MS_Crouching)
	{
		float CurrentHalfHeight = CapsuleComp->GetUnscaledCapsuleHalfHeight();
		float HeightDifference = MaxCapsuleHalfHeight - CurrentHalfHeight;
		HeightDifference *= 2.0f;
		HeightDifference += 10.0f;
		
		FVector Pos = GetActorLocation();
		Pos.Z += CurrentHalfHeight;
		FVector EndPos = Pos;
		EndPos.Z += HeightDifference - 20.0f;

		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(this);
		QueryParams.bTraceComplex = true;
		
		FHitResult Hit;
		GetWorld()->SweepSingleByChannel(Hit, Pos, EndPos, FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeSphere(20.0f), QueryParams );
		if (Hit.bBlockingHit)
		{
			return false;
		}
	}
	return true;
}

void APlayerCharacter::HandleProgressCrouchEyeHeight(float Value)
{
	FVector Pos = CameraComp->GetRelativeLocation();
	Pos.Z = FMath::Lerp(BaseEyeHeight, CrouchedEyeHeight, Value);
	CameraComp->SetRelativeLocation(Pos);
}

void APlayerCharacter::HandleProgressCrouchHalfHeight(float Value)
{
	CapsuleComp->SetCapsuleHalfHeight(FMath::Lerp(StandingCapsuleHalfHeight, CrouchingCapsuleHalfHeight, Value));
}

FVector APlayerCharacter::GetFloorSurfaceVector() const
{
	FFindFloorResult FloorResult;
	MovementComp->FindFloor(GetActorLocation(), FloorResult, true, nullptr);
	if (FloorResult.bBlockingHit)
	{
		const FVector SurfaceNormal = FloorResult.HitResult.Normal;
		if (SurfaceNormal == FVector::UpVector)
		{
			return FVector::ZeroVector;
		}

		FVector ReturnVector = ReturnVector.CrossProduct(FVector::UpVector,SurfaceNormal);
		ReturnVector = ReturnVector.CrossProduct(ReturnVector, SurfaceNormal);
		ReturnVector.Normalize();
		float ReturnScalar = 1.0f;
		ReturnScalar -= FVector::ZeroVector.DotProduct(SurfaceNormal, FVector::UpVector);
		ReturnScalar = FMath::Clamp(ReturnScalar, 0.0f, 1.0f);
		ReturnScalar *= SlideForceMultiplier;
		return ReturnVector * ReturnScalar;
	}
	return FVector::ZeroVector;
}

void APlayerCharacter::AnticipateAndResizeCrouch()
{
	FVector IntendedStartPos = GetActorLocation();
	IntendedStartPos.Z -= CapsuleComp->GetUnscaledCapsuleHalfHeight();
	IntendedStartPos.Z += CrouchingCapsuleHalfHeight;
	FVector CurrentStartPos = IntendedStartPos;
	float OffsetAnticipation = 50.0f;
	FVector Offset = FVector::ZeroVector;
	if (MoveState == EMovementState::MS_Crouching)
	{
		OffsetAnticipation = 50.0f;
		Offset = MovementComp->GetLastInputVector();
	}
	else if (MoveState == EMovementState::MS_Sliding)
	{
		OffsetAnticipation = 100.0f;
		Offset = MovementComp->Velocity;
	}
	
	Offset.Normalize();
	Offset.Z = 0.0f;
	Offset *= CapsuleComp->GetUnscaledCapsuleRadius() + OffsetAnticipation;
	IntendedStartPos += Offset;

	FVector IntendedEndPos = IntendedStartPos;
	FVector CurrentEndPos = CurrentStartPos;
	IntendedEndPos.Z += CrouchingCapsuleHalfHeight;
	CurrentEndPos.Z = IntendedEndPos.Z;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	QueryParams.bTraceComplex = true;

	//DrawDebugLine(GetWorld(), IntendedStartPos, IntendedEndPos, FColor::Emerald, false, 1.0f);
	//DrawDebugLine(GetWorld(), CurrentStartPos, CurrentEndPos, FColor::Red, false, 1.0f, 0.0f, 1.0f);
	FHitResult IntendedHit;
	//GetWorld()->LineTraceSingleByChannel(IntendedHit, IntendedStartPos, IntendedEndPos, ECC_WorldStatic, QueryParams);
	GetWorld()->SweepSingleByChannel(IntendedHit, IntendedStartPos, IntendedEndPos, FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeSphere(1.0f), QueryParams );
	FHitResult CurrentHit;
	GetWorld()->SweepSingleByChannel(CurrentHit, CurrentStartPos, CurrentEndPos, FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeSphere(15.0f), QueryParams );
	
	float NewHalfHeight = 0.0f;
	bool bIntendedOKAngle = false;
	bool bCurrentOKAngle = false;

	if (IntendedHit.bBlockingHit)
	{
		float Angle = FVector::DotProduct(FVector::DownVector, IntendedHit.ImpactNormal);
		Angle = UKismetMathLibrary::DegAcos(Angle);
		if (Angle < 30.0f) bIntendedOKAngle = true;
	}
	if (CurrentHit.bBlockingHit)
	{
		float Angle = FVector::DotProduct(CurrentHit.ImpactNormal, FVector::DownVector);
		Angle = UKismetMathLibrary::DegAcos(Angle);
		if (Angle < 30.0f) bCurrentOKAngle = true;
	}
	
	if (bIntendedOKAngle && bCurrentOKAngle)
	{
		if (IntendedHit.ImpactPoint.Z < CurrentHit.ImpactPoint.Z)
		{
			NewHalfHeight = IntendedHit.ImpactPoint.Z;
		}
		else
		{
			NewHalfHeight = CurrentHit.ImpactPoint.Z;
		}
		//DrawDebugSphere(GetWorld(), IntendedHit.ImpactPoint, 3.0f, 16, FColor::Red, false, 1.0f);
		//DrawDebugSphere(GetWorld(), CurrentHit.ImpactPoint, 3.0f, 16, FColor::Blue, false, 1.0f);
	}
	else
	{
		if (bIntendedOKAngle)
		{
			NewHalfHeight = IntendedHit.ImpactPoint.Z;
			//DrawDebugSphere(GetWorld(), IntendedHit.ImpactPoint, 3.0f, 16, FColor::Red, false, 1.0f);
		}
		else if (bCurrentOKAngle)
		{
			NewHalfHeight = CurrentHit.ImpactPoint.Z;
			//DrawDebugSphere(GetWorld(), CurrentHit.ImpactPoint, 3.0f, 16, FColor::Blue, false, 1.0f);
		}
		else
		{
		CrouchingTargetCapsuleHalfHeight = CrouchingCapsuleHalfHeight;
		return;
		}
	}
	float FeetPosition = GetActorLocation().Z;
	FeetPosition -= CapsuleComp->GetUnscaledCapsuleHalfHeight();
	NewHalfHeight -=  FeetPosition;
	NewHalfHeight -= 5.0f;
	NewHalfHeight /= 2.0f;
	NewHalfHeight = FMath::CeilToFloat(NewHalfHeight);
	if (NewHalfHeight < MinCapsuleHalfHeight) return;
	CrouchingTargetCapsuleHalfHeight = NewHalfHeight;
}

void APlayerCharacter::ToggleCrouchHeight_DEBUG()
{
	MinCapsuleHalfHeight = MinCapsuleHalfHeight == 40.0f ? 1.0f : 40.0f;
}

// Called every frame
void APlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateMovementState();

	CameraComp->SetFieldOfView(FMath::FInterpTo(CameraComp->FieldOfView, TargetFOV, DeltaTime, FOVTransitionTime));
	if (MoveState == EMovementState::MS_Crouching || MoveState == EMovementState::MS_Sliding)
	{
		AnticipateAndResizeCrouch();
		CapsuleComp->SetCapsuleHalfHeight(FMath::FInterpConstantTo(CapsuleComp->GetUnscaledCapsuleHalfHeight(), CrouchingTargetCapsuleHalfHeight, DeltaTime, CrouchInterpolationTime));
	}
	TimelineCrouchEyeHeight.TickTimeline(DeltaTime);
	TimelineCrouchHalfHeight.TickTimeline(DeltaTime);
}

// Called to bind functionality to input
void APlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis("MoveForward", this, &APlayerCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &APlayerCharacter::MoveRight);

	PlayerInputComponent->BindAxis("LookUp", this, &APlayerCharacter::LookUp);
	PlayerInputComponent->BindAxis("Turn", this, &APlayerCharacter::Turn);

	PlayerInputComponent->BindAction("Crouch", IE_Pressed, this, &APlayerCharacter::PressedCrouch);
	PlayerInputComponent->BindAction("Crouch", IE_Released, this, &APlayerCharacter::ReleasedCrouch);
	PlayerInputComponent->BindAction("ToggleCrouch", IE_Pressed, this, &APlayerCharacter::ToggleCrouch);

	PlayerInputComponent->BindAction("Run", IE_Pressed, this, &APlayerCharacter::PressedRun);
	PlayerInputComponent->BindAction("Run", IE_Released, this, &APlayerCharacter::ReleasedRun);

	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);

	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &APlayerCharacter::Fire);

	PlayerInputComponent->BindAction("ToggleCrouchHeight_DEBUG", IE_Pressed, this, &APlayerCharacter::ToggleCrouchHeight_DEBUG);
}

void APlayerCharacter::LookUp(float Val)
{
	AddControllerPitchInput(Val * InvertCamera);
}

void APlayerCharacter::Turn(float Val)
{
	AddControllerYawInput(Val);
}

void APlayerCharacter::MoveForward(float Val)
{
	if (MoveState != EMovementState::MS_Sliding)
	{
		float BackMult = DefaultMoveBackMultiplier;
		if (MoveState == EMovementState::MS_Running) BackMult = RunMoveBackMultiplier;
		if (Val > 0) AddMovementInput(GetActorForwardVector() * Val);
		else AddMovementInput(GetActorForwardVector() * Val, BackMult);
	}
}

void APlayerCharacter::MoveRight(float Val)
{
	if (MoveState != EMovementState::MS_Sliding)
	{
		float StrafeMult = DefaultStrafeMultiplier;
		if (MoveState == EMovementState::MS_Running) StrafeMult = RunStrafeMultiplier;
		AddMovementInput(GetActorRightVector() * Val, StrafeMult);
	}
}

void APlayerCharacter::ToggleCrouch()
{
	bWantsToCrouch = !bWantsToCrouch;
}

void APlayerCharacter::PressedCrouch()
{
	bWantsToCrouch = true;
}

void APlayerCharacter::ReleasedCrouch()
{
	bWantsToCrouch = false;
}

void APlayerCharacter::BeginCrouch()
{
	MovementComp->MaxWalkSpeed = CrouchSpeed;
	TimelineCrouchEyeHeight.Play();
	TimelineCrouchHalfHeight.Play();
	ChangeMovementState(EMovementState::MS_Crouching);
}

void APlayerCharacter::EndCrouch()
{
	MovementComp->MaxWalkSpeed = DefaultSpeed;
	TimelineCrouchEyeHeight.Reverse();
	TimelineCrouchHalfHeight.Reverse();
	ChangeMovementState(EMovementState::MS_Standing);
}

void APlayerCharacter::PressedRun()
{
	bWantsToRun = true;
}

void APlayerCharacter::ReleasedRun()
{
	bWantsToRun = false;
}

void APlayerCharacter::BeginRun()
{
	MovementComp->MaxWalkSpeed = RunSpeed;
	ChangeMovementState(EMovementState::MS_Running);
	bWantsToCrouch = false;
	TargetFOV = RunningFOV;
}

void APlayerCharacter::EndRun()
{
	MovementComp->MaxWalkSpeed = DefaultSpeed;
	ChangeMovementState(EMovementState::MS_Standing);
	TargetFOV = CameraFOV;
}

void APlayerCharacter::BeginSlide()
{
	FVector Velocity = GetVelocity();
	ChangeMovementState(EMovementState::MS_Sliding);
	MovementComp->Velocity = Velocity;
	MovementComp->BrakingDecelerationWalking = SlidingBrakingDeceleration;
	MovementComp->GroundFriction = SlidingGroundFriction;
	GetWorldTimerManager().SetTimer(TimerHandle_SlideTime,this, &APlayerCharacter::EndSlide, MaxSlideTime);
	TimelineCrouchEyeHeight.Play();
	TimelineCrouchHalfHeight.Play();
}

void APlayerCharacter::EndSlide()
{
	GetWorldTimerManager().ClearTimer(TimerHandle_SlideTime);
	MovementComp->BrakingDecelerationWalking = DefaultBrakingDeceleration;
	MovementComp->GroundFriction = DefaultGroundFriction;
	BeginCrouch();
}

void APlayerCharacter::Fire()
{
	return;
}
