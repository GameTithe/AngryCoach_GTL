#include "pch.h"
#include "DefaultJumpAttackSkill.h"
#include "AngryCoachCharacter.h"
#include "CharacterMovementComponent.h"
#include "Source/Runtime/Engine/Animation/AnimMontage.h"
#include <cmath>

UDefaultJumpAttackSkill::UDefaultJumpAttackSkill()
{
	ObjectName = "DefaultJumpAttack";
	Montage = RESOURCE.Load<UAnimMontage>("Data/Montages/PlayerJumpKick.montage.json");
	if (!Montage)
	{
		UE_LOG("[DefaultJumpAttackSkill] Failed to load montage!");
	}
}

void UDefaultJumpAttackSkill::Activate(AActor* Caster)
{
	AAngryCoachCharacter* Character = Cast<AAngryCoachCharacter>(Caster);
	if (!Character)
	{
		UE_LOG("[DefaultJumpAttackSkill] Caster is not AAngryCoachCharacter!");
		return;
	}

	// 몽타주 재생
	if (Montage)
	{
		Character->PlayMontage(Montage);
	}

	// 입력 방향으로 돌진
	FVector Direction = Character->GetJumpAttackDirection();
	if (Direction.IsZero())
	{
		// 입력 없으면 현재 회전에서 방향 계산
		FVector Euler = Character->GetActorRotation().ToEulerZYXDeg();
		float Yaw = Euler.Z * (PI / 180.0f);
		Direction = FVector(std::cos(Yaw), std::sin(Yaw), 0.0f);
	}

	// 캐릭터 회전 (공격 방향으로)
	float TargetYaw = std::atan2(Direction.Y, Direction.X) * (180.0f / PI);
	FQuat TargetRotation = FQuat::MakeFromEulerZYX(FVector(0.0f, 0.0f, TargetYaw));
	Character->SetActorRotation(TargetRotation);

	// 돌진 (XY 방향 + 아래로)
	FVector LaunchVelocity = Direction * LaunchSpeed;
	LaunchVelocity.Z = LaunchZSpeed;

	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		// 속도 초기화 후 돌진
		Movement->SetVelocity(FVector::Zero());
		Movement->LaunchCharacter(LaunchVelocity, true, true);
	}

	UE_LOG("[Skill] Jump Attack! Dir=(%.2f, %.2f, %.2f)", Direction.X, Direction.Y, Direction.Z);
}
