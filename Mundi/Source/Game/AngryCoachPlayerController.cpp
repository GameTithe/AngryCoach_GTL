#include "pch.h"
#include "AngryCoachPlayerController.h"
#include "AngryCoachCharacter.h"
#include "CameraComponent.h"
#include "CameraActor.h"
#include "InputManager.h"
#include "PlayerCameraManager.h" // APlayerCameraManager
#include "World.h"               // GWorld
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
	GameCamera->SetActorRotation(FVector{0.f, 30.f, 0.f});
}

void AAngryCoachPlayerController::SetGamePadVibration(bool bEnable, AAngryCoachCharacter* InPlayer, float VibTime)
{
	if (!InPlayer)
	{
		return;
	}

	if (Player1 == InPlayer)
	{
		GamePadIndex = 0;
		P1VibrationTime = VibTime;
		bEnableP1Vibration = bEnable;
	}
	else if (Player2 == InPlayer)
	{
		GamePadIndex = 1;
		P2VibrationTime = VibTime;
		bEnableP2Vibration = bEnable;
	}
}

void AAngryCoachPlayerController::Tick(float DeltaSeconds)
{
	// 부모의 Tick은 호출하지 않음 (기존 WASD 로직 무시)
	AActor::Tick(DeltaSeconds);

	// 게임플레이 입력이 비활성화된 경우 (UI 상태 등) 캐릭터 입력 처리 스킵
	UInputManager& InputManager = UInputManager::GetInstance();

	// 게임패드 자동 등록 (버튼 누르면 등록)
	InputManager.TryRegisterGamepadFromInput();
	if (!InputManager.IsGameplayInputEnabled())
	{
		// 카메라 위치는 계속 업데이트 (Select 화면에서도 카메라 동작 유지)
		UpdateCameraPosition(DeltaSeconds);
		return;
	}

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

	UpdateGamePadVibration(DeltaSeconds);
	if (bEnableP1Vibration && P1VibrationTime <= 0.0f)
	{
		bEnableP1Vibration = false;
		P1VibrationTime = 0.0f;
		GamePadIndex = 0;
		GamePadIndex = -1;
		InputManager.StopGamepadVibration(0);
	}
	if (bEnableP2Vibration && P2VibrationTime <= 0.0f)
	{
		bEnableP2Vibration = false;
		P2VibrationTime = 0.0f;
		GamePadIndex = 0;
		GamePadIndex = -1;
		InputManager.StopGamepadVibration(1);
	}
	if (bEnableP1Vibration)
	{
		P1VibrationTime -= DeltaSeconds;
	}

	if (bEnableP2Vibration)
	{
		P2VibrationTime -= DeltaSeconds;
	}

	// 카메라 위치 업데이트
	UpdateCameraPosition(DeltaSeconds);
}

void AAngryCoachPlayerController::ProcessPlayer1Input(float DeltaTime)
{
	if (!Player1) return;	

	UInputManager& InputManager = UInputManager::GetInstance();

	bool bIsTKeyDown = InputManager.IsKeyDown('T');
	bool bIsYKeyDown = InputManager.IsKeyDown('Y');
	if (Player1->IsGuard())
	{
		// 두 버튼 모두 떼야 가드 해제 (|| → &&)
		if (!bIsTKeyDown && !bIsYKeyDown)
		{
			Player1->StopGuard();
		}
		return;
	}

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

	// 춤 montage일 때는 예외적으로 input을 받자 
	if (!InputDir.IsZero() && ( !Player1->IsPlayingMontage() || Player1->GetIsDancing() ) )
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

		// 이동 적용 (GetVelocity는 속도 크기, DeltaTime은 AddMovementInput 내부에서 처리)
		Player1->AddMovementInput(WorldDir * Player1->GetVelocity(), 1.0f);
	}

	// I - 점프
	if (InputManager.IsKeyPressed('I'))
	{
		Player1->Jump();
	}

	// 공격
	if (bIsP1InputBuffering || Player1->CanAttack())
	{
		ProcessPlayer1Attack(DeltaTime);
	}
	
	// decal
	if (InputManager.IsKeyPressed('G'))
	{
		Player1->PaintPlayer1Decal();
	}

	// 춤 
	if (InputManager.IsKeyPressed('H'))
	{

		Player1->DancingCoach();	
	}
}

