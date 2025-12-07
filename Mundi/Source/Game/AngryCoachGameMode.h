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

	

protected:
	virtual APlayerController* Login() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;

private:
	AAngryCoachPlayerController* AngryCoachController = nullptr;
	AAngryCoachCharacter* Player1 = nullptr;
	AAngryCoachCharacter* Player2 = nullptr;
	ACameraActor* GameCamera = nullptr;
};
