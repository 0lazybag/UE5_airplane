#include "CPP_Character1.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/PlayerController.h"
#include "Components/PrimitiveComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "TimerManager.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"

// NPC headers
#include "CPP_NPC01.h"
#include "CPP_NPC02.h"
#include "CPP_NPC03.h"
#include "CPP_NPC04.h"
#include "CPP_NPC05.h"

ACPP_Character1::ACPP_Character1()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	Speed = 0.f;
	bIsInAir = false;
	bIsSprinting = false;
	IsJumping2 = false;

	InteractionDistance = 500.f;
	DialogWidgetClass = nullptr;
	DialogWidgetClass2 = nullptr;
	DialogWidgetClass3 = nullptr;
	DialogWidgetClass4 = nullptr;
	DialogWidgetClass5 = nullptr;
	DialogWidgetInstance = nullptr;

	bInteractionStopping = false;
	LastInteractedNPC = nullptr;
	LastInteractedNPC02 = nullptr;
	LastInteractedNPC03 = nullptr;
	LastInteractedNPC04 = nullptr;
	LastInteractedNPC05 = nullptr;
	StopDelayAfterTrace = 0.05f;

	bAwaitingUnfreezeOnMove = false;

	// default debug flag true (can be toggled in editor)
	bDrawInteractionTrace = true;

	// Controller/rotation settings: do NOT apply controller pitch to Character (so mesh stays vertical)
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;  // keep yaw controlled by controller so character can turn horizontally
	bUseControllerRotationRoll = false;

	// Character movement: do not orient rotation to movement (we want yaw from controller)
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->bOrientRotationToMovement = false;
		GetCharacterMovement()->RotationRate = FRotator(0.f, 720.f, 0.f);
		GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	}

	// Camera boom (spring arm)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 300.f;
	CameraBoom->bUsePawnControlRotation = true; // let the boom use controller rotation (including pitch)

	// Camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false; // camera follows boom, not controller directly

	// Prevent mesh from inheriting controller pitch/roll
	if (GetMesh())
	{
		// Ensure mesh uses only yaw from owner; do not let controller pitch rotate mesh
		GetMesh()->SetOnlyOwnerSee(false);
		GetMesh()->bOwnerNoSee = false;
		// Typical UE mannequin offset — adjust as needed
		GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -90.f));
		GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	}

	// Load widget for NPC01
	static ConstructorHelpers::FClassFinder<UUserWidget> DialogWidgetBPClass(TEXT("/Game/Game/Dialog/WBP_Dialog.WBP_Dialog_C"));
	if (DialogWidgetBPClass.Succeeded())
	{
		DialogWidgetClass = DialogWidgetBPClass.Class;
	}

	// Load widget for NPC02
	static ConstructorHelpers::FClassFinder<UUserWidget> DialogWidgetBPClass2(TEXT("/Game/Game/Dialog02/WBP_Dialog02.WBP_Dialog02_C"));
	if (DialogWidgetBPClass2.Succeeded())
	{
		DialogWidgetClass2 = DialogWidgetBPClass2.Class;
	}

	// Load widget for NPC03
	static ConstructorHelpers::FClassFinder<UUserWidget> DialogWidgetBPClass3(TEXT("/Game/Game/Dialog03/WBP_Dialog03.WBP_Dialog03_C"));
	if (DialogWidgetBPClass3.Succeeded())
	{
		DialogWidgetClass3 = DialogWidgetBPClass3.Class;
	}

	// Load widget for NPC04
	static ConstructorHelpers::FClassFinder<UUserWidget> DialogWidgetBPClass4(TEXT("/Game/Game/Dialog04/WBP_Dialog04.WBP_Dialog04_C"));
	if (DialogWidgetBPClass4.Succeeded())
	{
		DialogWidgetClass4 = DialogWidgetBPClass4.Class;
	}

	// Load widget for NPC05
	static ConstructorHelpers::FClassFinder<UUserWidget> DialogWidgetBPClass5(TEXT("/Game/Game/Dialog05/WBP_Dialog05.WBP_Dialog05_C"));
	if (DialogWidgetBPClass5.Succeeded())
	{
		DialogWidgetClass5 = DialogWidgetBPClass5.Class;
	}
}

