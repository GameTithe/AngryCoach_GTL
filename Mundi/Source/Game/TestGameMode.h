#pragma once
#include "GameModeBase.h"
#include "ATestGameMode.generated.h"

class AAngryCoachCharacter;
class AAngryCoachPlayerController;
class ACameraActor;

/**
 * 테스트용 GameMode - 캐릭터는 스폰하지만 UI 흐름/상태 전환 없음
 *
 * AngryCoachGameMode와 동일하게 Player1, Player2, 카메라를 스폰하지만
 * StartMatch()를 호출하지 않아 Intro/StartPage/CharacterSelect 등 UI 흐름이 없음
 */
class ATestGameMode : public AGameModeBase
{
public:
	GENERATED_REFLECTION_BODY()

	ATestGameMode();
	virtual ~ATestGameMode() override;

	virtual void StartPlay() override;

protected:
	virtual APlayerController* Login() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;

private:
	AAngryCoachPlayerController* AngryCoachController = nullptr;
	AAngryCoachCharacter* Player1 = nullptr;
	AAngryCoachCharacter* Player2 = nullptr;
	ACameraActor* GameCamera = nullptr;
};
