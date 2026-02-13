// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkRTTrPM_ConnectionSettings.generated.h"

USTRUCT()
struct RTTRP_MOTION_API FLiveLinkRTTrPM_ConnectionSettings
{
	GENERATED_BODY()

public:
	/** IP address of the free-d tracking source */
	UPROPERTY(EditAnywhere, Category = "Connection Settings")
	FString IPAddress = TEXT("127.0.0.1");

	/** UDP port number */
	UPROPERTY(EditAnywhere, Category = "Connection Settings")
	uint16 UDPPortNumber = 40000;
};