void ACPP_Character1::BeginPlay()
{
	Super::BeginPlay();
}

void ACPP_Character1::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const FVector Vel = GetVelocity();
	Speed = Vel.Size2D();
	bIsInAir = GetCharacterMovement() ? GetCharacterMovement()->IsFalling() : false;

	// If we've landed, ensure IsJumping2 is false
	if (!bIsInAir)
	{
		IsJumping2 = false;
	}
}

void ACPP_Character1::TryUnfreezeFromInput()
{
	if (!bAwaitingUnfreezeOnMove) return;

	bAwaitingUnfreezeOnMove = false;

	// Ensure movement enabled
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}

	// Resume NPCs
	if (LastInteractedNPC)
	{
		LastInteractedNPC->ResumeMovement();
		LastInteractedNPC = nullptr;
	}
	if (LastInteractedNPC02)
	{
		LastInteractedNPC02->ResumeMovement();
		LastInteractedNPC02 = nullptr;
	}
	if (LastInteractedNPC03)
	{
		LastInteractedNPC03->ResumeMovement();
		LastInteractedNPC03 = nullptr;
	}
	if (LastInteractedNPC04)
	{
		LastInteractedNPC04->ResumeMovement();
		LastInteractedNPC04 = nullptr;
	}
	if (LastInteractedNPC05)
	{
		LastInteractedNPC05->ResumeMovement();
		LastInteractedNPC05 = nullptr;
	}

	// Flush pressed keys so held keys start movement
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->FlushPressedKeys();
	}
}

void ACPP_Character1::MoveForward(float Value)
{
	// Prevent movement input while in forced stop phase
	if (bInteractionStopping) return;

	if (!FMath::IsNearlyZero(Value))
	{
		TryUnfreezeFromInput();
	}

	if (Controller == nullptr || FMath::IsNearlyZero(Value)) return;

	// If moving backwards or not forward input, force walk speed (no sprinting)
	if (Value <= 0.f)
	{
		if (GetCharacterMovement())
		{
			GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
		}
	}
	else // Value > 0.f (forward)
	{
		// Allow sprint only when moving forward
		if (bIsSprinting)
		{
			if (GetCharacterMovement())
			{
				GetCharacterMovement()->MaxWalkSpeed = RunSpeed;
			}
		}
		else
		{
			if (GetCharacterMovement())
			{
				GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
			}
		}
	}

	const FRotator YawRot(0.f, Controller->GetControlRotation().Yaw, 0.f);
	const FVector Dir = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
	AddMovementInput(Dir, Value);
}

void ACPP_Character1::MoveRight(float Value)
{
	if (bInteractionStopping) return;

	if (!FMath::IsNearlyZero(Value))
	{
		TryUnfreezeFromInput();
	}

	if (Controller == nullptr || FMath::IsNearlyZero(Value)) return;

	// Never allow sprinting sideways — force walk speed
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	}

	const FRotator YawRot(0.f, Controller->GetControlRotation().Yaw, 0.f);
	const FVector Dir = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);
	AddMovementInput(Dir, Value);
}

void ACPP_Character1::StartJump()
{
	TryUnfreezeFromInput();
	if (!bInteractionStopping)
	{
		IsJumping2 = true;
		Jump();
	}
}

void ACPP_Character1::StopJump()
{
	StopJumping();
}

void ACPP_Character1::StartSprint()
{
	TryUnfreezeFromInput();
	if (bInteractionStopping) return;
	bIsSprinting = true;

	if (GetCharacterMovement())
	{
		GetCharacterMovement()->MaxWalkSpeed = RunSpeed;
	}
}

