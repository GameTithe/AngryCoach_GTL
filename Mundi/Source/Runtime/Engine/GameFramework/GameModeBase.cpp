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

AGameModeBase::AGameModeBase()
{
	DefaultPawnClass = ACharacter::StaticClass();
	PlayerControllerClass = APlayerController::StaticClass();
	GameStateClass = AGameStateBase::StaticClass();

	bCanEverTick = true;
}

AGameModeBase::~AGameModeBase()
{
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
		if (RoundWinner >= -1 && RoundWinner != -1)  // -1이 아니면 승자가 결정됨 (-2는 무승부)
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

	// 첫 번째 라운드 시작
	GameState->AdvanceToNextRound();
	StartCountDown();
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

void AGameModeBase::StartRound()
{
	if (!GameState)
	{
		return;
	}

	GameState->SetRoundState(ERoundState::InProgress);
	GameState->SetRoundTimeRemaining(GameState->GetRoundDuration());

	// Lua 콜백: OnRoundStart(roundNumber)
	CallLuaCallbackWithInt("OnRoundStart", GameState->GetCurrentRound());
}

void AGameModeBase::EndRound(int32 WinnerIndex)
{
	if (!GameState)
	{
		return;
	}

	GameState->SetRoundState(ERoundState::RoundEnd);

	// 라운드 승자 기록
	if (WinnerIndex >= 0)
	{
		GameState->AddRoundWin(WinnerIndex);
	}

	// 라운드 종료 알림
	GameState->OnRoundEnded.Broadcast(GameState->GetCurrentRound(), WinnerIndex);

	// Lua 콜백: OnRoundEnd(roundNumber, winnerIndex)
	CallLuaCallbackWithTwoInts("OnRoundEnd", GameState->GetCurrentRound(), WinnerIndex);

	// 매치 승리 조건 체크
	int32 MatchWinner = CheckMatchWinCondition();
	if (MatchWinner >= 0)
	{
		// 게임 종료
		EGameResult Result = (MatchWinner == 0) ? EGameResult::Win : EGameResult::Lose;
		HandleGameOver(Result);
	}
	else if (GameState->GetCurrentRound() >= GameState->GetMaxRounds())
	{
		// 최대 라운드 도달 - 무승부 처리
		HandleGameOver(EGameResult::Draw);
	}
	else
	{
		// 다음 라운드로 진행
		GameState->AdvanceToNextRound();
		StartCountDown();
	}
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
}

// ============================================
// 승리/패배 조건 체크
// ============================================

int32 AGameModeBase::CheckRoundWinCondition()
{
	// 기본 구현: 시간 초과 시 무승부
	// 서브클래스에서 오버라이드하여 실제 승리 조건 구현
	if (GameState && GameState->GetRoundTimeRemaining() <= 0.0f)
	{
		return -2;  // 무승부
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

void AGameModeBase::CallLuaCallbackWithResult(const char* FuncName, EGameResult Result)
{
	if (LuaScript)
	{
		// EGameResult를 int로 변환해서 전달
		LuaScript->CallWithInt(FuncName, static_cast<int32>(Result));
	}
}

