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
#include "Kismet/GameplayStatics.h"

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

/// <summary>
/// Update for the character
/// </summary>
/// <param name="deltaTime"></param>
void ATestComplexSystemCharacter::Tick(float deltaTime)
{
	//DEBUG
	FString deltaTimeString;
	float ForwardVelocity = FVector::DotProduct(GetVelocity(), GetActorForwardVector());
	if (ForwardVelocity <= 100.0f && _isWallRunning)
		deltaTimeString = "True";
	else
		deltaTimeString = "RFalse";
	GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, deltaTimeString);
	//DEBUG

	//Sets the current height of the player for wall running
	_currentFrameHeight = GetActorLocation().Z;

	//If the character is falling, check for wallrunning
	if (GetCharacterMovement()->IsFalling())
	{
		CheckForWallRunning();
	}
	//Else...
	else
	{
		//...set wallrunning, inaction, rightside and leftside to be false
		//to turn off wall running
		_isWallRunning = false;
		inAction = false;
		_rightSide = false;
		_leftSide = false;
		//Set the gravity scale and plane constraint back to normal
		GetCharacterMovement()->GravityScale = 1.0f;
		GetCharacterMovement()->SetPlaneConstraintNormal(FVector(0.0f, 0.0f, 0.0f));
	}
	
	//If the forward velocity is less than 100 and the player is still wallrunning...
	if (ForwardVelocity <= 100.0f && _isWallRunning)
	{
		//...turn off wallrunning by setting wallrunning, inaction, rightside and leftside to be false
		_isWallRunning = false;
		inAction = false;
		_rightSide = false;
		_leftSide = false;
		//Set the gravity scale and plane constraint back to normal
		GetCharacterMovement()->GravityScale = 50.0f;
		GetCharacterMovement()->SetPlaneConstraintNormal(FVector(0.0f, 0.0f, 0.0f));

		//GetWorldTimerManager().SetTimer(timerHandle, this, &ATestComplexSystemCharacter::TurnOffJumpOffWall, 1.5f, false);
	}
	
	//Set the last frame height to be the current frame height
	_lastFrameHeight = _currentFrameHeight;
	
}

//////////////////////////////////////////////////////////////////////////
// Input

void ATestComplexSystemCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up gameplay key bindings
	check(PlayerInputComponent);
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ATestComplexSystemCharacter::CheckJump);
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

/// <summary>
/// Sets the players move speed
/// </summary>
/// <param name="speed">the speed to set the player to</param>
void ATestComplexSystemCharacter::SetMoveSpeed(float speed)
{
	GetCharacterMovement()->MaxWalkSpeed = speed;
}

/// <summary>
/// Start crouching functionality
/// </summary>
void ATestComplexSystemCharacter::StartCrouch()
{
	//if already sliding or already falling, return
	if (isSliding || GetCharacterMovement()->IsFalling())
		return;

	/*if (GetCharacterMovement()->Velocity.Size() >= 400.0f && !GetCharacterMovement()->IsFalling())
		StartSlide();
	else
		Crouch();*/

	//Crouch and set iscrouching to true
	Crouch();
	isCrouching = true;
}

/// <summary>
/// Stop crouching functionality
/// </summary>
void ATestComplexSystemCharacter::StopCrouch()
{
	//Uncrouch and set iscrouching to false
	UnCrouch();
	isCrouching = false;
}

/// <summary>
/// Start sliding functionality
/// </summary>
void ATestComplexSystemCharacter::StartSlide()
{
	//If already in action or sliding, return
	if (inAction || isSliding)
		return;
	//Set in action and is sliding to be true
	inAction = true;
	isSliding = true;

	//Set the half heigh and get the new mesh location
	GetCapsuleComponent()->SetCapsuleHalfHeight(48.0f);
	FVector meshLocation = GetMesh()->GetComponentTransform().GetLocation();
	meshLocation.Z += 50.0f;

	//Set the new mesh location
	GetMesh()->SetWorldLocation(meshLocation);
}

/// <summary>
/// Stop sliding functionality
/// </summary>
void ATestComplexSystemCharacter::StopSlide()
{
	//Set in action and is sliding to be false
	inAction = false;
	isSliding = false;

	//Set the half heigh and get the new mesh location
	GetCapsuleComponent()->SetCapsuleHalfHeight(96.0f);
	FVector meshLocation = GetMesh()->GetComponentTransform().GetLocation();
	meshLocation.Z -= 50.0f;

	//Set the new mesh location
	GetMesh()->SetWorldLocation(meshLocation);
}

