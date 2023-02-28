#include "Character/ShooterCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"
#include "Engine/SkeletalMeshSocket.h"
#include "DrawDebugHelpers.h"
#include "Particles/ParticleSystemComponent.h"

AShooterCharacter::AShooterCharacter() :
	BaseTurnRate(45.f),
	BaseLookUpRate(45.f),
	HipTurnRate(90.f),
	HipLookUpRate(90.f),
	AimingTurnRate(20.f),
	AimingLookUpRate(20.f),
	MouseHipTurnRate(1.0f),
	MouseHipLookUpRate(1.0f),
	MouseAimingTurnRate(0.2f),
	MouseAimingLookUpRate(0.2f),
	bAiming(false),
	CameraDefaultFOV(0.f),	// set in BeginPlay
	CameraZoomedFOV(35.f),
	CameraCurrentFOV(0.f),
	ZoomInterpSpeed(20.f),
	TargetArmLengthDefault(180.f),
	TargetArmLengthZoomed(130.f),
	TargetArmLengthCurrent(0.f),
	SocketOffsetDefault(0.f,50.f,70.f),
	SocketOffsetZoomed(0.f, -10.f, 65.f),
	SocketOffsetCurrent(0.f, 0.f, 0.f),
	CrosshairSpreadMultiplier(0.f),
	CrosshairVelocityFactor(0.f),
	CrosshairInAirFactor(0.f),
	CrosshairAimFactor(0.f),
	CrosshairShootingFactor(0.f),
	ShootTimeDuration(0.05f),
	bFiringBullet(false)
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

void AShooterCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (Camera)
	{
		CameraDefaultFOV = GetCamera()->FieldOfView;
		CameraCurrentFOV = CameraDefaultFOV;
		
	}

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && LevelStartMontage)
	{
		AnimInstance->Montage_Play(LevelStartMontage);
		AnimInstance->Montage_JumpToSection(FName("LevelStart"));
	}
}

void AShooterCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	

	// Handle interpolation for zoom when aiming
	CameraInterpZoom(DeltaTime);

	// Change look sensitivity based on aiming
	SetLookRates();

	// Calculate crosshair spread multiplier
	CalculateCrosshairSpread(DeltaTime);

	
}

// Don't Rotate when the character controller rotates
void AShooterCharacter::BlockCharacterRotationWithCamera()
{
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;
}

float AShooterCharacter::GetCrosshairSpreadMultiplier() const
{
	return CrosshairSpreadMultiplier;
}

void AShooterCharacter::CreateSpringArm()
{
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(GetRootComponent());
	TargetArmLengthCurrent = TargetArmLengthDefault;
	SpringArm->TargetArmLength = TargetArmLengthCurrent;
	SocketOffsetCurrent = SocketOffsetDefault;
	SpringArm->SocketOffset = SocketOffsetCurrent;
	SpringArm->bUsePawnControlRotation = true;
}

void AShooterCharacter::CreateCamera()
{
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;
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

void AShooterCharacter::TurnAtRate(float Rate)
{
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void AShooterCharacter::LookUpAtRate(float Rate)
{
	AddControllerPitchInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void AShooterCharacter::Turn(float Value)
{
	float TurnScaleFactor{};
	if (bAiming)
	{
		TurnScaleFactor = MouseAimingTurnRate;
	}
	else
	{
		TurnScaleFactor = MouseHipTurnRate;
	}
	AddControllerYawInput(Value * TurnScaleFactor);
}

void AShooterCharacter::LookUp(float Value)
{
	float LookUpScaleFactor{};
	if (bAiming)
	{
		LookUpScaleFactor = MouseAimingLookUpRate;
	}
	else
	{
		LookUpScaleFactor = MouseHipLookUpRate;
	}
	AddControllerPitchInput(Value * LookUpScaleFactor);
}

// Called when Firebutton is pressed
void AShooterCharacter::FireWeapon()
{
	PlayFireSound();

	const USkeletalMeshSocket* BarrelSocket = GetMesh()->GetSocketByName("BarrelSocket");
	if (BarrelSocket)
	{
		const FTransform SocketTransform = BarrelSocket->GetSocketTransform(GetMesh());
		SpawnMuzzleFlashParticles(SocketTransform);

		FVector BeamEnd;
		bool bBeamEnd = GetBeamEndLocation(SocketTransform.GetLocation(), BeamEnd);
		if (bBeamEnd)
		{
			SpawnImpactParticles(BeamEnd);
			SpawnBeamParticles(SocketTransform, BeamEnd);
		}
	}

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && HitFireMontage)
	{
		AnimInstance->Montage_Play(HitFireMontage);
		AnimInstance->Montage_JumpToSection(FName("StartFire"));
	}

	// Start bullet fire timer for crosshairs
	StartCrosshairBulletFire();
}

void AShooterCharacter::SpawnMuzzleFlashParticles(const FTransform& SocketTransform)
{
	if (MuzzleFlash)
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), MuzzleFlash, SocketTransform);
	}
}

