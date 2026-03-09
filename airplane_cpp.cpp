// ------------------------- Реализация методов -------------------------

#include "airplane_cpp.h"                  // Заголовок класса
#include "Components/InputComponent.h"    // Для привязки входных действий
#include "Engine/Engine.h"                // Для GEngine и отладки на экране
#include "Engine/World.h"                 // Для доступа к миру
#include "GameFramework/PlayerController.h" // Для получения PlayerController
#include "Components/SceneComponent.h"    // Для RootComponent

// Конструктор: создаёт компоненты и задаёт начальные значения
Aairplane_cpp::Aairplane_cpp()
{
	PrimaryActorTick.bCanEverTick = true; // Включаем Tick для этого актора

	LeftThrustPercent = 0.f;  // Инициализация текущей тяги левого двигателя
	RightThrustPercent = 0.f; // Инициализация текущей тяги правого двигателя
	PitchIndex = 0;           // Начальный индекс pitch
	bOnGround = true;         // По умолчанию считаем, что актор на земле
	bBrakingRequested = false;// Торможение не запрошено

	// Инициализация карт мощностей (индекс 0 = текущая цель)
	LeftPowerMap = { 0,10,20,30,40,50,60,70,80,90,100 };
	RightPowerMap = LeftPowerMap; // Правая карта — копия левой

	TargetRollAngle = 0.f;   // Целевой крен: 0
	CurrentRollAngle = 0.f;  // Текущий крен: 0
	RollHoldState = 0;       // Нет удержания крена

	// Создаём корневой компонент сцены
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// Создаём и настраиваем компонент камеры
	CameraComp = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	CameraComp->SetupAttachment(RootComponent);                    // Прикрепляем к Root
	CameraComp->SetRelativeLocation(FVector(-300.f, 0.f, 100.f));  // Смещаем камеру назад и вверх
	CameraComp->bUsePawnControlRotation = false;                   // Камера не зависит от контроллера по умолчанию
}

// BeginPlay: вызывается при старте игры
void Aairplane_cpp::BeginPlay()
{
	Super::BeginPlay(); // Вызов базовой реализации

	// Если никто не захватил Pawn, то первый PlayerController попытается позаабсить его
	if (!GetController() && GetWorld())
	{
		APlayerController* PC = GetWorld()->GetFirstPlayerController(); // Получаем первый контроллер
		if (PC) PC->Possess(this); // Контроллер берёт управление этим Pawn
	}
}

