#pragma once
#include "Character.h"
#include "Source/Runtime/Engine/Skill/SkillTypes.h"
#include "AAngryCoachCharacter.generated.h"

class UAnimMontage;
class USkillComponent;
class AAccessoryActor;
class USound;

// 입력 타입 (컨트롤러 → 캐릭터)
enum class EAttackInput
{
	Light,   // 약공
	Heavy,   // 강공
	Skill    // 스킬
};

class AAngryCoachCharacter : public ACharacter
{
public:
	GENERATED_REFLECTION_BODY()

	AAngryCoachCharacter();
	virtual ~AAngryCoachCharacter() override;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
	virtual void DuplicateSubObjects() override;

	// ===== 몽타주 =====
	void PlayMontage(UAnimMontage* Montage);
	void StopCurrentMontage(float BlendOutTime = 0.2f);
	bool IsPlayingMontage() const;
	bool PlayMontageSection(UAnimMontage* Montage, const FString& SectionName);

	// ===== 악세서리 =====
	void EquipAccessory(AAccessoryActor* Accessory);
	void UnequipAccessory();
	AAccessoryActor* GetCurrentAccessory() const { return CurrentAccessory; }
	void AddAttackShape(UShapeComponent* Shape);
	void ClearAttackShapes();

	// ===== 스킬 =====
    void OnAttackInput(EAttackInput Input);
    void OnJumpAttackInput(const FVector& InputDirection);
    USkillComponent* GetSkillComponent() const { return SkillComponent; }
    ESkillSlot GetCurrentAttackSlot() const { return CurrentAttackSlot; }
    FVector GetJumpAttackDirection() const { return JumpAttackDirection; }
    bool IsJumpAttacking() const { return bIsJumpAttacking; }
    USound* GetSkillSound() const { return SkillSound; }

	// ==== Decal Painting ====
	void PaintPlayer1Decal();
	void PaintPlayer2Decal();

	// === 춤 ====
	void DancingCoach();
	void StopDancingCoach();
	AActor* CachedDiscoBall;

	// ===== 이동 =====
	void AddMovementInput(FVector Direction, float Scale) override;

	// 착지 시 호출 (CharacterMovementComponent에서 호출)
	void OnLanded();

	// 노티파이용 함수
	void AttackBegin() override;
	void AttackEnd() override;

	bool bIsCGC = true;
	// 충돌
	void OnBeginOverlap(UPrimitiveComponent* MyComp, UPrimitiveComponent* OtherComp, const FHitResult& HitResult) override;
	void OnEndOverlap(UPrimitiveComponent* MyComp, UPrimitiveComponent* OtherComp, const FHitResult& HitResult) override;
	void OnHit(UPrimitiveComponent* MyComp, UPrimitiveComponent* OtherComp, const FHitResult& HitResult) override;

	float TakeDamage(float DamageAmount, const FHitResult& HitResult, AActor* Instigator) override;

	void HitReation() override;
	void Revive() override;  // 라운드 시작 시 캐릭터 복원 (Ragdoll 해제, collision 복원 등)
	void ClearState() override;

	void DoGuard();
	void StopGuard();

	bool IsGuard() const { return CurrentState == ECharacterState::Guard; }
	
	bool bCanPlayHitReactionMontage = true; // New flag to control hit reaction montage playback

	UPROPERTY(EditAnywhere, Category="[캐릭터]")
	float KnockbackPower = 10.0f;

	UPROPERTY(EditAnywhere, Category="[캐릭터]")
	float VibrationDuration = 1.0f;
	
	// 킬존 진입 여부 검사 - 낙사
	bool IsBelowKillZ();
	// 애니메이션 노티파이용 함수
	void ToggleGorillaFormOnAccessory();

	// 춤 추고 있는 지 
	bool GetIsDancing() { return bIsDancing; }
protected:

	//TODO: 다른 컴포넌트를 만들어서 관리하는게 맞는 방법 같은데,
	// 게임을 위한 컴포넌트를 cpp로 추가하기 싫어서 coach한테,  인자 몇개 만
	// Decal 생성 쿨타임
    UPROPERTY(EditAnywhere, Category="[Decal]")
    FVector DecalScale = FVector(0.2f, 2.0f, 2.0f); 

    UPROPERTY(EditAnywhere, Category="[Decal]")
    float DecalSurfaceOffset = 0.1f;

    UPROPERTY(EditAnywhere, Category="[Decal]")
    float DecalMinDistance = 0.6f;   
	TArray<float> DecalMinInterval = { 0.8f , 0.8f };
	TArray<float> LastDecalTime = { 0,0 };

    FVector LastDecalSpawnPos = FVector::Zero(); 
    
private:
	// 델리게이트 바인딩 헬퍼 함수
	void DelegateBindToCachedShape();
	// 죽었을 때 필요한 로직 모아둔 헬퍼
	void Die();
	// 게임패드 진동 활성화 헬퍼
	void EnableGamePadVibration();

protected:
    // 스킬/악세서리
    USkillComponent* SkillComponent = nullptr;
    AAccessoryActor* CurrentAccessory = nullptr;
    TArray<UShapeComponent*> CachedAttackShapes;

    // 현재 공격 슬롯(약/강/스페셜 분기용)
    ESkillSlot CurrentAttackSlot = ESkillSlot::None;

    // 점프 공격 방향 (스킬에서 사용)
    FVector JumpAttackDirection = FVector::Zero();
    bool bIsJumpAttacking = false;

	UAnimMontage* HitReationMontage = nullptr;
	UAnimMontage* GuardMontage = nullptr;
	UAnimMontage* GorillaGuardMontage = nullptr;
	USound* Hit1Sound = nullptr;
	USound* Hit2Sound = nullptr;
	USound* SkillSound = nullptr;
	USound* DieSound = nullptr;

	TArray<AActor*> HitActors;

	// 춤 관련
	bool bIsDancing = false;  // 춤 중인지 플래그

	// decal
	TArray<AActor*> CachedDecal = { nullptr, nullptr } ;

	UAnimMontage* DacingMontage = nullptr;
};
