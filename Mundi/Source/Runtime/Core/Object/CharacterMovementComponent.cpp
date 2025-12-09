#include "pch.h"
#include "CharacterMovementComponent.h"

#include "AngryCoachCharacter.h"
#include "Character.h"
#include "SceneComponent.h"
#include "CapsuleComponent.h"
#include "World.h"
#include "Source/Runtime/Engine/Physics/PhysScene.h"
#include "Source/Runtime/Engine/Collision/Collision.h"

UCharacterMovementComponent::UCharacterMovementComponent()
{
	// 캐릭터 전용 설정 값
 	MaxWalkSpeed = 6.0f;
	MaxAcceleration = 20.0f;
	JumpZVelocity = 4.0;

	BrackingDeceleration = 20.0f; // 입력이 없을 때 감속도
	GroundFriction = 8.0f; //바닥 마찰 계수
}

UCharacterMovementComponent::~UCharacterMovementComponent()
{
}

void UCharacterMovementComponent::InitializeComponent()
{
	Super::InitializeComponent();

	CharacterOwner = Cast<ACharacter>(GetOwner());
}

void UCharacterMovementComponent::TickComponent(float DeltaSeconds)
{
	//Super::TickComponent(DeltaSeconds);

	if (!UpdatedComponent || !CharacterOwner) return;

	// 강제 이동 중인 경우 (스킬 사용 등)
	if (bForceMovement)
	{
		ForcedMovementTimer += DeltaSeconds;

		// 타이머가 Duration을 초과하면 강제 이동 종료
		if (ForcedMovementTimer >= ForcedMovementDuration)
		{
			ClearForcedMovement();
		}
		else
		{
			// 강제 속도로 이동
			Velocity = ForcedVelocity;
			FHitResult Hit;
			SafeMoveUpdatedComponent(Velocity * DeltaSeconds, Hit);

			// 충돌 시 강제 이동 종료
			if (Hit.bBlockingHit)
			{
				ClearForcedMovement();
			} 
			return;
		}
	}

	if (bIsApplyKnockback)
	{
		ApplyKnockback(DeltaSeconds);
	}

	// 일반 이동 로직
	if (bIsFalling)
	{
		PhysFalling(DeltaSeconds);
	}
	else
	{
		PhysWalking(DeltaSeconds);
	}
}

void UCharacterMovementComponent::DoJump()
{
	// 점프 가능 조건: 사용한 횟수 < 최대 횟수
	if (CurrentJumpCount < MaxJumpCount)
	{
		Velocity.Z = JumpZVelocity;
		bIsFalling = true;
		CurrentJumpCount++;
	}
}

void UCharacterMovementComponent::StopJump()
{
	//if (bIsFalling && Velocity.Z > 0.0f)
	//{
	//	Velocity.Z *= 0.5f;
	//}
}

void UCharacterMovementComponent::ResolvePenetration(const FHitResult& Hit)
{
	AActor* OtherActor = Hit.HitActor;
	if (!OtherActor)
	{
		return;
	}

	// 내 캡슐 정보
	float Radius, HalfHeight;
	GetCapsuleSize(Radius, HalfHeight);
	FVector P1 = UpdatedComponent->GetWorldLocation();

	// 상대방 캡슐 정보 (상대방이 ACharacter라고 가정)
	float R2 = 0.5f; // 기본값
	FVector P2 = OtherActor->GetActorLocation();
    
	ACharacter* OtherChar = Cast<ACharacter>(OtherActor);
	if (OtherChar && OtherChar->GetCapsuleComponent())
	{
		R2 = OtherChar->GetCapsuleComponent()->CapsuleRadius;
		// 스케일 고려는 생략(필요시 추가)
	}

	// 수평 거리 계산 (Z축 제외)
	FVector Diff = P1 - P2;
	Diff.Z = 0.0f;
	float DistSq = Diff.SizeSquared();
	float MinDist = Radius + R2;

	// 겹쳐 있다면 (거리 < 반지름 합)
	if (DistSq < MinDist * MinDist)
	{
		float Dist = FMath::Sqrt(DistSq);
        
		// 밀어낼 방향 (중심 -> 중심)
		FVector PushDir;
		if (Dist > KINDA_SMALL_NUMBER)
		{
			PushDir = Diff / Dist;
		}
		else
		{
			PushDir = UpdatedComponent->GetWorldRotation().GetForwardVector(); // 완전히 겹치면 앞쪽으로
		}

		// 밀어낼 거리: (반지름 합 - 현재 거리) + 약간의 여유(SkinWidth)
		float PushDist = (MinDist - Dist) + 0.005f;
		PushDist = FMath::Max(PushDist, 0.05f);

		// 강제 이동
		UpdatedComponent->AddWorldOffset(PushDir * PushDist);
	}
}

