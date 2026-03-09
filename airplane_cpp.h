#pragma once
// Заголовочный guard для единичного включения файла

#include "CoreMinimal.h"               // Основные типы и макросы движка
#include "GameFramework/Pawn.h"        // Базовый класс APawn
#include "Camera/CameraComponent.h"    // Компонент камеры
#include "airplane_cpp.generated.h"    // Генерация кода Unreal Header Tool

// Класс самолёта: Pawn — управляемый игроком актор
UCLASS()
class AIRPLANE_API Aairplane_cpp : public APawn
{
	GENERATED_BODY()

public:
	// Конструктор: инициализация компонентов и значений по умолчанию
	Aairplane_cpp();

protected:
	// Вызывается при старте игры / спавне актора
	virtual void BeginPlay() override;

	// Вызывается каждый кадр; основная логика передвижения/физики
	virtual void Tick(float DeltaTime) override;

	// Привязка входных действий к методам
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

private:
	// Текущий процент тяги левого двигателя (0..100)
	float LeftThrustPercent;
	// Текущий процент тяги правого двигателя (0..100)
	float RightThrustPercent;

	// Индекс pitch (наклона), диапазон -4..4
	int32 PitchIndex;

	// --- Параметры полёта (настраиваемые в редакторе) ---
	// Максимальная сила тяги (усреднённая для обоих двигателей)
	UPROPERTY(EditAnywhere, Category = "Flight")
	float MaxThrust = 12500.f;

	// Скорость интерполяции изменения % тяги
	UPROPERTY(EditAnywhere, Category = "Flight")
	float ThrustResponse = 3.0f;

	// Коэффициент рыскания на единицу разницы % двигателей (градусы/сек на 1.0 разницы)
	UPROPERTY(EditAnywhere, Category = "Flight")
	float TurnYawFactor = 30.f;

	// Коэффициент сопротивления (drag)
	UPROPERTY(EditAnywhere, Category = "Flight")
	float DragCoefficient = 0.005f;

	// Пороговая скорость для появления подъёма (lift)
	UPROPERTY(EditAnywhere, Category = "Flight")
	float LiftSpeedThreshold = 400.f;

	// --- Параметры крена (roll) ---
	// Максимальный угол крена в градусах
	UPROPERTY(EditAnywhere, Category = "Flight|Roll")
	float MaxRollAngle = 25.f;

	// Скорость крена (градусы в секунду при удержании)
	UPROPERTY(EditAnywhere, Category = "Flight|Roll")
	float RollRate = 45.f;

	// Скорость интерполяции между текущим и целевым креном
	UPROPERTY(EditAnywhere, Category = "Flight|Roll")
	float RollInterpSpeed = 6.0f;

	// Целевой угол крена (рассчитывается по состоянию клавиш)
	float TargetRollAngle;
	// Текущий угол крена (интерполируется к TargetRollAngle)
	float CurrentRollAngle;

	// Карты степеней мощности: индекс 0 = текущая целевая мощность
	TArray<int32> LeftPowerMap;
	TArray<int32> RightPowerMap;

	// Камера компонента — вид от игрока
	UPROPERTY(VisibleAnywhere)
	UCameraComponent* CameraComp;

	// Обработчик нажатия цифровой клавиши (0..9) — устанавливает цель мощности / торможение
	void OnDigitPressed(int32 Digit);

	// Обработчик для изменения PitchIndex (Q..O)
	void OnPitchKeyPressed(int32 NewPitchIndex);

	// Обёртки для BindAction: отдельная функция на каждую цифру
	void OnDigit0(); void OnDigit1(); void OnDigit2(); void OnDigit3(); void OnDigit4(); void OnDigit5();
	void OnDigit6(); void OnDigit7(); void OnDigit8(); void OnDigit9();

	// Обёртки для клавиш pitch Q..O
	void OnPitch_Q(); void OnPitch_W(); void OnPitch_E(); void OnPitch_R(); void OnPitch_T();
	void OnPitch_Y(); void OnPitch_U(); void OnPitch_I(); void OnPitch_O();

	// Действия крена: нажатие/отпускание влево/вправо
	void OnRollLeftPressed();
	void OnRollLeftReleased();
	void OnRollRightPressed();
	void OnRollRightReleased();

	// Возвращает целевой процент левого двигателя (из карты или округлённый текущий)
	int32 GetTargetLeftPercent() const;

	// Возвращает целевой процент правого двигателя (из карты или округлённый текущий)
	int32 GetTargetRightPercent() const;

	// Флаг: находится ли самолёт на земле (простая эвристика)
	bool bOnGround;

	// Состояние удержания крена: -1 влево, 0 нет, +1 вправо
	int8 RollHoldState;

	// Сила торможения по земле (при запросе торможения)
	UPROPERTY(EditAnywhere, Category = "Flight")
	float GroundBrakeStrength = 8000.f;

	// Флаг запроса торможения; если true — применится в следующем Tick (и затем сбросится)
	bool bBrakingRequested;

	// Минимальный целевой процент, при котором разрешён подъём
	UPROPERTY(EditAnywhere, Category = "Flight")
	int32 MinPercentToLift = 30;

	// Верхняя граница "ground-only" режима; если цель <= этого числа — подъём запрещён
	UPROPERTY(EditAnywhere, Category = "Flight")
	int32 GroundOnlyMaxPercent = 20;
};