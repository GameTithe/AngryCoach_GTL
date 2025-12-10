#include "pch.h"
#include "AngryCoachCharacter.h"

#include <types.hpp>

#include "SkeletalMeshComponent.h"
#include "SkillComponent.h"
#include "AccessoryActor.h"
#include "AngryCoachPlayerController.h"
#include "GorillaAccessoryActor.h"
#include "CapsuleComponent.h"
#include "CharacterMovementComponent.h"
#include "GameplayStatics.h"
#include "KnifeAccessoryActor.h"
#include "PunchAccessoryActor.h"
#include "ShapeComponent.h"
#include "StaticMeshComponent.h"
#include "World.h"
#include "Source/Runtime/Engine/Animation/AnimInstance.h"
#include "Source/Runtime/Engine/Animation/AnimMontage.h"
#include "Source/Runtime/Engine/Skill/SkillTypes.h"
#include "Source/Runtime/Core/Misc/PathUtils.h"
#include "CloakAccessoryActor.h" 
#include "FAudioDevice.h"
#include "PlayerCameraManager.h"

AAngryCoachCharacter::AAngryCoachCharacter()
{
	// SkillComponent 생성
	HitReationMontage = RESOURCE.Load<UAnimMontage>("Data/Montages/HitReaction.montage.json");
	GuardMontage = RESOURCE.Load<UAnimMontage>("Data/Montages/Guard.montage.json");
	GorillaGuardMontage = RESOURCE.Load<UAnimMontage>("Data/Montages/Guard.montage.json");
	if (GuardMontage)
	{
		GuardMontage->bLoop = true;
	}
	if (GorillaGuardMontage)
	{
		GorillaGuardMontage->bLoop = true;
	}
	
	Hit1Sound = RESOURCE.Load<USound>("Data/Audio/HIT1.wav");
	Hit2Sound = RESOURCE.Load<USound>("Data/Audio/HIT2.wav");
	SkillSound = RESOURCE.Load<USound>("Data/Audio/SKILL.wav");
	DieSound = RESOURCE.Load<USound>("Data/Audio/Die.wav");
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

	FString PrefabPath = "Data/Prefabs/Gorilla.prefab";
	AGorillaAccessoryActor * GorillaAccessory = Cast<AGorillaAccessoryActor>(GWorld->SpawnPrefabActor(UTF8ToWide(PrefabPath)));
	if (GorillaAccessory)
	{
		EquipAccessory(GorillaAccessory);
		if (SkillComponent)
		{
			SkillComponent->OverrideSkills(GorillaAccessory->GetGrantedSkills(), GorillaAccessory);
		}
 	
		GorillaAccessory->GetRootComponent()->SetOwner(this);
	} 
}

void AAngryCoachCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!IsAlive())
	{
		return;
	}
	
	if (IsBelowKillZ())
	{
		Die();
	}
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
	if (!Montage) return;
	if (IsPlayingMontage()) return;

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

bool AAngryCoachCharacter::PlayMontageSection(UAnimMontage* Montage, const FString& SectionName)
{
	if (!Montage->HasSections())
	{
		return false;
	}

	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp) return false ;
	
	UAnimInstance* AnimInstance = MeshComp->GetAnimInstance();
	if (!AnimInstance)
	{
		return false;
	}

	return AnimInstance->JumpToSection(SectionName);	
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

	// 기존 악세서리 액터 파괴
	CurrentAccessory->Destroy();

	CurrentAccessory = nullptr;
}

void AAngryCoachCharacter::AddAttackShape(UShapeComponent* Shape)
{
	if (!Shape)
	{
		return;
	}

	// 이미 등록된 shape인지 확인
	if (CachedAttackShapes.Contains(Shape))
	{
		return;
	}

	CachedAttackShapes.Add(Shape);

	// 델리게이트 바인딩
	Shape->OnComponentBeginOverlap.AddDynamic(this, &AAngryCoachCharacter::OnBeginOverlap);
	UE_LOG("AttackShape added and delegate bound. Total shapes: %d", CachedAttackShapes.Num());
}

void AAngryCoachCharacter::ClearAttackShapes()
{
	CachedAttackShapes.Empty();
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
			Slot = ESkillSlot::Specical;
			/*
			 * TODO
			 * Skil별 데미지 적용
			 */
			BaseDamage = 15.0f;
			if (SkillSound)
			{
				FAudioDevice::PlaySoundAtLocationOneShot(SkillSound, GetActorLocation());
			}
			break;
		}
	}

    if (Slot != ESkillSlot::None)
    {
        // 현재 공격 슬롯 기록 (이펙트 분기용)
        CurrentAttackSlot = Slot;
        SkillComponent->HandleInput(Slot);
    }
}

