#include "lighting.h"

#pragma once

#include "Misc/ByteSwap.h"

using namespace std;

LightingOutput::LightingOutput()
{
	this->uniList = NULL;
}

LightingOutput::LightingOutput(vector<unsigned char> *data, uint16_t intSig, uint16_t fltSig) : Packet(intSig, fltSig, data)
{
	copy(data->begin(), data->begin() + 2, (unsigned char*)&this->size);
	data->erase(data->begin(), data->begin() + 2);

	copy(data->begin(), data->begin() + 4, (unsigned char*)&this->lightSeuquence);
	data->erase(data->begin(), data->begin() + 4);

	copy(data->begin(), data->begin() + 1, (unsigned char*)&this->action);
	data->erase(data->begin(), data->begin() + 1);

	copy(data->begin(), data->begin() + 4, (unsigned char*)&this->holdTime);
	data->erase(data->begin(), data->begin() + 4);

	copy(data->begin(), data->begin() + 2, (unsigned char*)&this->numUniverses);
	data->erase(data->begin(), data->begin() + 2);

	if (this->intSig == 0x4154)
	{
		this->size = BYTESWAP_ORDER16(this->size);
		this->lightSeuquence = BYTESWAP_ORDER32(this->lightSeuquence);
		this->holdTime = BYTESWAP_ORDER32(this->holdTime);
		this->numUniverses = BYTESWAP_ORDER16(this->numUniverses);
	}

}

LightingOutput::~LightingOutput()
{
	delete(this->uniList);
}

void LightingOutput::printModule(void)
{
	cout << "==================Lighting Output==================" << endl;
	cout << "Size: " << dec << this->size << endl;
	cout << "Session Sequence: " << this->lightSeuquence << endl;
	cout << "Action: " << this->action << endl;
	cout << "Hold Time: " << this->holdTime << endl;
	cout << "Number of Universes: " << this->numUniverses << endl;
	cout << "===================================================" << endl;
}

LightingSync::LightingSync()
{
}

LightingSync::LightingSync(vector<unsigned char> *data, uint16_t intSig, uint16_t fltSig) : Packet(intSig, fltSig, data)
{
	copy(data->begin(), data->begin() + 2, (unsigned char *)&this->size);
	data->erase(data->begin(), data->begin() + 2);

	copy(data->begin(), data->begin() + 4, (unsigned char *)&this->deviceID);
	data->erase(data->begin(), data->begin() + 4);

	copy(data->begin(), data->begin() + 4, (unsigned char *)&this->deviceSubID0);
	data->erase(data->begin(), data->begin() + 4);

	copy(data->begin(), data->begin() + 4, (unsigned char *)&this->deviceSubID1);
	data->erase(data->begin(), data->begin() + 4);

	copy(data->begin(), data->begin() + 4, (unsigned char *)&this->seqNum);
	data->erase(data->begin(), data->begin() + 4);

	if (this->intSig == 0x4154)
	{
		this->size = BYTESWAP_ORDER16(this->size);
		this->deviceID = BYTESWAP_ORDER32(this->deviceID);
		this->deviceSubID0 = BYTESWAP_ORDER32(this->deviceSubID0);
		this->deviceSubID1 = BYTESWAP_ORDER32(this->deviceSubID1);
		this->seqNum = BYTESWAP_ORDER32(this->seqNum);
	}
}

LightingSync::~LightingSync()
{

}

void LightingSync::printModule(void)
{
	cout << "==================Lighting Sync==================" << endl;
	cout << "Size: " << this->size << endl;
	cout << "Device ID: " << this->deviceID << endl;
	cout << "Device Sub ID 0: " << this->deviceSubID0 << endl;
	cout << "Device Sub ID 1: " << this->deviceSubID1 << endl;
	cout << "Sequence Number: " << this->seqNum << endl;
	cout << "==================================================" << endl;
}

Universe::Universe()
{
	this->spotList = NULL;
}

