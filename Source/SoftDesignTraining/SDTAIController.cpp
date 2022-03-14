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
#include "PhysicsHelpers.h"
#include <algorithm>
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
    DrawDebugSphere(GetWorld(), sphereLocation, fleeSphereRadius, 100, FColor::Red);
    return location;
}

//Part 2-3
void ASDTAIController::GoToBestTarget(float deltaTime)
{
    //The navigation system will find a path to the target using the nav mesh
    MoveToLocation(target);
    //Print path
    ShowNavigationPath();
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

//Part 2
//Starting at the NPC location, print all the nodes and segments of the path
void ASDTAIController::ShowNavigationPath()
{   
    FNavPathSharedPtr path = GetPathFollowingComponent()->GetPath();

    if(path)
    {
        TArray<FNavPathPoint>& pathPoints = path->GetPathPoints();
        FVector previousNode = GetPawn()->GetActorLocation();
        for (int pointiter = 0; pointiter < pathPoints.Num(); pointiter++)
        {
            DrawDebugSphere(GetWorld(), pathPoints[pointiter], 30.0f, 32, FColor(255, 0, 0));
            DrawDebugLine(GetWorld(), previousNode, pathPoints[pointiter], FColor(255, 0, 0));
            previousNode = pathPoints[pointiter];
        }
    }
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

    //collect objects in field of view
    FVector detectionStartLocation = selfPawn->GetActorLocation() + selfPawn->GetActorForwardVector() * m_DetectionCapsuleForwardStartingOffset;
    FVector detectionEndLocation = detectionStartLocation + selfPawn->GetActorForwardVector() * m_DetectionCapsuleHalfLength * 2;

    TArray<TEnumAsByte<EObjectTypeQuery>> detectionTraceObjectTypes;
    detectionTraceObjectTypes.Add(UEngineTypes::ConvertToObjectType(COLLISION_COLLECTIBLE));
    detectionTraceObjectTypes.Add(UEngineTypes::ConvertToObjectType(COLLISION_PLAYER));

    TArray<FHitResult> allDetectionHits;
    GetWorld()->SweepMultiByObjectType(allDetectionHits, detectionStartLocation, detectionEndLocation, FQuat::Identity, detectionTraceObjectTypes, FCollisionShape::MakeSphere(m_DetectionCapsuleRadius));

    FHitResult detectionHit;
    FVector sphereLocation;
    PhysicsHelpers physicHelper(GetWorld());
    bool hit = GetHightestPriorityDetectionHit(allDetectionHits, detectionHit);

    if (hit && IsVisibleAndReachable(selfPawn, mainCharacter, physicHelper, GetWorld())) //if a player is seen, update behavior
    {
        
        bool fleeLocationDetected = false;
        //found a player, check if powered up and adapt behavior
        if (mainCharacter->IsPoweredUp() && memory <= 0)
        {
            m_Pursuing = false;
            m_Fleeing = true;
            memory = 150; //continue fleeing for a minimum amount of time before checking for a better flee location

            //check for a flee location in a sphere behind agent, then compute path
            sphereLocation = selfPawn->GetActorLocation() - selfPawn->GetActorForwardVector() * OffSet;
            target = FindFleeLocation(selfPawn, fleeLocationDetected, sphereLocation);
            
            FVector lateralOffset(1000.f, 0.f, 0.f);
            if (!fleeLocationDetected) //didn't find a flee location behind him, looking slightly to the right
            {
                sphereLocation = selfPawn->GetActorLocation() - selfPawn->GetActorForwardVector() * OffSet + lateralOffset;
                target = FindFleeLocation(selfPawn, fleeLocationDetected, sphereLocation);
            }
            else if (!fleeLocationDetected)//didn't find a flee location to the right, looking slightly to the left
            {
                sphereLocation = selfPawn->GetActorLocation() - selfPawn->GetActorForwardVector() * OffSet - lateralOffset;
                target = FindFleeLocation(selfPawn, fleeLocationDetected, sphereLocation);
            }
        }
        else if (!mainCharacter->IsPoweredUp())
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

    if ((selfPawn->GetActorLocation() - target).Size() <= 200.f) //if agent reached its target, go back to default state
    {
        m_Pursuing = false;
        m_Fleeing = false;
    }

    memory = std::max(0, memory - 1);
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
                
                outDetectionHit = hit;
                return true;
            }
        }
    }
    return out;
}

 bool ASDTAIController::IsVisibleAndReachable(APawn* selfPawn, AActor* actor, PhysicsHelpers& physicHelper, UWorld* world) const
 {
     FVector selfPosition = selfPawn->GetActorLocation();
     FVector actorPosition = actor->GetActorLocation();

     FVector const toTarget = actorPosition - selfPosition;
     FVector const chrForward = selfPawn->GetActorForwardVector();

     // Cast a Ray from the character to the player and checks if there is anything in between
     TArray<FHitResult> hitResults;
     physicHelper.CastRay(selfPosition, actorPosition, hitResults, true, physicHelper.RayCastChannel::default);

     bool canReach = true;

     for (FHitResult hitResult : hitResults) {
         // If any object (different from the agent or the collectible) is in between, change to unreachable
         if (hitResult.GetActor() == actor || hitResult.GetActor() == selfPawn) continue;
         canReach = false;
         break;
     }

     return canReach;
 }

void ASDTAIController::AIStateInterrupted()
{
    StopMovement();
    m_ReachedTarget = true;
}
bool ASDTAIController::getReachedTarget()
{
    return ((GetPawn()->GetActorLocation() - target).Size() <= 200.f);
}

//Part 2
ASDTCollectible* ASDTAIController::FindClosestCollectible()
{
    FVector chrLocation = GetPawn()->GetActorLocation();

    ASDTCollectible* closestCollectible = NULL;
    float collectibleDistance = 0;
    
    //Get all the actors
    for (TActorIterator<AActor> actor(GetWorld()); actor; ++actor)
    {
        //If the actor is a collectible, check if it is the closest
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

//Part 5
bool ASDTAIController::Jump(FVector start, FVector end) {
    AtJumpSegment = true;

    FVector nextPosition= FVector( start.X + jumpingProgress * (end - start).X, start.Y + jumpingProgress * (end - start).Y, -2* JumpApexHeight * jumpingProgress * jumpingProgress + 2* JumpApexHeight* jumpingProgress + 216);
    jumpingProgress += 0.01f;
    if (jumpingProgress == 1)
    {
        jumpingProgress = 0;
        AtJumpSegment = false;
    }
    APawn* selfPawn = GetPawn();
    selfPawn->SetActorLocation(nextPosition);
    return AtJumpSegment;
}