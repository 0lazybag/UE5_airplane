#include "CPP_NPC05.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "Kismet/KismetMathLibrary.h"
#include "CollisionQueryParams.h"

// forward include player character
#include "CPP_Character1.h"

ACPP_NPC05::ACPP_NPC05()
{
    PrimaryActorTick.bCanEverTick = true;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    CapsuleComp = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CapsuleComp"));
    CapsuleComp->SetupAttachment(Root);
    CapsuleComp->InitCapsuleSize(34.f, 88.f);
    CapsuleComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    CapsuleComp->SetCollisionObjectType(ECC_Pawn);
    CapsuleComp->SetCollisionResponseToAllChannels(ECR_Ignore);
    CapsuleComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    CapsuleComp->SetGenerateOverlapEvents(false);

    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    MeshComp->SetupAttachment(Root);
    MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MeshComp->SetGenerateOverlapEvents(false);

    CurrentIndex = 0;
    Direction = 1;

    // interaction defaults
    bMovementEnabled = true;
    bHasRotationTarget = false;
    StopDelayAfterTrace = 0.05f;
    LastInteractingCharacter = nullptr;
    DialogWidgetInstance = nullptr;

    // speed defaults
    CurrentSpeed = 0.f;
    ForwardSpeed = 0.f;
    RightSpeed = 0.f;
    PreviousLocation = FVector::ZeroVector;
}

void ACPP_NPC05::BeginPlay()
{
    Super::BeginPlay();

    if (PatrolPoints.Num() == 0)
    {
        PatrolPoints.Add(FVector(-650.f, 7200.f, 88.f));
        PatrolPoints.Add(FVector(-496.f, 4155.f, 88.f));
        PatrolPoints.Add(FVector(260.f, 4155.f, 88.f));
        PatrolPoints.Add(FVector(285.f, 3155.f, 88.f));
        PatrolPoints.Add(FVector(537.f, 2493.f, 88.f));
        PatrolPoints.Add(FVector(394.f, 1743.f, 88.f));
        PatrolPoints.Add(FVector(360.f, 1743.f, 88.f));
        PatrolPoints.Add(FVector(-1124.f, 1026.f, 88.f));
    }

    if (PatrolPoints.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("ACPP_NPC05: No patrol points set."));
    }
    else
    {
        CurrentIndex = 0;
        Direction = 1;
    }

    // initialize previous location for speed calc
    PreviousLocation = GetActorLocation();
}

FVector ACPP_NPC05::GetCurrentTarget() const
{
    if (PatrolPoints.Num() == 0) return GetActorLocation();
    return PatrolPoints[CurrentIndex];
}

void ACPP_NPC05::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // --- Update speeds for AnimBP ---
    if (DeltaTime > KINDA_SMALL_NUMBER)
    {
        FVector CurrentLocation = GetActorLocation();
        FVector Delta = CurrentLocation - PreviousLocation;

        // ignore vertical motion for animation speed (optional)
        FVector DeltaXY = Delta;
        DeltaXY.Z = 0.f;

        float NewSpeed = DeltaXY.Size() / DeltaTime;
        // optional smoothing:
        CurrentSpeed = FMath::FInterpTo(CurrentSpeed, NewSpeed, DeltaTime, 8.f);

        // Local (actor-space) speeds: forward = X, right = Y in UE coordinate
        FVector LocalDelta = GetActorRotation().UnrotateVector(DeltaXY);
        float NewForward = LocalDelta.X / DeltaTime;
        float NewRight = LocalDelta.Y / DeltaTime;
        ForwardSpeed = FMath::FInterpTo(ForwardSpeed, NewForward, DeltaTime, 8.f);
        RightSpeed = FMath::FInterpTo(RightSpeed, NewRight, DeltaTime, 8.f);

        PreviousLocation = CurrentLocation;
    }

    // Handle rotation target if set (FacePlayerAndStop or ResumeMovement)
    if (bHasRotationTarget)
    {
        FRotator CurrentRot = GetActorRotation();
        FRotator NewRot = FMath::RInterpConstantTo(CurrentRot, RotationTarget, DeltaTime, 180.f);
        SetActorRotation(NewRot);

        if (CurrentRot.Equals(RotationTarget, 0.5f))
        {
            SetActorRotation(RotationTarget);
            bHasRotationTarget = false;
        }
        if (!bMovementEnabled)
        {
            return;
        }
    }

    if (PatrolPoints.Num() == 0) return;

    FVector Avoid = ComputeAvoidanceVector();

    FVector Target = GetCurrentTarget();
    FVector ToTarget = (Target - GetActorLocation());
    ToTarget.Z = 0;
    float Dist = ToTarget.Size();

    if (Dist <= AcceptanceRadius)
    {
        int32 Next = CurrentIndex + Direction;
        if (Next < 0 || Next >= PatrolPoints.Num())
        {
            Direction *= -1;
            Next = CurrentIndex + Direction;
        }
        CurrentIndex = FMath::Clamp(Next, 0, PatrolPoints.Num() - 1);
        return;
    }

    if (!bMovementEnabled) return;

    FVector MoveDir = ToTarget.GetSafeNormal();
    if (MoveDir.IsNearlyZero()) return;

    if (!Avoid.IsNearlyZero())
    {
        FVector AvoidDir = Avoid.GetSafeNormal();
        float forwardDot = FVector::DotProduct(AvoidDir, MoveDir);
        if (FMath::Abs(forwardDot) < 0.5f)
        {
            MoveDir = (MoveDir * 0.4f + AvoidDir * 1.0f).GetSafeNormal();
        }
        else
        {
            MoveDir = (MoveDir + AvoidDir * 1.0f).GetSafeNormal();
        }
    }

    FVector NewLocation = GetActorLocation() + MoveDir * MoveSpeed * DeltaTime;
    SetActorLocation(NewLocation, true);

    FRotator DesiredRot = MoveDir.Rotation();
    DesiredRot.Pitch = 0;
    DesiredRot.Roll = 0;
    SetActorRotation(FMath::RInterpTo(GetActorRotation(), DesiredRot, DeltaTime, 5.f));
}

