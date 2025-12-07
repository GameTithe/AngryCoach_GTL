#include "pch.h"
#include "AngryCoachCharacter.h"
#include "SkeletalMeshComponent.h"
#include "SkillComponent.h"
#include "AccessoryActor.h"
#include "KnifeAccessoryActor.h"
#include "PunchAccessoryActor.h"
#include "World.h"
#include "Source/Runtime/Engine/Animation/AnimInstance.h"
#include "Source/Runtime/Engine/Animation/AnimMontage.h"
#include "Source/Runtime/Engine/Skill/SkillTypes.h"
#include "Source/Runtime/Core/Misc/PathUtils.h"

AAngryCoachCharacter::AAngryCoachCharacter()
{
	// SkillComponent 생성
}

AAngryCoachCharacter::~AAngryCoachCharacter()
{
}

void AAngryCoachCharacter::BeginPlay()
{
	/*
	 * 부모 클래스(ACharacter)에서 Delegate 바인딩 중
	 * 구현에 따라서 AAngryCoachCharacter에서 바인딩할 지 결정
	 */
	Super::BeginPlay();

	// 기본 펀치 악세서리 장착 (이미 장착된 게 없을 때만)
	if (GWorld && !CurrentAccessory)
	{
		// APunchAccessoryActor* PunchAccessory = GWorld->SpawnActor<APunchAccessoryActor>();
		// if (PunchAccessory)
		// {
		// 	EquipAccessory(PunchAccessory);
		//
		// 	if (SkillComponent)
		// 	{
		// 		SkillComponent->OverrideSkills(PunchAccessory->GetGrantedSkills(), PunchAccessory);
		// 	}
		// }

		AKnifeAccessoryActor* KnifeAccessory = GWorld->SpawnActor<AKnifeAccessoryActor>();
		if (KnifeAccessory)
		{
			EquipAccessory(KnifeAccessory);
		
			if (SkillComponent)
			{
				SkillComponent->OverrideSkills(KnifeAccessory->GetGrantedSkills(), KnifeAccessory);
			}
		}
	}
}

void AAngryCoachCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
}

void AAngryCoachCharacter::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		// 컴포넌트 포인터 재바인딩
		SkillComponent = nullptr;
		CurrentAccessory = nullptr;

		for (UActorComponent* Comp : GetOwnedComponents())
		{
			if (auto* Skill = Cast<USkillComponent>(Comp))
			{
				SkillComponent = Skill;
			}
		}

		// SkillComponent가 없으면 새로 생성
		if (!SkillComponent)
		{
			SkillComponent = CreateDefaultSubobject<USkillComponent>("SkillComponent");
		}
	}
}

void AAngryCoachCharacter::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();

	SkillComponent = nullptr;
	CurrentAccessory = nullptr;

	for (UActorComponent* Comp : GetOwnedComponents())
	{
		if (auto* Skill = Cast<USkillComponent>(Comp))
		{
			SkillComponent = Skill;
		}
	}

	// SkillComponent가 없으면 새로 생성
	if (!SkillComponent)
	{
		SkillComponent = CreateDefaultSubobject<USkillComponent>("SkillComponent");
	}
}

// ===== 몽타주 =====
void AAngryCoachCharacter::PlayMontage(UAnimMontage* Montage)
{
	if (!Montage)
	{
		UE_LOG("[PlayMontage] Montage is null!");
		return;
	}

	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		UE_LOG("[PlayMontage] MeshComp is null!");
		return;
	}

	UAnimInstance* AnimInstance = MeshComp->GetAnimInstance();
	if (!AnimInstance)
	{
		UE_LOG("[PlayMontage] AnimInstance is null! MeshComp: %p", MeshComp);
		return;
	}

	UE_LOG("[PlayMontage] Playing montage. AnimInstance: %p", AnimInstance);
	AnimInstance->PlayMontage(Montage);
}

void AAngryCoachCharacter::StopCurrentMontage(float BlendOutTime)
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp) return;

	UAnimInstance* AnimInstance = MeshComp->GetAnimInstance();
	if (!AnimInstance) return;

	AnimInstance->StopMontage(BlendOutTime);
}

bool AAngryCoachCharacter::IsPlayingMontage() const
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp) return false;

	UAnimInstance* AnimInstance = MeshComp->GetAnimInstance();
	if (!AnimInstance) return false;

	return AnimInstance->IsPlayingMontage();
}

// ===== 악세서리 =====

void AAngryCoachCharacter::EquipAccessory(AAccessoryActor* Accessory)
{
	if (!Accessory)
		return;

	// 기존 악세서리가 있으면 먼저 해제
	if (CurrentAccessory)
	{
		UnequipAccessory();
	}

	// 새 악세서리 등록
	CurrentAccessory = Accessory;

	// 악세서리의 Equip 로직 실행 (부착 + 스킬 등록)
	Accessory->Equip(this);
}

void AAngryCoachCharacter::UnequipAccessory()
{
	if (!CurrentAccessory)
		return;

	// 악세서리의 Unequip 로직 실행
	CurrentAccessory->Unequip();

	// 스킬을 기본 스킬로 복원
	if (SkillComponent)
	{
		SkillComponent->SetDefaultSkills();
	}

	CurrentAccessory = nullptr;
}

// ===== 스킬 =====
void AAngryCoachCharacter::OnAttackInput(EAttackInput Input)
{
	if (!SkillComponent)
		return;

	// TODO: 점프 중이면 JumpAttack, 콤보 중이면 다음 콤보 등 상태 체크
	// 지금은 단순 매핑
	ESkillSlot Slot = ESkillSlot::None;

	// BaseDage 테스트용 하드 코딩
	switch (Input)
	{
	case EAttackInput::Light:
		{
			Slot = ESkillSlot::LightAttack;			
			BaseDamage = 5.0f;
			break;
		}
	case EAttackInput::Heavy:
		{
			Slot = ESkillSlot::HeavyAttack;
			BaseDamage = 10.0f;
			break;
		}
	case EAttackInput::Skill:
		{
			Slot = ESkillSlot::Speical;
			/*
			 * TODO
			 * Skil별 데미지 적용
			 */
			BaseDamage = 15.0f;
			break;
		}
	}

	if (Slot != ESkillSlot::None)
	{
		SkillComponent->HandleInput(Slot);
	}
}

REGISTER_FUNCTION_NOTIFY(AAngryCoachCharacter, AttackBegin)
void AAngryCoachCharacter::AttackBegin()
{
	/*
	 * TODO
	 * 공격을 발생시키는 ShapeComp의 콜리전 활성화
	 */
	// GetCapsuleComponent()->SetBlockComponent(true);
}

REGISTER_FUNCTION_NOTIFY(AAngryCoachCharacter, AttackEnd)
void AAngryCoachCharacter::AttackEnd()
{
	/*
	 * TODO
	 * 공격을 발생시키는 ShapeComp의 콜리전 비활성화
	 */
	// GetCapsuleComponent()->SetBlockComponent(false);
}
