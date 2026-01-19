// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Common/UDPSocketReceiver.h"

#include "lib/thirdParty_motion.h"
#include "lib/RTTrP.h"

#include "RTTrPM_Component.generated.h"


USTRUCT(BlueprintType)
struct FRTTrPM_LED {
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTTrP_Motion")
	int32 Index;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTTrP_Motion")
	FVector Position;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTTrP_Motion")
	FVector Velocity;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTTrP_Motion")
	FVector Acceleration;

	FRTTrPM_LED()
	{
		Index = -1;
		Position = FVector::ZeroVector;
		Velocity = FVector::ZeroVector;
		Acceleration = FVector::ZeroVector;
	}
	FRTTrPM_LED(const FRTTrPM_LED& Other)
	{
		Index = Other.Index;
		Position = Other.Position;
		Velocity = Other.Velocity;
		Acceleration = Other.Acceleration;
	}
	FRTTrPM_LED(int32 InIndex, FVector InPosition, FVector InVelocity, FVector InAcceleration)
	{
		Index = InIndex;
		Position = InPosition;
		Velocity = InVelocity;
		Acceleration = InAcceleration;
	}
};

USTRUCT(BlueprintType)
struct FRTTrPM_Trackable
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTTrP_Motion")
	FString Name;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTTrP_Motion")
	FTransform Transform;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTTrP_Motion")
	FVector Velocity;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTTrP_Motion")
	FVector Acceleration;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTTrP_Motion")
	TArray<FRTTrPM_LED> LEDs;

public:
	FRTTrPM_Trackable()
	{
		Name = "";
		Transform = FTransform::Identity;
		Velocity = FVector::ZeroVector;
		Acceleration = FVector::ZeroVector;
	}

	FRTTrPM_Trackable(const FRTTrPM_Trackable& Other)
	{
		Name = Other.Name;
		Transform = Other.Transform;
		Velocity = Other.Velocity;
		Acceleration = Other.Acceleration;
		for(const FRTTrPM_LED& led : Other.LEDs) {
			LEDs.Add(FRTTrPM_LED(led));
		}
	}

	FRTTrPM_Trackable(const RTTrPM& motionPacket) {
		Name = FString(motionPacket.trackable->name.c_str());
		if (motionPacket.centroidMod != nullptr) {
			Transform.SetLocation(FVector(
				motionPacket.centroidMod->x * 100.0,
				motionPacket.centroidMod->y * 100.0,
				motionPacket.centroidMod->z * 100.0));
		}
		if( motionPacket.quatMod != nullptr) {
			Transform.SetRotation(FQuat(
				motionPacket.quatMod->Qx,
				motionPacket.quatMod->Qy,
				motionPacket.quatMod->Qz,
				motionPacket.quatMod->Qw));
		}
		if(motionPacket.cavMod != nullptr) {
			Transform.SetLocation(FVector(
				motionPacket.cavMod->x * 100.0,
				motionPacket.cavMod->y * 100.0,
				motionPacket.cavMod->z * 100.0));
			Velocity = FVector(
				motionPacket.cavMod->velx * 100.0,
				motionPacket.cavMod->vely * 100.0,
				motionPacket.cavMod->velz * 100.0);
			Acceleration = FVector(
				motionPacket.cavMod->accx * 100.0,
				motionPacket.cavMod->accy * 100.0,
				motionPacket.cavMod->accz * 100.0);
		}
		if(motionPacket.eulerMod != nullptr) {
			FRotator Rotator = FRotator(
				FMath::RadiansToDegrees(motionPacket.eulerMod->R1),
				FMath::RadiansToDegrees(motionPacket.eulerMod->R2),
				FMath::RadiansToDegrees(motionPacket.eulerMod->R3));
			Transform.SetRotation(FQuat(Rotator));
		}

	}
};

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
