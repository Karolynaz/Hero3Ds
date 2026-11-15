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
#if defined( TARGET_NINTENDO_3DS )
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include <3ds.h>
#include <tremor/ivorbisfile.h>

#include "audio.h"
#include "logging.h"

namespace
{
    constexpr int maxChannels = 23; // NDSP channel 23 is reserved for streamed music.
    constexpr int musicChannel = 23;
    struct Channel
    {
        ndspWaveBuf wave{};
        void * data{ nullptr };
    };
    std::array<Channel, maxChannels> channels;
    std::mutex audioMutex;
    bool ready = false;
    bool muted = false;
    bool suspended = false;
    std::atomic<bool> streamPaused{ false };
    int channelCount = maxChannels;
    float volume = 1.0f;
    float musicVolume = 1.0f;
    float musicFade = 1.0f;
    int fadeInMs = 0;
    void applyMusicVolume()
    {
        float mix[12]{};
        mix[0] = mix[1] = ( muted || suspended ) ? 0.0f : musicVolume * musicFade;
        ndspChnSetMix( musicChannel, mix );
    }

    uint16_t read16( const uint8_t * ptr )
    {
        return static_cast<uint16_t>( ptr[0] | ( ptr[1] << 8 ) );
    }
    uint32_t read32( const uint8_t * ptr )
    {
        return ptr[0] | ( uint32_t( ptr[1] ) << 8 ) | ( uint32_t( ptr[2] ) << 16 ) | ( uint32_t( ptr[3] ) << 24 );
    }
    void release( int id )
    {
        ndspChnWaveBufClear( id );
        if ( channels[id].data )
            linearFree( channels[id].data );
        channels[id] = {};
    }
    void applyVolume( int id )
    {
        float mix[12]{};
        mix[0] = mix[1] = ( muted || suspended ) ? 0.0f : volume;
        ndspChnSetMix( id, mix );
    }
}
namespace Audio
{
    void Init()
    {
        std::lock_guard<std::mutex> lock( audioMutex );
        if ( !ready )
            ready = R_SUCCEEDED( ndspInit() );
    }
    void Quit()
    {
        {
            std::lock_guard<std::mutex> lock( audioMutex );
            if ( !ready )
                return;
            // Block new playback before joining the decoder thread.
            ready = false;
        }
        Music::Stop();
        std::lock_guard<std::mutex> lock( audioMutex );
        for ( int id = 0; id < maxChannels; ++id )
            release( id );
        ndspExit();
    }
    bool isValid()
    {
        std::lock_guard<std::mutex> lock( audioMutex );
        return ready;
    }
    void Mute()
    {
        std::lock_guard<std::mutex> lock( audioMutex );
        muted = true;
        if ( ready ) {
            for ( int id = 0; id < channelCount; ++id )
                applyVolume( id );
            applyMusicVolume();
        }
    }
    void Unmute()
    {
        std::lock_guard<std::mutex> lock( audioMutex );
        muted = false;
        if ( ready ) {
            for ( int id = 0; id < channelCount; ++id )
                applyVolume( id );
            applyMusicVolume();
        }
    }
    void onSuspend3DS()
    {
        streamPaused.store( true );
        std::lock_guard<std::mutex> lock( audioMutex );
        suspended = true;
        if ( ready ) {
            for ( int id = 0; id < channelCount; ++id )
                applyVolume( id );
            applyMusicVolume();
        }
    }
    void onResume3DS()
    {
        {
            std::lock_guard<std::mutex> lock( audioMutex );
            suspended = false;
            if ( ready ) {
                for ( int id = 0; id < channelCount; ++id )
                    applyVolume( id );
                applyMusicVolume();
            }
        }
        streamPaused.store( false );
    }
}
namespace Mixer
{
    void SetChannels( int count )
    {
        std::lock_guard<std::mutex> lock( audioMutex );
        channelCount = std::clamp( count, 1, maxChannels );
    }
    int getChannelCount()
    {
        std::lock_guard<std::mutex> lock( audioMutex );
        return channelCount;
    }
    int Play( const uint8_t * ptr, uint32_t size, bool loop, const std::optional<std::pair<int16_t, uint8_t>> )
    {
        std::lock_guard<std::mutex> lock( audioMutex );
        if ( !ready || !ptr || size < 12 || std::memcmp( ptr, "RIFF", 4 ) || std::memcmp( ptr + 8, "WAVE", 4 ) )
            return -1;
        uint16_t format = 0, bits = 0, count = 0;
        uint32_t rate = 0, pcmSize = 0;
        const uint8_t * pcm = nullptr;
        for ( uint32_t offset = 12; offset <= size - 8; ) {
            const uint32_t length = read32( ptr + offset + 4 );
            if ( length > size - offset - 8 )
                return -1;
            if ( !std::memcmp( ptr + offset, "fmt ", 4 ) && length >= 16 ) {
                format = read16( ptr + offset + 8 );
                count = read16( ptr + offset + 10 );
                rate = read32( ptr + offset + 12 );
                bits = read16( ptr + offset + 22 );
            }
            else if ( !std::memcmp( ptr + offset, "data", 4 ) ) {
                pcm = ptr + offset + 8;
                pcmSize = length;
            }
            if ( length == UINT32_MAX || length + ( length & 1U ) > size - offset - 8 )
                break;
            offset += 8 + length + ( length & 1U );
        }
        if ( format != 1 || !pcm || !pcmSize || !rate || ( count != 1 && count != 2 ) || ( bits != 8 && bits != 16 ) )
            return -1;
        int id = 0;
        for ( ; id < channelCount; ++id )
            if ( !channels[id].data || channels[id].wave.status == NDSP_WBUF_DONE )
                break;
        if ( id == channelCount )
            return -1;
        release( id );
        // NDSP consumes signed PCM8; WAV PCM8 uses an unsigned midpoint.
        channels[id].data = linearAlloc( pcmSize );
        if ( !channels[id].data )
            return -1;
        std::memcpy( channels[id].data, pcm, pcmSize );
        if ( bits == 8 ) {
            auto * samples = static_cast<uint8_t *>( channels[id].data );
            for ( uint32_t i = 0; i < pcmSize; ++i )
                samples[i] ^= 128;
        }
        DSP_FlushDataCache( channels[id].data, pcmSize );
        ndspChnReset( id );
        ndspChnSetInterp( id, NDSP_INTERP_LINEAR );
        ndspChnSetRate( id, static_cast<float>( rate ) );
        const uint16_t ndspFormat
            = bits == 8 ? ( count == 1 ? NDSP_FORMAT_MONO_PCM8 : NDSP_FORMAT_STEREO_PCM8 ) : ( count == 1 ? NDSP_FORMAT_MONO_PCM16 : NDSP_FORMAT_STEREO_PCM16 );
        ndspChnSetFormat( id, ndspFormat );
        applyVolume( id );
        auto & wave = channels[id].wave;
        wave.data_vaddr = channels[id].data;
        wave.nsamples = pcmSize / ( count * ( bits / 8 ) );
        wave.looping = loop;
        ndspChnWaveBufAdd( id, &wave );
        return id;
    }
    void Stop( int id )
    {
        std::lock_guard<std::mutex> lock( audioMutex );
        if ( !ready )
            return;
        if ( id < 0 )
            for ( int i = 0; i < maxChannels; ++i )
                release( i );
        else if ( id < maxChannels )
            release( id );
    }
    bool isPlaying( int id )
    {
        std::lock_guard<std::mutex> lock( audioMutex );
        return ready && id >= 0 && id < maxChannels && channels[id].data && channels[id].wave.status != NDSP_WBUF_DONE;
    }
    void setVolume( int percent )
    {
        std::lock_guard<std::mutex> lock( audioMutex );
        volume = std::clamp( percent, 0, 100 ) / 100.0f;
        if ( ready )
            for ( int id = 0; id < channelCount; ++id )
                applyVolume( id );
    }
    void setPosition( int, int16_t, uint8_t ) {}
}
namespace
{
    constexpr size_t musicBufferBytes = 16 * 1024;
    constexpr size_t musicBufferCount = 3;
    constexpr long midiSampleRate = 22050;
    constexpr size_t midiMaxVoices = 24;
    constexpr size_t midiSliceFrames = 64;

