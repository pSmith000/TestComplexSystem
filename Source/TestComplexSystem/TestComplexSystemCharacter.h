// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "TestComplexSystemCharacter.generated.h"

UCLASS(config=Game)
class ATestComplexSystemCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class UCameraComponent* FollowCamera;

private:
	FTimerHandle TimerHandle;
	float _delayTimer;
public:
	ATestComplexSystemCharacter();

	virtual void Tick(float deltaTime) override;

	/** Base turn rate, in deg/sec. Other scaling may affect final turn rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera)
	float BaseTurnRate;

	/** Base look up/down rate, in deg/sec. Other scaling may affect final rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera)
	float BaseLookUpRate;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Parkour)
	bool isSprinting;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Parkour)
	bool isSliding;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Parkour)
	bool isClimbing;
	bool inAction;

public:
	//Variables used for checking for climbing
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Parkour)
	bool _shouldPlayerClimb;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Parkour)
	bool _isWallRunning;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Parkour)
	bool _leftSide;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Parkour)
	bool _rightSide;
private:
	bool _isWallThick;
	bool _canClimb;
	FVector _wallLocation;
	FVector _wallNormal;
	FVector _wallHeight;
	FVector _otherWallHeight;

	FVector _startPosition;
	FVector _endPosition;
	float _totalDistance;
	float _currentDistance;
	FVector _direction;
	
	
	bool _onRightSide;
	bool _isJumpingOffWall;
	bool _isJumping;

	UFUNCTION()
	void TurnOffJumpOffWall();
	FTimerHandle timerHandle;

protected:

	/** Resets HMD orientation in VR. */
	void OnResetVR();

	/** Called for forwards/backward input */
	void MoveForward(float Value);

	/** Called for side to side input */
	void MoveRight(float Value);

	/** 
	 * Called via input to turn at a given rate. 
	 * @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	 */
	void TurnAtRate(float Rate);

	/**
	 * Called via input to turn look up/down at a given rate. 
	 * @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	 */
	void LookUpAtRate(float Rate);

	/** Handler for when a touch input begins. */
	void TouchStarted(ETouchIndex::Type FingerIndex, FVector Location);

	/** Handler for when a touch input stops. */
	void TouchStopped(ETouchIndex::Type FingerIndex, FVector Location);

protected:
	// APawn interface
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	// End of APawn interface

public:
	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	UFUNCTION(BlueprintCallable, Category = "Parkour")
	void SetMoveSpeed(float speed);

	UFUNCTION(BlueprintCallable, Category = "Parkour")
		void StartCrouch();

	UFUNCTION(BlueprintCallable, Category = "Parkour")
		void StopCrouch();

	UFUNCTION(BlueprintCallable, Category = "Parkour")
		void StartSlide();

	UFUNCTION(BlueprintCallable, Category = "Parkour")
		void StopSlide();

	UFUNCTION(BlueprintCallable, Category = "Parkour")
		bool CheckForClimbing();

	UFUNCTION(BlueprintCallable, Category = "Parkour")
		void StartVaultOrGetUp();

	UFUNCTION(BlueprintCallable, Category = "Parkour")
		void StopVaultOrGetUp();

	UFUNCTION(BlueprintCallable, Category = "Parkour")
		void CheckForWallRunning();

	UFUNCTION(BlueprintCallable, Category = "Parkour")
		void CheckJump();
};

