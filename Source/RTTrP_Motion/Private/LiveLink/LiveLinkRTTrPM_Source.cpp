// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkRTTrPM_Source.h"
#include "LiveLinkRTTrPM_.h"
#include "ILiveLinkClient.h"
#include "Async/Async.h"
#include "LiveLinkRTTrPM_ConnectionSettings.h"
#include "Misc/CoreDelegates.h"
#include "LiveLinkRTTrPM_PacketInfo.h"
#include "Roles/LiveLinkCameraRole.h"

#include "Common/UdpSocketBuilder.h"
#include "Roles/LiveLinkCameraTypes.h"

#define LOCTEXT_NAMESPACE "LiveLinkRTTrPM_SourceFactory"

FLiveLinkRTTrPM_Source::FLiveLinkRTTrPM_Source(const FLiveLinkRTTrPM_ConnectionSettings& ConnectionSettings)
: Client(nullptr)
, Stopping(false)
, Thread(nullptr)
{
	SourceStatus = LOCTEXT("SourceStatus_NoData", "No data");
	SourceType = LOCTEXT("SourceType_RTTrPM_", "RTTrPM");
	SourceMachineName = FText::Format(LOCTEXT("RTTrPM_SourceMachineName", "{0}:{1}"), FText::FromString(ConnectionSettings.IPAddress), FText::AsNumber(ConnectionSettings.UDPPortNumber, &FNumberFormattingOptions::DefaultNoGrouping()));

	FIPv4Address::Parse(ConnectionSettings.IPAddress, DeviceEndpoint.Address);
	DeviceEndpoint.Port = ConnectionSettings.UDPPortNumber;

	Socket = FUdpSocketBuilder(TEXT("RTTrPM_ListenerSocket"))
		.AsNonBlocking()
		.AsReusable()
		.BoundToEndpoint(DeviceEndpoint)
		.WithReceiveBufferSize(ReceiveBufferSize);

	if ((Socket != nullptr) && (Socket->GetSocketType() == SOCKTYPE_Datagram))
	{
		ReceiveBuffer.SetNumUninitialized(ReceiveBufferSize);
		SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		DeferredStartDelegateHandle = FCoreDelegates::OnEndFrame.AddRaw(this, &FLiveLinkRTTrPM_Source::Start);

		UE_LOG(LogLiveLinkRTTrPM, Log, TEXT("LiveLinkRTTrPMSource: Opened UDP socket with IP address %s"), *DeviceEndpoint.ToString());
	}
	else
	{
		UE_LOG(LogLiveLinkRTTrPM, Error, TEXT("LiveLinkRTTrPMSource: Failed to open UDP socket with IP address %s"), *DeviceEndpoint.ToString());
	}
}

FLiveLinkRTTrPM_Source::~FLiveLinkRTTrPM_Source()
{
	// This could happen if the object is destroyed before FCoreDelegates::OnEndFrame calls FLiveLinkRTTrPM_Source::Start
	if (DeferredStartDelegateHandle.IsValid())
	{
		FCoreDelegates::OnEndFrame.Remove(DeferredStartDelegateHandle);
	}

	Stop();

	if (Thread != nullptr)
	{
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
	}

	if (Socket != nullptr)
	{
		Socket->Close();
		SocketSubsystem->DestroySocket(Socket);
		Socket = nullptr;
	}
}

void FLiveLinkRTTrPM_Source::ReceiveClient(ILiveLinkClient* InClient, FGuid InSourceGuid)
{
	Client = InClient;
	SourceGuid = InSourceGuid;
}

void FLiveLinkRTTrPM_Source::InitializeSettings(ULiveLinkSourceSettings* Settings)
{
	// Save our source settings pointer so we can use it directly
	SavedSourceSettings = Cast<ULiveLinkRTTrPM_SourceSettings>(Settings);
}

bool FLiveLinkRTTrPM_Source::IsSourceStillValid() const
{
	// Source is valid if we have a valid thread
	bool bIsSourceValid = !Stopping && (Thread != nullptr) && (Socket != nullptr);
	return bIsSourceValid;
}

bool FLiveLinkRTTrPM_Source::RequestSourceShutdown()
{
	Stop();

	return true;
}

//
// Specific manufacturer default data (we still use auto-ranging as the default)
//
// Name			UDP port	Zoom (wide)	(tele)		Focus (near)	(far)		Spare
// Generic		40000		0x0			0x10000		0x0				0x10000		Unused
// Panasonic	1111		0x555		0xfff		0x555			0xfff		Iris 0x555 (close) - 0xfff (open)
// Sony			40000		0x0			0xFFFFFF	0x7FFFFF		0x000000	Lower 12 bits - Iris (F value * 100); Upper 4 bits - Frame number
// Mosys		8001		0x0			0xffff		0x0				0xffff		Lower 8 bits - Tracking quality 0-3 (undef, good, caution, bad)
// Stype		6301		0x0			0xffffff	0x0				0xffffff	Unused
// Ncam			6301		0x0			0xffffff	0x0				0xffffff	Unused
//

