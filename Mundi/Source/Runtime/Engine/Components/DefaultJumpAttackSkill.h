#pragma once
#include "SkillBase.h"
#include "UDefaultJumpAttackSkill.generated.h"

UCLASS()
class UDefaultJumpAttackSkill : public USkillBase
{
public:
	GENERATED_REFLECTION_BODY()

	UDefaultJumpAttackSkill();

	virtual void Activate(AActor* Caster) override;

protected:
	// 돌진 속도 (XY 방향)
	float LaunchSpeed = 8.0f;
	// 수직 속도 (음수면 아래로)
	float LaunchZSpeed = -3.0f;
};
