/*
 *  network_data_formats.cpp

	Copyright (C) 2001 and beyond by Woody Zenfell, III
	and the "Aleph One" developers.
 
	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	This license is contained in the file "COPYING",
	which is included with this source code; it is available online at
	http://www.gnu.org/licenses/gpl.html

 *  Created by Woody Zenfell, III on Thu Oct 11 2001.

	16 Jan 2002 (Loren Petrich): Added packing/unpacking functions
 */

#include "network_data_formats.h"
#include "Packing.h"

#pragma mark game_info

void
netcpy(game_info_NET* dest, const game_info* src)
{
	uint8 *S = dest->data;
	ValueToStream(S,src->initial_random_seed);
	ValueToStream(S,src->net_game_type);
	ValueToStream(S,src->time_limit);
	ValueToStream(S,src->kill_limit);
	ValueToStream(S,src->game_options);
	ValueToStream(S,src->difficulty_level);
	*(S++) = src->server_is_playing ? 1 : 0;
	*(S++) = src->allow_mic ? 1 : 0;
	ValueToStream(S,src->level_number);
	BytesToStream(S,src->level_name,sizeof(src->level_name));
	ValueToStream(S,src->initial_updates_per_packet);
	ValueToStream(S,src->initial_update_latency);
	assert(S == dest->data + SIZEOF_game_info);
}

void
netcpy(game_info* dest, const game_info_NET* src)
{
 	uint8 *S = (uint8 *)src->data;
	StreamToValue(S,dest->initial_random_seed);
	StreamToValue(S,dest->net_game_type);
	StreamToValue(S,dest->time_limit);
	StreamToValue(S,dest->kill_limit);
	StreamToValue(S,dest->game_options);
	StreamToValue(S,dest->difficulty_level);
	dest->server_is_playing = *(S++) != 0;
	dest->allow_mic = *(S++) != 0;
	StreamToValue(S,dest->level_number);
	StreamToBytes(S,dest->level_name,sizeof(dest->level_name));
	StreamToValue(S,dest->initial_updates_per_packet);
	StreamToValue(S,dest->initial_update_latency);
	assert(S == src->data + SIZEOF_game_info);
}

#pragma mark -





#pragma mark player_info

void
netcpy(player_info_NET* dest, const player_info* src)
{
	uint8 *S = dest->data;
	BytesToStream(S,src->name,sizeof(src->name));
	ValueToStream(S,src->desired_color);
	ValueToStream(S,src->team);
	ValueToStream(S,src->color);
	BytesToStream(S,src->long_serial_number,sizeof(src->long_serial_number));
	assert(S == dest->data + SIZEOF_player_info);
}

void
netcpy(player_info* dest, const player_info_NET* src)
{
	uint8 *S = (uint8 *)src->data;
	StreamToBytes(S,dest->name,sizeof(dest->name));
	StreamToValue(S,dest->desired_color);
	StreamToValue(S,dest->team);
	StreamToValue(S,dest->color);
	StreamToBytes(S,dest->long_serial_number,sizeof(dest->long_serial_number));
	assert(S == src->data + SIZEOF_player_info);
}

#pragma mark -





#pragma mark NetPacketHeader

void
netcpy(NetPacketHeader_NET* dest, const NetPacketHeader* src)
{
	uint8 *S = dest->data;
	ValueToStream(S,src->tag);
	ValueToStream(S,src->sequence);
	assert(S == dest->data + SIZEOF_NetPacketHeader);
}

void
netcpy(NetPacketHeader* dest, const NetPacketHeader_NET* src)
{
	uint8 *S = (uint8 *)src->data;
	StreamToValue(S,dest->tag);
	StreamToValue(S,dest->sequence);
	assert(S == src->data + SIZEOF_NetPacketHeader);
}

#pragma mark -





#pragma mark NetPacket

void
netcpy(NetPacket_NET* dest, const NetPacket* src)
{
	uint8 *S = dest->data;
	*(S++) = src->ring_packet_type;
	*(S++) = src->server_player_index;
	ValueToStream(S,src->server_net_time);
	ValueToStream(S,src->required_action_flags);
	for (int i=0; i<MAXIMUM_NUMBER_OF_NETWORK_PLAYERS; i++)
		ValueToStream(S,src->action_flag_count[i]);
	assert(S == dest->data + SIZEOF_NetPacket);
}

