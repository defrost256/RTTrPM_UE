// Copyright Epic Games, Inc. All Rights Reserved.

#include "RTTrP_Motion.h"

#if WITH_EDITOR
    #include "ISettingsModule.h"
    #include "ISettingsSection.h"
#endif

#define LOCTEXT_NAMESPACE "FRTTrP_MotionModule"

void FRTTrP_MotionModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

}

void FRTTrP_MotionModule::ShutdownModule()
{
	//StopListeningForRTTrPM();
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

void FRTTrP_MotionModule::OnAppPreExit()
{
    // Remove any bound delegates. It's no longer relevant for us to
    // send a transport error when we are in the shutdown phase.
	//StopListeningForRTTrPM();
}

bool FRTTrP_MotionModule::OnSettingsChanged()
{
	//StopListeningForRTTrPM();
    // Restart services to apply changes
    //return ListenForRTTrPM();
    return true;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FRTTrP_MotionModule, RTTrP_Motion)