// Tick: основной цикл логики — физика, тяга, подъём, крен, рыскание, торможение
void Aairplane_cpp::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime); // Вызов базового Tick

	// Получаем целевые проценты из карт (или текущие значения)
	const float TargetL = (float)GetTargetLeftPercent();   // Целевой % для левого двигателя
	const float TargetR = (float)GetTargetRightPercent();  // Целевой % для правого двигателя

	// Плавно интерполируем текущие проценты тяги к целевым
	LeftThrustPercent = FMath::FInterpTo(LeftThrustPercent, TargetL, DeltaTime, ThrustResponse);
	RightThrustPercent = FMath::FInterpTo(RightThrustPercent, TargetR, DeltaTime, ThrustResponse);

	// Ограничиваем проценты в пределах [0,100]
	LeftThrustPercent = FMath::Clamp(LeftThrustPercent, 0.f, 100.f);
	RightThrustPercent = FMath::Clamp(RightThrustPercent, 0.f, 100.f);

	// Рассчитываем суммарную продольную силу (усреднённая мощность -> сила)
	const float ForwardThrust = ((LeftThrustPercent + RightThrustPercent) * 0.5f) / 100.f * MaxThrust;
	FVector ForwardForce = GetActorForwardVector() * ForwardThrust; // Вектор силы вдоль forward

	// Получаем текущую скорость актора в мире
	FVector Velocity = GetVelocity();

	// Простейшее трение по земле (если на земле) — противоположно продольной компоненте скорости
	FVector GroundFriction = FVector::ZeroVector;
	if (bOnGround)
	{
		FVector ForwardVel = FVector::DotProduct(Velocity, GetActorForwardVector()) * GetActorForwardVector(); // Продольная компонента скорости
		float FrictionFactor = 0.5f; // Множитель трения для настройки
		GroundFriction = -ForwardVel * FrictionFactor; // Сила трения противоположна продольной скорости
	}

	// Простой линейный drag, масштабируется с силой тяги для ощущения сопротивления
	FVector Drag = -Velocity * DragCoefficient * FMath::Max(1.f, ForwardThrust / 1000.f);

	// Обработка запроса торможения: сильное торможение по земле
	if (bBrakingRequested && bOnGround)
	{
		// Продольная компонента скорости (вдоль forward)
		FVector ForwardVel = FVector::DotProduct(Velocity, GetActorForwardVector()) * GetActorForwardVector();
		// Направленная тормозная сила
		FVector BrakeForce = -ForwardVel.GetSafeNormal() * GroundBrakeStrength;

		// Если почти остановились — убираем продольную составляющую скорости полностью
		if (ForwardVel.Size() < 100.f)
		{
			FVector NewVel = Velocity - ForwardVel; // Оставшаяся (поперечная) составляющая скорости
			// Перемещаем актор на оставшуюся составляющую для симуляции мгновенной потери продольной скорости
			SetActorLocation(GetActorLocation() + NewVel * DeltaTime, true);
		}
		else
		{
			// Применяем смещение, учитывая тормозную силу, drag и тягу (упрощённая физика)
			AddActorWorldOffset((BrakeForce + Drag + ForwardForce + GroundFriction) * DeltaTime, true);
		}

		// Сбрасываем флаг запроса торможения — применилось один раз
		bBrakingRequested = false;

		// Отладочное сообщение на экран (жёлтые)
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::Yellow, TEXT("BRAKING"));
		}

		// Выход из Tick — остальная логика пропускается в этом кадре
		return;
	}

	// Простое определение "на земле": по высоте и медленной скорости
	if (GetActorLocation().Z < 120.f && Velocity.Size() < 200.f) bOnGround = true;
	else if (Velocity.Size() > LiftSpeedThreshold) bOnGround = false;

	// Решаем, разрешён ли подъём: средний целевой процент >= MinPercentToLift
	int32 AvgTargetPercent = (GetTargetLeftPercent() + GetTargetRightPercent()) / 2;
	bool bAllowLift = (AvgTargetPercent >= MinPercentToLift);

	// Если целевые проценты находятся в диапазоне "ground-only" — запрещаем подъём
	if (AvgTargetPercent <= GroundOnlyMaxPercent) bAllowLift = false;

	// Применяем подъём, если в воздухе и подъём разрешён
	if (!bOnGround && bAllowLift)
	{
		float LiftFactor = FMath::Clamp(Velocity.Size() / LiftSpeedThreshold, 0.f, 2.f); // Масштаб подъёма от скорости
		float Lift = LiftFactor * 600.f * DeltaTime; // Смещение вверх за кадр
		AddActorWorldOffset(FVector(0.f, 0.f, Lift), true); // Перемещаем актор вверх
	}

	// Перемещение актора по сумме сил: тяга + drag + трение по земле
	AddActorWorldOffset((ForwardForce + Drag + GroundFriction) * DeltaTime, true);

	// Дифференциальное рыскание: разница % двигателей порождает yaw
	float DiffPercent = (RightThrustPercent - LeftThrustPercent) / 100.f; // Разница в диапазоне [-1..1]
	float YawDegPerSec = DiffPercent * TurnYawFactor; // Градусы рыскания в секунду
	float YawThisFrame = YawDegPerSec * DeltaTime; // Градусы рыскания в этом кадре

	// Pitch рассчитывается из индекса: мягкая скорость поворота
	float PitchDegPerSec = (float)PitchIndex * 6.f; // Градусы pitch в секунду
	float PitchThisFrame = PitchDegPerSec * DeltaTime; // Pitch за текущий кадр

	// Обновляем целевой угол крена в зависимости от состояния удержания клавиш
	if (RollHoldState == -1) TargetRollAngle = -MaxRollAngle; // Держим влево
	else if (RollHoldState == 1) TargetRollAngle = MaxRollAngle; // Держим вправо
	else TargetRollAngle = 0.f; // Нет удержания — возвращаемся к 0

	// Плавная интерполяция текущего крена к целевому
	CurrentRollAngle = FMath::FInterpTo(CurrentRollAngle, TargetRollAngle, DeltaTime, RollInterpSpeed);
	// Вычисляем угол крена за кадр с учётом скорости RollRate
	float RollThisFrame = CurrentRollAngle * DeltaTime * (RollRate / MaxRollAngle);

	// Собираем дельта-вращение по осям (Pitch, Yaw, Roll)
	FRotator DeltaRot = FRotator(PitchThisFrame, YawThisFrame, RollThisFrame);
	// Применяем локальную ротацию актора
	AddActorLocalRotation(FQuat(DeltaRot));

	// Вывод отладочной информации на экран: L/R %, разница, скорость, pitch index, крен, onGround
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(1, 0.f, FColor::Green,
			FString::Printf(TEXT("L:%.1f R:%.1f Diff:%.2f V:%.0f PitchIdx:%d Roll:%.1f OnGround:%d"),
				LeftThrustPercent, RightThrustPercent, DiffPercent, Velocity.Size(), PitchIndex, (int)bOnGround));
	}
}

