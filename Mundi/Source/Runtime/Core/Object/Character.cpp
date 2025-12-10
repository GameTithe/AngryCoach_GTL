#include "pch.h"
#include "Character.h"

#include "PrimitiveComponent.h"
#include "BoxComponent.h"
#include "CapsuleComponent.h"
#include "SkeletalMeshComponent.h"
#include "CharacterMovementComponent.h" 
#include "SkillComponent.h"
#include "AccessoryActor.h"
#include "InputManager.h"
#include "ObjectMacros.h" 
#include "World.h"
#include "Source/Runtime/Core/Misc/PathUtils.h"
#include "Source/Runtime/Engine/Components/AccessoryActor.h"
ACharacter::ACharacter()
{
	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>("CapsuleComponent");
	CapsuleComponent->SetTag("CharacterCollider");

	SetRootComponent(CapsuleComponent);
	CapsuleComponent->SetBlockComponent(true);
	CapsuleComponent->SetGenerateOverlapEvents(false);

	if (SkeletalMeshComp)
	{
		SkeletalMeshComp->SetupAttachment(CapsuleComponent);
		SkeletalMeshComp->SetupAttachment(CapsuleComponent);
	}
	 
	CharacterMovement = CreateDefaultSubobject<UCharacterMovementComponent>("CharacterMovement");
	if (CharacterMovement)
	{
		CharacterMovement->SetUpdatedComponent(CapsuleComponent);
	}
}

ACharacter::~ACharacter()
{
}

void ACharacter::Tick(float DeltaSecond)
{
	Super::Tick(DeltaSecond);

	// 상태 업데이트를 위한 변위 계산
	{
		FVector CurrentLocation = GetActorLocation();
		FVector Displacement = CurrentLocation - LastFrameLocation;
		Displacement.Z = 0.0f;

		float CurrentSpeedSq = 0.0f;
		if (DeltaSecond > KINDA_SMALL_NUMBER)
		{
			CurrentSpeedSq = Displacement.SizeSquared() / (DeltaSecond * DeltaSecond);
		}

		LastFrameLocation = CurrentLocation;
		UpdateCharacterState(CurrentSpeedSq);
	}	
}

void ACharacter::BeginPlay()
{
	Super::BeginPlay();

	LastFrameLocation = GetActorLocation();
	// Hardcode: equip FlowKnife prefab on PIE start (moved from Lua to C++)
	//if (GWorld && GWorld->bPie)
	//{
	//	FWideString KnifePath = UTF8ToWide("Data/Prefabs/FlowKnife.prefab");
	//	AActor* Spawned = GWorld->SpawnPrefabActor(KnifePath);
	//	if (!Spawned)
	//	{
	//		return;
	//	}
	//}
}
void ACharacter::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

    if (bInIsLoading)
    {
        // Rebind important component pointers after load (prefab/scene)
        CapsuleComponent = nullptr;
        CharacterMovement = nullptr;

        SkillComponent = nullptr;
        CurrentAccessory = nullptr;
        SkeletalMeshComp = nullptr;  // APawn 멤버 - 재바인딩 필요

        for (UActorComponent* Comp : GetOwnedComponents())
        {
        	if (!Comp)
        	{
        		continue;
        	}

        	FString CompTag = Comp->GetTag();

            if (auto* Cap = Cast<UCapsuleComponent>(Comp))
            {
            	if (CompTag == FString("CharacterCollider"))
            	{
            		CapsuleComponent = Cap;
            	}
            }

            if (auto* Move = Cast<UCharacterMovementComponent>(Comp))
            {
                CharacterMovement = Move;
            }

            if (auto* Skill = Cast<USkillComponent>(Comp))
            {
                SkillComponent = Skill;
            }

            if (auto* SkelMesh = Cast<USkeletalMeshComponent>(Comp))
            {
                SkeletalMeshComp = SkelMesh;  // APawn 멤버 재바인딩
            }
        }

		if (CharacterMovement)
		{
			USceneComponent* Updated = CapsuleComponent ? reinterpret_cast<USceneComponent*>(CapsuleComponent)
			                                            : GetRootComponent();
			CharacterMovement->SetUpdatedComponent(Updated);
		}
	}
}