void AAngryCoachPlayerController::ProcessPlayer2Input(float DeltaTime)
{
	if (!Player2) return;

	UInputManager& InputManager = UInputManager::GetInstance();

	bool bIsNum1KeyDown = InputManager.IsKeyDown(VK_NUMPAD1);
	bool bIsNum2KeyDown = InputManager.IsKeyDown(VK_NUMPAD2);
	if (Player2->IsGuard())
	{
		// 두 버튼 모두 떼야 가드 해제 (|| → &&)
		if (!bIsNum1KeyDown && !bIsNum2KeyDown)
		{
			Player2->StopGuard();
		}
		return;
	}
	
	FVector InputDir = FVector::Zero();

	// 화살표 이동
	if (InputManager.IsKeyDown(VK_UP))    { InputDir.X += 1.0f; }
	if (InputManager.IsKeyDown(VK_DOWN))  { InputDir.X -= 1.0f; }
	if (InputManager.IsKeyDown(VK_RIGHT)) { InputDir.Y += 1.0f; }
	if (InputManager.IsKeyDown(VK_LEFT))  { InputDir.Y -= 1.0f; }

	if (!InputDir.IsZero() && ( !Player2->IsPlayingMontage() || Player2->GetIsDancing()))
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

		// 이동 적용 (GetVelocity는 속도 크기, DeltaTime은 AddMovementInput 내부에서 처리)
		Player2->AddMovementInput(WorldDir * Player2->GetVelocity(), 1.0f);
	}

	// Numpad6 - 점프
	if (InputManager.IsKeyPressed(VK_NUMPAD6))
	{
		Player2->Jump();
	}

	if (bIsP2InputBuffering || Player2->CanAttack())
	{
		ProcessPlayer2Attack(DeltaTime);
	}

	// decal
	if (InputManager.IsKeyPressed(VK_NUMPAD4))
	{
		Player2->PaintPlayer2Decal();
	}

	if (InputManager.IsKeyPressed(VK_NUMPAD5))
	{

		Player2->DancingCoach();
	}
}

void AAngryCoachPlayerController::UpdateCameraPosition(float DeltaTime)
{
	if (!Player1 || !Player2 || !GameCamera) return;
	if (Player1->GetHealthPercent() <= 0.f || Player2->GetHealthPercent() <= 0.f) return;

	// 두 캐릭터의 중심점 계산
	FVector P1Pos = Player1->GetActorLocation();
	FVector P2Pos = Player2->GetActorLocation();
	FVector CenterPos = (P1Pos + P2Pos) * 0.5f;

	// 두 캐릭터 사이 거리에 따라 카메라 거리 조절 (미터 단위)
	float Distance = FVector::Distance(P1Pos, P2Pos);
	float ZoomFactor = FMath::Max(1.0f, Distance / 10.0f);  // 10m 이상 떨어지면 줌아웃

	FVector TargetCameraPos = CenterPos + CameraOffset * ZoomFactor;

	// 현재 카메라 위치에서 목표 위치로 부드럽게 보간
	FVector CurrentPos = GameCamera->GetActorLocation();
	FVector NewPos = FMath::Lerp(CurrentPos, TargetCameraPos, CameraLerpSpeed * DeltaTime);
	GameCamera->SetActorLocation(NewPos);

	// ===== 포스트 프로세싱 적용  =====
	APlayerCameraManager* PlayerCameraManager = GWorld->GetPlayerCameraManager();
	if (PlayerCameraManager)
	{
		// 1. 동적 피사계 심도 (DOF)
		float DistToP1 = FVector::Distance(NewPos, P1Pos);
		float DistToP2 = FVector::Distance(NewPos, P2Pos);
		
		float MinPlayerDistToCam = FMath::Min(DistToP1, DistToP2);
		float MaxPlayerDistToCam = FMath::Max(DistToP1, DistToP2);

		// 초점 거리는 두 플레이어까지의 평균 거리로 설정
		float DofFocalDistance = (MinPlayerDistToCam + MaxPlayerDistToCam) / 2.0f;
		// 초점 영역은 두 플레이어의 거리를 커버하도록 설정 (최소값 보장)
		float DofFocalRegion = FMath::Max(5.f, (MaxPlayerDistToCam - MinPlayerDistToCam) * 2.f); // 1.2f는 약간의 여유

		float DofNearTransition = 0.1f;
		float DofFarTransition = 0.5f;

		// ZoomFactor에 따라 블러 강도 조절 (멀리 떨어지면 블러 약화, 가까우면 강화)
		float DofMaxNearBlur = 2.0f / FMath::Max(1.0f, ZoomFactor);
		float DofMaxFarBlur = 2.0f / FMath::Max(1.0f, ZoomFactor);
		
		PlayerCameraManager->StartDOF(
			DofFocalDistance, DofFocalRegion,
			DofNearTransition, DofFarTransition,
			DofMaxNearBlur, DofMaxFarBlur,
			0 // Priority
		);

		// 2. 동적 비네트 (Vignette)
		float CameraMovementDistance = (NewPos - CurrentPos).Size();
		float VignetteIntensity = FMath::Clamp(CameraMovementDistance / 0.1f, 0.0f, 0.8f);  
		float VignetteRadius = 0.5f;
		float VignetteSoftness = 1.f;
		float VignetteRoundness = 2.0f;
		FLinearColor VignetteColor = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f); // 검은색 비네트

		PlayerCameraManager->StartVignette(
			0.5f, // Duration
			VignetteRadius, VignetteSoftness,
			VignetteIntensity, VignetteRoundness,
			VignetteColor,
			0 // Priority
		);
	}
}

