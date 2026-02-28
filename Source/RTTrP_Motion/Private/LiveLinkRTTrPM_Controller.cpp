// Fill out your copyright notice in the Description page of Project Settings.


#include "LiveLinkRTTrPM_Controller.h"

#include "LiveLinkRTTrPM_Role.h"

void ULiveLinkRTTrPM_Controller::Tick(float DeltaTime, const FLiveLinkSubjectFrameData& SubjectData)
{
	FLiveLinkRTTrPM_FrameData FrameData = *SubjectData.FrameData.Cast<FLiveLinkRTTrPM_FrameData>();
	switch (SubjectType)
	{
	case ERTTrPM_SubjectType::Centroid:
		FrameData.Transform.SetTranslation(FrameData.CentroidPosition);
		break;
	case ERTTrPM_SubjectType::LED1:
		FrameData.Transform.SetTranslation(FrameData.LED1Position);
		break;
	case ERTTrPM_SubjectType::LED2:
		FrameData.Transform.SetTranslation(FrameData.LED2Position);
		break;
	case ERTTrPM_SubjectType::LED3:
		FrameData.Transform.SetTranslation(FrameData.LED3Position);
		break;
	default:
		break;
	}
	FLiveLinkSubjectFrameData ModifiedSubjectData;
	ModifiedSubjectData.StaticData.InitializeWith(SubjectData.StaticData);
	ModifiedSubjectData.FrameData = FLiveLinkFrameDataStruct(FLiveLinkRTTrPM_FrameData::StaticStruct(), &FrameData);

	Super::Tick(DeltaTime, ModifiedSubjectData);
}

bool ULiveLinkRTTrPM_Controller::IsRoleSupported(const TSubclassOf<ULiveLinkRole>& RoleToSupport)
{
	return RoleToSupport == ULiveLinkRTTrPM_Role::StaticClass();
}
