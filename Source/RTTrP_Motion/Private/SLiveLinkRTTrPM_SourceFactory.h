// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "LiveLinkRTTrPM_Connection.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

#if WITH_EDITOR
#include "IStructureDetailsView.h"
#endif //WITH_EDITOR

#include "Input/Reply.h"

struct FLiveLinkRTTrPM_ConnectionSettings;

DECLARE_DELEGATE_OneParam(FOnLiveLinkRTTrPMConnectionSettingsAccepted, FLiveLinkRTTrPM_ConnectionSettings);

class SLiveLinkRTTrPMSourceFactory : public SCompoundWidget
{
    SLATE_BEGIN_ARGS(SLiveLinkRTTrPMSourceFactory)
    {}
        SLATE_EVENT(FOnLiveLinkRTTrPMConnectionSettingsAccepted, OnConnectionSettingsAccepted)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args);

private:
    FLiveLinkRTTrPM_ConnectionSettings ConnectionSettings;

#if WITH_EDITOR
    TSharedPtr<FStructOnScope> StructOnScope;
    TSharedPtr<IStructureDetailsView> StructureDetailsView;
#endif //WITH_EDITOR

    FReply OnSettingsAccepted();
    FOnLiveLinkRTTrPMConnectionSettingsAccepted OnConnectionSettingsAccepted;
};
