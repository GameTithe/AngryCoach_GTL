#pragma once

#include <d2d1_1.h>
#include <dwrite.h>
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>

// Forward declarations
struct ID3D11Device;
struct ID3D11DeviceContext;
struct IDXGISwapChain;
class UUIWidget;
class UProgressBarWidget;
class UTextureWidget;
class UUICanvas;

/**
 * @brief UI 매니저 - Direct2D 기반 UI 시스템
 *
 * Direct2D를 사용하여 2D UI를 렌더링하는 범용 UI 시스템입니다.
 * Canvas와 Widget을 통해 동적 UI를 구성할 수 있습니다.
 */
class UGameUIManager
{
public:
    static UGameUIManager& Get();

    // 초기화 및 종료
    void Initialize(ID3D11Device* InDevice, ID3D11DeviceContext* InContext, IDXGISwapChain* InSwapChain);
    void Shutdown();

    // 매 프레임 호출
    void Update(float DeltaTime);
    void Render();

    // Direct2D 리소스 접근 (자식 UI에서 사용)
    ID2D1DeviceContext* GetD2DContext() const { return D2DContext; }
    IDWriteFactory* GetDWriteFactory() const { return DWriteFactory; }

    // 공용 브러시
    ID2D1SolidColorBrush* GetWhiteBrush() const { return BrushWhite; }
    ID2D1SolidColorBrush* GetBlackBrush() const { return BrushBlack; }
    ID2D1SolidColorBrush* GetRedBrush() const { return BrushRed; }
    ID2D1SolidColorBrush* GetYellowBrush() const { return BrushYellow; }
    ID2D1SolidColorBrush* GetBlueBrush() const { return BrushBlue; }
    ID2D1SolidColorBrush* GetGreenBrush() const { return BrushGreen; }

    // 텍스트 포맷
    IDWriteTextFormat* GetDefaultTextFormat() const { return TextFormatDefault; }
    IDWriteTextFormat* GetLargeTextFormat() const { return TextFormatLarge; }

    // 화면 크기
    float GetScreenWidth() const { return ScreenWidth; }
    float GetScreenHeight() const { return ScreenHeight; }

    // 뷰포트 설정 (에디터에서 사용)
    void SetViewport(float X, float Y, float Width, float Height);
    float GetViewportX() const { return ViewportX; }
    float GetViewportY() const { return ViewportY; }
    float GetViewportWidth() const { return ViewportWidth; }
    float GetViewportHeight() const { return ViewportHeight; }

    // ============================================
    // Canvas 시스템 (Lua 동적 UI)
    // ============================================

    /**
     * @brief 캔버스 생성
     * @param Name 캔버스 이름
     * @param ZOrder 렌더링 순서 (높을수록 위에 그려짐)
     * @return 생성된 캔버스 포인터 (실패 시 nullptr)
     */
    UUICanvas* CreateCanvas(const std::string& Name, int32_t ZOrder = 0);

    /**
     * @brief 이름으로 캔버스 찾기
     */
    UUICanvas* FindCanvas(const std::string& Name);

    /**
     * @brief 캔버스 삭제
     */
    void RemoveCanvas(const std::string& Name);

    /**
     * @brief 모든 캔버스 삭제
     */
    void RemoveAllCanvases();

    /**
     * @brief 캔버스 가시성 설정
     */
    void SetCanvasVisible(const std::string& Name, bool bVisible);

    /**
     * @brief 캔버스 Z순서 설정
     */
    void SetCanvasZOrder(const std::string& Name, int32_t Z);

    /**
     * @brief .uiasset 파일에서 UI를 로드하여 캔버스와 위젯 생성
     * @param FilePath .uiasset 파일 경로
     * @return 생성된 캔버스 포인터 (실패 시 nullptr)
     */
    UUICanvas* LoadUIAsset(const std::string& FilePath);

    /**
     * @brief UI가 마우스 입력을 소비했는지 확인
     * @return UI(버튼 등)가 입력을 처리했으면 true
     */
    bool IsUIConsumedInput() const { return bUIConsumedInput; }

private:
    UGameUIManager() = default;
    ~UGameUIManager() = default;
    UGameUIManager(const UGameUIManager&) = delete;
    UGameUIManager& operator=(const UGameUIManager&) = delete;

    // D2D 리소스 생성/해제
    void CreateD2DResources();
    void ReleaseD2DResources();
    void CreateBrushes();
    void CreateTextFormats();

    // 렌더링 헬퍼
    void BeginD2DDraw();
    void EndD2DDraw();

private:
    bool bInitialized = false;

    // D3D 참조
    ID3D11Device* D3DDevice = nullptr;
    ID3D11DeviceContext* D3DContext = nullptr;
    IDXGISwapChain* SwapChain = nullptr;

    // D2D 리소스
    ID2D1Factory1* D2DFactory = nullptr;
    ID2D1Device* D2DDevice = nullptr;
    ID2D1DeviceContext* D2DContext = nullptr;
    IDWriteFactory* DWriteFactory = nullptr;

    // 현재 렌더 타겟 (Draw 호출마다 갱신)
    ID2D1Bitmap1* CurrentTargetBitmap = nullptr;
    IDXGISurface* CurrentSurface = nullptr;

    // 공용 브러시
    ID2D1SolidColorBrush* BrushWhite = nullptr;
    ID2D1SolidColorBrush* BrushBlack = nullptr;
    ID2D1SolidColorBrush* BrushBlackTransparent = nullptr;
    ID2D1SolidColorBrush* BrushRed = nullptr;
    ID2D1SolidColorBrush* BrushYellow = nullptr;
    ID2D1SolidColorBrush* BrushBlue = nullptr;
    ID2D1SolidColorBrush* BrushGreen = nullptr;

    // 텍스트 포맷
    IDWriteTextFormat* TextFormatDefault = nullptr;   // 기본 폰트 (16pt)
    IDWriteTextFormat* TextFormatLarge = nullptr;     // 큰 폰트 (32pt)

    // 화면 크기 (전체 창)
    float ScreenWidth = 1920.0f;
    float ScreenHeight = 1080.0f;

    // 뷰포트 크기 (에디터 뷰포트 또는 게임 영역)
    float ViewportX = 0.0f;
    float ViewportY = 0.0f;
    float ViewportWidth = 1920.0f;
    float ViewportHeight = 1080.0f;

    // ============================================
    // Canvas 시스템
    // ============================================

    // 캔버스 컨테이너 (이름으로 빠른 검색)
    std::unordered_map<std::string, std::unique_ptr<UUICanvas>> Canvases;

    // Z순서로 정렬된 렌더링 순서 (ZOrder 변경 시 갱신)
    std::vector<UUICanvas*> SortedCanvases;
    bool bCanvasesSortDirty = false;

    // 캔버스 렌더링
    void RenderCanvases();

    // 정렬 갱신
    void UpdateCanvasSortOrder();

    // 마우스 입력 처리
    void ProcessMouseInput();
    bool bUIConsumedInput = false;
};