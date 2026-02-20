// Fill out your copyright notice in the Description page of Project Settings.


#include "LiveLink/LiveLinkRTTrPM_Source.h"
#include "Common/UdpSocketBuilder.h"
#include "RTTrP_types.h"
#include "LiveLink/LiveLinkRTTrPM_Role.h"
#include "SocketSubsystem.h"
#include "Async/Async.h"
#include "LiveLink/LiveLinkRTTrPM_DataTypes.h"

FLiveLinkRTTrPM_Source::FLiveLinkRTTrPM_Source(const FLiveLinkRTTrPM_ConnectionSettings& InConnectionSettings)
    : ConnectionSettings(InConnectionSettings)
    , Client(nullptr)
    , Socket(nullptr)
    , UDPReceiver(nullptr)
    , bIsConnected(false)
{
    SourceStatus = FText::FromString(TEXT("No data"));
    SourceType = FText::FromString(TEXT("RTTrPM"));

    FString MachineNameString = FString::Printf(TEXT("RTTrPM@%s:%d"), *ConnectionSettings.AdapterIP, ConnectionSettings.ListenPort);
    SourceMachineName = FText::FromString(MachineNameString);
    Start();
}

FLiveLinkRTTrPM_Source::~FLiveLinkRTTrPM_Source()
{
    Stop();
}

void FLiveLinkRTTrPM_Source::ReceiveClient(ILiveLinkClient* InClient, FGuid InSourceGuid)
{
    Client = InClient;
    SourceGuid = InSourceGuid;
}

bool FLiveLinkRTTrPM_Source::IsSourceStillValid() const
{
    return bIsConnected;
}

bool FLiveLinkRTTrPM_Source::RequestSourceShutdown()
{
    Stop();
    return true;
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
    mSettings->TransformSource = ERTTrPM_SubjectType::Centroid;
}

uint32 FLiveLinkRTTrPM_Source::Run()
{
    // Not used - we rely on FUdpSocketReceiver callbacks
    return 0;
}

void FLiveLinkRTTrPM_Source::Start()
{
    if (bIsConnected)
    {
        UE_LOG(LogRTTrP, Warning, TEXT("LiveLinkRTTrPM_Source: Already connected."));
        return;
    }

    FIPv4Address ipv4;
    if (!FIPv4Address::Parse(ConnectionSettings.AdapterIP, ipv4)) {
        UE_LOG(LogRTTrP, Error, TEXT("LiveLinkRTTrPM_Source: Invalid Adapter IP address: %s"), *ConnectionSettings.AdapterIP);
        return;
    }

    FIPv4Address multiGroup;
    if (!FIPv4Address::Parse(ConnectionSettings.MulticastIP, multiGroup)) {
        UE_LOG(LogRTTrP, Error, TEXT("LiveLinkRTTrPM_Source: Invalid Multicast IP address: %s"), *ConnectionSettings.MulticastIP);
        return;
    }

    FUdpSocketBuilder SocketBuilder(TEXT("LiveLinkRTTrPM_Socket"));
    SocketBuilder.BoundToAddress(ipv4);
    SocketBuilder.BoundToPort(ConnectionSettings.ListenPort).AsNonBlocking().AsReusable();
    if (ConnectionSettings.bMulticast) {
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
    if (Socket == nullptr) {
        UE_LOG(LogRTTrP, Error, TEXT("LiveLinkRTTrPM_Source: Failed to create socket on %s:%d"), *ConnectionSettings.AdapterIP, ConnectionSettings.ListenPort);
        return;
    }

    UDPReceiver = new FUdpSocketReceiver(
        Socket,
        FTimespan::FromMilliseconds(1000),
        TEXT("LiveLinkRTTrPM_UDPReceiver")
    );
    UDPReceiver->OnDataReceived().BindRaw(this, &FLiveLinkRTTrPM_Source::OnPacketReceived);
    UDPReceiver->Start();

    bIsConnected = true;
    SourceStatus = FText::FromString(TEXT("Receiving"));

    if (ConnectionSettings.bMulticast) {
        UE_LOG(LogRTTrP, Log, TEXT("LiveLinkRTTrPM_Source: Connected to multicast group %s:%d on %s"), *ConnectionSettings.MulticastIP, ConnectionSettings.ListenPort, *ConnectionSettings.AdapterIP);
    }
    else {
        UE_LOG(LogRTTrP, Log, TEXT("LiveLinkRTTrPM_Source: Connected to RTTrP host on %s:%d"), *ConnectionSettings.AdapterIP, ConnectionSettings.ListenPort);
    }
}

void FLiveLinkRTTrPM_Source::Stop()
{
    if (!bIsConnected)
    {
        return;
    }

    FIPv4Endpoint tmpEndpoint;
    if (Socket != nullptr) {
        if (UDPReceiver != nullptr) {
            UDPReceiver->Stop();
            delete UDPReceiver;
            UDPReceiver = nullptr;
        }
        Socket->Close();
        delete Socket;
        Socket = nullptr;
    }

    bIsConnected = false;
    SourceStatus = FText::FromString(TEXT("Stopped"));
    UE_LOG(LogRTTrP, Log, TEXT("LiveLinkRTTrPM_Source: Disconnected."));
}

void FLiveLinkRTTrPM_Source::OnPacketReceived(const FArrayReaderPtr& Data, const FIPv4Endpoint& Endpoint) {
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
    switch (mSettings->TransformSource) {
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

    Client->PushSubjectFrameData_AnyThread({ SourceGuid, SubjectName }, MoveTemp(FrameData));
}