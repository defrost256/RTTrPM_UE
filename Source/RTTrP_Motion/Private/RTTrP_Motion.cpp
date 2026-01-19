// Copyright Epic Games, Inc. All Rights Reserved.

#include "RTTrP_Motion.h"

#if WITH_EDITOR
    #include "ISettingsModule.h"
    #include "ISettingsSection.h"
#endif
#include "Common/UdpSocketBuilder.h"
#include "lib/RTTrP.h"

#include "RTTrP_Settings.h"

#define LOCTEXT_NAMESPACE "FRTTrP_MotionModule"

DEFINE_LOG_CATEGORY(LogRTTrP);

void FRTTrP_MotionModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

    // Hook to the PreExit callback, needed to execute UObject related shutdowns
    FCoreDelegates::OnPreExit.AddRaw(
        this, &FRTTrP_MotionModule::OnAppPreExit);

#if WITH_EDITOR

    // Register settings
    if (ISettingsModule* SettingsModule
        = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
    {
        const ISettingsSectionPtr SettingsSection
            = SettingsModule->RegisterSettings("Project", "Plugins", "QuicMessaging",
                LOCTEXT("QuicMessagingSettingsName", "QUIC Messaging"),
                LOCTEXT("QuicMessagingSettingsDescription", "Configure the QUIC Messaging plugin."),
                GetMutableDefault<URTTrP_Settings>()
            );

        if (SettingsSection.IsValid())
        {
            SettingsSection->OnModified().BindRaw(
                this, &FRTTrP_MotionModule::OnSettingsChanged);
        }
    }
#endif

	if (GetMutableDefault<URTTrP_Settings>()->bAutoconnect) {
		ListenForRTTrPM();
	}

}

