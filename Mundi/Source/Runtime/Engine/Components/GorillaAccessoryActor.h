#pragma once
#include "AccessoryActor.h"
#include "AGorillaAccessoryActor.generated.h"

class UAnimationGraph;
class UPhysicsAsset;
class UTexture;

UCLASS(DisplayName = "고릴라 악세서리", Description = "고릴라 공격을 수행하는 악세서리입니다")
class AGorillaAccessoryActor : public AAccessoryActor
{
public:
	GENERATED_REFLECTION_BODY();
	AGorillaAccessoryActor();

	void DuplicateSubObjects() override;
	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

	// 스킬에서 호출할 상태 전환 함수
	void ToggleGorillaForm();

	bool GetIsGorillaForm() {return bIsGorillaFormActive;}
private:
	// 고릴라 형태가 활성화되었는지 여부
	bool bIsGorillaFormActive = false;

	// 고릴라 애셋 (한 번 로드)
	FString GorillaSkeletalMeshPath;

	UAnimationGraph* GorillaAnimGraph = nullptr;

	UPhysicsAsset* GorillaPhysicsAsset = nullptr;
	
	UTexture* GorillaSkinTexture = nullptr;
	
	UTexture* GorillaSkinNormal = nullptr;

	// 캐릭터의 기본 애셋 (최초 활성화 시 저장)
	FString DefaultSkeletalMeshPath;

	UAnimationGraph* DefaultAnimGraph = nullptr;

	UPhysicsAsset* DefaultPhysicsAsset = nullptr;
	
	UTexture* DefaultSkinTexture = nullptr;
	
	UTexture* DefaultSkinNormal = nullptr;
};
