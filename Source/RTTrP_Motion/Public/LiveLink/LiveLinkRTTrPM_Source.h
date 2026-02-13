// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "ILiveLinkSource.h"
#include "ILiveLinkClient.h"
#include "Roles/LiveLinkTransformTypes.h"
#include "HAL/Runnable.h"
#include "Common/UDPSocketReceiver.h"

#include "RTTrP_types.h"
#include "LiveLinkRTTrPM_Connection.h"
#include "LiveLinkRTTrPM_SourceSettings.h"

class FUdpSocketReceiver;
class FSocket;

class RTTRP_MOTION_API FLiveLinkRTTrPM_Source : public ILiveLinkSource, public FRunnable, public TSharedFromThis<FLiveLinkRTTrPM_Source>
{
public:

	FLiveLinkRTTrPM_Source(const FLiveLinkRTTrPM_ConnectionSettings& ConnectionSettings);
	virtual ~FLiveLinkRTTrPM_Source();
	// Inherited via ILiveLinkSource
	void ReceiveClient(ILiveLinkClient* InClient, FGuid InSourceGuid) override;
	bool IsSourceStillValid() const override;
	bool RequestSourceShutdown() override;
	FText GetSourceType() const override;
	FText GetSourceMachineName() const override;
	FText GetSourceStatus() const override;
	virtual TSubclassOf<ULiveLinkSourceSettings> GetSettingsClass() const override { return ULiveLinkRTTrPM_SourceSettings::StaticClass(); }
	// End ILiveLinkSource Interface

	// Inherited via FRunnable

	virtual uint32 Run() override;
	void Start();
	virtual void Stop() override;
	// LiveLink client
	ILiveLinkClient* Client = nullptr;
	FGuid SourceGuid;

	// Source display info
	FText SourceType;
	FText SourceMachineName;
	FText SourceStatus;

	// UDP receiver
	FSocket* Socket = nullptr;
	FUdpSocketReceiver* UDPReceiver = nullptr;
	bool bIsConnected = false;

	// Connection settings
	FLiveLinkRTTrPM_ConnectionSettings ConnectionSettings;

	// Track subjects we've registered
	TSet<FName> EncounteredSubjects;

	void SendTrackable(const FRTTrPM_Trackable& Trackable);

private:
	void OnPacketReceived(const FArrayReaderPtr& Data, const FIPv4Endpoint& Endpoint);

};