/// <summary>
/// Checks if the player can climb the object it is facing
/// </summary>
/// <returns>true if the player can climb</returns>
bool ATestComplexSystemCharacter::CheckForClimbing()
{
	//Hit result and collision params for use in line tracing
	FHitResult out;
	FCollisionQueryParams TraceParams;
	//Ignores the player for line tracing
	TraceParams.AddIgnoredActor(this);

	//Get the actor location and forward
	FVector actorLocation = GetActorLocation(); 
	FVector actorForward = GetActorForwardVector();

	//Sets the actor location and forward to what it needs to be to climb
	actorLocation.Z -= 44.0f; 
	actorForward = actorForward * 70.0f;

	//Sets the start location and end location for line tracing
	FVector startLocation = actorLocation; 
	FVector endLocation = actorLocation + actorForward; 

	//Line traces to the object to climb
	bool hasHit = GetWorld()->LineTraceSingleByChannel(out, startLocation, endLocation, ECC_Visibility, TraceParams);

	//If the line trace hits nothing, return
	if (!hasHit)
		return false;

	//Gets the objects location and facing
	_wallLocation = out.Location;
	_wallNormal = out.Normal;

	DrawDebugLine(GetWorld(), startLocation, endLocation, FColor::Red, false, 2.0f);

	//Creates a rotator from the wall normal and gets the forward vector from the wall
	FRotator rotator = UKismetMathLibrary::MakeRotFromX(_wallNormal);
	FVector wallForward = UKismetMathLibrary::GetForwardVector(rotator);

	//Sets the start and end location for line tracing using the walls forward and location.
	//line traces to get the height of the wall to see if the player can vault or climb that high
	wallForward *= -10.0f;
	startLocation = (wallForward + _wallLocation);
	startLocation.Z += 200.0f;
	endLocation = startLocation;
	endLocation.Z -= 200.0f;
	
	//Line trace the wall
	hasHit = GetWorld()->LineTraceSingleByChannel(out, startLocation, endLocation, ECC_Visibility, TraceParams);
	
	//If the line trace hits nothing, return
	if (!hasHit)
		return false;
	
	DrawDebugLine(GetWorld(), startLocation, endLocation, FColor::Red, false, 2.0f);

	//Wall height is the out location of the line trace
	_wallHeight = out.Location;
	//Sets if the player should climb based off the height of the wall
	_shouldPlayerClimb = _wallHeight.Z - _wallLocation.Z > 60.0f;

	//Creates a rotator from the wall normal and gets the forward vector from the wall
	rotator = UKismetMathLibrary::MakeRotFromX(_wallNormal);
	wallForward = UKismetMathLibrary::GetForwardVector(rotator);

	//Sets the start and end location for line tracing using the walls forward and location.
	//Line traces to get the thickness of the wall
	wallForward *= -50.0f;
	startLocation = (wallForward + _wallLocation);
	startLocation.Z += 250.0f;
	endLocation = startLocation;
	endLocation.Z -= 300.0f;

	//Line trace the wall to check the thickness 
	hasHit = GetWorld()->LineTraceSingleByChannel(out, startLocation, endLocation, ECC_Visibility, TraceParams);

	//If the line trace hits nothing, the wall is not thick
	if (!hasHit)
		_isWallThick = false;

	DrawDebugLine(GetWorld(), startLocation, endLocation, FColor::Red, false, 2.0f);

	//The height of the other wall is the line traces hit location
	_otherWallHeight = out.Location;

	//Sets if the wall is too thick based off the width of the walls hit
	_isWallThick = !(_wallHeight.Z - _otherWallHeight.Z > 30.0f);

	return true;
}

/// <summary>
/// Starts vaulting functionality
/// </summary>
void ATestComplexSystemCharacter::StartVaultOrGetUp()
{
	//If already in action, return then set in action  and is climbing to be true
	if (inAction || isClimbing || isVaulting)
		return;
	inAction = true;

	//Set the player collision to be off and movement mode to be none
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Flying);

	FVector actorNewLocation;

	if (_isWallThick)
	{
		isClimbing = true;
		FRotator rotator = UKismetMathLibrary::MakeRotFromX(_wallNormal);
		FVector wallForward = UKismetMathLibrary::GetForwardVector(rotator);
		wallForward *= 50.0f;
		actorNewLocation = wallForward + GetActorLocation();
		SetActorLocation(actorNewLocation);
	}

	else
	{
		isVaulting = true;
		actorNewLocation = GetActorLocation();
		actorNewLocation.Z = _wallHeight.Z - 20.0f;
		SetActorLocation(actorNewLocation);

	}

	_startPosition = GetActorLocation();
	_endPosition = actorNewLocation;

	GetWorldTimerManager().SetTimer(timerHandle, this, &ATestComplexSystemCharacter::StopVaultOrGetUp, 1.0f, false);
}

void ATestComplexSystemCharacter::StopVaultOrGetUp()
{
	

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Walking);

	inAction = false;
	isClimbing = false;
	isVaulting = false;
}

