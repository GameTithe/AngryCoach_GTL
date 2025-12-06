#include "pch.h"
#include "GameStateBase.h"

AGameStateBase::AGameStateBase()
{
	// 기본 2명의 플레이어 (플레이어 vs AI/상대)
	RoundWinsPerPlayer.SetNum(2);
	RoundWinsPerPlayer[0] = 0;
	RoundWinsPerPlayer[1] = 0;
}

AGameStateBase::~AGameStateBase()
{
}

void AGameStateBase::SetGameState(EGameState NewState)
{
	if (CurrentGameState != NewState)
	{
		EGameState OldState = CurrentGameState;
		CurrentGameState = NewState;
		OnGameStateChanged.Broadcast(OldState, NewState);
	}
}

void AGameStateBase::SetRoundState(ERoundState NewState)
{
	if (CurrentRoundState != NewState)
	{
		ERoundState OldState = CurrentRoundState;
		CurrentRoundState = NewState;
		OnRoundStateChanged.Broadcast(OldState, NewState);
	}
}

int32 AGameStateBase::GetRoundWins(int32 PlayerIndex) const
{
	if (PlayerIndex >= 0 && PlayerIndex < RoundWinsPerPlayer.Num())
	{
		return RoundWinsPerPlayer[PlayerIndex];
	}
	return 0;
}

void AGameStateBase::AddRoundWin(int32 PlayerIndex)
{
	if (PlayerIndex >= 0 && PlayerIndex < RoundWinsPerPlayer.Num())
	{
		RoundWinsPerPlayer[PlayerIndex]++;
	}
}

void AGameStateBase::ResetRoundWins()
{
	for (int32 i = 0; i < RoundWinsPerPlayer.Num(); i++)
	{
		RoundWinsPerPlayer[i] = 0;
	}
}

void AGameStateBase::SetGameResult(EGameResult Result)
{
	if (GameResult != Result)
	{
		GameResult = Result;
		OnGameEnded.Broadcast(Result);
	}
}

void AGameStateBase::AdvanceToNextRound()
{
	CurrentRound++;
	RoundTimeRemaining = RoundDuration;
	SetRoundState(ERoundState::None);  // GameModeBase에서 CharacterSelect로 설정
	OnRoundStarted.Broadcast(CurrentRound);
}

void AGameStateBase::ResetGameState()
{
	CurrentGameState = EGameState::None;
	CurrentRoundState = ERoundState::None;
	CurrentRound = 0;
	RoundTimeRemaining = RoundDuration;
	GameResult = EGameResult::None;
	ResetRoundWins();
}
