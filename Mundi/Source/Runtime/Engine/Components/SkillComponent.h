#pragma once
#include "ActorComponent.h"
#include "USkillComponent.generated.h"
#include "Source/Runtime/Engine/Skill/SkillTypes.h"

class USkillBase;
class AAccessoryActor;

class USkillComponent  : public UActorComponent
{
public:
	GENERATED_REFLECTION_BODY();

	USkillComponent();

protected:

	// 현재 활성화된 스킬 인스턴스들
	TMap<ESkillSlot, USkillBase*> ActiveSkills;

	// 맨몸일 때, 사용할 기본 스킬 클래스들
	TMap<ESkillSlot, USkillBase*> DefualtSkill;

	AAccessoryActor* CurrentAccessory;

public:
	virtual void BeginPlay() override;

	// 입력에 따른 스킬 처리
	void HandleInput(ESkillSlot Slot);
	  // 스킬 덮어쓰기 ( 악세 장착했을 때 )
	void OverrideSkills(const TMap<ESkillSlot, USkillBase*>& NewSkill, AAccessoryActor* InAccessory);

	// 기본 스킬 복구 ( 악세 떨어뜨렸을 때 )
	void SetDefaultSkills();

private:
	void BuildSkillInstances(const TMap<ESkillSlot, USkillBase*>& SkillMap, AAccessoryActor* Source);
};