#pragma once
#include "SkillBase.h"
#include "UGorillaLightAttackSkill.generated.h"

UCLASS()
class UGorillaLightAttackSkill : public USkillBase
{
public:
	GENERATED_REFLECTION_BODY()

	UGorillaLightAttackSkill();

	virtual void Activate(AActor* Caster) override;
};
