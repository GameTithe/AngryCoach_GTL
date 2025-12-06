#include "pch.h"
#include "AngryCoachCharacter.h"
#include "SkeletalMeshComponent.h"
#include "SkillComponent.h"
#include "AccessoryActor.h"
#include "PunchAccessoryActor.h"
#include "World.h"
#include "Source/Runtime/Engine/Animation/AnimInstance.h"
#include "Source/Runtime/Engine/Animation/AnimMontage.h"
#include "Source/Runtime/Engine/Skill/SkillTypes.h"
#include "Source/Runtime/Core/Misc/PathUtils.h"

AAngryCoachCharacter::AAngryCoachCharacter()
{
	// SkillComponent 생성
	SkillComponent = CreateDefaultSubobject<USkillComponent>("SkillComponent");
}

AAngryCoachCharacter::~AAngryCoachCharacter()
{
}

void AAngryCoachCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 기본 펀치 악세서리 장착
	if (GWorld)
	{
		APunchAccessoryActor* PunchAccessory = GWorld->SpawnActor<APunchAccessoryActor>();
		if (PunchAccessory)
		{
			EquipAccessory(PunchAccessory);

			if (SkillComponent)
			{
				SkillComponent->OverrideSkills(PunchAccessory->GetGrantedSkills(), PunchAccessory);
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
	if (!Montage) return;

	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp) return;

	UAnimInstance* AnimInstance = MeshComp->GetAnimInstance();
	if (!AnimInstance) return;

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

	switch (Input)
	{
	case EAttackInput::Light:
		Slot = ESkillSlot::LightAttack;
		break;
	case EAttackInput::Heavy:
		Slot = ESkillSlot::HeavyAttack;
		break;
	case EAttackInput::Skill:
		Slot = ESkillSlot::Speical;
		break;
	}

	if (Slot != ESkillSlot::None)
	{
		SkillComponent->HandleInput(Slot);
	}
}