void AAngryCoachPlayerController::ProcessPlayer1Attack(float DeltaTime)
{
	UInputManager& InputManager = UInputManager::GetInstance();

	bool bIsTKeyDown = InputManager.IsKeyDown('T');
	bool bIsYKeyDown = InputManager.IsKeyDown('Y');

	// 점프 중 공격 → 점프 공격
	if (Player1->GetCurrentState() == ECharacterState::Jumping)
	{
		if (bIsTKeyDown || bIsYKeyDown)
		{
			FVector InputDir = FVector::Zero();
			if (InputManager.IsKeyDown('W')) InputDir.X += 1.0f;
			if (InputManager.IsKeyDown('S')) InputDir.X -= 1.0f;
			if (InputManager.IsKeyDown('D')) InputDir.Y += 1.0f;
			if (InputManager.IsKeyDown('A')) InputDir.Y -= 1.0f;

			Player1->OnJumpAttackInput(InputDir);
			ResetInputBuffer(bIsP1InputBuffering, P1PendingKey, P1InputBufferTime);
			return;
		}
		return;  // 점프 중엔 일반 공격 처리 안함
	}

	// 스킬은 즉발로 처리
	if (InputManager.IsKeyPressed('U'))
	{
		Player1->OnAttackInput(EAttackInput::Skill);
		ResetInputBuffer(bIsP1InputBuffering, P1PendingKey, P1InputBufferTime);
		return;
	}
	
	// painting 즉발로 처리
	if (InputManager.IsKeyPressed('G'))
	{
		ResetInputBuffer(bIsP1InputBuffering, P1PendingKey, P1InputBufferTime);
		return;
	}

	if (bIsP1InputBuffering)
	{
		P1InputBufferTime += DeltaTime;
		if (P1PendingKey == 'T' && bIsYKeyDown || P1PendingKey == 'Y' && bIsTKeyDown)
		{
			Player1->DoGuard();
			ResetInputBuffer(bIsP1InputBuffering, P1PendingKey, P1InputBufferTime);

			return;
		}

		if (P1InputBufferTime >= InputBufferLimit)
		{
			// T - 약공, Y - 강공, U - 스킬
			if (P1PendingKey == 'T')
			{
				Player1->OnAttackInput(EAttackInput::Light);
			}
			else if (P1PendingKey == 'Y')
			{
				Player1->OnAttackInput(EAttackInput::Heavy);
			}			

			ResetInputBuffer(bIsP1InputBuffering, P1PendingKey, P1InputBufferTime);
		}

		// 키를 눌렀다 바로 떼면 버퍼를 기다리지 않고 바로 공격
		if (P1PendingKey == 'T' && !bIsTKeyDown)
		{
			Player1->OnAttackInput(EAttackInput::Light);
			bIsP1InputBuffering = false;
		}
		else if (P1PendingKey == 'Y' && !bIsYKeyDown)
		{
			Player1->OnAttackInput(EAttackInput::Heavy);
			bIsP1InputBuffering = false;
		}
	}

	// 새로운 입력 발생 (대기 상태)
	else
	{
		if (bIsTKeyDown && bIsYKeyDown)
		{
			Player1->DoGuard();
		}
		else if(bIsTKeyDown)
		{
			bIsP1InputBuffering = true;
			P1InputBufferTime = 0.0f;
			P1PendingKey = 'T';
		}
		else if (bIsYKeyDown)
		{
			bIsP1InputBuffering = true;
			P1InputBufferTime = 0.0f;
			P1PendingKey = 'Y';
		}
	}
}

