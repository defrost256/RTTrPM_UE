// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LiveLinkSourceSettings.h"
#include "LiveLinkRTTrPM_DataTypes.h"
#include "LiveLinkRTTrPM_SourceSettings.generated.h"

UCLASS()
class RTTRP_MOTION_API ULiveLinkRTTrPM_SourceSettings : public ULiveLinkSourceSettings
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="LiveLink")
	ERTTrPM_SubjectType TransformSource;
	
};
