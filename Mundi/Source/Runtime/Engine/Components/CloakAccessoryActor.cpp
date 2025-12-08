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
#include "ParticleSystemComponent.h"

ACloakAccessoryActor::ACloakAccessoryActor()
{
	ObjectName = "CloakAccessory";
	AccessoryName = "Cloak";
	Description = "Speed boost and double jump";

	// Tick 활성화 (Wind 업데이트를 위해)
	bCanEverTick = true;

	// Cloth 컴포넌트 생성
	ClothComponent = CreateDefaultSubobject<UClothComponent>("ClothComponent");
	if (ClothComponent)
	{
		ClothComponent->SetupAttachment(RootComponent);
	}

	PassiveEffectParticle = CreateDefaultSubobject<UParticleSystemComponent>("PassiveEffectParticle");
	PassiveEffectParticle->ObjectName = FName("PassiveEffectParticle");
	PassiveEffectParticle->SetupAttachment(RootComponent);
 
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

void ACloakAccessoryActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 캐릭터가 있고 ClothComponent가 있을 때만 Wind 업데이트
	if (OwningCharacter && ClothComponent)
	{
		// 캐릭터의 현재 속도 가져오기
		UCharacterMovementComponent* Movement = OwningCharacter->GetCharacterMovement();
		if (Movement)
		{
			FVector CharacterVelocity = Movement->GetVelocity();
			UpdateWindFromVelocity(CharacterVelocity);
		}
	}
}

void ACloakAccessoryActor::UpdateWindFromVelocity(const FVector& CharacterVelocity)
{
	if (!ClothComponent || !OwningCharacter)
		return;

	// 속도가 거의 0이면 바람 없음
	float VelocityMagnitude = CharacterVelocity.Size();
	if (VelocityMagnitude < 0.5f)
	{
		ClothComponent->SetWindVelocity(FVector::Zero());
		return;
	}

	// 2D 속도만 사용 (XY 평면, Z축 제외)
	FVector Velocity2D = FVector(CharacterVelocity.X, CharacterVelocity.Y, 0.0f);

	// 속도의 반대 방향으로 바람 설정
	//FVector WindDirection = -Velocity2D.GetSafeNormal();
	//FVector WindDirection = FVector(-Velocity2D.X, Velocity2D.Y, 0.0f).GetSafeNormal();
	FVector WindDirection = FVector(10.0, 0.0f, 0.0f).GetSafeNormal();

	// 바람 강도 설정 (속도에 비례)
	float WindScale = 5.5f;
	float WindMagnitude = Velocity2D.Size() * WindScale;

	FVector FinalWind = WindDirection * WindMagnitude;

	// 디버그 로그 (필요시)
	// UE_LOG(LogTemp, Warning, TEXT("Velocity: %s, Wind: %s"), *CharacterVelocity.ToString(), *FinalWind.ToString());

	ClothComponent->SetWindVelocity(FVector(0,20,0));
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
