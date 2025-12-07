#include "pch.h"
#include "IntroCutscene.h"
#include "Source/Game/UI/GameUIManager.h"
#include "Source/Game/UI/Widgets/UICanvas.h"
#include "Source/Game/UI/Widgets/UIWidget.h"
#include "Source/Game/UI/Widgets/TextureWidget.h"

bool UIntroCutscene::Start()
{
	UE_LOG("[IntroCutscene] Starting...\n");

	// UI 에셋 로드
	Canvas = UGameUIManager::Get().LoadUIAsset("Data/UI/Intro.uiasset");
	if (!Canvas)
	{
		UE_LOG("[IntroCutscene] ERROR: Failed to load Intro.uiasset\n");
		// 로드 실패 시 바로 완료 처리
		TransitionTo(EIntroPhase::Finished);
		return false;
	}

	UE_LOG("[IntroCutscene] Intro.uiasset loaded successfully\n");

	// 위젯 초기화
	InitializeWidgets();

	// 첫 페이즈로 전환 (1초 대기)
	TransitionTo(EIntroPhase::InitialWait);

	return true;
}

void UIntroCutscene::Update(float DeltaTime)
{
	if (CurrentPhase == EIntroPhase::Idle || CurrentPhase == EIntroPhase::Finished)
	{
		return;
	}

	PhaseTime += DeltaTime;

	// 페이즈별 처리
	switch (CurrentPhase)
	{
	// === 대기 페이즈들 ===
	case EIntroPhase::InitialWait:
		if (PhaseTime >= WAIT_DURATION)
		{
			TransitionTo(EIntroPhase::GtlEnter);
		}
		break;

	case EIntroPhase::GtlWait:
		if (PhaseTime >= WAIT_DURATION)
		{
			TransitionTo(EIntroPhase::TeamLabelEnter);
		}
		break;

	case EIntroPhase::TeamLabelWait:
		if (PhaseTime >= WAIT_DURATION)
		{
			TransitionTo(EIntroPhase::MembersEnter);
		}
		break;

	// === 애니메이션 페이즈들 (완료까지 대기) ===
	case EIntroPhase::GtlEnter:
		if (IsWidgetAnimationDone("gtl"))
		{
			TransitionTo(EIntroPhase::GtlExit);
		}
		break;

	case EIntroPhase::GtlExit:
		if (IsWidgetAnimationDone("gtl"))
		{
			TransitionTo(EIntroPhase::GtlWait);
		}
		break;

	case EIntroPhase::TeamLabelEnter:
		if (IsWidgetAnimationDone("team_label"))
		{
			TransitionTo(EIntroPhase::TeamLabelExit);
		}
		break;

	case EIntroPhase::TeamLabelExit:
		if (IsWidgetAnimationDone("team_label"))
		{
			TransitionTo(EIntroPhase::TeamLabelWait);
		}
		break;

	case EIntroPhase::MembersEnter:
		if (AreMembersAnimationDone())
		{
			TransitionTo(EIntroPhase::MembersWait);
		}
		break;

	case EIntroPhase::MembersWait:
		if (PhaseTime >= MEMBERS_WAIT_DURATION)
		{
			TransitionTo(EIntroPhase::MembersExit);
		}
		break;

	case EIntroPhase::MembersExit:
		if (AreMembersAnimationDone())
		{
			TransitionTo(EIntroPhase::Finished);
		}
		break;

	default:
		break;
	}
}

void UIntroCutscene::Skip()
{
	UE_LOG("[IntroCutscene] Skipped!\n");
	Cleanup();
	TransitionTo(EIntroPhase::Finished);
}

