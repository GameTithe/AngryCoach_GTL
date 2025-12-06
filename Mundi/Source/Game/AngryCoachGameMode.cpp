#include "pch.h"
#include "AngryCoachGameMode.h"
#include "AngryCoachPlayerController.h"
#include "Character.h"
#include "World.h"
#include "CameraComponent.h"
#include "CameraActor.h"
#include "PlayerCameraManager.h"
#include "Source/Runtime/Core/Misc/PathUtils.h"

AAngryCoachGameMode::AAngryCoachGameMode()
{
	DefaultPawnClass = nullptr; // 직접 스폰할 거라 비활성화
	PlayerControllerClass = nullptr; // 직접 생성할 거라 비활성화
}

AAngryCoachGameMode::~AAngryCoachGameMode()
{
}

void AAngryCoachGameMode::StartPlay()
{
	Login();
	PostLogin(AngryCoachController);
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
		Player1 = Cast<ACharacter>(P1Actor);
		if (Player1)
		{
			Player1->SetActorLocation(FVector(0, -5, 2));
			Player1->PossessedBy(AngryCoachController);  // 컨트롤러 등록 (이동 활성화)
		}
	}

	// Player 2 스폰 (화살표 + RCtrl) - 프리팹으로 스폰
	FWideString P2PrefabPath = UTF8ToWide(GDataDir) + L"/Prefabs/BSHPlayer.prefab";
	if (AActor* P2Actor = GWorld->SpawnPrefabActor(P2PrefabPath))
	{
		Player2 = Cast<ACharacter>(P2Actor);
		if (Player2)
		{
			Player2->SetActorLocation(FVector(0, 5, 2));
			Player2->PossessedBy(AngryCoachController);  // 컨트롤러 등록 (이동 활성화)
		}
	}

	// 카메라 액터 스폰
	FTransform CameraTransform;
	CameraTransform.Translation = FVector(-5, 0, 3);
	GameCamera = GetWorld()->SpawnActor<ACameraActor>(CameraTransform);
	if (GameCamera)
	{
		// 두 캐릭터 중심을 바라보도록 초기 설정
		GameCamera->SetCameraPitch(-30.0f);
		GameCamera->SetCameraYaw(0.0f);

		// PlayerCameraManager에 등록
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
