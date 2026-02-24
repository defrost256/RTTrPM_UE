// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Roles/LiveLinkTransformRole.h"
#include "LiveLinkRTTrPM_Role.generated.h"

/**
 * 
 */
UCLASS(BlueprintType, meta=(DisplayName="RTTrPM Transform Role"))
class RTTRP_MOTION_API ULiveLinkRTTrPM_Role : public ULiveLinkTransformRole
{
	GENERATED_BODY()
public:
	virtual UScriptStruct* GetStaticDataStruct() const override;
	virtual UScriptStruct* GetFrameDataStruct() const override;
	virtual UScriptStruct* GetBlueprintDataStruct() const override;

	bool InitializeBlueprintData(const FLiveLinkSubjectFrameData& InSourceData, FLiveLinkBlueprintDataStruct& OutBlueprintData) const override;

	virtual FText GetDisplayName() const override;
};