void UIntroCutscene::TransitionTo(EIntroPhase NewPhase)
{
	UE_LOG("[IntroCutscene] Phase: %d -> %d\n", static_cast<int>(CurrentPhase), static_cast<int>(NewPhase));

	CurrentPhase = NewPhase;
	PhaseTime = 0.0f;

	// 페이즈 진입 시 액션
	switch (NewPhase)
	{
	case EIntroPhase::InitialWait:
		UE_LOG("[IntroCutscene] Initial wait (1 sec)...\n");
		break;

	case EIntroPhase::GtlEnter:
		PlayWidgetEnter("gtl");
		// SubUV 위젯도 함께 Enter Animation + SubUV 자동 재생 시작
		PlayWidgetEnter("SubUV");
		if (Canvas)
		{
			if (UUIWidget* SubUVWidget = Canvas->FindWidget("SubUV"))
			{
				if (UTextureWidget* TexWidget = dynamic_cast<UTextureWidget*>(SubUVWidget))
				{
					TexWidget->StartSubUVAnimation(10.0f, true);  // 10 FPS, 루프
					UE_LOG("[IntroCutscene] SubUV animation started with gtl\n");
				}
			}
		}
		break;

	case EIntroPhase::GtlExit:
		PlayWidgetExit("gtl");
		// SubUV 위젯도 함께 Exit Animation (UI Editor에서 설정한 애니메이션 사용)
		PlayWidgetExit("SubUV");
		UE_LOG("[IntroCutscene] SubUV exit animation started with gtl exit\n");
		break;

	case EIntroPhase::GtlWait:
		UE_LOG("[IntroCutscene] GTL wait (1 sec)...\n");
		// SubUV 위젯 숨기고 애니메이션 정지
		if (Canvas)
		{
			if (UUIWidget* SubUVWidget = Canvas->FindWidget("SubUV"))
			{
				if (UTextureWidget* TexWidget = dynamic_cast<UTextureWidget*>(SubUVWidget))
				{
					TexWidget->StopSubUVAnimation();
				}
			}
			Canvas->SetWidgetVisible("SubUV", false);
			UE_LOG("[IntroCutscene] SubUV hidden after gtl exit\n");
		}
		break;

	case EIntroPhase::TeamLabelEnter:
		PlayWidgetEnter("team_label");
		break;

	case EIntroPhase::TeamLabelExit:
		//PlayWidgetExit("team_label");
		break;

	case EIntroPhase::TeamLabelWait:
		UE_LOG("[IntroCutscene] TeamLabel wait (1 sec)...\n");
		break;

	case EIntroPhase::MembersEnter:
		// mb1 ~ mb4 동시 Enter
		PlayWidgetEnter("mb1");
		PlayWidgetEnter("mb2");
		PlayWidgetEnter("mb3");
		PlayWidgetEnter("mb4");
		break;

	case EIntroPhase::MembersWait:
		UE_LOG("[IntroCutscene] Members wait (%.1f sec)...\n", MEMBERS_WAIT_DURATION);
		break;

	case EIntroPhase::MembersExit:
		// mb1 ~ mb4 동시 Exit
		PlayWidgetExit("team_label");
		PlayWidgetExit("mb1");
		PlayWidgetExit("mb2");
		PlayWidgetExit("mb3");
		PlayWidgetExit("mb4");
		break;

	case EIntroPhase::Finished:
		// 완료
		Cleanup();

		// 콜백 호출
		if (OnFinished)
		{
			UE_LOG("[IntroCutscene] Calling OnFinished callback\n");
			OnFinished(Owner);
		}
		break;

	default:
		break;
	}
}

void UIntroCutscene::InitializeWidgets()
{
	if (!Canvas)
	{
		return;
	}

	UE_LOG("[IntroCutscene] Initializing widgets (hiding all)\n");

	// 모든 위젯 숨기기
	Canvas->SetWidgetVisible("gtl", false);
	Canvas->SetWidgetVisible("SubUV", false);  // SubUV 위젯도 숨김
	Canvas->SetWidgetVisible("team_label", false);
	Canvas->SetWidgetVisible("mb1", false);
	Canvas->SetWidgetVisible("mb2", false);
	Canvas->SetWidgetVisible("mb3", false);
	Canvas->SetWidgetVisible("mb4", false);
}

void UIntroCutscene::PlayWidgetEnter(const std::string& WidgetName)
{
	if (!Canvas)
	{
		return;
	}

	UUIWidget* Widget = Canvas->FindWidget(WidgetName);
	if (Widget)
	{
		Widget->bVisible = true;
		Widget->PlayEnterAnimation();
		UE_LOG("[IntroCutscene] Playing Enter animation: %s\n", WidgetName.c_str());
	}
	else
	{
		UE_LOG("[IntroCutscene] WARNING: Widget not found: %s\n", WidgetName.c_str());
	}
}

void UIntroCutscene::PlayWidgetExit(const std::string& WidgetName)
{
	if (!Canvas)
	{
		return;
	}

	UUIWidget* Widget = Canvas->FindWidget(WidgetName);
	if (Widget)
	{
		Widget->PlayExitAnimation();
		UE_LOG("[IntroCutscene] Playing Exit animation: %s\n", WidgetName.c_str());
	}
	else
	{
		UE_LOG("[IntroCutscene] WARNING: Widget not found: %s\n", WidgetName.c_str());
	}
}

bool UIntroCutscene::IsWidgetAnimationDone(const std::string& WidgetName)
{
	if (!Canvas)
	{
		return true;  // 캔버스 없으면 완료로 처리
	}

	UUIWidget* Widget = Canvas->FindWidget(WidgetName);
	if (!Widget)
	{
		return true;  // 위젯 없으면 완료로 처리
	}

	return !Widget->IsAnimating();
}

bool UIntroCutscene::AreMembersAnimationDone()
{
	return IsWidgetAnimationDone("mb1") &&
	       IsWidgetAnimationDone("mb2") &&
	       IsWidgetAnimationDone("mb3") &&
	       IsWidgetAnimationDone("mb4");
}

void UIntroCutscene::Cleanup()
{
	UE_LOG("[IntroCutscene] Cleaning up...\n");

	// 캔버스 제거
	if (Canvas)
	{
		UGameUIManager::Get().RemoveCanvas(CanvasName);
		Canvas = nullptr;
	}

	UE_LOG("[IntroCutscene] Cleanup complete\n");
}
