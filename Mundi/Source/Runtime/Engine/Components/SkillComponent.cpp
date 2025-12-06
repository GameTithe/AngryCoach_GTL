#include "pch.h"
#include "SkillComponent.h"
#include "SkillBase.h"

USkillComponent::USkillComponent()
{
	bCanEverTick = false;
}

void USkillComponent::BeginPlay()
{
	Super::BeginPlay();

	// 게임 시작 시 기본 스킬 로드
	SetDefaultSkills(); 

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

	// 맨몸 스킬 등록
	BuildSkillInstances(DefualtSkill, nullptr);

	//악세 스킬 Override
	BuildSkillInstances(NewSkill, InAccessory);
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
			// 스킬 객체(UObject) 생성
			USkillBase* NewSkillInstance = NewObject<USkillBase>();
			NewSkillInstance = Skill;
			
	
			// 스킬과 악세서리 맵핑
			if (Source)
			{
				NewSkillInstance->SetSourceAccessory(Source);
			}

			// 맵에 등록 (이미 있으면 덮어씌워짐)
			ActiveSkills.Add(Slot, NewSkillInstance);
		}
	}
}
