#include "pch.h"
#include "AnimMontage.h"
#include "AnimSequence.h"
#include "JsonSerializer.h"
#include "Source/Runtime/AssetManagement/ResourceManager.h"

IMPLEMENT_CLASS(UAnimMontage)

float UAnimMontage::GetPlayLength() const
{
    if (Sequence)
    {
        return Sequence->GetPlayLength();
    }
    return 0.0f;
}

// ============================================================
// Serialization
// ============================================================

bool UAnimMontage::Save(const FString& FilePath) const
{
    if (FilePath.empty())
    {
        UE_LOG("UAnimMontage::Save - Empty file path");
        return false;
    }

    JSON Root = JSON::Make(JSON::Class::Object);

    // 시퀀스 경로 저장
    FString SequencePath = "";
    if (Sequence)
    {
        SequencePath = Sequence->GetFilePath();
    }
    Root["SequencePath"] = SequencePath.c_str();

    // 블렌드 시간 저장
    Root["BlendInTime"] = BlendInTime;
    Root["BlendOutTime"] = BlendOutTime;

    // 루프 여부
    Root["bLoop"] = bLoop;

    // 파일로 저장
    FWideString WPath = UTF8ToWide(FilePath);
    bool bSuccess = FJsonSerializer::SaveJsonToFile(Root, WPath);

    if (bSuccess)
    {
        UE_LOG("UAnimMontage::Save - Saved montage to %s", FilePath.c_str());
    }
    else
    {
        UE_LOG("UAnimMontage::Save - Failed to save montage to %s", FilePath.c_str());
    }

    return bSuccess;
}

bool UAnimMontage::Load(const FString& FilePath)
{
    if (FilePath.empty())
    {
        UE_LOG("UAnimMontage::Load - Empty file path");
        return false;
    }

    JSON Root;
    FWideString WPath = UTF8ToWide(FilePath);
    if (!FJsonSerializer::LoadJsonFromFile(Root, WPath))
    {
        UE_LOG("UAnimMontage::Load - Failed to load file: %s", FilePath.c_str());
        return false;
    }

    // 시퀀스 로드
    if (Root.hasKey("SequencePath"))
    {
        FString SeqPath = Root.at("SequencePath").ToString();
        if (!SeqPath.empty())
        {
            Sequence = UResourceManager::GetInstance().Load<UAnimSequence>(SeqPath);
            if (!Sequence)
            {
                UE_LOG("UAnimMontage::Load - Failed to load sequence: %s", SeqPath.c_str());
            }
        }
    }

    // 블렌드 시간 로드
    FJsonSerializer::ReadFloat(Root, "BlendInTime", BlendInTime, 0.2f, false);
    FJsonSerializer::ReadFloat(Root, "BlendOutTime", BlendOutTime, 0.2f, false);

    // 루프 여부 로드
    if (Root.hasKey("bLoop"))
    {
        bLoop = Root.at("bLoop").ToBool();
    }

    UE_LOG("UAnimMontage::Load - Loaded montage from %s (BlendIn: %.2f, BlendOut: %.2f, Loop: %d)",
        FilePath.c_str(), BlendInTime, BlendOutTime, bLoop ? 1 : 0);

    return true;
}

// ============================================================
// Factory
// ============================================================

UAnimMontage* UAnimMontage::Create(UAnimSequence* InSequence, float InBlendInTime, float InBlendOutTime, bool InbLoop)
{
    UAnimMontage* Montage = NewObject<UAnimMontage>();
    if (Montage)
    {
        Montage->Sequence = InSequence;
        Montage->BlendInTime = InBlendInTime;
        Montage->BlendOutTime = InBlendOutTime;
        Montage->bLoop = InbLoop;
    }
    return Montage;
}
