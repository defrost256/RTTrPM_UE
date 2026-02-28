// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <atomic>

#include "ILiveLinkSource.h"
#include "ILiveLinkClient.h"
#include "Roles/LiveLinkTransformTypes.h"
#include "HAL/Runnable.h"
#include "Common/UDPSocketReceiver.h"

#include "RTTrP_types.h"
#include "LiveLinkRTTrPM_Connection.h"
//#include "LiveLinkRTTrPM_SourceSettings.h"

class FUdpSocketReceiver;
class FSocket;

enum class ELiveLinkRTTrPMState : uint8
{
	NotStarted = 0,
	EndpointsReady,
	Receiving,
	ResetRequested,
	ShutDown,
};

class RTTRP_MOTION_API FLiveLinkRTTrPM_Source : public ILiveLinkSource, public TSharedFromThis<FLiveLinkRTTrPM_Source>
{
public:

	FLiveLinkRTTrPM_Source(const FLiveLinkRTTrPM_ConnectionSettings& ConnectionSettings);
	virtual ~FLiveLinkRTTrPM_Source();
	// Inherited via ILiveLinkSource
	void ReceiveClient(ILiveLinkClient* InClient, FGuid InSourceGuid) override;
	virtual void OnSettingsChanged(ULiveLinkSourceSettings* Settings, const FPropertyChangedEvent& PropertyChangedEvent) override;
	bool IsSourceStillValid() const override;
	bool RequestSourceShutdown() override;
	FText GetSourceType() const override;
	FText GetSourceMachineName() const override;
	FText GetSourceStatus() const override;
	//virtual void InitializeSettings(ULiveLinkSourceSettings* Settings) override;
	virtual void Update() override;
	//virtual TSubclassOf<ULiveLinkSourceSettings> GetSettingsClass() const override { return ULiveLinkRTTrPM_SourceSettings::StaticClass(); }
	// End ILiveLinkSource Interface

	// LiveLink client
	ILiveLinkClient* Client = nullptr;
	FGuid SourceGuid;

	// Source display info
	FText SourceType;
	FText SourceMachineName;
	FText SourceStatus;

	// UDP receiver
	FSocket* Socket = nullptr;
	TUniquePtr<FUdpSocketReceiver> UDPReceiver = nullptr;
	bool bIsConnected = false;
	ELiveLinkRTTrPMState ConnectionState = ELiveLinkRTTrPMState::NotStarted;
	std::atomic<double> LastDataReadTime = 0;
	bool bShutdownRequested = false;
	bool bResetRequested = false;

	// Connection settings
	FLiveLinkRTTrPM_ConnectionSettings ConnectionSettings;
	//ULiveLinkRTTrPM_SourceSettings* mSettings = nullptr;

	// Track subjects we've registered
	TSet<FName> EncounteredSubjects;

	void SendTrackable(const RTTrPM_Trackable& Trackable);

private:
	bool OpenSocket();
	void StopUdpReceiver();
	void CloseSocket();
	bool JoinMulticastGroup(const FIPv4Address& UnicastAddress, const FIPv4Address& MulticastAddress);

	void OnPacketReceived(const FArrayReaderPtr& Data, const FIPv4Endpoint& Endpoint);

};