void ACPP_Character1::StopSprint()
{
	bIsSprinting = false;
	if (GetCharacterMovement()) GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
}

void ACPP_Character1::Turn(float Value)
{
	if (FMath::IsNearlyZero(Value)) return;
	AddControllerYawInput(Value * TurnRate * GetWorld()->GetDeltaSeconds());
}

void ACPP_Character1::LookUp(float Value)
{
	if (FMath::IsNearlyZero(Value)) return;
	// Add pitch only to controller so spring arm + camera pitch, but not mesh
	AddControllerPitchInput(Value * LookUpRate * GetWorld()->GetDeltaSeconds());
}

void ACPP_Character1::Interact()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;

	// If dialog open -> close and resume
	if (DialogWidgetInstance)
	{
		// Remove widget
		DialogWidgetInstance->RemoveFromParent();
		DialogWidgetInstance = nullptr;

		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;

		// restore player movement immediately
		if (GetCharacterMovement())
		{
			GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		}

		// tell NPCs to resume
		if (LastInteractedNPC)
		{
			LastInteractedNPC->ResumeMovement();
			LastInteractedNPC = nullptr;
		}
		if (LastInteractedNPC02)
		{
			LastInteractedNPC02->ResumeMovement();
			LastInteractedNPC02 = nullptr;
		}
		if (LastInteractedNPC03)
		{
			LastInteractedNPC03->ResumeMovement();
			LastInteractedNPC03 = nullptr;
		}
		if (LastInteractedNPC04)
		{
			LastInteractedNPC04->ResumeMovement();
			LastInteractedNPC04 = nullptr;
		}
		if (LastInteractedNPC05)
		{
			LastInteractedNPC05->ResumeMovement();
			LastInteractedNPC05 = nullptr;
		}

		// Also set awaiting flag so first movement input unmasks everything
		bAwaitingUnfreezeOnMove = true;

		// Flush keys
		PC->FlushPressedKeys();

		return;
	}

	// perform trace from camera
	FVector Start;
	FRotator ViewRot;
	PC->GetPlayerViewPoint(Start, ViewRot);
	const FVector End = Start + (ViewRot.Vector() * InteractionDistance);

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);

	// Draw debug trace only when enabled
	if (bDrawInteractionTrace && GetWorld())
	{
#if WITH_EDITOR
		// Editor: keep existing appearance and longer lifetime for debugging in editor
		DrawDebugLine(GetWorld(), Start, End, bHit ? FColor::Green : FColor::Red, false, 2.f, 0, 1.f);
		if (bHit) DrawDebugSphere(GetWorld(), Hit.ImpactPoint, 8.f, 8, FColor::Yellow, false, 2.f);
#else
		// Runtime: optional shorter lifetime
		DrawDebugLine(GetWorld(), Start, End, bHit ? FColor::Green : FColor::Red, false, 0.1f, 0, 1.f);
		if (bHit) DrawDebugSphere(GetWorld(), Hit.ImpactPoint, 8.f, 8, FColor::Yellow, false, 0.1f);
#endif
	}

	if (bHit && Hit.GetActor())
	{
		// NPC handlers (same as before)
		if (ACPP_NPC01* HitNPC1 = Cast<ACPP_NPC01>(Hit.GetActor()))
		{
			HitNPC1->FacePlayerAndStop(GetActorLocation());
			bInteractionStopping = true;
			if (GetCharacterMovement())
			{
				GetCharacterMovement()->StopMovementImmediately();
				GetCharacterMovement()->DisableMovement();
			}
			if (PC) PC->FlushPressedKeys();
			LastInteractedNPC = HitNPC1;
			LastInteractedNPC02 = nullptr;
			LastInteractedNPC03 = nullptr;
			LastInteractedNPC04 = nullptr;
			LastInteractedNPC05 = nullptr;
			FTimerDelegate Del;
			Del.BindUObject(this, &ACPP_Character1::FinishStopAndShowDialog);
			GetWorldTimerManager().SetTimer(InteractionStopTimer, Del, StopDelayAfterTrace, false);
		}
		else if (ACPP_NPC02* HitNPC2 = Cast<ACPP_NPC02>(Hit.GetActor()))
		{
			HitNPC2->FacePlayerAndStop(GetActorLocation());
			bInteractionStopping = true;
			if (GetCharacterMovement())
			{
				GetCharacterMovement()->StopMovementImmediately();
				GetCharacterMovement()->DisableMovement();
			}
			if (PC) PC->FlushPressedKeys();
			LastInteractedNPC02 = HitNPC2;
			LastInteractedNPC = nullptr;
			LastInteractedNPC03 = nullptr;
			LastInteractedNPC04 = nullptr;
			LastInteractedNPC05 = nullptr;
			FTimerDelegate Del;
			Del.BindUObject(this, &ACPP_Character1::FinishStopAndShowDialog);
			GetWorldTimerManager().SetTimer(InteractionStopTimer, Del, StopDelayAfterTrace, false);
		}
		else if (ACPP_NPC03* HitNPC3 = Cast<ACPP_NPC03>(Hit.GetActor()))
		{
			HitNPC3->FacePlayerAndStop(GetActorLocation());
			bInteractionStopping = true;
			if (GetCharacterMovement())
			{
				GetCharacterMovement()->StopMovementImmediately();
				GetCharacterMovement()->DisableMovement();
			}
			if (PC) PC->FlushPressedKeys();
			LastInteractedNPC03 = HitNPC3;
			LastInteractedNPC = nullptr;
			LastInteractedNPC02 = nullptr;
			LastInteractedNPC04 = nullptr;
			LastInteractedNPC05 = nullptr;
			FTimerDelegate Del;
			Del.BindUObject(this, &ACPP_Character1::FinishStopAndShowDialog);
			GetWorldTimerManager().SetTimer(InteractionStopTimer, Del, StopDelayAfterTrace, false);
		}
		else if (ACPP_NPC04* HitNPC4 = Cast<ACPP_NPC04>(Hit.GetActor()))
		{
			HitNPC4->FacePlayerAndStop(GetActorLocation());
			bInteractionStopping = true;
			if (GetCharacterMovement())
			{
				GetCharacterMovement()->StopMovementImmediately();
				GetCharacterMovement()->DisableMovement();
			}
			if (PC) PC->FlushPressedKeys();
			LastInteractedNPC04 = HitNPC4;
			LastInteractedNPC = nullptr;
			LastInteractedNPC02 = nullptr;
			LastInteractedNPC03 = nullptr;
			LastInteractedNPC05 = nullptr;
			FTimerDelegate Del;
			Del.BindUObject(this, &ACPP_Character1::FinishStopAndShowDialog);
			GetWorldTimerManager().SetTimer(InteractionStopTimer, Del, StopDelayAfterTrace, false);
		}
		else if (ACPP_NPC05* HitNPC5 = Cast<ACPP_NPC05>(Hit.GetActor()))
		{
			HitNPC5->FacePlayerAndStop(GetActorLocation());
			bInteractionStopping = true;
			if (GetCharacterMovement())
			{
				GetCharacterMovement()->StopMovementImmediately();
				GetCharacterMovement()->DisableMovement();
			}
			if (PC) PC->FlushPressedKeys();
			LastInteractedNPC05 = HitNPC5;
			LastInteractedNPC = nullptr;
			LastInteractedNPC02 = nullptr;
			LastInteractedNPC03 = nullptr;
			LastInteractedNPC04 = nullptr;
			FTimerDelegate Del;
			Del.BindUObject(this, &ACPP_Character1::FinishStopAndShowDialog);
			GetWorldTimerManager().SetTimer(InteractionStopTimer, Del, StopDelayAfterTrace, false);
		}
	}
}

