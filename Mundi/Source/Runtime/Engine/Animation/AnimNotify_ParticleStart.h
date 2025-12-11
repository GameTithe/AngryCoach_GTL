#pragma once
#include "AnimNotify.h"

class UAnimNotify_ParticleStart : public UAnimNotify
{
public:
	DECLARE_CLASS(UAnimNotify_ParticleStart, UAnimNotify)

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;

	// 파티클을 붙일 소켓 이름 (비어있으면 루트)
	FName SocketName;

	// 파티클 시스템 경로
	FString ParticleSystemPath;
};