void UCharacterMovementComponent::PhysWalking(float DeltaSecond)
{
	// 입력 벡터 가져오기
	FVector InputVector = CharacterOwner->ConsumeMovementInputVector();

	// z축 입력은 걷기에서 무시
	InputVector.Z = 0.0f;
	if (!InputVector.IsZero())
	{
		InputVector.Normalize();
	}

	// 속도 계산
	CalcVelocity(InputVector, DeltaSecond, GroundFriction, BrackingDeceleration);

	if (!CharacterOwner || !CharacterOwner->GetController())
	{
		Acceleration = FVector::Zero();
		Velocity = FVector::Zero();
		return;
	}

	FVector DeltaLoc = Velocity * DeltaSecond;
	FVector RemainingDelta = DeltaLoc;
	for (int32 i = 0; i < MaxIteration; i++)
	{
		if (RemainingDelta.IsZero())
		{
			break;
		}
		FHitResult Hit;
		bool bMoved = SafeMoveUpdatedComponent(RemainingDelta, Hit);

		if (bMoved && !Hit.bBlockingHit)
		{
			break;
		}

		if (Hit.bBlockingHit)
		{
			/*
			 * 끼임(sticking) 방지
			 * 이동 거리가 0에 가깝거나 이미 겹쳐있는 상태라면 이동하다 부딪힌 게 아니라 껴있는 상태
			 */
			if (Hit.Distance < 0.001f)
			{
				// 동작은 하는데 벽에 끼이는 문제 있음
				// 쓰고 싶으면 좀 더 깎아야함
				// ResolvePenetration(Hit);
				AActor* Owner = GetOwner();
				AActor* HitActor = Hit.HitActor;
				// 다른 액터인지 확인
				if (HitActor && Owner != HitActor)
				{
					FVector MyLocation = UpdatedComponent->GetWorldLocation();
					FVector OtherLocation = HitActor->GetActorLocation();
					// z축 무시하고 수평으로 밀기
					MyLocation.Z = 0.0f;
					OtherLocation.Z = 0.0f;
				
					FVector PushDirection = MyLocation - OtherLocation;
					if (PushDirection.IsZero())
					{
						PushDirection = UpdatedComponent->GetWorldRotation().GetForwardVector();
					}
					PushDirection.Normalize();
				
					const float PushDistance = 0.01f;
					FVector FixVector = PushDirection * PushDistance;
					UpdatedComponent->AddWorldOffset(FixVector);
				}
			}
			
			float Time = 0.0f;
			if (RemainingDelta.Size() > KINDA_SMALL_NUMBER)
			{
				Time = Hit.Distance / RemainingDelta.Size();
			}

			// 남은 벡터 계산 (전체 - 이동한 것)
			FVector Travelled = RemainingDelta * Time;
			FVector Remaining = RemainingDelta - Travelled;

			// [핵심] 남은 벡터만 벽을 타고 미끄러지게 함
			FVector SlideVector = SlideAlongSurface(Remaining, Hit);

			// 다음 루프에서 이 슬라이딩 벡터만큼 이동 시도
			RemainingDelta = SlideVector;
		}
	}

	// Sweep 검사를 통한 안전한 이동
	// if (!DeltaLoc.IsZero())
	// {
	// 	FHitResult Hit;
	// 	bool bMoved = SafeMoveUpdatedComponent(DeltaLoc, Hit);
	//
	// 	// 충돌 시 슬라이딩 처리
	// 	if (!bMoved && Hit.bBlockingHit)
	// 	{
	// 		FVector SlideVector = SlideAlongSurface(DeltaLoc, Hit);
	// 		if (!SlideVector.IsZero())
	// 		{
	// 			FHitResult SlideHit;
	// 			SafeMoveUpdatedComponent(SlideVector, SlideHit);
	// 		}
	// 	}
	// }

	// 바닥 검사
	FHitResult FloorHit;
	if (!CheckFloor(FloorHit))
	{
		// 경사면 내려가기: 더 긴 거리로 바닥 찾기
		const float MaxStepDownHeight = 0.5f;
		FVector StepDownStart = UpdatedComponent->GetWorldLocation();
		FVector StepDownEnd = StepDownStart - FVector(0, 0, MaxStepDownHeight);

		float Radius, HalfHeight;
		GetCapsuleSize(Radius, HalfHeight);

		FPhysScene* PhysScene = GetPhysScene();
		FHitResult StepDownHit;

		if (PhysScene && PhysScene->SweepCapsule(StepDownStart, StepDownEnd, Radius, HalfHeight, StepDownHit, CharacterOwner))
		{
			// 바닥 찾음 - 스냅 (경사면 내려가기)
			if (StepDownHit.ImpactNormal.Z > 0.7f)
			{
				const float SkinWidth = 0.00125f;
				float SnapDistance = StepDownHit.Distance - SkinWidth;
				if (SnapDistance > KINDA_SMALL_NUMBER)
				{
					FVector NewLoc = UpdatedComponent->GetWorldLocation();
					NewLoc.Z -= SnapDistance;
					UpdatedComponent->SetWorldLocation(NewLoc);
				}
				// Walking 유지
				return;
			}
		}

		// 바닥 못 찾음 - Falling
		bIsFalling = true;
	}
}

