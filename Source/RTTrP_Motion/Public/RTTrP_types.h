// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Serialization/ArrayReader.h"

#include "Common/UdpSocketReceiver.h"
#include "lib/thirdParty_motion.h"
#include "lib/RTTrP.h"

#include "RTTrP_types.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogRTTrP, Log, All);

#define RTTrP_INT_LE 0x5441
#define RTTrP_INT_BE 0x4154
#define RTTrPM_FLT_LE 0x3443
#define RTTrPM_FLT_BE 0x4334
#define RTTrP_Version 0x0002

enum RTTrP_PacketType: uint8_t {
	Trackable = 0x01,
	Centroid_Pos = 0x02,
	Orientation_Quat = 0x03,
	Orientation_Euler = 0x04,
	LED_Pos = 0x06,
	Lighting = 0x07,
	Universe = 0x09,
	SpotID = 0x0A,
	Centroid_AccVel = 0x20,
	LED_AccVel = 0x21,
	Zone = 0x22,
	Trackable_TS = 0x51
};

enum RTTrPM_EulerOrder : uint16_t {
	XYZ = 0x0123,
	XZY = 0x0132,
	YXZ = 0x0213,
	YZX = 0x0231,
	ZXY = 0x0312,
	ZYX = 0x0321
};

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

	FRTTrPM_Trackable(FArrayReaderPtr data);

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

struct RTTrP_Header {
	uint16_t intSig, fltSig, version;
	uint32_t pID;
	uint8_t pForm;
	uint16_t pktSize;
	uint32_t context;
	uint8_t numMods;

	RTTrP_Header(FArrayReader& data);
};

struct RTTrPM_Centroid {
	uint8_t pkType;
	uint16_t size, latency;
	double x, y, z;
	float accx, accy, accz;
	float velx, vely, velz;

	void Update(FArrayReader& data, uint8_t pkType, uint16_t intSig, uint16_t fltSig);
};

struct RTTrPM_LED {
	uint8_t pkType;
	uint16_t size, latency;
	double x, y, z;
	float accx, accy, accz;
	float velx, vely, velz;
	uint8_t index = -1;

	void Update(FArrayReader& data, uint8_t pkType, uint16_t intSig, uint16_t fltSig);
	void Update(RTTrPM_LED& other);

	FVector GetPosition() const {
		return FVector(x * 100.0, y * -100.0, z * 100.0);
	}
};

struct RTTrPM_Orientation {
	uint8_t pkType;
	uint16_t size, latency, eulerOrder;
	double R1, R2, R3;
	double Qx, Qy, Qz, Qw;

	void Update(FArrayReader& data, uint8_t pkType, uint16_t intSig, uint16_t fltSig);
};

struct RTTrPM_Trackable {
	uint8_t pkType;
	uint16_t size;
	uint8_t nameLen;
	FString name;
	uint8_t numMods;
	uint32_t timeStamp = 0;
	RTTrPM_Centroid centroid;
	RTTrPM_Orientation orientation;
	TMap<uint8_t, RTTrPM_LED> LEDs;
	TArray<FString> zones;

	RTTrPM_Trackable(FArrayReader& data, uint16_t intSig, uint16_t fltSig);

	FTransform GetTransform() const{
		FTransform transform = FTransform::Identity;
		transform.SetLocation(FVector(
			centroid.x * 100.0,
			centroid.y * -100.0,
			centroid.z * 100.0));
		transform.SetRotation(FQuat(
			orientation.Qx,
			orientation.Qy,
			orientation.Qz,
			orientation.Qw));
		return transform;
	}

};



DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRTTrPTrackableReceived, FRTTrPM_Trackable, trackable);