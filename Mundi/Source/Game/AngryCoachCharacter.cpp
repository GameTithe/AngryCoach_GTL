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
#include "DecalComponent.h"
#include "SpotLightComponent.h"

AAngryCoachCharacter::AAngryCoachCharacter()
{
	// SkillComponent 생성
	HitReationMontage = RESOURCE.Load<UAnimMontage>("Data/Montages/HitReaction.montage.json");
	GuardMontage = RESOURCE.Load<UAnimMontage>("Data/Montages/Guard.montage.json");
	GorillaGuardMontage = RESOURCE.Load<UAnimMontage>("Data/Montages/Guard.montage.json");
	DacingMontage = RESOURCE.Load<UAnimMontage>("Data/Montages/DancingCoach.montage.json");
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

	// ⭐ DiscoBall 생성 및 부착 (BeginPlay에서 안전하게 스폰)
	if (!CachedDiscoBall)
	{
		FString DiscoBallPath = "Data/Prefabs/DiscoBall.prefab";
		AActor* DiscoBall = Cast<AActor>(GWorld->SpawnPrefabActor(UTF8ToWide(DiscoBallPath)));
		if (DiscoBall)
		{
			CachedDiscoBall = DiscoBall;

			// 캐릭터의 루트 컴포넌트에 부착
			if (USceneComponent* DiscoRootComp = CachedDiscoBall->GetRootComponent())
			{
				DiscoRootComp->SetupAttachment(GetRootComponent());
				DiscoRootComp->SetRelativeLocation(FVector(0.0f, 0.0f, 3.0f));
			} 
		}
	}
	StopDancingCoach();

	FString PrefabPath = "Data/Prefabs/FlowerKnife.prefab";
	AKnifeAccessoryActor * KnifeAccessory = Cast<AKnifeAccessoryActor>(GWorld->SpawnPrefabActor(UTF8ToWide(PrefabPath)));
	
	if (KnifeAccessory)
	{
		EquipAccessory(KnifeAccessory);
	
		if (SkillComponent)
		{
			SkillComponent->OverrideSkills(KnifeAccessory->GetGrantedSkills(), KnifeAccessory);
		}
		
		KnifeAccessory->GetRootComponent()->SetOwner(this);
	}
	
	
	////FString PrefabPath = "Data/Prefabs/Gorilla.prefab";
	//AGorillaAccessoryActor * GorillaAccessory = Cast<AGorillaAccessoryActor>(GWorld->SpawnPrefabActor(UTF8ToWide(PrefabPath)));
	//if (GorillaAccessory)
	//{
	//	EquipAccessory(GorillaAccessory);
	//	if (SkillComponent)
	//	{
	//		SkillComponent->OverrideSkills(GorillaAccessory->GetGrantedSkills(), GorillaAccessory);
	//	}
	//	
	//	GorillaAccessory->GetRootComponent()->SetOwner(this);
	//}
	
	 //FString PrefabPath = "Data/Prefabs/CloakAcce.prefab";
	 //ACloakAccessoryActor* CloakAccessory = Cast<ACloakAccessoryActor>(GWorld->SpawnPrefabActor(UTF8ToWide(PrefabPath)));
	 //
	 //	if (CloakAccessory)
	 //	{
	 //		EquipAccessory(CloakAccessory);
	 //
	 //		if (SkillComponent)
	 //		{
	 //			SkillComponent->OverrideSkills(CloakAccessory->GetGrantedSkills(), CloakAccessory);
	 //		}
	 //
	 //		CloakAccessory->GetRootComponent()->SetOwner(this);
	 //	}

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

	// Decal 쿨
	for (float& Last : LastDecalTime)
	{ 
		Last += DeltaSeconds;
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

	// 캐시된 AttackShapes 정리 (dangling pointer 방지)
	ClearAttackShapes();

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
void AAngryCoachCharacter::OnJumpAttackInput(const FVector& InputDirection)
{
    if (!SkillComponent)
        return;

    // 점프 중이 아니면 무시
    if (CurrentState != ECharacterState::Jumping || bIsJumpAttacking)
        return;

    // 방향 저장 (스킬에서 사용)
    JumpAttackDirection = InputDirection;
    if (JumpAttackDirection.IsZero())
    { 
        JumpAttackDirection = GetActorRotation().GetForwardVector();
        JumpAttackDirection.Z = 0.0f;  // 수평 방향만 사용
    }
    else
    {
        JumpAttackDirection.Z = 0.0f;
    }
    JumpAttackDirection.Normalize();

    CurrentAttackSlot = ESkillSlot::JumpAttack;
    BaseDamage = 10.0f;  // 점프 공격 데미지
    bIsJumpAttacking = true;
    SkillComponent->HandleInput(ESkillSlot::JumpAttack);
}

void AAngryCoachCharacter::OnAttackInput(EAttackInput Input)
{
    if (!SkillComponent)
        return;

	// ⭐ 춤 중이면 즉시 중단
	if (bIsDancing)
	{
		StopCurrentMontage(0.2f);
		bIsDancing = false;
	}

	// TODO: 점프 중이면 JumpAttack, 콤보 중이면 다음 콤보 등 상태 체크
	// 지금은 단순 매핑
	ESkillSlot Slot = ESkillSlot::None;

	// BaseDage 테스트용 하드 코딩
	switch (Input)
	{
	case EAttackInput::Light:
		{
			Slot = ESkillSlot::LightAttack;
			BaseDamage = 6.0f;
			break;
		}
	case EAttackInput::Heavy:
		{
			Slot = ESkillSlot::HeavyAttack;
			BaseDamage = 12.0f;
			break;
		}
	case EAttackInput::Skill:
		{
			Slot = ESkillSlot::Specical;
			/*
			 * TODO
			 * Skil별 데미지 적용
			 */
			BaseDamage = 18.0f;
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
    bIsJumpAttacking = false;
	  
    // 안전장치: 몽타주가 아직 재생 중이면 강제 정지
    if (IsPlayingMontage())
    { 
        StopCurrentMontage(0.1f);
    } 
    // Safety: ensure state exits Attacking even if no shapes are present
    if (GetCurrentState() == ECharacterState::Attacking)
    {
        SetCurrentState(ECharacterState::Idle);
    }
}

void AAngryCoachCharacter::PaintPlayer1Decal()
{ 

    if (!CharacterMovement)
    {
        return;
    }

    FHitResult FloorHit;
    if (!CharacterMovement->CheckFloor(FloorHit))
    {
        return;
    }

    const FVector ImpactPoint = FloorHit.ImpactPoint;
    const FVector ImpactNormal = FloorHit.ImpactNormal; 

	// 바닥과의 거리가 멀거나 ( 공중에 있을 때)
    if (FVector::Distance(ImpactPoint, LastDecalSpawnPos) < DecalMinDistance)
    {
        return;
    }
	// 너무 빠르게 다시 사용할 때
    if (LastDecalTime[0] < DecalMinInterval[0])
    {
        return;
    }
	 
	FString PrefabPath = "Data/Prefabs/CGCDecal.prefab";
	AActor* DecalActor = Cast<AActor>(GWorld->SpawnPrefabActor(UTF8ToWide(PrefabPath)));
	if (DecalActor == nullptr)
	{
		return;
	} 
	const FVector PlacePos = ImpactPoint - (ImpactNormal * DecalSurfaceOffset); 
	DecalActor->SetActorLocation(PlacePos);
	DecalActor->SetActorScale(DecalScale);
	
	// Ensure decal opacity starts at 1
	for (UActorComponent* Comp : DecalActor->GetOwnedComponents())
	{
	    if (auto* Decal = Cast<UDecalComponent>(Comp))
	    {
	        Decal->SetOpacity(1.0f);
	        break;
	    }
	}
	
	LastDecalSpawnPos = ImpactPoint; 
	LastDecalTime[0] = 0.0f;
	 
	//	CachedDecal[0] = DecalActor;
	// 1개만 생성하는 코드 약간의 버그.. 
	//if (CachedDecal[0] == nullptr)
	//{
	//	FString PrefabPath = "Data/Prefabs/CGCDecal.prefab";
	//	AActor* DecalActor = Cast<AActor>(GWorld->SpawnPrefabActor(UTF8ToWide(PrefabPath)));
	//	if (DecalActor == nullptr)
	//	{
	//		return;
	//	} 
	//	CachedDecal[0] = DecalActor;
	//} 
	//const FVector PlacePos = ImpactPoint - (ImpactNormal * DecalSurfaceOffset); 
	//CachedDecal[0]->SetActorLocation(PlacePos);
	//CachedDecal[0]->SetActorScale(DecalScale);
	//
    //// Ensure decal opacity starts at 1
    //for (UActorComponent* Comp : CachedDecal[0]->GetOwnedComponents())
    //{
    //    if (auto* Decal = Cast<UDecalComponent>(Comp))
    //    {
    //        Decal->SetOpacity(1.0f);
    //        break;
    //    }
    //}
	//
    //LastDecalSpawnPos = ImpactPoint; 
    //LastDecalTime[0] = 0.0f;
}

void AAngryCoachCharacter::PaintPlayer2Decal()
{

    if (!CharacterMovement)
    {
        return;
    }

    FHitResult FloorHit;
    if (!CharacterMovement->CheckFloor(FloorHit))
    {
        return;	
    }

    const FVector ImpactPoint = FloorHit.ImpactPoint;
    const FVector ImpactNormal = FloorHit.ImpactNormal;
	 
	FString PrefabPath = "Data/Prefabs/SHCDecal.prefab";
	AActor* DecalActor = Cast<AActor>(GWorld->SpawnPrefabActor(UTF8ToWide(PrefabPath)));
	if (DecalActor == nullptr)
	{
		return;
	}
	const FVector PlacePos = ImpactPoint - (ImpactNormal * DecalSurfaceOffset);
	DecalActor->SetActorLocation(PlacePos);
	DecalActor->SetActorScale(DecalScale);

	// Ensure decal opacity starts at 1
	for (UActorComponent* Comp : DecalActor->GetOwnedComponents())
	{
		if (auto* Decal = Cast<UDecalComponent>(Comp))
		{
			Decal->SetOpacity(1.0f);
			break;
		}
	}

	LastDecalSpawnPos = ImpactPoint;
	LastDecalTime[1] = 0.0f;

	//if (CachedDecal[1] == nullptr)
	//{
	//	FString PrefabPath = "Data/Prefabs/SHCDecal.prefab";
	//	AActor* DecalActor = Cast<AActor>(GWorld->SpawnPrefabActor(UTF8ToWide(PrefabPath)));
	//	if (!DecalActor)
	//	{
	//		return;
	//	}
	//
	//	CachedDecal[1] = DecalActor;
	//}
   	//
    //const FVector PlacePos = ImpactPoint - (ImpactNormal * DecalSurfaceOffset);
	//CachedDecal[1]->SetActorLocation(PlacePos);
	//CachedDecal[1]->SetActorScale(DecalScale);
	//
    //for (UActorComponent* Comp : CachedDecal[1]->GetOwnedComponents())
    //{
    //    if (auto* Decal = Cast<UDecalComponent>(Comp))
    //    {
    //        Decal->SetOpacity(1.0f);
    //        break;
    //    }
    //}
	//
    //LastDecalSpawnPos = ImpactPoint; 
    //LastDecalTime[1] = 0.0f;
}

void AAngryCoachCharacter::DancingCoach()
{
	bIsDancing = true;
	PlayMontage(DacingMontage);
	

	// ⭐ DiscoBall 및 자식 SpotLight 컴포넌트들 켜기
	if (CachedDiscoBall)
	{
		CachedDiscoBall->SetActorActive(true);

		// DiscoBall의 모든 자식 컴포넌트 순회
		TArray<USceneComponent*> AllComponents;
		AllComponents = CachedDiscoBall->GetSceneComponents();

		for (USceneComponent* Comp : AllComponents)
		{
			if (USpotLightComponent* SpotLight = Cast<USpotLightComponent>(Comp))
			{
				SpotLight->SetActive(true);
			}
		}
	}
	
}

void AAngryCoachCharacter::StopDancingCoach()
{
	bIsDancing = false;

	StopCurrentMontage(0.2f);  


	// ⭐ DiscoBall 및 자식 SpotLight 컴포넌트들 끄기
	if (CachedDiscoBall)
	{
		CachedDiscoBall->SetActorActive(false);

		// DiscoBall의 모든 자식 컴포넌트 순회
		TArray<USceneComponent*> AllComponents;
		AllComponents = CachedDiscoBall->GetSceneComponents();

		for (USceneComponent* Comp : AllComponents)
		{
			if (USpotLightComponent* SpotLight = Cast<USpotLightComponent>(Comp))
			{
				SpotLight->SetActive(false);
			}
		}
	}
}

void AAngryCoachCharacter::AddMovementInput(FVector Direction, float Scale)
{
	// 춤 중이고 이동 입력이 있으면 즉시 중단
	if (bIsDancing && Direction.SizeSquared() > 0.0f)
	{
		StopDancingCoach();
	}
	 
	// 부모 클래스 호출 (실제 이동 처리)
	Super::AddMovementInput(Direction, Scale);
}


void AAngryCoachCharacter::OnLanded()
{
    // 점프 공격 중 착지 시 몽타주 정지 + 공격 종료
    if (bIsJumpAttacking)
    {
        StopCurrentMontage(0.2f);
        AttackEnd();

        // 속도 초기화 (미끄러짐 방지)
        if (CharacterMovement)
        {
            CharacterMovement->SetVelocity(FVector::Zero());
        }
    }
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

	// Ragdoll
	if (SkeletalMeshComp)
	{
		SkeletalMeshComp->SetRagDollEnabled(true);
		SkeletalMeshComp->SetCollisionEnabled(ECollisionState::PhysicsOnly);
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