FVector ACPP_NPC05::ComputeAvoidanceVector()
{
    UWorld* World = GetWorld();
    if (!World) return FVector::ZeroVector;

    FVector Me = GetActorLocation();
    FVector Forward = GetActorForwardVector();
    Forward.Z = 0;
    Forward = Forward.GetSafeNormal();
    if (Forward.IsNearlyZero()) Forward = FVector::ForwardVector;

    float halfSpread = RaySpreadAngleDeg * 0.5f;
    int32 Count = FMath::Max(1, RayCount);

    FVector AccumAvoid = FVector::ZeroVector;
    float ClosestDist = FLT_MAX;
    FHitResult ClosestHit;

    FCollisionQueryParams qp(SCENE_QUERY_STAT(NPC05Trace), false, this);
    qp.bReturnPhysicalMaterial = false;
    qp.bTraceComplex = false;

    for (int32 i = 0; i < Count; ++i)
    {
        float t = (Count == 1) ? 0.5f : (float)i / (Count - 1);
        float angle = FMath::Lerp(-halfSpread, halfSpread, t);
        FRotator rot = FRotator(0.f, angle, 0.f);
        FVector Dir = rot.RotateVector(Forward).GetSafeNormal();

        FVector Start = Me + FVector(0, 0, 50.f);
        FVector End = Start + Dir * RayLength;

        FHitResult Hit;
        bool bHit = World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, qp);

        if (bDrawDebug)
        {
            DrawDebugLine(World, Start, End, bHit ? FColor::Red : FColor::Green, false, 0.1f, 0, 2.f);
            if (bHit) DrawDebugSphere(World, Hit.ImpactPoint, 6.f, 8, FColor::Yellow, false, 0.1f);
        }

        if (bHit)
        {
            float Dist = (Hit.ImpactPoint - Start).Size();
            if (Dist < ClosestDist)
            {
                ClosestDist = Dist;
                ClosestHit = Hit;
            }

            float Weight = FMath::Clamp(1.f - (Dist / RayLength), 0.f, 1.f);
            FVector Away = Hit.ImpactNormal;
            Away.Z = 0;
            Away = Away.GetSafeNormal();
            AccumAvoid += Away * Weight;
        }
    }

    if (ClosestDist < MinAvoidDistance && ClosestDist > 0.f)
    {
        FVector HitNormal = ClosestHit.ImpactNormal;
        HitNormal.Z = 0;
        HitNormal.Normalize();

        FVector SideA = FVector::CrossProduct(HitNormal, FVector::UpVector).GetSafeNormal();
        FVector SideB = -SideA;

        FVector ToTarget = (GetCurrentTarget() - Me);
        ToTarget.Z = 0;
        ToTarget.Normalize();

        float ScoreA = FVector::DotProduct(SideA, ToTarget);
        float ScoreB = FVector::DotProduct(SideB, ToTarget);
        FVector ChosenSide = (ScoreA >= ScoreB) ? SideA : SideB;

        float SideWeight = FMath::Clamp(1.f - (ClosestDist / MinAvoidDistance), 0.2f, 1.f);
        return ChosenSide * SideAvoidForce * SideWeight;
    }

    if (!AccumAvoid.IsNearlyZero())
    {
        return AccumAvoid.GetSafeNormal() * AvoidStrength;
    }

    return FVector::ZeroVector;
}

void ACPP_NPC05::FacePlayerAndStop(const FVector& PlayerLocation)
{
    bMovementEnabled = false;

    FVector ToPlayer = PlayerLocation - GetActorLocation();
    ToPlayer.Z = 0.f;
    if (ToPlayer.IsNearlyZero()) return;

    FRotator TargetRot = ToPlayer.Rotation();
    RotationTarget = FRotator(0.f, TargetRot.Yaw, 0.f);
    bHasRotationTarget = true;

    if (bDrawDebug)
    {
        UE_LOG(LogTemp, Log, TEXT("NPC05: FacePlayerAndStop called. TargetYaw=%f"), RotationTarget.Yaw);
    }
}

void ACPP_NPC05::ResumeMovement()
{
    bMovementEnabled = true;

    FVector NextTarget = GetCurrentTarget() - GetActorLocation();
    NextTarget.Z = 0.f;
    if (!NextTarget.IsNearlyZero())
    {
        RotationTarget = NextTarget.Rotation();
        RotationTarget.Pitch = 0.f;
        RotationTarget.Roll = 0.f;
        bHasRotationTarget = true;
    }

    if (bDrawDebug)
    {
        UE_LOG(LogTemp, Log, TEXT("NPC05: ResumeMovement called."));
    }
}
