#include "pch.h"
#include "Character.h"
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
	//SkillComponent = CreateDefaultSubobject<USkillComponent>("SkillComponent");
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

    // Hardcode: equip FlowKnife prefab on PIE start (moved from Lua to C++)
    if (GWorld && GWorld->bPie)
    {
        FWideString KnifePath = UTF8ToWide("Data/Prefabs/FlowKnife.prefab");
        AActor* Spawned = GWorld->SpawnPrefabActor(KnifePath);
        if (!Spawned)
        {
             return;
        }

        if (AAccessoryActor* Accessory = Cast<AAccessoryActor>(Spawned))
        {
            EquipAccessory(Accessory);
 
             if (SkillComponent)
             {
                SkillComponent->OverrideSkills(Accessory->GetGrantedSkills(), Accessory);
             }
        }
    }
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
    CurrentAccessory = nullptr;

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

void ACharacter::EquipAccessory(AAccessoryActor* Accessory)
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

void ACharacter::UnequipAccessory()
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
	 

	CurrentAccessory = nullptr;
}
