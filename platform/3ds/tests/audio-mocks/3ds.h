// SPDX-License-Identifier: GPL-2.0-or-later
// Only the libctru audio/thread API used by the real backend is simulated.
// NDSP field types mirror libctru/include/3ds/ndsp/ndsp.h.
#pragma once
#include <cstdint>
#include <cstddef>
#define R_SUCCEEDED(x) ((x)>=0)
enum { NDSP_WBUF_FREE=0, NDSP_WBUF_QUEUED=1, NDSP_WBUF_PLAYING=2, NDSP_WBUF_DONE=3 };
enum { NDSP_INTERP_LINEAR=1, NDSP_FORMAT_MONO_PCM8=1, NDSP_FORMAT_STEREO_PCM8=2, NDSP_FORMAT_MONO_PCM16=5, NDSP_FORMAT_STEREO_PCM16=6 };
struct ndspWaveBuf { union { const void * data_vaddr; int16_t * data_pcm16; }; uint32_t nsamples; bool looping; uint8_t status; };
int ndspInit(); void ndspExit(); void ndspChnWaveBufClear(int);
void linearFree(void*); void *linearAlloc(size_t); void ndspChnSetMix(int,float*);
void DSP_FlushDataCache(const void*,uint32_t); void ndspChnReset(int);
void ndspChnSetInterp(int,int); void ndspChnSetRate(int,float);
void ndspChnSetFormat(int,uint16_t); void ndspChnWaveBufAdd(int,ndspWaveBuf*);
uint32_t ndspChnGetSamplePos(int); uint64_t osGetTime(); void svcSleepThread(int64_t);
using Thread = struct Thread_tag *;
Thread threadCreate(void(*)(void*),void*,size_t,int,int,bool);
int threadJoin(Thread,uint64_t); void threadFree(Thread);
