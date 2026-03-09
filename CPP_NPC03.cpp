#include "CPP_NPC03.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Kismet/KismetMathLibrary.h"
#include "GameFramework/Actor.h"
#include "CollisionQueryParams.h"

ACPP_NPC03::ACPP_NPC03()
{
	PrimaryActorTick.bCanEverTick = true;

	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	RootComponent = CollisionBox;
	CollisionBox->SetBoxExtent(FVector(36.f, 36.f, 48.f));
	CollisionBox->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	CollisionBox->SetNotifyRigidBodyCollision(true);
	CollisionBox->SetGenerateOverlapEvents(false);

	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(RootComponent);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualMesh->SetGenerateOverlapEvents(false);

	MoveSpeed = 260.f;
	RotationSpeed = 220.f;
	WanderRadius = 1000.f;
	WanderIntervalMin = 0.5f;
	WanderIntervalMax = 1.5f;
	AcceptRadius = 80.f;
	ObstacleDetectDistance = 160.f;
	StepHeightThreshold = 35.f;
	InvestigateTime = 1.2f;
	StepsBeforeIdle = 10;

	LastPosition = FVector::ZeroVector;
	TimeSinceLastStuckCheck = 0.f;
	StuckCheckInterval = 0.5f;
	StuckDistanceThreshold = 10.f;
	CurrentStuckAttempts = 0;
	StuckRecoverTurns = 2;

	bMovementEnabled = true;
	bHasRotationTarget = false;
	bBeingInteracted = false;
	HitCooldown = 0.12f;
	TimeSinceLastHit = HitCooldown;

	// recovery init
	bRecoveringForward = false;
	RecoverCheckTimer = 0.f;
	RecoverCheckDuration = 2.0f;
	RecoverTraceInterval = 0.12f;
	TimeSinceLastRecoverTrace = 0.f;

	bDrawDebugTraces = true;
}

void ACPP_NPC03::BeginPlay()
{
	Super::BeginPlay();

	WorldPtr = GetWorld();
	LastPosition = GetActorLocation();
	CurrentStuckAttempts = 0;
	SetState(ENPCState::Wander);

	if (CollisionBox)
	{
		CollisionBox->OnComponentHit.AddDynamic(this, &ACPP_NPC03::OnHit);
	}
}