void FLiveLinkRTTrPM_Source::OnSettingsChanged(ULiveLinkSourceSettings* Settings, const FPropertyChangedEvent& PropertyChangedEvent)
{
	ILiveLinkSource::OnSettingsChanged(Settings, PropertyChangedEvent);

	FProperty* MemberProperty = PropertyChangedEvent.MemberProperty;
	FProperty* Property = PropertyChangedEvent.Property;
	if (Property && MemberProperty && (PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive))
	{
		ULiveLinkRTTrPM_SourceSettings* SourceSettings = Cast<ULiveLinkRTTrPM_SourceSettings>(Settings);
		if (SavedSourceSettings != SourceSettings)
		{
			UE_LOG(LogLiveLinkRTTrPM, Error, TEXT("LiveLinkRTTrPMSource: OnSettingsChanged pointers don't match - this should never happen!"));
			return;
		}

		if (SourceSettings != nullptr)
		{
			static FName NAME_DefaultConfig = GET_MEMBER_NAME_CHECKED(ULiveLinkRTTrPM_SourceSettings, DefaultConfig);
			static FName NAME_FocusDistanceEncoderData = GET_MEMBER_NAME_CHECKED(ULiveLinkRTTrPM_SourceSettings, FocusDistanceEncoderData);
			static FName NAME_FocalLengthEncoderData = GET_MEMBER_NAME_CHECKED(ULiveLinkRTTrPM_SourceSettings, FocalLengthEncoderData);
			static FName NAME_UserDefinedEncoderData = GET_MEMBER_NAME_CHECKED(ULiveLinkRTTrPM_SourceSettings, UserDefinedEncoderData);
			const FName PropertyName = Property->GetFName();
			const FName MemberPropertyName = MemberProperty->GetFName();

			bool bFocusDistanceEncoderDataChanged = false;
			bool bFocalLengthEncoderDataChanged = false;
			bool bUserDefinedEncoderDataChanged = false;

			if (PropertyName == NAME_DefaultConfig)
			{
				bFocusDistanceEncoderDataChanged = true;
				bFocalLengthEncoderDataChanged = true;
				bUserDefinedEncoderDataChanged = true;
			}

			if (MemberPropertyName == NAME_FocusDistanceEncoderData)
			{
				bFocusDistanceEncoderDataChanged = true;
			}
			else if (MemberPropertyName == NAME_FocalLengthEncoderData)
			{
				bFocalLengthEncoderDataChanged = true;
			}
			else if (MemberPropertyName == NAME_UserDefinedEncoderData)
			{
				bUserDefinedEncoderDataChanged = true;
			}

			if (bFocusDistanceEncoderDataChanged)
			{
				UpdateEncoderData(&SourceSettings->FocusDistanceEncoderData);
			}

			if (bFocalLengthEncoderDataChanged)
			{
				UpdateEncoderData(&SourceSettings->FocalLengthEncoderData);
			}

			if (bUserDefinedEncoderDataChanged)
			{
				UpdateEncoderData(&SourceSettings->UserDefinedEncoderData);
			}
		}
	}
}

void FLiveLinkRTTrPM_Source::UpdateEncoderData(FRTTrPM_EncoderData* InEncoderData)
{
	if (InEncoderData->Min == InEncoderData->Max)
	{
		UE_LOG(LogLiveLinkRTTrPM, Error, TEXT("LiveLinkRTTrPM_Source: EncoderData Min/Max values can't be equal (you may need to rack your encoder) - incoming data may be invalid!"));
	}

	// Update any changed encoder data to reset min/max values for auto-ranging
	if (!InEncoderData->bUseManualRange)
	{
		InEncoderData->Min = InEncoderData->MaskBits;
		InEncoderData->Max = 0;
	}

	// Remove the static data from the EncounteredSubjects list so it will be automatically updated during the next Send()
	EncounteredSubjects.Remove(FName(CameraSubjectName));
}

// FRunnable interface
void FLiveLinkRTTrPM_Source::Start()
{
	check(DeferredStartDelegateHandle.IsValid());

	FCoreDelegates::OnEndFrame.Remove(DeferredStartDelegateHandle);
	DeferredStartDelegateHandle.Reset();

	SourceStatus = LOCTEXT("SourceStatus_Receiving", "Receiving");

	ThreadName = "LiveLinkRTTrPM_ Receiver ";
	ThreadName.AppendInt(FAsyncThreadIndex::GetNext());

	Thread = FRunnableThread::Create(this, *ThreadName, 128 * 1024, TPri_AboveNormal, FPlatformAffinity::GetPoolThreadMask());
}

