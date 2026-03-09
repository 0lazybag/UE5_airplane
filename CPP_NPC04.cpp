#include "CPP_NPC04.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "Kismet/KismetMathLibrary.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"

ACPP_NPC04::ACPP_NPC04()
{
	PrimaryActorTick.bCanEverTick = true;

	// Capsule as root so actor has collision
	CapsuleComp = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	CapsuleComp->InitCapsuleSize(34.f, 88.f);
	CapsuleComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CapsuleComp->SetCollisionObjectType(ECC_Pawn);
	CapsuleComp->SetCollisionResponseToAllChannels(ECR_Block);
	RootComponent = CapsuleComp;

	// Visual mesh
	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MeshComp->SetupAttachment(RootComponent);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Movement / sensing defaults
	MoveSpeed = 800.f;
	RotationSpeedDeg = 720.f;
	GoalAcceptanceRadius = 120.f;
	WanderRadius = 2000.f;

	TraceDistance = 600.f;
	TraceRadius = 30.f;
	bDrawDebug = true;
	SamplesPerDirection = 3;
	TraceChannel = ECC_WorldStatic;

	StuckFramesThreshold = 15;

	// initialize directions & smoothing
	GenerateTraceDirections();
	SmoothedDir = GetActorForwardVector();
	LastLocation = GetActorLocation();

	// interaction defaults
	bMovementEnabled = true;
	bWaiting = false;
	WaitTimer = 0.f;
	bHasRotationTarget = false;
	PatrolRotation = GetActorRotation();

	// speed defaults
	CurrentSpeed = 0.f;
	ForwardSpeed = 0.f;
	RightSpeed = 0.f;
	PreviousLocation = FVector::ZeroVector;
}

void ACPP_NPC04::BeginPlay()
{
	Super::BeginPlay();
	PickNewRandomGoal();
	LastLocation = GetActorLocation();
	PatrolRotation = GetActorRotation();

	// initialize previous location for speed calc
	PreviousLocation = GetActorLocation();
}

