#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CPP_NPC04.generated.h"

UCLASS()
class AIRPLANE_API ACPP_NPC04 : public AActor
{
	GENERATED_BODY()

public:
	ACPP_NPC04();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	// Components
	UPROPERTY(VisibleAnywhere, Category = "Components")
	class UCapsuleComponent* CapsuleComp;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	class UStaticMeshComponent* MeshComp;

	// Movement
	UPROPERTY(EditAnywhere, Category = "AI|Movement")
	float MoveSpeed = 800.f;

	UPROPERTY(EditAnywhere, Category = "AI|Movement")
	float RotationSpeedDeg = 720.f; // yaw speed multiplier for RInterp

	UPROPERTY(EditAnywhere, Category = "AI|Movement")
	float GoalAcceptanceRadius = 120.f;

	UPROPERTY(EditAnywhere, Category = "AI|Movement")
	float WanderRadius = 2000.f;

	// Sensing / traces
	UPROPERTY(EditAnywhere, Category = "AI|Sensing")
	float TraceDistance = 600.f;

	UPROPERTY(EditAnywhere, Category = "AI|Sensing")
	float TraceRadius = 30.f; // >0 uses sphere sweep

	UPROPERTY(EditAnywhere, Category = "AI|Sensing")
	bool bDrawDebug = true;

	UPROPERTY(EditAnywhere, Category = "AI|Sensing")
	int32 SamplesPerDirection = 3; // center, left, right (can increase)

	// Directions (local-space vectors; will be rotated by actor rotation)
	UPROPERTY(EditAnywhere, Category = "AI|Sensing")
	TArray<FVector> TraceDirections;

	// Trace channel
	UPROPERTY(EditAnywhere, Category = "AI|Collision")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_WorldStatic;

	// Exposed movement speeds for AnimBP
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
	float CurrentSpeed = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
	float ForwardSpeed = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
	float RightSpeed = 0.f;

	// Internal state
	FVector CurrentGoal;
	FVector SmoothedDir;
	int32 StuckFrames = 0;
	FVector LastLocation;

	// For speed calculation
	FVector PreviousLocation;

	// Helpers
	void GenerateTraceDirections();
	FVector ChooseBestDirection(const FVector& DesiredDir);
	void PickNewRandomGoal();
	FVector GetActorFeetLocation() const;

	// config
	UPROPERTY(EditAnywhere, Category = "AI|Movement")
	int32 StuckFramesThreshold = 15;

	// --- Interaction support (like NPC01/02/03) ---
public:
	// Called by player to make NPC face player and stop
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void FacePlayerAndStop(const FVector& PlayerLocation);

	// Called by player to resume movement
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void ResumeMovement();

protected:
	// Movement enabled flag and waiting state
	bool bMovementEnabled;
	bool bWaiting;
	float WaitTimer;
	UPROPERTY(EditAnywhere, Category = "AI|Movement")
	float WaitTimeAtEnd = 0.5f;

	// Rotation interpolation target
	bool bHasRotationTarget;
	FRotator RotationTarget;

	// Patrol/base rotation to return to
	FRotator PatrolRotation;
};