void UCharacterMovementComponent::PhysFalling(float DeltaSecond)
{
	// 중력 적용
	float ActualGravity = GLOBAL_GRAVITY_Z * GravityScale;
	Velocity.Z += ActualGravity * DeltaSecond;

	// 위치 이동 (Sweep 검사)
	FVector DeltaLoc = Velocity * DeltaSecond;

	FHitResult Hit;
	bool bMoved = SafeMoveUpdatedComponent(DeltaLoc, Hit);

	// 충돌 처리
	if (!bMoved && Hit.bBlockingHit)
	{
		// 바닥에 착지했는지 확인 (충돌 노말이 위를 향하면 바닥)
		// 단, 상승 중(Velocity.Z > 0)에는 착지하지 않음 (경사면에서 점프 시)
		if (Hit.ImpactNormal.Z > 0.7f && Velocity.Z <= 0.0f)
		{
			// 착지
			Velocity.Z = 0.0f;
			bIsFalling = false;
			CurrentJumpCount = 0;  // 점프 횟수 리셋
			// SafeMoveUpdatedComponent에서 이미 SkinWidth 적용된 위치로 설정됨
			// Hit.Location으로 덮어쓰면 경사면에 박힘
		}
		else
		{
			// 벽 또는 상승 중 경사면 충돌 - 슬라이딩
			FVector SlideVector = SlideAlongSurface(DeltaLoc, Hit);
			if (!SlideVector.IsZero())
			{
				FHitResult SlideHit;
				bool bSlided = SafeMoveUpdatedComponent(SlideVector, SlideHit);

				// 2차 슬라이딩 (모서리 처리)
				if (!bSlided && SlideHit.bBlockingHit)
				{
					FVector SlideVector2 = SlideAlongSurface(SlideVector, SlideHit);
					if (!SlideVector2.IsZero())
					{
						FHitResult SlideHit2;
						SafeMoveUpdatedComponent(SlideVector2, SlideHit2);
					}
				}
			}

			// 속도에서 충돌 방향 성분 제거 (벽에 부딪히면 그 방향 속도 0)
			float VelDot = FVector::Dot(Velocity, Hit.ImpactNormal);
			if (VelDot < 0.0f)
			{
				Velocity = Velocity - Hit.ImpactNormal * VelDot;
			}
		}
	}
	else
	{
		// 상승 중일 때는 바닥 검사 건너뛰기 (점프 직후 바로 착지 방지)
		if (Velocity.Z > 0.0f)
			return;

		// 바닥 검사
		FHitResult FloorHit;
		if (CheckFloor(FloorHit))
		{
			// 바닥에 닿음
			Velocity.Z = 0.0f;
			bIsFalling = false;
			CurrentJumpCount = 0;  // 점프 횟수 리셋

			// 여기다 놓으면 안좋음
			// StopJump처리가 애매해서 땜빵식 코드
			// 캐릭터 클래스에서 처리하는게 좋다.
			CharacterOwner->SetCurrentState(ECharacterState::Idle);			

			// 바닥으로 스냅 (SkinWidth 여유를 두고 이동)
			const float SkinWidth = 0.00125f;
			float SnapDistance = FloorHit.Distance - SkinWidth;
			if (SnapDistance > KINDA_SMALL_NUMBER)
			{
				FVector CurrentLoc = UpdatedComponent->GetWorldLocation();
				CurrentLoc.Z -= SnapDistance;
				UpdatedComponent->SetWorldLocation(CurrentLoc);
			}
		}
	}
}

