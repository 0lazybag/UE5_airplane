// CPP_NPC02.cpp
#include "CPP_NPC02.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Kismet/KismetMathLibrary.h"
#include "Blueprint/UserWidget.h"
#include "UObject/ConstructorHelpers.h"
#include "TimerManager.h"

// Player character header
#include "CPP_Character1.h"

ACPP_NPC02::ACPP_NPC02()
{
	PrimaryActorTick.bCanEverTick = true;

	// Root collision box
	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	RootComponent = CollisionBox;
	CollisionBox->SetBoxExtent(FVector(40.f, 40.f, 30.f));
	CollisionBox->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	CollisionBox->SetNotifyRigidBodyCollision(true);
	CollisionBox->SetGenerateOverlapEvents(false);

	// Visual mesh (no collision)
	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(RootComponent);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Bind hit event
	CollisionBox->OnComponentHit.AddDynamic(this, &ACPP_NPC02::OnHit);

	// Defaults
	MoveSpeed = 300.f;
	TurnSpeed = 180.f;
	MinTurnAngle = 90.f;
	MaxTurnAngle = 180.f;
	CollisionCooldown = 0.3f;

	bIsTurning = false;
	CollisionTimer = 0.f;
	TargetYaw = GetActorRotation().Yaw;

	bMovementEnabled = true;
	bHasRotationTarget = false;

	LastInteractingCharacter = nullptr;
	DialogWidgetInstance = nullptr;
	StopDelayAfterTrace = 0.05f;

	// Optional: try to find WBP_Dialog02 widget automatically
	static ConstructorHelpers::FClassFinder<UUserWidget> DialogBP(TEXT("/Game/Game/Dialog02/WBP_Dialog02.WBP_Dialog02_C"));
	if (DialogBP.Succeeded())
	{
		DialogWidgetClass = DialogBP.Class;
	}
}

void ACPP_NPC02::BeginPlay()
{
	Super::BeginPlay();

	bIsTurning = false;
	CollisionTimer = 0.f;
	TargetYaw = GetActorRotation().Yaw;
}

void ACPP_NPC02::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Update cooldown
	if (CollisionTimer > 0.f)
	{
		CollisionTimer = FMath::Max(0.f, CollisionTimer - DeltaTime);
	}

	FRotator CurrentRot = GetActorRotation();

	// handle rotation toward target if set
	if (bHasRotationTarget)
	{
		FRotator NewRot = FMath::RInterpConstantTo(CurrentRot, RotationTarget, DeltaTime, TurnSpeed);
		SetActorRotation(NewRot);

		if (CurrentRot.Equals(RotationTarget, 0.5f))
		{
			SetActorRotation(RotationTarget);
			bHasRotationTarget = false;
		}
		return;
	}

	if (bIsTurning)
	{
		// Smoothly turn toward TargetYaw (using FixedTurn for continuity)
		float NewYaw = FMath::FixedTurn(CurrentRot.Yaw, TargetYaw, TurnSpeed * DeltaTime);
		SetActorRotation(FRotator(CurrentRot.Pitch, NewYaw, CurrentRot.Roll));

		// When close enough, stop turning
		if (FMath::Abs(FMath::UnwindDegrees(TargetYaw - NewYaw)) <= 1.0f)
		{
			bIsTurning = false;
		}
	}
	else
	{
		// Move forward only when movement enabled
		if (bMovementEnabled)
		{
			FVector Forward = GetActorForwardVector();
			FVector NewLocation = GetActorLocation() + Forward * MoveSpeed * DeltaTime;
			SetActorLocation(NewLocation, true);
		}
	}
}

void ACPP_NPC02::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{
	if (!OtherActor || OtherActor == this) return;
	if (CollisionTimer > 0.f) return;

	// Start cooldown
	CollisionTimer = CollisionCooldown;

	// If we collide with something else (wall), do random turn as before
	if (!OtherActor->IsA(ACPP_Character1::StaticClass()))
	{
		float Angle = FMath::RandRange(MinTurnAngle, MaxTurnAngle);
		if (FMath::RandBool()) Angle = -Angle;
		float CurrentYaw = GetActorRotation().Yaw;
		TargetYaw = FMath::UnwindDegrees(CurrentYaw + Angle);
		bIsTurning = true;
	}
}

void ACPP_NPC02::FacePlayerAndStop(const FVector& PlayerLocation)
{
	// Stop movement
	bMovementEnabled = false;
	bIsTurning = false;

	// compute yaw target to face player
	FVector ToPlayer = PlayerLocation - GetActorLocation();
	ToPlayer.Z = 0.f;
	if (ToPlayer.IsNearlyZero()) return;

	FRotator TargetRot = ToPlayer.Rotation();
	RotationTarget = FRotator(0.f, TargetRot.Yaw, 0.f);
	bHasRotationTarget = true;
}

void ACPP_NPC02::ResumeMovement()
{
	bMovementEnabled = true;

	// rotate back to original forward vector smoothly
	FRotator PatrolRot = GetActorRotation();
	RotationTarget = PatrolRot;
	bHasRotationTarget = true;
}
