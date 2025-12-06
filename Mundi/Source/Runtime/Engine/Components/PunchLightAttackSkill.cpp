#include "pch.h"
#include "PunchLightAttackSkill.h"
#include "PunchAccessoryActor.h"

UPunchLightAttackSkill::UPunchLightAttackSkill()
{
	ObjectName = "PunchLightAttack";
	Montage = RESOURCE.Load<UAnimMontage>("Data/Montages/Combo.Montage.montage.json");
}

void UPunchLightAttackSkill::Activate(AActor* Caster)
{
	Super::Activate(Caster);

	// 펀치 악세서리에서 데미지 배율 가져오기
	float DamageMultiplier = 1.0f;
	if (APunchAccessoryActor* PunchAccessory = Cast<APunchAccessoryActor>(SourceAccessory))
	{
		DamageMultiplier = PunchAccessory->PunchDamageMultiplier;
	}

	UE_LOG("[Skill] Punch Light Attack! (Damage x%.2f)", DamageMultiplier);

	// TODO: 실제 펀치 공격 로직 구현
	// - 애니메이션 재생
	// - 히트박스 활성화
	// - 데미지 적용
}
