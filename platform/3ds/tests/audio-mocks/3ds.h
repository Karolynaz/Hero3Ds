/***************************************************************************
 *   fheroes2: https://github.com/ihhub/fheroes2                           *
 *   Copyright (C) 2026                                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

// SPDX-License-Identifier: GPL-2.0-or-later
// Only the libctru audio/thread API used by the real backend is simulated.
// NDSP field types mirror libctru/include/3ds/ndsp/ndsp.h.
#pragma once
#include <cstddef>
#include <cstdint>
#define R_SUCCEEDED( x ) ( ( x ) >= 0 )
enum
{
    NDSP_WBUF_FREE = 0,
    NDSP_WBUF_QUEUED = 1,
    NDSP_WBUF_PLAYING = 2,
    NDSP_WBUF_DONE = 3
};
enum
{
    NDSP_INTERP_LINEAR = 1,
    NDSP_FORMAT_MONO_PCM8 = 1,
    NDSP_FORMAT_STEREO_PCM8 = 2,
    NDSP_FORMAT_MONO_PCM16 = 5,
    NDSP_FORMAT_STEREO_PCM16 = 6
};
struct ndspWaveBuf
{
    union
    {
        const void * data_vaddr;
        int16_t * data_pcm16;
    };
    uint32_t nsamples;
    bool looping;
    uint8_t status;
};
int ndspInit();
void ndspExit();
void ndspChnWaveBufClear( int );
void linearFree( void * );
void * linearAlloc( size_t );
void ndspChnSetMix( int, float * );
void DSP_FlushDataCache( const void *, uint32_t );
void ndspChnReset( int );
void ndspChnSetInterp( int, int );
void ndspChnSetRate( int, float );
void ndspChnSetFormat( int, uint16_t );
void ndspChnWaveBufAdd( int, ndspWaveBuf * );
uint32_t ndspChnGetSamplePos( int );
uint64_t osGetTime();
void svcSleepThread( int64_t );
using Thread = struct Thread_tag *;
Thread threadCreate( void ( * )( void * ), void *, size_t, int, int, bool );
int threadJoin( Thread, uint64_t );
void threadFree( Thread );