void ACPP_NPC04::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// --- Update speeds for AnimBP ---
	if (DeltaTime > KINDA_SMALL_NUMBER)
	{
		FVector CurrLoc = GetActorLocation();
		FVector Delta = CurrLoc - PreviousLocation;

		FVector DeltaXY = Delta;
		DeltaXY.Z = 0.f; // ignore vertical motion for animation speed

		float NewSpeed = DeltaXY.Size() / DeltaTime;
		CurrentSpeed = FMath::FInterpTo(CurrentSpeed, NewSpeed, DeltaTime, 8.f); // smoothing

		// local (actor-space) speed components
		FVector LocalDelta = GetActorRotation().UnrotateVector(DeltaXY);
		float NewForward = LocalDelta.X / DeltaTime;
		float NewRight = LocalDelta.Y / DeltaTime;
		ForwardSpeed = FMath::FInterpTo(ForwardSpeed, NewForward, DeltaTime, 8.f);
		RightSpeed = FMath::FInterpTo(RightSpeed, NewRight, DeltaTime, 8.f);

		PreviousLocation = CurrLoc;
	}

	// Rotation interpolation (when target set by FacePlayerAndStop or ResumeMovement)
	if (bHasRotationTarget)
	{
		FRotator Curr = GetActorRotation();
		FRotator NewRot = FMath::RInterpConstantTo(Curr, RotationTarget, DeltaTime, RotationSpeedDeg);
		NewRot.Pitch = 0.f;
		NewRot.Roll = 0.f;
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
		}
		LastLocation = GetActorLocation();
		return;
	}

	if (!GetWorld()) return;

	FVector Loc = GetActorLocation();
	FVector ToGoal = CurrentGoal - Loc;
	ToGoal.Z = 0.f;
	float DistToGoal = ToGoal.Size();

	if (DistToGoal <= GoalAcceptanceRadius)
	{
		// pause briefly then pick new goal
		bWaiting = true;
		WaitTimer = 0.f;
		PickNewRandomGoal();
		return;
	}

	FVector DesiredDir = ToGoal.GetSafeNormal();
	FVector ChosenWorldDir = ChooseBestDirection(DesiredDir);

	// smoothing for stable movement
	const float SmoothSpeed = 8.f;
	if (SmoothedDir.IsNearlyZero()) SmoothedDir = ChosenWorldDir;
	SmoothedDir = FMath::VInterpNormalRotationTo(SmoothedDir, ChosenWorldDir, DeltaTime, SmoothSpeed);

	// Move with sweep to respect collision
	FVector NewLoc = Loc + SmoothedDir * MoveSpeed * DeltaTime;
	NewLoc.Z = Loc.Z;

	FHitResult MoveHit;
	SetActorLocation(NewLoc, true, &MoveHit, ETeleportType::None);

	// If we hit something physically, try to slide a bit along normal
	if (MoveHit.bBlockingHit)
	{
		FVector Deflection = FVector::VectorPlaneProject(SmoothedDir, MoveHit.Normal).GetSafeNormal();
		if (!Deflection.IsNearlyZero())
		{
			FVector SlideLoc = Loc + Deflection * MoveSpeed * DeltaTime * 0.5f;
			SlideLoc.Z = Loc.Z;
			SetActorLocation(SlideLoc, true);
			SmoothedDir = Deflection;
		}
	}

	// Rotate smoothly only in yaw toward movement dir (if not using rotation target)
	if (!bHasRotationTarget)
	{
		FRotator TargetRot = SmoothedDir.Rotation();
		FRotator CurrRot = GetActorRotation();
		FRotator NewRot = FMath::RInterpTo(CurrRot, TargetRot, DeltaTime, RotationSpeedDeg * 0.01f);
		NewRot.Pitch = 0.f;
		NewRot.Roll = 0.f;
		SetActorRotation(NewRot);
	}

	// stuck detection
	float MovedDist = (GetActorLocation() - LastLocation).Size();
	if (MovedDist < 1.5f)
	{
		StuckFrames++;
	}
	else
	{
		StuckFrames = 0;
	}
	LastLocation = GetActorLocation();

	if (StuckFrames > StuckFramesThreshold)
	{
		PickNewRandomGoal();
		StuckFrames = 0;
	}
}

void ACPP_NPC04::GenerateTraceDirections()
{
	TraceDirections.Empty();
	TraceDirections.Add(FVector(1, 0, 0));                     // forward
	TraceDirections.Add(FVector(1, 0.5f, 0).GetSafeNormal());  // forward-right small
	TraceDirections.Add(FVector(1, 1, 0).GetSafeNormal());     // forward-right
	TraceDirections.Add(FVector(0.5f, 1, 0).GetSafeNormal());  // right-forward
	TraceDirections.Add(FVector(0, 1, 0));                     // right
	TraceDirections.Add(FVector(-0.5f, 1, 0).GetSafeNormal()); // right-back
	TraceDirections.Add(FVector(-1, 1, 0).GetSafeNormal());    // back-right
	TraceDirections.Add(FVector(-1, 0, 0));                    // back
	TraceDirections.Add(FVector(-1, -1, 0).GetSafeNormal());   // back-left
	TraceDirections.Add(FVector(0, -1, 0));                    // left
	TraceDirections.Add(FVector(1, -1, 0).GetSafeNormal());    // forward-left
}