void ACharacter::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();

    CapsuleComponent = nullptr;
    CharacterMovement = nullptr;
    SkillComponent = nullptr;
    CurrentAccessory = nullptr;
	SkeletalMeshComp = nullptr;  // APawn 멤버 - 재바인딩 필요

    for (UActorComponent* Comp : GetOwnedComponents())
    {
    	if (!Comp)
    	{
    		continue;
    	}

    	FName CompName = Comp->GetName();

        if (auto* Cap = Cast<UCapsuleComponent>(Comp))
        {
            CapsuleComponent = Cap;
        }
        else if (auto* Move = Cast<UCharacterMovementComponent>(Comp))
        {
            CharacterMovement = Move;
        }
        else if (auto* Skill = Cast<USkillComponent>(Comp))
        {
            SkillComponent = Skill;
        }
        else if (auto* SkelMesh = Cast<USkeletalMeshComponent>(Comp))
        {
            SkeletalMeshComp = SkelMesh;  // APawn 멤버 재바인딩
        }
        
    }

	if (CharacterMovement)
	{
		USceneComponent* Updated = CapsuleComponent ? reinterpret_cast<USceneComponent*>(CapsuleComponent)
		                                            : GetRootComponent();
		CharacterMovement->SetUpdatedComponent(Updated);
	}
}

void ACharacter::Jump()
{
	if (CharacterMovement)
	{
		CharacterMovement->DoJump();
		SetCurrentState(ECharacterState::Jumping);
		UE_LOG("Jumping");
	}
}

void ACharacter::StopJumping()
{
	if (CharacterMovement)
	{
		CharacterMovement->StopJump();
		SetCurrentState(ECharacterState::Idle);
		UE_LOG("Stopjump");
	}
}

float ACharacter::TakeDamage(float DamageAmount, const FHitResult& HitResult, AActor* Instigator)
{
	return Super::TakeDamage(DamageAmount, HitResult, Instigator);
}

void ACharacter::OnBeginOverlap(UPrimitiveComponent* MyComp, UPrimitiveComponent* OtherComp, const FHitResult& HitResult)
{
	Super::OnBeginOverlap(MyComp, OtherComp, HitResult);
	UE_LOG("OnBeginOverlap");
}

void ACharacter::OnEndOverlap(UPrimitiveComponent* MyComp, UPrimitiveComponent* OtherComp, const FHitResult& HitResult)
{
	Super::OnEndOverlap(MyComp, OtherComp, HitResult);
	UE_LOG("OnEndOverlap");
}

void ACharacter::OnHit(UPrimitiveComponent* MyComp, UPrimitiveComponent* OtherComp, const FHitResult& HitResult)
{
	Super::OnHit(MyComp, OtherComp, HitResult);
	UE_LOG("OnHit");
}

void ACharacter::AttackBegin()
{
	Super::AttackBegin();
}

void ACharacter::AttackEnd()
{
	Super::AttackEnd();
}

float ACharacter::GetHealthPercent() const
{
	if (MaxHealth <= KINDA_SMALL_NUMBER)
	{
		UE_LOG("최대 체력이 0 이하입니다.");
		return 0.0f;
	}
	
	float HeathPercent = CurrentHealth / MaxHealth;
	return HeathPercent;
}

void ACharacter::UpdateCharacterState(float CurrentSpeedSq)
{
	// 높은 우선 순위 상태면 return
	if (CurrentState == ECharacterState::Attacking ||
		CurrentState == ECharacterState::Dead ||
		CurrentState != ECharacterState::Damaged)
	{
		return;
	}

	if (CurrentSpeedSq > 1.0f)
	{
		CurrentState = ECharacterState::Walking;
		// UE_LOG("UpdateCharacterState : walking");
	}
	else
	{
		CurrentState = ECharacterState::Idle;
		// UE_LOG("UpdateCharacterState : idle");
	}
}

bool ACharacter::CanAttack()
{
	return CurrentState != ECharacterState::Attacking &&
		CurrentState != ECharacterState::Walking &&
		CurrentState != ECharacterState::Damaged;
}

void ACharacter::HitReation()
{
}

void ACharacter::Revive()
{
	// 체력/상태 리셋
	CurrentHealth = MaxHealth;
	CurrentState = ECharacterState::Idle;

	// Ragdoll 비활성화 및 포즈 리셋
	if (SkeletalMeshComp)
	{
		SkeletalMeshComp->SetPhysicsAnimationState(EPhysicsAnimationState::AnimationDriven);
		SkeletalMeshComp->ResetToBindPose();
	}
}