    uint16_t readBe16( const uint8_t * ptr )
    {
        return static_cast<uint16_t>( ( ptr[0] << 8 ) | ptr[1] );
    }

    uint32_t readBe32( const uint8_t * ptr )
    {
        return ( uint32_t( ptr[0] ) << 24 ) | ( uint32_t( ptr[1] ) << 16 ) | ( uint32_t( ptr[2] ) << 8 ) | ptr[3];
    }

    bool readVlq( const uint8_t *& ptr, const uint8_t * end, uint32_t & out )
    {
        out = 0;
        for ( int i = 0; i < 4; ++i ) {
            if ( ptr >= end )
                return false;
            const uint8_t byte = *ptr++;
            out = ( out << 7 ) | ( byte & 0x7Fu );
            if ( ( byte & 0x80u ) == 0 )
                return true;
        }
        return true;
    }

    const std::array<float, 257> & sineLut()
    {
        static const std::array<float, 257> lut = [] {
            std::array<float, 257> table{};
            constexpr double twoPi = 6.28318530717958647692;
            for ( size_t i = 0; i <= 256; ++i )
                table[i] = static_cast<float>( std::sin( ( static_cast<double>( i ) / 256.0 ) * twoPi ) );
            return table;
        }();
        return lut;
    }

    float lookupSin( float phase )
    {
        phase -= std::floor( phase );
        const float scaled = phase * 256.0f;
        const size_t idx = static_cast<size_t>( scaled ) & 255u;
        const float frac = scaled - static_cast<float>( idx );
        const auto & lut = sineLut();
        return lut[idx] + ( lut[idx + 1] - lut[idx] ) * frac;
    }

    struct GmPatch
    {
        float modRatio;
        float modIndex;
        float attackSec;
        float decaySec;
        float sustainLevel;
        float releaseSec;
    };

    constexpr GmPatch gmFamilyPatch( const uint8_t program )
    {
        constexpr std::array<GmPatch, 16> families{ {
            { 2.0f, 1.15f, 0.005f, 0.45f, 0.25f, 0.22f }, // 0: Piano / Harpsichord
            { 3.5f, 0.85f, 0.004f, 0.30f, 0.10f, 0.18f }, // 1: Chromatic Percussion / Bells
            { 2.0f, 0.95f, 0.015f, 0.12f, 0.85f, 0.14f }, // 2: Organ
            { 2.0f, 1.00f, 0.004f, 0.38f, 0.18f, 0.18f }, // 3: Guitar / Lute / Harp
            { 1.0f, 1.25f, 0.006f, 0.26f, 0.45f, 0.14f }, // 4: Bass
            { 1.0f, 0.65f, 0.085f, 0.20f, 0.78f, 0.30f }, // 5: Strings
            { 1.0f, 0.55f, 0.095f, 0.22f, 0.80f, 0.32f }, // 6: Ensemble / Choir
            { 1.0f, 1.45f, 0.032f, 0.18f, 0.72f, 0.16f }, // 7: Brass
            { 2.0f, 1.10f, 0.028f, 0.16f, 0.68f, 0.15f }, // 8: Reed
            { 1.0f, 0.30f, 0.045f, 0.15f, 0.76f, 0.18f }, // 9: Pipe / Flute
            { 2.0f, 0.80f, 0.015f, 0.20f, 0.70f, 0.20f }, // 10: Synth Lead
            { 1.0f, 0.45f, 0.120f, 0.25f, 0.82f, 0.38f }, // 11: Synth Pad
            { 3.0f, 0.75f, 0.020f, 0.32f, 0.40f, 0.25f }, // 12: Synth Effects
            { 2.5f, 1.15f, 0.006f, 0.34f, 0.22f, 0.20f }, // 13: Ethnic
            { 1.4f, 1.10f, 0.003f, 0.28f, 0.05f, 0.18f }, // 14: Percussive
            { 1.5f, 0.90f, 0.010f, 0.25f, 0.10f, 0.20f }, // 15: Sound FX
        } };
        return families[( program >> 3 ) & 15u];
    }