void ACPP_NPC03::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!WorldPtr) return;

	TimeSinceLastHit += DeltaTime;

	if (bHasRotationTarget)
	{
		FRotator CurrentRot = GetActorRotation();
		FRotator NewRot = FMath::RInterpConstantTo(CurrentRot, RotationTarget, DeltaTime, RotationSpeed);
		SetActorRotation(NewRot);

		if (CurrentRot.Equals(RotationTarget, 0.5f))
		{
			SetActorRotation(RotationTarget);
			bHasRotationTarget = false;
			if (bBeingInteracted)
			{
				bMovementEnabled = false;
				return;
			}
		}
		if (!bMovementEnabled) return;
	}

	// Recovery routine (if active)
	if (TryRecoverForward(DeltaTime))
	{
		// recovered; continue normal processing
	}

	// Stuck detection (periodic)
	TimeSinceLastStuckCheck += DeltaTime;
	if (TimeSinceLastStuckCheck >= StuckCheckInterval)
	{
		float Dist = FVector::Dist2D(GetActorLocation(), LastPosition);
		if (Dist <= StuckDistanceThreshold)
		{
			if (!bRecoveringForward)
			{
				// start recovery scan
				bRecoveringForward = true;
				RecoverCheckTimer = 0.f;
				TimeSinceLastRecoverTrace = RecoverTraceInterval; // immediate trace
				FRotator R = GetActorRotation();
				SetActorRotation(R + FRotator(0.f, 30.f, 0.f));
			}
			else
			{
				// recovery already active and didn't resolve — fallback
				CurrentStuckAttempts++;
				if (CurrentStuckAttempts <= StuckRecoverTurns)
				{
					FRotator R = GetActorRotation();
					SetActorRotation(R + FRotator(0.f, 180.f, 0.f));
					TargetLocation = GetActorLocation() + GetActorForwardVector() * FMath::Clamp(WanderRadius * 0.25f, 150.f, WanderRadius);
					bRecoveringForward = false;
					RecoverCheckTimer = 0.f;
				}
				else
				{
					PickWanderTarget();
					CurrentStuckAttempts = 0;
					SetState(ENPCState::Wander);
					bRecoveringForward = false;
				}
			}
		}
		else
		{
			bRecoveringForward = false;
			RecoverCheckTimer = 0.f;
			CurrentStuckAttempts = 0;
		}

		LastPosition = GetActorLocation();
		TimeSinceLastStuckCheck = 0.f;
	}

	StateTimer += DeltaTime;

	switch (CurrentState)
	{
	case ENPCState::Wander:
	{
		if (!bMovementEnabled) break;

		FHitResult Hit;
		if (MultiProbeForwardObstacle(Hit))
		{
			float ObstacleZ = Hit.ImpactPoint.Z;
			float FeetZ = GetActorLocation().Z;
			if ((ObstacleZ - FeetZ) > StepHeightThreshold)
			{
				SetState(ENPCState::Investigate);
				return;
			}
			else
			{
				FRotator R = GetActorRotation();
				SetActorRotation(R + FRotator(0.f, 90.f, 0.f));
				TargetLocation = GetActorLocation() + GetActorForwardVector() * FMath::Clamp(WanderRadius * 0.35f, 200.f, WanderRadius);
			}
		}

		if (!PathClearTowards(TargetLocation))
		{
			PickWanderTarget();
			ChangeDirectionCount++;
		}

		MoveAlong(DeltaTime);

		float Dist2D = FVector::Dist2D(GetActorLocation(), TargetLocation);
		if (Dist2D <= AcceptRadius)
		{
			ChangeDirectionCount++;
			if (ChangeDirectionCount % StepsBeforeIdle == 0)
			{
				NextWanderDelay = FMath::FRandRange(WanderIntervalMin, WanderIntervalMax);
				StateTimer = 0.f;
			}
			else
			{
				PickWanderTarget();
			}
		}
	}
	break;

	case ENPCState::Investigate:
		if (StateTimer >= InvestigateTime)
		{
			PickWanderTarget();
			ChangeDirectionCount++;
			SetState(ENPCState::Wander);
		}
		break;
	}
}

void ACPP_NPC03::SetState(ENPCState NewState)
{
	CurrentState = NewState;
	StateTimer = 0.f;

	switch (NewState)
	{
	case ENPCState::Wander:
		if (!IsPointReachable(TargetLocation))
		{
			PickWanderTarget();
		}
		NextWanderDelay = FMath::FRandRange(WanderIntervalMin, WanderIntervalMax);
		break;

	case ENPCState::Investigate:
		NextWanderDelay = InvestigateTime;
		break;
	}
}

void ACPP_NPC03::PickWanderTarget()
{
	const int Attempts = 12;
	for (int i = 0; i < Attempts; ++i)
	{
		FVector Candidate = RandomPointInRadius(WanderRadius);
		if (IsPointReachable(Candidate))
		{
			TargetLocation = Candidate;
			return;
		}
	}

	TargetLocation = GetActorLocation() + GetActorForwardVector() * FMath::Clamp(WanderRadius * 0.4f, 200.f, WanderRadius);
}

bool ACPP_NPC03::IsPointReachable(const FVector& Point) const
{
	if (!WorldPtr) return false;

	FVector Start = GetActorLocation();
	Start.Z += 10.f;
	FVector End = Point;
	End.Z = Start.Z;

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(const_cast<ACPP_NPC03*>(this));

	FHitResult Hit;
	bool bHit = WorldPtr->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);

	if (bDrawDebugTraces && WorldPtr)
	{
		DrawDebugLine(WorldPtr, Start, End, bHit ? FColor::Red : FColor::Green, false, 1.2f, 0, 1.f);
	}

	if (bHit)
	{
		float ObstacleZ = Hit.ImpactPoint.Z;
		float FeetZ = Start.Z;
		if ((ObstacleZ - FeetZ) > StepHeightThreshold)
		{
			return false;
		}
	}

	FVector GroundCheckStart = Point + FVector(0.f, 0.f, 200.f);
	FVector GroundCheckEnd = Point - FVector(0.f, 0.f, 200.f);
	FCollisionQueryParams GroundParams;
	GroundParams.AddIgnoredActor(const_cast<ACPP_NPC03*>(this));
	FHitResult GroundHit;
	bool bGround = WorldPtr->LineTraceSingleByChannel(GroundHit, GroundCheckStart, GroundCheckEnd, ECC_Visibility, GroundParams);

	if (bDrawDebugTraces && WorldPtr)
	{
		DrawDebugLine(WorldPtr, GroundCheckStart, GroundCheckEnd, bGround ? FColor::Blue : FColor::Yellow, false, 1.2f, 0, 1.f);
	}

	return bGround;
}