FVector ACPP_NPC04::ChooseBestDirection(const FVector& DesiredDir)
{
	if (!GetWorld()) return GetActorForwardVector();

	struct FEval { FVector WorldDir; int32 NoHitCount; bool bAnyHit; float MinHitDist; int32 Index; };
	TArray<FEval> EvalList;
	EvalList.Reserve(TraceDirections.Num());

	for (int32 i = 0; i < TraceDirections.Num(); ++i)
	{
		FVector LocalDir = TraceDirections[i].GetSafeNormal();
		FVector WorldDir = GetActorRotation().RotateVector(LocalDir).GetSafeNormal();

		int32 NoHit = 0;
		bool bAnyHit = false;
		float MinHit = TraceDistance;

		TArray<FVector> Samples;
		Samples.Add(WorldDir);
		Samples.Add((WorldDir + GetActorRightVector() * 0.25f).GetSafeNormal());
		Samples.Add((WorldDir - GetActorRightVector() * 0.25f).GetSafeNormal());

		int32 UseSamples = FMath::Clamp(SamplesPerDirection, 1, Samples.Num());

		for (int32 s = 0; s < UseSamples; ++s)
		{
			const FVector& Sdir = Samples[s];
			FHitResult Hit;
			FVector Start = GetActorFeetLocation();
			FVector End = Start + Sdir * TraceDistance;

			FCollisionQueryParams Params(SCENE_QUERY_STAT(AITrace), true, this);
			bool bHit = false;
			if (TraceRadius > KINDA_SMALL_NUMBER)
			{
				bHit = GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, TraceChannel, FCollisionShape::MakeSphere(TraceRadius), Params);
			}
			else
			{
				bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, TraceChannel, Params);
			}

			if (!bHit) NoHit++;
			else
			{
				bAnyHit = true;
				float Dist = (Hit.ImpactPoint - Start).Size();
				MinHit = FMath::Min(MinHit, Dist);
			}

			if (bDrawDebug && GetWorld())
			{
				DrawDebugLine(GetWorld(), Start, End, bHit ? FColor::Red : FColor::Green, false, 0.12f, 0, 1.0f);
				if (bHit) DrawDebugPoint(GetWorld(), Hit.ImpactPoint, 4.f, FColor::Yellow, false, 0.12f);
			}
		}

		EvalList.Add({ WorldDir, NoHit, bAnyHit, MinHit, i });
	}

	int32 MaxNoHit = -1;
	for (const auto& E : EvalList) MaxNoHit = FMath::Max(MaxNoHit, E.NoHitCount);

	FVector Best = DesiredDir.GetSafeNormal();
	const float NoHitWeight = 1000.f;
	const float DistWeight = 1.f;
	const float AngleWeight = TraceDistance * 0.6f;

	float BestScore = -FLT_MAX;
	for (const auto& E : EvalList)
	{
		float AngleDot = FVector::DotProduct(E.WorldDir, DesiredDir);
		float Score = E.NoHitCount * NoHitWeight + E.MinHitDist * DistWeight + AngleDot * AngleWeight;

		if (MaxNoHit == SamplesPerDirection && E.NoHitCount < SamplesPerDirection)
		{
			Score -= 5000.f;
		}

		if (Score > BestScore)
		{
			BestScore = Score;
			Best = E.WorldDir;
		}
	}

	if (Best.IsNearlyZero()) return GetActorForwardVector();
	return Best.GetSafeNormal();
}

void ACPP_NPC04::PickNewRandomGoal()
{
	FVector Origin = GetActorLocation();
	FVector2D Rand = FMath::RandPointInCircle(WanderRadius);
	CurrentGoal = Origin + FVector(Rand.X, Rand.Y, 0.f);

	if (bDrawDebug && GetWorld())
	{
		DrawDebugSphere(GetWorld(), CurrentGoal, 40.f, 12, FColor::Blue, false, 5.f);
	}
}

FVector ACPP_NPC04::GetActorFeetLocation() const
{
	FVector L = GetActorLocation();
	L.Z += 20.f;
	return L;
}

// --- Interaction methods ---

void ACPP_NPC04::FacePlayerAndStop(const FVector& PlayerLocation)
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

void ACPP_NPC04::ResumeMovement()
{
	// resume wandering
	bMovementEnabled = true;

	// set rotation target back to patrol orientation for smooth return
	FRotator DesiredPatrolRot = PatrolRotation;
	RotationTarget = DesiredPatrolRot;
	bHasRotationTarget = true;
}