void FLiveLinkRTTrPM_Source::Stop()
{
	Stopping = true;
}

uint32 FLiveLinkRTTrPM_Source::Run()
{
	// Free-D max data rate is 100Hz
	const FTimespan SocketTimeout(FTimespan::FromMilliseconds(10));

	while (!Stopping)
	{
		if (Socket && Socket->Wait(ESocketWaitConditions::WaitForRead, SocketTimeout))
		{
			uint32 PendingDataSize = 0;
			while (Socket && Socket->HasPendingData(PendingDataSize))
			{
				int32 ReceivedDataSize = 0;
				if (Socket && Socket->Recv(ReceiveBuffer.GetData(), ReceiveBufferSize, ReceivedDataSize))
				{
					if (ReceivedDataSize > 0)
					{
						if (SavedSourceSettings == nullptr)
						{
							UE_LOG(LogLiveLinkRTTrPM, Error, TEXT("LiveLinkRTTrPM_Source: Received a packet, but we don't have a valid SavedSourceSettings!"));
						}
						else if (ReceiveBuffer[RTTrPM_PacketDefinition::PacketType] == RTTrPM_PacketDefinition::PacketTypeD1)
						{
							// The only message that we care about is the 0xD1 message which contains PnO data, zoom, focus, and a user defined field (usually iris)
							uint8 CameraId = ReceiveBuffer[RTTrPM_PacketDefinition::CameraID];
							FRotator Orientation;
							Orientation.Yaw = Decode_Signed_8_15(&ReceiveBuffer[RTTrPM_PacketDefinition::Yaw]);
							Orientation.Pitch = Decode_Signed_8_15(&ReceiveBuffer[RTTrPM_PacketDefinition::Pitch]);
							Orientation.Roll = Decode_Signed_8_15(&ReceiveBuffer[RTTrPM_PacketDefinition::Roll]);

							// RTTrPM_ has the X and Y axes flipped from Unreal
							FVector Position;
							Position.X = Decode_Signed_17_6(&ReceiveBuffer[RTTrPM_PacketDefinition::Y]);
							Position.Y = Decode_Signed_17_6(&ReceiveBuffer[RTTrPM_PacketDefinition::X]);
							Position.Z = Decode_Signed_17_6(&ReceiveBuffer[RTTrPM_PacketDefinition::Z]);

							int32 FocalLengthInt = Decode_Unsigned_24(&ReceiveBuffer[RTTrPM_PacketDefinition::FocalLength]);
							int32 FocusDistanceInt = Decode_Unsigned_24(&ReceiveBuffer[RTTrPM_PacketDefinition::FocusDistance]);
							int32 UserDefinedDataInt = Decode_Unsigned_16(&ReceiveBuffer[RTTrPM_PacketDefinition::UserDefined]);

							float FocalLength = ProcessEncoderData(SavedSourceSettings->FocalLengthEncoderData, FocalLengthInt);
							float FocusDistance = ProcessEncoderData(SavedSourceSettings->FocusDistanceEncoderData, FocusDistanceInt);
							float UserDefinedData = ProcessEncoderData(SavedSourceSettings->UserDefinedEncoderData, UserDefinedDataInt);

							uint8 Checksum = CalculateChecksum(&ReceiveBuffer[RTTrPM_PacketDefinition::PacketType], ReceivedDataSize - 1);
							if (Checksum != ReceiveBuffer[RTTrPM_PacketDefinition::Checksum])
							{
								UE_LOG(LogLiveLinkRTTrPM_, Warning, TEXT("LiveLinkRTTrPM_Source: Received packet checksum error - received 0x%02x, calculated 0x%02x"), ReceiveBuffer[RTTrPM_PacketDefinition::Checksum], Checksum);
							}

							if (ReceivedDataSize != RTTrPM_PacketDefinition::PacketSizeD1)
							{
								UE_LOG(LogLiveLinkRTTrPM_, Warning, TEXT("LiveLinkRTTrPM_Source: Received packet length mismatch - received 0x%02x, calculated 0x%02x"), ReceivedDataSize, RTTrPM_PacketDefinition::PacketSizeD1);
							}

							FLiveLinkFrameDataStruct FrameData(FLiveLinkCameraFrameData::StaticStruct());
							FLiveLinkCameraFrameData* CameraFrameData = FrameData.Cast<FLiveLinkCameraFrameData>();
							CameraFrameData->Transform = FTransform(Orientation, Position);

							if (SavedSourceSettings->bSendExtraMetaData)
							{
								CameraFrameData->MetaData.StringMetaData.Add(FName(TEXT("CameraId")), FString::Printf(TEXT("%d"), CameraId));
								CameraFrameData->MetaData.StringMetaData.Add(FName(TEXT("FrameCounter")), FString::Printf(TEXT("%d"), FrameCounter));
							}

							if (SavedSourceSettings->FocalLengthEncoderData.bIsValid)
							{
								CameraFrameData->FocalLength = FocalLength;
							}
							if (SavedSourceSettings->FocusDistanceEncoderData.bIsValid)
							{
								CameraFrameData->FocusDistance = FocusDistance;
							}
							if (SavedSourceSettings->UserDefinedEncoderData.bIsValid)
							{
								CameraFrameData->Aperture = UserDefinedData;
							}

							CameraSubjectName = FString::Printf(TEXT("Camera %d"), CameraId);
							Send(&FrameData, FName(CameraSubjectName));

							FrameCounter++;
						}
						else
						{
							UE_LOG(LogLiveLinkRTTrPM_, Warning, TEXT("LiveLinkRTTrPM_Source: Unsupported RTTrPM_ message type 0x%02x"), ReceiveBuffer[RTTrPM_PacketDefinition::PacketType]);
						}
					}
				}
			}
		}
	}
	
	return 0;
}

