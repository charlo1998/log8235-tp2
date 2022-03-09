// Fill out your copyright notice in the Description page of Project Settings.

#include "SDTAIController.h"
#include "SoftDesignTraining.h"
#include "SoftDesignTrainingMainCharacter.h"
#include "SDTCollectible.h"
#include "SDTFleeLocation.h"
#include "SDTPathFollowingComponent.h"
#include "DrawDebugHelpers.h"
#include "Kismet/KismetMathLibrary.h"
//#include "UnrealMathUtility.h"
#include "SDTUtils.h"
#include "EngineUtils.h"

//TArray<FOverlapResult> ASDTAIController::CollectTargetActorsInFrontOfCharacter(ASoftDesignTrainingCharacter const* chr, PhysicsHelpers& physicHelper, float frwd) const
//{
//    TArray<FOverlapResult> outResults;
//    physicHelper.SphereOverlap(chr->GetActorLocation() + chr->GetActorForwardVector() * frwd, chr->2000F., outResults, true, physicHelper.RayCastChannel::default);
//    return outResults;
//}

ASDTAIController::ASDTAIController(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer.SetDefaultSubobjectClass<USDTPathFollowingComponent>(TEXT("PathFollowingComponent")))
{

}

FVector ASDTAIController::FindFleeLocation(APawn* selfPawn, bool &found, FVector sphereLocation)
{
    FVector target = selfPawn->GetActorLocation();

    TArray<AActor*> fleeLocations;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASDTFleeLocation::StaticClass(), fleeLocations);
    for (const AActor* fleeLocation : fleeLocations)
    {
        if ((fleeLocation->GetActorLocation() - sphereLocation).Size() < fleeSphereRadius)
        {
            found = true;
            target = fleeLocation->GetActorLocation();
        }
    }
    return target;
}

void ASDTAIController::GoToBestTarget(float deltaTime)
{
    //Move to target depending on current behavior
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
    //Show current navigation path DrawDebugLine and DrawDebugSphere
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
    if (hit) //if no player or collectible is seen, continue exploring
    {
        FVector target;
        bool fleeLocationDetected = false;
        //Set behavior based on hit
        if (detectionHit.GetComponent()->GetCollisionObjectType() == COLLISION_PLAYER)
        {
            //found a player, check if powered up and adapt
            if (mainCharacter->IsPoweredUp())
            {
                //check for a flee location in a sphere behind agent, then compute path
                sphereLocation = selfPawn->GetActorLocation() - selfPawn->GetActorForwardVector() * OffSet;
                DrawDebugSphere(GetWorld(), sphereLocation, fleeSphereRadius, 100, FColor::Red);
                target = FindFleeLocation(selfPawn, fleeLocationDetected, sphereLocation);

                if (!fleeLocationDetected) //didn't find a flee location behind him, looking slightly to the right
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
            }
        }
        else if (detectionHit.GetComponent()->GetCollisionObjectType() == COLLISION_COLLECTIBLE)
        {
            // go to collectible
        }
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
            else if (component->GetCollisionObjectType() == COLLISION_COLLECTIBLE)
            {
                outDetectionHit = hit;
                out = true;
            }
        }
    }
    return out;
}

void ASDTAIController::AIStateInterrupted()
{
    StopMovement();
    m_ReachedTarget = true;
}