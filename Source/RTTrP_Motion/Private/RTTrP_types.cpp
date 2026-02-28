// Fill out your copyright notice in the Description page of Project Settings.

#include "RTTrP_types.h"
#include "Serialization/ArrayReader.h"

DEFINE_LOG_CATEGORY(LogRTTrP);

//TODO: Replace Byteswap orderEnum with INTEL_ORDER / NETWORK_ORDER for platform agnostic behaviour
//TODO: handle byteswap through FArchive byteorder?

RTTrP_Header::RTTrP_Header(FArrayReader& data)
{
	data << intSig;
	data << fltSig;
	intSig = BYTESWAP_ORDER16(intSig);
	fltSig = BYTESWAP_ORDER16(fltSig);

	data << version;
	data << pID;
	data << pForm;
	data << pktSize;
	data << context;
	data << numMods;

	if(intSig == RTTrP_INT_BE) // RTTrPM
	{
		version = BYTESWAP_ORDER16(version);
		pID = BYTESWAP_ORDER32(pID);
		pktSize = BYTESWAP_ORDER16(pktSize);
		context = BYTESWAP_ORDER32(context);
	}
}

RTTrPM_Trackable::RTTrPM_Trackable(FArrayReader& data, uint16_t intSig, uint16_t fltSig)
{
	data << pkType;
	data << size;
	data << nameLen;

	ANSICHAR* nameBytes = new ANSICHAR[nameLen + 1];
	data.Serialize(nameBytes, nameLen);
	nameBytes[nameLen] = 0; // Null termination TODO: is this neccessary?
	name = FString(nameBytes);
	delete[] nameBytes;

	if (pkType == RTTrP_PacketType::Trackable_TS) //Trackable with timestamp
	{
		data << timeStamp;
	}
	data << numMods;
	
	if (intSig == RTTrP_INT_BE) {
		size = BYTESWAP_ORDER16(size);
		timeStamp = BYTESWAP_ORDER32(timeStamp);
	}

	for (int modIdx = 0; modIdx < numMods; modIdx++) {
		uint8_t modType;
		data << modType;
		switch (modType) {
		case RTTrP_PacketType::Centroid_Pos: // Centroid Module
		case RTTrP_PacketType::Centroid_AccVel: // Centroid AccVel Module
			centroid.Update(data, modType, intSig, fltSig);
			break;
		case RTTrP_PacketType::Orientation_Quat: // Quaternion Module
		case RTTrP_PacketType::Orientation_Euler: // Euler Module
			orientation.Update(data, modType, intSig, fltSig);
			break;
		case RTTrP_PacketType::LED_Pos: // LED Module
		case RTTrP_PacketType::LED_AccVel: // LED AccVel Module
		{
			RTTrPM_LED newLed;
			newLed.Update(data, modType, intSig, fltSig);
			if (LEDs.Contains(newLed.index)) {
				LEDs[newLed.index].Update(newLed);
			}
			else {
				LEDs.Add(newLed.index, newLed);
			}
		}
			break;
		case RTTrP_PacketType::Zone: // Zone module
			uint16_t zoneModSize;
			uint8_t zoneCount;
			data << zoneModSize;
			data << zoneCount;
			if (intSig == RTTrP_INT_BE)
				zoneModSize = BYTESWAP_ORDER16(zoneModSize);
			for (int zoneIdx = 0; zoneIdx < zoneCount; zoneIdx++) {
				uint8_t zoneSubmodSize, zoneNameLength;
				data << zoneSubmodSize;
				data << zoneNameLength;

				nameBytes = new ANSICHAR[zoneNameLength + 1];
				data.Serialize(nameBytes, zoneNameLength);
				nameBytes[zoneNameLength] = 0; // Null termination TODO: is this neccessary?
				zones.Add(FString(nameBytes));
				delete[] nameBytes;
			}
			break;
		}

	}
}

void RTTrPM_Centroid::Update(FArrayReader& data, uint8_t _pkType, uint16_t intSig, uint16_t fltSig)
{
	this->pkType = _pkType;
	data << size;
	if (_pkType == RTTrP_PacketType::Centroid_Pos)
		data << latency;
	data << x;
	data << y;
	data << z;
	if (fltSig == RTTrPM_FLT_BE) { //swap
		x = BYTESWAP_ORDERD(x);
		y = BYTESWAP_ORDERD(y);
		z = BYTESWAP_ORDERD(z);
	}
	if (intSig == RTTrP_INT_BE) {
		size = BYTESWAP_ORDER16(size);
	}
	if (_pkType == RTTrP_PacketType::Centroid_AccVel) {
		data << accx;
		data << accy;
		data << accz;
		data << velx;
		data << vely;
		data << velz;
		if (fltSig == RTTrPM_FLT_BE) {
			accx = BYTESWAP_ORDERF(accx);
			accy = BYTESWAP_ORDERF(accy);
			accz = BYTESWAP_ORDERF(accz);
			velx = BYTESWAP_ORDERF(velx);
			vely = BYTESWAP_ORDERF(vely);
			velz = BYTESWAP_ORDERF(velz);
		}
	}
}