void
netcpy(NetPacket* dest, const NetPacket_NET* src)
{
	uint8 *S = (uint8 *)src->data;
	dest->ring_packet_type = *(S++);
	dest->server_player_index = *(S++);
	StreamToValue(S,dest->server_net_time);
	StreamToValue(S,dest->required_action_flags);
	for (int i=0; i<MAXIMUM_NUMBER_OF_NETWORK_PLAYERS; i++)
		StreamToValue(S,dest->action_flag_count[i]);
	assert(S == src->data + SIZEOF_NetPacket);
}

#pragma mark -





#pragma mark Action Flags

// (if not ALEPHONE_LITTLE_ENDIAN, this is unnecessary as memcpy is used instead.)
#ifdef ALEPHONE_LITTLE_ENDIAN
void
netcpy(uint32* dest, const uint32* src, size_t length) {
    assert(length % sizeof(uint32) == 0);
    
    int	num_items = length / sizeof(uint32);
	
	uint8 *S = (uint8 *)dest;
	ListToStream(S,src,num_items);
	assert(S == (uint8 *)dest + length);
}
#endif

#pragma mark -





#pragma mark NetDistributionPacket

void
netcpy(NetDistributionPacket_NET* dest, const NetDistributionPacket* src)
{
	uint8 *S = dest->data;
	ValueToStream(S,src->distribution_type);
	ValueToStream(S,src->first_player_index);
	ValueToStream(S,src->data_size);
	assert(S == dest->data + SIZEOF_NetDistributionPacket);
}

void
netcpy(NetDistributionPacket* dest, const NetDistributionPacket_NET* src)
{
	uint8 *S = (uint8 *)src->data;
	StreamToValue(S,dest->distribution_type);
	StreamToValue(S,dest->first_player_index);
	StreamToValue(S,dest->data_size);
	assert(S == src->data + SIZEOF_NetDistributionPacket);
}

#pragma mark -





#pragma mark IPaddress

// IP addresses are always in network byte order - do not swap
void
netcpy(IPaddress_NET* dest, const IPaddress* src)
{
	uint8 *S = dest->data;
	memcpy(S,&src->host,4);	// 32-bit integer
	S += 4;
	memcpy(S,&src->port,2);	// 16-bit integer
}

void
netcpy(IPaddress* dest, const IPaddress_NET* src) {
	uint8 *S = (uint8 *)src->data;
	memcpy(&dest->host,S,4);	// 32-bit integer
	S += 4;
	memcpy(&dest->port,S,2);	// 16-bit integer
}

#pragma mark -





#pragma mark NetPlayer

void
netcpy(NetPlayer_NET* dest, const NetPlayer* src)
{
	uint8 *S = dest->data;
	IPaddress_NET TempIPAddr;
	netcpy(&TempIPAddr,&src->dspAddress);
	BytesToStream(S,&TempIPAddr.data,SIZEOF_IPaddress);
	netcpy(&TempIPAddr,&src->ddpAddress);
	BytesToStream(S,&TempIPAddr.data,SIZEOF_IPaddress);
	ValueToStream(S,src->identifier);
	*(S++) = src->net_dead ? 1 : 0;
	BytesToStream(S,src->player_data,MAXIMUM_PLAYER_DATA_SIZE);
	assert(S == dest->data + SIZEOF_NetPlayer);
}

void
netcpy(NetPlayer* dest, const NetPlayer_NET* src)
{
	uint8 *S = (uint8 *)src->data;
	IPaddress_NET TempIPAddr;
	StreamToBytes(S,&TempIPAddr.data,SIZEOF_IPaddress);
	netcpy(&dest->dspAddress,&TempIPAddr);
	StreamToBytes(S,&TempIPAddr.data,SIZEOF_IPaddress);
	netcpy(&dest->ddpAddress,&TempIPAddr);
	StreamToValue(S,dest->identifier);
	dest->net_dead = *(S++) != 0;
	StreamToBytes(S,dest->player_data,MAXIMUM_PLAYER_DATA_SIZE);
	assert(S == src->data + SIZEOF_NetPlayer);
}