void ACPP_Character1::FinishStopAndShowDialog()
{
	bInteractionStopping = false;

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;

	// Remove any existing widget
	if (DialogWidgetInstance)
	{
		DialogWidgetInstance->RemoveFromParent();
		DialogWidgetInstance = nullptr;
	}

	// Choose which widget to show (priority: NPC04, NPC03, NPC02, NPC01, NPC05)
	TSubclassOf<UUserWidget> UseClass = nullptr;
	if (LastInteractedNPC04 && DialogWidgetClass4)
	{
		UseClass = DialogWidgetClass4;
	}
	else if (LastInteractedNPC03 && DialogWidgetClass3)
	{
		UseClass = DialogWidgetClass3;
	}
	else if (LastInteractedNPC02 && DialogWidgetClass2)
	{
		UseClass = DialogWidgetClass2;
	}
	else if (LastInteractedNPC && DialogWidgetClass)
	{
		UseClass = DialogWidgetClass;
	}
	else if (LastInteractedNPC05 && DialogWidgetClass5)
	{
		UseClass = DialogWidgetClass5;
	}

	if (UseClass)
	{
		DialogWidgetInstance = CreateWidget<UUserWidget>(PC, UseClass);
		if (DialogWidgetInstance)
		{
			DialogWidgetInstance->AddToViewport();

			FInputModeUIOnly InputMode;
			InputMode.SetWidgetToFocus(DialogWidgetInstance->TakeWidget());
			PC->SetInputMode(InputMode);
			PC->bShowMouseCursor = true;
		}
	}
}

