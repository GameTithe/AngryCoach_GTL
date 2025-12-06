#include "pch.h"
#include "AccessoryActor.h"
#include "SceneComponent.h"
#include "StaticMeshComponent.h"
#include "ParticleSystemComponent.h"
#include "Character.h"
#include "SkeletalMeshComponent.h"
#include "SkillComponent.h" 

AAccessoryActor::AAccessoryActor()
{
	ObjectName = "Accessory";

	// SceneComponent를 RootComponent로 생성
	SceneRoot = CreateDefaultSubobject<USceneComponent>("SceneRoot");
	RootComponent = SceneRoot;

	// Mesh와 Particle들을 SceneRoot의 자식으로 attach
	AccessoryMesh = CreateDefaultSubobject<UStaticMeshComponent>("AccessoryMesh");
	AccessoryMesh->SetupAttachment(SceneRoot);

	TryAttackParticle = CreateDefaultSubobject<UParticleSystemComponent>("TryAttackParticle");
	TryAttackParticle->SetupAttachment(SceneRoot);

	HitAttackParticle = CreateDefaultSubobject<UParticleSystemComponent>("HitAttackParticle");
	HitAttackParticle->SetupAttachment(SceneRoot);
}

void AAccessoryActor::Equip(ACharacter* OwnerCharacter)
{
	if (!OwnerCharacter)
		return;

	// 1. 캐릭터의 Mesh 소켓에 부착
	USkeletalMeshComponent* CharacterMesh = OwnerCharacter->GetMesh();
	if (CharacterMesh && RootComponent)
	{
		// AttachSocketName이 설정되어 있으면 소켓에, 아니면 컴포넌트 자체에 부착
		if (AttachSocketName.IsValid())
		{
			RootComponent->SetupAttachment(CharacterMesh);
			// TODO: Socket attach 구현 필요
			UE_LOG("[AccessoryActor] Attached to socket: %s", AttachSocketName.ToString().c_str());
		}
		else
		{
			RootComponent->SetupAttachment(CharacterMesh);
			UE_LOG("[AccessoryActor] Attached to character mesh");
		}
	}

	// 2. 캐릭터의 스킬 컴포넌트 찾기 및 스킬 등록
	USkillComponent* SkillComp = Cast<USkillComponent>(OwnerCharacter->GetComponent(USkillComponent::StaticClass()));
	if (SkillComp && !GrantedSkills.empty())
	{
		SkillComp->OverrideSkills(GrantedSkills, this);
		UE_LOG("[AccessoryActor] Skills granted to character");
	}

	UE_LOG("[AccessoryActor] %s equipped successfully", AccessoryName.c_str());
}

void AAccessoryActor::Unequip()
{
	// RootComponent를 detach
	if (RootComponent)
	{
		RootComponent->DetachFromParent();
	}

	// TODO: 스킬 컴포넌트에서 스킬 제거
	UE_LOG("[AccessoryActor] %s unequipped", AccessoryName.c_str());
}
