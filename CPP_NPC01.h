#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CPP_NPC01.generated.h"

UCLASS()
class AIRPLANE_API ACPP_NPC01 : public AActor
{
	GENERATED_BODY()
	
public:	
	ACPP_NPC01();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;

	// Mesh to set in Blueprint
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	UStaticMeshComponent* MeshComp;

	// Movement settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPC|Movement")
	float MoveSpeed = 200.f; // cm/s

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPC|Movement")
	float MoveDistance = 800.f; // cm

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPC|Movement")
	float WaitTimeAtEnd = 0.5f; // seconds pause at endpoints

	// Turning interpolation speed (degrees/sec)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPC|Rotation")
	float TurnInterpSpeed = 360.f;

	// Called by player to make NPC face player and stop
	void FacePlayerAndStop(const FVector& PlayerLocation);

	// Called by player to resume patrol
	void ResumeMovement();

private:
	// Patrol internal state
	FVector StartLocation;
	FVector TargetLocation;
	int32 MoveDirection; // +1 forward, -1 back
	bool bWaiting;
	float WaitTimer;

	bool bMovementEnabled;

	// Rotation interpolation
	bool bHasRotationTarget;
	FRotator RotationTarget;

	// original patrol base rotation
	FRotator PatrolRotation;
};
