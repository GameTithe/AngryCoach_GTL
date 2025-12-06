#pragma once
#include "Actor.h"
#include "Source/Runtime/Engine/Skill/SkillTypes.h" 
#include "AAccessoryActor.generated.h"
class UStaticMeshComponent;
class USkillBase;
class ACharacter;
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

	// === 악세서리 데이터 ===
	//UPROPERTY(EditAnywhere, Category = "Accessory", Tooltip = "이 악세서리가 부여하는 스킬들")
	TMap<ESkillSlot, USkillBase*> GrantedSkills;

	UPROPERTY(EditAnywhere, Category = "Accessory", Tooltip = "캐릭터의 어느 소켓에 부착할지 (예: hand_r, weapon_socket)")
	FName AttachSocketName;

	UPROPERTY(EditAnywhere, Category = "Accessory", Tooltip = "악세서리 이름")
	FString AccessoryName;

	UPROPERTY(EditAnywhere, Category = "Accessory", Tooltip = "악세서리 설명")
	FString Description;

public:
	UFUNCTION()
	void Equip(ACharacter* OwnerCharacter);

	UFUNCTION()
	void Unequip();

	// 스킬 getter
	const TMap<ESkillSlot, USkillBase*>& GetGrantedSkills() const { return GrantedSkills; }
};