    struct MidiEvent
    {
        uint64_t tick{ 0 };
        uint32_t order{ 0 };
        ogg_int64_t frame{ 0 };
        uint8_t status{ 0 };
        uint8_t d1{ 0 };
        uint8_t d2{ 0 };
        uint32_t tempoUs{ 500000 };
    };

    struct MidiChannelState
    {
        uint8_t program{ 0 };
        uint8_t volume{ 100 };
        uint8_t expression{ 127 };
        uint8_t pan{ 64 };
        float pitchRatio{ 1.0f };
        bool sustain{ false };
    };

    struct MidiVoice
    {
        enum class Stage : uint8_t
        {
            OFF,
            ATTACK,
            DECAY,
            SUSTAIN,
            RELEASE
        };
        Stage stage{ Stage::OFF };
        bool held{ false };
        bool sustained{ false };
        bool drum{ false };
        uint8_t channel{ 0 };
        uint8_t note{ 0 };
        uint8_t drumKind{ 0 };
        uint64_t age{ 0 };
        float velGain{ 0.0f };
        float env{ 0.0f };
        float baseFreq{ 440.0f };
        float phaseCar{ 0.0f };
        float phaseMod{ 0.0f };
        float modRatio{ 1.0f };
        float modIndex{ 0.5f };
        float attackStep{ 0.01f };
        float decayStep{ 0.001f };
        float sustainLevel{ 0.7f };
        float releaseStep{ 0.001f };
        uint32_t lfsr{ 0x12345u };
    };

    class MidiSynth
    {
    public:
        bool load( const std::vector<uint8_t> & data )
        {
            _events.clear();
            _totalFrames = 0;
            if ( data.size() < 14 || std::memcmp( data.data(), "MThd", 4 ) != 0 )
                return false;
            const uint32_t headerLen = readBe32( data.data() + 4 );
            if ( headerLen < 6 || 8 + headerLen > data.size() )
                return false;
            const uint16_t format = readBe16( data.data() + 8 );
            const uint16_t tracks = readBe16( data.data() + 10 );
            const uint16_t division = readBe16( data.data() + 12 );
            if ( format > 2 || tracks == 0 )
                return false;
            const uint32_t ticksPerQuarter = ( division & 0x8000u ) == 0 && division > 0 ? division : 120u;

            const uint8_t * p = data.data() + 8 + headerLen;
            const uint8_t * const fileEnd = data.data() + data.size();
            uint32_t nextOrder = 0;
            uint16_t parsedTracks = 0;

            while ( p + 8 <= fileEnd && parsedTracks < tracks ) {
                const uint32_t chunkLen = readBe32( p + 4 );
                const uint8_t * chunkStart = p + 8;
                const uint8_t * chunkEnd = chunkStart + std::min<size_t>( chunkLen, static_cast<size_t>( fileEnd - chunkStart ) );
                if ( std::memcmp( p, "MTrk", 4 ) != 0 ) {
                    p = chunkEnd;
                    continue;
                }
                ++parsedTracks;
                uint64_t tick = 0;
                uint8_t runningStatus = 0;
                const uint8_t * tp = chunkStart;
                while ( tp < chunkEnd ) {
                    uint32_t delta = 0;
                    if ( !readVlq( tp, chunkEnd, delta ) || tp >= chunkEnd )
                        break;
                    tick += delta;
                    uint8_t status = *tp;
                    if ( ( status & 0x80u ) != 0 ) {
                        ++tp;
                        if ( status < 0xF0u )
                            runningStatus = status;
                    }
                    else {
                        if ( runningStatus < 0x80u )
                            break;
                        status = runningStatus;
                    }

                    if ( status == 0xFFu ) {
                        if ( tp >= chunkEnd )
                            break;
                        const uint8_t metaType = *tp++;
                        uint32_t metaLen = 0;
                        if ( !readVlq( tp, chunkEnd, metaLen ) )
                            break;
                        const size_t avail = static_cast<size_t>( chunkEnd - tp );
                        const size_t safeLen = std::min<size_t>( metaLen, avail );
                        if ( metaType == 0x51u && safeLen == 3 ) {
                            MidiEvent ev;
                            ev.tick = tick;
                            ev.order = nextOrder++;
                            ev.status = 0xFFu;
                            ev.tempoUs = ( uint32_t( tp[0] ) << 16 ) | ( uint32_t( tp[1] ) << 8 ) | tp[2];
                            _events.push_back( ev );
                        }
                        tp += safeLen;
                        if ( metaType == 0x2Fu )
                            break;
                    }
                    else if ( status == 0xF0u || status == 0xF7u ) {
                        uint32_t sysLen = 0;
                        if ( !readVlq( tp, chunkEnd, sysLen ) )
                            break;
                        tp += std::min<size_t>( sysLen, static_cast<size_t>( chunkEnd - tp ) );
                    }
                    else {
                        const uint8_t msgType = status & 0xF0u;
                        const bool oneByte = ( msgType == 0xC0u || msgType == 0xD0u );
                        const size_t need = oneByte ? 1 : 2;
                        if ( static_cast<size_t>( chunkEnd - tp ) < need )
                            break;
                        MidiEvent ev;
                        ev.tick = tick;
                        ev.order = nextOrder++;
                        ev.status = status;
                        ev.d1 = tp[0] & 0x7Fu;
                        ev.d2 = oneByte ? 0 : ( tp[1] & 0x7Fu );
                        tp += need;
                        if ( msgType == 0x90u && ev.d2 == 0 )
                            ev.status = static_cast<uint8_t>( 0x80u | ( status & 0x0Fu ) );
                        if ( msgType == 0x80u || msgType == 0x90u || msgType == 0xB0u || msgType == 0xC0u || msgType == 0xE0u )
                            _events.push_back( ev );
                    }
                }
                p = chunkEnd;
            }

            if ( _events.empty() )
                return false;

            std::sort( _events.begin(), _events.end(), []( const MidiEvent & a, const MidiEvent & b ) {
                if ( a.tick != b.tick )
                    return a.tick < b.tick;
                const int pa = a.status == 0xFFu ? 0 : ( ( a.status & 0xF0u ) == 0x90u ? 2 : 1 );
                const int pb = b.status == 0xFFu ? 0 : ( ( b.status & 0xF0u ) == 0x90u ? 2 : 1 );
                if ( pa != pb )
                    return pa < pb;
                return a.order < b.order;
            } );

            double curFrame = 0.0;
            uint64_t prevTick = 0;
            uint32_t usPerQuarter = 500000u;
            for ( auto & ev : _events ) {
                const uint64_t dt = ev.tick - prevTick;
                if ( dt > 0 ) {
                    curFrame += static_cast<double>( dt ) * ( static_cast<double>( usPerQuarter ) / 1000000.0 ) * midiSampleRate / ticksPerQuarter;
                    prevTick = ev.tick;
                }
                ev.frame = static_cast<ogg_int64_t>( curFrame + 0.5 );
                if ( ev.status == 0xFFu && ev.tempoUs > 0 )
                    usPerQuarter = std::clamp<uint32_t>( ev.tempoUs, 50000u, 5000000u );
            }

            _totalFrames = std::max<ogg_int64_t>( 1, _events.back().frame + midiSampleRate / 2 );
            seek( 0 );
            return true;
        }

