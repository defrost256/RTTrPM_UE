// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Common/UDPSocketReceiver.h"

#include "RTTrP_types.h"

#include "RTTrPM_Component.generated.h"

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class RTTRP_MOTION_API URTTrPM_Component : public UActorComponent
{
	GENERATED_BODY()

public:

		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTTrP_Motion")
		int32 ListenPort = 24002;
		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTTrP_Motion")
		FString AdapterIP = "192.168.88.100";
		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTTrP_Motion")
		FString MulticastIP = "238.210.10.1";
		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTTrP_Motion")
		bool bMulticast = true;

		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTTrP_Motion")
		bool bUpdateInEditor = false;
public:	
	// Sets default values for this component's properties
	URTTrPM_Component();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "RTTrP_Motion")
	void ConnectRTTrP();
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "RTTrP_Motion")
	void DisconnectRTTrP();
	UFUNCTION(BlueprintCallable, Category = "RTTrP_Motion")
	FRTTrPM_Trackable GetTrackableByName(const FString& TrackableName) const;
	UFUNCTION(BlueprintCallable, Category = "RTTrP_Motion")
	TArray<FString> GetAllTrackableNames();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	virtual void OnPacketReceived(const FArrayReaderPtr& Data, const FIPv4Endpoint& Endpoint);

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
private:
	FSocket* Socket;
	FUdpSocketReceiver* UDPReceiver;
	bool bInitialized = false;

	TMap<FString, FRTTrPM_Trackable> Trackables;
		
};