REGISTER_FUNCTION_NOTIFY(AAngryCoachCharacter, AttackBegin)
void AAngryCoachCharacter::AttackBegin()
{
	if (CurrentState == ECharacterState::Damaged ||
		CurrentState == ECharacterState::Attacking ||
		CurrentState == ECharacterState::Jumping)
	{
		return;
	}

	HitActors.Empty();
	if (CachedAttackShapes.Num() > 0)
	{
		for (UShapeComponent* Shape : CachedAttackShapes)
		{
			if (Shape)
			{
				Shape->SetGenerateOverlapEvents(true);
			}
		}
		SetCurrentState(ECharacterState::Attacking);
		UE_LOG("attack begin - %d shapes activated", CachedAttackShapes.Num());
	}
}

REGISTER_FUNCTION_NOTIFY(AAngryCoachCharacter, AttackEnd)
void AAngryCoachCharacter::AttackEnd()
{
    if (CachedAttackShapes.Num() > 0)
    {
        for (UShapeComponent* Shape : CachedAttackShapes)
        {
            if (Shape)
            {
                Shape->SetGenerateOverlapEvents(false);
            }
        }
        SetCurrentState(ECharacterState::Idle);
    }
    // 공격 종료 시 슬롯 리셋
    CurrentAttackSlot = ESkillSlot::None;
}

void AAngryCoachCharacter::OnBeginOverlap(UPrimitiveComponent* MyComp, UPrimitiveComponent* OtherComp, const FHitResult& HitResult)
{
	if (HitActors.Contains(HitResult.HitActor))
	{
		return;
	}

	HitActors.Add(HitResult.HitActor);
	float AppliedDamage = UGameplayStatics::ApplyDamage(HitResult.HitActor, BaseDamage, this, HitResult);
}

void AAngryCoachCharacter::OnEndOverlap(UPrimitiveComponent* MyComp, UPrimitiveComponent* OtherComp, const FHitResult& HitResult)
{
}

void AAngryCoachCharacter::OnHit(UPrimitiveComponent* MyComp, UPrimitiveComponent* OtherComp, const FHitResult& HitResult)
{
	
	// float AppliedDamage = UGameplayStatics::ApplyDamage(HitResult.HitActor, BaseDamage, this, HitResult);
	// UE_LOG("Owner : %p, damaged actor : %p", this, HitResult.HitActor);
	// UE_LOG("Damage : %f", AppliedDamage);
}

float AAngryCoachCharacter::TakeDamage(float DamageAmount, const FHitResult& HitResult, AActor* Instigator)
{
	if (CurrentHealth <= 0.0f)
	{
		return 0.0f;
	}
	
	// 피해량을 감소시키는 요인이 있다면 감도된 피해량 적용	
	float ActualDamage = DamageAmount;

	GWorld->GetPlayerCameraManager()->StartCameraShake(
		0.3, 0.3, 0.3, DamageAmount
		);

	if (ActualDamage < 10.f)
	{
		if (Hit1Sound)
		{
			FAudioDevice::PlaySoundAtLocationOneShot(Hit1Sound, GetActorLocation());
		}
	}
	else
	{
		if (Hit2Sound)
		{
			FAudioDevice::PlaySoundAtLocationOneShot(Hit2Sound, GetActorLocation());
		}
	}

	if (!IsGuard())
	{
		// 오버킬 처리
		// 남은 체력보다 데미지가 크면 남은 체력이 실질적인 데미지
		ActualDamage = FMath::Min(ActualDamage, CurrentHealth);
		CurrentHealth = FMath::Max(CurrentHealth - ActualDamage, 0.0f);
		HitReation();
	}
	else if (bCanPlayHitReactionMontage)
	{
		ActualDamage = 0.0f;
		FVector KnockbackDirection = GetActorLocation() - HitResult.ImpactPoint;
		KnockbackDirection.Z = 0.0f;
		if (KnockbackDirection.IsZero())
		{
			KnockbackDirection = -GetActorForward();
		}
		else
		{
			KnockbackDirection.Normalize();
		}

		CharacterMovement->LaunchCharacter(KnockbackDirection * KnockbackPower, true, false);
		EnableGamePadVibration();

	}

	if (CurrentHealth <= 0.0f)
	{		
		Die();
	}	
	

	//CurrentAccessory->PlayHitParticle();
	// 피격 위치를 카메라 쪽(캐릭터 앞쪽)으로 오프셋
	CurrentAccessory->SpawnHitParticleAtLocation(HitResult.HitActor->GetActorLocation() + FVector(-2,0,01));

	return ActualDamage;
}

