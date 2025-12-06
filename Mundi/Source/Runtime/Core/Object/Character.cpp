#include "pch.h"
#include "Character.h"
#include "CapsuleComponent.h"
#include "SkeletalMeshComponent.h"
#include "CharacterMovementComponent.h"

ACharacter::ACharacter()
{
	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>("CapsuleComponent");
	SetRootComponent(CapsuleComponent);

	if (SkeletalMeshComp)
	{
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
}

void ACharacter::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		// 컴포넌트 포인터 재바인딩
		CapsuleComponent = nullptr;
		CharacterMovement = nullptr;

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