// SetupPlayerInputComponent: привязка входов к обработчикам
void Aairplane_cpp::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent); // Вызов базового метода
	if (!PlayerInputComponent) return; // Защита от nullptr

	// Привязки для цифровых клавиш 0..9
	PlayerInputComponent->BindAction("Digit0", IE_Pressed, this, &Aairplane_cpp::OnDigit0);
	PlayerInputComponent->BindAction("Digit1", IE_Pressed, this, &Aairplane_cpp::OnDigit1);
	PlayerInputComponent->BindAction("Digit2", IE_Pressed, this, &Aairplane_cpp::OnDigit2);
	PlayerInputComponent->BindAction("Digit3", IE_Pressed, this, &Aairplane_cpp::OnDigit3);
	PlayerInputComponent->BindAction("Digit4", IE_Pressed, this, &Aairplane_cpp::OnDigit4);
	PlayerInputComponent->BindAction("Digit5", IE_Pressed, this, &Aairplane_cpp::OnDigit5);
	PlayerInputComponent->BindAction("Digit6", IE_Pressed, this, &Aairplane_cpp::OnDigit6);
	PlayerInputComponent->BindAction("Digit7", IE_Pressed, this, &Aairplane_cpp::OnDigit7);
	PlayerInputComponent->BindAction("Digit8", IE_Pressed, this, &Aairplane_cpp::OnDigit8);
	PlayerInputComponent->BindAction("Digit9", IE_Pressed, this, &Aairplane_cpp::OnDigit9);

	// Привязки для pitch клавиш Q..O
	PlayerInputComponent->BindAction("PitchQ", IE_Pressed, this, &Aairplane_cpp::OnPitch_Q);
	PlayerInputComponent->BindAction("PitchW", IE_Pressed, this, &Aairplane_cpp::OnPitch_W);
	PlayerInputComponent->BindAction("PitchE", IE_Pressed, this, &Aairplane_cpp::OnPitch_E);
	PlayerInputComponent->BindAction("PitchR", IE_Pressed, this, &Aairplane_cpp::OnPitch_R);
	PlayerInputComponent->BindAction("PitchT", IE_Pressed, this, &Aairplane_cpp::OnPitch_T);
	PlayerInputComponent->BindAction("PitchY", IE_Pressed, this, &Aairplane_cpp::OnPitch_Y);
	PlayerInputComponent->BindAction("PitchU", IE_Pressed, this, &Aairplane_cpp::OnPitch_U);
	PlayerInputComponent->BindAction("PitchI", IE_Pressed, this, &Aairplane_cpp::OnPitch_I);
	PlayerInputComponent->BindAction("PitchO", IE_Pressed, this, &Aairplane_cpp::OnPitch_O);

	// Привязки для крена (нажатие/отпускание)
	PlayerInputComponent->BindAction("RollLeft", IE_Pressed, this, &Aairplane_cpp::OnRollLeftPressed);
	PlayerInputComponent->BindAction("RollLeft", IE_Released, this, &Aairplane_cpp::OnRollLeftReleased);
	PlayerInputComponent->BindAction("RollRight", IE_Pressed, this, &Aairplane_cpp::OnRollRightPressed);
	PlayerInputComponent->BindAction("RollRight", IE_Released, this, &Aairplane_cpp::OnRollRightReleased);
}