void AAngryCoachCharacter::HitReation()
{
	// bCanPlayHitReactionMontage가 true이고 HitReationMontage가 유효할 때만 몽타주 재생
	if (!bCanPlayHitReactionMontage || !HitReationMontage)
	{
		return;
	}
	Super::HitReation();
	// 재생중인 몽타주 정지
	if (IsPlayingMontage())
	{
		StopCurrentMontage();
	}

	SetCurrentState(ECharacterState::Damaged);
	PlayMontage(HitReationMontage);
}

REGISTER_FUNCTION_NOTIFY(AAngryCoachCharacter, ClearState)
void AAngryCoachCharacter::ClearState()
{
	Super::ClearState();
	CurrentState = ECharacterState::Idle;
}

void AAngryCoachCharacter::DoGuard()
{
	if (!GuardMontage)
	{
		return;
	}

	// 피격 중, 공격 중, 점프 중에는 가드 불가능
	if (CurrentState == ECharacterState::Damaged ||
		CurrentState == ECharacterState::Attacking ||
		CurrentState == ECharacterState::Jumping)
	{
		return;
	}
	
	SetCurrentState(ECharacterState::Guard);

	if (bCanPlayHitReactionMontage)
	{
		PlayMontage(GuardMontage);
	}
	else
	{
		PlayMontage(GorillaGuardMontage);
	}
}

void AAngryCoachCharacter::StopGuard()
{
	if (!GuardMontage)
	{
		return;
	}

	SetCurrentState(ECharacterState::Idle);
	StopCurrentMontage();
}

void AAngryCoachCharacter::DelegateBindToCachedShape()
{
	// 이제 AddAttackShape에서 개별적으로 바인딩합니다
}

void AAngryCoachCharacter::Revive()
{
	// 부모 클래스의 Revive 호출 (체력/상태 리셋, Ragdoll 비활성화)
	Super::Revive();

	// CapsuleComponent collision 복원 (기본값으로)
	// 참고: Character 생성자에서 SetBlockComponent(false), SetGenerateOverlapEvents(false)로 설정됨
	// Revive에서는 필요에 따라 다시 설정
	// CapsuleComponent는 기본적으로 collision이 꺼져있으므로 그대로 둠
}

void AAngryCoachCharacter::Die()
{
	// Ragdoll
	if (SkeletalMeshComp)
	{
		SkeletalMeshComp->SetRagDollEnabled(true);
	}
	// Collision - 모든 AttackShape 비활성화
	for (UShapeComponent* Shape : CachedAttackShapes)
	{
		if (Shape)
		{
			Shape->SetBlockComponent(false);
			Shape->SetGenerateOverlapEvents(false);
		}
	}

	if (CapsuleComponent)
	{
		CapsuleComponent->SetBlockComponent(false);
		CapsuleComponent->SetGenerateOverlapEvents(false);
	}

	if (DieSound)
	{
		FAudioDevice::PlaySoundAtLocationOneShot(DieSound, GetActorLocation());
	}

	SetCurrentState(ECharacterState::Dead);
	// 낙사처리를 위해서 내부에서 체력 0으로 처리
	CurrentHealth = 0.0f;
}

void AAngryCoachCharacter::EnableGamePadVibration()
{
	if(AController* Controller = GetController())
	{
		if (AAngryCoachPlayerController* AngryController = Cast<AAngryCoachPlayerController>(Controller))
		{
			AngryController->SetGamePadVibration(true, this, VibrationDuration);
		}
	}
}

bool AAngryCoachCharacter::IsBelowKillZ()
{
	return GetActorLocation().Z <= -5.0f;
}

REGISTER_FUNCTION_NOTIFY(AAngryCoachCharacter, ToggleGorillaFormOnAccessory)
void AAngryCoachCharacter::ToggleGorillaFormOnAccessory()
{
	if (CurrentAccessory)
	{
		if (AGorillaAccessoryActor* GA = Cast<AGorillaAccessoryActor>(CurrentAccessory))
		{
			GA->ToggleGorillaForm();
		}
	}
}