void ACPP_Character1::ForceUnfreeze()
{
	// Called from widget when it closes itself
	// Remove widget if still present
	if (DialogWidgetInstance)
	{
		DialogWidgetInstance->RemoveFromParent();
		DialogWidgetInstance = nullptr;
	}

	// Ensure PC input mode restored
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
		PC->FlushPressedKeys();
	}

	// Immediately restore movement and resume NPCs
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}

	if (LastInteractedNPC)
	{
		LastInteractedNPC->ResumeMovement();
		LastInteractedNPC = nullptr;
	}
	if (LastInteractedNPC02)
	{
		LastInteractedNPC02->ResumeMovement();
		LastInteractedNPC02 = nullptr;
	}
	if (LastInteractedNPC03)
	{
		LastInteractedNPC03->ResumeMovement();
		LastInteractedNPC03 = nullptr;
	}
	if (LastInteractedNPC04)
	{
		LastInteractedNPC04->ResumeMovement();
		LastInteractedNPC04 = nullptr;
	}
	if (LastInteractedNPC05)
	{
		LastInteractedNPC05->ResumeMovement();
		LastInteractedNPC05 = nullptr;
	}

	// Clear interaction flags
	bInteractionStopping = false;
	bAwaitingUnfreezeOnMove = false;
}

void ACPP_Character1::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis("MoveForward", this, &ACPP_Character1::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &ACPP_Character1::MoveRight);

	PlayerInputComponent->BindAction("Space_Bar", IE_Pressed, this, &ACPP_Character1::StartJump);
	PlayerInputComponent->BindAction("Space_Bar", IE_Released, this, &ACPP_Character1::StopJump);

	PlayerInputComponent->BindAction("Left_Shift", IE_Pressed, this, &ACPP_Character1::StartSprint);
	PlayerInputComponent->BindAction("Left_Shift", IE_Released, this, &ACPP_Character1::StopSprint);

	PlayerInputComponent->BindAxis("Turn", this, &ACPP_Character1::Turn);
	PlayerInputComponent->BindAxis("LookUp", this, &ACPP_Character1::LookUp);

	PlayerInputComponent->BindAction("E", IE_Pressed, this, &ACPP_Character1::Interact);
}
