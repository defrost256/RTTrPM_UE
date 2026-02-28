// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Controllers/LiveLinkTransformController.h"
#include "LiveLinkRole.h"

#include "LiveLinkRTTrPM_DataTypes.h"
#include "LiveLinkRTTrPM_Controller.generated.h"

/**
 * 
 */
UCLASS()
class RTTRP_MOTION_API ULiveLinkRTTrPM_Controller : public ULiveLinkTransformController
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LiveLink")
	ERTTrPM_SubjectType SubjectType = ERTTrPM_SubjectType::Centroid;

public:
	//~ Begin ULiveLinkControllerBase interface
	virtual void Tick(float DeltaTime, const FLiveLinkSubjectFrameData& SubjectData) override;
	virtual bool IsRoleSupported(const TSubclassOf<ULiveLinkRole>& RoleToSupport) override;
	//~ End ULiveLinkControllerBase interface
};