bool ACPP_NPC03::MultiProbeForwardObstacle(FHitResult& OutHit) const
{
	if (!WorldPtr || !CollisionBox) return false;

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(const_cast<ACPP_NPC03*>(this));

	FVector BoxExtent = CollisionBox->GetUnscaledBoxExtent();
	FTransform BoxTransform = CollisionBox->GetComponentTransform();

	TArray<float> LocalYPositions = {
		static_cast<float>(-BoxExtent.Y),
		0.0f,
		static_cast<float>(BoxExtent.Y)
	};

	TArray<float> HeightFractions = { 0.25f, 0.5f, 0.75f };

	FVector Forward = BoxTransform.GetRotation().RotateVector(FVector::ForwardVector);
	FVector Right = BoxTransform.GetRotation().RotateVector(FVector::RightVector);
	FVector Up = BoxTransform.GetRotation().RotateVector(FVector::UpVector);

	FVector BoxCenter = BoxTransform.GetLocation();
	const float ForwardBias = 5.f;

	for (float FracZ : HeightFractions)
	{
		float LocalZ = -BoxExtent.Z + FracZ * (2.0f * BoxExtent.Z);

		for (float LocalY : LocalYPositions)
		{
			FVector Start = BoxCenter + Right * LocalY + Up * LocalZ;
			Start += Forward * ForwardBias;

			FVector End = Start + Forward * ObstacleDetectDistance;

			FHitResult Hit;
			bool bHit = WorldPtr->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);

			if (bDrawDebugTraces && WorldPtr)
			{
				DrawDebugLine(WorldPtr, Start, End, bHit ? FColor::Red : FColor::Green, false, 0.12f, 0, 1.5f);
				if (bHit) DrawDebugSphere(WorldPtr, Hit.ImpactPoint, 6.f, 6, FColor::Yellow, false, 0.12f);
			}

			if (bHit)
			{
				OutHit = Hit;
				return true;
			}
		}
	}

	return false;
}