        ogg_int64_t totalFrames() const
        {
            return _totalFrames;
        }
        ogg_int64_t currentFrame() const
        {
            return _currentFrame;
        }

        void seek( ogg_int64_t frame )
        {
            if ( _totalFrames <= 0 ) {
                _currentFrame = 0;
                _eventCursor = 0;
                return;
            }
            frame = std::clamp<ogg_int64_t>( frame, 0, _totalFrames - 1 );
            _channels.fill( MidiChannelState{} );
            _voices.fill( MidiVoice{} );
            _voiceCounter = 0;
            _currentFrame = frame;
            _eventCursor = 0;
            while ( _eventCursor < _events.size() && _events[_eventCursor].frame < frame ) {
                const auto & ev = _events[_eventCursor++];
                const uint8_t msgType = ev.status & 0xF0u;
                if ( msgType == 0xB0u || msgType == 0xC0u || msgType == 0xE0u )
                    applyControlEvent( ev );
            }
        }

        size_t render( int16_t * dst, const size_t maxBytes, const bool loop, bool & reachedEof )
        {
            if ( _totalFrames <= 0 ) {
                reachedEof = true;
                return 0;
            }
            constexpr size_t frameBytes = 2 * sizeof( int16_t );
            const size_t targetFrames = maxBytes / frameBytes;
            size_t writtenFrames = 0;

            while ( writtenFrames < targetFrames ) {
                while ( _eventCursor < _events.size() && _events[_eventCursor].frame <= _currentFrame )
                    dispatchEvent( _events[_eventCursor++] );

                if ( _currentFrame >= _totalFrames ) {
                    if ( !loop ) {
                        reachedEof = true;
                        break;
                    }
                    seek( 0 );
                    if ( writtenFrames > 0 )
                        break;
                    continue;
                }

                ogg_int64_t nextBoundary = _totalFrames;
                if ( _eventCursor < _events.size() && _events[_eventCursor].frame < nextBoundary )
                    nextBoundary = _events[_eventCursor].frame;
                const size_t untilNext = static_cast<size_t>( std::max<ogg_int64_t>( 1, nextBoundary - _currentFrame ) );
                const size_t slice = std::min( { targetFrames - writtenFrames, untilNext, midiSliceFrames } );
                renderSlice( dst + writtenFrames * 2, slice );
                writtenFrames += slice;
                _currentFrame += static_cast<ogg_int64_t>( slice );
            }
            return writtenFrames * frameBytes;
        }

    private:
        std::vector<MidiEvent> _events;
        std::array<MidiChannelState, 16> _channels{};
        std::array<MidiVoice, midiMaxVoices> _voices{};
        ogg_int64_t _totalFrames{ 0 };
        ogg_int64_t _currentFrame{ 0 };
        size_t _eventCursor{ 0 };
        uint64_t _voiceCounter{ 0 };

