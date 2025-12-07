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
	CapsuleComponent->SetBlockComponent(false);
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
}

void ACharacter::BeginPlay()
{
	Super::BeginPlay();

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
	}
}

void ACharacter::StopJumping()
{
	if (CharacterMovement)
	{
		CharacterMovement->StopJump();
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

void ACharacter::Attack()
{
	
}
