// Fill out your copyright notice in the Description page of Project Settings.


#include "LiveLinkRTTrPM_Source.h"
#include "Common/UdpSocketBuilder.h"
#include "RTTrP_types.h"
#include "LiveLinkRTTrPM_Role.h"
#include "SocketSubsystem.h"
#include "Async/Async.h"
#include "LiveLinkRTTrPM_DataTypes.h"
#include "Sockets.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "HAL/PlatformTime.h"

namespace
{
void DoJoinMulticastGroup(const TSharedRef<FInternetAddr>& MulticastAddr, const TSharedPtr<FInternetAddr>& InterfaceAddr, FSocket* MulticastSocket)
{
    if (!InterfaceAddr.IsValid() || MulticastSocket == nullptr)
    {
        return;
    }

    const bool bJoinedGroup = MulticastSocket->JoinMulticastGroup(*MulticastAddr, *InterfaceAddr);
    if (bJoinedGroup)
    {
        UE_LOG(LogRTTrP, Log, TEXT("LiveLinkRTTrPM_Source: Added local interface '%s' to multicast group '%s'"),
            *InterfaceAddr->ToString(false), *MulticastAddr->ToString(true));
    }
    else
    {
        UE_LOG(LogRTTrP, Warning, TEXT("LiveLinkRTTrPM_Source: Failed to join multicast group '%s' on interface '%s'"),
            *MulticastAddr->ToString(true), *InterfaceAddr->ToString(false));
    }
}
}

FLiveLinkRTTrPM_Source::FLiveLinkRTTrPM_Source(const FLiveLinkRTTrPM_ConnectionSettings& InConnectionSettings)
    : ConnectionSettings(InConnectionSettings)
    , Client(nullptr)
    , Socket(nullptr)
    , UDPReceiver(nullptr)
    , bIsConnected(false)
{
    SourceStatus = FText::FromString(TEXT("Initializing receivers..."));
    SourceType = FText::FromString(TEXT("RTTrPM"));

    FString MachineNameString = FString::Printf(TEXT("RTTrPM@%s:%d"), *ConnectionSettings.AdapterIP, ConnectionSettings.ListenPort);
    SourceMachineName = FText::FromString(MachineNameString);

    bShutdownRequested = false;
    bResetRequested = true;
    Update();

    if (ConnectionState == ELiveLinkRTTrPMState::EndpointsReady)
    {
        Update();
    }
}

FLiveLinkRTTrPM_Source::~FLiveLinkRTTrPM_Source()
{
    CloseSocket();
}

void FLiveLinkRTTrPM_Source::ReceiveClient(ILiveLinkClient* InClient, FGuid InSourceGuid)
{
    Client = InClient;
    SourceGuid = InSourceGuid;
}

bool FLiveLinkRTTrPM_Source::IsSourceStillValid() const
{
    return ConnectionState == ELiveLinkRTTrPMState::Receiving;
}

bool FLiveLinkRTTrPM_Source::RequestSourceShutdown()
{
    if (ConnectionState == ELiveLinkRTTrPMState::ShutDown)
    {
        return true;
    }

    StopUdpReceiver();
    bShutdownRequested = true;

    return false;
}

FText FLiveLinkRTTrPM_Source::GetSourceType() const
{
    return SourceType;
}

FText FLiveLinkRTTrPM_Source::GetSourceMachineName() const
{
    return SourceMachineName;
}

FText FLiveLinkRTTrPM_Source::GetSourceStatus() const
{
    return SourceStatus;
}

void FLiveLinkRTTrPM_Source::InitializeSettings(ULiveLinkSourceSettings* Settings)
{
    mSettings = Cast<ULiveLinkRTTrPM_SourceSettings>(Settings);
    if (mSettings != nullptr)
    {
        mSettings->TransformSource = ERTTrPM_SubjectType::Centroid;
    }
}

