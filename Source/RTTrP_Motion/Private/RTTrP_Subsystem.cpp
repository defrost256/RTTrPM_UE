// Fill out your copyright notice in the Description page of Project Settings.


#include "RTTrP_Subsystem.h"
#include "RTTrP_Settings.h"

#include "Common/UdpSocketBuilder.h"

bool URTTrP_Subsystem::IsConnected()
{
	URTTrP_Subsystem* Self = GEngine->GetEngineSubsystem<URTTrP_Subsystem>();
	return Self->bIsConnected;
}

bool URTTrP_Subsystem::ListenForRTTrPM()
{
	URTTrP_Subsystem* Self = GEngine->GetEngineSubsystem<URTTrP_Subsystem>();
	URTTrP_Settings* Settings = GetMutableDefault<URTTrP_Settings>();
	if (!Self->bIsConnected) {
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

		FSocket* Socket = SocketBuilder.Build();
		if (Socket == nullptr) {
			UE_LOG(LogRTTrP, Error, TEXT("RTTrP_Motion: Failed to create socket on %s:%d"), *Settings->AdapterIP, Settings->ListenPort);
			return false;
		}
		Self->UDPReceiver = new FUdpSocketReceiver(
			Socket,
			FTimespan::FromMilliseconds(1000),
			TEXT("RTTrP_Motion_UDPReceiver")
		);
		Self->UDPReceiver->OnDataReceived().BindUObject(Self, &URTTrP_Subsystem::OnPacketReceived);
		Self->UDPReceiver->Start();
		Self->Socket = Socket;
		Self->bIsConnected = true;
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

void URTTrP_Subsystem::StopListeningForRTTrPM()
{
	URTTrP_Subsystem* Self = GEngine->GetEngineSubsystem<URTTrP_Subsystem>();
	if (Self->bIsConnected) {
		TSharedRef<FInternetAddr> tmpAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
		FSocket* Socket = Self->Socket;
		if (Socket != nullptr) {
			Socket->GetAddress(*tmpAddr);
			if (Self->UDPReceiver != nullptr) {
				Self->UDPReceiver->Stop();
				delete Self->UDPReceiver;
			}
			Socket->Close();
			delete Socket;
		}
		Self->bIsConnected = false;
		UE_LOG(LogRTTrP, Log, TEXT("RTTrP_Motion: DbIsConnected from %s:%d"), *tmpAddr->ToString(true), Socket->GetPortNo());
	}
	else {
		UE_LOG(LogRTTrP, Warning, TEXT("RTTrP_Motion: Not connected."));
	}
}

void URTTrP_Subsystem::BindRTTrPClient(TScriptInterface<IRTTrP_ClientInterface> Client, FString TrackableName)
{
	URTTrP_Subsystem* Self = GEngine->GetEngineSubsystem<URTTrP_Subsystem>();

	if(Self->LastKnownTrackableMap.Contains(Client.GetInterface())) {
		UnbindRTTrPClient(Client);
	}
	Self->TrackableClientsMap.FindOrAdd(TrackableName).Add(Client.GetInterface());
	Self->LastKnownTrackableMap.Add(Client.GetInterface(), TrackableName);
}

void URTTrP_Subsystem::UnbindRTTrPClient(TScriptInterface<IRTTrP_ClientInterface> Client)
{
	URTTrP_Subsystem* Self = GEngine->GetEngineSubsystem<URTTrP_Subsystem>();
	FString* LastKnownTrackable = Self->LastKnownTrackableMap.Find(Client.GetInterface());
	if (LastKnownTrackable != nullptr) {
		TArray<IRTTrP_ClientInterface*>* ClientList = Self->TrackableClientsMap.Find(*LastKnownTrackable);
		if (ClientList != nullptr) {
			ClientList->Remove(Client.GetInterface());
			if (ClientList->Num() == 0) {
				Self->TrackableClientsMap.Remove(*LastKnownTrackable);
			}
		}
		Self->LastKnownTrackableMap.Remove(Client.GetInterface());
	}
}

void URTTrP_Subsystem::RebindRTTrPClient(TScriptInterface<IRTTrP_ClientInterface> Client, FString NewTrackableName)
{
	UnbindRTTrPClient(Client);
	URTTrP_Subsystem* Self = GEngine->GetEngineSubsystem<URTTrP_Subsystem>();
	Self->TrackableClientsMap.FindOrAdd(NewTrackableName).Add(Client.GetInterface());
	Self->LastKnownTrackableMap.Add(Client.GetInterface(), NewTrackableName);
}

void URTTrP_Subsystem::OnPacketReceived(const FArrayReaderPtr& Data, const FIPv4Endpoint& Endpoint) {
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
			SendTrackable(trackable);
			KnownTrackablesSet.Add(trackable.Name);
			Trackables.Add(trackable);
		}
		OnRTTrPTrackableReceived.Broadcast(Trackables);
	}
}

void URTTrP_Subsystem::SendTrackable(const FRTTrPM_Trackable& trackable)
{
	TArray<IRTTrP_ClientInterface*>* clients = TrackableClientsMap.Find(trackable.Name);
	for(IRTTrP_ClientInterface* client : *clients) {
		client->UpdateTrackable(trackable);
	}
}