        void applyControlEvent( const MidiEvent & ev )
        {
            const uint8_t ch = ev.status & 0x0Fu;
            const uint8_t msgType = ev.status & 0xF0u;
            auto & state = _channels[ch];
            if ( msgType == 0xC0u )
                state.program = ev.d1;
            else if ( msgType == 0xE0u ) {
                const int bend = ( ( int( ev.d2 ) << 7 ) | ev.d1 ) - 8192;
                state.pitchRatio = std::pow( 2.0f, ( static_cast<float>( bend ) / 8192.0f ) * ( 2.0f / 12.0f ) );
            }
            else if ( msgType == 0xB0u ) {
                if ( ev.d1 == 7 )
                    state.volume = ev.d2;
                else if ( ev.d1 == 10 )
                    state.pan = ev.d2;
                else if ( ev.d1 == 11 )
                    state.expression = ev.d2;
                else if ( ev.d1 == 64 ) {
                    state.sustain = ev.d2 >= 64;
                    if ( !state.sustain ) {
                        for ( auto & v : _voices ) {
                            if ( v.stage != MidiVoice::Stage::OFF && v.channel == ch && v.sustained && !v.held ) {
                                v.sustained = false;
                                v.stage = MidiVoice::Stage::RELEASE;
                            }
                        }
                    }
                }
                else if ( ev.d1 == 121 ) {
                    state.expression = 127;
                    state.pitchRatio = 1.0f;
                    state.sustain = false;
                }
                else if ( ev.d1 == 120 || ev.d1 == 123 ) {
                    for ( auto & v : _voices ) {
                        if ( v.channel == ch && v.stage != MidiVoice::Stage::OFF ) {
                            v.held = false;
                            v.sustained = false;
                            if ( ev.d1 == 120 )
                                v.stage = MidiVoice::Stage::OFF;
                            else
                                v.stage = MidiVoice::Stage::RELEASE;
                        }
                    }
                }
            }
        }

        void dispatchEvent( const MidiEvent & ev )
        {
            const uint8_t msgType = ev.status & 0xF0u;
            if ( msgType == 0xB0u || msgType == 0xC0u || msgType == 0xE0u ) {
                applyControlEvent( ev );
                return;
            }
            const uint8_t ch = ev.status & 0x0Fu;
            if ( msgType == 0x80u ) {
                for ( auto & v : _voices ) {
                    if ( v.stage != MidiVoice::Stage::OFF && v.channel == ch && v.note == ev.d1 && v.held ) {
                        v.held = false;
                        if ( _channels[ch].sustain && !v.drum )
                            v.sustained = true;
                        else
                            v.stage = MidiVoice::Stage::RELEASE;
                    }
                }
                return;
            }
            if ( msgType != 0x90u )
                return;

            MidiVoice * slot = nullptr;
            for ( auto & v : _voices ) {
                if ( v.stage == MidiVoice::Stage::OFF ) {
                    slot = &v;
                    break;
                }
            }
            if ( !slot ) {
                for ( auto & v : _voices ) {
                    if ( !slot || ( v.stage == MidiVoice::Stage::RELEASE && slot->stage != MidiVoice::Stage::RELEASE )
                         || ( ( v.stage == MidiVoice::Stage::RELEASE ) == ( slot->stage == MidiVoice::Stage::RELEASE ) && v.age < slot->age ) )
                        slot = &v;
                }
            }
            if ( !slot )
                return;

            MidiVoice voice;
            voice.stage = MidiVoice::Stage::ATTACK;
            voice.held = true;
            voice.channel = ch;
            voice.note = ev.d1;
            voice.drum = ( ch == 9 );
            voice.age = ++_voiceCounter;
            voice.velGain = std::pow( static_cast<float>( ev.d2 ) / 127.0f, 1.35f );
            voice.lfsr = 0xACE1u + ev.d1 * 257u;

            if ( voice.drum ) {
                voice.modRatio = 1.0f;
                voice.modIndex = 0.0f;
                voice.sustainLevel = 0.0f;
                voice.attackStep = 1.0f;
                voice.releaseStep = 1.0f / ( 0.04f * midiSampleRate );
                if ( ev.d1 == 35 || ev.d1 == 36 ) {
                    voice.drumKind = 1; // Kick
                    voice.baseFreq = 135.0f;
                    voice.decayStep = 1.0f / ( 0.16f * midiSampleRate );
                }
                else if ( ev.d1 == 38 || ev.d1 == 40 || ev.d1 == 37 ) {
                    voice.drumKind = 2; // Snare
                    voice.baseFreq = 185.0f;
                    voice.decayStep = 1.0f / ( 0.15f * midiSampleRate );
                }
                else if ( ev.d1 == 42 || ev.d1 == 44 || ev.d1 == 46 ) {
                    voice.drumKind = 3; // Hi-hat
                    voice.decayStep = 1.0f / ( ( ev.d1 == 46 ? 0.24f : 0.06f ) * midiSampleRate );
                }
                else if ( ev.d1 == 49 || ev.d1 == 51 || ev.d1 == 52 || ev.d1 == 55 || ev.d1 == 57 || ev.d1 == 59 ) {
                    voice.drumKind = 4; // Cymbal
                    voice.decayStep = 1.0f / ( 0.55f * midiSampleRate );
                }
                else {
                    voice.drumKind = 5; // Toms / percussion
                    voice.baseFreq = 75.0f + static_cast<float>( ( ev.d1 % 12 ) * 14 );
                    voice.decayStep = 1.0f / ( 0.20f * midiSampleRate );
                }
            }
            else {
                const GmPatch patch = gmFamilyPatch( _channels[ch].program );
                voice.baseFreq = 440.0f * std::pow( 2.0f, ( static_cast<int>( ev.d1 ) - 69 ) / 12.0f );
                voice.modRatio = patch.modRatio;
                voice.modIndex = patch.modIndex;
                voice.sustainLevel = patch.sustainLevel;
                voice.attackStep = 1.0f / std::max( 1.0f, patch.attackSec * midiSampleRate );
                voice.decayStep = ( 1.0f - patch.sustainLevel ) / std::max( 1.0f, patch.decaySec * midiSampleRate );
                voice.releaseStep = std::max( 0.05f, patch.sustainLevel ) / std::max( 1.0f, patch.releaseSec * midiSampleRate );
            }
            *slot = voice;
        }

