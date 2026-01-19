// Fill out your copyright notice in the Description page of Project Settings.


#include "RTTrPM_Component.h"
#include "lib/RTTrP.h"

#include "Common/UdpSocketBuilder.h"

// Sets default values for this component's properties
URTTrPM_Component::URTTrPM_Component()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	
	// ...
}

void URTTrPM_Component::ConnectRTTrP()
{
	if (!bInitialized) {
		FIPv4Address ipv4;
		if (!FIPv4Address::Parse(AdapterIP, ipv4)) {
			UE_LOG(LogTemp, Error, TEXT("RTTrP_Motion: Invalid Adapter IP address: %s"), *AdapterIP);
			return;
		}
		FIPv4Address multiGroup;
		if (!FIPv4Address::Parse(MulticastIP, multiGroup)) {
			UE_LOG(LogTemp, Error, TEXT("RTTrP_Motion: Invalid Adapter IP address: %s"), *MulticastIP);
			return;
		}
		FUdpSocketBuilder SocketBuilder(TEXT("RTTrP_Motion_Socket"));
		SocketBuilder.BoundToAddress(ipv4);
		SocketBuilder.BoundToPort(ListenPort).AsNonBlocking().AsReusable();
		if (bMulticast) {
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
		UDPReceiver = new FUdpSocketReceiver(
			Socket,
			FTimespan::FromMilliseconds(1000),
			TEXT("RTTrP_Motion_UDPReceiver")
		);
		UDPReceiver->OnDataReceived().BindUObject(this, &URTTrPM_Component::OnPacketReceived);
		UDPReceiver->Start();
		bInitialized = true;
		if (bMulticast) {
			TSharedRef<FInternetAddr> tmpAddr = ISocketSubsystem::Get()->CreateInternetAddr();
			Socket->GetAddress(*tmpAddr);

			UE_LOG(LogTemp, Log, TEXT("RTTrP_Motion: Connected to multicast group %s:%d on %s\n\t%s"), *MulticastIP, ListenPort, *AdapterIP, *tmpAddr->ToString(true));
		}
		else {
			UE_LOG(LogTemp, Log, TEXT("RTTrP_Motion: Connected to RTTrP host on %s:%d"), *AdapterIP, ListenPort);
		}
	} else {
		UE_LOG(LogTemp, Warning, TEXT("RTTrP_Motion: Already connected."));
	}
}

void URTTrPM_Component::DisconnectRTTrP()
{
	if (bInitialized) {
		UDPReceiver->Stop();
		delete UDPReceiver;
		Socket->Close();
		delete Socket;
		bInitialized = false;
		UE_LOG(LogTemp, Log, TEXT("RTTrP_Motion: Disconnected from %s:%d"), *AdapterIP, ListenPort);
	} else {
		UE_LOG(LogTemp, Warning, TEXT("RTTrP_Motion: Not connected."));
	}
}

FRTTrPM_Trackable URTTrPM_Component::GetTrackableByName(const FString& TrackableName) const
{
	if(Trackables.Contains(TrackableName)) {
		return Trackables[TrackableName];
	}
	return FRTTrPM_Trackable();
}

TArray<FString> URTTrPM_Component::GetAllTrackableNames()
{
	TArray<FString> keys;
	Trackables.GetKeys(keys);
	return keys;
}


// Called when the game starts
void URTTrPM_Component::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}

void URTTrPM_Component::OnPacketReceived(const FArrayReaderPtr& Data, const FIPv4Endpoint& Endpoint)
{
	// Process received data
	std::vector<UCHAR> dataVec(Data->GetData(), Data->GetData() + Data->NumBytes());
	RTTrP header(dataVec);
	if (header.fltHeader == 0x4334 || header.fltHeader == 0x3443) //RTTrPM
	{
		UE_LOG(LogTemp, Log, TEXT("RTTrP_Motion: Received RTTrPM packet from %s"), *Endpoint.ToString());
		dataVec = header.data; // Remaining data after header
		for(int trackableIndex = 0; trackableIndex < header.numMods; trackableIndex++) {
			RTTrPM motionPacket;
			motionPacket.header = &header;
			motionPacket.trackable = new Trackable(&dataVec, header.intHeader, header.fltHeader);
			for(int modIndex = 0; modIndex < motionPacket.trackable->numMods; modIndex++) {
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
			Trackables.Add(FString(motionPacket.trackable->name.c_str()), trackable);
		}
	}
}


// Called every frame
void URTTrPM_Component::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

