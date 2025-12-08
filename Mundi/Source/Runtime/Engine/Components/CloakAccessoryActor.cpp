#include "pch.h"
#include "CloakAccessoryActor.h"
#include "DefaultLightAttackSkill.h"
#include "DefaultHeavyAttackSkill.h"
#include "AngryCoachCharacter.h"
#include "CharacterMovementComponent.h"
#include "ClothComponent.h"
#include "CloakHeavyAttackSkill.h"
#include "CloakLightAttackSkill.h"
#include "CloakSpecialAttackSkill.h"

ACloakAccessoryActor::ACloakAccessoryActor()
{
	ObjectName = "CloakAccessory";
	AccessoryName = "Cloak";
	Description = "Speed boost and double jump";

	// Cloth 컴포넌트 생성
	ClothComponent = CreateDefaultSubobject<UClothComponent>("ClothComponent");
	if (ClothComponent)
	{
		ClothComponent->SetupAttachment(RootComponent);
	}
 
	// Default 스킬 사용
	GrantedSkills.clear();
	UCloakLightAttackSkill* LightSkill = NewObject<UCloakLightAttackSkill>();
	UCloakHeavyAttackSkill* HeavySkill = NewObject<UCloakHeavyAttackSkill>();
	UCloakSpecialAttackSkill* SpecialSkill = NewObject<UCloakSpecialAttackSkill>();
	
	GrantedSkills.Add(ESkillSlot::LightAttack, LightSkill);
	GrantedSkills.Add(ESkillSlot::HeavyAttack, HeavySkill);
	GrantedSkills.Add(ESkillSlot::Specical, SpecialSkill);
}

void ACloakAccessoryActor::Equip(AAngryCoachCharacter* OwnerCharacter)
{
	// 부모 Equip 호출 (소켓 부착 + 스킬 등록)
	Super::Equip(OwnerCharacter);

	if (!OwnerCharacter) return;

	UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement();
	if (Movement)
	{
		// 원래 값 저장
		OriginalMaxWalkSpeed = Movement->MaxWalkSpeed;
		OriginalMaxJumpCount = Movement->GetMaxJumpCount();

		// 이동속도 증가
		Movement->MaxWalkSpeed *= SpeedBoostMultiplier;

		// 점프 횟수 증가
		Movement->SetMaxJumpCount(OriginalMaxJumpCount + BonusJumpCount);

		UE_LOG("[CloakAccessory] Equipped: Speed %.1f -> %.1f, Jump %d -> %d",
			OriginalMaxWalkSpeed, Movement->MaxWalkSpeed,
			OriginalMaxJumpCount, Movement->GetMaxJumpCount());
	}
}

void ACloakAccessoryActor::Unequip()
{
	if (OwningCharacter)
	{
		UCharacterMovementComponent* Movement = OwningCharacter->GetCharacterMovement();
		if (Movement)
		{
			// 원래 값 복원
			Movement->MaxWalkSpeed = OriginalMaxWalkSpeed;
			Movement->SetMaxJumpCount(OriginalMaxJumpCount);

			UE_LOG("[CloakAccessory] Unequipped: Speed/Jump restored");
		}
	}

	Super::Unequip();
}

void ACloakAccessoryActor::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		// ClothComponent 포인터 복구
		ClothComponent = nullptr;
		for (UActorComponent* Comp : GetOwnedComponents())
		{
			if (auto* Cloth = Cast<UClothComponent>(Comp))
			{
				ClothComponent = Cloth;
				break;
			}
		}

		// Default 스킬로 재생성
		GrantedSkills.clear();
		UCloakLightAttackSkill* LightSkill = NewObject<UCloakLightAttackSkill>();
		UCloakHeavyAttackSkill* HeavySkill = NewObject<UCloakHeavyAttackSkill>();
		UCloakSpecialAttackSkill* SpecialSkill = NewObject<UCloakSpecialAttackSkill>();
		GrantedSkills.Add(ESkillSlot::LightAttack, LightSkill);
		GrantedSkills.Add(ESkillSlot::HeavyAttack, HeavySkill);
		GrantedSkills.Add(ESkillSlot::Specical, SpecialSkill);

		// 저장된 값 초기화
		OriginalMaxWalkSpeed = 0.0f;
		OriginalMaxJumpCount = 0;
	}
}

void ACloakAccessoryActor::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();

	// ClothComponent 포인터 복구
	ClothComponent = nullptr;
	for (UActorComponent* Comp : GetOwnedComponents())
	{
		if (auto* Cloth = Cast<UClothComponent>(Comp))
		{
			ClothComponent = Cloth;
			break;
		}
	}

	// Default 스킬로 재생성
	GrantedSkills.clear();
	UCloakLightAttackSkill* LightSkill = NewObject<UCloakLightAttackSkill>();
	UCloakHeavyAttackSkill* HeavySkill = NewObject<UCloakHeavyAttackSkill>();
	UCloakHeavyAttackSkill* SpecialSkill = NewObject<UCloakHeavyAttackSkill>();

	GrantedSkills.Add(ESkillSlot::LightAttack, LightSkill);
	GrantedSkills.Add(ESkillSlot::HeavyAttack, HeavySkill);
	GrantedSkills.Add(ESkillSlot::Specical, SpecialSkill);

	// 저장된 값 초기화
	OriginalMaxWalkSpeed = 0.0f;
	OriginalMaxJumpCount = 0;
}
