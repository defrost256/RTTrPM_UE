// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "lib/thirdParty_motion.h"
#include "lib/RTTrP.h"

#include "RTTrP_types.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogRTTrP, Log, All);

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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTTrP_Motion")
	TArray<FString> ActiveZones;

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
		for (const FRTTrPM_LED& led : Other.LEDs) {
			LEDs.Add(FRTTrPM_LED(led));
		}
		for (FString zone : Other.ActiveZones) {
			ActiveZones.Add(zone);
		}
	}

	FRTTrPM_Trackable(const RTTrPM& motionPacket) {
		Name = FString(motionPacket.trackable->name.c_str());
		if (motionPacket.centroidMod != nullptr) {
			Transform.SetLocation(FVector(
				motionPacket.centroidMod->x * 100.0,
				motionPacket.centroidMod->y * -100.0,
				motionPacket.centroidMod->z * 100.0));
		}
		if (motionPacket.quatMod != nullptr) {
			Transform.SetRotation(FQuat(
				motionPacket.quatMod->Qx,
				motionPacket.quatMod->Qy,
				motionPacket.quatMod->Qz,
				motionPacket.quatMod->Qw));
		}
		if (motionPacket.cavMod != nullptr) {
			Transform.SetLocation(FVector(
				motionPacket.cavMod->x * 100.0,
				motionPacket.cavMod->y * -100.0,
				motionPacket.cavMod->z * 100.0));
			Velocity = FVector(
				motionPacket.cavMod->velx * 100.0,
				motionPacket.cavMod->vely * -100.0,
				motionPacket.cavMod->velz * 100.0);
			Acceleration = FVector(
				motionPacket.cavMod->accx * 100.0,
				motionPacket.cavMod->accy * -100.0,
				motionPacket.cavMod->accz * 100.0);
		}
		if (motionPacket.eulerMod != nullptr) {
			FRotator Rotator = FRotator(
				FMath::RadiansToDegrees(motionPacket.eulerMod->R1),
				FMath::RadiansToDegrees(motionPacket.eulerMod->R2),
				FMath::RadiansToDegrees(motionPacket.eulerMod->R3));
			Transform.SetRotation(FQuat(Rotator));
		}
		if(motionPacket.zoneMod != nullptr) {
			for (int i = 0; i < motionPacket.zoneMod->numofZoneSubModules; i++) {
				ZoneSubMod* subMod = motionPacket.zoneSubMod->at(i);
				if (subMod == nullptr) {
					UE_LOG(LogRTTrP, Warning, TEXT("Malformed submod"));
					continue;
				}
				const ANSICHAR* zoneName_c = subMod->zoneName.c_str();
				/*if (zoneName_c == nullptr)
					UE_LOG(LogRTTrP, Warning, TEXT("Malformed zone name"));
					continue;*/
				FString ZoneName = FString(subMod->zoneName.data(), subMod->zoneNameLength);
				//UE_LOG(LogRTTrP, Log, TEXT("%s Entered zone %s"), *Name, *ZoneName);
				ActiveZones.Add(ZoneName);
			}
		}
	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRTTrPTrackableReceived, FRTTrPM_Trackable, trackable);