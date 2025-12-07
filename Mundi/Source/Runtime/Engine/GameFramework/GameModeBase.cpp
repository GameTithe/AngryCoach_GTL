#include "pch.h"
#include "GameModeBase.h"
#include "GameStateBase.h"
#include "PlayerController.h"
#include "Pawn.h"
#include "World.h"
#include "CameraComponent.h"
#include "SpringArmComponent.h"
#include "PlayerCameraManager.h"
#include "Character.h"
#include "Source/Runtime/Core/Misc/PathUtils.h"
#include "LuaScriptComponent.h"
#include "Source/Game/Cutscene/IntroCutscene.h"
#include "Source/Runtime/InputCore/InputManager.h"

AGameModeBase::AGameModeBase()
{
	DefaultPawnClass = ACharacter::StaticClass();
	PlayerControllerClass = APlayerController::StaticClass();
	GameStateClass = AGameStateBase::StaticClass();

	bCanEverTick = true;
}

AGameModeBase::~AGameModeBase()
{
	if (IntroCutscene)
	{
		delete IntroCutscene;
		IntroCutscene = nullptr;
	}
}

void AGameModeBase::StartPlay()
{
	// GameState 초기화
	InitGameState();

	// Lua 스크립트 컴포넌트 찾기
	FindLuaScriptComponent();

	// 플레이어 로그인 처리
	Login();
	PostLogin(PlayerController);

	// 매치 시작
	StartMatch();
}

void AGameModeBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!GameState)
	{
		return;
	}

	// 인트로 컷신 업데이트
	if (IntroCutscene && !IntroCutscene->IsFinished())
	{
		IntroCutscene->Update(DeltaTime);
		return;
	}

	// 카운트다운 처리
	if (bIsCountingDown)
	{
		UpdateCountDown(DeltaTime);
		return;
	}

	// 라운드 진행 중일 때
	if (GameState->GetRoundState() == ERoundState::InProgress)
	{
		// 라운드 시간 업데이트
		UpdateRoundTimer(DeltaTime);

		// 승리 조건 체크
		int32 RoundWinner = CheckRoundWinCondition();
		if (RoundWinner != -1)  // -1이 아니면 승자가 결정됨 (0,1=승자, -2=무승부)
		{
			EndRound(RoundWinner == -2 ? -1 : RoundWinner);
		}
	}
}

// ============================================
// 게임 흐름 제어
// ============================================

void AGameModeBase::StartMatch()
{
	if (!GameState)
	{
		return;
	}

	GameState->ResetGameState();
	GameState->SetGameState(EGameState::InProgress);

	// Lua 콜백: OnMatchStart
	CallLuaCallback("OnMatchStart");

	// 첫 번째 라운드 준비
	GameState->AdvanceToNextRound();

	// 인트로 컷신으로 시작 (최초 1회)
	StartIntro();
}

void AGameModeBase::EndMatch()
{
	if (!GameState)
	{
		return;
	}

	GameState->SetGameState(EGameState::GameOver);

	// Lua 콜백: OnMatchEnd
	CallLuaCallback("OnMatchEnd");
}

void AGameModeBase::RestartMatch()
{
	if (!GameState)
	{
		return;
	}

	UE_LOG("[GameMode] RestartMatch - Resetting game state and starting new match\n");

	// GameState 초기화 (라운드, 승리 횟수 등)
	GameState->ResetGameState();
	GameState->SetGameState(EGameState::InProgress);

	// 첫 번째 라운드 준비
	GameState->AdvanceToNextRound();

	// Lua 콜백: OnMatchRestart
	CallLuaCallback("OnMatchRestart");

	// 캐릭터 선택으로 바로 이동 (Intro 스킵)
	StartCharacterSelect();
}

