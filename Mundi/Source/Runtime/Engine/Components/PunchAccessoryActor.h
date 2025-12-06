#pragma once
#include "AccessoryActor.h"
#include "APunchAccessoryActor.generated.h"

UCLASS(DisplayName = "펀치 악세서리", Description = "펀치 공격을 수행하는 악세서리입니다")
class APunchAccessoryActor : public AAccessoryActor
{
public:
	GENERATED_REFLECTION_BODY();
	APunchAccessoryActor();

	// 펀치 특화 속성
	UPROPERTY(EditAnywhere, Category = "Punch", Tooltip = "펀치 데미지 배율")
	float PunchDamageMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Punch", Tooltip = "펀치 공격 속도 배율")
	float PunchSpeedMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Punch", Tooltip = "펀치 넉백 거리")
	float KnockbackDistance = 100.0f;

	void DuplicateSubObjects() override;
	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
};