void FLiveLinkRTTrPM_Source::Update()
{
    if (bShutdownRequested && ConnectionState != ELiveLinkRTTrPMState::ShutDown)
    {
        CloseSocket();
        bIsConnected = false;
        ConnectionState = ELiveLinkRTTrPMState::ShutDown;
    }
    else if (bResetRequested && ConnectionState != ELiveLinkRTTrPMState::ShutDown)
    {
        ConnectionState = ELiveLinkRTTrPMState::ResetRequested;
        bResetRequested = false;
    }

    switch (ConnectionState)
    {
    case ELiveLinkRTTrPMState::NotStarted:
        SourceStatus = FText::FromString(TEXT("Not started"));
        break;

    case ELiveLinkRTTrPMState::ResetRequested:
        SourceStatus = FText::FromString(TEXT("Resetting source."));
        CloseSocket();
        bIsConnected = false;
        LastDataReadTime = 0;
        ConnectionState = ELiveLinkRTTrPMState::EndpointsReady;
        break;

    case ELiveLinkRTTrPMState::EndpointsReady:
        SourceStatus = FText::FromString(TEXT("Starting socket setup."));
        if (OpenSocket())
        {
            bIsConnected = true;
            ConnectionState = ELiveLinkRTTrPMState::Receiving;

            if (ConnectionSettings.bMulticast)
            {
                UE_LOG(LogRTTrP, Log, TEXT("LiveLinkRTTrPM_Source: Connected to multicast group %s:%d on %s"), *ConnectionSettings.MulticastIP, ConnectionSettings.ListenPort, *ConnectionSettings.AdapterIP);
            }
            else
            {
                UE_LOG(LogRTTrP, Log, TEXT("LiveLinkRTTrPM_Source: Connected to RTTrP host on %s:%d"), *ConnectionSettings.AdapterIP, ConnectionSettings.ListenPort);
            }
        }
        else
        {
            ConnectionState = ELiveLinkRTTrPMState::ResetRequested;
        }
        break;

    case ELiveLinkRTTrPMState::Receiving:
        if (FPlatformTime::Seconds() - LastDataReadTime < 1.0)
        {
            SourceStatus = FText::FromString(TEXT("Receiving."));
        }
        else
        {
            SourceStatus = FText::FromString(TEXT("Waiting for data."));
        }
        break;

    case ELiveLinkRTTrPMState::ShutDown:
        SourceStatus = FText::FromString(TEXT("Shut Down"));
        break;

    default:
        checkNoEntry();
        break;
    }
}

bool FLiveLinkRTTrPM_Source::OpenSocket()
{
    FIPv4Address UnicastAddress;
    if (!FIPv4Address::Parse(ConnectionSettings.AdapterIP, UnicastAddress))
    {
        UE_LOG(LogRTTrP, Error, TEXT("LiveLinkRTTrPM_Source: Invalid Adapter IP address: %s"), *ConnectionSettings.AdapterIP);
        return false;
    }

    FIPv4Address MulticastAddress;
    if (ConnectionSettings.bMulticast && !FIPv4Address::Parse(ConnectionSettings.MulticastIP, MulticastAddress))
    {
        UE_LOG(LogRTTrP, Error, TEXT("LiveLinkRTTrPM_Source: Invalid Multicast IP address: %s"), *ConnectionSettings.MulticastIP);
        return false;
    }

    const uint32 ReceiveBufferSize = 2 * 1024 * 1024;

    FUdpSocketBuilder SocketBuilder(TEXT("LiveLinkRTTrPM_Socket"));
    SocketBuilder
        .AsNonBlocking()
        .AsReusable()
        .BoundToAddress(UnicastAddress)
        .BoundToPort(ConnectionSettings.ListenPort)
        .WithReceiveBufferSize(ReceiveBufferSize);

    if (ConnectionSettings.bMulticast)
    {
        SocketBuilder
            .WithMulticastTtl(8)
            .WithMulticastLoopback()
            .WithMulticastInterface(UnicastAddress);
    }

    Socket = SocketBuilder.Build();
    if (Socket == nullptr)
    {
        UE_LOG(LogRTTrP, Error, TEXT("LiveLinkRTTrPM_Source: Failed to create socket on %s:%d"), *ConnectionSettings.AdapterIP, ConnectionSettings.ListenPort);
        return false;
    }

    if (ConnectionSettings.bMulticast)
    {
        JoinMulticastGroup(UnicastAddress, MulticastAddress);
    }

    const FTimespan ThreadWaitTime = FTimespan::FromMilliseconds(100);
    UDPReceiver = MakeUnique<FUdpSocketReceiver>(Socket, ThreadWaitTime, TEXT("LiveLinkRTTrPM_UDPReceiver"));
    UDPReceiver->OnDataReceived().BindRaw(this, &FLiveLinkRTTrPM_Source::OnPacketReceived);
    UDPReceiver->SetMaxReadBufferSize(ReceiveBufferSize);
    UDPReceiver->SetThreadStackSize(512 * 1024);
    UDPReceiver->Start();

    return true;
}

void FLiveLinkRTTrPM_Source::StopUdpReceiver()
{
    if (UDPReceiver)
    {
        UDPReceiver->Stop();
    }
}

void FLiveLinkRTTrPM_Source::CloseSocket()
{
    StopUdpReceiver();
	UDPReceiver.Reset();

    if (Socket)
    {
        ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
        if (ensure(SocketSubsystem != nullptr))
        {
            SocketSubsystem->DestroySocket(Socket);
        }
        else
        {
            Socket->Close();
        }
        Socket = nullptr;
    }
}

