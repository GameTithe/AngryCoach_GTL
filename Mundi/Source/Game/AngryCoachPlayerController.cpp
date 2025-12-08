#include "pch.h"
#include "AngryCoachPlayerController.h"
#include "AngryCoachCharacter.h"
#include "CameraComponent.h"
#include "CameraActor.h"
#include "InputManager.h"
#include <cmath>

AAngryCoachPlayerController::AAngryCoachPlayerController()
{
}

AAngryCoachPlayerController::~AAngryCoachPlayerController()
{
}

void AAngryCoachPlayerController::SetControlledCharacters(AAngryCoachCharacter* InPlayer1, AAngryCoachCharacter* InPlayer2)
{
	Player1 = InPlayer1;
	Player2 = InPlayer2;
}

void AAngryCoachPlayerController::SetGameCamera(ACameraActor* InCamera)
{
	GameCamera = InCamera;
}

void AAngryCoachPlayerController::Tick(float DeltaSeconds)
{
	// 부모의 Tick은 호출하지 않음 (기존 WASD 로직 무시)
	AActor::Tick(DeltaSeconds);

	// 각 플레이어 입력 처리
	// 생존한 경우만 입력받음
	if (Player1->IsAlive())
	{
		ProcessPlayer1Input(DeltaSeconds);
	}
	if (Player2->IsAlive())
	{
		ProcessPlayer2Input(DeltaSeconds);
	}

	// 카메라 위치 업데이트
	UpdateCameraPosition(DeltaSeconds);
}

void AAngryCoachPlayerController::ProcessPlayer1Input(float DeltaTime)
{
	if (!Player1) return;	

	UInputManager& InputManager = UInputManager::GetInstance();
	FVector InputDir = FVector::Zero();

	// WASD 이동
	if (InputManager.IsKeyDown('W'))
	{
		InputDir.X += 1.0f;
	}
	if (InputManager.IsKeyDown('S'))
	{
		InputDir.X -= 1.0f;
	}
	if (InputManager.IsKeyDown('D'))
	{
		InputDir.Y += 1.0f;		
	}
	if (InputManager.IsKeyDown('A'))
	{
		InputDir.Y -= 1.0f;		
	}

	if (!InputDir.IsZero() && !Player1->IsPlayingMontage())
	{
		InputDir.Normalize();

		// 월드 기준 이동 (카메라 회전 무시, 고정 방향)
		FVector WorldDir = InputDir;
		WorldDir.Z = 0.0f;
		WorldDir.Normalize();

		// 이동 방향으로 캐릭터 회전
		float TargetYaw = std::atan2(WorldDir.Y, WorldDir.X) * (180.0f / PI);
		FQuat TargetRotation = FQuat::MakeFromEulerZYX(FVector(0.0f, 0.0f, TargetYaw));
		FQuat CurrentRotation = Player1->GetActorRotation();
		FQuat NewRotation = FQuat::Slerp(CurrentRotation, TargetRotation, FMath::Clamp(DeltaTime * 10.0f, 0.0f, 1.0f));
		Player1->SetActorRotation(NewRotation);

		// 이동 적용
		Player1->AddMovementInput(WorldDir * (Player1->GetVelocity() * DeltaTime));
	}

	// I - 점프
	if (InputManager.IsKeyPressed('I'))
	{
		Player1->Jump();
	}

	if (Player1->CanAttack())
	{
		ProcessPlayer1Attack(DeltaTime);
	}
}

