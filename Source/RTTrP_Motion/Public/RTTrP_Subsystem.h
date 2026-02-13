// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "Common/UDPSocketReceiver.h"
#include "UObject/Interface.h"

#include "RTTrP_Settings.h"
#include "RTTrP_types.h"
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
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category= "RTTrP_Motion")
	void UpdateTrackable(const FRTTrPM_Trackable& trackable);
};

/**
 * 
 */
UCLASS(BlueprintType)
class RTTRP_MOTION_API URTTrP_Subsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "RTTrP_Motion")
	static bool IsConnected();
	UFUNCTION(BlueprintCallable, Category = "RTTrP_Motion")
	static bool ListenForRTTrPM(const URTTrP_Settings* settings_in);
	UFUNCTION(BlueprintCallable, Category = "RTTrP_Motion")
	static void StopListeningForRTTrPM();
	UFUNCTION(BlueprintCallable, Category = "RTTrP_Motion")
	static void BindRTTrPClient(TScriptInterface<IRTTrP_ClientInterface> Client, FString TrackableName);
	UFUNCTION(BlueprintCallable, Category = "RTTrP_Motion")
	static void UnbindRTTrPClient(TScriptInterface<IRTTrP_ClientInterface> Client);
	UFUNCTION(BlueprintCallable, Category = "RTTrP_Motion")
	static void RebindRTTrPClient(TScriptInterface<IRTTrP_ClientInterface> Client, FString NewTrackableName);


public:
	// -- Subsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	// -- End of Subsystem interface
private:
	void OnPacketReceived(const FArrayReaderPtr& Data, const FIPv4Endpoint& Endpoint);
	void SendTrackable(FRTTrPM_Trackable Trackable);
private:
	bool bIsConnected = false;
	FSocket* Socket;
	FUdpSocketReceiver* UDPReceiver;
	
	TMap<FString, TArray<UObject*>> TrackableClientsMap;
	TMap<UObject*, FString> LastKnownTrackableMap;
	TSet<FString> KnownTrackablesSet;
};
