#pragma once
#include "AccessoryActor.h"
#include "AKnifeAccessoryActor.generated.h"

UCLASS(DisplayName = "나이프 악세서리", Description = "나이프 공격을 수행하는 악세서리입니다")
class AKnifeAccessoryActor : public AAccessoryActor
{
public:
	GENERATED_REFLECTION_BODY();
	AKnifeAccessoryActor();

	UPROPERTY(EditAnywhere, Category = "Knife", Tooltip = "나이프 데미지 배율")
	float KnifeDamageMultiplier = 1.2f;

	UPROPERTY(EditAnywhere, Category = "Knife", Tooltip = "나이프 공격 속도 배율")
	float KnifeSpeedMultiplier = 1.5f;

	UPROPERTY(EditAnywhere, Category = "Knife", Tooltip = "나이프 공격 범위")
	float AttackRange = 150.0f;

	void DuplicateSubObjects() override;
	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
};