void AGameModeBase::StartIntro()
{
	if (!GameState)
	{
		return;
	}

	GameState->SetRoundState(ERoundState::Intro);

	// 인트로 중에는 게임플레이 입력 비활성화
	UInputManager::GetInstance().SetGameplayInputEnabled(false);

	// 인트로 컷신 시작 (C++ 클래스로 처리)
	if (IntroCutscene)
	{
		delete IntroCutscene;
	}
	IntroCutscene = new UIntroCutscene();
	IntroCutscene->SetOnFinished([](void* InOwner)
	{
		// 컷신 완료 시 EndIntro 호출
		if (AGameModeBase* GM = Cast<AGameModeBase>(static_cast<UObject*>(InOwner)))
		{
			GM->EndIntro();
		}
	}, this);

	if (!IntroCutscene->Start())
	{
		UE_LOG("[GameMode] IntroCutscene failed to start, skipping intro...\n");
		// 시작 실패 시 바로 EndIntro (콜백에서 이미 호출됨)
	}

	// Lua 콜백: OnIntroStart (옵션 - Lua에서 추가 처리 가능)
	CallLuaCallback("OnIntroStart");
}

void AGameModeBase::EndIntro()
{
	if (!GameState)
	{
		return;
	}

	// 인트로 컷신 정리
	if (IntroCutscene)
	{
		delete IntroCutscene;
		IntroCutscene = nullptr;
	}

	// Lua 콜백: OnIntroEnd
	CallLuaCallback("OnIntroEnd");

	// 인트로 완료 → 게임 스타트 화면으로
	StartStartPage();
}

void AGameModeBase::StartStartPage()
{
	if (!GameState)
	{
		return;
	}

	GameState->SetRoundState(ERoundState::StartPage);

	// 스타트 페이지 중에는 게임플레이 입력 비활성화
	UInputManager::GetInstance().SetGameplayInputEnabled(false);

	// Lua 콜백: OnStartPageStart
	CallLuaCallback("OnStartPageStart");
}

void AGameModeBase::EndStartPage()
{
	if (!GameState)
	{
		return;
	}

	// Lua 콜백: OnStartPageEnd
	CallLuaCallback("OnStartPageEnd");

	// 게임 스타트 화면 완료 → 캐릭터 선택으로
	StartCharacterSelect();
}

void AGameModeBase::StartCharacterSelect()
{
	if (!GameState)
	{
		return;
	}

	// 라운드 종료 상태에서 호출된 경우 (다음 라운드로 진행)
	if (GameState->GetRoundState() == ERoundState::RoundEnd)
	{
		GameState->AdvanceToNextRound();
	}

	GameState->SetRoundState(ERoundState::CharacterSelect);

	// 캐릭터 선택 중에는 게임플레이 입력 비활성화
	UInputManager::GetInstance().SetGameplayInputEnabled(false);

	// Lua 콜백: OnCharacterSelectStart
	CallLuaCallback("OnCharacterSelectStart");
}

void AGameModeBase::EndCharacterSelect()
{
	if (!GameState)
	{
		return;
	}

	// Lua 콜백: OnCharacterSelectEnd
	CallLuaCallback("OnCharacterSelectEnd");

	// 캐릭터 선택 완료 → 바로 라운드 시작 (Lua에서 ReadyGo 시퀀스 처리)
	StartRound();
}

void AGameModeBase::StartRound()
{
	if (!GameState)
	{
		return;
	}

	// CountDown 상태로 설정 (Lua에서 ReadyGo 시퀀스 처리)
	// 타이머는 아직 시작하지 않음 - BeginBattle()에서 시작
	GameState->SetRoundState(ERoundState::CountDown);

	// 카운트다운 중에는 게임플레이 입력 비활성화
	UInputManager::GetInstance().SetGameplayInputEnabled(false);

	// Lua 콜백: OnRoundStart(roundNumber)
	CallLuaCallbackWithInt("OnRoundStart", GameState->GetCurrentRound());
}

void AGameModeBase::BeginBattle()
{
	if (!GameState)
	{
		return;
	}

	// 이제 진짜 전투 시작 - 타이머 시작
	GameState->SetRoundState(ERoundState::InProgress);
	GameState->SetRoundTimeRemaining(GameState->GetRoundDuration());

	// 전투 시작 시 게임플레이 입력 활성화
	UInputManager::GetInstance().SetGameplayInputEnabled(true);
}