        void renderSlice( int16_t * dst, const size_t frames )
        {
            constexpr float invSr = 1.0f / static_cast<float>( midiSampleRate );
            for ( size_t f = 0; f < frames; ++f ) {
                float mixL = 0.0f;
                float mixR = 0.0f;
                for ( auto & v : _voices ) {
                    if ( v.stage == MidiVoice::Stage::OFF )
                        continue;

                    if ( v.stage == MidiVoice::Stage::ATTACK ) {
                        v.env += v.attackStep;
                        if ( v.env >= 1.0f ) {
                            v.env = 1.0f;
                            v.stage = MidiVoice::Stage::DECAY;
                        }
                    }
                    else if ( v.stage == MidiVoice::Stage::DECAY ) {
                        v.env -= v.decayStep;
                        if ( v.env <= v.sustainLevel ) {
                            v.env = v.sustainLevel;
                            v.stage = v.drum ? MidiVoice::Stage::OFF : MidiVoice::Stage::SUSTAIN;
                        }
                    }
                    else if ( v.stage == MidiVoice::Stage::RELEASE ) {
                        v.env -= v.releaseStep;
                        if ( v.env <= 0.0f ) {
                            v.env = 0.0f;
                            v.stage = MidiVoice::Stage::OFF;
                            continue;
                        }
                    }

                    const auto & ch = _channels[v.channel];
                    float sample = 0.0f;
                    if ( v.drum ) {
                        const uint32_t bit = ( ( v.lfsr >> 0 ) ^ ( v.lfsr >> 2 ) ^ ( v.lfsr >> 3 ) ^ ( v.lfsr >> 5 ) ) & 1u;
                        v.lfsr = ( v.lfsr >> 1 ) | ( bit << 15 );
                        const float noise = ( ( v.lfsr & 1u ) ? 1.0f : -1.0f ) * 0.65f;
                        if ( v.drumKind == 1 || v.drumKind == 5 ) {
                            const float freq = v.baseFreq * ( 0.35f + 0.65f * v.env );
                            v.phaseCar += freq * invSr;
                            sample = lookupSin( v.phaseCar ) * 1.15f;
                        }
                        else if ( v.drumKind == 2 ) {
                            v.phaseCar += v.baseFreq * invSr;
                            sample = lookupSin( v.phaseCar ) * 0.45f + noise * 0.65f;
                        }
                        else {
                            sample = noise * 0.70f;
                        }
                    }
                    else {
                        const float freq = v.baseFreq * ch.pitchRatio;
                        v.phaseCar += freq * invSr;
                        v.phaseMod += freq * v.modRatio * invSr;
                        const float mod = lookupSin( v.phaseMod ) * ( v.modIndex * ( 0.35f + 0.65f * v.env ) / 6.2831853f );
                        sample = lookupSin( v.phaseCar + mod );
                    }

                    const float chGain = ( static_cast<float>( ch.volume ) / 127.0f ) * ( static_cast<float>( ch.expression ) / 127.0f );
                    const float panNorm = static_cast<float>( ch.pan ) / 127.0f;
                    const float amp = sample * v.env * v.velGain * chGain * 0.24f;
                    mixL += amp * ( 1.0f - panNorm * 0.45f );
                    mixR += amp * ( 0.55f + panNorm * 0.45f );
                }

                // Soft-clip headroom limiter avoids harsh overflow on dense orchestral chords.
                const auto clampSample = []( float x ) -> int16_t {
                    x = x / ( 1.0f + std::abs( x ) * 0.25f );
                    return static_cast<int16_t>( std::clamp( static_cast<long>( x * 32767.0f ), -32767L, 32767L ) );
                };
                dst[f * 2] = clampSample( mixL );
                dst[f * 2 + 1] = clampSample( mixR );
            }
        }
    };

    struct MusicTrack
    {
        std::string path;
        std::vector<uint8_t> midiData;
        ogg_int64_t resumeFrame{ 0 };
    };
    struct MusicBuffer
    {
        ndspWaveBuf wave{};
        ogg_int64_t startFrame{ 0 };
        uint64_t order{ 0 };
    };
    struct MusicStream
    {
        OggVorbis_File decoder{};
        bool decoderOpen{ false };
        std::unique_ptr<MidiSynth> midi;
        std::array<MusicBuffer, musicBufferCount> buffers{};
        void * pcm{ nullptr };
        MusicTrack * track{ nullptr };
        int channels{ 0 };
        long sampleRate{ 0 };
        ogg_int64_t totalFrames{ 0 };
        Music::PlaybackMode mode{ Music::PlaybackMode::PLAY_ONCE };
        int fadeMs{ 0 };
        ::Thread thread{ nullptr };
        std::atomic<bool> stop{ false };
        std::atomic<bool> playing{ false };
        ~MusicStream()
        {
            // The owner joins the worker and clears NDSP before destruction.
            if ( pcm )
                linearFree( pcm );
            if ( decoderOpen )
                ov_clear( &decoder );
        }
    };
    // API calls serialize start/stop/cache access; the worker never acquires this
    // mutex, so stop can join without deadlocking it. NDSP access uses audioMutex.
    std::mutex musicControlMutex;
    std::map<uint64_t, MusicTrack> musicTracks;
    std::unique_ptr<MusicStream> musicStream;

