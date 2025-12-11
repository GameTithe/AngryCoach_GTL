#include "pch.h"
#include "SkillComponent.h"
#include "SkillBase.h"
#include "DefaultLightAttackSkill.h"
#include "DefaultHeavyAttackSkill.h"
#include "DefaultJumpAttackSkill.h"

USkillComponent::USkillComponent()
{
	bCanEverTick = false;
}

void USkillComponent::BeginPlay()
{
	Super::BeginPlay();

	// 기본 스킬 생성 및 등록
	UDefaultLightAttackSkill* DefaultLight = NewObject<UDefaultLightAttackSkill>();
	UDefaultHeavyAttackSkill* DefaultHeavy = NewObject<UDefaultHeavyAttackSkill>();
	UDefaultJumpAttackSkill* DefaultJump = NewObject<UDefaultJumpAttackSkill>();

	DefualtSkill.Add(ESkillSlot::LightAttack, DefaultLight);
	DefualtSkill.Add(ESkillSlot::HeavyAttack, DefaultHeavy);
	DefualtSkill.Add(ESkillSlot::JumpAttack, DefaultJump);

	// 악세서리가 없을 때만 기본 스킬 활성화
	if (!CurrentAccessory)
	{
		SetDefaultSkills();
	}
}

void USkillComponent::HandleInput(ESkillSlot Slot)
{
	if (USkillBase** FoundSkill = ActiveSkills.Find(Slot))
	{
		if (*FoundSkill)
		{
			(*FoundSkill)->Activate(GetOwner());
		}
	}
	else
	{
		UE_LOG("Cant Find Skill");
	}
}

void USkillComponent::OverrideSkills(const TMap<ESkillSlot, USkillBase*>& NewSkill, AAccessoryActor* InAccessory)
{
	CurrentAccessory = InAccessory;

	// 기존에 가지고 있던 스킬 목록 비우기
	ActiveSkills.Empty();

	// 1. 기본 스킬 먼저 등록 (점프 공격 등)
	BuildSkillInstances(DefualtSkill, nullptr);

	// 2. 악세서리 스킬로 덮어쓰기 (nullptr이면 해당 슬롯 제거)
	for (const auto& Pair : NewSkill)
	{
		ESkillSlot Slot = Pair.first;
		USkillBase* Skill = Pair.second;

		if (Skill)
		{
			Skill->SetSourceAccessory(InAccessory);
			ActiveSkills.Add(Slot, Skill);
			UE_LOG("[SkillComponent] Override skill for slot %d", static_cast<int>(Slot));
		}
		else
		{
			// nullptr이면 해당 슬롯 제거 (고릴라 점프 공격 비활성화용)
			ActiveSkills.Remove(Slot);
			UE_LOG("[SkillComponent] Removed skill for slot %d", static_cast<int>(Slot));
		}
	}
}

void USkillComponent::SetDefaultSkills()
{
	CurrentAccessory = nullptr;
	ActiveSkills.Empty();
	BuildSkillInstances(DefualtSkill, nullptr);
}

void USkillComponent::BuildSkillInstances(const TMap<ESkillSlot, USkillBase*>& SkillMap, AAccessoryActor* Source)
{
	for (const auto& Pair : SkillMap)
	{
		ESkillSlot Slot = Pair.first;
		USkillBase* Skill = Pair.second;

		if (Skill)
		{
			// 스킬과 악세서리 맵핑
			if (Source)
			{
				Skill->SetSourceAccessory(Source);
			}

			// 맵에 등록 (이미 있으면 덮어씌워짐)
			ActiveSkills.Add(Slot, Skill);

			UE_LOG("[SkillComponent] Registered skill for slot %d", static_cast<int>(Slot));
		}
	}
}
