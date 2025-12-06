#pragma once
#include "Actor.h"
#include "Source/Runtime/Core/Misc/Enums.h"
#include "AGameModeBase.generated.h"

class APawn;
class ACharacter;
class AController;
class APlayerController;
class AGameStateBase;
class ULuaScriptComponent;


class AGameModeBase : public AActor
{
public:
	GENERATED_REFLECTION_BODY()

	AGameModeBase();
	~AGameModeBase() override;

	APawn* DefaultPawn;
	APlayerController* PlayerController;

	UClass* DefaultPawnClass;
	UClass* PlayerControllerClass;
	UClass* GameStateClass;

	// 게임 시작 시 호출
	virtual void StartPlay();

	// 매 프레임 호출
	virtual void Tick(float DeltaTime) override;

	// ============================================
	// 게임 흐름 제어
	// ============================================

	// 매치 시작/종료
	virtual void StartMatch();
	virtual void EndMatch();

	// 캐릭터 선택 시작/종료
	virtual void StartCharacterSelect();
	virtual void EndCharacterSelect();  // Lua에서 선택 완료 시 호출

	// 라운드 시작/종료
	virtual void StartRound();
	virtual void BeginBattle();  // ReadyGo 완료 후 실제 전투 시작 (타이머 시작)
	virtual void EndRound(int32 WinnerIndex);  // -1이면 무승부

	// 카운트다운 시작 (Ready/Go 시퀀스)
	virtual void StartCountDown(float CountDownTime = 3.0f);

	// ============================================
	// 승리/패배 조건 체크
	// ============================================

	// 라운드 승리 조건 체크 (서브클래스에서 오버라이드)
	// 반환값: 승자 인덱스 (-1이면 아직 승자 없음, -2면 무승부)
	virtual int32 CheckRoundWinCondition();

	// 매치 승리 조건 체크
	// 반환값: 승자 인덱스 (-1이면 아직 승자 없음)
	virtual int32 CheckMatchWinCondition();

	// 게임 결과 처리
	virtual void HandleGameOver(EGameResult Result);

	// ============================================
	// Getter
	// ============================================

	AGameStateBase* GetGameState() const { return GameState; }
	EGameState GetCurrentGameState() const;
	ERoundState GetCurrentRoundState() const;

	// ============================================
	// 설정
	// ============================================

	void SetMaxRounds(int32 InMaxRounds);
	void SetRoundsToWin(int32 InRoundsToWin);
	void SetRoundDuration(float Duration);

protected:

	// 플레이어 접속 처리(PlayerController생성)
	virtual APlayerController* Login();

	// 접속 후 폰 스폰 및 빙의
	virtual void PostLogin(APlayerController* NewPlayer);

	// 기본 폰 스폰
	APawn* SpawnDefaultPawnFor(AController* NewPlayer, AActor* StartSpot);

	// 시작 지점 찾기
	AActor* FindPlayerStart(AController* Player);

	// GameState 생성
	virtual void InitGameState();

	// 라운드 시간 업데이트
	virtual void UpdateRoundTimer(float DeltaTime);

	// 카운트다운 업데이트
	virtual void UpdateCountDown(float DeltaTime);

protected:
	// 게임 상태 인스턴스
	AGameStateBase* GameState = nullptr;

	// 카운트다운
	float CountDownRemaining = 0.0f;
	bool bIsCountingDown = false;

	// ============================================
	// Lua 콜백 시스템
	// ============================================

	// 자신에게 붙은 LuaScriptComponent (콜백 호출용)
	ULuaScriptComponent* LuaScript = nullptr;

	// LuaScriptComponent 찾기
	void FindLuaScriptComponent();

	// Lua 콜백 호출 헬퍼
	void CallLuaCallback(const char* FuncName);
	void CallLuaCallbackWithInt(const char* FuncName, int32 Value);
	void CallLuaCallbackWithTwoInts(const char* FuncName, int32 Value1, int32 Value2);
	void CallLuaCallbackWithResult(const char* FuncName, EGameResult Result);
};