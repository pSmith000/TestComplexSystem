// Copyright Epic Games, Inc. All Rights Reserved.

#include "TestComplexSystemCharacter.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "DrawDebugHelpers.h"
#include "Kismet/KismetMathLibrary.h"
#include <Kismet/KismetSystemLibrary.h>

//////////////////////////////////////////////////////////////////////////
// ATestComplexSystemCharacter

ATestComplexSystemCharacter::ATestComplexSystemCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f); // ...at this rotation rate
	GetCharacterMovement()->JumpZVelocity = 600.f;
	GetCharacterMovement()->AirControl = 0.2f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 300.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named MyCharacter (to avoid direct content references in C++)
}

//////////////////////////////////////////////////////////////////////////
// Input

void ATestComplexSystemCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up gameplay key bindings
	check(PlayerInputComponent);
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	PlayerInputComponent->BindAction("Crouch", IE_Pressed, this, &ATestComplexSystemCharacter::StartCrouch);
	PlayerInputComponent->BindAction("Crouch", IE_Released, this, &ATestComplexSystemCharacter::StopCrouch);

	PlayerInputComponent->BindAxis("MoveForward", this, &ATestComplexSystemCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &ATestComplexSystemCharacter::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &ATestComplexSystemCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &ATestComplexSystemCharacter::LookUpAtRate);

	// handle touch devices
	PlayerInputComponent->BindTouch(IE_Pressed, this, &ATestComplexSystemCharacter::TouchStarted);
	PlayerInputComponent->BindTouch(IE_Released, this, &ATestComplexSystemCharacter::TouchStopped);

	// VR headset functionality
	PlayerInputComponent->BindAction("ResetVR", IE_Pressed, this, &ATestComplexSystemCharacter::OnResetVR);
}

void ATestComplexSystemCharacter::SetMoveSpeed(float speed)
{
	GetCharacterMovement()->MaxWalkSpeed = speed;
}

void ATestComplexSystemCharacter::StartCrouch()
{
	if (isSliding || GetCharacterMovement()->IsFalling())
		return;

	/*if (GetCharacterMovement()->Velocity.Size() >= 400.0f && !GetCharacterMovement()->IsFalling())
		StartSlide();
	else
		Crouch();*/

	Crouch();
}

void ATestComplexSystemCharacter::StopCrouch()
{
	UnCrouch();
}

void ATestComplexSystemCharacter::StartSlide()
{
	if (isSliding)
		return;
	isSliding = true;
	GetCapsuleComponent()->SetCapsuleHalfHeight(48.0f);
	FVector meshLocation = GetMesh()->GetComponentTransform().GetLocation();
	meshLocation.Z += 50.0f;

	GetMesh()->SetWorldLocation(meshLocation);

}

void ATestComplexSystemCharacter::StopSlide()
{
	isSliding = false;
	GetCapsuleComponent()->SetCapsuleHalfHeight(96.0f);
	FVector meshLocation = GetMesh()->GetComponentTransform().GetLocation();
	meshLocation.Z -= 50.0f;

	GetMesh()->SetWorldLocation(meshLocation);
}