void AShooterCharacter::PlayFireSound()
{
	if (FireSound)
	{
		UGameplayStatics::PlaySound2D(this, FireSound);
	}
}

bool AShooterCharacter::GetBeamEndLocation(const FVector& MuzzleSocketLocation, FVector& OutBeamLocation)
{
	// Get current size of the viewport
	FVector2D ViewportSize;
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}

	// Get screen space location of crosshair
	FVector2D CrossHairLocation(ViewportSize.X / 2.f, ViewportSize.Y / 2.f);
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

		OutBeamLocation = End;	// set beam end point to line trace end point

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
			OutBeamLocation = ScreenTraceHit.Location;
		}

		// Perform a second trace, this time from the gun barrel
		FHitResult WeaponTraceHit;
		const FVector WeaponTraceStart = MuzzleSocketLocation;
		const FVector WeaponTraceEnd = OutBeamLocation;
		GetWorld()->LineTraceSingleByChannel(
			WeaponTraceHit,
			WeaponTraceStart,
			WeaponTraceEnd,
			ECollisionChannel::ECC_Visibility
		);
		if (WeaponTraceHit.bBlockingHit) // object between barrel and BeamEndPoint ?
		{
			OutBeamLocation = WeaponTraceHit.Location;
		}
		return true;
	}
	return false;
}

void AShooterCharacter::SpawnBeamParticles(const FTransform& SocketTransform, const FVector& BeamEnd)
{
	if (BeamParticles)
	{
		UParticleSystemComponent* Beam = UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), BeamParticles, SocketTransform);
		if (Beam)
		{
			Beam->SetVectorParameter(FName("Target"), BeamEnd);
		}
	}
}

