#include "CPP_NPC01.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/KismetMathLibrary.h"

ACPP_NPC01::ACPP_NPC01()
{
	PrimaryActorTick.bCanEverTick = true;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	SetRootComponent(MeshComp);

	MoveSpeed = 200.f;
	MoveDistance = 800.f;
	WaitTimeAtEnd = 0.5f;
	TurnInterpSpeed = 360.f;

	MoveDirection = 1;
	bWaiting = false;
	WaitTimer = 0.f;
	bMovementEnabled = true;

	bHasRotationTarget = false;
}

void ACPP_NPC01::BeginPlay()
{
	Super::BeginPlay();

	StartLocation = GetActorLocation();
	TargetLocation = StartLocation + GetActorForwardVector() * MoveDistance;
	MoveDirection = 1;
	bWaiting = false;
	WaitTimer = 0.f;
	bMovementEnabled = true;

	PatrolRotation = GetActorRotation();
}

void ACPP_NPC01::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Rotation interpolation
	if (bHasRotationTarget)
	{
		FRotator Curr = GetActorRotation();
		FRotator NewRot = FMath::RInterpConstantTo(Curr, RotationTarget, DeltaTime, TurnInterpSpeed);
		SetActorRotation(NewRot);

		if (Curr.Equals(RotationTarget, 0.5f))
		{
			SetActorRotation(RotationTarget);
			bHasRotationTarget = false;
		}
	}

	// Movement only when enabled
	if (!bMovementEnabled) return;

	// Waiting at endpoint
	if (bWaiting)
	{
		WaitTimer += DeltaTime;
		if (WaitTimer >= WaitTimeAtEnd)
		{
			WaitTimer = 0.f;
			bWaiting = false;
			MoveDirection *= -1;

			// set patrol rotation target (smooth turn to face new patrol direction)
			FRotator NewPatrolRot = PatrolRotation;
			NewPatrolRot.Yaw += (MoveDirection > 0) ? 0.f : 180.f;
			RotationTarget = NewPatrolRot;
			bHasRotationTarget = true;
		}
		return;
	}

	// Patrol movement toward destination
	FVector Current = GetActorLocation();
	FVector Destination = (MoveDirection > 0) ? TargetLocation : StartLocation;
	FVector ToDest = Destination - Current;
	float Dist = ToDest.Size();

	if (Dist <= KINDA_SMALL_NUMBER)
	{
		bWaiting = true;
		return;
	}

	FVector Dir = ToDest / Dist;
	FVector Next = Current + Dir * MoveSpeed * DeltaTime;

	// Clamp overshoot
	if (FVector::DistSquared(Next, Destination) <= KINDA_SMALL_NUMBER || FVector::DotProduct(Dir, Destination - Current) < 0.f)
	{
		Next = Destination;
	}

	SetActorLocation(Next);

	const float ArriveEpsilon = 5.f;
	if (FVector::Dist(Next, Destination) <= ArriveEpsilon)
	{
		SetActorLocation(Destination);
		bWaiting = true;
	}
}

void ACPP_NPC01::FacePlayerAndStop(const FVector& PlayerLocation)
{
	// stop movement
	bMovementEnabled = false;
	bWaiting = false;
	WaitTimer = 0.f;

	// compute yaw target to face player
	FVector ToPlayer = PlayerLocation - GetActorLocation();
	ToPlayer.Z = 0.f;
	if (ToPlayer.IsNearlyZero()) return;

	FRotator TargetRot = ToPlayer.Rotation();
	RotationTarget = FRotator(0.f, TargetRot.Yaw, 0.f);
	bHasRotationTarget = true;
}

void ACPP_NPC01::ResumeMovement()
{
	// resume patrol
	bMovementEnabled = true;

	// set rotation target back to patrol orientation for smooth return
	FRotator DesiredPatrolRot = PatrolRotation;
	DesiredPatrolRot.Yaw += (MoveDirection > 0) ? 0.f : 180.f;
	RotationTarget = DesiredPatrolRot;
	bHasRotationTarget = true;
}
