#include "pch.h"
#include "AnimMontage.h"
#include "AnimSequence.h"
#include "JsonSerializer.h"
#include "Source/Runtime/AssetManagement/ResourceManager.h"

IMPLEMENT_CLASS(UAnimMontage)

float UAnimMontage::GetPlayLength() const
{
    // 첫 번째 섹션 길이 반환 (전체 길이는 섹션별로 다름)
    if (Sections.Num() > 0 && Sections[0].Sequence)
    {
        return Sections[0].Sequence->GetPlayLength();
    }
    return 0.0f;
}

// ============================================================
// Section API
// ============================================================

void UAnimMontage::AddSection(const FString& Name, UAnimSequence* Seq, float BlendIn)
{
    Sections.Add(FMontageSection(Name, Seq, BlendIn));
}

UAnimSequence* UAnimMontage::GetSectionSequence(int32 Index) const
{
    if (Index >= 0 && Index < Sections.Num())
    {
        return Sections[Index].Sequence;
    }
    return nullptr;
}

int32 UAnimMontage::FindSectionIndex(const FString& Name) const
{
    for (int32 i = 0; i < Sections.Num(); ++i)
    {
        if (Sections[i].Name == Name)
        {
            return i;
        }
    }
    return -1;
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

    // 블렌드 시간 저장
    Root["BlendInTime"] = BlendInTime;
    Root["BlendOutTime"] = BlendOutTime;
    Root["bLoop"] = bLoop;

    // 섹션 저장
    JSON SectionsArray = JSON::Make(JSON::Class::Array);
    for (const FMontageSection& Section : Sections)
    {
        JSON SectionObj = JSON::Make(JSON::Class::Object);
        SectionObj["Name"] = Section.Name.c_str();
        SectionObj["SequencePath"] = Section.Sequence ? Section.Sequence->GetFilePath().c_str() : "";
        SectionObj["BlendInTime"] = Section.BlendInTime;
        SectionsArray.append(SectionObj);
    }
    Root["Sections"] = SectionsArray;

    // 파일로 저장
    FWideString WPath = UTF8ToWide(FilePath);
    bool bSuccess = FJsonSerializer::SaveJsonToFile(Root, WPath);

    if (bSuccess)
    {
        UE_LOG("UAnimMontage::Save - Saved montage to %s (%d sections)", FilePath.c_str(), Sections.Num());

        // 노티파이도 저장 (.montage.notify.json)
        FString NotifyPath = FilePath;
        size_t pos = NotifyPath.rfind(".montage.json");
        if (pos != FString::npos)
        {
            NotifyPath = NotifyPath.substr(0, pos) + ".montage.notify.json";
            SaveMeta(NotifyPath);
        }
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

    // 블렌드 시간 로드
    FJsonSerializer::ReadFloat(Root, "BlendInTime", BlendInTime, 0.2f, false);
    FJsonSerializer::ReadFloat(Root, "BlendOutTime", BlendOutTime, 0.2f, false);

    if (Root.hasKey("bLoop"))
    {
        bLoop = Root.at("bLoop").ToBool();
    }

    // 섹션 로드
    Sections.Empty();
    if (Root.hasKey("Sections"))
    {
        const JSON& SectionsArray = Root.at("Sections");
        for (size_t i = 0; i < SectionsArray.size(); ++i)
        {
            const JSON& SectionObj = SectionsArray.at(i);

            FMontageSection Section;
            if (SectionObj.hasKey("Name"))
            {
                Section.Name = SectionObj.at("Name").ToString();
            }
            if (SectionObj.hasKey("SequencePath"))
            {
                FString SeqPath = SectionObj.at("SequencePath").ToString();
                if (!SeqPath.empty())
                {
                    Section.Sequence = UResourceManager::GetInstance().Get<UAnimSequence>(SeqPath);
                }
            }
            FJsonSerializer::ReadFloat(SectionObj, "BlendInTime", Section.BlendInTime, 0.1f, false);

            Sections.Add(Section);
        }
    }

    UE_LOG("UAnimMontage::Load - Loaded montage from %s (%d sections)", FilePath.c_str(), Sections.Num());

    // 노티파이도 로드 (.montage.notify.json)
    FString NotifyPath = FilePath;
    size_t pos = NotifyPath.rfind(".montage.json");
    if (pos != FString::npos)
    {
        NotifyPath = NotifyPath.substr(0, pos) + ".montage.notify.json";
        LoadMeta(NotifyPath);
    }

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
        // 단일 섹션으로 추가
        Montage->AddSection("Default", InSequence, InBlendInTime);
        Montage->BlendInTime = InBlendInTime;
        Montage->BlendOutTime = InBlendOutTime;
        Montage->bLoop = InbLoop;
    }
    return Montage;
}
