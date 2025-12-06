#include "pch.h"
#include "LuaManager.h"
#include "LuaComponentProxy.h"
#include "GameObject.h"
#include "ObjectIterator.h"
#include "LuaPhysicsTypes.h"
#include "CameraActor.h"
#include "CameraComponent.h"
#include "PlayerCameraManager.h"
#include "GameModeBase.h"
#include "GameStateBase.h"
#include <tuple>

#include "Source/Game/UI/GameUIManager.h"
#include "Source/Game/UI/Widgets/UICanvas.h"
#include "Source/Game/UI/Widgets/ButtonWidget.h"

sol::object MakeCompProxy(sol::state_view SolState, void* Instance, UClass* Class) {
    BuildBoundClass(Class);
    LuaComponentProxy Proxy;
    Proxy.Instance = Instance;
    Proxy.Class = Class;
    return sol::make_object(SolState, std::move(Proxy));
}

FLuaManager::FLuaManager()
{
    Lua = new sol::state();
    
    
    // Open essential standard libraries for gameplay scripts
    Lua->open_libraries(
        sol::lib::base,
        sol::lib::coroutine,
        sol::lib::math,
        sol::lib::table,
        sol::lib::string
    );

    SharedLib = Lua->create_table();

    // Lua에서 Actor와 FGameObject 가 1대1로 매칭되고
    // Component는 그대로 Component와 1대1로 매칭된다
    // NOTE: 그냥 FGameObject 개념을 없애고 그냥 Actor/Component 그대로 사용하는 게 좋을듯?
    Lua->new_usertype<FGameObject>("GameObject",
        "UUID", &FGameObject::UUID,
        "Tag", sol::property(&FGameObject::GetTag, &FGameObject::SetTag),
        "Location", sol::property(&FGameObject::GetLocation, &FGameObject::SetLocation),
        "Rotation", sol::property(&FGameObject::GetRotation, &FGameObject::SetRotation), 
        "Scale", sol::property(&FGameObject::GetScale, &FGameObject::SetScale),
        "bIsActive", sol::property(&FGameObject::GetIsActive, &FGameObject::SetIsActive),
        "Velocity", &FGameObject::Velocity,
        "PrintLocation", &FGameObject::PrintLocation,
        "GetForward", &FGameObject::GetForward
    );
    
    Lua->new_usertype<UCameraComponent>("CameraComponent",
        sol::no_constructor,
        "SetLocation", sol::overload(
            [](UCameraComponent* Camera, FVector Location)
            {
                if (!Camera)
                {
                    return;
                }
                Camera->SetWorldLocation(Location);
            },
            [](UCameraComponent* Camera, float X, float Y, float Z)
            {
                if (!Camera)
                {
                    return;
                }
                Camera->SetWorldLocation(FVector(X, Y, Z));
            }
        ),
        "SetCameraForward",
        [](UCameraComponent* Camera, FVector Direction)
        {
            if (!Camera)
            {
                return;
            }
            ACameraActor* CameraActor = Cast<ACameraActor>(Camera->GetOwner());
            CameraActor->SetForward(Direction);
        },
        "GetActorLocation", [](UCameraComponent* Camera) -> FVector
        {
            if (!Camera)
            {
                // 유효하지 않은 경우 0 벡터 반환
                return FVector(0.f, 0.f, 0.f);
            }
            return Camera->GetWorldLocation();
        },
        "GetActorRight", [](UCameraComponent* Camera) -> FVector
        {
            if (!Camera) return FVector(0.f, 0.f, 1.f); // 기본값 (World Forward)

            // C++ UCameraComponent 클래스의 실제 함수명으로 변경해야 합니다.
            // (예: GetActorForwardVector(), GetForward() 등)
            return Camera->GetForward();
        }
    );
    Lua->new_usertype<UInputManager>("InputManager",
        "IsKeyDown", sol::overload(
            &UInputManager::IsKeyDown,
            [](UInputManager* Self, const FString& Key) {
                if (Key.empty()) return false;
                return Self->IsKeyDown(Key[0]);
            }),
        "IsKeyPressed", sol::overload(
            &UInputManager::IsKeyPressed,
            [](UInputManager* Self, const FString& Key) {
                if (Key.empty()) return false;
                return Self->IsKeyPressed(Key[0]);
            }),
        "IsKeyReleased", sol::overload(
            &UInputManager::IsKeyReleased,
            [](UInputManager* Self, const FString& Key) {
                if (Key.empty()) return false;
                return Self->IsKeyReleased(Key[0]);
            }),
        "IsMouseButtonDown", &UInputManager::IsMouseButtonDown,
        "IsMouseButtonPressed", &UInputManager::IsMouseButtonPressed,
        "IsMouseButtonReleased", &UInputManager::IsMouseButtonReleased,
        "GetMouseDelta", [](UInputManager* Self) {
            const FVector2D Delta = Self->GetMouseDelta();
            return FVector(Delta.X, Delta.Y, 1.0);
        },
        "SetCursorVisible", [](UInputManager* Self, bool bVisible){
            if (bVisible)
            { 
                Self->SetCursorVisible(true);
                if (Self->IsCursorLocked())
                    Self->ReleaseCursor();
            } else
            { 
                Self->SetCursorVisible(false);
                Self->LockCursor();
            }
        }
    );                
    
    sol::table MouseButton = Lua->create_table("MouseButton");
    MouseButton["Left"] = EMouseButton::LeftButton;
    MouseButton["Right"] = EMouseButton::RightButton;
    MouseButton["Middle"] = EMouseButton::MiddleButton;
    MouseButton["XButton1"] = EMouseButton::XButton1;
    MouseButton["XButton2"] = EMouseButton::XButton2;
    
    Lua->set_function("print", sol::overload(                             
        [](const FString& msg) {                                          
            UE_LOG("[Lua-Str] %s\n", msg.c_str());                        
        },                                                                
                                                                          
        [](int num){                                                      
            UE_LOG("[Lua] %d\n", num);                                    
        },                                                                
                                                                          
        [](double num){                                                   
            UE_LOG("[Lua] %f\n", num);                                    
        },                                                                
                                                                          
        [](FVector Vector)                                                    
        {                                                                 
            UE_LOG("[Lua] (%f, %f, %f)\n", Vector.X, Vector.Y, Vector.Z); 
        }                                                                 
    ));

    // Physics types
    Lua->new_usertype<LuaContactInfo>("ContactInfo",
        sol::no_constructor,
        "OtherActor", sol::readonly(&LuaContactInfo::OtherActor),
        "Position", sol::readonly(&LuaContactInfo::Position),
        "Normal", sol::readonly(&LuaContactInfo::Normal),
        "Impulse", sol::readonly(&LuaContactInfo::Impulse)
    );

    Lua->new_usertype<LuaTriggerInfo>("TriggerInfo",
        sol::no_constructor,
        "OtherActor", sol::readonly(&LuaTriggerInfo::OtherActor),
        "IsEnter", sol::readonly(&LuaTriggerInfo::bIsEnter)
    );
    
    // GlobalConfig는 전역 table
    SharedLib["GlobalConfig"] = Lua->create_table(); 
    // SharedLib["GlobalConfig"]["Gravity"] = 9.8;

    SharedLib.set_function("SpawnPrefab", sol::overload(
        [](const FString& PrefabPath) -> FGameObject*
        {
            FGameObject* NewObject = nullptr;

            AActor* NewActor = GWorld->SpawnPrefabActor(UTF8ToWide(PrefabPath));

            if (NewActor)
            {
                NewObject = NewActor->GetGameObject();
            }

            return NewObject;
        }
    ));
    SharedLib.set_function("DeleteObject", sol::overload(
        [](const FGameObject& GameObject)
        {
            for (TObjectIterator<AActor> It; It; ++It)
            {
                AActor* Actor = *It;

                if (Actor->UUID == GameObject.UUID)
                {
                    Actor->Destroy();   // 지연 삭제 요청 (즉시 삭제하면 터짐)
                    break;
                }
            }
        }
    ));
    SharedLib.set_function("FindObjectByName",
        [](const FString& ActorName) -> FGameObject*
        {
            if (!GWorld)
            {
                return nullptr;
            }

            // Lua의 FString을 FName으로 변환
            FName NameToFind(ActorName);

            AActor* FoundActor = GWorld->FindActorByName(NameToFind);

            // Lua 스크립트가 사용할 수 있도록 AActor*를 FGameObject*로 변환
            if (FoundActor && !FoundActor->IsPendingDestroy())
            {
                return FoundActor->GetGameObject();
            }

            return nullptr; // 찾지 못함
        }
    );
    SharedLib.set_function("FindComponentByName",
        [this](const FString& ComponentName) -> UActorComponent*
        {
            if (!GWorld)
            {
                return nullptr;
            }

            // Lua의 FString을 FName으로 변환
            FName NameToFind(ComponentName);

            UActorComponent* FoundComponent = GWorld->FindComponentByName(NameToFind);

            // Lua 스크립트가 사용할 수 있도록 UActorComponent*를 LuaComponentProxy로 변환
            if (FoundComponent && !FoundComponent->IsPendingDestroy())
            {
                return FoundComponent;
            }

            return nullptr; // 찾지 못함
        }
    );
    SharedLib.set_function("GetCamera",
        []() -> UCameraComponent*
        {
            if (!GWorld)
            {
                return nullptr;
            }
            return GWorld->GetPlayerCameraManager()->GetViewCamera();
        }
    );
    SharedLib.set_function("GetCameraManager",
        []() -> APlayerCameraManager*
        {
            if (!GWorld)
            {
                return nullptr;
            }
            return GWorld->GetPlayerCameraManager();
        }
    );
    SharedLib.set_function("SetPlayerForward",
        [](FGameObject& GameObject, FVector Direction)
        {
            AActor* Player = GameObject.GetOwner();
            if (!Player)
            {
                return;
            }

            USceneComponent* SceneComponent = Player->GetRootComponent();

            if (!SceneComponent)
            {
                return;
            }

            SceneComponent->SetForward(Direction);
        }
   );
    SharedLib.set_function("Vector", sol::overload(
       []() { return FVector(0.0f, 0.0f, 0.0f); },
       [](float x, float y, float z) { return FVector(x, y, z); }
   ));

    //@TODO(Timing)
    SharedLib.set_function("SetSlomo", [](float Duration , float Dilation) { GWorld->RequestSlomo(Duration, Dilation); });

    SharedLib.set_function("HitStop", [](float Duration, sol::optional<float> Scale) { GWorld->RequestHitStop(Duration, Scale.value_or(0.0f)); });
    
    SharedLib.set_function("TargetHitStop", [](FGameObject& Obj, float Duration, sol::optional<float> Scale) 
        {
            if (AActor* Owner = Obj.GetOwner())
            {
                Owner->SetCustomTimeDillation(Duration, Scale.value_or(0.0f));
            }
        });
    
    // FVector usertype 등록 (메서드와 프로퍼티)
    SharedLib.new_usertype<FVector>("FVector",
        sol::no_constructor,  // 생성자는 위에서 Vector 함수로 등록했음
        // Properties
        "X", &FVector::X,
        "Y", &FVector::Y,
        "Z", &FVector::Z,
        // Operators
        sol::meta_function::addition, [](const FVector& a, const FVector& b) -> FVector {
            return FVector(a.X + b.X, a.Y + b.Y, a.Z + b.Z);
        },
        sol::meta_function::subtraction, [](const FVector& a, const FVector& b) -> FVector {
            return FVector(a.X - b.X, a.Y - b.Y, a.Z - b.Z);
        },
        sol::meta_function::multiplication, sol::overload(
            [](const FVector& v, float f) -> FVector { return v * f; },
            [](float f, const FVector& v) -> FVector { return v * f; }
        ),
        // Methods
        "Length", &FVector::Distance,
        "Normalize", &FVector::Normalize,
        "Dot", [](const FVector& a, const FVector& b) { return FVector::Dot(a, b); },
        "Cross", [](const FVector& a, const FVector& b) { return FVector::Cross(a, b); }
    );

    Lua->set_function("Color", sol::overload(
        []() { return FLinearColor(0.0f, 0.0f, 0.0f, 1.0f); },
        [](float R, float G, float B) { return FLinearColor(R, G, B, 1.0f); },
        [](float R, float G, float B, float A) { return FLinearColor(R, G, B, A); }
    ));


    SharedLib.new_usertype<FLinearColor>("FLinearColor",
        sol::no_constructor,
        "R", &FLinearColor::R,
        "G", &FLinearColor::G,
        "B", &FLinearColor::B,
        "A", &FLinearColor::A
    );

    // ============================================
    // Game Flow Enums
    // ============================================
    SharedLib.new_enum("EGameState",
        "None", EGameState::None,
        "WaitingToStart", EGameState::WaitingToStart,
        "InProgress", EGameState::InProgress,
        "Paused", EGameState::Paused,
        "GameOver", EGameState::GameOver
    );

    SharedLib.new_enum("ERoundState",
        "None", ERoundState::None,
        "Intro", ERoundState::Intro,
        "StartPage", ERoundState::StartPage,
        "CharacterSelect", ERoundState::CharacterSelect,
        "CountDown", ERoundState::CountDown,
        "InProgress", ERoundState::InProgress,
        "RoundEnd", ERoundState::RoundEnd
    );

    SharedLib.new_enum("EGameResult",
        "None", EGameResult::None,
        "Win", EGameResult::Win,
        "Lose", EGameResult::Lose,
        "Draw", EGameResult::Draw
    );

    // ============================================
    // GameMode/GameState Global Functions
    // ============================================
    SharedLib.set_function("GetGameMode",
        []() -> AGameModeBase*
        {
            if (!GWorld) return nullptr;
            return GWorld->GetGameMode();
        }
    );

    SharedLib.set_function("GetGameState",
        []() -> AGameStateBase*
        {
            if (!GWorld) return nullptr;
            if (auto* GameMode = GWorld->GetGameMode())
            {
                return GameMode->GetGameState();
            }
            return nullptr;
        }
    );

    SharedLib.set_function("GetCurrentGameState",
        []() -> EGameState
        {
            if (!GWorld) return EGameState::None;
            if (auto* GameMode = GWorld->GetGameMode())
            {
                return GameMode->GetCurrentGameState();
            }
            return EGameState::None;
        }
    );

    SharedLib.set_function("GetCurrentRoundState",
        []() -> ERoundState
        {
            if (!GWorld) return ERoundState::None;
            if (auto* GameMode = GWorld->GetGameMode())
            {
                return GameMode->GetCurrentRoundState();
            }
            return ERoundState::None;
        }
    );

    SharedLib.set_function("GetCurrentRound",
        []() -> int32
        {
            if (!GWorld) return 0;
            if (auto* GameMode = GWorld->GetGameMode())
            {
                if (auto* GameState = GameMode->GetGameState())
                {
                    return GameState->GetCurrentRound();
                }
            }
            return 0;
        }
    );

    SharedLib.set_function("GetRoundTimeRemaining",
        []() -> float
        {
            if (!GWorld) return 0.0f;
            if (auto* GameMode = GWorld->GetGameMode())
            {
                if (auto* GameState = GameMode->GetGameState())
                {
                    return GameState->GetRoundTimeRemaining();
                }
            }
            return 0.0f;
        }
    );

    // 라운드 타이머 리셋 (ReadyGo 시퀀스 후 호출)
    SharedLib.set_function("ResetRoundTimer",
        []()
        {
            if (!GWorld) return;
            if (auto* GameMode = GWorld->GetGameMode())
            {
                if (auto* GameState = GameMode->GetGameState())
                {
                    GameState->SetRoundTimeRemaining(GameState->GetRoundDuration());
                }
            }
        }
    );

    SharedLib.set_function("GetRoundWins",
        [](int32 PlayerIndex) -> int32
        {
            if (!GWorld) return 0;
            if (auto* GameMode = GWorld->GetGameMode())
            {
                if (auto* GameState = GameMode->GetGameState())
                {
                    return GameState->GetRoundWins(PlayerIndex);
                }
            }
            return 0;
        }
    );

    SharedLib.set_function("StartMatch",
        []()
        {
            if (!GWorld) return;
            if (auto* GameMode = GWorld->GetGameMode())
            {
                GameMode->StartMatch();
            }
        }
    );

    SharedLib.set_function("EndMatch",
        []()
        {
            if (!GWorld) return;
            if (auto* GameMode = GWorld->GetGameMode())
            {
                GameMode->EndMatch();
            }
        }
    );

    SharedLib.set_function("EndIntro",
        []()
        {
            if (!GWorld) return;
            if (auto* GameMode = GWorld->GetGameMode())
            {
                GameMode->EndIntro();
            }
        }
    );

    SharedLib.set_function("EndStartPage",
        []()
        {
            if (!GWorld) return;
            if (auto* GameMode = GWorld->GetGameMode())
            {
                GameMode->EndStartPage();
            }
        }
    );

    SharedLib.set_function("StartCharacterSelect",
        []()
        {
            if (!GWorld) return;
            if (auto* GameMode = GWorld->GetGameMode())
            {
                GameMode->StartCharacterSelect();
            }
        }
    );

    SharedLib.set_function("EndCharacterSelect",
        []()
        {
            if (!GWorld) return;
            if (auto* GameMode = GWorld->GetGameMode())
            {
                GameMode->EndCharacterSelect();
            }
        }
    );

    SharedLib.set_function("StartRound",
        []()
        {
            if (!GWorld) return;
            if (auto* GameMode = GWorld->GetGameMode())
            {
                GameMode->StartRound();
            }
        }
    );

    SharedLib.set_function("BeginBattle",
        []()
        {
            if (!GWorld) return;
            if (auto* GameMode = GWorld->GetGameMode())
            {
                GameMode->BeginBattle();
            }
        }
    );

    SharedLib.set_function("EndRound",
        [](int32 WinnerIndex)
        {
            if (!GWorld) return;
            if (auto* GameMode = GWorld->GetGameMode())
            {
                GameMode->EndRound(WinnerIndex);
            }
        }
    );

    SharedLib.set_function("StartCountDown",
        [](sol::optional<float> CountDownTime)
        {
            if (!GWorld) return;
            if (auto* GameMode = GWorld->GetGameMode())
            {
                GameMode->StartCountDown(CountDownTime.value_or(3.0f));
            }
        }
    );

    // R키 카운트 관련 함수 (테스트용)
    SharedLib.set_function("GetRKeyCount",
        []() -> int32
        {
            if (!GWorld) return 0;
            if (auto* GameMode = GWorld->GetGameMode())
            {
                if (auto* GameState = GameMode->GetGameState())
                {
                    return GameState->GetRKeyCount();
                }
            }
            return 0;
        }
    );

    SharedLib.set_function("IncrementRKeyCount",
        []()
        {
            if (!GWorld) return;
            if (auto* GameMode = GWorld->GetGameMode())
            {
                if (auto* GameState = GameMode->GetGameState())
                {
                    GameState->IncrementRKeyCount();
                }
            }
        }
    );

    SharedLib.set_function("ResetRKeyCount",
        []()
        {
            if (!GWorld) return;
            if (auto* GameMode = GWorld->GetGameMode())
            {
                if (auto* GameState = GameMode->GetGameState())
                {
                    GameState->ResetRKeyCount();
                }
            }
        }
    );

    RegisterComponentProxy(*Lua);
    ExposeGlobalFunctions();
    ExposeAllComponentsToLua();

    // UI 위젯 시스템 바인딩
    ExposeUIFunctions();

    // 위 등록 마친 뒤 fall back 설정 : Shared lib의 fall back은 G
    sol::table MetaTableShared = Lua->create_table();
    MetaTableShared[sol::meta_function::index] = Lua->globals();
    SharedLib[sol::metatable_key]  = MetaTableShared;
}

