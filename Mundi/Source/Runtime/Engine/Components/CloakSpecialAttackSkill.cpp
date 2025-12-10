#include "pch.h"
#include "CloakSpecialAttackSkill.h"
#include "CloakAccessoryActor.h"
#include "AngryCoachCharacter.h"
#include "Source/Runtime/Engine/Animation/AnimMontage.h"
#include "CharacterMovementComponent.h"
#include "FAudioDevice.h"


UCloakSpecialAttackSkill::UCloakSpecialAttackSkill()
{
	ObjectName = "CloakSpecialAttack";

	Montage = RESOURCE.Load<UAnimMontage>("Data/Montages/PlayerSpecial.montage.json");
	if (!Montage)
	{
		UE_LOG("[CloakSpecialSkill] Failed to load Cloak montage!");
	}
}

void UCloakSpecialAttackSkill::Activate(AActor* Caster)
{
	// Super::Activate(Caster);

	float DamageMultiplier = 1.0f;
	float Range = 150.0f;
	if (ACloakAccessoryActor* CloakAccessory = Cast<ACloakAccessoryActor>(SourceAccessory))
	{
		//DamageMultiplier = CloakAccessory->KnifeDamageMultiplier;
		//Range = KnifeAccessory->AttackRange;
	}

	// 애니메이션 재생
	if (AAngryCoachCharacter* Character = Cast<AAngryCoachCharacter>(Caster))
	{
		if (!Character->GetCharacterMovement()->IsFalling() && Montage)
		{
			// 사운드 재생
			if (Character->GetSkillSound())
			{
				FAudioDevice::PlaySoundAtLocationOneShot(Character->GetSkillSound(), Character->GetActorLocation());
			}

			// 강제 이동 설정: 바라보는 방향으로 일정 속도로 이동
			FVector DashDirection = Character->GetActorRotation().GetForwardVector();
			float DashSpeed = 5.0f;  // 대시 속도
			float DashDuration = Montage->GetPlayLength();  // 몽타주 재생 시간 동안 이동

			Character->GetCharacterMovement()->SetForcedMovement(
				DashDirection * DashSpeed,
				DashDuration
			);
			Character->PlayMontage(Montage);
		}
	}
}