void AAngryCoachPlayerController::ProcessPlayer2Attack(float DeltaTime)
{
	UInputManager& InputManager = UInputManager::GetInstance();

	bool bIsNum1KeyDown = InputManager.IsKeyDown(VK_NUMPAD1);
	bool bIsNum2KeyDown = InputManager.IsKeyDown(VK_NUMPAD2);

	// 점프 중 공격 → 점프 공격
	if (Player2->GetCurrentState() == ECharacterState::Jumping)
	{
		if (bIsNum1KeyDown || bIsNum2KeyDown)
		{
			FVector InputDir = FVector::Zero();
			if (InputManager.IsKeyDown(VK_UP))    InputDir.X += 1.0f;
			if (InputManager.IsKeyDown(VK_DOWN))  InputDir.X -= 1.0f;
			if (InputManager.IsKeyDown(VK_RIGHT)) InputDir.Y += 1.0f;
			if (InputManager.IsKeyDown(VK_LEFT))  InputDir.Y -= 1.0f;

			Player2->OnJumpAttackInput(InputDir);
			ResetInputBuffer(bIsP2InputBuffering, P2PendingKey, P2InputBufferTime);
			return;
		}
		return;  // 점프 중엔 일반 공격 처리 안함
	}

	// 테스트용
	if (InputManager.IsKeyPressed('M'))
	{
		Player2->OnAttackInput(EAttackInput::Light);
		ResetInputBuffer(bIsP2InputBuffering, P2PendingKey, P2InputBufferTime);
		return;
	}

	// 스킬은 즉발로 처리
	if (InputManager.IsKeyPressed(VK_NUMPAD3))
	{
		Player2->OnAttackInput(EAttackInput::Skill);
		ResetInputBuffer(bIsP2InputBuffering, P2PendingKey, P2InputBufferTime);
		return;
	}

	if (bIsP2InputBuffering)
	{
		P2InputBufferTime += DeltaTime;
		if (P2PendingKey == VK_NUMPAD1 && bIsNum2KeyDown || P2PendingKey == VK_NUMPAD2 && bIsNum1KeyDown)
		{
			Player2->DoGuard();
			ResetInputBuffer(bIsP2InputBuffering, P2PendingKey, P2InputBufferTime);

			return;
		}

		if (P2InputBufferTime >= InputBufferLimit)
		{
			// Numpad1 - 약공, Numpad2 - 강공, Numpad3 - 스킬
			if (P2PendingKey == VK_NUMPAD1)
			{
				Player2->OnAttackInput(EAttackInput::Light);
			}
			else if (P2PendingKey == VK_NUMPAD2)
			{
				Player2->OnAttackInput(EAttackInput::Heavy);
			}			

			ResetInputBuffer(bIsP2InputBuffering, P2PendingKey, P2InputBufferTime);
		}

		// 키를 눌렀다 바로 떼면 버퍼를 기다리지 않고 바로 공격
		if (P2PendingKey == VK_NUMPAD1 && !bIsNum1KeyDown)
		{
			Player2->OnAttackInput(EAttackInput::Light);
			bIsP2InputBuffering = false;
		}
		else if (P2PendingKey == VK_NUMPAD2 && !bIsNum2KeyDown)
		{
			Player2->OnAttackInput(EAttackInput::Heavy);
			bIsP2InputBuffering = false;
		}
	}
	// 새로운 입력 발생 (대기 상태)
	else
	{
		if (bIsNum1KeyDown && bIsNum2KeyDown)
		{
			Player2->DoGuard();
		}
		else if(bIsNum1KeyDown)
		{
			bIsP2InputBuffering = true;
			P2InputBufferTime = 0.0f;
			P2PendingKey = VK_NUMPAD1;
		}
		else if (bIsNum2KeyDown)
		{
			bIsP2InputBuffering = true;
			P2InputBufferTime = 0.0f;
			P2PendingKey = VK_NUMPAD2;
		}
	}
}

void AAngryCoachPlayerController::ResetInputBuffer(bool& bIsInputBuffering, char& PendingKey, float& InputBufferTime)
{
	bIsInputBuffering = false;
	PendingKey = 0;
	InputBufferTime = 0.0f;
}

void AAngryCoachPlayerController::UpdateGamePadVibration(float DeltaTime)
{
	UInputManager& InputManager = UInputManager::GetInstance();
	
	if (GamePadIndex == 0 && bEnableP1Vibration)
	{
		InputManager.SetGamepadVibration(GamePadIndex, P1VibrationTime, P1VibrationTime);
	}

	if (GamePadIndex == 1 && bEnableP2Vibration)
	{
		InputManager.SetGamepadVibration(GamePadIndex, P2VibrationTime, P2VibrationTime);
	}
}
