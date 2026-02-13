// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkRTTrPM_SourceFactory.h"
#include "LiveLinkRTTrPM_Source.h"
#include "SLiveLinkRTTrPM_SourceFactory.h"


#define LOCTEXT_NAMESPACE "LiveLinkRTTrPM_SourceFactory"

FText ULiveLinkRTTrPM_SourceFactory::GetSourceDisplayName() const
{
	return LOCTEXT("SourceDisplayName", "LiveLinkRTTrPM_ Source");	
}

FText ULiveLinkRTTrPM_SourceFactory::GetSourceTooltip() const
{
	return LOCTEXT("SourceTooltip", "Allows creation of multiple LiveLink sources using the RTTrPM_ tracking system");
}

TSharedPtr<SWidget> ULiveLinkRTTrPM_SourceFactory::BuildCreationPanel(FOnLiveLinkSourceCreated InOnLiveLinkSourceCreated) const
{
	return SNew(SLiveLinkRTTrPM_SourceFactory)
		.OnConnectionSettingsAccepted(FOnLiveLinkRTTrPM_ConnectionSettingsAccepted::CreateUObject(this, &ULiveLinkRTTrPM_SourceFactory::CreateSourceFromSettings, InOnLiveLinkSourceCreated));
}

TSharedPtr<ILiveLinkSource> ULiveLinkRTTrPM_SourceFactory::CreateSource(const FString& ConnectionString) const
{
	FLiveLinkRTTrPM_ConnectionSettings ConnectionSettings;
	if (!ConnectionString.IsEmpty())
	{
		FLiveLinkRTTrPM_ConnectionSettings::StaticStruct()->ImportText(*ConnectionString, &ConnectionSettings, nullptr, PPF_None, GLog, TEXT("ULiveLinkRTTrPM_SourceFactory"));
	}
	return MakeShared<FLiveLinkRTTrPM_Source>(ConnectionSettings);
}

void ULiveLinkRTTrPM_SourceFactory::CreateSourceFromSettings(FLiveLinkRTTrPM_ConnectionSettings InConnectionSettings, FOnLiveLinkSourceCreated OnSourceCreated) const
{
	FString ConnectionString;
	FLiveLinkRTTrPM_ConnectionSettings::StaticStruct()->ExportText(ConnectionString, &InConnectionSettings, nullptr, nullptr, PPF_None, nullptr);

	TSharedPtr<FLiveLinkRTTrPM_Source> SharedPtr = MakeShared<FLiveLinkRTTrPM_Source>(InConnectionSettings);
	OnSourceCreated.ExecuteIfBound(SharedPtr, MoveTemp(ConnectionString));
}

#undef LOCTEXT_NAMESPACE
