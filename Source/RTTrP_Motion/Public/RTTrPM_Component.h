// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "RTTrP_types.h"
#include "RTTrP_Subsystem.h"

#include "RTTrPM_Component.generated.h"

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class RTTRP_MOTION_API URTTrPM_Component : public UActorComponent, public IRTTrP_ClientInterface
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTTrP_Motion")
	FString TrackableName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTTrP_Motion")
	bool bUpdateInEditor = false;

	UPROPERTY(BlueprintAssignable, Category = "RTTrP_Motion")
	FOnRTTrPTrackableReceived OnTrackableUpdated;
public:	
	// Sets default values for this component's properties
	URTTrPM_Component();
	~URTTrPM_Component();
	virtual void OnComponentCreated() override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	// IRTTrP_ClientInterface implementation
	virtual void UpdateTrackable_Implementation(const FRTTrPM_Trackable& trackable) override;
protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
private:
	FRTTrPM_Trackable CurrentTrackable;
	void UpdateState();
};
