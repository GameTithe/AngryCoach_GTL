#pragma once
#include "Pawn.h"
#include "ACharacter.generated.h"

class UCapsuleComponent;
class USkeletalMeshComponent;
class UCharacterMovementComponent;

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

	// 컴포넌트 접근자
	UCapsuleComponent* GetCapsuleComponent() const { return CapsuleComponent; }
	UCharacterMovementComponent* GetCharacterMovement() const { return CharacterMovement; }
	USkeletalMeshComponent* GetMesh() const { return SkeletalMeshComp; }

	// APawn 인터페이스
	virtual UPawnMovementComponent* GetMovementComponent() const override { return reinterpret_cast<UPawnMovementComponent*>(CharacterMovement); }

protected:
	UCapsuleComponent* CapsuleComponent = nullptr;
	UCharacterMovementComponent* CharacterMovement = nullptr;
};
