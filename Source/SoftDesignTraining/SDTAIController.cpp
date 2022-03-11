// Fill out your copyright notice in the Description page of Project Settings.

#include "SDTAIController.h"
#include "SoftDesignTraining.h"
#include "SoftDesignTrainingMainCharacter.h"
#include "SDTCollectible.h"
#include "SDTFleeLocation.h"
#include "SDTPathFollowingComponent.h"
#include "DrawDebugHelpers.h"
#include "Kismet/KismetMathLibrary.h"
#include "NavigationSystem.h"
//#include "UnrealMathUtility.h"
#include "SDTUtils.h"
#include "EngineUtils.h"


ASDTAIController::ASDTAIController(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer.SetDefaultSubobjectClass<USDTPathFollowingComponent>(TEXT("PathFollowingComponent")))
{

}

FVector ASDTAIController::FindFleeLocation(APawn* selfPawn, bool &found, FVector sphereLocation)
{
    FVector location = selfPawn->GetActorLocation();

    TArray<AActor*> fleeLocations;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASDTFleeLocation::StaticClass(), fleeLocations);
    for (const AActor* fleeLocation : fleeLocations)
    {
        if ((fleeLocation->GetActorLocation() - sphereLocation).Size() < fleeSphereRadius)
        {
            found = true;
            location = fleeLocation->GetActorLocation();
        }
    }
    return location;
}

void ASDTAIController::GoToBestTarget(float deltaTime)
{

    //TODO: Consider nav links in part 5
    UNavigationSystemV1* navSys = UNavigationSystemV1::GetCurrent(GetWorld());
    UNavigationPath* path = navSys->FindPathToLocationSynchronously(GetWorld(), GetPawn()->GetActorLocation(), target);

    //Trying to send the path to PathFollowingComponenent
    GetPathFollowingComponent()->RequestMove(FAIMoveRequest(GetPawn()), path->GetPath());

    MoveCharacter(path);
    ShowNavigationPath();


    USDTPathFollowingComponent* pathFol = dynamic_cast<USDTPathFollowingComponent*>(GetPathFollowingComponent());
    pathFol->FollowPathSegment(deltaTime);
}

void ASDTAIController::OnMoveToTarget()
{
    m_ReachedTarget = false;
}

void ASDTAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
    Super::OnMoveCompleted(RequestID, Result);

    m_ReachedTarget = true;
}

void ASDTAIController::ShowNavigationPath()
{
    //TArray<FNavPathPoint>& path = GetPathFollowingComponent()->GetPath()->GetPathPoints();
    //if (path.Num() > 0)
    //{
    //    FVector previousNode = GetPawn()->GetActorLocation();
    //    for (int pointiter = 0; pointiter < path.Num(); pointiter++)
    //    {
    //        DrawDebugSphere(GetWorld(), path[pointiter], 30.0f, 12, FColor(255, 0, 0));
    //        DrawDebugLine(GetWorld(), previousNode, path[pointiter], FColor(255, 0, 0));
    //        previousNode = path[pointiter];
    //    }
    //}
}

void ASDTAIController::ChooseBehavior(float deltaTime)
{
    //add states here? possible states: pursuing, fleeing, collecting collectible, default
    UpdatePlayerInteraction(deltaTime);
}

