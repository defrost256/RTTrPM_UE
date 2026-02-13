// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ILiveLinkSource.h"
#include "LiveLinkRTTrPM_ConnectionSettings.h"
#include "LiveLinkRTTrPM_SourceSettings.h"
#include "Roles/LiveLinkCameraTypes.h"

#include "Delegates/IDelegateInstance.h"
#include "MessageEndpoint.h"
#include "IMessageContext.h"
#include "HAL/ThreadSafeBool.h"
#include "HAL/Runnable.h"

#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"

#include "LiveLinkRTTrPM_PacketInfo.h"

struct ULiveLinkRTTrPM_Settings;

class ILiveLinkClient;

class RTTRP_MOTION_API FLiveLinkRTTrPM_Source : public ILiveLinkSource, public FRunnable, public TSharedFromThis<FLiveLinkRTTrPM_Source>
{
public:

	FLiveLinkRTTrPM_Source(const FLiveLinkRTTrPM_ConnectionSettings& ConnectionSettings);

	virtual ~FLiveLinkRTTrPM_Source();

	// Begin ILiveLinkSource Interface
	
	virtual void ReceiveClient(ILiveLinkClient* InClient, FGuid InSourceGuid) override;
	virtual void InitializeSettings(ULiveLinkSourceSettings* Settings) override;

	virtual bool IsSourceStillValid() const override;

	virtual bool RequestSourceShutdown() override;

	virtual FText GetSourceType() const override { return SourceType; };
	virtual FText GetSourceMachineName() const override { return SourceMachineName; }
	virtual FText GetSourceStatus() const override { return SourceStatus; }

	virtual TSubclassOf<ULiveLinkSourceSettings> GetSettingsClass() const override { return ULiveLinkRTTrPM_SourceSettings::StaticClass(); }
	virtual void OnSettingsChanged(ULiveLinkSourceSettings* Settings, const FPropertyChangedEvent& PropertyChangedEvent) override;

	// End ILiveLinkSource Interface

	// Begin FRunnable Interface

	virtual bool Init() override { return true; }
	virtual uint32 Run() override;
	void Start();
	virtual void Stop() override;
	virtual void Exit() override { }

	// End FRunnable Interface

private:
	ILiveLinkClient* Client;

	// Our identifier in LiveLink
	FGuid SourceGuid;

	FMessageAddress ConnectionAddress;

	FText SourceType;
	FText SourceMachineName;
	FText SourceStatus;
	
	// Threadsafe Bool for terminating the main thread loop
	FThreadSafeBool Stopping;
	
	// Thread to run socket operations on
	FRunnableThread* Thread;
	
	// Name of the sockets thread
	FString ThreadName;

	FSocket* Socket;
	ISocketSubsystem* SocketSubsystem;
	FIPv4Endpoint DeviceEndpoint;

	// Size of receive buffer
	const uint32 ReceiveBufferSize = 1024 * 16;

	// Receive buffer for UDP socket
	TArray<uint8> ReceiveBuffer;
	
	// List of subjects we've already encountered
	TSet<FName> EncounteredSubjects;

	// Deferred start delegate handle.
	FDelegateHandle DeferredStartDelegateHandle;

	// Frame counter for incoming data packets
	int32 FrameCounter = 0;

	// LiveLink subject name for this camera data
	FString CameraSubjectName = TEXT("Camera");

	// Pointer to the settings for this source so we don't have to duplicate data
	ULiveLinkRTTrPM_SourceSettings* SavedSourceSettings = nullptr;
};
