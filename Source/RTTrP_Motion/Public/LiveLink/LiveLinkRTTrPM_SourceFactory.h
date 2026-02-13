// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LiveLinkSourceFactory.h"
#include "LiveLinkRTTrPM_Connection.h"
#include "LiveLinkRTTrPM_SourceFactory.generated.h"

/**
 * 
 */
UCLASS()
class RTTRP_MOTION_API ULiveLinkRTTrPM_SourceFactory : public ULiveLinkSourceFactory
{
	GENERATED_BODY()
public:
	virtual FText GetSourceDisplayName() const override;
	virtual FText GetSourceTooltip() const override;

	virtual EMenuType GetMenuType() const override { return EMenuType::SubPanel; }
	virtual TSharedPtr<SWidget> BuildCreationPanel(FOnLiveLinkSourceCreated OnLiveLinkSourceCreated) const override;
	virtual TSharedPtr<ILiveLinkSource> CreateSource(const FString& ConnectionString) const override;

private:
	void CreateSourceFromSettings(FLiveLinkRTTrPM_ConnectionSettings ConnectionSettings, FOnLiveLinkSourceCreated OnSourceCreated) const;
};
