#pragma once
#include "GameModeBase.h"
#include "AAngryCoachGameMode.generated.h"

class ACharacter;
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
	ACharacter* Player1 = nullptr;
	ACharacter* Player2 = nullptr;
	ACameraActor* GameCamera = nullptr;
};
