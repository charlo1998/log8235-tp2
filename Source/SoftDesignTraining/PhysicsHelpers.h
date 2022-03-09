// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

//Forward
class UWorld;

/**
 * 
 */
class SOFTDESIGNTRAINING_API PhysicsHelpers
{
public:
	PhysicsHelpers( UWorld* world );
	~PhysicsHelpers();

    //Used to specify if a raycast has to look for deathfloors
    enum RayCastChannel { default, deathfloor };

    bool CastRay( const FVector& start,const FVector& end, TArray<struct FHitResult>& outHits, bool drawDebug, RayCastChannel channel);
    bool SphereCast(const FVector& start,const FVector& end, float radius, TArray<struct FHitResult>& outHits, bool drawDebug, RayCastChannel channel);
    bool SphereOverlap( const FVector& pos, float radius, TArray<struct FOverlapResult>& outOverlaps, bool drawdebug, RayCastChannel channel);



protected:

    void DebugDrawHitPoint(const FHitResult& hit );
    void DebugDrawPrimitive(const UPrimitiveComponent& primitive );

    UWorld* m_world;
};