void AAngryCoachPlayerController::ProcessPlayer2Input(float DeltaTime)
{
	if (!Player2) return;

	UInputManager& InputManager = UInputManager::GetInstance();
	FVector InputDir = FVector::Zero();

	// 화살표 이동
	if (InputManager.IsKeyDown(VK_UP))    { InputDir.X += 1.0f; }
	if (InputManager.IsKeyDown(VK_DOWN))  { InputDir.X -= 1.0f; }
	if (InputManager.IsKeyDown(VK_RIGHT)) { InputDir.Y += 1.0f; }
	if (InputManager.IsKeyDown(VK_LEFT))  { InputDir.Y -= 1.0f; }

	if (!InputDir.IsZero() && !Player2->IsPlayingMontage())
	{
		InputDir.Normalize();

		// 월드 기준 이동
		FVector WorldDir = InputDir;
		WorldDir.Z = 0.0f;
		WorldDir.Normalize();

		// 이동 방향으로 캐릭터 회전
		float TargetYaw = std::atan2(WorldDir.Y, WorldDir.X) * (180.0f / PI);
		FQuat TargetRotation = FQuat::MakeFromEulerZYX(FVector(0.0f, 0.0f, TargetYaw));
		FQuat CurrentRotation = Player2->GetActorRotation();
		FQuat NewRotation = FQuat::Slerp(CurrentRotation, TargetRotation, FMath::Clamp(DeltaTime * 10.0f, 0.0f, 1.0f));
		Player2->SetActorRotation(NewRotation);

		// 이동 적용
		Player2->AddMovementInput(WorldDir * (Player2->GetVelocity() * DeltaTime));
	}

	// Numpad6 - 점프
	if (InputManager.IsKeyPressed(VK_NUMPAD6))
	{
		Player2->Jump();
	}

	if (Player2->CanAttack())
	{
		ProcessPlayer2Attack(DeltaTime);
	}
}

void AAngryCoachPlayerController::UpdateCameraPosition(float DeltaTime)
{
	if (!Player1 || !Player2 || !GameCamera) return;

	// 두 캐릭터의 중심점 계산
	FVector P1Pos = Player1->GetActorLocation();
	FVector P2Pos = Player2->GetActorLocation();
	FVector CenterPos = (P1Pos + P2Pos) * 0.5f;

	// 두 캐릭터 사이 거리에 따라 카메라 거리 조절 (미터 단위)
	float Distance = FVector::Distance(P1Pos, P2Pos);
	float ZoomFactor = FMath::Max(1.0f, Distance / 5.0f);  // 5m 이상 떨어지면 줌아웃

	FVector TargetCameraPos = CenterPos + CameraOffset * ZoomFactor;

	// 현재 카메라 위치에서 목표 위치로 부드럽게 보간
	FVector CurrentPos = GameCamera->GetActorLocation();
	FVector NewPos = FMath::Lerp(CurrentPos, TargetCameraPos, CameraLerpSpeed * DeltaTime);
	GameCamera->SetActorLocation(NewPos);
}

void AAngryCoachPlayerController::ProcessPlayer1Attack(float DeltaTime)
{
	UInputManager& InputManager = UInputManager::GetInstance();
	// T - 약공, Y - 강공, U - 스킬
	if (InputManager.IsKeyPressed('T'))
	{
		Player1->OnAttackInput(EAttackInput::Light);
	}
	if (InputManager.IsKeyPressed('Y'))
	{
		Player1->OnAttackInput(EAttackInput::Heavy);
	}
	if (InputManager.IsKeyPressed('U'))
	{
		Player1->OnAttackInput(EAttackInput::Skill);
	}
}

void AAngryCoachPlayerController::ProcessPlayer2Attack(float DeltaTime)
{
	UInputManager& InputManager = UInputManager::GetInstance();
	// Numpad1 - 약공, Numpad2 - 강공, Numpad3 - 스킬
	if (InputManager.IsKeyPressed(VK_NUMPAD1))
	{
		Player2->OnAttackInput(EAttackInput::Light);
	}
	if (InputManager.IsKeyPressed(VK_NUMPAD2))
	{
		Player2->OnAttackInput(EAttackInput::Heavy);
	}
	if (InputManager.IsKeyPressed(VK_NUMPAD3))
	{
		Player2->OnAttackInput(EAttackInput::Skill);
	}
}
