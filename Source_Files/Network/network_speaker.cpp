/*
NETWORK_SPEAKER.C

	Copyright (C) 1991-2001 and beyond by Bungie Studios, Inc.
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

Sunday, August 14, 1994 1:20:55 AM
*/

/*
//we should only fill one buffer with static to lower the delay before actually playing incoming data
//we should use our doubleback procedure to fill the buffers
//we should not crap if our initialization fails but queue_�() gets called anyway
*/

#include <stdlib.h>

#include "macintosh_cseries.h"

#include "network_sound.h"

#ifdef env68k
#pragma segment sound
#endif

/* ---------- constants */

#define MAXIMUM_DOUBLE_BUFFER_SIZE 1024
#define MAXIMUM_QUEUE_SIZE (3*MAXIMUM_DOUBLE_BUFFER_SIZE)

enum /* speaker states */
{
	_speaker_is_off, /* no sound is being played, we�re waiting for some initial data */
	_speaker_is_turning_on, /* we�ve been triggered, but can�t start playing until we get three blocks worth of data */
	_speaker_is_on /* we�re playing sound data in real-time, responding to doubleback requests and everything */
	/* when we don�t get data for a threshold number of doublebacks, we go directly to _speaker_is_off */
};

/* ---------- structures */

struct speaker_definition
{
	SndChannelPtr channel;
	SndDoubleBufferHeaderPtr header;

	long queue_size; /* the number of bytes in the queue */
	Ptr queue;
	
	short block_size; /* the number of bytes in each of our double buffers */
	short connection_threshold; /* the number of times we can doubleback without data before turning off */
	short connection_status; /* the number of times we have doublebacked without data (reset to zero if we get data) */
	short state;
	
	uint16 random_seed;
};

/* ---------- globals */

static struct speaker_definition *speaker= (struct speaker_definition *) NULL;
static SndDoubleBackUPP doubleback_routine_descriptor;

/* ---------- private code */

static void fill_buffer_with_static(byte *buffer, short count);
static void fill_double_buffer_with_static(SndDoubleBufferPtr buffer);

static pascal void network_speaker_doubleback_procedure(SndChannelPtr channel, SndDoubleBufferPtr doubleBufferPtr);
void fill_network_speaker_buffer(SndDoubleBufferPtr doubleBufferPtr);

/* ---------- code */

OSErr open_network_speaker(
	short block_size,
	short connection_threshold)
{
	OSErr error;
	
	assert(!speaker);
	assert(block_size>0&&block_size<=MAXIMUM_DOUBLE_BUFFER_SIZE);
	assert(connection_threshold>1&&connection_threshold<16);

	/* install our atexit cleanup procedure and build a routine descriptor */	
	{
		static bool initialization= true;
		
		if (initialization)
		{
			// Thomas Herzog fix
 			#if UNIVERSAL_INTERFACES_VERSION < 0x0340
 				doubleback_routine_descriptor= NewSndDoubleBackProc((ProcPtr)network_speaker_doubleback_procedure);
 			#else
 				doubleback_routine_descriptor= NewSndDoubleBackUPP(network_speaker_doubleback_procedure);
 			#endif
 			// doubleback_routine_descriptor= NewSndDoubleBackProc((ProcPtr)network_speaker_doubleback_procedure);
			assert(doubleback_routine_descriptor);
			
			atexit(close_network_speaker);
		}
	}
	
	speaker= (struct speaker_definition *) NewPtr(sizeof(struct speaker_definition));
	if ((error= MemError())==noErr)
	{
		speaker->random_seed= 1;
		speaker->block_size= block_size;
		speaker->connection_threshold= connection_threshold;

		speaker->channel= (SndChannelPtr) NewPtrClear(sizeof(SndChannel));
		speaker->header= (SndDoubleBufferHeaderPtr) NewPtrClear(sizeof(SndDoubleBufferHeader));
		speaker->queue= NewPtr(sizeof(byte)*MAXIMUM_QUEUE_SIZE);
		if ((error= MemError())==noErr)
		{
			SndDoubleBufferHeaderPtr header= speaker->header;
			
			header->dbhNumChannels= 1;
			header->dbhSampleSize= 8;
			header->dbhCompressionID= sixToOne;
			header->dbhPacketSize= sixToOnePacketSize;
			header->dbhSampleRate= rate22khz;
			header->dbhDoubleBack= doubleback_routine_descriptor;
			header->dbhBufferPtr[0]= (SndDoubleBufferPtr) NewPtrClear(sizeof(SndDoubleBuffer)+MAXIMUM_DOUBLE_BUFFER_SIZE);	
			header->dbhBufferPtr[1]= (SndDoubleBufferPtr) NewPtrClear(sizeof(SndDoubleBuffer)+MAXIMUM_DOUBLE_BUFFER_SIZE);	
			
			if ((error= MemError())==noErr)
			{
				speaker->channel->qLength= stdQLength;
#ifdef env68k
				speaker->channel->userInfo= (long) get_a5();
#endif
				
				error= SndNewChannel(&speaker->channel, sampledSynth, initMono|initMACE6, NULL);
				if (error==noErr)
				{
					quiet_network_speaker(); /* to set defaults */
				}
			}
		}
	}

	/* if something went wrong, zero the speaker definition (without freeing any of our memory
		like we should) */
	if (error!=noErr) speaker= (struct speaker_definition *) NULL;
	
	return error;
}

