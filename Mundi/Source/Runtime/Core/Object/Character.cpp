#include "pch.h"
#include "Character.h"
#include "CapsuleComponent.h"
#include "SkeletalMeshComponent.h"
#include "CharacterMovementComponent.h"
#include "SkillComponent.h"
#include "AccessoryActor.h"
#include "InputManager.h"
#include "ObjectMacros.h" 
ACharacter::ACharacter()
{
	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>("CapsuleComponent");
	//CapsuleComponent->SetSize();

	SetRootComponent(CapsuleComponent);

	if (SkeletalMeshComp)
	{
		SkeletalMeshComp->SetupAttachment(CapsuleComponent);

		//SkeletalMeshComp->SetRelativeLocation(FVector());
		//SkeletalMeshComp->SetRelativeScale(FVector());
	}

	CharacterMovement = CreateDefaultSubobject<UCharacterMovementComponent>("CharacterMovement");
	if (CharacterMovement)
	{
		CharacterMovement->SetUpdatedComponent(CapsuleComponent);
	}

	// SkillComponent 생성
	SkillComponent = CreateDefaultSubobject<USkillComponent>("SkillComponent");
}

ACharacter::~ACharacter()
{

}

void ACharacter::Tick(float DeltaSecond)
{
	Super::Tick(DeltaSecond);

	// 스킬 입력 처리
	HandleSkillInput();
}

void ACharacter::BeginPlay()
{
    Super::BeginPlay();

	// 자동으로 부착된 AccessoryActor를 Equip 처리
	//if (SkeletalMeshComp)
	//{
	//	TArray<USceneComponent*> AttachedComponents;
	//	//SkeletalMeshComp->GetChildrenComponents(false, AttachedComponents);
	//
	//	for (USceneComponent* Child : AttachedComponents)
	//	{
	//		AActor* ChildOwner = Child->GetOwner();
	//		if (ChildOwner && ChildOwner != this)
	//		{
	//			AAccessoryActor* Accessory = Cast<AAccessoryActor>(ChildOwner);
	//			if (Accessory)
	//			{
	//				Accessory->Equip(this);  
	//			}
	//		}
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

        for (UActorComponent* Comp : GetOwnedComponents())
        {
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

    for (UActorComponent* Comp : GetOwnedComponents())
    {
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
    }

    // Ensure movement component tracks the correct updated component
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
		// 점프 scale을 조절할 때 사용,
		// 지금은 비어있음
		CharacterMovement->StopJump();
	}
}

void ACharacter::HandleSkillInput()
{
	if (!SkillComponent)
		return;

	UInputManager& InputMgr = UInputManager::GetInstance();

	// F키 - Light Attack
	if (InputMgr.IsKeyPressed('F'))
	{
		SkillComponent->HandleInput(ESkillSlot::LightAttack);
	}

	// G키 - Heavy Attack
	if (InputMgr.IsKeyPressed('G'))
	{
		SkillComponent->HandleInput(ESkillSlot::HeavyAttack);
	}
}
