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

void FLiveLinkRTTrPM_Source::OnSettingsChanged(ULiveLinkSourceSettings* Settings, const FPropertyChangedEvent& PropertyChangedEvent)
{
    ILiveLinkSource::OnSettingsChanged(Settings, PropertyChangedEvent);

    const FProperty* const MemberProperty = PropertyChangedEvent.MemberProperty;
    const FProperty* const Property = PropertyChangedEvent.Property;
    if (Property && MemberProperty && (PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive))
    {
        bResetRequested = true;
    }
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

//void FLiveLinkRTTrPM_Source::InitializeSettings(ULiveLinkSourceSettings* Settings)
//{
//
//}

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
    
    TRACE_CPUPROFILER_EVENT_SCOPE(RTTrPM::DataPacket);
    
    LastDataReadTime = FPlatformTime::Seconds();
    RTTrP_Header header(Data.ToSharedRef().Get());
    if (header.fltSig == RTTrPM_FLT_BE || header.fltSig == RTTrPM_FLT_LE) //RTTrPM
    {
        for (int trackableIndex = 0; trackableIndex < header.numMods; trackableIndex++) {

			RTTrPM_Trackable trackable(Data.ToSharedRef().Get(), header.intSig, header.fltSig);
            SendTrackable(trackable);
        }
    }
}

void FLiveLinkRTTrPM_Source::SendTrackable(const RTTrPM_Trackable& Trackable)
{
    if (Client == nullptr) {
        return;
    }

    FName SubjectName = FName(*Trackable.name);

    if (!EncounteredSubjects.Contains(SubjectName)) {
        // Transform role static data
        FLiveLinkStaticDataStruct StaticData(FLiveLinkRTTrPM_StaticData::StaticStruct());
        Client->PushSubjectStaticData_AnyThread({ SourceGuid, SubjectName }, ULiveLinkRTTrPM_Role::StaticClass(), MoveTemp(StaticData));
        EncounteredSubjects.Add(SubjectName);
    }

    FLiveLinkFrameDataStruct FrameData(FLiveLinkRTTrPM_FrameData::StaticStruct());
    FLiveLinkRTTrPM_FrameData* TransformFrameData = FrameData.Cast<FLiveLinkRTTrPM_FrameData>();
    //Update transform position based on Source settings
    TransformFrameData->Transform = Trackable.GetTransform();
    TransformFrameData->CentroidPosition = Trackable.GetTransform().GetLocation();
    for (const TTuple<uint8_t, RTTrPM_LED>& led : Trackable.LEDs) {
        switch (led.Key) {
        case 0:
            TransformFrameData->LED1Position = led.Value.GetPosition();
            break;
        case 1:
            TransformFrameData->LED2Position = led.Value.GetPosition();
            break;
        case 2:
            TransformFrameData->LED3Position = led.Value.GetPosition();
            break;
        default:
            break;
        }
    }

    TransformFrameData->Zones.Append(Trackable.zones);
    Client->PushSubjectFrameData_AnyThread({ SourceGuid, SubjectName }, MoveTemp(FrameData));
}
