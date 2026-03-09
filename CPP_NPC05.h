#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CPP_NPC05.generated.h"

UCLASS()
class AIRPLANE_API ACPP_NPC05 : public AActor
{
    GENERATED_BODY()

public:
    ACPP_NPC05();

protected:
    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaTime) override;

    // Components
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USceneComponent* Root;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* MeshComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class UCapsuleComponent* CapsuleComp;

    // Patrol points (editable)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrol")
    TArray<FVector> PatrolPoints;

    // Movement
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float MoveSpeed = 300.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float AcceptanceRadius = 50.f;

    // Ray sight
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sight")
    int32 RayCount = 5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sight")
    float RayLength = 200.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sight")
    float RaySpreadAngleDeg = 60.f;

    // Avoidance
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avoidance")
    float AvoidStrength = 250.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avoidance")
    float MinAvoidDistance = 150.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avoidance")
    float SideAvoidForce = 600.f;

    // Exposed movement speeds for AnimBP
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
    float CurrentSpeed;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
    float ForwardSpeed;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
    float RightSpeed;

    // Internal patrol state
    int32 CurrentIndex;
    int32 Direction; // +1 or -1

    // Helpers
    FVector GetCurrentTarget() const;
    void MoveTowards(const FVector& Target, float DeltaTime);
    FVector ComputeAvoidanceVector();

    // Debug
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool bDrawDebug = true;

    // Interaction API (used by player character)
    UFUNCTION(BlueprintCallable, Category = "Interaction")
    void FacePlayerAndStop(const FVector& PlayerLocation);

    UFUNCTION(BlueprintCallable, Category = "Interaction")
    void ResumeMovement();

private:
    // Movement / rotation control
    bool bMovementEnabled;
    bool bHasRotationTarget;
    FRotator RotationTarget;

    // Interaction bookkeeping
    FTimerHandle InteractionStopTimer;
    float StopDelayAfterTrace;

    UPROPERTY()
    class ACPP_Character1* LastInteractingCharacter;

    UPROPERTY()
    class UUserWidget* DialogWidgetInstance;

    // Previous location for speed calc
    FVector PreviousLocation;
};