void AShooterCharacter::SpawnImpactParticles(const FVector& BeamEnd)
{
	// Spawn impact particles after updating BeamEndPoint
	if (ImpactParticles)
	{
		UGameplayStatics::SpawnEmitterAtLocation(
			GetWorld(),
			ImpactParticles,
			BeamEnd
		);
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

void AShooterCharacter::AimingButtonPressed()
{
	bAiming = true;
}

void AShooterCharacter::AimingButtonReleased()
{
	bAiming = false;
}

void AShooterCharacter::CameraInterpZoom(float DeltaTime)
{
	// Set current camera field of view
	if (bAiming)
	{
		// Interpolate to zoomed FOV
		CameraCurrentFOV = FMath::FInterpTo(CameraCurrentFOV, CameraZoomedFOV, DeltaTime, ZoomInterpSpeed);
		
		TargetArmLengthCurrent = FMath::FInterpTo(TargetArmLengthCurrent, TargetArmLengthZoomed, DeltaTime, ZoomInterpSpeed);
	
		SocketOffsetCurrent.X = FMath::FInterpTo(SocketOffsetCurrent.X, SocketOffsetZoomed.X, DeltaTime, ZoomInterpSpeed);
		SocketOffsetCurrent.Y = FMath::FInterpTo(SocketOffsetCurrent.Y, SocketOffsetZoomed.Y, DeltaTime, ZoomInterpSpeed);
		SocketOffsetCurrent.Z = FMath::FInterpTo(SocketOffsetCurrent.Z, SocketOffsetZoomed.Z, DeltaTime, ZoomInterpSpeed);
	}
	else
	{
		// Interpolate to default FOV
		CameraCurrentFOV = FMath::FInterpTo(CameraCurrentFOV, CameraDefaultFOV, DeltaTime, ZoomInterpSpeed);
		
		TargetArmLengthCurrent = FMath::FInterpTo(TargetArmLengthCurrent, TargetArmLengthDefault, DeltaTime, ZoomInterpSpeed);
		
		SocketOffsetCurrent.X = FMath::FInterpTo(SocketOffsetCurrent.X, SocketOffsetDefault.X, DeltaTime, ZoomInterpSpeed);
		SocketOffsetCurrent.Y = FMath::FInterpTo(SocketOffsetCurrent.Y, SocketOffsetDefault.Y, DeltaTime, ZoomInterpSpeed);
		SocketOffsetCurrent.Z = FMath::FInterpTo(SocketOffsetCurrent.Z, SocketOffsetDefault.Z, DeltaTime, ZoomInterpSpeed);
	}
	GetCamera()->SetFieldOfView(CameraCurrentFOV);
	SpringArm->TargetArmLength = TargetArmLengthCurrent;
	SpringArm->SocketOffset = SocketOffsetCurrent;
}

void AShooterCharacter::SetLookRates()
{
	if (bAiming)
	{
		BaseTurnRate = AimingTurnRate;
		BaseLookUpRate = AimingLookUpRate;
	}
	else
	{
		BaseTurnRate = HipTurnRate;
		BaseLookUpRate = HipLookUpRate;
	}
}

void AShooterCharacter::CalculateCrosshairSpread(float DeltaTime)
{
	FVector2D WalkSpeedRange{ 0.f, 600.f };
	FVector2D VelocityMultiplierRange{ 0.f, 1.f };
	FVector Velocity{ GetVelocity() };
	Velocity.Z = 0.f;

	// Calculate crosshair velocity factor
	CrosshairVelocityFactor = FMath::GetMappedRangeValueClamped(
		WalkSpeedRange, VelocityMultiplierRange, Velocity.Size()
	);

	// Calculate crosshair in air factor
	if (GetCharacterMovement()->IsFalling())
	{
		// Spread the crosshairs slowly while in air
		CrosshairInAirFactor = FMath::FInterpTo(CrosshairInAirFactor, 2.25f, DeltaTime, 2.25f);
	}
	else
	{
		// Character is on the ground. Shrink the crosshairs rapidly while on the ground
		CrosshairInAirFactor = FMath::FInterpTo(CrosshairInAirFactor, 0.f, DeltaTime, 30.f);
	}

	// Calculate crosshair aim factor
	if (bAiming)	// Are we aiming?
	{
		// Shrink crosshairs a small amount very quickly
		CrosshairAimFactor = FMath::FInterpTo(CrosshairAimFactor, 0.6f, DeltaTime, 30.f);
	}
	else
	{
		CrosshairAimFactor = FMath::FInterpTo(CrosshairAimFactor, 0.f, DeltaTime, 30.f);
	}

	// True 0.05 second after firing
	if (bFiringBullet)
	{
		CrosshairShootingFactor = FMath::FInterpTo(CrosshairShootingFactor, 0.3f, DeltaTime, 60.f);
	}
	else
	{
		CrosshairShootingFactor = FMath::FInterpTo(CrosshairShootingFactor, 0.f, DeltaTime, 60.f);
	}

	CrosshairSpreadMultiplier = 
		0.5f + CrosshairVelocityFactor + CrosshairInAirFactor - CrosshairAimFactor + CrosshairShootingFactor;
}

void AShooterCharacter::StartCrosshairBulletFire()
{
	bFiringBullet = true;
	
	GetWorldTimerManager().SetTimer(CrosshairShootTimer, this, &AShooterCharacter::FinishCrosshairBulletFire, ShootTimeDuration);
}

void AShooterCharacter::FinishCrosshairBulletFire()
{
	bFiringBullet = false;
}

void AShooterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (PlayerInputComponent)
	{
		PlayerInputComponent->BindAxis("MoveForward", this, &AShooterCharacter::MoveForward);
		PlayerInputComponent->BindAxis("MoveRight", this, &AShooterCharacter::MoveRight);
		PlayerInputComponent->BindAxis("TurnRate", this, &AShooterCharacter::TurnAtRate);
		PlayerInputComponent->BindAxis("LookUpRate", this, &AShooterCharacter::LookUpAtRate);
		PlayerInputComponent->BindAxis("Turn", this, &AShooterCharacter::Turn);
		PlayerInputComponent->BindAxis("LookUp", this, &AShooterCharacter::LookUp);
		PlayerInputComponent->BindAction("Jump", EInputEvent::IE_Pressed, this, &ACharacter::Jump);
		PlayerInputComponent->BindAction("FireButton", EInputEvent::IE_Pressed, this, &AShooterCharacter::FireWeapon);
		PlayerInputComponent->BindAction("AimingButton", EInputEvent::IE_Pressed, this, &AShooterCharacter::AimingButtonPressed);
		PlayerInputComponent->BindAction("AimingButton", EInputEvent::IE_Released, this, &AShooterCharacter::AimingButtonReleased);
	}
}