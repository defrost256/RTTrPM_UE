// Fill out your copyright notice in the Description page of Project Settings.


#include "LiveLinkRTTrPM_Role.h"
#include "LiveLinkRTTrPM_DataTypes.h"

#define LOCTEXT_NAMESPACE "RTTrPM_LiveLink"

UScriptStruct* ULiveLinkRTTrPM_Role::GetStaticDataStruct() const
{
	return FLiveLinkRTTrPM_StaticData::StaticStruct();
}

UScriptStruct* ULiveLinkRTTrPM_Role::GetFrameDataStruct() const
{
	return FLiveLinkRTTrPM_FrameData::StaticStruct();
}

UScriptStruct* ULiveLinkRTTrPM_Role::GetBlueprintDataStruct() const
{
	return FLiveLinkRTTrPM_BlueprintData::StaticStruct();
}

bool ULiveLinkRTTrPM_Role::InitializeBlueprintData(const FLiveLinkSubjectFrameData& InSourceData, FLiveLinkBlueprintDataStruct& OutBlueprintData) const
{
	bool bSuccess = false;

	FLiveLinkRTTrPM_BlueprintData* BlueprintData = OutBlueprintData.Cast<FLiveLinkRTTrPM_BlueprintData>();
	const FLiveLinkRTTrPM_StaticData* StaticData = InSourceData.StaticData.Cast<FLiveLinkRTTrPM_StaticData>();
	const FLiveLinkRTTrPM_FrameData* FrameData = InSourceData.FrameData.Cast<FLiveLinkRTTrPM_FrameData>();
	if (BlueprintData && StaticData && FrameData)
	{
		GetStaticDataStruct()->CopyScriptStruct(&BlueprintData->StaticData, StaticData);
		GetFrameDataStruct()->CopyScriptStruct(&BlueprintData->FrameData, FrameData);
		bSuccess = true;
	}
	return bSuccess;
}

FText ULiveLinkRTTrPM_Role::GetDisplayName() const
{
	return LOCTEXT("RTTrPM Role", "RTTrPM Trackable");
}

#undef LOCTEXT_NAMESPACE
