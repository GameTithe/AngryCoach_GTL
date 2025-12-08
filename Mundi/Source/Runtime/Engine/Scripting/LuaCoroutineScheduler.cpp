#include "pch.h"
#include "LuaCoroutineScheduler.h"

void FLuaCoroutineScheduler::ShutdownBeforeLuaClose()
{
	for (auto& Task : Tasks)
	{
		if (Task.Co.valid())
		{
			Task.Co.abandon(); // Lua쪽 Coroutine 무력화 필수
		}
	}
	Tasks.Empty(); 
}

FLuaCoroutineScheduler::FLuaCoroutineScheduler()
{
	Tasks.Reserve(100);
}

FLuaCoroHandle FLuaCoroutineScheduler::Register(sol::thread&& Thread, sol::coroutine&& Co, void* Owner)
{
	FCoroTask Task;
	Task.Thread = std::move(Thread); /* Thread Anchoring */
	Task.Co     = std::move(Co);
	Task.Owner  = Owner;
	Task.Id     = ++NextId;

	const uint32 TaskId = Task.Id; // move 전에 Id 저장

	// Process 중이면 대기열에 추가 (반복자 무효화 방지)
	if (bIsProcessing)
	{
		PendingTasks.push_back(std::move(Task));
	}
	else
	{
		Tasks.push_back(std::move(Task));
	}

	return FLuaCoroHandle{ TaskId };
}

void FLuaCoroutineScheduler::Tick(double DeltaTime)
{
	// TODO : Release 할 때는(Debug 안 하는 모드에서) MaxDeltaClamp 뺄 것!
	const double Clamped = std::min(DeltaTime, MaxDeltaClamp);
	NowSeconds += Clamped;

	Process(NowSeconds);
}
void FLuaCoroutineScheduler::Process(double Now)
{
	bIsProcessing = true;

	for (size_t i = 0; i < Tasks.Num(); ++i)
	{
		auto& task = Tasks[i];
		if (task.Finished)
			continue;

		// 조건 충족 여부
		switch (task.WaitType)
		{
		case EWaitType::Time:
			// UE_LOG("Now=%.3f Wake=%.3f", TotalTime, task.WakeTime);
			if (task.WakeTime > Now) continue;
			break;
		case EWaitType::Predicate:
			// 람다 함수가 있지만, 조건이 달성 안 됐을 때
			if (task.Predicate && !task.Predicate()) continue; 
			break;
			// case EWaitType::Event: // Event "Trigger"는 Tick 함수 내에서 조건 확인하지 않음
		}

		// 조건 충족 시 resume 실행
		sol::protected_function_result Result = task.Co();
		if (!Result.valid())
		{
			sol::error Err = Result;
			UE_LOG("[Lua][error] Coroutine error: %s\n", Err.what());
			task.Finished = true;
			continue;
		}
		
		// 이후 yield가 다시 올 경우, 다음 조건 실행 = 재세팅
		if (Result.status() == sol::call_status::yielded)
		{
			std::string Tag = Result.get<FString>(0); // 해당 Co의 첫번째 string 매개변수
			if (Tag == "wait_time")
			{
				double Sec = Result.get<double>(1);
				task.WaitType = EWaitType::Time;
				task.WakeTime = Now + Sec;
			}
			else if (Tag == "wait_predicate")
			{
				sol::function Condition = Result.get<sol::function>(1); 
				task.WaitType = EWaitType::Predicate;
				task.Predicate = [Condition]()
				{
					sol::protected_function_result Result = Condition();
					if (!Result.valid()) return false; 
					return Result.get<bool>();
				};
			}
			else if (Tag == "wait_event")
			{
				task.WaitType = EWaitType::Event;
				task.EventName = Result.get<FString>(1);
			}
			else
			{
				task.WaitType = EWaitType::None;
			}
		}
		else if (Result.status() == sol::call_status::runtime)
		{
			task.Finished = true;
		}
		else if (Result.status() == sol::call_status::ok)
		{
			task.Finished = true;
		}
		else if (Result.status() == sol::call_status::file)
		{
			task.Finished = true;
		}
		else if (Result.status() == sol::call_status::memory)
		{
			task.Finished = true;
		}
	}

	bIsProcessing = false;

	// 완료된 태스크 정리 (역순으로 순회하여 인덱스 무효화 방지)
	for (int32 i = Tasks.Num() - 1; i >= 0; --i)
	{
		if (Tasks[i].Finished)
		{
			Tasks.RemoveAtSwap(i);
		}
	}

	// 대기 중인 태스크 추가
	FlushPendingTasks();
}

void FLuaCoroutineScheduler::FlushPendingTasks()
{
	for (auto& Task : PendingTasks)
	{
		Tasks.push_back(std::move(Task));
	}
	PendingTasks.Empty();
}

void FLuaCoroutineScheduler::AddCoroutine(sol::coroutine&& Co)
{
	Tasks.push_back({std::move(Co)});
}

void FLuaCoroutineScheduler::TriggerEvent(const FString& EventName)
{
	bIsProcessing = true;

	for (size_t i = 0; i < Tasks.Num(); ++i)
	{
		auto& task = Tasks[i];
		if (task.Finished) continue;
		if (task.WaitType != EWaitType::Event) continue;
		if (task.EventName == EventName)
		{
			task.WaitType = EWaitType::None;
			task.Co(); // resume
		}
	}

	bIsProcessing = false;
	FlushPendingTasks();
}

void FLuaCoroutineScheduler::CancelByOwner(void* Owner)
{
	for (auto& Task : Tasks) {
		if (Task.Owner == Owner && !Task.Finished) {
			Task.Finished = true;
			Task.Co = sol::coroutine(); // 참조 해제
		}
	}
}
