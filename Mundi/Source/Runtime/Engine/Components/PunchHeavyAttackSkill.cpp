#include "pch.h"
#include "PunchHeavyAttackSkill.h"

#include "AngryCoachCharacter.h"
#include "PunchAccessoryActor.h"

UPunchHeavyAttackSkill::UPunchHeavyAttackSkill()
{
	ObjectName = "PunchHeavyAttack";
	Montage = RESOURCE.Load<UAnimMontage>("Data/Montages/Combo.Montage.montage.json");
}

void UPunchHeavyAttackSkill::Activate(AActor* Caster)
{
	Super::Activate(Caster);

	// 펀치 악세서리에서 데미지/넉백 정보 가져오기
	float DamageMultiplier = 1.0f;
	float KnockbackDist = 100.0f;
	if (APunchAccessoryActor* PunchAccessory = Cast<APunchAccessoryActor>(SourceAccessory))
	{
		DamageMultiplier = PunchAccessory->PunchDamageMultiplier;
		KnockbackDist = PunchAccessory->KnockbackDistance;
	}

	UE_LOG("[Skill] Punch Heavy Attack! (Damage x%.2f, Knockback %.1f)", DamageMultiplier, KnockbackDist);

	// TODO: 실제 헤비 펀치 공격 로직 구현
	// - 히트박스 활성화
	// - 데미지 적용 (더 높은 데미지)
	// - 넉백 적용
}
