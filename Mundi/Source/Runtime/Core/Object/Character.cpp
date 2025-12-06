#include "pch.h"
#include "Character.h"

#include "PrimitiveComponent.h"
#include "BoxComponent.h"
#include "CapsuleComponent.h"
#include "SkeletalMeshComponent.h"
#include "CharacterMovementComponent.h" 
#include "ObjectMacros.h"

ACharacter::ACharacter()
{
	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>("CapsuleComponent");
	CapsuleComponent->SetTag("CharacterCollider");
	//CapsuleComponent->SetSize();

	SetRootComponent(CapsuleComponent);
	CapsuleComponent->SetBlockComponent(false);

	if (SkeletalMeshComp)
	{
		SkeletalMeshComp->SetupAttachment(CapsuleComponent);		

		//SkeletalMeshComp->SetRelativeLocation(FVector());
		//SkeletalMeshComp->SetRelativeScale(FVector());
	}

	
	FistComponent = CreateDefaultSubobject<UCapsuleComponent>("FistComponent");
	if (FistComponent)
	{
		FistComponent->SetupAttachment(CapsuleComponent);
		FistComponent->SetBlockComponent(false);
		FistComponent->SetTag("Fist");
	}
	
	KickComponent = CreateDefaultSubobject<UCapsuleComponent>("KickComponent");
	if (KickComponent)
	{
		KickComponent->SetupAttachment(CapsuleComponent);
		KickComponent->SetBlockComponent(false);
		FistComponent->SetTag("Kick");
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

	if (FistComponent)
	{
		FistComponent->OnComponentHit.AddDynamic(this, &ACharacter::OnHit);
		FistComponent->OnComponentBeginOverlap.AddDynamic(this, &ACharacter::OnBeginOverlap);
		FistComponent->OnComponentEndOverlap.AddDynamic(this, &ACharacter::OnEndOverlap);
	}

	if (KickComponent)
	{
		KickComponent->OnComponentHit.AddDynamic(this, &ACharacter::OnHit);
		KickComponent->OnComponentBeginOverlap.AddDynamic(this, &ACharacter::OnBeginOverlap);
		KickComponent->OnComponentEndOverlap.AddDynamic(this, &ACharacter::OnEndOverlap);
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
    	FistComponent = nullptr;
    	KickComponent = nullptr;

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
        	
        	if (auto* Shape = Cast<UShapeComponent>(Comp))
        	{
        		if (CompTag == FString("Fist"))
        		{
        			FistComponent = Shape;
        		}
        		if (CompTag == FString("Kick"))
        		{
        			KickComponent = Shape;
        		}
        		Shape->SetBlockComponent(false);
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
	FistComponent = nullptr;
	KickComponent = nullptr;

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
        else if (auto* Shape = Cast<UShapeComponent>(Comp))
        {
        	if (CompName == FName("FistComponent"))
        	{
        		FistComponent = Shape;
        	}
        	else if (CompName == FName("KickComponent"))
        	{
        		KickComponent = Shape;
        	}
        	Shape->SetBlockComponent(false);
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

void ACharacter::Attack()
{
	if (FistComponent)
	{
		FistComponent->SetBlockComponent(true);
		FistComponent->SetGenerateOverlapEvents(true);
	}
}
