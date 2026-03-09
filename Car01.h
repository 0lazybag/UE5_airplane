#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Car01.generated.h"

UENUM(BlueprintType)
enum class EWheelSpinAxis : uint8
{
	XAxis UMETA(DisplayName = "Local X (Pitch)"),
	YAxis UMETA(DisplayName = "Local Y (Yaw)"),
	ZAxis UMETA(DisplayName = "Local Z (Roll)")
};

UCLASS()
class AIRPLANE_API ACar01 : public AActor
{
	GENERATED_BODY()

public:
	ACar01();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

private:
	// movement
	UPROPERTY(EditAnywhere, Category = "Car|Simple")
	float TravelDistance = 1900.0f;

	UPROPERTY(EditAnywhere, Category = "Car|Simple")
	float Speed = 600.0f; // units per second

	UPROPERTY(EditAnywhere, Category = "Car|Simple")
	float StopDuration = 5.0f;

	// wheel properties
	UPROPERTY(EditAnywhere, Category = "Car|Wheels", meta = (ClampMin = "1.0"))
	float WheelRadius = 30.0f; // Unreal units (cm)

	UPROPERTY(EditAnywhere, Category = "Car|Wheels")
	EWheelSpinAxis WheelSpinAxis = EWheelSpinAxis::XAxis;

	// Components
	UPROPERTY(VisibleAnywhere, Category = "Car|Components")
	USceneComponent* Root;

	UPROPERTY(VisibleAnywhere, Category = "Car|Components")
	UStaticMeshComponent* Body;

	UPROPERTY(VisibleAnywhere, Category = "Car|Components")
	UStaticMeshComponent* WheelFL;

	UPROPERTY(VisibleAnywhere, Category = "Car|Components")
	UStaticMeshComponent* WheelFR;

	UPROPERTY(VisibleAnywhere, Category = "Car|Components")
	UStaticMeshComponent* WheelRL;

	UPROPERTY(VisibleAnywhere, Category = "Car|Components")
	UStaticMeshComponent* WheelRR;

	// Doors (can be replaced in Blueprint child)
	UPROPERTY(VisibleAnywhere, Category = "Car|Components")
	UStaticMeshComponent* DoorFL;

	UPROPERTY(VisibleAnywhere, Category = "Car|Components")
	UStaticMeshComponent* DoorFR;

	// door animation settings
	UPROPERTY(EditAnywhere, Category = "Car|Doors")
	float DoorOpenAngle = 75.0f; // degrees

	UPROPERTY(EditAnywhere, Category = "Car|Doors")
	float DoorOpenTime = 0.5f; // seconds to open (and to close)

	UPROPERTY(EditAnywhere, Category = "Car|Doors")
	float PauseBeforeOpen = 1.0f; // pause before starting to open

	UPROPERTY(EditAnywhere, Category = "Car|Doors")
	float PauseAfterClose = 1.0f; // pause after closing before moving

	// internal
	FVector StartLocation;
	FVector ForwardDir;
	int32 Direction; // +1 forward, -1 back
	bool bIsStopped;
	float StopTimer;

	// optional accumulator (keeps absolute spin if needed)
	float AccumulatedTravel;

	// door animation state
	bool bDoorsAnimating;
	float DoorAnimTimer;
	int32 DoorPhase; // 0 = waiting before open, 1 = opening, 2 = waiting after open (unused), 3 = closing, 4 = waiting after close
	FRotator DoorFL_StartRot;
	FRotator DoorFR_StartRot;
};