void AGameModeBase::EndRound(int32 WinnerIndex)
{
	UE_LOG("[GameMode] EndRound called! WinnerIndex=%d\n", WinnerIndex);

	if (!GameState)
	{
		UE_LOG("[GameMode] EndRound: GameState is null!\n");
		return;
	}

	GameState->SetRoundState(ERoundState::RoundEnd);
	UE_LOG("[GameMode] EndRound: RoundState set to RoundEnd\n");

	// 라운드 종료 시 게임플레이 입력 비활성화
	UInputManager::GetInstance().SetGameplayInputEnabled(false);

	// 라운드 승자 기록
	if (WinnerIndex >= 0)
	{
		GameState->AddRoundWin(WinnerIndex);
	}

	// 라운드 종료 알림
	GameState->OnRoundEnded.Broadcast(GameState->GetCurrentRound(), WinnerIndex);

	// 매치 승리 조건 체크
	// matchResult: -1 = 아직 매치 승자 없음, 0 = P0 매치 승리, 1 = P1 매치 승리, 2 = 무승부
	int32 MatchResult = -1;
	int32 MatchWinner = CheckMatchWinCondition();
	if (MatchWinner >= 0)
	{
		UE_LOG("[GameMode] EndRound: Match winner found! Player %d wins the match.\n", MatchWinner);
		MatchResult = MatchWinner;  // 0 또는 1
	}
	else if (GameState->GetCurrentRound() >= GameState->GetMaxRounds())
	{
		UE_LOG("[GameMode] EndRound: Max rounds reached! Game ends in draw.\n");
		MatchResult = 2;  // 무승부
	}

	// 항상 OnRoundEnd Lua 콜백 호출 (GameSet 표시 후 Lua에서 GameOver 처리)
	// 3번째 인자로 매치 결과 전달 (-1이면 다음 라운드, 0이상이면 GameOver)
	UE_LOG("[GameMode] EndRound: Calling Lua OnRoundEnd callback with matchResult=%d...\n", MatchResult);
	CallLuaCallbackWithThreeInts("OnRoundEnd", GameState->GetCurrentRound(), WinnerIndex, MatchResult);
	UE_LOG("[GameMode] EndRound: Lua callback returned\n");
}

void AGameModeBase::StartCountDown(float CountDownTime)
{
	if (!GameState)
	{
		return;
	}

	bIsCountingDown = true;
	CountDownRemaining = CountDownTime;
	GameState->SetRoundState(ERoundState::CountDown);

	// 카운트다운 중에는 게임플레이 입력 비활성화
	UInputManager::GetInstance().SetGameplayInputEnabled(false);
}

// ============================================
// 승리/패배 조건 체크
// ============================================

int32 AGameModeBase::CheckRoundWinCondition()
{
	if (!GameState)
	{
		return -1;
	}

	// R키 3번 입력 시 플레이어 승리 (테스트용)
	if (GameState->GetRKeyCount() >= 3)
	{
		return 0;  // 플레이어 0 승리
	}

	// 시간 초과 시 랜덤 승자 (테스트용)
	if (GameState->GetRoundTimeRemaining() <= 0.0f)
	{
		return rand() % 2;  // 0 또는 1 랜덤 승자
	}

	return -1;  // 아직 승자 없음
}

int32 AGameModeBase::CheckMatchWinCondition()
{
	if (!GameState)
	{
		return -1;
	}

	int32 RoundsToWin = GameState->GetRoundsToWin();

	// 각 플레이어의 승리 횟수 확인
	for (int32 i = 0; i < 2; i++)
	{
		if (GameState->GetRoundWins(i) >= RoundsToWin)
		{
			return i;  // 승자 인덱스 반환
		}
	}

	return -1;  // 아직 승자 없음
}

void AGameModeBase::HandleGameOver(EGameResult Result)
{
	if (!GameState)
	{
		return;
	}

	// Lua 콜백: OnGameOver(result)
	CallLuaCallbackWithResult("OnGameOver", Result);

	GameState->SetGameResult(Result);
	EndMatch();
}

// ============================================
// Getter
// ============================================

