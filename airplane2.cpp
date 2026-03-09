#include "airplane2.h"
#include "GameFramework/Actor.h"

// Sets default values
Aairplane2::Aairplane2()
{
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void Aairplane2::BeginPlay()
{
	Super::BeginPlay();
	StartLocation = GetActorLocation();
}

// Called every frame
void Aairplane2::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// масштабируем приращение времени скоростью
	TimeAccumulator += DeltaTime * FMath::Max(0.0f, SpeedMultiplier);

	const float TwoPi = 2.0f * PI;
	float X = AmplitudeX * FMath::Sin(TwoPi * FrequencyX * TimeAccumulator + Phase);
	float Y = AmplitudeY * FMath::Sin(TwoPi * FrequencyY * TimeAccumulator);

	FVector NewLocation = StartLocation + FVector(X, Y, 0.0f);
	SetActorLocation(NewLocation);

	// Ориентация вдоль движения (малый шаг)
	{
		float dt = 0.01f;
		float X2 = AmplitudeX * FMath::Sin(TwoPi * FrequencyX * (TimeAccumulator + dt) + Phase);
		float Y2 = AmplitudeY * FMath::Sin(TwoPi * FrequencyY * (TimeAccumulator + dt));
		FVector Next = StartLocation + FVector(X2, Y2, 0.0f);
		FVector Delta = (Next - NewLocation);
		if (!Delta.IsNearlyZero())
		{
			FRotator NewRot = Delta.Rotation();
			SetActorRotation(NewRot);
		}
	}
}