FLuaManager::~FLuaManager()
{
    ShutdownBeforeLuaClose();

    // Lua 상태 삭제 전에 UI 캔버스들의 Lua 콜백 정리
    // (버튼 콜백들이 sol::protected_function을 shared_ptr로 가지고 있음)
    UGameUIManager::Get().RemoveAllCanvases();

    if (Lua)
    {
        delete Lua;
        Lua = nullptr;
    }
}

sol::environment FLuaManager::CreateEnvironment()
{
    sol::environment Env(*Lua, sol::create);

    // Environment의 Fall back은 SharedLib
    sol::table MetaTable = Lua->create_table();
    MetaTable[sol::meta_function::index] = SharedLib;
    Env[sol::metatable_key] = MetaTable;
    
    return Env;
}

void FLuaManager::RegisterComponentProxy(sol::state& Lua) {
    Lua.new_usertype<LuaComponentProxy>("Component",
        sol::meta_function::index,     &LuaComponentProxy::Index,
        sol::meta_function::new_index, &LuaComponentProxy::NewIndex
    );
}

void FLuaManager::ExposeAllComponentsToLua()
{
    SharedLib["Components"] = Lua->create_table();

    SharedLib.set_function("AddComponent",
        [this](sol::object Obj, const FString& ClassName)
        {
           if (!Obj.is<FGameObject&>()) {
                UE_LOG("[Lua][error] Error: Expected GameObject\n");
                return sol::make_object(*Lua, sol::nil);
            }
        
            FGameObject& GameObject = Obj.as<FGameObject&>();
            AActor* Actor = GameObject.GetOwner();

            UClass* Class = UClass::FindClass(ClassName);
            if (!Class) return sol::make_object(*Lua, sol::nil);

            UActorComponent* Comp = Actor->AddNewComponent(Class);
            return MakeCompProxy(*Lua, Comp, Class);
        });

    SharedLib.set_function("GetComponent",
        [this](sol::object Obj, const FString& ClassName)
        {
            if (!Obj.is<FGameObject&>()) {
                UE_LOG("[Lua][error] Error: Expected GameObject\n");
                return sol::make_object(*Lua, sol::nil);
            }
            
            FGameObject& GameObject = Obj.as<FGameObject&>();
            AActor* Actor = GameObject.GetOwner();

            UClass* Class = UClass::FindClass(ClassName);
            if (!Class) return sol::make_object(*Lua, sol::nil);
            
            UActorComponent* Comp = Actor->GetComponent(Class);
            if (!Comp) return sol::make_object(*Lua, sol::nil); 
            
            return MakeCompProxy(*Lua, Comp, Class);
        }
    );
}

