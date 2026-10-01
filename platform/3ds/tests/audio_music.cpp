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
// Host integration test: real Tremor/libogg decoder, simulated NDSP and libctru
// thread scheduling. Compile with tests/audio-mocks first on the include path.
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <deque>
#include <thread>

#include "../../../src/engine/audio_3ds.cpp"

struct Thread_tag
{
    std::thread worker;
    explicit Thread_tag( void ( *entry )( void * ), void * arg )
        : worker( entry, arg )
    {}
};
namespace
{
    std::deque<ndspWaveBuf *> queued;
    size_t allocations = 0, maximumBytes = 0, allocatedBytes = 0;
    std::map<void *, size_t> allocationSizes;
    bool dspInitialized = false, failThread = false, failAllocation = false;
    float lastMusicVolume = -1;
    ndspWaveBuf * active = nullptr;
}
namespace Logging
{
    std::string GetTimeString()
    {
        return {};
    }
}
int ndspInit()
{
    dspInitialized = true;
    return 0;
}
void ndspExit()
{
    assert( allocations == 0 && !musicStream );
    dspInitialized = false;
}
void ndspChnWaveBufClear( int channel )
{
    if ( channel == musicChannel ) {
        queued.clear();
        active = nullptr;
    }
}
void * linearAlloc( size_t size )
{
    if ( failAllocation )
        return nullptr;
    void * ptr = std::malloc( size );
    assert( ptr );
    allocationSizes[ptr] = size;
    ++allocations;
    allocatedBytes += size;
    maximumBytes = std::max( maximumBytes, allocatedBytes );
    return ptr;
}
void linearFree( void * ptr )
{
    assert( allocationSizes.count( ptr ) );
    allocatedBytes -= allocationSizes[ptr];
    allocationSizes.erase( ptr );
    --allocations;
    std::free( ptr );
}
void ndspChnSetMix( int channel, float * mix )
{
    assert( dspInitialized );
    if ( channel == musicChannel )
        lastMusicVolume = mix[0];
}
void DSP_FlushDataCache( const void *, uint32_t ) {}
void ndspChnReset( int channel )
{
    ndspChnWaveBufClear( channel );
}
void ndspChnSetInterp( int, int ) {}
void ndspChnSetRate( int, float ) {}
void ndspChnSetFormat( int, uint16_t ) {}
void ndspChnWaveBufAdd( int channel, ndspWaveBuf * wave )
{
    assert( dspInitialized && wave->nsamples > 0 );
    if ( channel == musicChannel ) {
        wave->status = NDSP_WBUF_QUEUED;
        queued.push_back( wave );
    }
}
uint32_t ndspChnGetSamplePos( int )
{
    return active ? active->nsamples / 2 : 0;
}
uint64_t osGetTime()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::steady_clock::now().time_since_epoch() ).count();
}
Thread threadCreate( void ( *entry )( void * ), void * arg, size_t stack, int, int core, bool )
{
    assert( stack <= 64 * 1024 && core == 0 );
    return failThread ? nullptr : new Thread_tag( entry, arg );
}
int threadJoin( Thread thread, uint64_t )
{
    thread->worker.join();
    return 0;
}
void threadFree( Thread thread )
{
    delete thread;
}
void svcSleepThread( int64_t )
{
    {
        std::lock_guard<std::mutex> lock( audioMutex );
        if ( active )
            active->status = NDSP_WBUF_DONE;
        active = nullptr;
        if ( !queued.empty() ) {
            active = queued.front();
            queued.pop_front();
            active->status = NDSP_WBUF_PLAYING;
        }
    }
    std::this_thread::sleep_for( std::chrono::milliseconds( 5 ) );
}
int main( int argc, char ** argv )
{
    assert( argc == 2 );
    Audio::Init();
    assert( Audio::isValid() && Mixer::getChannelCount() == 23 );
    assert( !Music::Play( 10, Music::PlaybackMode::PLAY_ONCE ) );
    Music::Play( 10, argv[1], Music::PlaybackMode::RESUME_AND_PLAY_INFINITE );
    std::this_thread::sleep_for( std::chrono::milliseconds( 12 ) );
    assert( Music::isPlaying() );
    Music::Stop();
    assert( !Music::isPlaying() && allocations == 0 );
    assert( musicTracks[10].resumeFrame > 0 );
    const auto resume = musicTracks[10].resumeFrame;
    assert( Music::Play( 10, Music::PlaybackMode::RESUME_AND_PLAY_INFINITE ) );
    std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
    Music::setVolume( 50 );
    Audio::Mute();
    {
        std::lock_guard<std::mutex> lock( audioMutex );
        assert( lastMusicVolume == 0 );
    }
    Audio::Unmute();
    {
        std::lock_guard<std::mutex> lock( audioMutex );
        assert( lastMusicVolume == 0.5f );
    }
    Music::Stop();
    assert( musicTracks[10].resumeFrame >= 0 && resume > 0 );
    assert( Music::Play( 10, Music::PlaybackMode::PLAY_ONCE ) );
    for ( int i = 0; i < 200 && Music::isPlaying(); ++i )
        std::this_thread::sleep_for( std::chrono::milliseconds( 5 ) );
    assert( !Music::isPlaying() );
    Music::Stop();
    assert( musicTracks[10].resumeFrame == 0 );
    failAllocation = true;
    assert( !Music::Play( 10, Music::PlaybackMode::PLAY_ONCE ) );
    failAllocation = false;
    failThread = true;
    assert( !Music::Play( 10, Music::PlaybackMode::PLAY_ONCE ) );
    failThread = false;
    assert( allocations == 0 );
    Music::Play( 11, "/missing/music.ogg", Music::PlaybackMode::PLAY_ONCE );
    assert( !Music::isPlaying() );
    const std::string invalidPath = std::string( argv[1] ) + ".invalid";
    {
        FILE * invalid = std::fopen( invalidPath.c_str(), "wb" );
        assert( invalid );
        std::fputs( "invalid vorbis", invalid );
        std::fclose( invalid );
    }
    Music::Play( 12, invalidPath, Music::PlaybackMode::PLAY_ONCE );
    std::remove( invalidPath.c_str() );
    assert( !Music::isPlaying() );
    assert( Music::Play( 10, Music::PlaybackMode::REWIND_AND_PLAY_INFINITE ) );
    Audio::Quit();
    assert( !Audio::isValid() && !Music::isPlaying() && allocations == 0 && !dspInitialized );
    assert( maximumBytes == 3 * 16 * 1024 );
}
