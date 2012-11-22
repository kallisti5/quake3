/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <stdio.h>
#include <PushGameSound.h>
#include <GameSoundDefs.h>

#include "../game/q_shared.h"
#include "../client/snd_local.h"

#define BUFFER_FRAME_COUNT 1024
int				snd_inited = 0;
BPushGameSound	*gGameSound = NULL;
void			*base = NULL;
size_t			size = 0;


extern "C" qboolean SNDDMA_Init(void) {

	gs_audio_format	gs_fmt;
	gs_fmt.format = gs_audio_format::B_GS_S16;
	gs_fmt.frame_rate = 22050;
	gs_fmt.channel_count = 2;
	gs_fmt.byte_order = 2;
	gs_fmt.buffer_size = BUFFER_FRAME_COUNT * 2 * 2;

	gGameSound = new BPushGameSound(BUFFER_FRAME_COUNT, &gs_fmt);
	if(gGameSound->InitCheck() != B_OK)	{
		delete gGameSound;
		gGameSound = NULL;
		return qfalse;
	}

	// get access to the complete buffer
	if(BPushGameSound::lock_failed == gGameSound->LockForCyclic(&base, &size)) {
		delete gGameSound;
		gGameSound = NULL;
		return qfalse;
	}
	
	// at this point base points to the first byte of the audio buffer and
	// size is the total size of this buffer in bytes

	dma.samplebits = 16;
	dma.speed = 22050;
	dma.channels = 2;
	dma.samples = size / (dma.samplebits/8);
	dma.buffer = (byte*)base;
	dma.submission_chunk = 1;
	
	if (gGameSound->StartPlaying() == B_OK)	{
		snd_inited = 1;
		return qtrue;
	} else {
		gGameSound->UnlockCyclic();
		delete gGameSound;
		gGameSound = NULL;
		return qfalse;
	}
}

extern "C" int SNDDMA_GetDMAPos(void) {
	//Com_Printf("SNDDMA_GetDMAPos\n");
	if(!snd_inited)
		return 0;
	return gGameSound->CurrentPosition()/(dma.samplebits/8);
}

extern "C" void SNDDMA_Shutdown(void) {
	if(snd_inited) {
		gGameSound->StopPlaying();
		gGameSound->UnlockCyclic();
		delete gGameSound;
		gGameSound = NULL;
		snd_inited = 0;
	}
}

/*
==============
SNDDMA_Submit

Send sound to device if buffer isn't really the dma buffer
===============
*/

extern "C" void SNDDMA_Submit(void) {
}

extern "C" void SNDDMA_BeginPainting (void) {
}