void RTTrPM_LED::Update(FArrayReader& data, uint8_t _pkType, uint16_t intSig, uint16_t fltSig)
{
	this->pkType = _pkType;
	data << size;
	if (_pkType == RTTrP_PacketType::LED_Pos)
		data << latency;
	data << x;
	data << y;
	data << z;
	if (fltSig == RTTrPM_FLT_BE) { //swap
		x = BYTESWAP_ORDERD(x);
		y = BYTESWAP_ORDERD(y);
		z = BYTESWAP_ORDERD(z);
	}
	if (intSig == RTTrP_INT_BE) {
		size = BYTESWAP_ORDER16(size);
	}
	if (_pkType == RTTrP_PacketType::LED_AccVel) {
		data << accx;
		data << accy;
		data << accz;
		data << velx;
		data << vely;
		data << velz;
		if (fltSig == RTTrPM_FLT_BE) {
			accx = BYTESWAP_ORDERF(accx);
			accy = BYTESWAP_ORDERF(accy);
			accz = BYTESWAP_ORDERF(accz);
			velx = BYTESWAP_ORDERF(velx);
			vely = BYTESWAP_ORDERF(vely);
			velz = BYTESWAP_ORDERF(velz);
		}
	}
	data << index;
}

void RTTrPM_LED::Update(RTTrPM_LED& other)
{
	ensure(index == -1 || other.index == index);

	x = other.x;
	y = other.y;
	z = other.z;

	if (other.pkType == RTTrP_PacketType::LED_Pos) {
		latency = other.latency;
	}
	else {
		accx = other.accx;
		accy = other.accy;
		accz = other.accz;
		velx = other.velx;
		vely = other.vely;
		velz = other.velz;
	}
}

void RTTrPM_Orientation::Update(FArrayReader& data, uint8_t _pkType, uint16_t intSig, uint16_t fltSig)
{
	this->pkType = _pkType;
	data << size;
	data << latency;
	if (intSig == RTTrP_INT_BE) {
		size = BYTESWAP_ORDER16(size);
		latency = BYTESWAP_ORDER16(latency);
	}
	if (_pkType == RTTrP_PacketType::Orientation_Euler) {
		data << eulerOrder;
		if (intSig == RTTrP_INT_BE)
			eulerOrder = BYTESWAP_ORDER16(eulerOrder);
		data << R1;
		data << R2;
		data << R3;
		if (fltSig == RTTrPM_FLT_BE) {
			R1 = BYTESWAP_ORDERD(R1);
			R2 = BYTESWAP_ORDERD(R2);
			R3 = BYTESWAP_ORDERD(R3);
		}
		double rad2deg = FMath::RadiansToDegrees(1);
		R1 *= rad2deg;
		R2 *= rad2deg;
		R3 *= rad2deg;

		//Update quaternion values based on euler angles
		RTTrPM_EulerOrder orderEnum = (RTTrPM_EulerOrder)eulerOrder;
		FQuat calculatedQuat = AnimationCore::QuatFromEuler(FVector(R1, R2, R3), ConvertEulerOrder(orderEnum), true);
		Qx = calculatedQuat.X;
		Qy = calculatedQuat.Y;
		Qz = calculatedQuat.Z;
		Qw = calculatedQuat.W;
	}
	else {
		data << Qx;
		data << Qy;
		data << Qz;
		data << Qw;
		if (fltSig == RTTrPM_FLT_BE) {
			Qx = BYTESWAP_ORDERD(Qx);
			Qy = BYTESWAP_ORDERD(Qy);
			Qz = BYTESWAP_ORDERD(Qz);
			Qw = BYTESWAP_ORDERD(Qw);
		}

		//Update euler angles based on quaternion values
		FQuat quat = FQuat(Qx, Qy, Qz, Qw);
		FRotator rotator = quat.Rotator();
		R1 = rotator.Roll;
		R2 = rotator.Pitch;
		R3 = rotator.Yaw;
		eulerOrder = (uint16_t)RTTrPM_EulerOrder::ZYX; //Default Unreal rotation order ZYX
	}
}
