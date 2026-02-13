// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkRTTrPM_ConnectionSettings.h"
#include "Widgets/SCompoundWidget.h"

class FStructOnScope;
class IStructureDetailsView;

#if WITH_EDITOR
#endif //WITH_EDITOR


struct FLiveLinkRTTrPM_ConnectionSettings;

DECLARE_DELEGATE_OneParam(FOnLiveLinkRTTrPM_ConnectionSettingsAccepted, FLiveLinkRTTrPM_ConnectionSettings);

class SLiveLinkRTTrPM_SourceFactory : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SLiveLinkRTTrPM_SourceFactory)
	{}
		SLATE_EVENT(FOnLiveLinkRTTrPM_ConnectionSettingsAccepted, OnConnectionSettingsAccepted)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);


private:
	FLiveLinkRTTrPM_ConnectionSettings ConnectionSettings;

#if WITH_EDITOR
	TSharedPtr<FStructOnScope> StructOnScope;
	TSharedPtr<IStructureDetailsView> StructureDetailsView;
#endif //WITH_EDITOR

	FReply OnSettingsAccepted();
	FOnLiveLinkRTTrPM_ConnectionSettingsAccepted OnConnectionSettingsAccepted;
};
