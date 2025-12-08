#pragma once
#include "Actor.h"
#include "Source/Runtime/Engine/Skill/SkillTypes.h"
#include "AAccessoryActor.generated.h"

class UStaticMeshComponent;
class USkillBase;
class AAngryCoachCharacter;
class UParticleSystemComponent;

struct FActiveParticle
{
    UParticleSystemComponent* Comp;
    float TimeRemaining;

};
UCLASS(DisplayName = "악세서리", Description = "악세서리 액터입니다")
class AAccessoryActor : public AActor
{
public:
    GENERATED_REFLECTION_BODY();
    AAccessoryActor();

    TArray<FActiveParticle> ActiveParticles;
    
    // === 컴포넌트 ===
    USceneComponent* SceneRoot;
    UStaticMeshComponent* AccessoryMesh;

    // 타격 시도 Particle
    UParticleSystemComponent* TryAttackParticle;

    // 타격 성공 Particle
    UParticleSystemComponent* HitAttackParticle; 

    // 기본 Particle
    UParticleSystemComponent* BaseEffectParticle;

    UShapeComponent* AttackShape = nullptr;

    // === 악세서리 데이터 ===
    //UPROPERTY(EditAnywhere, Category = "Accessory", Tooltip = "이 악세서리가 부여하는 스킬들")
    TMap<ESkillSlot, USkillBase*> GrantedSkills;

    UPROPERTY(EditAnywhere, Category = "Accessory", Tooltip = "캐릭터의 어느 소켓에 부착할지 (예: hand_r, weapon_socket)")
    FName AttachSocketName;

    UPROPERTY(EditAnywhere, Category = "Accessory", Tooltip = "악세서리 이름")
    FString AccessoryName;

    UPROPERTY(EditAnywhere, Category = "Accessory", Tooltip = "악세서리 설명")
    FString Description;

    void DuplicateSubObjects() override;
    void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

    void PlayTryParticle();
    void StopTryParticle();

    void PlayHitParticle();
    void StopHitParticle();

    void SpawnHitParticleAtLocation(const FVector& Location);
    

    // Tick 오버라이드 (파티클 자동 종료용)
    void Tick(float DeltaTime) override;

protected:
    // 자식 클래스에서 호출하는 헬퍼 함수
    template <typename T>
    void CreateAttackShape(const FName& Name);

private:
    // 헬퍼 함수
    void SetAttackShapeNameAndAttach(const FName& Name);

    // 파티클 자동 종료용 타이머
    float HitParticleElapsedTime = 0.0f; 

    float ElectricParticleElapsedTime = 0.0f;
    bool bElectricParticleActive = false;

    const float ParticleLifetime = 3.3f;  // 파티클 유지 시간 (초)

protected:
    AAngryCoachCharacter* OwningCharacter = nullptr;

public:
    UFUNCTION()
    virtual void Equip(AAngryCoachCharacter* OwnerCharacter);

    UFUNCTION()
    virtual void Unequip();

    // 스킬 getter
    const TMap<ESkillSlot, USkillBase*>& GetGrantedSkills() const { return GrantedSkills; }
    AAngryCoachCharacter* GetOwningCharacter() const { return OwningCharacter; }

private:
    // Runtime state guard to avoid duplicate Play notifications from montage
    bool bTryParticleActive = false;
};

template <typename T>
void AAccessoryActor::CreateAttackShape(const FName& Name)
{
    static_assert(std::is_base_of<UShapeComponent, T>::value, "T는 ShapeComponent가 아닙니다.");
    AttackShape = CreateDefaultSubobject<T>("AttackShape");

    SetAttackShapeNameAndAttach(Name);
}
