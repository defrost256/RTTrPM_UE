// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Roles/LiveLinkTransformTypes.h"
#include "LiveLinkRTTrPM_DataTypes.generated.h"

UENUM(BlueprintType)
enum class ERTTrPM_SubjectType : uint8 {
	Centroid,
	LED1,
	LED2,
	LED3,
	Unknown
};

USTRUCT(BlueprintType)
struct FLiveLinkRTTrPM_StaticData : public FLiveLinkTransformStaticData {
	GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct FLiveLinkRTTrPM_FrameData : public FLiveLinkTransformFrameData {
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LiveLink")
	FVector CentroidPosition;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LiveLink")
	FVector LED1Position;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LiveLink")
	FVector LED2Position;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LiveLink")
	FVector LED3Position;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LiveLink")
	TArray<FString> Zones;
};

USTRUCT(BlueprintType)
struct FLiveLinkRTTrPM_BlueprintData : public FLiveLinkBaseBlueprintData {
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LiveLink")
	FLiveLinkRTTrPM_StaticData StaticData;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LiveLink")
	FLiveLinkRTTrPM_FrameData FrameData;
};