bool FLiveLinkRTTrPM_Source::JoinMulticastGroup(const FIPv4Address& UnicastAddress, const FIPv4Address& MulticastAddress)
{
#if PLATFORM_SUPPORTS_UDP_MULTICAST_GROUP
    if (Socket == nullptr)
    {
        return false;
    }

    const FIPv4Endpoint MulticastEndpoint(MulticastAddress, ConnectionSettings.ListenPort);
    TSharedRef<FInternetAddr> MulticastAddr = MulticastEndpoint.ToInternetAddr();

    if (UnicastAddress == FIPv4Address::Any)
    {
        TArray<TSharedPtr<FInternetAddr>> LocalIps;
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetLocalAdapterAddresses(LocalIps);
        for (const TSharedPtr<FInternetAddr>& LocalIp : LocalIps)
        {
            DoJoinMulticastGroup(MulticastAddr, LocalIp, Socket);
        }

        if (LocalIps.Num() == 0)
        {
            bool bCanBindAll = false;
            DoJoinMulticastGroup(MulticastAddr, ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetLocalHostAddr(*GLog, bCanBindAll), Socket);
        }
    }
    else
    {
        const FIPv4Endpoint UnicastEndpoint(UnicastAddress, ConnectionSettings.ListenPort);
        DoJoinMulticastGroup(MulticastAddr, UnicastEndpoint.ToInternetAddr(), Socket);
    }

    return true;
#else
    return false;
#endif
}

void FLiveLinkRTTrPM_Source::OnPacketReceived(const FArrayReaderPtr& Data, const FIPv4Endpoint& Endpoint) {
    LastDataReadTime = FPlatformTime::Seconds();

    std::vector<UCHAR> dataVec(Data->GetData(), Data->GetData() + Data->NumBytes());
    RTTrP header(dataVec);
    if (header.fltHeader == 0x4334 || header.fltHeader == 0x3443) //RTTrPM
    {
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
                case 0x22: // Zone module
                {
                    ZoneMod* zoneMod = new ZoneMod(&dataVec, header.intHeader, header.fltHeader);
                    if (motionPacket.zoneSubMod == nullptr && zoneMod->numofZoneSubModules > 0) {
                        motionPacket.zoneSubMod = new std::vector<ZoneSubMod*>();
                        for (int zoneModeIdx = 0; zoneModeIdx < zoneMod->numofZoneSubModules; zoneModeIdx++) {
                            ZoneSubMod* subMod = new ZoneSubMod(&dataVec, header.intHeader);
                            motionPacket.zoneSubMod->push_back(subMod);
                        }
                    }
                    motionPacket.zoneMod = zoneMod;
                    break;
                }
                default:
                    UE_LOG(LogRTTrP, Warning, TEXT("LiveLinkRTTrPM_Source: Unknown module type: 0x%02X"), pkType);
                    return;
                }
            }

            FRTTrPM_Trackable trackable(motionPacket);
            SendTrackable(trackable);
        }
    }
}

void FLiveLinkRTTrPM_Source::SendTrackable(const FRTTrPM_Trackable& Trackable)
{
    if (Client == nullptr) {
        return;
    }

    FName SubjectName = FName(*Trackable.Name);

    if (!EncounteredSubjects.Contains(SubjectName)) {
        // Transform role static data
        FLiveLinkStaticDataStruct StaticData(FLiveLinkRTTrPM_StaticData::StaticStruct());
        Client->PushSubjectStaticData_AnyThread({ SourceGuid, SubjectName }, ULiveLinkRTTrPM_Role::StaticClass(), MoveTemp(StaticData));
        EncounteredSubjects.Add(SubjectName);
    }

    FLiveLinkFrameDataStruct FrameData(FLiveLinkRTTrPM_FrameData::StaticStruct());
    FLiveLinkRTTrPM_FrameData* TransformFrameData = FrameData.Cast<FLiveLinkRTTrPM_FrameData>();
    //Update transform position based on Source settings
    TransformFrameData->Transform = Trackable.Transform;
    TransformFrameData->CentroidPosition = Trackable.Transform.GetLocation();
    for (const FRTTrPM_LED& led : Trackable.LEDs) {
        switch (led.Index) {
        case 0:
            TransformFrameData->LED1Position = led.Position;
            break;
        case 1:
            TransformFrameData->LED2Position = led.Position;
            break;
        case 2:
            TransformFrameData->LED3Position = led.Position;
            break;
        default:
            break;
        }
    }
    const ERTTrPM_SubjectType TransformSource = (mSettings != nullptr)
        ? mSettings->TransformSource
        : ERTTrPM_SubjectType::Centroid;

    switch (TransformSource) {
    case ERTTrPM_SubjectType::Centroid:
        break;
    case ERTTrPM_SubjectType::LED1:
        TransformFrameData->Transform.SetLocation(TransformFrameData->LED1Position);
        break;
    case ERTTrPM_SubjectType::LED2:
        TransformFrameData->Transform.SetLocation(TransformFrameData->LED2Position);
        break;
    case ERTTrPM_SubjectType::LED3:
        TransformFrameData->Transform.SetLocation(TransformFrameData->LED3Position);
        break;
    default:
        break;
    }
    TransformFrameData->Zones.Append(Trackable.ActiveZones);
    Client->PushSubjectFrameData_AnyThread({ SourceGuid, SubjectName }, MoveTemp(FrameData));
}