void ATestComplexSystemCharacter::CheckForWallRunning()
{
	if (!_leftSide)
	{
		FHitResult out;
		FCollisionQueryParams TraceParams;

		FVector startLocation = GetActorLocation();
		FVector endLocation = (GetActorRightVector() * 50.0f) + startLocation;

		bool hasHit = GetWorld()->LineTraceSingleByChannel(out, startLocation, endLocation, ECC_Visibility, TraceParams);
		DrawDebugLine(GetWorld(), startLocation, endLocation, FColor::Red, false, 2.0f);

		if (hasHit && _currentFrameHeight - _lastFrameHeight <= 0.0f && !GetCharacterMovement()->IsMovingOnGround())
		{
			if (out.GetActor()->ActorHasTag("NoWallrun"))
				return;

			_rightSide = true;
			_onRightSide = true;

			if (!_isJumpingOffWall)
			{
				inAction = true;
				FRotator newRotation = UKismetMathLibrary::MakeRotFromX(out.Normal);
				newRotation.Yaw += 90.0f;
				newRotation.Roll = 0.0f;
				newRotation.Pitch = 0.0f;
				SetActorRotation(newRotation);

				FVector actorForward = GetActorForwardVector();
				actorForward.X *= 500.0f;
				actorForward.Y *= 500.0f;
				actorForward.Z = 0.0f;

				GetCharacterMovement()->GravityScale = 15.0f;
				GetCharacterMovement()->Velocity = actorForward;
				GetCharacterMovement()->SetPlaneConstraintNormal(FVector(0.0f, 0.0f, 1.0f));

				_isWallRunning = true;
			}	
		}

		else
		{
			_isWallRunning = false;
			inAction = false;
			_rightSide = false;
			GetCharacterMovement()->GravityScale = 1.0f;
			GetCharacterMovement()->SetPlaneConstraintNormal(FVector(0.0f, 0.0f, 0.0f));
		}
	}

	if (!_rightSide)
	{
		FHitResult out;
		FCollisionQueryParams TraceParams;
		TraceParams.AddIgnoredActor(this);

		FVector startLocation = GetActorLocation();
		FVector endLocation = (GetActorRightVector() * -50.0f) + startLocation;

		bool hasHit = GetWorld()->LineTraceSingleByChannel(out, startLocation, endLocation, ECC_Visibility, TraceParams);
		DrawDebugLine(GetWorld(), startLocation, endLocation, FColor::Red, false, 2.0f);

		if (hasHit && _currentFrameHeight - _lastFrameHeight <= 0.0f && !GetCharacterMovement()->IsMovingOnGround())
		{
			if (out.GetActor()->ActorHasTag("NoWallrun"))
				return;

			_leftSide = true;
			_onRightSide = false;

			if (!_isJumpingOffWall)
			{
				inAction = true;
				FRotator newRotation = UKismetMathLibrary::MakeRotFromX(out.Normal);
				newRotation.Yaw -= 90.0f;
				newRotation.Roll = 0.0f;
				newRotation.Pitch = 0.0f;
				SetActorRotation(newRotation);

				FVector actorForward = GetActorForwardVector();
				actorForward.X *= 500.0f;
				actorForward.Y *= 500.0f;
				actorForward.Z = 0.0f;

				GetCharacterMovement()->GravityScale = 15.0f;
				GetCharacterMovement()->Velocity = actorForward;
				GetCharacterMovement()->SetPlaneConstraintNormal(FVector(0.0f, 0.0f, 1.0f));
			
				_isWallRunning = true;
			}
		}

		else
		{
			_isWallRunning = false;
			inAction = false;
			_leftSide = false;
			GetCharacterMovement()->GravityScale = 1.0f;
			GetCharacterMovement()->SetPlaneConstraintNormal(FVector(0.0f, 0.0f, 0.0f));
		}
	}
}

void ATestComplexSystemCharacter::CheckJump()
{
	if ((!(_rightSide || _leftSide)) && GetCharacterMovement()->IsMovingOnGround())
		Jump();
	else if (_isWallRunning)
	{
		_isWallRunning = false;
		_isJumpingOffWall = true;

		FVector actorRightVector = GetActorRightVector();
		FVector launchVelocity = UKismetMathLibrary::SelectVector(actorRightVector * -450.0f, actorRightVector * 450.0f, _onRightSide);

		launchVelocity.Z = 450.0f;

		LaunchCharacter(launchVelocity, false, false);

		GetWorldTimerManager().SetTimer(timerHandle, this, &ATestComplexSystemCharacter::TurnOffJumpOffWall, .5f, false);
	}
}


void ATestComplexSystemCharacter::TurnOffJumpOffWall()
{
	_isJumpingOffWall = false;
	inAction = false;
	GetCharacterMovement()->GravityScale = 1.0f;
	GetCharacterMovement()->SetPlaneConstraintNormal(FVector(0.0f, 0.0f, 0.0f));
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
