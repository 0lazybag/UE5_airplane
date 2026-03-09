#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CPP_NPC03.generated.h"

UENUM(BlueprintType)
enum class ENPCState : uint8
{
	Wander,
	Investigate
};

UCLASS()
class AIRPLANE_API ACPP_NPC03 : public AActor
{
	GENERATED_BODY()

public:
	ACPP_NPC03();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	class UBoxComponent* CollisionBox;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	class UStaticMeshComponent* VisualMesh;

	// Movement
	UPROPERTY(EditAnywhere, Category = "NPC|Movement")
	float MoveSpeed = 260.f;

	UPROPERTY(EditAnywhere, Category = "NPC|Movement")
	float RotationSpeed = 220.f;

	// Wandering
	UPROPERTY(EditAnywhere, Category = "NPC|Wander")
	float WanderRadius = 1000.f;

	UPROPERTY(EditAnywhere, Category = "NPC|Wander")
	float WanderIntervalMin = 0.5f;

	UPROPERTY(EditAnywhere, Category = "NPC|Wander")
	float WanderIntervalMax = 1.5f;

	UPROPERTY(EditAnywhere, Category = "NPC|Wander")
	float AcceptRadius = 80.f;

	// Sensing / obstacle detection
	UPROPERTY(EditAnywhere, Category = "NPC|Sensing")
	float ObstacleDetectDistance = 160.f;

	UPROPERTY(EditAnywhere, Category = "NPC|Sensing")
	float StepHeightThreshold = 35.f;

	// Behavior
	UPROPERTY(EditAnywhere, Category = "NPC|Behavior")
	float InvestigateTime = 1.2f;

	UPROPERTY(EditAnywhere, Category = "NPC|Behavior")
	int32 StepsBeforeIdle = 10;

	// Debug
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDrawDebugTraces = true;

private:
	ENPCState CurrentState = ENPCState::Wander;

	FVector TargetLocation;
	float StateTimer = 0.f;
	float NextWanderDelay = 0.f;

	int32 ChangeDirectionCount = 0;

	UWorld* WorldPtr = nullptr;

	// Stuck detection
	FVector LastPosition;
	float StuckCheckInterval = 0.5f;
	float TimeSinceLastStuckCheck = 0.f;
	float StuckDistanceThreshold = 10.f; // cm
	int32 CurrentStuckAttempts = 0;
	int32 StuckRecoverTurns = 2;

	// Recovery forward attempt
	bool bRecoveringForward = false;
	float RecoverCheckTimer = 0.f;
	float RecoverCheckDuration = 2.0f; // seconds required clear forward
	float RecoverTraceInterval = 0.12f; // front trace interval
	float TimeSinceLastRecoverTrace = 0.f;

	// Helpers
	void SetState(ENPCState NewState);
	void PickWanderTarget();
	bool IsPointReachable(const FVector& Point) const;
	bool MultiProbeForwardObstacle(FHitResult& OutHit) const;
	bool PathClearTowards(const FVector& Point) const;
	void MoveAlong(float DeltaTime);
	FVector RandomPointInRadius(float Radius) const;

	// Interaction state
	bool bMovementEnabled = true;
	bool bHasRotationTarget = false;
	FRotator RotationTarget;

	// Hit cooldown
	float HitCooldown = 0.12f;
	float TimeSinceLastHit = 0.f;

	bool bBeingInteracted = false;

	// Recovery helper
	bool TryRecoverForward(float DeltaTime);

	// Handlers
	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);

public:
	void FacePlayerAndStop(const FVector& PlayerLocation);
	void ResumeMovement();
};
