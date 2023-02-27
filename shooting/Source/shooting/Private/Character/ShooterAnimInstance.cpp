#include "Character/ShooterAnimInstance.h"
#include "Character/ShooterCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"

void UShooterAnimInstance::UpdateAnimationProperties(float DeltaTime)
{
	if (ShooterCharacter == nullptr)
	{
		ShooterCharacter = Cast<AShooterCharacter>(TryGetPawnOwner());
	}

	if (ShooterCharacter)
	{
		SetCharacterSpeed();
		SetIsInAir();
		SetIsAccelerating();
	}

	FRotator AimRotation = ShooterCharacter->GetBaseAimRotation();
	FRotator MovementRotation = UKismetMathLibrary::MakeRotFromX(ShooterCharacter->GetVelocity());
	MovementOffsetYaw = UKismetMathLibrary::NormalizedDeltaRotator(MovementRotation, AimRotation).Yaw;



	//FString RotationMessage = FString::Printf(TEXT("Base Aim Rotation: %f"), AimRotation.Yaw);
	//FString MovementMessage = FString::Printf(TEXT("Movement Rotation: %f"), MovementRotation.Yaw);
	//FString MovementOffsetYawMessage = FString::Printf(TEXT("MovementOffsetYaw: %f"), MovementOffsetYaw);
	/*if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(1, 0.f, FColor::White, RotationMessage);
		GEngine->AddOnScreenDebugMessage(2, 0.f, FColor::White, MovementMessage);
		GEngine->AddOnScreenDebugMessage(3, 0.f, FColor::White, MovementOffsetYawMessage);
	}*/
}

void UShooterAnimInstance::NativeInitializeAnimation()
{
	ShooterCharacter = Cast<AShooterCharacter>(TryGetPawnOwner());
}

// Is the character accelerating ?
void UShooterAnimInstance::SetIsAccelerating()
{
	if (ShooterCharacter->GetCharacterMovement()->GetCurrentAcceleration().Size() > 0.f)
	{
		bIsAccelerating = true;
	}
	else
	{
		bIsAccelerating = false;
	}
}

// Is the character in the air?
void UShooterAnimInstance::SetIsInAir()
{
	bIsInAir = ShooterCharacter->GetCharacterMovement()->IsFalling();
}

// Get the lateral speed of the character from velocity
void UShooterAnimInstance::SetCharacterSpeed()
{
	FVector Velocity = ShooterCharacter->GetVelocity();
	Velocity.Z = 0;
	Speed = Velocity.Size();
}


