#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CPP_Character1.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UUserWidget;
class ACPP_NPC01;
class ACPP_NPC02;
class ACPP_NPC03;
class ACPP_NPC04;
class ACPP_NPC05;

UCLASS()
class AIRPLANE_API ACPP_Character1 : public ACharacter
{
	GENERATED_BODY()

public:
	ACPP_Character1();

protected:
	virtual void BeginPlay() override;

	// Movement input
	void MoveForward(float Value);
	void MoveRight(float Value);

	// Jump
	void StartJump();
	void StopJump();

	// Sprint
	void StartSprint();
	void StopSprint();

	// Mouse look
	void Turn(float Value);
	void LookUp(float Value);

	// Interaction
	void Interact();
	void FinishStopAndShowDialog();

	// Animation/state exposed to AnimBP
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Anim", meta = (AllowPrivateAccess = "true"))
	float Speed;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Anim", meta = (AllowPrivateAccess = "true"))
	bool bIsInAir;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Anim", meta = (AllowPrivateAccess = "true"))
	bool bIsSprinting;

	// New: explicit jump flag accessible to AnimBP
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Anim", meta = (AllowPrivateAccess = "true"))
	bool IsJumping2;

	// Movement config
	UPROPERTY(EditAnywhere, Category = "Movement")
	float WalkSpeed = 300.f;

	UPROPERTY(EditAnywhere, Category = "Movement")
	float RunSpeed = 600.f;

	// Mouse sensitivity (degrees per second)
	UPROPERTY(EditAnywhere, Category = "Camera")
	float TurnRate = 45.f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float LookUpRate = 45.f;

	// Interaction config
	UPROPERTY(EditAnywhere, Category = "Interaction")
	float InteractionDistance = 500.f;

	// Debug
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bDrawInteractionTrace = true;

	// Camera components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;

	// Widget classes
	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<UUserWidget> DialogWidgetClass;

	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<UUserWidget> DialogWidgetClass2;

	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<UUserWidget> DialogWidgetClass3;

	// Widget class for NPC04
	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<UUserWidget> DialogWidgetClass4;

	// Widget class for NPC05
	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<UUserWidget> DialogWidgetClass5;

	// Runtime instance
	UPROPERTY()
	UUserWidget* DialogWidgetInstance;

	// Interaction stop flow
	bool bInteractionStopping;
	FTimerHandle InteractionStopTimer;
	UPROPERTY()
	ACPP_NPC01* LastInteractedNPC;
	UPROPERTY()
	ACPP_NPC02* LastInteractedNPC02;
	UPROPERTY()
	ACPP_NPC03* LastInteractedNPC03;
	UPROPERTY()
	ACPP_NPC04* LastInteractedNPC04;
	UPROPERTY()
	ACPP_NPC05* LastInteractedNPC05;
	float StopDelayAfterTrace;

	// New flags
	bool bAwaitingUnfreezeOnMove;

	// Public method to force-unfreeze (callable from widget)
	UFUNCTION(BlueprintCallable)
	void ForceUnfreeze();

private:
	// internal helper
	void TryUnfreezeFromInput();

public:
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
};