Universe::Universe(vector<unsigned char> *data, uint16_t intSig, uint16_t fltSig) : Packet(intSig, fltSig, data)
{
	copy(data->begin(), data->begin() + 2, (unsigned char*)&this->size);
	data->erase(data->begin(), data->begin() + 2);

	copy(data->begin(), data->begin() + 2, (unsigned char*)&this->universeID);
	data->erase(data->begin(), data->begin() + 2);

	copy(data->begin(), data->begin() + 2, (unsigned char*)&this->numSpots);
	data->erase(data->begin(), data->begin() + 2);

	if (this->intSig == 0x4154)
	{
		this->size = BYTESWAP_ORDER16(this->size);
		this->universeID = BYTESWAP_ORDER16(this->universeID);
		this->numSpots = BYTESWAP_ORDER16(this->numSpots);
	}
}

Universe::~Universe()
{
	delete(this->spotList);
}

void Universe::printModule(void)
{
	cout << "==================Universe==================" << endl;
	cout << "Size: " << this->size << endl;
	cout << "Universe ID: " << this->universeID << endl;
	cout << "Number of Spots: " << this->numSpots << endl;
	cout << "=============================================" << endl;
}

Spot::Spot()
{
	this->chanBlocks = NULL;
}

Spot::Spot(vector<unsigned char> *data, uint16_t intSig, uint16_t fltSig) : Packet(intSig, fltSig, data)
{
	copy(data->begin(), data->begin() + 2, (unsigned char*)&this->size);
	data->erase(data->begin(), data->begin() + 2);

	copy(data->begin(), data->begin() + 2, (unsigned char*)&this->spotID);
	data->erase(data->begin(), data->begin() + 2);

	copy(data->begin(), data->begin() + 2, (unsigned char*)&this->spotOffset);
	data->erase(data->begin(), data->begin() + 2);

	copy(data->begin(), data->begin() + 2, (unsigned char*)&this->channelStruct);
	data->erase(data->begin(), data->begin() + 2);

	if (this->intSig == 0x4154)
	{
		this->size = BYTESWAP_ORDER16(this->size);
		this->spotID = BYTESWAP_ORDER16(this->spotID);
		this->spotOffset = BYTESWAP_ORDER16(this->spotOffset);
		this->channelStruct = BYTESWAP_ORDER16(this->channelStruct);
	}

	if (this->chanBlocks == NULL)
		this->chanBlocks = new vector<ChannelBlock*>(this->channelStruct);
}

Spot::~Spot()
{
	delete(this->chanBlocks);
}

void Spot::printModule(void)
{
	cout << "==================Spot=======================" << endl;
	cout << "Size: " << this->size << endl;
	cout << "Spot ID: " << this->spotID << endl;
	cout << "Spot Offset: " << this->spotOffset << endl;
	cout << "Number of Channel Blocks: " << this->channelStruct << endl;
	cout << "=============================================" << endl;
}

ChannelBlock::ChannelBlock()
{
}

ChannelBlock::ChannelBlock(vector<unsigned char> *data, uint16_t intSig, uint16_t fltSig)
{
	this->intSig = intSig;
	this->fltSig = fltSig;
	
	copy(data->begin(), data->begin() + 2, (unsigned char*)&this->chanOffset);
	data->erase(data->begin(), data->begin() + 2);

	copy(data->begin(), data->begin() + 2, (unsigned char*)&this->xFade);
	data->erase(data->begin(), data->begin() + 2);

	copy(data->begin(), data->begin() + 1, (unsigned char*)&this->value);
	data->erase(data->begin(), data->begin() + 1);

	if (this->intSig == 0x4154)
	{
		this->chanOffset = BYTESWAP_ORDER16(this->chanOffset);
		this->xFade = BYTESWAP_ORDER16(this->xFade);
	}
}

ChannelBlock::~ChannelBlock()
{

}

void ChannelBlock::printModule(void)
{
	cout << "==================Channel Block==============" << endl;
	cout << "Channel Offset: " << this->chanOffset << endl;
	cout << "X-Fade: " << this->xFade << endl;
	cout << "Value " << this->value << endl;
	cout << "=============================================" << endl;
}
