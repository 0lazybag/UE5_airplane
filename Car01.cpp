#include "Car01.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Math/UnrealMathUtility.h"

ACar01::ACar01()
{
	PrimaryActorTick.bCanEverTick = true;

	TravelDistance = 1900.f;
	Speed = 600.f;
	StopDuration = 5.f;
	WheelRadius = 30.f;
	WheelSpinAxis = EWheelSpinAxis::XAxis;

	Direction = 1;
	bIsStopped = false;
	StopTimer = 0.f;
	AccumulatedTravel = 0.f;

	bDoorsAnimating = false;
	DoorAnimTimer = 0.f;
	DoorPhase = 0;
	DoorOpenAngle = 75.f;
	DoorOpenTime = 0.5f;
	PauseBeforeOpen = 1.0f;
	PauseAfterClose = 1.0f;

	// components setup
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);

	WheelFL = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WheelFL"));
	WheelFL->SetupAttachment(Body);

	WheelFR = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WheelFR"));
	WheelFR->SetupAttachment(Body);

	WheelRL = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WheelRL"));
	WheelRL->SetupAttachment(Body);

	WheelRR = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WheelRR"));
	WheelRR->SetupAttachment(Body);

	DoorFL = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorFL"));
	DoorFL->SetupAttachment(Body);

	DoorFR = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorFR"));
	DoorFR->SetupAttachment(Body);

	Body->SetSimulatePhysics(false);
	WheelFL->SetSimulatePhysics(false);
	WheelFR->SetSimulatePhysics(false);
	WheelRL->SetSimulatePhysics(false);
	WheelRR->SetSimulatePhysics(false);
	DoorFL->SetSimulatePhysics(false);
	DoorFR->SetSimulatePhysics(false);
}

void ACar01::BeginPlay()
{
	Super::BeginPlay();

	StartLocation = GetActorLocation();
	ForwardDir = GetActorForwardVector().GetSafeNormal();
	Direction = 1;
	bIsStopped = false;
	StopTimer = 0.f;
	AccumulatedTravel = 0.f;

	bDoorsAnimating = false;
	DoorAnimTimer = 0.f;
	DoorPhase = 0;

	if (DoorFL)
	{
		DoorFL_StartRot = DoorFL->GetRelativeRotation();
	}
	if (DoorFR)
	{
		DoorFR_StartRot = DoorFR->GetRelativeRotation();
	}
}

void ACar01::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (DeltaTime <= 0.f) return;

	// Door animation handling (highest priority). While animating, movement is paused.
	if (bDoorsAnimating)
	{
		DoorAnimTimer += DeltaTime;

		// Phase machine:
		// 0 = pause before opening (PauseBeforeOpen)
		// 1 = opening (DoorOpenTime)
		// 3 = closing (DoorOpenTime)
		// 4 = pause after close (PauseAfterClose)

		if (DoorPhase == 0)
		{
			if (DoorAnimTimer >= PauseBeforeOpen)
			{
				DoorPhase = 1;
				DoorAnimTimer = 0.f;
			}
		}
		else if (DoorPhase == 1) // opening
		{
			const float PhaseTime = FMath::Max(0.0001f, DoorOpenTime);
			const float Alpha = FMath::Clamp(DoorAnimTimer / PhaseTime, 0.f, 1.f);
			const float Angle = FMath::Lerp(0.f, DoorOpenAngle, Alpha);

			if (DoorFL) DoorFL->SetRelativeRotation(DoorFL_StartRot + FRotator(0.f, Angle, 0.f));
			if (DoorFR) DoorFR->SetRelativeRotation(DoorFR_StartRot + FRotator(0.f, -Angle, 0.f));

			if (Alpha >= 1.f)
			{
				// immediately start closing (no extra wait open)
				DoorPhase = 3;
				DoorAnimTimer = 0.f;
			}
		}
		else if (DoorPhase == 3) // closing
		{
			const float PhaseTime = FMath::Max(0.0001f, DoorOpenTime);
			const float Alpha = FMath::Clamp(DoorAnimTimer / PhaseTime, 0.f, 1.f);
			const float Angle = FMath::Lerp(DoorOpenAngle, 0.f, Alpha);

			if (DoorFL) DoorFL->SetRelativeRotation(DoorFL_StartRot + FRotator(0.f, Angle, 0.f));
			if (DoorFR) DoorFR->SetRelativeRotation(DoorFR_StartRot + FRotator(0.f, -Angle, 0.f));

			if (Alpha >= 1.f)
			{
				DoorPhase = 4;
				DoorAnimTimer = 0.f;
			}
		}
		else if (DoorPhase == 4) // pause after close
		{
			if (DoorAnimTimer >= PauseAfterClose)
			{
				// finish animation sequence
				bDoorsAnimating = false;
				bIsStopped = false;
				DoorAnimTimer = 0.f;
				DoorPhase = 0;
			}
		}

		// while animating doors we skip the rest (no movement)
		return;
	}

	if (bIsStopped)
	{
		StopTimer += DeltaTime;
		if (StopTimer >= StopDuration)
		{
			StopTimer = 0.f;
			// invert direction for next leg
			Direction *= -1;

			// if next direction is forward, play door open+close before moving
			if (Direction == 1)
			{
				bDoorsAnimating = true;
				DoorPhase = 0; // start with PauseBeforeOpen
				DoorAnimTimer = 0.f;
			}
			else
			{
				// start moving immediately when going backward
				bIsStopped = false;
			}
		}
		return;
	}

	// movement
	const float MoveDist = Speed * DeltaTime * Direction;
	const FVector Delta = ForwardDir * MoveDist;
	const FVector NewLoc = GetActorLocation() + Delta;
	SetActorLocation(NewLoc);

	// update accumulator (optional)
	AccumulatedTravel += MoveDist;

	// compute incremental wheel rotation for this frame
	const float DeltaAngleRad = MoveDist / FMath::Max(WheelRadius, 1.0f);
	const float DeltaAngleDeg = FMath::RadiansToDegrees(DeltaAngleRad);

	// invert direction for visual wheel spin (so wheel rotates opposite to travel)
	const float InvertedDeltaDeg = -DeltaAngleDeg;

	FRotator DeltaRot = FRotator::ZeroRotator;
	switch (WheelSpinAxis)
	{
	case EWheelSpinAxis::XAxis: DeltaRot.Pitch = InvertedDeltaDeg; break;
	case EWheelSpinAxis::YAxis: DeltaRot.Yaw = InvertedDeltaDeg; break;
	case EWheelSpinAxis::ZAxis: DeltaRot.Roll = InvertedDeltaDeg; break;
	default: DeltaRot.Pitch = InvertedDeltaDeg; break;
	}

	if (WheelFL) WheelFL->AddLocalRotation(DeltaRot);
	if (WheelFR) WheelFR->AddLocalRotation(DeltaRot);
	if (WheelRL) WheelRL->AddLocalRotation(DeltaRot);
	if (WheelRR) WheelRR->AddLocalRotation(DeltaRot);

	// endpoints
	const FVector ToCurrent = GetActorLocation() - StartLocation;
	const float Project = FVector::DotProduct(ToCurrent, ForwardDir);

	if (Project >= TravelDistance)
	{
		SetActorLocation(StartLocation + ForwardDir * TravelDistance);
		AccumulatedTravel = TravelDistance;
		bIsStopped = true;
		StopTimer = 0.f;
	}
	else if (Project <= 0.f)
	{
		SetActorLocation(StartLocation);
		AccumulatedTravel = 0.f;
		bIsStopped = true;
		StopTimer = 0.f;
	}
}
