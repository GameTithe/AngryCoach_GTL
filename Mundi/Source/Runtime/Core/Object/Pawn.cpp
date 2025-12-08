#include "pch.h"
#include "Pawn.h"

#include <types.hpp>

#include "SkeletalMeshComponent.h"
#include "PawnMovementComponent.h"
IMPLEMENT_CLASS(APawn)

APawn::APawn()
{
	SkeletalMeshComp = CreateDefaultSubobject<USkeletalMeshComponent>("SkeletalMeshComponent");
	SetRootComponent(SkeletalMeshComp);


	//PawnMovementComponent = CreateDefaultSubobject<UPawnMovementComponent>("PawnMovementComponent");
	//if (PawnMovementComponent)
	//{
	//	PawnMovementComponent->SetUpdatedComponent(SkeletalMeshComp);
	//} 
	/*SkeletalMeshComp->SetSkeletalMesh("James.fbx");*/
}

void APawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	 
	//  PawnMovementComponent가 없을 때는 직접 이동 
	if (!GetMovementComponent())
	{
		// 수집한 이동처리 
		AddActorWorldLocation(ConsumeMovementInputVector());
	}
}

void APawn::BeginPlay()
{
	Super::BeginPlay();
}

void APawn::PossessedBy(AController* NewController)
{
	Controller = NewController;
}

void APawn::UnPossessed()
{
	Controller = nullptr;
}

void APawn::AddMovementInput(FVector Direction, float Scale)
{
	Direction.Z = 0.0f;
	InternalMovementInputVector += Direction * Scale;	
}

float APawn::TakeDamage(float DamageAmount, const FHitResult& HitResult, AActor* Instigator)
{
	// hit actor랑 damaged actor 가 같아야함
	UE_LOG("Damage Amount : %f, Hit Actor : %p, Damaged Actor : %p, Intiagtor : %p", DamageAmount, HitResult.HitActor, this, Instigator);
	return DamageAmount;
}

FVector APawn::ConsumeMovementInputVector()
{
	FVector Ret = InternalMovementInputVector;
	InternalMovementInputVector = FVector(0, 0, 0);
	
	return Ret;
}

void APawn::AttackBegin()
{
}

void APawn::AttackEnd()
{
}

void APawn::ClearState()
{
}

void APawn::OnBeginOverlap(UPrimitiveComponent* MyComp, UPrimitiveComponent* OtherComp, const FHitResult& HitResult)
{
}

void APawn::OnEndOverlap(UPrimitiveComponent* MyComp, UPrimitiveComponent* OtherComp, const FHitResult& HitResult)
{
}

void APawn::OnHit(UPrimitiveComponent* MyComp, UPrimitiveComponent* OtherComp, const FHitResult& HitResult)
{
}

APawn::~APawn()
{
}
