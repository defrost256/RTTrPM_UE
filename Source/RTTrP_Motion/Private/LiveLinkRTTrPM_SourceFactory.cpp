// Fill out your copyright notice in the Description page of Project Settings.


#include "LiveLinkRTTrPM_SourceFactory.h"
#include "SLiveLinkRTTrPM_SourceFactory.h"
#include "LiveLinkRTTrPM_Source.h"

#define LOCTEXT_NAMESPACE "LiveLinkRTTrPMSourceFactory"

FText ULiveLinkRTTrPM_SourceFactory::GetSourceDisplayName() const
{
    return LOCTEXT("SourceDisplayName", "LiveLink RTTrPM Source");
}

FText ULiveLinkRTTrPM_SourceFactory::GetSourceTooltip() const
{
    return LOCTEXT("SourceTooltip", "Allows creation of multiple LiveLink sources from RTTrPM UDP packets");
}

TSharedPtr<SWidget> ULiveLinkRTTrPM_SourceFactory::BuildCreationPanel(FOnLiveLinkSourceCreated InOnLiveLinkSourceCreated) const
{
    return SNew(SLiveLinkRTTrPMSourceFactory)
        .OnConnectionSettingsAccepted(FOnLiveLinkRTTrPMConnectionSettingsAccepted::CreateUObject(this, &ULiveLinkRTTrPM_SourceFactory::CreateSourceFromSettings, InOnLiveLinkSourceCreated));
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
