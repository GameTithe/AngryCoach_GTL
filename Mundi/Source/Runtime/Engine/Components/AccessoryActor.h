#pragma once
#include "Actor.h"
#include "Source/Runtime/Engine/Skill/SkillTypes.h"
#include "AAccessoryActor.generated.h"

class UStaticMeshComponent;
class USkillBase;
class AAngryCoachCharacter;
class UParticleSystemComponent;

UCLASS(DisplayName = "악세서리", Description = "악세서리 액터입니다")

class AAccessoryActor : public AActor
{
public:
    GENERATED_REFLECTION_BODY();
    AAccessoryActor();

    // === 컴포넌트 ===
    USceneComponent* SceneRoot;
    UStaticMeshComponent* AccessoryMesh;

    // 타격 시도 Particle
    UParticleSystemComponent* TryAttackParticle;

    // 타격 성공 Particle
    UParticleSystemComponent* HitAttackParticle;

    // 전기 타격 Particle (일회성 스폰용)
    UParticleSystemComponent* ElectricHitParticle;

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
    void SpawnElectricHitParticleAtLocation(const FVector& Location);

protected:
    // 자식 클래스에서 호출하는 헬퍼 함수
    template <typename T>
    void CreateAttackShape(const FName& Name);

private:
    // 헬퍼 함수
    void SetAttackShapeNameAndAttach(const FName& Name);

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