void FLiveLinkRTTrPM_Source::Send(FLiveLinkFrameDataStruct* FrameDataToSend, FName SubjectName)
{
	if (Stopping || (Client == nullptr))
	{
		return;
	}

	if (!EncounteredSubjects.Contains(SubjectName))
	{
		FLiveLinkStaticDataStruct StaticData(FLiveLinkCameraStaticData::StaticStruct());

		FLiveLinkCameraStaticData& CameraData = *StaticData.Cast<FLiveLinkCameraStaticData>();
		CameraData.bIsFocusDistanceSupported = SavedSourceSettings->FocusDistanceEncoderData.bIsValid;
		CameraData.bIsFocalLengthSupported = SavedSourceSettings->FocalLengthEncoderData.bIsValid;
		CameraData.bIsApertureSupported = SavedSourceSettings->UserDefinedEncoderData.bIsValid;
		Client->PushSubjectStaticData_AnyThread({ SourceGuid, SubjectName }, ULiveLinkCameraRole::StaticClass(), MoveTemp(StaticData));
		EncounteredSubjects.Add(SubjectName);
	}

	Client->PushSubjectFrameData_AnyThread({ SourceGuid, SubjectName }, MoveTemp(*FrameDataToSend));
}

float FLiveLinkRTTrPM_Source::ProcessEncoderData(FRTTrPM_EncoderData& EncoderData, int32 RawEncoderValueInt)
{
	float FinalEncoderValue = 0.0f;

	if (EncoderData.bIsValid)
	{
		RawEncoderValueInt &= EncoderData.MaskBits;

		// Auto-range the input
		if (!EncoderData.bUseManualRange)
		{
			if (RawEncoderValueInt < EncoderData.Min)
			{
				EncoderData.Min = RawEncoderValueInt;
			}
			if (RawEncoderValueInt > EncoderData.Max)
			{
				EncoderData.Max = RawEncoderValueInt;
			}
		}

		int32 Delta = EncoderData.Max - EncoderData.Min;
		if (Delta != 0)
		{
			FinalEncoderValue = FMath::Clamp((float)(RawEncoderValueInt - EncoderData.Min) / (float)Delta, 0.0f, 1.0f);
			if (EncoderData.bInvertEncoder)
			{
				FinalEncoderValue = 1.0f - FinalEncoderValue;
			}
		}
	}

	return FinalEncoderValue;
}

float FLiveLinkRTTrPM_Source::Decode_Signed_8_15(uint8* InBytes)
{
	int32 ret = (*InBytes << 16) | (*(InBytes + 1) << 8) | *(InBytes + 2);
	if (*InBytes & 0x80)
	{
		ret -= 0x00ffffff;
	}
	return (float)ret / 32768.0f;
}

float FLiveLinkRTTrPM_Source::Decode_Signed_17_6(uint8* InBytes)
{
	int32 ret = (*InBytes << 16) | (*(InBytes + 1) << 8) | *(InBytes + 2);
	if (*InBytes & 0x80)
	{
		ret -= 0x00ffffff;
	}
	return (float)ret / 640.0f;
}

uint32 FLiveLinkRTTrPM_Source::Decode_Unsigned_24(uint8* InBytes)
{
	uint32 ret = (*InBytes << 16) | (*(InBytes + 1) << 8) | *(InBytes + 2);
	return ret;
}

uint16 FLiveLinkRTTrPM_Source::Decode_Unsigned_16(uint8* InBytes)
{
	uint16 ret = (*InBytes << 8) | *(InBytes + 1);
	return ret;
}

uint8 FLiveLinkRTTrPM_Source::CalculateChecksum(uint8* InBytes, uint32 Size)
{
	uint8 sum = 0x40;
	for (uint32 i = 0; i < Size; i++)
	{
		sum -= *(InBytes + i);
	}
	return sum;
}

#undef LOCTEXT_NAMESPACE