void ASDTAIController::UpdatePlayerInteraction(float deltaTime)
{
    //finish jump before updating AI state
    if (AtJumpSegment)
        return;

    APawn* selfPawn = GetPawn();
    if (!selfPawn)
        return;

    ACharacter* playerCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
    ASoftDesignTrainingMainCharacter* mainCharacter = dynamic_cast<ASoftDesignTrainingMainCharacter*>(playerCharacter);
    if (!playerCharacter)
        return;

    FVector detectionStartLocation = selfPawn->GetActorLocation() + selfPawn->GetActorForwardVector() * m_DetectionCapsuleForwardStartingOffset;
    FVector detectionEndLocation = detectionStartLocation + selfPawn->GetActorForwardVector() * m_DetectionCapsuleHalfLength * 2;

    TArray<TEnumAsByte<EObjectTypeQuery>> detectionTraceObjectTypes;
    detectionTraceObjectTypes.Add(UEngineTypes::ConvertToObjectType(COLLISION_COLLECTIBLE));
    detectionTraceObjectTypes.Add(UEngineTypes::ConvertToObjectType(COLLISION_PLAYER));

    TArray<FHitResult> allDetectionHits;
    GetWorld()->SweepMultiByObjectType(allDetectionHits, detectionStartLocation, detectionEndLocation, FQuat::Identity, detectionTraceObjectTypes, FCollisionShape::MakeSphere(m_DetectionCapsuleRadius));

    FHitResult detectionHit;
    FVector sphereLocation;
    bool hit = GetHightestPriorityDetectionHit(allDetectionHits, detectionHit);
    if (hit) //if a player is seen, update behavior
    {
        
        bool fleeLocationDetected = false;
        //found a player, check if powered up and adapt
        if (mainCharacter->IsPoweredUp())
        {
            m_Pursuing = false;
            m_Fleeing = true;
            //check for a flee location in a sphere behind agent, then compute path
            sphereLocation = selfPawn->GetActorLocation() - selfPawn->GetActorForwardVector() * OffSet;
            DrawDebugSphere(GetWorld(), sphereLocation, fleeSphereRadius, 100, FColor::Red);
            target = FindFleeLocation(selfPawn, fleeLocationDetected, sphereLocation);
            

            if (!fleeLocationDetected) //didn't find a flee location behind him, looking slightly to the left
            {
                sphereLocation = selfPawn->GetActorLocation() - selfPawn->GetActorRightVector() * OffSet;
                DrawDebugSphere(GetWorld(), sphereLocation, fleeSphereRadius, 100, FColor::Red);
                target = FindFleeLocation(selfPawn, fleeLocationDetected, sphereLocation);
            }
        }
        else
        {
            //compute path to player and go there
            target = playerCharacter->GetActorLocation();
            m_Pursuing = true;
            m_Fleeing = false;
        }
    }
    else if (!m_Fleeing && !m_Pursuing) //if no player is seen and agent is not fleeing or pursuing the last location, find the closest collectible
    {
        target = FindClosestCollectible()->GetActorLocation();
    }

    if ((selfPawn->GetActorLocation() - target).Size() <= 10.f) //if agent reached its target, go back to default state
    {
        m_Pursuing = false;
        m_Fleeing = false;
    }

    DrawDebugCapsule(GetWorld(), detectionStartLocation + m_DetectionCapsuleHalfLength * selfPawn->GetActorForwardVector(), m_DetectionCapsuleHalfLength, m_DetectionCapsuleRadius, selfPawn->GetActorQuat() * selfPawn->GetActorUpVector().ToOrientationQuat(), FColor::Blue);
}

 bool ASDTAIController::GetHightestPriorityDetectionHit(const TArray<FHitResult>& hits, FHitResult& outDetectionHit)
{
    bool out = false;
    for (const FHitResult& hit : hits)
    {
        if (UPrimitiveComponent* component = hit.GetComponent())
        {
            if (component->GetCollisionObjectType() == COLLISION_PLAYER)
            {
                //we can't get more important than the player
                outDetectionHit = hit;
                return true;
            }
            //else if (component->GetCollisionObjectType() == COLLISION_COLLECTIBLE) //not needed, since we don't use vision to find collectibles anymore
            //{

            //    outDetectionHit = hit;
            //    out = true;
            //}
        }
    }
    return out;
}

void ASDTAIController::AIStateInterrupted()
{
    StopMovement();
    m_ReachedTarget = true;
}

ASDTCollectible* ASDTAIController::FindClosestCollectible()
{
    FVector chrLocation = GetPawn()->GetActorLocation();

    ASDTCollectible* closestCollectible = NULL;
    float collectibleDistance = 0;

    for (TActorIterator<AActor> actor(GetWorld()); actor; ++actor)
    {
        if (ASDTCollectible* collectible = dynamic_cast<ASDTCollectible*>(*actor))
        {
            if (!collectible->IsOnCooldown())
            {
                float distance = FVector::Dist(chrLocation, collectible->GetActorLocation());
                if (closestCollectible == NULL)
                {
                    closestCollectible = collectible;
                    collectibleDistance = distance;
                }
                else if (distance < collectibleDistance)
                {
                    closestCollectible = collectible;
                    collectibleDistance = distance;
                }
            }
                
        }
    }

    return closestCollectible;
}

//Temporary function to move the character
void ASDTAIController::MoveCharacter(UNavigationPath* path)
{
    FVector direction;

    if (path->PathPoints.Num() < 3 || FVector::Distance(path->PathPoints[1], path->PathPoints[0]) > 95.0f)
        direction = path->PathPoints[1] - GetPawn()->GetActorLocation();
    else
        direction = path->PathPoints[2] - GetPawn()->GetActorLocation();

    direction.Normalize();

    if (direction.Size() < 1)
    {
        direction /= direction.Size();
    }


    FVector newLoc = GetPawn()->GetActorLocation() + (direction * 2.5f);

    GetPawn()->SetActorLocation(newLoc);
}