EGameState AGameModeBase::GetCurrentGameState() const
{
	return GameState ? GameState->GetGameState() : EGameState::None;
}

ERoundState AGameModeBase::GetCurrentRoundState() const
{
	return GameState ? GameState->GetRoundState() : ERoundState::None;
}

// ============================================
// 설정
// ============================================

void AGameModeBase::SetMaxRounds(int32 InMaxRounds)
{
	if (GameState)
	{
		GameState->SetMaxRounds(InMaxRounds);
	}
}

void AGameModeBase::SetRoundsToWin(int32 InRoundsToWin)
{
	if (GameState)
	{
		GameState->SetRoundsToWin(InRoundsToWin);
	}
}

void AGameModeBase::SetRoundDuration(float Duration)
{
	if (GameState)
	{
		GameState->SetRoundDuration(Duration);
	}
}

// ============================================
// Protected
// ============================================

void AGameModeBase::InitGameState()
{
	if (GameStateClass && GWorld)
	{
		GameState = Cast<AGameStateBase>(GWorld->SpawnActor(GameStateClass, FTransform()));
	}
	else if (GWorld)
	{
		GameState = GWorld->SpawnActor<AGameStateBase>(FTransform());
	}
}

void AGameModeBase::UpdateRoundTimer(float DeltaTime)
{
	if (!GameState)
	{
		return;
	}

	float TimeRemaining = GameState->GetRoundTimeRemaining();
	TimeRemaining -= DeltaTime;

	if (TimeRemaining < 0.0f)
	{
		TimeRemaining = 0.0f;
	}

	GameState->SetRoundTimeRemaining(TimeRemaining);
}

void AGameModeBase::UpdateCountDown(float DeltaTime)
{
	CountDownRemaining -= DeltaTime;

	if (CountDownRemaining <= 0.0f)
	{
		bIsCountingDown = false;
		CountDownRemaining = 0.0f;
		StartRound();
	}
}

APlayerController* AGameModeBase::Login()
{
	if (PlayerControllerClass)
	{
		PlayerController = Cast<APlayerController>(GWorld->SpawnActor(PlayerControllerClass, FTransform())); 
	}
	else
	{
		PlayerController = GWorld->SpawnActor<APlayerController>(FTransform());
	}

	return PlayerController;
}

void AGameModeBase::PostLogin(APlayerController* NewPlayer)
{
	// 스폰 위치 찾기
	AActor* StartSpot = FindPlayerStart(NewPlayer);
	APawn* NewPawn = NewPlayer->GetPawn();

	// Pawn이 없으면 생성
    if (!NewPawn && NewPlayer)
    {
        // Try spawning a default prefab as the player's pawn first
        {
            FWideString PrefabPath = UTF8ToWide(GDataDir) + L"/Prefabs/Future.prefab";
            if (AActor* PrefabActor = GWorld->SpawnPrefabActor(PrefabPath))
            {
                if (APawn* PrefabPawn = Cast<APawn>(PrefabActor))
                {
                    NewPawn = PrefabPawn;
                }
            }
        }

        // Fallback to default pawn class if prefab did not yield a pawn
        if (!NewPawn)
        {
            NewPawn = SpawnDefaultPawnFor(NewPlayer, StartSpot);
        }

        if (NewPawn)
        {
            NewPlayer->Possess(NewPawn);
        }

    }

	// SpringArm + Camera 구조로 부착
	if (NewPawn && !NewPawn->GetComponent(USpringArmComponent::StaticClass()))
	{
		// 1. SpringArm 생성 및 RootComponent에 부착
		USpringArmComponent* SpringArm = nullptr;
		if (UActorComponent* SpringArmComp = NewPawn->AddNewComponent(USpringArmComponent::StaticClass(), NewPawn->GetRootComponent()))
		{
			SpringArm = Cast<USpringArmComponent>(SpringArmComp);
			SpringArm->SetRelativeLocation(FVector(0, 0, 2.0f));  // 캐릭터 머리 위쪽
			SpringArm->SetTargetArmLength(10.0f);                  // 카메라 거리 (스프링 암에서 카메라 컴포넌트까지의)
			SpringArm->SetDoCollisionTest(true);                  // 충돌 체크 활성화
			SpringArm->SetUsePawnControlRotation(true);           // 컨트롤러 회전 사용
		}

		// 2. Camera를 SpringArm에 부착
		if (SpringArm)
		{
			if (UActorComponent* CameraComp = NewPawn->AddNewComponent(UCameraComponent::StaticClass(), SpringArm))
			{
				auto* Camera = Cast<UCameraComponent>(CameraComp);
				// 카메라는 SpringArm 끝에 위치 (SpringArm이 위치 계산)
				Camera->SetRelativeLocation(FVector(0, 0, 0));
				Camera->SetRelativeRotationEuler(FVector(0, 0, 0));
			}
		}
	}

	if (auto* PCM = GWorld->GetPlayerCameraManager())
	{
		if (auto* Camera = Cast<UCameraComponent>(NewPawn->GetComponent(UCameraComponent::StaticClass())))
		{
			PCM->SetViewCamera(Camera);
		}
	}

	// Possess를 수행 
	if (NewPlayer)
	{
		NewPlayer->Possess(NewPlayer->GetPawn());
	} 
}

