#include "Character/ShooterCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"
#include "Engine/SkeletalMeshSocket.h"
#include "DrawDebugHelpers.h"
#include "Particles/ParticleSystemComponent.h"

AShooterCharacter::AShooterCharacter()
{
 	PrimaryActorTick.bCanEverTick = true;

	CreateSpringArm();
	CreateCamera();
	BlockCharacterRotationWithCamera();

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = false;	// Character move in the direction of input
	GetCharacterMovement()->RotationRate = FRotator(0.f, 500.f, 0.f);
	GetCharacterMovement()->JumpZVelocity = 600.f;
	GetCharacterMovement()->AirControl = 0.2f;
}


// Don't Rotate when the character controller rotates
void AShooterCharacter::BlockCharacterRotationWithCamera()
{
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;
}

void AShooterCharacter::CreateSpringArm()
{
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(GetRootComponent());
	SpringArm->TargetArmLength = 300.f;
	SpringArm->bUsePawnControlRotation = true;
	SpringArm->SocketOffset = FVector(0.f, 50.f, 50.f);
}

void AShooterCharacter::CreateCamera()
{
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;
}



void AShooterCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && LevelStartMontage)
	{
		AnimInstance->Montage_Play(LevelStartMontage);
		AnimInstance->Montage_JumpToSection(FName("LevelStart"));
	}
}

void AShooterCharacter::MoveForward(float Value)
{
	if ((Controller != nullptr) && (Value != 0.f))
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation = FRotator(0.f, Rotation.Yaw, 0.f);
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		AddMovementInput(Direction, Value);
	}
}

void AShooterCharacter::MoveRight(float Value)
{
	if ((Controller != nullptr) && (Value != 0.f))
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation = FRotator(0.f, Rotation.Yaw, 0.f);
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		AddMovementInput(Direction, Value);
	}
}


// Called when Firebutton is pressed
void AShooterCharacter::FireWeapon()
{
	if (FireSound)
	{
		UGameplayStatics::PlaySound2D(this, FireSound);
	}

	const USkeletalMeshSocket* BarrelSocket = GetMesh()->GetSocketByName("BarrelSocket");
	if (BarrelSocket)
	{
		const FTransform SocketTransform = BarrelSocket->GetSocketTransform(GetMesh());
		if (MuzzleFlash)
		{
			UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), MuzzleFlash, SocketTransform);
		}

		// Get current size of the viewport
		FVector2D ViewportSize;
		if (GEngine && GEngine->GameViewport)
		{
			GEngine->GameViewport->GetViewportSize(ViewportSize);
		}

		// Get screen space location of crosshair
		FVector2D CrossHairLocation(ViewportSize.X / 2.f, ViewportSize.Y / 2.f);
		CrossHairLocation.Y -= 50.f;

		FVector CrossHairWorldPosition;
		FVector CrossHairWorldDirection;

		// Get world position and direction of crosshair
		bool bScreenToWorld = UGameplayStatics::DeprojectScreenToWorld(UGameplayStatics::GetPlayerController(this, 0), 
			CrossHairLocation,
			CrossHairWorldPosition, 
			CrossHairWorldDirection);

		if (bScreenToWorld)	// was deprojection successful ?
		{
			FHitResult ScreenTraceHit;
			const FVector Start = CrossHairWorldPosition;
			const FVector End = CrossHairWorldPosition + CrossHairWorldDirection * 50000.f;
			
			FVector BeamEndPoint = End;	// set beam end point to line trace end point
			
			// Trace outward from crosshair world location
			GetWorld()->LineTraceSingleByChannel(
				ScreenTraceHit,
				Start,
				End,
				ECollisionChannel::ECC_Visibility
			);

			if (ScreenTraceHit.bBlockingHit)	// was there a trace hit ?
			{
				// Beam end point is now trace hit location
				BeamEndPoint = ScreenTraceHit.Location;
			}

			// Perform a second trace, this time from the gun barrel
			FHitResult WeaponTraceHit;
			const FVector WeaponTraceStart = SocketTransform.GetLocation();
			const FVector WeaponTraceEnd = BeamEndPoint;
			GetWorld()->LineTraceSingleByChannel(
				WeaponTraceHit,
				WeaponTraceStart,
				WeaponTraceEnd,
				ECollisionChannel::ECC_Visibility
			);
			if (WeaponTraceHit.bBlockingHit) // object between barrel and BeamEndPoint ?
			{
				BeamEndPoint = WeaponTraceHit.Location;
			}

			// Spawn impact particles after updating BeamEndPoint
			if (ImpactParticles)
			{
				UGameplayStatics::SpawnEmitterAtLocation(
					GetWorld(),
					ImpactParticles,
					BeamEndPoint
				);
			}

			if (BeamParticles)
			{
				UParticleSystemComponent* Beam = UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), BeamParticles, SocketTransform);
				if (Beam)
				{
					Beam->SetVectorParameter(FName("Target"), BeamEndPoint);
				}
			}
		}
	}

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && HitFireMontage)
	{
		AnimInstance->Montage_Play(HitFireMontage);
		AnimInstance->Montage_JumpToSection(FName("StartFire"));
	}
}

void AShooterCharacter::SpawnLevelStartParticle()
{
	const USkeletalMeshSocket* Socket = GetMesh()->GetSocketByName("LevelStart");
	if (Socket)
	{
		const FTransform SocketTransform = Socket->GetSocketTransform(GetMesh());
		if (LevelStartParticle)
		{
			UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), LevelStartParticle, SocketTransform);
		}
	}
}


void AShooterCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AShooterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (PlayerInputComponent)
	{
		PlayerInputComponent->BindAxis("MoveForward", this, &AShooterCharacter::MoveForward);
		PlayerInputComponent->BindAxis("MoveRight", this, &AShooterCharacter::MoveRight);
		PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
		PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
		PlayerInputComponent->BindAction("Jump", EInputEvent::IE_Pressed, this, &ACharacter::Jump);
		PlayerInputComponent->BindAction("FireButton", EInputEvent::IE_Pressed, this, &AShooterCharacter::FireWeapon);
	}
}