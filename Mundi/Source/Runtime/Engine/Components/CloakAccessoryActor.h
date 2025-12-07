#pragma once
#include "AccessoryActor.h"
#include "ACloakAccessoryActor.generated.h"

UCLASS(DisplayName = "망토 악세서리", Description = "망토 스킬을 수행하는 악세서리입니다")
class ACloakAccessoryActor : public AAccessoryActor
{
public:
	GENERATED_REFLECTION_BODY();
	ACloakAccessoryActor();

	void DuplicateSubObjects() override;
	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
};