bool ATestComplexSystemCharacter::CheckForClimbing()
{
	FHitResult out;
	FCollisionQueryParams TraceParams;
	TraceParams.AddIgnoredActor(this);

	FVector actorLocation = GetActorLocation(); 
	FVector actorForward = GetActorForwardVector();

	actorLocation.Z -= 44.0f; 
	actorForward = actorForward * 70.0f;

	FVector startLocation = actorLocation; 
	FVector endLocation = actorLocation + actorForward; 

	bool hasHit = GetWorld()->LineTraceSingleByChannel(out, startLocation, endLocation, ECC_Visibility, TraceParams);

	if (!hasHit)
		return false;

	//GEngine->AddOnScreenDebugMessage(1, 5.0f, FColor::Blue, out.GetActor()->GetName());

	_wallLocation = out.Location;
	_wallNormal = out.Normal;

	DrawDebugLine(GetWorld(), startLocation, endLocation, FColor::Red, false, 2.0f);

	FRotator rotator = UKismetMathLibrary::MakeRotFromX(_wallNormal);
	FVector wallForward = UKismetMathLibrary::GetForwardVector(rotator);

	wallForward *= -10.0f;
	startLocation = (wallForward + _wallLocation);
	startLocation.Z += 200.0f;
	endLocation = startLocation;
	endLocation.Z -= 200.0f;
	
	hasHit = GetWorld()->LineTraceSingleByChannel(out, startLocation, endLocation, ECC_Visibility, TraceParams);
	
	if (!hasHit)
		return false;

	//GEngine->AddOnScreenDebugMessage(2, 5.0f, FColor::Blue, out.GetActor()->GetName());
	
	DrawDebugLine(GetWorld(), startLocation, endLocation, FColor::Red, false, 2.0f);

	_wallHeight = out.Location;

	_shouldPlayerClimb = _wallHeight.Z - _wallLocation.Z > 60.0f;

	rotator = UKismetMathLibrary::MakeRotFromX(_wallNormal);
	wallForward = UKismetMathLibrary::GetForwardVector(rotator);

	wallForward *= -50.0f;
	startLocation = (wallForward + _wallLocation);
	startLocation.Z += 250.0f;
	endLocation = startLocation;
	endLocation.Z -= 300.0f;

	hasHit = GetWorld()->LineTraceSingleByChannel(out, startLocation, endLocation, ECC_Visibility, TraceParams);

	if (!hasHit)
		_isWallThick = false;

	//GEngine->AddOnScreenDebugMessage(2, 5.0f, FColor::Blue, out.GetActor()->GetName());

	DrawDebugLine(GetWorld(), startLocation, endLocation, FColor::Red, false, 2.0f);

	_otherWallHeight = out.Location;

	_isWallThick = !(_wallHeight.Z - _otherWallHeight.Z > 30.0f);

	return true;
}

void ATestComplexSystemCharacter::StartVaultOrGetUp()
{
	isClimbing = true;

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Flying);

	FRotator rotator = UKismetMathLibrary::MakeRotFromX(_wallNormal);
	FVector wallForward = UKismetMathLibrary::GetForwardVector(rotator);
	wallForward *= -50.0f;
	FVector actorNewLocation = wallForward + GetActorLocation();
	actorNewLocation.Z += 50.0f;

	

	if (_isWallThick)
		SetActorLocation(actorNewLocation);
	else 
	{
		SetActorLocation(actorNewLocation);
		//UKismetMathLibrary::VInterpTo_Constant(GetActorLocation(), actorNewLocation, GetWorld()->GetDeltaSeconds(), 1.0f);
	}
}

void ATestComplexSystemCharacter::StopVaultOrGetUp()
{
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Walking);

	isClimbing = false;
}


void ATestComplexSystemCharacter::OnResetVR()
{
	// If TestComplexSystem is added to a project via 'Add Feature' in the Unreal Editor the dependency on HeadMountedDisplay in TestComplexSystem.Build.cs is not automatically propagated
	// and a linker error will result.
	// You will need to either:
	//		Add "HeadMountedDisplay" to [YourProject].Build.cs PublicDependencyModuleNames in order to build successfully (appropriate if supporting VR).
	// or:
	//		Comment or delete the call to ResetOrientationAndPosition below (appropriate if not supporting VR)
	UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition();
}

void ATestComplexSystemCharacter::TouchStarted(ETouchIndex::Type FingerIndex, FVector Location)
{
		Jump();
}

void ATestComplexSystemCharacter::TouchStopped(ETouchIndex::Type FingerIndex, FVector Location)
{
		StopJumping();
}

void ATestComplexSystemCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void ATestComplexSystemCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

void ATestComplexSystemCharacter::MoveForward(float Value)
{
	if ((Controller != nullptr) && (Value != 0.0f))
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		AddMovementInput(Direction, Value);
	}
}

void ATestComplexSystemCharacter::MoveRight(float Value)
{
	if ( (Controller != nullptr) && (Value != 0.0f) )
	{
		// find out which way is right
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
	
		// get right vector 
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		// add movement in that direction
		AddMovementInput(Direction, Value);
	}
}