APawn* AGameModeBase::SpawnDefaultPawnFor(AController* NewPlayer, AActor* StartSpot)
{
	// 위치 결정
	FVector SpawnLoc = FVector::Zero();
	FQuat SpawnRot = FQuat::Identity();

	if (StartSpot)
	{
		SpawnLoc = StartSpot->GetActorLocation();
		SpawnRot = StartSpot->GetActorRotation();
	}
	 
	APawn* ResultPawn = nullptr;
	if (DefaultPawnClass)
	{
		ResultPawn = Cast<APawn>(GetWorld()->SpawnActor(DefaultPawnClass, FTransform(SpawnLoc, SpawnRot, FVector(1, 1, 1))));
	}

	return ResultPawn;
}

AActor* AGameModeBase::FindPlayerStart(AController* Player)
{
	// TODO: PlayerStart Actor를 찾아서 반환하도록 구현 필요
	return nullptr;
}

// ============================================
// Lua 콜백 시스템
// ============================================

void AGameModeBase::FindLuaScriptComponent()
{
	LuaScript = Cast<ULuaScriptComponent>(GetComponent(ULuaScriptComponent::StaticClass()));

	// LuaScriptComponent가 없으면 자동 생성하고 GameMode.lua 로드
	if (!LuaScript)
	{
		if (UActorComponent* NewComp = AddNewComponent(ULuaScriptComponent::StaticClass(), nullptr))
		{
			LuaScript = Cast<ULuaScriptComponent>(NewComp);
			if (LuaScript)
			{
				LuaScript->SetScriptFilePath("Data/Scripts/GameMode.lua");
				LuaScript->BeginPlay();  // 수동으로 BeginPlay 호출하여 스크립트 로드
			}
		}
	}
}

void AGameModeBase::CallLuaCallback(const char* FuncName)
{
	if (LuaScript)
	{
		LuaScript->Call(FuncName);
	}
}

void AGameModeBase::CallLuaCallbackWithInt(const char* FuncName, int32 Value)
{
	if (LuaScript)
	{
		LuaScript->CallWithInt(FuncName, Value);
	}
}

void AGameModeBase::CallLuaCallbackWithTwoInts(const char* FuncName, int32 Value1, int32 Value2)
{
	if (LuaScript)
	{
		LuaScript->CallWithTwoInts(FuncName, Value1, Value2);
	}
}

void AGameModeBase::CallLuaCallbackWithThreeInts(const char* FuncName, int32 Value1, int32 Value2, int32 Value3)
{
	if (LuaScript)
	{
		LuaScript->CallWithThreeInts(FuncName, Value1, Value2, Value3);
	}
}

void AGameModeBase::CallLuaCallbackWithResult(const char* FuncName, EGameResult Result)
{
	if (LuaScript)
	{
		// EGameResult를 int로 변환해서 전달
		LuaScript->CallWithInt(FuncName, static_cast<int32>(Result));
	}
}

