#pragma once
#include "AccessoryActor.h"
#include "ACloakAccessoryActor.generated.h"

class UClothComponent;

UCLASS(DisplayName = "망토 악세서리", Description = "이동속도 증가 및 더블점프를 부여하는 악세서리입니다")
class ACloakAccessoryActor : public AAccessoryActor
{
public:
	GENERATED_REFLECTION_BODY();
	ACloakAccessoryActor();

	// 망토 효과 수치
	UPROPERTY(EditAnywhere, Category = "Cloak", Tooltip = "이동속도 배율")
	float SpeedBoostMultiplier = 1.4f;  // 이동속도 40% 증가

	UPROPERTY(EditAnywhere, Category = "Cloak", Tooltip = "추가 점프 횟수")
	int32 BonusJumpCount = 1;  // 추가 점프 횟수

	// Cloth 컴포넌트
	UClothComponent* ClothComponent = nullptr;

	// Equip/Unequip 오버라이드
	void Equip(AAngryCoachCharacter* OwnerCharacter) override;
	void Unequip() override;

	void DuplicateSubObjects() override;
	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

private:
	// 원래 값 저장 (Unequip 시 복원용)
	float OriginalMaxWalkSpeed = 0.0f;
	int32 OriginalMaxJumpCount = 0;
};
