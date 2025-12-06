#pragma once

#include "TextureWidget.h"
#include <functional>

/**
 * @brief 버튼 상태 열거형
 */
enum class EButtonState : uint8_t
{
    Normal,     // 기본 상태
    Hovered,    // 마우스 오버
    Pressed,    // 눌린 상태
    Disabled    // 비활성화
};

/**
 * @brief 버튼 위젯
 *
 * 클릭 가능한 UI 버튼입니다.
 * 상태별로 다른 텍스처/색상을 표시할 수 있습니다.
 */
class UButtonWidget : public UTextureWidget
{
public:
    UButtonWidget();
    virtual ~UButtonWidget()
    {
        // Lua 콜백 정리 (Lua 상태보다 먼저 소멸되도록)
        OnClick = nullptr;
        OnHoverStart = nullptr;
        OnHoverEnd = nullptr;
        OnPressStart = nullptr;
        OnPressEnd = nullptr;
        ReleaseAllTextures();
    }

    // === 버튼 상태 ===
    EButtonState State = EButtonState::Normal;
    bool bInteractable = true;  // false면 클릭 불가 (Disabled 상태)

    // === 상태별 텍스처 (선택적) ===
    ID2D1Bitmap* HoveredBitmap = nullptr;
    ID2D1Bitmap* PressedBitmap = nullptr;
    ID2D1Bitmap* DisabledBitmap = nullptr;

    // === 상태별 색상 틴트 ===
    D2D1_COLOR_F NormalTint;
    D2D1_COLOR_F HoveredTint;
    D2D1_COLOR_F PressedTint;
    D2D1_COLOR_F DisabledTint;

    // === 콜백 함수 ===
    std::function<void()> OnClick;
    std::function<void()> OnHoverStart;
    std::function<void()> OnHoverEnd;
    std::function<void()> OnPressStart;
    std::function<void()> OnPressEnd;

    // === 콜백 ID (Lua 바인딩용) ===
    std::string OnClickCallbackName;
    std::string OnHoverStartCallbackName;
    std::string OnHoverEndCallbackName;

    /**
     * @brief 렌더링 (상태에 따른 텍스처/틴트 적용)
     */
    void Render(ID2D1DeviceContext* Context) override;

    /**
     * @brief 상태별 텍스처 로드
     */
    bool LoadHoveredTexture(const wchar_t* Path, ID2D1DeviceContext* Context);
    bool LoadPressedTexture(const wchar_t* Path, ID2D1DeviceContext* Context);
    bool LoadDisabledTexture(const wchar_t* Path, ID2D1DeviceContext* Context);

    /**
     * @brief 상태별 틴트 색상 설정
     */
    void SetNormalTint(float R, float G, float B, float A);
    void SetHoveredTint(float R, float G, float B, float A);
    void SetPressedTint(float R, float G, float B, float A);
    void SetDisabledTint(float R, float G, float B, float A);

    /**
     * @brief 상태 변경
     */
    void SetState(EButtonState NewState);
    EButtonState GetState() const { return State; }

    /**
     * @brief 활성화/비활성화
     */
    void SetInteractable(bool bEnabled);
    bool IsInteractable() const { return bInteractable; }

    // === 마우스 이벤트 처리 (UICanvas에서 호출) ===

    /**
     * @brief 마우스가 버튼 위로 들어왔을 때
     */
    void OnMouseEnter();

    /**
     * @brief 마우스가 버튼에서 나갔을 때
     */
    void OnMouseLeave();

    /**
     * @brief 마우스 버튼 눌렀을 때
     */
    void OnMouseDown();

    /**
     * @brief 마우스 버튼 뗐을 때
     * @return 클릭이 완료되었으면 true
     */
    bool OnMouseUp();

    /**
     * @brief 텍스처 해제
     */
    void ReleaseAllTextures();

private:
    bool bMouseOver = false;    // 마우스가 버튼 위에 있는지
    bool bMouseDown = false;    // 마우스가 눌린 상태인지

    /**
     * @brief 현재 상태에 맞는 텍스처 반환
     */
    ID2D1Bitmap* GetCurrentBitmap() const;

    /**
     * @brief 현재 상태에 맞는 틴트 반환
     */
    D2D1_COLOR_F GetCurrentTint() const;
};