void UCharacterMovementComponent::CalcVelocity(const FVector& Input, float DeltaSecond, float Friction, float BrackingDecel)
{
	//  Z축을 속도에서 제외
	FVector CurrentVelocity = Velocity;
	CurrentVelocity.Z = 0.0f;

	// 입력이 있는 지 확인
	bool bHasInput = !Input.IsZero();

	// 입력이 있으면 가속 
	if (bHasInput)
	{
		FVector AccelerationVec = Input * MaxAcceleration;

		CurrentVelocity += AccelerationVec * DeltaSecond;

		if (CurrentVelocity.Size() > MaxWalkSpeed)
		{
			CurrentVelocity = CurrentVelocity.GetNormalized() * MaxWalkSpeed;
		}
	}

	// 입력이 없으면 감속 
	else
	{
		float CurrentSpeed = CurrentVelocity.Size();
		if (CurrentSpeed > 0.0f)
		{
			float DropAmount = BrackingDecel * DeltaSecond;

			//속도가 0 아래로 내려가지 않도록 방어처리 
			float NewSpeed = FMath::Max(CurrentSpeed -  DropAmount, 0.0f);
			
			CurrentVelocity = CurrentVelocity.GetNormalized() * NewSpeed;
		}  
	}

	Velocity.X = CurrentVelocity.X;
	Velocity.Y = CurrentVelocity.Y;
}

bool UCharacterMovementComponent::SafeMoveUpdatedComponent(const FVector& Delta, FHitResult& OutHit)
{
	OutHit.Reset();

	if (!UpdatedComponent || Delta.IsZero())
		return true;

	FPhysScene* PhysScene = GetPhysScene();
	if (!PhysScene)
	{
		// PhysScene이 없으면 그냥 이동
		UE_LOG("[CharacterMovement] PhysScene is null - moving without sweep");
		UpdatedComponent->AddRelativeLocation(Delta);
		return true;
	}

	float Radius, HalfHeight;
	GetCapsuleSize(Radius, HalfHeight);

	FVector Start = UpdatedComponent->GetWorldLocation();
	FVector End = Start + Delta;

	// 자기 자신은 무시
	AActor* OwnerActor = CharacterOwner;

	bool bHit = PhysScene->SweepCapsule(Start, End, Radius, HalfHeight, OutHit, OwnerActor);

	// 디버깅 로그
	if (bHit)
	{
		// UE_LOG("[CharacterMovement] Sweep HIT! Distance: %.3f, Normal: (%.2f, %.2f, %.2f)",
		// 	OutHit.Distance, OutHit.ImpactNormal.X, OutHit.ImpactNormal.Y, OutHit.ImpactNormal.Z);
	}

	if (bHit && OutHit.bBlockingHit)
	{
		// 충돌 지점 직전까지만 이동 (약간의 여유 거리)
		const float SkinWidth = 0.00125f;
		float SafeDistance = FMath::Max(0.0f, OutHit.Distance - SkinWidth);
		FVector SafeLocation = Start + Delta.GetNormalized() * SafeDistance;
		UpdatedComponent->SetWorldLocation(SafeLocation);

		// 다른 캐릭터와 충돌 시 밀어내기
		if (OutHit.HitActor)
		{
			if (ACharacter* OtherCharacter = Cast<ACharacter>(OutHit.HitActor))
			{
				// 밀어내기 방향 (내 이동 방향)
				FVector PushDirection = Delta.GetNormalized();
				PushDirection.Z = 0.0f; // 수평으로만 밀기
				PushDirection = PushDirection.GetNormalized();

				// 침투 깊이만큼 상대방 밀어내기
				float PushDistance = FMath::Max(0.0f, Delta.Size() - SafeDistance);
				FVector PushVector = PushDirection * PushDistance;

				if (USceneComponent* OtherRoot = OtherCharacter->GetRootComponent())
				{
					OtherRoot->AddWorldOffset(PushVector);
				}
			}
		}

		return false;
	}
	else
	{
		// 충돌 없음 - 전체 이동
		UpdatedComponent->AddRelativeLocation(Delta);
		return true;
	}
}

