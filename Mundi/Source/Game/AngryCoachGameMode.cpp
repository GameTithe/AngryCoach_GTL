#include "pch.h"
#include "AngryCoachGameMode.h"
#include "AngryCoachPlayerController.h"
#include "AngryCoachCharacter.h"
#include "World.h"
#include "CameraComponent.h"
#include "CameraActor.h"
#include "FAudioDevice.h"
#include "PlayerCameraManager.h"
#include "InputManager.h"
#include "Source/Runtime/Core/Misc/PathUtils.h"

AAngryCoachGameMode::AAngryCoachGameMode()
{
	DefaultPawnClass = nullptr; // 직접 스폰할 거라 비활성화
	PlayerControllerClass = nullptr; // 직접 생성할 거라 비활성화
	
	MainSound = RESOURCE.Load<USound>("Data/Audio/MAINSOUND.wav");
}

AAngryCoachGameMode::~AAngryCoachGameMode()
{
	FAudioDevice::StopAllSounds();
}

void AAngryCoachGameMode::StartPlay()
{
	Super::StartPlay();
	if (MainSound)
	{
		FAudioDevice::PlaySound3D(MainSound, {0,0, 0,}, 0.1f, true);
	}
}

APlayerController* AAngryCoachGameMode::Login()
{
	// AngryCoach 전용 컨트롤러 생성
	AngryCoachController = GetWorld()->SpawnActor<AAngryCoachPlayerController>(FTransform());
	PlayerController = AngryCoachController;
	return AngryCoachController;
}

void AAngryCoachGameMode::PostLogin(APlayerController* NewPlayer)
{
	// Player 1 스폰 (WASD + Space) - 프리팹으로 스폰
	FWideString P1PrefabPath = UTF8ToWide(GDataDir) + L"/Prefabs/ICGPlayer.prefab";
	if (AActor* P1Actor = GWorld->SpawnPrefabActor(P1PrefabPath))
	{
		Player1 = Cast<AAngryCoachCharacter>(P1Actor);
		if (Player1)
		{
			Player1->SetActorLocation(FVector(0, -2.5f, 2));
			Player1->SetActorRotation(FVector(0, 0, 90));  // Yaw=90: +Y 방향 (Player2를 향함)
			Player1->PossessedBy(AngryCoachController);
		}
	}

	// Player 2 스폰 (화살표 + RCtrl) - 프리팹으로 스폰
	FWideString P2PrefabPath = UTF8ToWide(GDataDir) + L"/Prefabs/BSHPlayer.prefab";
	if (AActor* P2Actor = GWorld->SpawnPrefabActor(P2PrefabPath))
	{
		Player2 = Cast<AAngryCoachCharacter>(P2Actor);
		if (Player2)
		{
			Player2->bIsCGC = false;
			Player2->SetActorLocation(FVector(0, 2.5f, 2));
			Player2->SetActorRotation(FVector(0, 0, -90));  // Yaw=-90: -Y 방향 (Player1을 향함)
			Player2->PossessedBy(AngryCoachController);
		}
	}

	// 카메라 액터 스폰
	FTransform CameraTransform;
	CameraTransform.Translation = FVector(-15, 0, 5);
	GameCamera = GetWorld()->SpawnActor<ACameraActor>(CameraTransform);
	if (GameCamera)
	{
		GameCamera->SetCameraPitch(-15.0f);
		GameCamera->SetCameraYaw(0.0f);

		if (auto* PCM = GWorld->GetPlayerCameraManager())
		{
			PCM->SetViewCamera(GameCamera->GetCameraComponent());
		}
	}

	// 컨트롤러에 두 캐릭터 + 카메라 등록
	if (AngryCoachController)
	{
		AngryCoachController->SetControlledCharacters(Player1, Player2);
		AngryCoachController->SetGameCamera(GameCamera);
	}
}

float AAngryCoachGameMode::GetP1HealthPercent() const
{
	if (Player1)
	{
		return Player1->GetHealthPercent();
	}
	return 0.0f;
}

float AAngryCoachGameMode::GetP2HealthPercent() const
{
	if (Player2)
	{
		return Player2->GetHealthPercent();
	}
	return 0.0f;
}

bool AAngryCoachGameMode::IsP1Alive() const
{
	if (Player1)
	{
		return Player1->IsAlive();
	}
	return false;
}

bool AAngryCoachGameMode::IsP2Alive() const
{
	if (Player2)
	{
		return Player2->IsAlive();
	}
	return false;
}

void AAngryCoachGameMode::ResetPlayersHP()
{
	if (Player1)
	{
		Player1->Revive();
		Player1->SetActorLocation(FVector(0, -5, 2));  // 초기 위치로 리셋
		Player1->SetActorRotation(FVector(0, 0, 0));   // 회전 리셋 (오일러 각도)
	}
	if (Player2)
	{
		Player2->Revive();
		Player2->SetActorLocation(FVector(0, 5, 2));   // 초기 위치로 리셋
		Player2->SetActorRotation(FVector(0, 0, 0));   // 회전 리셋 (오일러 각도)
	}
}
