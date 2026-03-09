// CPP_NPC02.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CPP_NPC02.generated.h"

UCLASS()
class AIRPLANE_API ACPP_NPC02 : public AActor
{
	GENERATED_BODY()

public:
	ACPP_NPC02();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	// Called by player when traced/hit for interaction
	void FacePlayerAndStop(const FVector& PlayerLocation);
	void ResumeMovement();

protected:
	// Movement speed (units/sec)
	UPROPERTY(EditAnywhere, Category = "VacuumBot")
	float MoveSpeed = 300.f;

	// Degrees per second when turning toward target yaw
	UPROPERTY(EditAnywhere, Category = "VacuumBot")
	float TurnSpeed = 180.f;

	// Minimum and maximum random turn angle (degrees) when hitting obstacle
	UPROPERTY(EditAnywhere, Category = "VacuumBot")
	float MinTurnAngle = 90.f;

	UPROPERTY(EditAnywhere, Category = "VacuumBot")
	float MaxTurnAngle = 180.f;

	// Cooldown to avoid re-triggering OnHit repeatedly
	UPROPERTY(EditAnywhere, Category = "VacuumBot")
	float CollisionCooldown = 0.3f;

	// Dialog widget class (optional)
	UPROPERTY(EditAnywhere, Category = "Interaction")
	TSubclassOf<class UUserWidget> DialogWidgetClass;

private:
	// Collision component (root)
	UPROPERTY(VisibleAnywhere)
	class UBoxComponent* CollisionBox;

	// Optional visual mesh
	UPROPERTY(VisibleAnywhere)
	class UStaticMeshComponent* VisualMesh;

	// State
	bool bIsTurning = false;
	float TargetYaw = 0.f;
	float CollisionTimer = 0.f;

	// Interaction stop state
	bool bMovementEnabled = true;
	bool bHasRotationTarget = false;
	FRotator RotationTarget;
	FTimerHandle InteractionStopTimer;
	float StopDelayAfterTrace = 0.05f;

	// Last interacting character (to resume later)
	UPROPERTY()
	class ACPP_Character1* LastInteractingCharacter;

	// Runtime widget instance
	UPROPERTY()
	class UUserWidget* DialogWidgetInstance;

	// Handlers
	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);
};