FVector UCharacterMovementComponent::SlideAlongSurface(const FVector& Delta, const FHitResult& Hit)
{
	if (!Hit.bBlockingHit)
		return Delta;

	// 충돌 노말에 수직인 방향으로 슬라이딩
	FVector Normal = Hit.ImpactNormal;

	// Delta에서 노말 방향 성분을 제거
	float DotProduct = FVector::Dot(Delta, Normal);
	FVector SlideVector = Delta - Normal * DotProduct;

	return SlideVector;
}

bool UCharacterMovementComponent::CheckFloor(FHitResult& OutHit)
{
	OutHit.Reset();

	if (!UpdatedComponent)
		return false;

	FPhysScene* PhysScene = GetPhysScene();
	if (!PhysScene)
	{
		// PhysScene이 없으면 임시로 Z=0을 바닥으로 취급
		UE_LOG("[CharacterMovement] CheckFloor: PhysScene is null, using Z=0 fallback");
		return UpdatedComponent->GetWorldLocation().Z <= 0.001f;
	}

	float Radius, HalfHeight;
	GetCapsuleSize(Radius, HalfHeight);

	FVector Start = UpdatedComponent->GetWorldLocation();
	// 바닥 검사는 캡슐 바닥에서 약간 아래로 Sweep
	const float FloorCheckDistance = 0.1f;  // 검사 거리 증가
	FVector End = Start - FVector(0, 0, FloorCheckDistance);

	AActor* OwnerActor = CharacterOwner;

	bool bHit = PhysScene->SweepCapsule(Start, End, Radius, HalfHeight, OutHit, OwnerActor);

	// 디버깅 로그
	/*UE_LOG("[CharacterMovement] CheckFloor: Capsule(R=%.2f, H=%.2f), Start=(%.2f, %.2f, %.2f), Hit=%s",
		Radius, HalfHeight, Start.X, Start.Y, Start.Z, bHit ? "YES" : "NO");*/

	// 바닥으로 인정하려면 노말이 위를 향해야 함
	if (bHit && OutHit.ImpactNormal.Z > 0.7f)
	{
		return true;
	}

	return false;
}

void UCharacterMovementComponent::GetCapsuleSize(float& OutRadius, float& OutHalfHeight) const
{
	// 기본값
	OutRadius = 0.5f;
	OutHalfHeight = 1.0f;

	if (CharacterOwner)
	{
		if (UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent())
		{
			// 월드 스케일 적용 (디버그 렌더링과 일치시키기 위함)
			const FTransform WorldTransform = Capsule->GetWorldTransform();
			const float AbsScaleX = std::fabs(WorldTransform.Scale3D.X);
			const float AbsScaleY = std::fabs(WorldTransform.Scale3D.Y);
			const float AbsScaleZ = std::fabs(WorldTransform.Scale3D.Z);

			OutRadius = Capsule->CapsuleRadius * FMath::Max(AbsScaleX, AbsScaleY);
			OutHalfHeight = Capsule->CapsuleHalfHeight * AbsScaleZ;
		}
	}
}

