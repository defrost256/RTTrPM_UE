// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "Common/UDPSocketReceiver.h"
#include "UObject/Interface.h"

#include "RTTrP_types.h"
#include "RTTrP_Client.h"
#include "RTTrP_Subsystem.generated.h"

UINTERFACE(BlueprintType, MinimalAPI)
class URTTrP_ClientInterface : public UInterface
{
	GENERATED_BODY()
};
class IRTTrP_ClientInterface
{
	GENERATED_BODY()

public:
	UFUNCTION()
	virtual void UpdateTrackable(const FRTTrPM_Trackable& trackable) = 0;
};

/**
 * 
 */
UCLASS(BlueprintType)
class RTTRP_MOTION_API URTTrP_Subsystem : public UEngineSubsystem
{
	GENERATED_BODY()
public:

	UPROPERTY(BlueprintAssignable, Category = "RTTrP_Motion")
	FOnRTTrPTrackableReceived OnRTTrPTrackableReceived;

public:

	UFUNCTION(BlueprintCallable, Category = "RTTrP_Motion")
	static bool IsConnected();
	UFUNCTION(BlueprintCallable, Category = "RTTrP_Motion")
	static bool ListenForRTTrPM();
	UFUNCTION(BlueprintCallable, Category = "RTTrP_Motion")
	static void StopListeningForRTTrPM();
	UFUNCTION(BlueprintCallable, Category = "RTTrP_Motion")
	static void BindRTTrPClient(TScriptInterface<IRTTrP_ClientInterface> Client, FString TrackableName);
	UFUNCTION(BlueprintCallable, Category = "RTTrP_Motion")
	static void UnbindRTTrPClient(TScriptInterface<IRTTrP_ClientInterface> Client);
	UFUNCTION(BlueprintCallable, Category = "RTTrP_Motion")
	static void RebindRTTrPClient(TScriptInterface<IRTTrP_ClientInterface> Client, FString NewTrackableName);

private:
	void OnPacketReceived(const FArrayReaderPtr& Data, const FIPv4Endpoint& Endpoint);
	void SendTrackable(const FRTTrPM_Trackable& Trackable);
private:
	bool bIsConnected = false;
	FSocket* Socket;
	FUdpSocketReceiver* UDPReceiver;
	
	TMap<FString, TArray<IRTTrP_ClientInterface*>> TrackableClientsMap;
	TMap<IRTTrP_ClientInterface*, FString> LastKnownTrackableMap;
	TSet<FString> KnownTrackablesSet;
};