#pragma mark -





#pragma mark NetTopology

void
netcpy(NetTopology_NET* dest, const NetTopology* src)
{
	uint8 *S = dest->data;
	ValueToStream(S,src->tag);
	ValueToStream(S,src->player_count);
	ValueToStream(S,src->nextIdentifier);
	BytesToStream(S,src->game_data,MAXIMUM_GAME_DATA_SIZE);
	for (int i=0; i<MAXIMUM_NUMBER_OF_NETWORK_PLAYERS; i++)
	{
		NetPlayer_NET TempPlyrData;
		netcpy(&TempPlyrData,&src->players[i]);
		BytesToStream(S,TempPlyrData.data,SIZEOF_NetPlayer);
	}
	assert(S == dest->data + SIZEOF_NetTopology);
}

void
netcpy(NetTopology* dest, const NetTopology_NET* src)
{
	uint8 *S = (uint8 *)src->data;
	StreamToValue(S,dest->tag);
	StreamToValue(S,dest->player_count);
	StreamToValue(S,dest->nextIdentifier);
	StreamToBytes(S,dest->game_data,MAXIMUM_GAME_DATA_SIZE);
	for (int i=0; i<MAXIMUM_NUMBER_OF_NETWORK_PLAYERS; i++)
	{
		NetPlayer_NET TempPlyrData;
		StreamToBytes(S,TempPlyrData.data,SIZEOF_NetPlayer);
		netcpy(&dest->players[i],&TempPlyrData);
	}
	assert(S == src->data + SIZEOF_NetTopology);
}

#pragma mark -





#pragma mark gather_player_data

void
netcpy(gather_player_data_NET* dest, const gather_player_data* src)
{
	uint8 *S = dest->data;
	ValueToStream(S,src->new_local_player_identifier);
	assert(S == dest->data + SIZEOF_gather_player_data);
}

void
netcpy(gather_player_data* dest, const gather_player_data_NET* src)
{
	uint8 *S = (uint8 *)src->data;
	StreamToValue(S,dest->new_local_player_identifier);
	assert(S == src->data + SIZEOF_gather_player_data);
}

#pragma mark -





#pragma mark accept_gather_data

void
netcpy(accept_gather_data_NET* dest, const accept_gather_data* src)
{
	uint8 *S = dest->data;
	*(S++) = src->accepted ? 1 : 0;
	NetPlayer_NET TempPlyrData;
	netcpy(&TempPlyrData,&src->player);
	BytesToStream(S,TempPlyrData.data,SIZEOF_NetPlayer);
	assert(S == dest->data + SIZEOF_accept_gather_data);
}

void
netcpy(accept_gather_data* dest, const accept_gather_data_NET* src)
{
	uint8 *S = (uint8 *)src->data;
	dest->accepted = *(S++) != 0;
	NetPlayer_NET TempPlyrData;
	StreamToBytes(S,TempPlyrData.data,SIZEOF_NetPlayer);
	netcpy(&dest->player,&TempPlyrData);
	assert(S == src->data + SIZEOF_accept_gather_data);
}

#pragma mark -





#ifdef NETWORK_CHAT
#pragma mark NetChatMessage

void
netcpy(NetChatMessage_NET* dest, const NetChatMessage* src)
{
	uint8 *S = dest->data;
	ValueToStream(S,src->sender_identifier);
	BytesToStream(S,src->text,CHAT_MESSAGE_TEXT_BUFFER_SIZE);
	assert(S == dest->data + SIZEOF_NetChatMessage);
}

void
netcpy(NetChatMessage* dest, const NetChatMessage_NET* src)
{
	uint8 *S = (uint8 *)src->data;
	StreamToValue(S,dest->sender_identifier);
	StreamToBytes(S,dest->text,CHAT_MESSAGE_TEXT_BUFFER_SIZE);
	assert(S == src->data + SIZEOF_NetChatMessage);
}

#endif // NETWORK_CHAT