FPhysScene* UCharacterMovementComponent::GetPhysScene() const
{
	if (CharacterOwner)
	{
		if (UWorld* World = CharacterOwner->GetWorld())
		{
			return World->GetPhysScene();
		}
	}
	return nullptr;
}

void UCharacterMovementComponent::LaunchCharacter(FVector LaunchVelocity, bool bXYOverride, bool bZOverride)
{
	if (bXYOverride)
	{
		PhysicsVelocity.X = LaunchVelocity.X;
		PhysicsVelocity.Y = LaunchVelocity.Y;
	}
	else
	{
		PhysicsVelocity.X += LaunchVelocity.X; 
		PhysicsVelocity.Y += LaunchVelocity.Y; 
	}

	if (bZOverride)
	{
		PhysicsVelocity.Z = LaunchVelocity.Z;
	}
	else
	{
		PhysicsVelocity.Z += LaunchVelocity.Z;
	}

	bIsApplyKnockback = true;
	CharacterOwner->SetCurrentState(ECharacterState::Knockback);
}

void UCharacterMovementComponent::ApplyKnockback(float DeltaTime)
{
	if (DeltaTime <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	FHitResult OutHit;
	bool bIsGrounded = CheckFloor(OutHit);

	if (PhysicsVelocity.SizeSquared() < 10.0f && bIsGrounded)
	{
		PhysicsVelocity = FVector::Zero();
		bIsApplyKnockback = false;
		if (AAngryCoachCharacter* Character = Cast<AAngryCoachCharacter>(CharacterOwner))
		{
			if(Character->IsGuard())
			{
				CharacterOwner->SetCurrentState(ECharacterState::Guard);
			}
		}
		CharacterOwner->SetCurrentState(ECharacterState::Idle);
		return;
	}

	if (!bIsGrounded)
	{
		PhysicsVelocity.Z += GLOBAL_GRAVITY_Z * DeltaTime;
	}
	else
	{
		PhysicsVelocity.Z = 0.0f;
		// 마찰 (선형 근사)
		float FrictionFactor = FMath::Max(0.01f, 1.0f - (GroundFriction * DeltaTime));
		PhysicsVelocity.X *= FrictionFactor;
		PhysicsVelocity.Y *= FrictionFactor;
	}

	float RemainingTime = DeltaTime;
	int32 LoopCount = 0;

	while (RemainingTime > KINDA_SMALL_NUMBER && LoopCount < 2)
	{
		LoopCount++;
		FVector DeltaMove = PhysicsVelocity * RemainingTime;
		if (DeltaMove.IsZero())
		{
			break;
		}
		
		SafeMoveUpdatedComponent(DeltaMove, OutHit);

		if (OutHit.bBlockingHit)
		{
			float Dot = FVector::Dot(PhysicsVelocity, OutHit.ImpactNormal);

			if (Dot < KINDA_SMALL_NUMBER)
			{
				PhysicsVelocity = PhysicsVelocity - (OutHit.ImpactNormal * Dot);
				if (OutHit.ImpactNormal.Z > 0.7f)
				{
					bIsGrounded = true;					
					PhysicsVelocity.Z = FMath::Max(0.0f, PhysicsVelocity.Z);
				}
			}
			float TimeConsumed = RemainingTime * OutHit.Time;
			RemainingTime -= TimeConsumed;
		}
		else
		{
			break;
		}
	}
}

void UCharacterMovementComponent::SetForcedMovement(const FVector& InVelocity, float Duration)
{
	bForceMovement = true;
	ForcedVelocity = InVelocity;
	ForcedMovementDuration = Duration;
	ForcedMovementTimer = 0.0f;
}

void UCharacterMovementComponent::ClearForcedMovement()
{
	bForceMovement = false;
	ForcedVelocity = FVector::Zero();
	ForcedMovementDuration = 0.0f;
	ForcedMovementTimer = 0.0f;
}
