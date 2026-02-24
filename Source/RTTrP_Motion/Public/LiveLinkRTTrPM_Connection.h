// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "LiveLinkRTTrPM_Connection.generated.h"

/**
 * 
 */
USTRUCT()
struct RTTRP_MOTION_API FLiveLinkRTTrPM_ConnectionSettings
{
	GENERATED_BODY()

	UPROPERTY(config, EditAnywhere, Category = "RTTrP_Motion|LiveLink")
	FString AdapterIP = "192.168.88.100";

	UPROPERTY(config, EditAnywhere, Category = "RTTrP_Motion|LiveLink")
	int32 ListenPort = 24002;

	UPROPERTY(config, EditAnywhere, Category = "RTTrP_Motion|LiveLink")
	bool bMulticast = true;

	UPROPERTY(config, EditAnywhere, Category = "RTTrP_Motion|LiveLink")
	FString MulticastIP = "238.210.10.1";
};
