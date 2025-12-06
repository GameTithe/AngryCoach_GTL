#include "pch.h"
#include "AccessoryActor.h"
#include "SceneComponent.h"
#include "StaticMeshComponent.h"
#include "ParticleSystemComponent.h"
#include "AngryCoachCharacter.h"
#include "SkeletalMeshComponent.h"
#include "SkillComponent.h"
#include "AccessoryLightAttackSkill.h"
#include "AccessoryHeavyAttackSkill.h" 

AAccessoryActor::AAccessoryActor()
{
	ObjectName = "Accessory";

	// SceneComponent를 RootComponent로 생성
	SceneRoot = CreateDefaultSubobject<USceneComponent>("SceneRoot");
	RootComponent = SceneRoot;

	// Mesh와 Particle들을 SceneRoot의 자식으로 attach
	AccessoryMesh = CreateDefaultSubobject<UStaticMeshComponent>("AccessoryMesh");
	AccessoryMesh->SetupAttachment(SceneRoot);

	TryAttackParticle = CreateDefaultSubobject<UParticleSystemComponent>("TryAttac;article");
	TryAttackParticle->SetupAttachment(SceneRoot);

	HitAttackParticle = CreateDefaultSubobject<UParticleSystemComponent>("HitAttackParticle");
	HitAttackParticle->SetupAttachment(SceneRoot);

	// 악세서리 스킬 생성 및 등록
	UAccessoryLightAttackSkill* LightSkill = NewObject<UAccessoryLightAttackSkill>();
	UAccessoryHeavyAttackSkill* HeavySkill = NewObject<UAccessoryHeavyAttackSkill>();

	GrantedSkills.Add(ESkillSlot::LightAttack, LightSkill);
	GrantedSkills.Add(ESkillSlot::HeavyAttack, HeavySkill);
}

void AAccessoryActor::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		// 로드 후 컴포넌트 포인터 재설정
		SceneRoot = nullptr;
		AccessoryMesh = nullptr;
		TryAttackParticle = nullptr;
		HitAttackParticle = nullptr;
		OwningCharacter = nullptr;

		for (UActorComponent* Comp : GetOwnedComponents())
		{
			if (auto* Scene = Cast<USceneComponent>(Comp))
			{
				if (Scene == GetRootComponent())
				{
					SceneRoot = Scene;
				}
			}

			if (auto* Mesh = Cast<UStaticMeshComponent>(Comp))
			{
				AccessoryMesh = Mesh;
			}
			else if (auto* Particle = Cast<UParticleSystemComponent>(Comp))
			{
				FString ParticleName = Particle->ObjectName.ToString();
				if (ParticleName.find("TryAttack") != std::string::npos)
				{
					TryAttackParticle = Particle;
				}
				else if (ParticleName.find("HitAttack") != std::string::npos)
				{
					HitAttackParticle = Particle;
				}
			}
		}

		// 스킬 재생성
		GrantedSkills.clear();
		UAccessoryLightAttackSkill* LightSkill = NewObject<UAccessoryLightAttackSkill>();
		UAccessoryHeavyAttackSkill* HeavySkill = NewObject<UAccessoryHeavyAttackSkill>();
		GrantedSkills.Add(ESkillSlot::LightAttack, LightSkill);
		GrantedSkills.Add(ESkillSlot::HeavyAttack, HeavySkill);
	}
}

void AAccessoryActor::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();

	// 컴포넌트 포인터 초기화
	SceneRoot = nullptr;
	AccessoryMesh = nullptr;
	TryAttackParticle = nullptr;
	HitAttackParticle = nullptr;
	OwningCharacter = nullptr;

	// 복제된 컴포넌트들을 다시 찾아서 포인터 재설정
	for (UActorComponent* Comp : GetOwnedComponents())
	{
		if (auto* Scene = Cast<USceneComponent>(Comp))
		{
			// RootComponent가 SceneComponent인지 확인
			if (Scene == GetRootComponent())
			{
				SceneRoot = Scene;
			}
		}

		if (auto* Mesh = Cast<UStaticMeshComponent>(Comp))
		{
			AccessoryMesh = Mesh;
		}
		else if (auto* Particle = Cast<UParticleSystemComponent>(Comp))
		{
			// Particle은 2개가 있으므로 ObjectName으로 구분
			FString ParticleName = Particle->ObjectName.ToString();
			if (ParticleName.find("TryAttack") != std::string::npos)
			{
				TryAttackParticle = Particle;
			}
			else if (ParticleName.find("HitAttack") != std::string::npos)
			{
				HitAttackParticle = Particle;
			}
		}
	}

	// RootComponent 재설정
	if (SceneRoot)
	{
		RootComponent = SceneRoot;
	}

	// 스킬도 다시 생성
	GrantedSkills.clear();
	UAccessoryLightAttackSkill* LightSkill = NewObject<UAccessoryLightAttackSkill>();
	UAccessoryHeavyAttackSkill* HeavySkill = NewObject<UAccessoryHeavyAttackSkill>();
	GrantedSkills.Add(ESkillSlot::LightAttack, LightSkill);
	GrantedSkills.Add(ESkillSlot::HeavyAttack, HeavySkill);
}

void AAccessoryActor::Equip(AAngryCoachCharacter* OwnerCharacter)
{
	if (!OwnerCharacter)
		return;

	// 캐릭터 참조 저장
	OwningCharacter = OwnerCharacter;

	// 1. 캐릭터의 Mesh 소켓에 부착
	// USkeletalMeshComponent* CharacterMesh = OwnerCharacter->GetMesh();
	// if (CharacterMesh && RootComponent)
	// {
	// 	// AttachSocketName이 설정되어 있으면 소켓에, 아니면 컴포넌트 자체에 부착
	// 	if (AttachSocketName.IsValid())
	// 	{
	// 		//RootComponent->SetupAttachment(CharacterMesh);
	// 		// TODO: Socket attach 구현 필요 
	// 	}
	// 	else
	// 	{
	// 		//RootComponent->SetupAttachment(CharacterMesh); 
	// 	}
	// }

	// 2. 캐릭터의 스킬 컴포넌트 찾기 및 스킬 등록
	USkillComponent* SkillComp = OwnerCharacter->GetSkillComponent();
	if (SkillComp && !GrantedSkills.empty())
	{
		SkillComp->OverrideSkills(GrantedSkills, this); 
	}
}

void AAccessoryActor::Unequip()
{
	// 캐릭터 참조 해제
	OwningCharacter = nullptr;

	// RootComponent를 detach
	if (RootComponent)
	{
		RootComponent->DetachFromParent();
		UE_LOG("[AccessoryActor] Detached from character");
	}
}