void close_network_speaker(
	void)
{
	if (speaker)
	{
		OSErr error;
		
		error= SndDisposeChannel(speaker->channel, true);
		warn(error==noErr);
		
		DisposePtr((Ptr)speaker->header->dbhBufferPtr[0]);
		DisposePtr((Ptr)speaker->header->dbhBufferPtr[1]);
		DisposePtr((Ptr)speaker->header);
		DisposePtr((Ptr)speaker->channel);
		DisposePtr((Ptr)speaker->queue);
		DisposePtr((Ptr)speaker);
		
		speaker= (struct speaker_definition *) NULL;
	}
}

/* can be called at interrupt time */
void queue_network_speaker_data(
	byte *buffer,
	short count)
{
	if (speaker)
	{
		switch (speaker->state)
		{
			case _speaker_is_off:
				/* we were off but now we�re getting data; fill one double buffer with static and
					change our state to _turning_on (we�ll wait until we receive another full
					double buffer worth of data before beginning to replay) */
				quiet_network_speaker(); /* we could be playing trailing static */
				speaker->state= _speaker_is_turning_on;
				fill_network_speaker_buffer(speaker->header->dbhBufferPtr[0]);
				break;
			
			case _speaker_is_on:
			case _speaker_is_turning_on:
				speaker->connection_status= 0;
				break;
			
			default:
				vhalt(csprintf(temporary, "what the hell is #%d!?", speaker->state));
		}
		
		/* move incoming data into queue, NULL buffer means static */
		if (speaker->queue_size+count<=MAXIMUM_QUEUE_SIZE)
		{
			if (buffer)
			{
				BlockMove(buffer, speaker->queue+speaker->queue_size, count);
			}
			else
			{
				fill_buffer_with_static((byte *)speaker->queue+speaker->queue_size, count);
			}
			
			speaker->queue_size+= count;
		}
		else
		{
			vpause(csprintf(temporary, "queue_net_speaker_data() is ignoring data: #%d+#%d>#%d", speaker->queue_size, count, MAXIMUM_QUEUE_SIZE));
		}
	
#ifdef SNDPLAYDOUBLEBUFFER_DOESNT_SUCK
		switch (speaker->state)
		{
			case _speaker_is_turning_on:
				/* check and see if we have enough data to turn on */
				if (speaker->queue_size>=speaker->block_size)
				{
					OSErr error;
					
					error= SndPlayDoubleBuffer(speaker->channel, speaker->header);
					vwarn(error==noErr, csprintf(temporary, "SndPlayDoubleBuffer(%p,%p)==#%d", speaker->channel, speaker->header, error));
					
					speaker->state= _speaker_is_on;
				}
				break;
		}
#endif
	}
}