void FLuaManager::ExposeGlobalFunctions()
{
    // APlayerCameraManager 클래스의 멤버 함수들 바인딩
    Lua->new_usertype<APlayerCameraManager>("PlayerCameraManager",
        sol::no_constructor,

        // --- StartCameraShake ---
        "StartCameraShake", sol::overload(
            // (Full) 5개 인수
            [](APlayerCameraManager* Self, float InDuration, float AmpLoc, float AmpRotDeg, float Frequency, int32 InPriority)
            {
                if (Self) Self->StartCameraShake(InDuration, AmpLoc, AmpRotDeg, Frequency, InPriority);
            },
            // (Priority 기본값 사용) 4개 인수
            [](APlayerCameraManager* Self, float InDuration, float AmpLoc, float AmpRotDeg, float Frequency)
            {
                if (Self) Self->StartCameraShake(InDuration, AmpLoc, AmpRotDeg, Frequency);
            }
        ),

        // --- StartFade ---
        "StartFade", sol::overload(
            // (Full) 5개 인수
            [](APlayerCameraManager* Self, float InDuration, float FromAlpha, float ToAlpha, const FLinearColor& InColor, int32 InPriority)
            {
                if (Self) Self->StartFade(InDuration, FromAlpha, ToAlpha, InColor, InPriority);
            },
            // (Priority 기본값 사용) 4개 인수
            [](APlayerCameraManager* Self, float InDuration, float FromAlpha, float ToAlpha, const FLinearColor& InColor)
            {
                if (Self) Self->StartFade(InDuration, FromAlpha, ToAlpha, InColor);
            },
            // (Color, Priority 기본값 사용) 3개 인수
            [](APlayerCameraManager* Self, float InDuration, float FromAlpha, float ToAlpha)
            {
                if (Self) Self->StartFade(InDuration, FromAlpha, ToAlpha);
            }
        ),

        // --- StartLetterBox ---
        "StartLetterBox", sol::overload(
            // (Full) 5개 인수
            [](APlayerCameraManager* Self, float InDuration, float Aspect, float BarHeight, const FLinearColor& InColor, int32 InPriority)
            {
                if (Self) Self->StartLetterBox(InDuration, Aspect, BarHeight, InColor, InPriority);
            },
            // (Priority 기본값 사용) 4개 인수
            [](APlayerCameraManager* Self, float InDuration, float Aspect, float BarHeight, const FLinearColor& InColor)
            {
                if (Self) Self->StartLetterBox(InDuration, Aspect, BarHeight, InColor);
            },
            // (Color, Priority 기본값 사용) 3개 인수
            [](APlayerCameraManager* Self, float InDuration, float Aspect, float BarHeight)
            {
                if (Self) Self->StartLetterBox(InDuration, Aspect, BarHeight);
            }
        ),

        // --- StartVignette (int 반환) ---
        "StartVignette", sol::overload(
            // (Full) 7개 인수
            [](APlayerCameraManager* Self, float InDuration, float Radius, float Softness, float Intensity, float Roundness, const FLinearColor& InColor, int32 InPriority) -> int
            {
                return Self ? Self->StartVignette(InDuration, Radius, Softness, Intensity, Roundness, InColor, InPriority) : -1;
            },
            // (Priority 기본값 사용) 6개 인수
            [](APlayerCameraManager* Self, float InDuration, float Radius, float Softness, float Intensity, float Roundness, const FLinearColor& InColor) -> int
            {
                return Self ? Self->StartVignette(InDuration, Radius, Softness, Intensity, Roundness, InColor) : -1;
            },
            // (Color, Priority 기본값 사용) 5개 인수
            [](APlayerCameraManager* Self, float InDuration, float Radius, float Softness, float Intensity, float Roundness) -> int
            {
                return Self ? Self->StartVignette(InDuration, Radius, Softness, Intensity, Roundness) : -1;
            }
        ),

        // --- UpdateVignette (int 반환) ---
        "UpdateVignette", sol::overload(
            // (Full) 8개 인수
            [](APlayerCameraManager* Self, int Idx, float InDuration, float Radius, float Softness, float Intensity, float Roundness, const FLinearColor& InColor, int32 InPriority) -> int
            {
                return Self ? Self->UpdateVignette(Idx, InDuration, Radius, Softness, Intensity, Roundness, InColor, InPriority) : -1;
            },
            // (Priority 기본값 사용) 7개 인수
            [](APlayerCameraManager* Self, int Idx, float InDuration, float Radius, float Softness, float Intensity, float Roundness, const FLinearColor& InColor) -> int
            {
                return Self ? Self->UpdateVignette(Idx, InDuration, Radius, Softness, Intensity, Roundness, InColor) : -1;
            },
            // (Color, Priority 기본값 사용) 6개 인수
            [](APlayerCameraManager* Self, int Idx, float InDuration, float Radius, float Softness, float Intensity, float Roundness) -> int
            {
                return Self ? Self->UpdateVignette(Idx, InDuration, Radius, Softness, Intensity, Roundness) : -1;
            }
        ),

        // --- AdjustVignette ---
        "AdjustVignette", sol::overload(
            // (Full) 7개 인수
            [](APlayerCameraManager* Self, float InDuration, float Radius, float Softness, float Intensity, float Roundness, const FLinearColor& InColor, int32 InPriority)
            {
                if (Self) Self->AdjustVignette(InDuration, Radius, Softness, Intensity, Roundness, InColor, InPriority);
            },
            // (Priority 기본값 사용) 6개 인수
            [](APlayerCameraManager* Self, float InDuration, float Radius, float Softness, float Intensity, float Roundness, const FLinearColor& InColor)
            {
                if (Self) Self->AdjustVignette(InDuration, Radius, Softness, Intensity, Roundness, InColor);
            },
            // (Color, Priority 기본값 사용) 5개 인수
            [](APlayerCameraManager* Self, float InDuration, float Radius, float Softness, float Intensity, float Roundness)
            {
                if (Self) Self->AdjustVignette(InDuration, Radius, Softness, Intensity, Roundness);
            }
        ),

        // --- DeleteVignette ---
        "DeleteVignette", [](APlayerCameraManager* Self)
        {
            if (Self) Self->DeleteVignette();
        },
            
        "SetViewTarget", [](APlayerCameraManager* self, LuaComponentProxy& Proxy)
        {
            // 타입 안정성 확인
            if (self && Proxy.Instance && Proxy.Class == UCameraComponent::StaticClass())
            {
                // 프록시에서 실제 컴포넌트 포인터 추출
                auto* CameraComp = static_cast<UCameraComponent*>(Proxy.Instance);
                self->SetViewCamera(CameraComp);
            }
        },

        "SetViewTargetWithBlend", [](APlayerCameraManager* self, LuaComponentProxy& Proxy, float InBlendTime)
        {
            // 타입 안정성 확인
            if (self && Proxy.Instance && Proxy.Class == UCameraComponent::StaticClass())
            {
                // 프록시에서 실제 컴포넌트 포인터 추출
                auto* CameraComp = static_cast<UCameraComponent*>(Proxy.Instance);
                self->SetViewCameraWithBlend(CameraComp, InBlendTime);
            }
        },

        // --- Gamma Correction ---
         // (Gamma Correction 기본값 사용) 1개 인수
        "StartGamma", [](APlayerCameraManager* Self, float Gamma)
        {
            if (Self)
            {
                Self->StartGamma(Gamma);
            }
        },

        // --- Depth of Field ---
        // (Full) 7개 인수
        "StartDOF", [](APlayerCameraManager* Self, float FocalDistance, float FocalRegion, float NearTransitionRegion, float FarTransitionRegion, float MaxNearBlurSize, float MaxFarBlurSize, int32 InPriority)
        {
            if (Self)
            {
                Self->StartDOF(FocalDistance, FocalRegion, NearTransitionRegion, FarTransitionRegion, MaxNearBlurSize, MaxFarBlurSize, InPriority);
            }
        }
    );
}

