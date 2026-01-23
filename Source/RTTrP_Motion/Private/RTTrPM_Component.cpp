// Fill out your copyright notice in the Description page of Project Settings.


#include "RTTrPM_Component.h"
#include "lib/RTTrP.h"

// Sets default values for this component's properties
URTTrPM_Component::URTTrPM_Component()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	
	

	// ...
}

URTTrPM_Component::~URTTrPM_Component()
{
	
}

void URTTrPM_Component::OnComponentCreated()
{
	URTTrP_Subsystem* RTTP_Subsystem = GEngine->GetEngineSubsystem<URTTrP_Subsystem>();
	RTTP_Subsystem->BindRTTrPClient(TScriptInterface<IRTTrP_ClientInterface>(this), TrackableName);
}

void URTTrPM_Component::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	URTTrP_Subsystem* RTTP_Subsystem = GEngine->GetEngineSubsystem<URTTrP_Subsystem>();
	RTTP_Subsystem->UnbindRTTrPClient(TScriptInterface<IRTTrP_ClientInterface>(this));
}

void URTTrPM_Component::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	if (PropertyName == GET_MEMBER_NAME_CHECKED(URTTrPM_Component, TrackableName))
	{
		URTTrP_Subsystem* RTTP_Subsystem = GEngine->GetEngineSubsystem<URTTrP_Subsystem>();
		RTTP_Subsystem->RebindRTTrPClient(TScriptInterface<IRTTrP_ClientInterface>(this), TrackableName);
	}
}

void URTTrPM_Component::UpdateTrackable_Implementation(const FRTTrPM_Trackable& trackable)
{
	// Make a local copy of the transform for the async lambda
	CurrentTrackable = trackable;

	// If we're already on the game thread just update directly.
	if (IsInGameThread())
	{
		UpdateState();
		return;
	}

	// Determine whether we're playing in editor (PIE).
	bool bIsPlayInEditor = false;
	if (UWorld* World = GetWorld())
	{
#if WITH_EDITOR
		bIsPlayInEditor = (World->WorldType == EWorldType::PIE);
#endif
	}

	// If we're NOT playing in editor, ensure UpdateState runs on the Game Thread.
	// Also allow updates in PIE only when bUpdateInEditor is true.
	if (!bIsPlayInEditor)
	{
		AsyncTask(ENamedThreads::GameThread, [this]()
			{
				// Check object validity before touching it on the game thread
				if (IsValid(this) && !this->IsBeingDestroyed())
				{
					UpdateState();
				}
			});
	}
	// Otherwise (playing in editor and not allowed to update), do nothing.
}


// Called when the game starts
void URTTrPM_Component::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}

// Called every frame
void URTTrPM_Component::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	this->GetOwner()->GetRootComponent()->SetRelativeLocation(CurrentTrackable.Transform.GetLocation());
	// ...
}

void URTTrPM_Component::UpdateState()
{
	if (!bUpdateInEditor)
		return;
	AActor* owner = this->GetOwner();
	if(owner == nullptr) {
		return;
	}
	owner->GetRootComponent()->SetRelativeLocation(CurrentTrackable.Transform.GetLocation());
	if(OnTrackableUpdated.IsBound()) {
		OnTrackableUpdated.Broadcast(CurrentTrackable);
	}
}

