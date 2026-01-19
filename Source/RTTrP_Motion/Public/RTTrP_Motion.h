// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"
#include "Common/UDPSocketReceiver.h"
#include "RTTrP_types.h"

DECLARE_LOG_CATEGORY_EXTERN(LogRTTrP, Log, All);

class FRTTrP_MotionModule : public IModuleInterface
{
public:

	UPROPERTY(BlueprintAssignable, Category = "RTTrP_Motion")
	FOnRTTrPTrackableReceived OnRTTrPTrackableReceived;

public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	bool IsConnected();
	bool ListenForRTTrPM();
	void StopListeningForRTTrPM();
private:
	void OnAppPreExit();
	bool OnSettingsChanged();
	void OnPacketReceived(const FArrayReaderPtr& Data, const FIPv4Endpoint& Endpoint);
	

private:
	bool bIsConnected = false;
	FSocket* Socket;
	FUdpSocketReceiver* UDPReceiver;
};
