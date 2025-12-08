#pragma once
#include "GameModeBase.h"
#include "AAngryCoachGameMode.generated.h"

class AAngryCoachCharacter;
class AAngryCoachPlayerController;
class ACameraActor;

class AAngryCoachGameMode : public AGameModeBase
{
public:
	GENERATED_REFLECTION_BODY()

	AAngryCoachGameMode();
	virtual ~AAngryCoachGameMode() override;

	virtual void StartPlay() override;

	// 플레이어 체력 관련 Getter (Lua 바인딩용)
	float GetP1HealthPercent() const;
	float GetP2HealthPercent() const;
	bool IsP1Alive() const;
	bool IsP2Alive() const;

	// 플레이어 체력 리셋
	void ResetPlayersHP();

	// 플레이어 접근자
	AAngryCoachCharacter* GetPlayer1() const { return Player1; }
	AAngryCoachCharacter* GetPlayer2() const { return Player2; }

protected:
	virtual APlayerController* Login() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;

private:
	AAngryCoachPlayerController* AngryCoachController = nullptr;
	AAngryCoachCharacter* Player1 = nullptr;
	AAngryCoachCharacter* Player2 = nullptr;
	ACameraActor* GameCamera = nullptr;

	USound* MainSound = nullptr;
};
