// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "UObject/NoExportTypes.h"
#include "RTTrP_Settings.generated.h"

/**
 * 
 */
UCLASS(Config=Engine, DefaultConfig)
class RTTRP_MOTION_API URTTrP_Settings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
		UPROPERTY(Config, EditAnywhere, Category = "RTTrP_Motion")
	bool bAutoconnect = true;

	UPROPERTY(Config, EditAnywhere, Category = "RTTrP_Motion")
	FString AdapterIP = "192.168.88.100";

	UPROPERTY(Config, EditAnywhere, Category = "RTTrP_Motion")
	int32 ListenPort = 24002;

	UPROPERTY(Config, EditAnywhere, Category = "RTTrP_Motion")
	bool bMulticast = true;

	UPROPERTY(Config, EditAnywhere, Category = "RTTrP_Motion")
	FString MulticastIP = "238.210.10.1";



		
};
