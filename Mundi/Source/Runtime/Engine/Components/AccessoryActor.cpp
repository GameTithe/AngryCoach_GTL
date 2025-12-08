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
	//SceneRoot = CreateDefaultSubobject<USceneComponent>("SceneRoot");
	//RootComponent = SceneRoot;

	// Mesh와 Particle들을 SceneRoot의 자식으로 attach
	AccessoryMesh = CreateDefaultSubobject<UStaticMeshComponent>("AccessoryMesh");
	RootComponent = AccessoryMesh;

	//AccessoryMesh->SetupAttachment(SceneRoot);

	TryAttackParticle = CreateDefaultSubobject<UParticleSystemComponent>("TryAttackParticle");
	TryAttackParticle->ObjectName = FName("TryAttackParticle");
	TryAttackParticle->SetupAttachment(RootComponent);

	HitAttackParticle = CreateDefaultSubobject<UParticleSystemComponent>("HitAttackParticle");
	HitAttackParticle->ObjectName = FName("HitAttackParticle");
	HitAttackParticle->SetupAttachment(RootComponent);

	ElectricHitParticle = CreateDefaultSubobject<UParticleSystemComponent>("ElectricHitParticle");
	ElectricHitParticle->ObjectName = FName("ElectricHitParticle");
	ElectricHitParticle->SetupAttachment(RootComponent);
	ElectricHitParticle->bAutoActivate = false;

	BaseEffectParticle = CreateDefaultSubobject<UParticleSystemComponent>("BaseEffectParticle");
	BaseEffectParticle->ObjectName = FName("BaseEffectParticle");
	BaseEffectParticle->SetupAttachment(RootComponent);

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
		ElectricHitParticle = nullptr;
		BaseEffectParticle = nullptr;
		OwningCharacter = nullptr;
		AttackShape = nullptr;

		for (UActorComponent* Comp : GetOwnedComponents())
		{
			if (auto* Scene = Cast<USceneComponent>(Comp))
			{
				if (Scene == GetRootComponent())
				{
					SceneRoot = Scene;
				}
			}

			if (auto* Shape = Cast<UShapeComponent>(Comp))
			{
				FString Tag = Shape->GetTag();
				if (Shape->ObjectName == FName("AttackShape"))					
				{
					AttackShape = Shape;
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
				else if (ParticleName.find("ElectricHit") != std::string::npos)
				{
					ElectricHitParticle = Particle;
				}
				else if (ParticleName.find("HitAttack") != std::string::npos)
				{
					HitAttackParticle = Particle;
				}
				else if (ParticleName.find("BaseEffect") != std::string::npos)
				{
					BaseEffectParticle = Particle;
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
REGISTER_FUNCTION_NOTIFY(AAccessoryActor, PlayTryParticle)
void AAccessoryActor::PlayTryParticle()
{	
	if (TryAttackParticle)
	{
		TryAttackParticle->ActivateSystem();
		// UE_LOG("Play Try Particle");
	}
	else
	{
		UE_LOG("Error Load Try Particle");
	}
}

REGISTER_FUNCTION_NOTIFY(AAccessoryActor, StopTryParticle)
void AAccessoryActor::StopTryParticle()
{
	if (TryAttackParticle)
	{
		TryAttackParticle->StopSpawning();  // 새 파티클 생성 중지, 기존 파티클은 자연스럽게 소멸
		UE_LOG("Stop Try Particle");
	}
	else
	{
		UE_LOG("Error Stop Try Particle");
	}
}

void AAccessoryActor::PlayHitParticle()
{
	if (HitAttackParticle)
	{
		HitAttackParticle->ActivateSystem();
		UE_LOG("Play Hit Particle");
	}
	else
	{
		UE_LOG("Error Play Hit Particle");
	}
}

void AAccessoryActor::StopHitParticle()
{
	if (HitAttackParticle)
	{
		HitAttackParticle->StopSpawning();  // 새 파티클 생성 중지, 기존 파티클은 자연스럽게 소멸
		UE_LOG("Stop Hit Particle");
	}
	else
	{
		UE_LOG("Error Stop Hit Particle");
	}
}

void AAccessoryActor::SpawnHitParticleAtLocation(const FVector& Location)
{
	if (!HitAttackParticle)
	{
		UE_LOG("SpawnHitParticle: HitAttackParticle is null");
		return;
	}

	// 위치 이동
	HitAttackParticle->SetWorldLocation(Location);

	// 리셋 후 활성화 (한 번만 재생)
	HitAttackParticle->ResetAndActivate();

	UE_LOG("Spawned hit particle at (%.1f, %.1f, %.1f)",
	       Location.X, Location.Y, Location.Z);
}

void AAccessoryActor::SpawnElectricHitParticleAtLocation(const FVector& Location)
{
	if (!ElectricHitParticle)
	{
		UE_LOG("SpawnElectricHitParticle: ElectricHitParticle is null");
		return;
	}

	// 위치 이동
	ElectricHitParticle->SetWorldLocation(Location);

	// 리셋 후 활성화 (한 번만 재생)
	ElectricHitParticle->ResetAndActivate();

	UE_LOG("Spawned electric hit particle at (%.1f, %.1f, %.1f)",
	       Location.X, Location.Y, Location.Z);
}

void AAccessoryActor::SetAttackShapeNameAndAttach(const FName& Name)
{
	if (!RootComponent || !AttackShape)
	{
		return;
	}

	AttackShape->SetupAttachment(RootComponent);
	AttackShape->ObjectName = Name;
}

void AAccessoryActor::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();

	// 컴포넌트 포인터 초기화
	SceneRoot = nullptr;
	AccessoryMesh = nullptr;
	TryAttackParticle = nullptr;
	HitAttackParticle = nullptr;
	ElectricHitParticle = nullptr;
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
			// Particle은 여러 개가 있으므로 ObjectName으로 구분
			FString ParticleName = Particle->ObjectName.ToString();
			if (ParticleName.find("TryAttack") != std::string::npos)
			{
				TryAttackParticle = Particle;
			}
			else if (ParticleName.find("ElectricHit") != std::string::npos)
			{
				ElectricHitParticle = Particle;
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
	USkeletalMeshComponent* CharacterMesh = OwnerCharacter->GetMesh();
	if (CharacterMesh && AccessoryMesh && AttachSocketName.IsValid())
	{ 
		AccessoryMesh->SetupAttachment(CharacterMesh, AttachSocketName);
		AccessoryMesh->RegisterComponent(OwnerCharacter->GetWorld());

		// 월드 스케일 1이 되도록 상대 스케일 계산 (부모 스케일 상쇄)
		FTransform SocketWorld = CharacterMesh->GetSocketTransform(AttachSocketName);
		FVector SocketWorldScale = SocketWorld.Scale3D;
		FVector RelativeScaleForWorldOne = FVector(
			SocketWorldScale.X != 0.0f ? 1.0f / SocketWorldScale.X : 1.0f,
			SocketWorldScale.Y != 0.0f ? 1.0f / SocketWorldScale.Y : 1.0f,
			SocketWorldScale.Z != 0.0f ? 1.0f / SocketWorldScale.Z : 1.0f
		);
		AccessoryMesh->SetRelativeScale(RelativeScaleForWorldOne);
		 
	}

	// 2. 캐릭터의 스킬 컴포넌트 찾기 및 스킬 등록
	USkillComponent* SkillComp = OwnerCharacter->GetSkillComponent();
	if (SkillComp && !GrantedSkills.empty())
	{
		SkillComp->OverrideSkills(GrantedSkills, this);
	}

	// 3. Attack Shape을 캐릭터에 캐싱
	if (AttackShape)
	{
		OwnerCharacter->SetAttackShape(AttackShape);
		// 자신을 공격하는 걸 방지하기 위해서 owner 설정
		AttackShape->SetOwner(OwnerCharacter);
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