bool ACPP_NPC03::PathClearTowards(const FVector& Point) const
{
	if (!WorldPtr) return true;

	FVector To = Point - GetActorLocation();
	To.Z = 0.f;
	if (To.IsNearlyZero()) return true;

	To.Normalize();

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(const_cast<ACPP_NPC03*>(this));

	{
		FVector Start = GetActorLocation() + FVector(0.f, 0.f, 25.f);
		FVector End = Start + To * ObstacleDetectDistance;
		FHitResult Hit;
		if (WorldPtr->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
			return false;
	}

	float SideAngle = 25.f;
	for (float Angle : { -SideAngle, SideAngle })
	{
		FRotator R = To.Rotation();
		FRotator Rot = R + FRotator(0.f, Angle, 0.f);
		FVector Dir = Rot.Vector();
		FVector Start = GetActorLocation() + FVector(0.f, 0.f, 25.f);
		FVector End = Start + Dir * ObstacleDetectDistance;
		FHitResult Hit;
		if (WorldPtr->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
			return false;
	}

	return true;
}

void ACPP_NPC03::MoveAlong(float DeltaTime)
{
	FVector To = TargetLocation - GetActorLocation();
	To.Z = 0.f;
	if (To.IsNearlyZero()) return;

	FVector Dir = To.GetSafeNormal();
	FRotator CurrentRot = GetActorRotation();
	FRotator TargetRot = Dir.Rotation();
	float NewYaw = FMath::FixedTurn(CurrentRot.Yaw, TargetRot.Yaw, RotationSpeed * DeltaTime);
	SetActorRotation(FRotator(CurrentRot.Pitch, NewYaw, CurrentRot.Roll));

	FVector Forward = GetActorForwardVector();
	FVector Prev = GetActorLocation();
	FVector NewLoc = Prev + Forward * MoveSpeed * DeltaTime;

	SetActorLocation(NewLoc, true);
	FVector After = GetActorLocation();

	if (FVector::Dist2D(Prev, After) < 1.f)
	{
		CurrentStuckAttempts++;
		// start recovery
		bRecoveringForward = true;
		RecoverCheckTimer = 0.f;
		TimeSinceLastRecoverTrace = RecoverTraceInterval;
		FRotator R = GetActorRotation();
		SetActorRotation(R + FRotator(0.f, 30.f, 0.f));
	}
}

FVector ACPP_NPC03::RandomPointInRadius(float Radius) const
{
	FVector Origin = GetActorLocation();
	FVector RandDir = UKismetMathLibrary::RandomUnitVector();
	float Dist = FMath::FRandRange(200.f, Radius);
	FVector Pt = Origin + RandDir * Dist;
	Pt.Z += FMath::FRandRange(-20.f, 20.f);
	return Pt;
}

void ACPP_NPC03::FacePlayerAndStop(const FVector& PlayerLocation)
{
	bMovementEnabled = false;
	bBeingInteracted = true;
	bHasRotationTarget = false;

	FVector ToPlayer = PlayerLocation - GetActorLocation();
	ToPlayer.Z = 0.f;
	if (ToPlayer.IsNearlyZero()) return;

	FRotator TargetRot = ToPlayer.Rotation();
	RotationTarget = FRotator(0.f, TargetRot.Yaw, 0.f);
	bHasRotationTarget = true;
}

void ACPP_NPC03::ResumeMovement()
{
	bMovementEnabled = true;
	bHasRotationTarget = false;
	bBeingInteracted = false;
}

void ACPP_NPC03::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{
	// Minimal: стартуем recovery при столкновении, без дополнительной логики
	if (!OtherActor || OtherActor == this) return;

	// дебаунс по времени ударов
	if (TimeSinceLastHit < HitCooldown) return;
	TimeSinceLastHit = 0.f;

	// Начать recovery-процесс: NPC будет сканировать поворотом и проверять фронтальную трассу
	bRecoveringForward = true;
	RecoverCheckTimer = 0.f;
	TimeSinceLastRecoverTrace = RecoverTraceInterval; // сразу сделать трасс
	// начать с малого поворота
	FRotator R = GetActorRotation();
	SetActorRotation(R + FRotator(0.f, 30.f, 0.f));
}

bool ACPP_NPC03::TryRecoverForward(float DeltaTime)
{
	if (!bRecoveringForward || !WorldPtr) return false;

	TimeSinceLastRecoverTrace += DeltaTime;

	// медленный поворот в процессе сканирования
	const float ScanTurnRateDegPerSec = 30.f;
	FRotator Current = GetActorRotation();
	FRotator Desired = Current + FRotator(0.f, ScanTurnRateDegPerSec * DeltaTime, 0.f);
	FRotator NewRot = FMath::RInterpConstantTo(Current, Desired, DeltaTime, ScanTurnRateDegPerSec);
	SetActorRotation(NewRot);

	// проверяем фронтальную трассу периодически
	if (TimeSinceLastRecoverTrace >= RecoverTraceInterval)
	{
		TimeSinceLastRecoverTrace = 0.f;

		FVector Start = GetActorLocation() + FVector(0.f, 0.f, 25.f);
		FVector End = Start + GetActorForwardVector() * ObstacleDetectDistance;

		FCollisionQueryParams Params;
		Params.AddIgnoredActor(this);

		FHitResult Hit;
		bool bHit = WorldPtr->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);

		if (bDrawDebugTraces && WorldPtr)
		{
			DrawDebugLine(WorldPtr, Start, End, bHit ? FColor::Red : FColor::Green, false, RecoverTraceInterval + 0.05f, 0, 1.5f);
		}

		if (!bHit)
		{
			// если фронт свободен — накапливаем время очистки
			RecoverCheckTimer += RecoverTraceInterval;
			if (RecoverCheckTimer >= RecoverCheckDuration)
			{
				// восстановление успешно — возвращаемся в обычный режим
				bRecoveringForward = false;
				RecoverCheckTimer = 0.f;
				TargetLocation = GetActorLocation() + GetActorForwardVector() * FMath::Clamp(WanderRadius * 0.2f, 150.f, WanderRadius);
				SetState(ENPCState::Wander);
				return true;
			}
		}
		else
		{
			// найдено препятствие — сбрасываем накопление времени
			RecoverCheckTimer = 0.f;
		}
	}

	return false;
}
