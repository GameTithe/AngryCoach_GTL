#pragma once
#include "Info.h"
#include "Source/Runtime/Core/Misc/Delegates.h"
#include "Source/Runtime/Core/Misc/Enums.h"
#include "AGameStateBase.generated.h"

class APlayerController;

/**
 * AGameStateBase - 게임 상태 데이터를 관리하는 클래스
 *
 * GameMode가 게임 규칙과 흐름을 제어한다면,
 * GameState는 현재 게임의 상태 데이터를 보관하고 알림을 보냅니다.
 */
class AGameStateBase : public AInfo
{
public:
	GENERATED_REFLECTION_BODY()

	AGameStateBase();
	virtual ~AGameStateBase() override;

	// ============================================
	// Delegates (상태 변경 알림)
	// ============================================

	// 게임 상태 변경 시 (OldState, NewState)
	DECLARE_DELEGATE(OnGameStateChanged, EGameState, EGameState);

	// 라운드 상태 변경 시 (OldState, NewState)
	DECLARE_DELEGATE(OnRoundStateChanged, ERoundState, ERoundState);

	// 라운드 시작 시 (RoundNumber)
	DECLARE_DELEGATE(OnRoundStarted, int32);

	// 라운드 종료 시 (RoundNumber, WinnerIndex: -1이면 무승부)
	DECLARE_DELEGATE(OnRoundEnded, int32, int32);

	// 게임 종료 시 (Result)
	DECLARE_DELEGATE(OnGameEnded, EGameResult);

	// ============================================
	// 상태 Getter/Setter
	// ============================================

	EGameState GetGameState() const { return CurrentGameState; }
	void SetGameState(EGameState NewState);

	ERoundState GetRoundState() const { return CurrentRoundState; }
	void SetRoundState(ERoundState NewState);

	// ============================================
	// 라운드 관련
	// ============================================

	int32 GetCurrentRound() const { return CurrentRound; }
	int32 GetMaxRounds() const { return MaxRounds; }
	void SetMaxRounds(int32 InMaxRounds) { MaxRounds = InMaxRounds; }

	// 각 플레이어의 라운드 승리 횟수 (인덱스 0: 플레이어, 1: 상대)
	int32 GetRoundWins(int32 PlayerIndex) const;
	void AddRoundWin(int32 PlayerIndex);
	void ResetRoundWins();

	// ============================================
	// 시간 관련
	// ============================================

	float GetRoundTimeRemaining() const { return RoundTimeRemaining; }
	void SetRoundTimeRemaining(float Time) { RoundTimeRemaining = Time; }
	float GetRoundDuration() const { return RoundDuration; }
	void SetRoundDuration(float Duration) { RoundDuration = Duration; }

	// ============================================
	// 게임 결과
	// ============================================

	EGameResult GetGameResult() const { return GameResult; }
	void SetGameResult(EGameResult Result);

	// 승리에 필요한 라운드 수 (예: 3판 2선승이면 2)
	int32 GetRoundsToWin() const { return RoundsToWin; }
	void SetRoundsToWin(int32 InRoundsToWin) { RoundsToWin = InRoundsToWin; }

	// ============================================
	// 유틸리티
	// ============================================

	// 다음 라운드로 진행
	void AdvanceToNextRound();

	// 상태 초기화
	void ResetGameState();

	// ============================================
	// 커스텀 게임 데이터 (테스트용)
	// ============================================

	// R키 입력 카운트
	int32 GetRKeyCount() const { return RKeyPressCount; }
	void IncrementRKeyCount() { RKeyPressCount++; }
	void ResetRKeyCount() { RKeyPressCount = 0; }

protected:
	// 현재 상태
	EGameState CurrentGameState = EGameState::None;
	ERoundState CurrentRoundState = ERoundState::None;

	// 라운드 정보
	int32 CurrentRound = 0;
	int32 MaxRounds = 1;           // 총 라운드 수
	int32 RoundsToWin = 1;         // 승리에 필요한 라운드 수

	// 각 플레이어의 라운드 승리 횟수
	TArray<int32> RoundWinsPerPlayer;

	// 시간 정보
	float RoundDuration = 99.0f;   // 라운드 제한 시간 (초)
	float RoundTimeRemaining = 0.0f;

	// 게임 결과
	EGameResult GameResult = EGameResult::None;

	// 커스텀 게임 데이터
	int32 RKeyPressCount = 0;
};
