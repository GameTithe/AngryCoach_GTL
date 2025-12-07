#include "pch.h"
#include "KnifeAccessoryActor.h"

#include "BoxComponent.h"
#include "CapsuleComponent.h"
#include "KnifeLightAttackSkill.h"
#include "KnifeHeavyAttackSkill.h"
#include "StaticMeshComponent.h"
#include "Source/Runtime/AssetManagement/ResourceManager.h"

AKnifeAccessoryActor::AKnifeAccessoryActor()
{
	ObjectName = "KnifeAccessory";
	AccessoryName = "Knife";
	Description = "Fast knife attacks";
	AttachSocketName = FName("LEFT_KNIFE");

	// 나이프 메시 로드 및 설정
	if (AccessoryMesh)
	{
		AccessoryMesh->SetStaticMesh("Data/Model/FlowerKnife.obj");
	}

	GrantedSkills.clear();

	UKnifeLightAttackSkill* LightSkill = NewObject<UKnifeLightAttackSkill>();
	UKnifeHeavyAttackSkill* HeavySkill = NewObject<UKnifeHeavyAttackSkill>();

	GrantedSkills.Add(ESkillSlot::LightAttack, LightSkill);
	GrantedSkills.Add(ESkillSlot::HeavyAttack, HeavySkill);

	AttackShape = CreateDefaultSubobject<UBoxComponent>("AttackShape");
	AttackShape->SetTag(FString("Attack"));
	if (RootComponent)
	{
		AttackShape->SetupAttachment(RootComponent);
		if (auto* Box = Cast<UBoxComponent>(AttackShape))
		{
			Box->BoxExtent = FVector(0.575f, 0.15f, 0.02f);
			Box->SetRelativeLocation(FVector(-0.4f, 0.0f, 0.0f));
		}
	}
}

void AKnifeAccessoryActor::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		GrantedSkills.clear();
		UKnifeLightAttackSkill* LightSkill = NewObject<UKnifeLightAttackSkill>();
		UKnifeHeavyAttackSkill* HeavySkill = NewObject<UKnifeHeavyAttackSkill>();
		GrantedSkills.Add(ESkillSlot::LightAttack, LightSkill);
		GrantedSkills.Add(ESkillSlot::HeavyAttack, HeavySkill);
	}
}

void AKnifeAccessoryActor::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();

	GrantedSkills.clear();
	UKnifeLightAttackSkill* LightSkill = NewObject<UKnifeLightAttackSkill>();
	UKnifeHeavyAttackSkill* HeavySkill = NewObject<UKnifeHeavyAttackSkill>();
	GrantedSkills.Add(ESkillSlot::LightAttack, LightSkill);
	GrantedSkills.Add(ESkillSlot::HeavyAttack, HeavySkill);
}