bool FLuaManager::LoadScriptInto(sol::environment& Env, const FString& Path) {
    auto Chunk = Lua->load_file(Path);
    if (!Chunk.valid()) { sol::error Err = Chunk; UE_LOG("[Lua][error] %s", Err.what()); return false; }
    
    sol::protected_function ProtectedFunc = Chunk;
    sol::set_environment(Env, ProtectedFunc);         
    auto Result = ProtectedFunc();
    if (!Result.valid()) { sol::error Err = Result; UE_LOG("[Lua][error] %s", Err.what()); return false; }
    return true;
}

void FLuaManager::Tick(double DeltaSeconds)
{
    CoroutineSchedular.Tick(DeltaSeconds);
}

void FLuaManager::ShutdownBeforeLuaClose()
{
    CoroutineSchedular.ShutdownBeforeLuaClose();
    
    FLuaBindRegistry::Get().Reset();
    
    SharedLib = sol::nil;
}

// Lua 함수 캐시 함수
sol::protected_function FLuaManager::GetFunc(sol::environment& Env, const char* Name)
{
    // (*Lua)[BeginPlay]()를 VM이 아닌, env에서 생성 및 캐시한다.
    // TODO : 함수 이름이 중복된다면?
    if (!Env.valid())
        return {};

    sol::object Object = Env[Name];

    if (Object == sol::nil || Object.get_type() != sol::type::function)
        return {};

    sol::protected_function Func = Object.as<sol::protected_function>();

    return Func;
}

