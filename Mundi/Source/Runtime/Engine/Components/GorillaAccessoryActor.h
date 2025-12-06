#pragma once
#include "AccessoryActor.h"
#include "AGorillaAccessoryActor.generated.h"

UCLASS(DisplayName = "고릴라 악세서리", Description = "고릴라 공격을 수행하는 악세서리입니다")
class AGorillaAccessoryActor : public AAccessoryActor
{
public:
	GENERATED_REFLECTION_BODY();
	AGorillaAccessoryActor();

	void DuplicateSubObjects() override;
	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
};
