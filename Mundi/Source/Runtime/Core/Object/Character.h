#pragma once
#include "Pawn.h"
#include "ACharacter.generated.h"

class UCapsuleComponent;
class USkeletalMeshComponent;
class UCharacterMovementComponent;
class USkillComponent;

UCLASS(DisplayName = "캐릭터", Description = "캐릭터 액터 (프레임워크)")
class ACharacter : public APawn
{
public:
	GENERATED_REFLECTION_BODY()

	ACharacter();
	virtual ~ACharacter() override;

	virtual void Tick(float DeltaSecond) override;
	virtual void BeginPlay() override;
	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
	void DuplicateSubObjects() override;

	// 캐릭터 고유 기능
	virtual void Jump();
	virtual void StopJumping();
	

	float TakeDamage(float DamageAmount, const FHitResult& HitResult, AActor* Instigator) override;
	 
	UCapsuleComponent* GetCapsuleComponent() const { return CapsuleComponent; }
	UCharacterMovementComponent* GetCharacterMovement() const { return CharacterMovement; }

	// APawn 인터페이스: 파생 클래스의 MovementComponent를 노출
	virtual UPawnMovementComponent* GetMovementComponent() const override { return reinterpret_cast<UPawnMovementComponent*>(CharacterMovement); }

	//APawn에서 정의 됨
	USkeletalMeshComponent* GetMesh() const { return SkeletalMeshComp; }

	void OnBeginOverlap(UPrimitiveComponent* MyComp, UPrimitiveComponent* OtherComp, const FHitResult& HitResult) override;
	void OnEndOverlap(UPrimitiveComponent* MyComp, UPrimitiveComponent* OtherComp, const FHitResult& HitResult) override;
	void OnHit(UPrimitiveComponent* MyComp, UPrimitiveComponent* OtherComp, const FHitResult& HitResult) override;

	void Attack();

protected:
	UCapsuleComponent* CapsuleComponent = nullptr;
	UCharacterMovementComponent* CharacterMovement = nullptr;
	// 캐릭터 기본 공격 - 주먹, 발차기
	UShapeComponent* FistComponent;
	UShapeComponent* KickComponent;
	USkillComponent* SkillComponent;
	class AAccessoryActor* CurrentAccessory = nullptr; 

	float BaseDamage = 10.0f;

};
