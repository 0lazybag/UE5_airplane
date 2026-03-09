#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "airplane2.generated.h"

UCLASS()
class AIRPLANE_API Aairplane2 : public AActor
{
	GENERATED_BODY()

public:
	Aairplane2();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	// Настройки траектории
	UPROPERTY(EditAnywhere, Category = "Flight")
	float AmplitudeX = 1000.f;

	UPROPERTY(EditAnywhere, Category = "Flight")
	float AmplitudeY = 500.f;

	UPROPERTY(EditAnywhere, Category = "Flight")
	float FrequencyX = 0.5f; // Гц

	UPROPERTY(EditAnywhere, Category = "Flight")
	float FrequencyY = 1.0f; // Гц

	UPROPERTY(EditAnywhere, Category = "Flight", meta = (ClampMin = "0.0"))
	float SpeedMultiplier = 1.0f; // масштабирует "время" — > управляет скоростью

	UPROPERTY(EditAnywhere, Category = "Flight")
	float Phase = 0.f;

private:
	float TimeAccumulator = 0.f;
	FVector StartLocation;
};
