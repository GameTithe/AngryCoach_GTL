#include "pch.h"
#include "TestGameMode.h"
#include "AngryCoachPlayerController.h"
#include "AngryCoachCharacter.h"
#include "World.h"
#include "PlayerCameraManager.h"
#include "CameraComponent.h"
#include "CameraActor.h"
#include "Source/Runtime/Core/Misc/PathUtils.h"
#include "Source/Runtime/InputCore/InputManager.h"

ATestGameMode::ATestGameMode()
{
	DefaultPawnClass = nullptr;
	PlayerControllerClass = nullptr;
}

ATestGameMode::~ATestGameMode()
{
}

void ATestGameMode::StartPlay()
{
	// GameState 초기화
	InitGameState();

	// 플레이어 로그인 처리 (컨트롤러 + 캐릭터 스폰)
	Login();
	PostLogin(PlayerController);

	// StartMatch() 호출 안함 - UI 흐름 없음

	// 게임플레이 입력 바로 활성화
	UInputManager::GetInstance().SetGameplayInputEnabled(true);

	UE_LOG("[TestGameMode] StartPlay - Characters spawned, no UI flow.");
}

APlayerController* ATestGameMode::Login()
{
	// AngryCoach 전용 컨트롤러 생성
	AngryCoachController = GetWorld()->SpawnActor<AAngryCoachPlayerController>(FTransform());
	PlayerController = AngryCoachController;
	return AngryCoachController;
}

void ATestGameMode::PostLogin(APlayerController* NewPlayer)
{
	// Player 1 스폰 (WASD + Space) - 프리팹으로 스폰
	FWideString P1PrefabPath = UTF8ToWide(GDataDir) + L"/Prefabs/ICGPlayer.prefab";
	if (AActor* P1Actor = GWorld->SpawnPrefabActor(P1PrefabPath))
	{
		Player1 = Cast<AAngryCoachCharacter>(P1Actor);
		if (Player1)
		{
			Player1->SetActorLocation(FVector(0, -5, 2));
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
			Player2->SetActorLocation(FVector(0, 5, 2));
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