    size_t fillMusicBuffer( MusicStream & stream, MusicBuffer & buffer, const int frameBytes, bool & eof )
    {
        if ( stream.midi ) {
            buffer.startFrame = stream.midi->currentFrame();
            const size_t bytes = stream.midi->render( buffer.wave.data_pcm16, musicBufferBytes, stream.mode != Music::PlaybackMode::PLAY_ONCE, eof );
            return bytes - bytes % static_cast<size_t>( frameBytes );
        }

        buffer.startFrame = ov_pcm_tell( &stream.decoder );
        size_t bytes = 0;
        int holes = 0;
        while ( bytes < musicBufferBytes && !stream.stop.load() ) {
            int bitstream = 0;
            const long decoded
                = ov_read( &stream.decoder, reinterpret_cast<char *>( buffer.wave.data_pcm16 ) + bytes, static_cast<int>( musicBufferBytes - bytes ), &bitstream );
            if ( decoded > 0 ) {
                const auto * info = ov_info( &stream.decoder, bitstream );
                // Chained files that change format cannot share one NDSP queue.
                if ( !info || info->channels != stream.channels || info->rate != stream.sampleRate ) {
                    eof = true;
                    break;
                }
                bytes += static_cast<size_t>( decoded );
                holes = 0;
            }
            else if ( decoded == OV_HOLE && ++holes <= 8 )
                continue;
            else if ( decoded == 0 ) {
                if ( stream.mode == Music::PlaybackMode::PLAY_ONCE )
                    eof = true;
                else if ( ov_pcm_seek( &stream.decoder, 0 ) != 0 )
                    eof = true;
                // Submit the tail separately, keeping accurate resume positions.
                if ( bytes || eof )
                    break;
                buffer.startFrame = 0;
                // Reject an empty stream instead of endlessly seeking its EOF.
                if ( ++holes > 1 ) {
                    eof = true;
                    break;
                }
            }
            else {
                eof = true;
                break;
            }
        }
        return bytes - bytes % static_cast<size_t>( frameBytes );
    }

    void decodeMusic( void * arg )
    {
        auto & stream = *static_cast<MusicStream *>( arg );
        const int frameBytes = stream.channels * int( sizeof( int16_t ) );
        const uint64_t startedAt = osGetTime();
        bool eof = false;
        uint64_t nextOrder = 1;
        uint64_t lastCompletedOrder = 0;
        ogg_int64_t completedPosition = stream.track->resumeFrame;
        while ( !stream.stop.load() ) {
            if ( streamPaused.load() ) {
                svcSleepThread( 20 * 1000 * 1000 );
                continue;
            }
            bool anyQueued = false;
            for ( auto & buffer : stream.buffers ) {
                bool available = false;
                {
                    std::lock_guard<std::mutex> lock( audioMutex );
                    if ( !ready ) {
                        stream.stop.store( true );
                        break;
                    }
                    available = buffer.wave.status == NDSP_WBUF_DONE || buffer.wave.status == NDSP_WBUF_FREE;
                    if ( !available )
                        anyQueued = true;
                    if ( available && buffer.order > lastCompletedOrder ) {
                        lastCompletedOrder = buffer.order;
                        completedPosition = buffer.startFrame + buffer.wave.nsamples;
                    }
                }
                if ( !available || stream.stop.load() || eof )
                    continue;
                const size_t bytes = fillMusicBuffer( stream, buffer, frameBytes, eof );
                if ( bytes && !stream.stop.load() ) {
                    DSP_FlushDataCache( buffer.wave.data_vaddr, static_cast<uint32_t>( bytes ) );
                    std::lock_guard<std::mutex> lock( audioMutex );
                    if ( !ready ) {
                        stream.stop.store( true );
                        break;
                    }
                    buffer.wave.nsamples = static_cast<uint32_t>( bytes / frameBytes );
                    buffer.wave.looping = false;
                    buffer.order = nextOrder++;
                    ndspChnWaveBufAdd( musicChannel, &buffer.wave );
                    anyQueued = true;
                }
            }
            {
                std::lock_guard<std::mutex> lock( audioMutex );
                if ( ready ) {
                    musicFade = stream.fadeMs <= 0 ? 1.0f : std::min( 1.0f, static_cast<float>( osGetTime() - startedAt ) / stream.fadeMs );
                    applyMusicVolume();
                }
            }
            if ( eof && !anyQueued )
                break;
            // Yield cooperatively; stop is checked at most 5 ms after queued audio.
            svcSleepThread( 5 * 1000 * 1000 );
        }
        {
            std::lock_guard<std::mutex> lock( audioMutex );
            // Keep the audible position rather than the decoder's read-ahead position.
            for ( const auto & buffer : stream.buffers ) {
                if ( buffer.wave.status == NDSP_WBUF_PLAYING ) {
                    completedPosition = buffer.startFrame + std::min( ndspChnGetSamplePos( musicChannel ), buffer.wave.nsamples );
                    break;
                }
            }
            ndspChnWaveBufClear( musicChannel );
        }
        stream.track->resumeFrame = stream.mode == Music::PlaybackMode::RESUME_AND_PLAY_INFINITE && stream.totalFrames > 0
                                        ? std::max<ogg_int64_t>( 0, completedPosition ) % stream.totalFrames
                                        : 0;
        stream.playing.store( false );
    }

    void stopMusicInternally()
    {
        if ( !musicStream )
            return;
        musicStream->stop.store( true );
        if ( musicStream->thread ) {
            threadJoin( musicStream->thread, UINT64_MAX );
            threadFree( musicStream->thread );
        }
        {
            std::lock_guard<std::mutex> lock( audioMutex );
            ndspChnWaveBufClear( musicChannel );
        }
        musicStream.reset();
    }