void FLuaManager::ExposeUIFunctions()
{
    // UI 테이블 생성 (pch.h의 UI 매크로와 충돌 방지를 위해 GameUI로 명명)
    sol::table GameUI = Lua->create_named_table("UI");

    // ========================================
    // Canvas usertype 등록
    // ========================================
    Lua->new_usertype<UUICanvas>("UICanvas",
        sol::no_constructor,

        // 위젯 생성
        "CreateProgressBar", [](UUICanvas* Self, const std::string& Name,
                                float X, float Y, float W, float H) -> bool
        {
            return Self ? Self->CreateProgressBar(Name, X, Y, W, H) : false;
        },
        "CreateRect", [](UUICanvas* Self, const std::string& Name,
                         float X, float Y, float W, float H) -> bool
        {
            return Self ? Self->CreateRect(Name, X, Y, W, H) : false;
        },
        "CreateTexture", [](UUICanvas* Self, const std::string& Name, const std::string& Path,
                            float X, float Y, float W, float H) -> bool
        {
            if (!Self) return false;
            return Self->CreateTextureWidget(Name, Path, X, Y, W, H,
                                             UGameUIManager::Get().GetD2DContext());
        },

        // 위젯 속성 설정
        "SetProgress", [](UUICanvas* Self, const std::string& Name, float Value)
        {
            if (Self) Self->SetWidgetProgress(Name, Value);
        },
        "SetWidgetPosition", [](UUICanvas* Self, const std::string& Name, float X, float Y)
        {
            if (Self) Self->SetWidgetPosition(Name, X, Y);
        },
        "SetWidgetSize", [](UUICanvas* Self, const std::string& Name, float W, float H)
        {
            if (Self) Self->SetWidgetSize(Name, W, H);
        },
        "SetWidgetVisible", [](UUICanvas* Self, const std::string& Name, bool bVisible)
        {
            if (Self) Self->SetWidgetVisible(Name, bVisible);
        },
        "SetWidgetZOrder", [](UUICanvas* Self, const std::string& Name, int32_t Z)
        {
            if (Self) Self->SetWidgetZOrder(Name, Z);
        },
        "SetWidgetOpacity", [](UUICanvas* Self, const std::string& Name, float Opacity)
        {
            if (Self) Self->SetWidgetOpacity(Name, Opacity);
        },
        "SetForegroundColor", [](UUICanvas* Self, const std::string& Name,
                                 float R, float G, float B, float A)
        {
            if (Self) Self->SetWidgetForegroundColor(Name, R, G, B, A);
        },
        "SetBackgroundColor", [](UUICanvas* Self, const std::string& Name,
                                 float R, float G, float B, float A)
        {
            if (Self) Self->SetWidgetBackgroundColor(Name, R, G, B, A);
        },
        "SetColor", [](UUICanvas* Self, const std::string& Name,
                       float R, float G, float B, float A)
        {
            if (Self) Self->SetWidgetForegroundColor(Name, R, G, B, A);
        },
        "SetRightToLeft", [](UUICanvas* Self, const std::string& Name, bool bRTL)
        {
            if (Self) Self->SetWidgetRightToLeft(Name, bRTL);
        },

        // ProgressBar 텍스처 설정
        "SetProgressBarForegroundTexture", [](UUICanvas* Self, const std::string& Name, const std::string& Path) -> bool
        {
            if (!Self) return false;
            return Self->SetProgressBarForegroundTexture(Name, Path, UGameUIManager::Get().GetD2DContext());
        },
        "SetProgressBarBackgroundTexture", [](UUICanvas* Self, const std::string& Name, const std::string& Path) -> bool
        {
            if (!Self) return false;
            return Self->SetProgressBarBackgroundTexture(Name, Path, UGameUIManager::Get().GetD2DContext());
        },
        "SetProgressBarLowTexture", [](UUICanvas* Self, const std::string& Name, const std::string& Path) -> bool
        {
            if (!Self) return false;
            return Self->SetProgressBarLowTexture(Name, Path, UGameUIManager::Get().GetD2DContext());
        },
        "SetProgressBarTextureOpacity", [](UUICanvas* Self, const std::string& Name, float Opacity)
        {
            if (Self) Self->SetProgressBarTextureOpacity(Name, Opacity);
        },
        "ClearProgressBarTextures", [](UUICanvas* Self, const std::string& Name)
        {
            if (Self) Self->ClearProgressBarTextures(Name);
        },

        // SubUV 설정
        "SetTextureSubUVGrid", [](UUICanvas* Self, const std::string& Name, int NX, int NY)
        {
            if (Self) Self->SetTextureSubUVGrid(Name, NX, NY);
        },
        "SetTextureSubUVFrame", [](UUICanvas* Self, const std::string& Name, int FrameIndex)
        {
            if (Self) Self->SetTextureSubUVFrame(Name, FrameIndex);
        },
        "SetTextureSubUV", [](UUICanvas* Self, const std::string& Name, int FrameIndex, int NX, int NY)
        {
            if (Self) Self->SetTextureSubUV(Name, FrameIndex, NX, NY);
        },
        "SetProgressBarForegroundSubUV", [](UUICanvas* Self, const std::string& Name, int FrameIndex, int NX, int NY)
        {
            if (Self) Self->SetProgressBarForegroundSubUV(Name, FrameIndex, NX, NY);
        },
        "SetProgressBarBackgroundSubUV", [](UUICanvas* Self, const std::string& Name, int FrameIndex, int NX, int NY)
        {
            if (Self) Self->SetProgressBarBackgroundSubUV(Name, FrameIndex, NX, NY);
        },

        // 블렌드 모드
        "SetTextureAdditive", [](UUICanvas* Self, const std::string& Name, bool bAdditive)
        {
            if (Self) Self->SetTextureAdditive(Name, bAdditive);
        },

        // 위젯 삭제
        "RemoveWidget", [](UUICanvas* Self, const std::string& Name)
        {
            if (Self) Self->RemoveWidget(Name);
        },
        "RemoveAllWidgets", [](UUICanvas* Self)
        {
            if (Self) Self->RemoveAllWidgets();
        },

        // 캔버스 속성
        "SetPosition", [](UUICanvas* Self, float X, float Y)
        {
            if (Self) Self->SetPosition(X, Y);
        },
        "SetSize", [](UUICanvas* Self, float W, float H)
        {
            if (Self) Self->SetSize(W, H);
        },
        "SetVisible", [](UUICanvas* Self, bool bVisible)
        {
            if (Self) Self->SetVisible(bVisible);
        },
        "SetZOrder", [](UUICanvas* Self, int32_t Z)
        {
            if (Self) Self->SetZOrder(Z);
        },

        // 캔버스 정보
        "GetWidgetCount", [](UUICanvas* Self) -> size_t
        {
            return Self ? Self->GetWidgetCount() : 0;
        },

        // ======== 위젯 애니메이션 ========

        // 이동 애니메이션
        "MoveWidget", sol::overload(
            [](UUICanvas* Self, const std::string& Name, float X, float Y, float Duration)
            {
                if (Self) Self->MoveWidget(Name, X, Y, Duration, EEasingType::Linear);
            },
            [](UUICanvas* Self, const std::string& Name, float X, float Y, float Duration,
               const std::string& Easing)
            {
                if (!Self) return;
                EEasingType Type = EEasingType::Linear;
                if (Easing == "EaseIn") Type = EEasingType::EaseIn;
                else if (Easing == "EaseOut") Type = EEasingType::EaseOut;
                else if (Easing == "EaseInOut") Type = EEasingType::EaseInOut;
                Self->MoveWidget(Name, X, Y, Duration, Type);
            }
        ),

        // 크기 애니메이션
        "ResizeWidget", sol::overload(
            [](UUICanvas* Self, const std::string& Name, float W, float H, float Duration)
            {
                if (Self) Self->ResizeWidget(Name, W, H, Duration, EEasingType::Linear);
            },
            [](UUICanvas* Self, const std::string& Name, float W, float H, float Duration,
               const std::string& Easing)
            {
                if (!Self) return;
                EEasingType Type = EEasingType::Linear;
                if (Easing == "EaseIn") Type = EEasingType::EaseIn;
                else if (Easing == "EaseOut") Type = EEasingType::EaseOut;
                else if (Easing == "EaseInOut") Type = EEasingType::EaseInOut;
                Self->ResizeWidget(Name, W, H, Duration, Type);
            }
        ),

        // 회전 애니메이션
        "RotateWidget", sol::overload(
            [](UUICanvas* Self, const std::string& Name, float Angle, float Duration)
            {
                if (Self) Self->RotateWidget(Name, Angle, Duration, EEasingType::Linear);
            },
            [](UUICanvas* Self, const std::string& Name, float Angle, float Duration,
               const std::string& Easing)
            {
                if (!Self) return;
                EEasingType Type = EEasingType::Linear;
                if (Easing == "EaseIn") Type = EEasingType::EaseIn;
                else if (Easing == "EaseOut") Type = EEasingType::EaseOut;
                else if (Easing == "EaseInOut") Type = EEasingType::EaseInOut;
                Self->RotateWidget(Name, Angle, Duration, Type);
            }
        ),

        // 페이드 애니메이션
        "FadeWidget", sol::overload(
            [](UUICanvas* Self, const std::string& Name, float Opacity, float Duration)
            {
                if (Self) Self->FadeWidget(Name, Opacity, Duration, EEasingType::Linear);
            },
            [](UUICanvas* Self, const std::string& Name, float Opacity, float Duration,
               const std::string& Easing)
            {
                if (!Self) return;
                EEasingType Type = EEasingType::Linear;
                if (Easing == "EaseIn") Type = EEasingType::EaseIn;
                else if (Easing == "EaseOut") Type = EEasingType::EaseOut;
                else if (Easing == "EaseInOut") Type = EEasingType::EaseInOut;
                Self->FadeWidget(Name, Opacity, Duration, Type);
            }
        ),

        // 애니메이션 중지
        "StopAnimation", [](UUICanvas* Self, const std::string& Name)
        {
            if (Self) Self->StopWidgetAnimation(Name);
        },

        // ======== 진동 애니메이션 ========

        // 진동 시작 (강도, 지속시간, 빈도, 감쇠여부)
        "ShakeWidget", sol::overload(
            // ShakeWidget(name, intensity)
            [](UUICanvas* Self, const std::string& Name, float Intensity)
            {
                if (Self) Self->ShakeWidget(Name, Intensity, 0.0f, 15.0f, true);
            },
            // ShakeWidget(name, intensity, duration)
            [](UUICanvas* Self, const std::string& Name, float Intensity, float Duration)
            {
                if (Self) Self->ShakeWidget(Name, Intensity, Duration, 15.0f, true);
            },
            // ShakeWidget(name, intensity, duration, frequency)
            [](UUICanvas* Self, const std::string& Name, float Intensity, float Duration, float Frequency)
            {
                if (Self) Self->ShakeWidget(Name, Intensity, Duration, Frequency, true);
            },
            // ShakeWidget(name, intensity, duration, frequency, decay)
            [](UUICanvas* Self, const std::string& Name, float Intensity, float Duration, float Frequency, bool bDecay)
            {
                if (Self) Self->ShakeWidget(Name, Intensity, Duration, Frequency, bDecay);
            }
        ),

        // 진동 중지
        "StopShake", [](UUICanvas* Self, const std::string& Name)
        {
            if (Self) Self->StopWidgetShake(Name);
        },

        // Enter 애니메이션 재생
        "PlayEnterAnimation", [](UUICanvas* Self, const std::string& Name)
        {
            if (Self)
            {
                if (UUIWidget* Widget = Self->FindWidget(Name))
                {
                    Widget->PlayEnterAnimation();
                }
            }
        },

        // Exit 애니메이션 재생
        "PlayExitAnimation", [](UUICanvas* Self, const std::string& Name)
        {
            if (Self)
            {
                if (UUIWidget* Widget = Self->FindWidget(Name))
                {
                    Widget->PlayExitAnimation();
                }
            }
        },

        // 모든 위젯 Enter 애니메이션 재생
        "PlayAllEnterAnimations", [](UUICanvas* Self)
        {
            if (Self) Self->PlayAllEnterAnimations();
        },

        // 모든 위젯 Exit 애니메이션 재생
        "PlayAllExitAnimations", [](UUICanvas* Self)
        {
            if (Self) Self->PlayAllExitAnimations();
        },

        // ======== 버튼 위젯 ========

        // 버튼 생성
        "CreateButton", [](UUICanvas* Self, const std::string& Name, const std::string& TexturePath,
                          float X, float Y, float W, float H) -> bool
        {
            if (!Self) return false;
            return Self->CreateButton(Name, TexturePath, X, Y, W, H,
                                      UGameUIManager::Get().GetD2DContext());
        },

        // 버튼 활성화/비활성화
        "SetButtonInteractable", [](UUICanvas* Self, const std::string& Name, bool bInteractable)
        {
            if (Self) Self->SetButtonInteractable(Name, bInteractable);
        },

        // 버튼 상태별 텍스처 설정
        "SetButtonHoveredTexture", [](UUICanvas* Self, const std::string& Name, const std::string& Path) -> bool
        {
            if (!Self) return false;
            return Self->SetButtonHoveredTexture(Name, Path, UGameUIManager::Get().GetD2DContext());
        },
        "SetButtonPressedTexture", [](UUICanvas* Self, const std::string& Name, const std::string& Path) -> bool
        {
            if (!Self) return false;
            return Self->SetButtonPressedTexture(Name, Path, UGameUIManager::Get().GetD2DContext());
        },
        "SetButtonDisabledTexture", [](UUICanvas* Self, const std::string& Name, const std::string& Path) -> bool
        {
            if (!Self) return false;
            return Self->SetButtonDisabledTexture(Name, Path, UGameUIManager::Get().GetD2DContext());
        },

        // 버튼 상태별 틴트 설정
        "SetButtonNormalTint", [](UUICanvas* Self, const std::string& Name, float R, float G, float B, float A)
        {
            if (Self) Self->SetButtonNormalTint(Name, R, G, B, A);
        },
        "SetButtonHoveredTint", [](UUICanvas* Self, const std::string& Name, float R, float G, float B, float A)
        {
            if (Self) Self->SetButtonHoveredTint(Name, R, G, B, A);
        },
        "SetButtonPressedTint", [](UUICanvas* Self, const std::string& Name, float R, float G, float B, float A)
        {
            if (Self) Self->SetButtonPressedTint(Name, R, G, B, A);
        },
        "SetButtonDisabledTint", [](UUICanvas* Self, const std::string& Name, float R, float G, float B, float A)
        {
            if (Self) Self->SetButtonDisabledTint(Name, R, G, B, A);
        },

        // 버튼 클릭 콜백 설정
        // NOTE: sol::protected_function을 shared_ptr로 래핑하여 람다 캡처 시 수명 문제 해결
        "SetOnClick", [](UUICanvas* Self, const std::string& Name, sol::protected_function Callback)
        {
            if (!Self) return;
            auto* Button = dynamic_cast<UButtonWidget*>(Self->FindWidget(Name));
            if (!Button || !Callback.valid()) return;

            auto CallbackPtr = std::make_shared<sol::protected_function>(Callback);
            Button->OnClick = [CallbackPtr]()
            {
                if (CallbackPtr && CallbackPtr->valid())
                {
                    (*CallbackPtr)();
                }
            };
        },

        // 버튼 호버 시작 콜백 설정
        "SetOnHoverStart", [](UUICanvas* Self, const std::string& Name, sol::protected_function Callback)
        {
            if (!Self) return;
            if (auto* Button = dynamic_cast<UButtonWidget*>(Self->FindWidget(Name)))
            {
                auto CallbackPtr = std::make_shared<sol::protected_function>(Callback);
                Button->OnHoverStart = [CallbackPtr]()
                {
                    if (CallbackPtr && CallbackPtr->valid())
                    {
                        sol::protected_function_result result = (*CallbackPtr)();
                        if (!result.valid())
                        {
                            sol::error err = result;
                            UE_LOG("[UI] Button OnHoverStart error: %s\n", err.what());
                        }
                    }
                };
            }
        },

        // 버튼 호버 종료 콜백 설정
        "SetOnHoverEnd", [](UUICanvas* Self, const std::string& Name, sol::protected_function Callback)
        {
            if (!Self) return;
            if (auto* Button = dynamic_cast<UButtonWidget*>(Self->FindWidget(Name)))
            {
                auto CallbackPtr = std::make_shared<sol::protected_function>(Callback);
                Button->OnHoverEnd = [CallbackPtr]()
                {
                    if (CallbackPtr && CallbackPtr->valid())
                    {
                        sol::protected_function_result result = (*CallbackPtr)();
                        if (!result.valid())
                        {
                            sol::error err = result;
                            UE_LOG("[UI] Button OnHoverEnd error: %s\n", err.what());
                        }
                    }
                };
            }
        }
    );

    // ========================================
    // 캔버스 관리 함수들
    // ========================================

    // 캔버스 생성
    GameUI.set_function("CreateCanvas", sol::overload(
        [](const std::string& Name, int32_t ZOrder) -> UUICanvas*
        {
            return UGameUIManager::Get().CreateCanvas(Name, ZOrder);
        },
        [](const std::string& Name) -> UUICanvas*
        {
            return UGameUIManager::Get().CreateCanvas(Name, 0);
        }
    ));

    // 캔버스 찾기
    GameUI.set_function("FindCanvas", [](const std::string& Name) -> UUICanvas*
    {
        return UGameUIManager::Get().FindCanvas(Name);
    });

    // 캔버스 삭제
    GameUI.set_function("RemoveCanvas", [](const std::string& Name)
    {
        UGameUIManager::Get().RemoveCanvas(Name);
    });

    // 모든 캔버스 삭제
    GameUI.set_function("RemoveAllCanvases", []()
    {
        UGameUIManager::Get().RemoveAllCanvases();
    });

    // 캔버스 가시성 설정
    GameUI.set_function("SetCanvasVisible", [](const std::string& Name, bool bVisible)
    {
        UGameUIManager::Get().SetCanvasVisible(Name, bVisible);
    });

    // 캔버스 Z순서 설정
    GameUI.set_function("SetCanvasZOrder", [](const std::string& Name, int32_t Z)
    {
        UGameUIManager::Get().SetCanvasZOrder(Name, Z);
    });

    // 뷰포트 정보 가져오기
    GameUI.set_function("GetViewportWidth", []() -> float
    {
        return UGameUIManager::Get().GetViewportWidth();
    });

    GameUI.set_function("GetViewportHeight", []() -> float
    {
        return UGameUIManager::Get().GetViewportHeight();
    });

    // .uiasset 파일 로드
    GameUI.set_function("LoadUIAsset", [](const std::string& FilePath) -> UUICanvas*
    {
        return UGameUIManager::Get().LoadUIAsset(FilePath);
    });
}