// OnDigitPressed: обработка нажатий цифр 0..9 — установка целевого процента или запрос торможения
void Aairplane_cpp::OnDigitPressed(int32 Digit)
{
	// Переводим цифру в процент (0->0, 1->10, ..., 9->90)
	int32 Percent = FMath::Clamp(Digit * 10, 0, 100);

	// Если нажали 0 — это запрос торможения
	if (Digit == 0)
	{
		// Устанавливаем целевой 0 в картах (если они есть)
		if (LeftPowerMap.Num() > 0) LeftPowerMap[0] = 0;
		if (RightPowerMap.Num() > 0) RightPowerMap[0] = 0;

		// Помечаем запрос торможения (применится в Tick, если на земле)
		bBrakingRequested = true;
		return;
	}

	// Для 1..9 — устанавливаем процент в индекс 0 карт
	if (LeftPowerMap.Num() > 0) LeftPowerMap[0] = Percent;
	if (RightPowerMap.Num() > 0) RightPowerMap[0] = Percent;

	// Если карты пусты — меняем текущие значения напрямую
	if (LeftPowerMap.Num() == 0) LeftThrustPercent = Percent;
	if (RightPowerMap.Num() == 0) RightThrustPercent = Percent;

	// Примечание: дополнительная логика "остаться на земле" реализована через параметры GroundOnlyMaxPercent
}

// OnPitchKeyPressed: установить индекс pitch (ограничено -4..4)
void Aairplane_cpp::OnPitchKeyPressed(int32 NewPitchIndex)
{
	PitchIndex = FMath::Clamp(NewPitchIndex, -4, 4); // Ограничиваем и сохраняем
}

// Обёртки для цифр вызывают общий обработчик с соответствующим аргументом
void Aairplane_cpp::OnDigit0() { OnDigitPressed(0); }
void Aairplane_cpp::OnDigit1() { OnDigitPressed(1); }
void Aairplane_cpp::OnDigit2() { OnDigitPressed(2); }
void Aairplane_cpp::OnDigit3() { OnDigitPressed(3); }
void Aairplane_cpp::OnDigit4() { OnDigitPressed(4); }
void Aairplane_cpp::OnDigit5() { OnDigitPressed(5); }
void Aairplane_cpp::OnDigit6() { OnDigitPressed(6); }
void Aairplane_cpp::OnDigit7() { OnDigitPressed(7); }
void Aairplane_cpp::OnDigit8() { OnDigitPressed(8); }
void Aairplane_cpp::OnDigit9() { OnDigitPressed(9); }

// Обёртки для pitch клавиш устанавливают соответствующие индексы
void Aairplane_cpp::OnPitch_Q() { OnPitchKeyPressed(-4); }
void Aairplane_cpp::OnPitch_W() { OnPitchKeyPressed(-3); }
void Aairplane_cpp::OnPitch_E() { OnPitchKeyPressed(-2); }
void Aairplane_cpp::OnPitch_R() { OnPitchKeyPressed(-1); }
void Aairplane_cpp::OnPitch_T() { OnPitchKeyPressed(0); }
void Aairplane_cpp::OnPitch_Y() { OnPitchKeyPressed(1); }
void Aairplane_cpp::OnPitch_U() { OnPitchKeyPressed(2); }
void Aairplane_cpp::OnPitch_I() { OnPitchKeyPressed(3); }
void Aairplane_cpp::OnPitch_O() { OnPitchKeyPressed(4); }

// Обработчики состояний крена: нажатие/отпускание влево/вправо
void Aairplane_cpp::OnRollLeftPressed() { RollHoldState = -1; }
void Aairplane_cpp::OnRollLeftReleased() { if (RollHoldState == -1) RollHoldState = 0; }
void Aairplane_cpp::OnRollRightPressed() { RollHoldState = 1; }
void Aairplane_cpp::OnRollRightReleased() { if (RollHoldState == 1) RollHoldState = 0; }

// GetTargetLeftPercent: возвращает целевой % левого двигателя (из карты или округлённый текущий)
int32 Aairplane_cpp::GetTargetLeftPercent() const
{
	if (LeftPowerMap.Num() > 0) return LeftPowerMap[0];
	return FMath::RoundToInt(LeftThrustPercent);
}

// GetTargetRightPercent: возвращает целевой % правого двигателя (из карты или округлённый текущий)
int32 Aairplane_cpp::GetTargetRightPercent() const
{
	if (RightPowerMap.Num() > 0) return RightPowerMap[0];
	return FMath::RoundToInt(RightThrustPercent);
}