void quiet_network_speaker(
	void)
{
	if (speaker)
	{
		SndCommand command;
		OSErr error;
		
		command.cmd= flushCmd;
		command.param1= 0;
		command.param2= 0;
		error= SndDoImmediate(speaker->channel, &command);
		assert(error==noErr);
		
		command.cmd= quietCmd;
		command.param1= 0;
		command.param2= 0;
		error= SndDoImmediate(speaker->channel, &command);
		assert(error==noErr);
		
		/* speaker is off, no missed doublebacks, queue is empty, double buffers are not ready */
		speaker->state= _speaker_is_off;
		speaker->connection_status= 0;
		speaker->queue_size= 0;
		speaker->header->dbhBufferPtr[0]->dbFlags= 0;
		speaker->header->dbhBufferPtr[1]->dbFlags= 0;
	}
}

/* because SndPlayDoubleBuffer() is not safe at interrupt time */
void network_speaker_idle_proc(
	void)
{
	if (speaker)
	{
		switch (speaker->state)
		{
			case _speaker_is_turning_on:
				/* check and see if we have enough data to turn on, if so fill the second
					double buffer and call SndPlayDoubleBuffer */
				if (speaker->queue_size>=speaker->block_size)
				{
					OSErr error;
					
					fill_network_speaker_buffer(speaker->header->dbhBufferPtr[1]);
					error= SndPlayDoubleBuffer(speaker->channel, speaker->header);
					vwarn(error==noErr, csprintf(temporary, "SndPlayDoubleBuffer(%p,%p)==#%d", speaker->channel, speaker->header, error));
					
					speaker->state= _speaker_is_on;
				}
				break;
		}
	}
}

/* ---------- private code */

static void fill_buffer_with_static(
	byte *buffer,
	short count)
{
	uint16 seed= speaker->random_seed;
	
	while ((count-=1)>=0)
	{
		*buffer++= seed;
		if (seed&1) seed= (seed>>1)^0xb400; else seed= seed>>1;
	}
	speaker->random_seed= seed;
}

static pascal void network_speaker_doubleback_procedure(
	SndChannelPtr channel,
	SndDoubleBufferPtr doubleBufferPtr)
{
#ifdef env68k
	long old_a5= set_a5(channel->userInfo); /* set our a5 world */
#else
	(void)channel;
#endif
	
	fill_network_speaker_buffer(doubleBufferPtr);
	
#ifdef env68k
	set_a5(old_a5); /* restore a5 */
#endif
}

void fill_network_speaker_buffer(
	SndDoubleBufferPtr doubleBufferPtr)
{
	long available_bytes= MIN(speaker->queue_size, speaker->block_size);
	long missing_bytes= speaker->block_size-available_bytes;
	long extra_bytes= speaker->queue_size-available_bytes;

	assert(speaker);
	assert(speaker->queue_size>=0&&speaker->queue_size<=MAXIMUM_QUEUE_SIZE);
	assert(speaker->block_size>0&&speaker->block_size<=MAXIMUM_DOUBLE_BUFFER_SIZE);

	/* if we don�t have any data, check and see if we�re over the threshold of the number of
		times we can miss data; if we are, turn the speaker off and tell the sound manager
		this is the last buffer it�s going to get from us */
	if (missing_bytes==speaker->block_size)
	{
		if ((speaker->connection_status+= 1)>speaker->connection_threshold)
		{
			speaker->state= _speaker_is_off;
		}
	}

	/* for better or for worse, fill the waiting buffer */
	if (available_bytes)
		BlockMove(speaker->queue, doubleBufferPtr->dbSoundData, available_bytes);
	if (missing_bytes)
		fill_buffer_with_static((byte *)doubleBufferPtr->dbSoundData+available_bytes, missing_bytes);
	if (extra_bytes)
		BlockMove(speaker->queue+speaker->block_size, speaker->queue, extra_bytes);
	speaker->queue_size-= available_bytes;

	switch (speaker->state)
	{
		case _speaker_is_off:
			doubleBufferPtr->dbFlags|= dbLastBuffer|dbBufferReady;
			doubleBufferPtr->dbNumFrames= 0;
			break;

		case _speaker_is_on:
		case _speaker_is_turning_on:
			doubleBufferPtr->dbFlags|= dbBufferReady;
			doubleBufferPtr->dbNumFrames= speaker->block_size;
			break;
		
		default:
			vhalt(csprintf(temporary, "what the hell is #%d!?", speaker->state));
	}
}