    bool startMusicInternally( MusicTrack & track, const Music::PlaybackMode mode )
    {
        stopMusicInternally();
        {
            std::lock_guard<std::mutex> lock( audioMutex );
            if ( !ready )
                return false;
        }
        auto stream = std::make_unique<MusicStream>();
        if ( !track.midiData.empty() ) {
            stream->midi = std::make_unique<MidiSynth>();
            if ( !stream->midi->load( track.midiData ) )
                return false;
            stream->totalFrames = stream->midi->totalFrames();
            stream->channels = 2;
            stream->sampleRate = midiSampleRate;
            if ( mode != Music::PlaybackMode::RESUME_AND_PLAY_INFINITE )
                track.resumeFrame = 0;
            if ( track.resumeFrame > 0 )
                stream->midi->seek( track.resumeFrame % stream->totalFrames );
        }
        else {
            FILE * file = std::fopen( track.path.c_str(), "rb" );
            if ( !file )
                return false;
            if ( ov_open( file, &stream->decoder, nullptr, 0 ) != 0 ) {
                std::fclose( file );
                return false;
            }
            stream->decoderOpen = true;
            const auto * info = ov_info( &stream->decoder, -1 );
            if ( !info || ( info->channels != 1 && info->channels != 2 ) || info->rate <= 0 || info->rate > 192000 || !ov_seekable( &stream->decoder ) )
                return false;
            stream->totalFrames = ov_pcm_total( &stream->decoder, -1 );
            if ( stream->totalFrames <= 0 )
                return false;
            stream->channels = info->channels;
            stream->sampleRate = info->rate;
            if ( mode != Music::PlaybackMode::RESUME_AND_PLAY_INFINITE )
                track.resumeFrame = 0;
            if ( track.resumeFrame > 0 && ov_pcm_seek( &stream->decoder, track.resumeFrame % stream->totalFrames ) != 0 )
                track.resumeFrame = 0;
        }
        stream->track = &track;
        stream->mode = mode;
        stream->pcm = linearAlloc( musicBufferBytes * musicBufferCount );
        if ( !stream->pcm )
            return false;
        for ( size_t i = 0; i < stream->buffers.size(); ++i ) {
            stream->buffers[i].wave.data_vaddr = static_cast<uint8_t *>( stream->pcm ) + i * musicBufferBytes;
            stream->buffers[i].wave.status = NDSP_WBUF_DONE;
        }
        {
            std::lock_guard<std::mutex> lock( audioMutex );
            if ( !ready )
                return false;
            stream->fadeMs = fadeInMs;
            musicFade = fadeInMs > 0 ? 0.0f : 1.0f;
            ndspChnReset( musicChannel );
            ndspChnSetInterp( musicChannel, NDSP_INTERP_LINEAR );
            ndspChnSetRate( musicChannel, static_cast<float>( stream->sampleRate ) );
            ndspChnSetFormat( musicChannel, stream->channels == 1 ? NDSP_FORMAT_MONO_PCM16 : NDSP_FORMAT_STEREO_PCM16 );
            applyMusicVolume();
        }
        stream->playing.store( true );
        // One application-core worker works on both Old and New 3DS, without
        // requesting a privileged system core or assuming additional CPU cores.
        stream->thread = threadCreate( decodeMusic, stream.get(), 64 * 1024, 0x2F, 0, false );
        if ( !stream->thread )
            return false;
        musicStream = std::move( stream );
        return true;
    }
}
namespace Music
{
    bool Play( const uint64_t uid, const PlaybackMode mode )
    {
        std::lock_guard<std::mutex> lock( musicControlMutex );
        const auto track = musicTracks.find( uid );
        return track != musicTracks.end() && startMusicInternally( track->second, mode );
    }
    void Play( const uint64_t uid, const std::vector<uint8_t> & v, const PlaybackMode mode )
    {
        std::lock_guard<std::mutex> lock( musicControlMutex );
        stopMusicInternally();
        auto & track = musicTracks[uid];
        if ( !track.path.empty() || track.midiData != v ) {
            track.path.clear();
            track.midiData = v;
            track.resumeFrame = 0;
        }
        if ( !startMusicInternally( track, mode ) )
            ERROR_LOG( "Unable to play MIDI music track" )
    }
    void Play( const uint64_t uid, const std::string & file, const PlaybackMode mode )
    {
        std::lock_guard<std::mutex> lock( musicControlMutex );
        stopMusicInternally();
        auto & track = musicTracks[uid];
        if ( track.path != file || !track.midiData.empty() ) {
            track.path = file;
            track.midiData.clear();
            track.resumeFrame = 0;
        }
        if ( !startMusicInternally( track, mode ) )
            ERROR_LOG( "Unable to stream OGG music: " << file )
    }
    void setVolume( int percent )
    {
        std::lock_guard<std::mutex> lock( audioMutex );
        musicVolume = std::clamp( percent, 0, 100 ) / 100.0f;
        if ( ready )
            applyMusicVolume();
    }
    void SetFadeInMs( int timeMs )
    {
        std::lock_guard<std::mutex> lock( audioMutex );
        fadeInMs = std::max( 0, timeMs );
    }
    void Stop()
    {
        std::lock_guard<std::mutex> lock( musicControlMutex );
        stopMusicInternally();
    }
    bool isPlaying()
    {
        std::lock_guard<std::mutex> lock( musicControlMutex );
        return musicStream && musicStream->playing.load();
    }
    void setMidiSoundFonts( const ListFiles & ) {}
    void setMidiTimidityCfg( const std::string & ) {}
}
#endif