void FRTTrP_MotionModule::ShutdownModule()
{
	StopListeningForRTTrPM();
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

bool FRTTrP_MotionModule::IsConnected()
{
    return bIsConnected;
}

bool FRTTrP_MotionModule::ListenForRTTrPM()
{
	URTTrP_Settings* Settings = GetMutableDefault<URTTrP_Settings>();
	if (!bIsConnected) {
		FIPv4Address ipv4;
		if (!FIPv4Address::Parse(Settings->AdapterIP, ipv4)) {
			UE_LOG(LogRTTrP, Error, TEXT("RTTrP_Motion: Invalid Adapter IP address: %s"), *Settings->AdapterIP);
			return false;
		}
		FIPv4Address multiGroup;
		if (!FIPv4Address::Parse(Settings->MulticastIP, multiGroup)) {
			UE_LOG(LogRTTrP, Error, TEXT("RTTrP_Motion: Invalid Adapter IP address: %s"), *Settings->MulticastIP);
			return false;
		}
		FUdpSocketBuilder SocketBuilder(TEXT("RTTrP_Motion_Socket"));
		SocketBuilder.BoundToAddress(ipv4);
		SocketBuilder.BoundToPort(Settings->ListenPort).AsNonBlocking().AsReusable();
		if (Settings->bMulticast) {
			SocketBuilder.WithMulticastTtl(8).WithMulticastLoopback().WithMulticastInterface(ipv4);
			if (ipv4 == FIPv4Address::Any)
			{
				TArray<TSharedPtr<FInternetAddr>> LocapIps;
				ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetLocalAdapterAddresses(LocapIps);
				for (const TSharedPtr<FInternetAddr>& LocalIp : LocapIps)
				{
					uint32 locIpRaw = 0;
					LocalIp->GetIp(locIpRaw);
					SocketBuilder.JoinedToGroup(multiGroup, FIPv4Address(locIpRaw));

				}

				// GetLocalAdapterAddresses returns empty list when all network adapters are offline
				if (LocapIps.Num() == 0)
				{
					bool bCanBindAll = false;
					uint32 locIpRaw = 0;
					ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetLocalHostAddr(*GLog, bCanBindAll)->GetIp(locIpRaw);
					SocketBuilder.JoinedToGroup(multiGroup, locIpRaw);
				}
			}
			else {
				SocketBuilder.JoinedToGroup(multiGroup, ipv4);
			}
		}

		Socket = SocketBuilder.Build();
		if(Socket == nullptr) {
			UE_LOG(LogRTTrP, Error, TEXT("RTTrP_Motion: Failed to create socket on %s:%d"), *Settings->AdapterIP, Settings->ListenPort);
			return false;
		}
		UDPReceiver = new FUdpSocketReceiver(
			Socket,
			FTimespan::FromMilliseconds(1000),
			TEXT("RTTrP_Motion_UDPReceiver")
		);
		UDPReceiver->OnDataReceived().BindRaw(this, &FRTTrP_MotionModule::OnPacketReceived);
		UDPReceiver->Start();
		bIsConnected = true;
		if (Settings->bMulticast) {
			UE_LOG(LogRTTrP, Log, TEXT("RTTrP_Motion: Connected to multicast group %s:%d on %s"), *Settings->MulticastIP, Settings->ListenPort, *Settings->AdapterIP);
		}
		else {
			UE_LOG(LogRTTrP, Log, TEXT("RTTrP_Motion: Connected to RTTrP host on %s:%d"), *Settings->AdapterIP, Settings->ListenPort);
		}
	}
	else {
		UE_LOG(LogRTTrP, Warning, TEXT("RTTrP_Motion: Already connected."));
	}
	return true;
}

void FRTTrP_MotionModule::StopListeningForRTTrPM()
{
	if (bIsConnected) {
		TSharedRef<FInternetAddr> tmpAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
		if (Socket != nullptr) {
			Socket->GetAddress(*tmpAddr);
			if(UDPReceiver != nullptr) {
				UDPReceiver->Stop();
				delete UDPReceiver;
			}
			Socket->Close();
			delete Socket;
		}
		bIsConnected = false;
		UE_LOG(LogRTTrP, Log, TEXT("RTTrP_Motion: DbIsConnected from %s:%d"), *tmpAddr->ToString(true), Socket->GetPortNo());
	}
	else {
		UE_LOG(LogRTTrP, Warning, TEXT("RTTrP_Motion: Not connected."));
	}
}

void FRTTrP_MotionModule::OnPacketReceived(const FArrayReaderPtr& Data, const FIPv4Endpoint& Endpoint) {
	// Process received data
	std::vector<UCHAR> dataVec(Data->GetData(), Data->GetData() + Data->NumBytes());
	RTTrP header(dataVec);
	TArray<FRTTrPM_Trackable> Trackables;
	if (header.fltHeader == 0x4334 || header.fltHeader == 0x3443) //RTTrPM
	{
		//UE_LOG(LogTemp, Log, TEXT("RTTrP_Motion: Received RTTrPM packet from %s"), *Endpoint.ToString());
		dataVec = header.data; // Remaining data after header
		for (int trackableIndex = 0; trackableIndex < header.numMods; trackableIndex++) {
			RTTrPM motionPacket;
			motionPacket.header = &header;
			motionPacket.trackable = new Trackable(&dataVec, header.intHeader, header.fltHeader);
			for (int modIndex = 0; modIndex < motionPacket.trackable->numMods; modIndex++) {
				uint8_t pkType = dataVec[0];
				motionPacket.pkType.push_back(pkType);
				switch (pkType) {
				case 0x02: // Centroid Module
					motionPacket.centroidMod = new CentroidMod(&dataVec, header.intHeader, header.fltHeader);
					break;
				case 0x03: // Quaternion Module
					motionPacket.quatMod = new QuatModule(&dataVec, header.intHeader, header.fltHeader);
					break;
				case 0x04: // Euler Module
					motionPacket.eulerMod = new EulerModule(&dataVec, header.intHeader, header.fltHeader);
					break;
				case 0x06: // LED Module
				{
					if (motionPacket.ledMod == nullptr) {
						motionPacket.ledMod = new std::vector<LEDModule*>();
					}
					LEDModule* ledModule = new LEDModule(&dataVec, header.intHeader, header.fltHeader);
					motionPacket.ledMod->push_back(ledModule);
					break;
				}
				case 0x20: // Centroid AccVel Module
					motionPacket.cavMod = new CentroidAccVelMod(&dataVec, header.intHeader, header.fltHeader);
					break;
				case 0x21: // LED AccVel Module
				{
					if (motionPacket.lavMod == nullptr) {
						motionPacket.lavMod = new std::vector<LEDAccVelMod*>();
					}
					LEDAccVelMod* lavModule = new LEDAccVelMod(&dataVec, header.intHeader, header.fltHeader);
					motionPacket.lavMod->push_back(lavModule);
					break;
				}
				default:
					UE_LOG(LogTemp, Warning, TEXT("RTTrP_Motion: Unknown module type: 0x%02X"), pkType);
					return;
				}
			}
			FRTTrPM_Trackable trackable(motionPacket);
			Trackables.Add(trackable);
		}
		OnRTTrPTrackableReceived.Broadcast(Trackables);
	}
}


void FRTTrP_MotionModule::OnAppPreExit()
{
    // Remove any bound delegates. It's no longer relevant for us to
    // send a transport error when we are in the shutdown phase.
	StopListeningForRTTrPM();
}

bool FRTTrP_MotionModule::OnSettingsChanged()
{
	StopListeningForRTTrPM();
    // Restart services to apply changes
    return ListenForRTTrPM();
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FRTTrP_MotionModule, RTTrP_Motion)