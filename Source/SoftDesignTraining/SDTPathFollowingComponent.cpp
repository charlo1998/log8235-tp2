// Fill out your copyright notice in the Description page of Project Settings.

#include "SDTPathFollowingComponent.h"
#include "SoftDesignTraining.h"
#include "SDTUtils.h"
#include "SDTAIController.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "DrawDebugHelpers.h"

USDTPathFollowingComponent::USDTPathFollowingComponent(const FObjectInitializer& ObjectInitializer)
{

}

void USDTPathFollowingComponent::FollowPathSegment(float deltaTime)
{
    const TArray<FNavPathPoint>& points = Path->GetPathPoints();
    const FNavPathPoint& segmentStart = points[MoveSegmentStartIndex];
    if (isJumping) {

        //ASDTAIController* Controller = Cast<ASDTAIController>(Cast<APawn>(GetOwner())->Controller);
        isJumping= Cast<ASDTAIController>(GetOwner())->Jump(segmentStart, points[MoveSegmentStartIndex - 1]);
       }
    else
    {
        Super::FollowPathSegment(deltaTime);
        Speed = MovementComp->Velocity.Size();
        if (GEngine)
            GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Green, TEXT("no jump flag!"));
    }
}

void USDTPathFollowingComponent::SetMoveSegment(int32 segmentStartIndex)
{

    const TArray<FNavPathPoint>& points = Path->GetPathPoints();

    const FNavPathPoint& segmentStart = points[MoveSegmentStartIndex];

    if (GEngine)
        GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, FString::SanitizeFloat(segmentStart.Flags)); //81668

    if (segmentStart.Flags == 81668)
    {
        isJumping = true;
        Super::SetMoveSegment(segmentStartIndex);

    }
   else if (isJumping){
    }
   else {
        Super::SetMoveSegment(segmentStartIndex);

    }
    //if (SDTUtils::HasJumpFlag(segmentStart) && FNavMeshNodeFlags(segmentStart.Flags).IsNavLink())
    //{
    //    if (GEngine)
    //        GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, TEXT("Move Segment Set with jump flag!"));
    //}
    //else
    //{
    //    if (GEngine)
    //        GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Red, TEXT("Move Segment Set!"));
    //}
}

