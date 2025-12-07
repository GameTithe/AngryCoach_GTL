#pragma once
#include "Character.h"
#include "AAngryCoachCharacter.generated.h"

class UAnimMontage;
class USkillComponent;
class AAccessoryActor;

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

	// ===== 악세서리 =====
	void EquipAccessory(AAccessoryActor* Accessory);
	void UnequipAccessory();
	AAccessoryActor* GetCurrentAccessory() const { return CurrentAccessory; }
	void SetAttackShape(UShapeComponent* Shape);

	// ===== 스킬 =====
	void OnAttackInput(EAttackInput Input);
	USkillComponent* GetSkillComponent() const { return SkillComponent; }

	// 노티파이용 함수
	void AttackBegin() override;
	void AttackEnd() override;

	// 충돌
	void OnBeginOverlap(UPrimitiveComponent* MyComp, UPrimitiveComponent* OtherComp, const FHitResult& HitResult) override;
	void OnEndOverlap(UPrimitiveComponent* MyComp, UPrimitiveComponent* OtherComp, const FHitResult& HitResult) override;
	void OnHit(UPrimitiveComponent* MyComp, UPrimitiveComponent* OtherComp, const FHitResult& HitResult) override;

	float TakeDamage(float DamageAmount, const FHitResult& HitResult, AActor* Instigator) override;

private:
	// 델리게이트 바인딩 헬퍼 함수
	void DelegateBindToCachedShape();

protected:
	// 스킬/악세서리
	USkillComponent* SkillComponent = nullptr;
	AAccessoryActor* CurrentAccessory = nullptr;
	UShapeComponent* CachedAttackShape = nullptr;

	float MaxHealth = 100.0f;
	float CurrentHealth = MaxHealth;
};
