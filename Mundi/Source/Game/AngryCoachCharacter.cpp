#include "pch.h"
#include "AngryCoachCharacter.h"

#include <types.hpp>

#include "SkeletalMeshComponent.h"
#include "SkillComponent.h"
#include "AccessoryActor.h"
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
		//
		// 	PunchAccessory->GetRootComponent()->SetOwner(this);
		// }

		// AKnifeAccessoryActor* KnifeAccessory = GWorld->SpawnActor<AKnifeAccessoryActor>();
		 
		// FString PrefabPath = "Data/Prefabs/CloakAcce.prefab";
		// ACloakAccessoryActor* CloakAccessory = Cast<ACloakAccessoryActor>(GWorld->SpawnPrefabActor(UTF8ToWide(PrefabPath)));
		//
		// if (CloakAccessory)
		// {
		// 	EquipAccessory(CloakAccessory);
		// 	 FString PrefabPath = "Data/Prefabs/FlowerKnife.prefab";
		// 	 AKnifeAccessoryActor * KnifeAccessory = Cast<AKnifeAccessoryActor>(GWorld->SpawnPrefabActor(UTF8ToWide(PrefabPath)));
		//
		// 	 if (KnifeAccessory)
		// 	 {
		//  		EquipAccessory(KnifeAccessory);
		// 	
		// 		if (SkillComponent)
		// 		{
		// 			SkillComponent->OverrideSkills(CloakAccessory->GetGrantedSkills(), CloakAccessory);
		// 		}
		// 		
		// 		CloakAccessory->GetRootComponent()->SetOwner(this);
		// 	}
		//
		// // FString PrefabPath = "Data/Prefabs/Gorilla.prefab";
		// // AGorillaAccessoryActor * GorillaAccessory = Cast<AGorillaAccessoryActor>(GWorld->SpawnPrefabActor(UTF8ToWide(PrefabPath)));
		// //
		// // if (GorillaAccessory)
		// // {
		// // 	EquipAccessory(GorillaAccessory);
		// //
		// // 	if (SkillComponent)
		// // 	{
		// // 		SkillComponent->OverrideSkills(GorillaAccessory->GetGrantedSkills(), GorillaAccessory);
		// // 	}
		// // 	
		// // 	GorillaAccessory->GetRootComponent()->SetOwner(this);
		// // }
		// 	
		//  	if (SkillComponent)
		//  	{
		//  		SkillComponent->OverrideSkills(KnifeAccessory->GetGrantedSkills(), KnifeAccessory);
		//  	}
		//  }

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
	UE_LOG("[AAngryCoachCharacter::PlayMontage] Called!");

	if (!Montage)
	{
		UE_LOG("[PlayMontage] Montage is null!");
		return;
	}
	if (IsPlayingMontage())
	{
		UE_LOG("[PlayMontage] Montage is already playing!");
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

bool AAngryCoachCharacter::PlayMontageSection(UAnimMontage* Montage, const FString& SectionName)
{
	if (!Montage->HasSections())
	{
		UE_LOG("몽타주에 섹션이 없습니다.");
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

void AAngryCoachCharacter::SetAttackShape(UShapeComponent* Shape)
{
	if (!Shape)
	{
		return;
	}
	
	if (CachedAttackShape == Shape)
	{
		return;
	}

	CachedAttackShape = Shape;

	DelegateBindToCachedShape();
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
	
	if (CachedAttackShape)
	{
		CachedAttackShape->SetBlockComponent(true);
		SetCurrentState(ECharacterState::Attacking);
		UE_LOG("attack begine");
	}
}

REGISTER_FUNCTION_NOTIFY(AAngryCoachCharacter, AttackEnd)
void AAngryCoachCharacter::AttackEnd()
{	
	if (CachedAttackShape)
	{
		CachedAttackShape->SetBlockComponent(false);
		SetCurrentState(ECharacterState::Idle);
		UE_LOG("attack end");
	}
}

void AAngryCoachCharacter::OnBeginOverlap(UPrimitiveComponent* MyComp, UPrimitiveComponent* OtherComp, const FHitResult& HitResult)
{
	UE_LOG("OnBeginOverlap");
}

void AAngryCoachCharacter::OnEndOverlap(UPrimitiveComponent* MyComp, UPrimitiveComponent* OtherComp, const FHitResult& HitResult)
{
	UE_LOG("OnEndOverlap");
}

void AAngryCoachCharacter::OnHit(UPrimitiveComponent* MyComp, UPrimitiveComponent* OtherComp, const FHitResult& HitResult)
{
	
	float AppliedDamage = UGameplayStatics::ApplyDamage(HitResult.HitActor, BaseDamage, this, HitResult);
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

		float KnockbackPower = 10.0f;
		CharacterMovement->LaunchCharacter(KnockbackDirection * KnockbackPower, true, false);

		// float KnockbackDistance = 0.2f;
		// RootComponent->AddWorldOffset(KnockbackDirection);
	}

	if (CurrentHealth <= 0.0f)
	{
		CurrentState = ECharacterState::Dead;
		Die();
	}	
	
	UE_LOG("[TakeDamage] Owner %p, insti %p cur %f", this, Instigator, CurrentHealth);
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
	CachedAttackShape->OnComponentHit.AddDynamic(this, &AAngryCoachCharacter::OnHit);
	UE_LOG("Delegate Bind");
}

void AAngryCoachCharacter::Revive()
{
	// 부모 클래스의 Revive 호출 (체력/상태 리셋, Ragdoll 비활성화)
	Super::Revive();

	// CapsuleComponent collision 복원 (기본값으로)
	// 참고: Character 생성자에서 SetBlockComponent(false), SetGenerateOverlapEvents(false)로 설정됨
	// Revive에서는 필요에 따라 다시 설정
	// CapsuleComponent는 기본적으로 collision이 꺼져있으므로 그대로 둠

	UE_LOG("[AngryCoachCharacter] Revived! HP=%f", CurrentHealth);
}

void AAngryCoachCharacter::Die()
{
	// Ragdoll
	if (SkeletalMeshComp)
	{
		SkeletalMeshComp->ChangePhysicsState();
	}
	// Collision
	if (CachedAttackShape)
	{
		CachedAttackShape->SetBlockComponent(false);
		CachedAttackShape->SetGenerateOverlapEvents(false);
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
