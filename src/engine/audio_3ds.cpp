// SPDX-License-Identifier: GPL-2.0-or-later
#if defined( TARGET_NINTENDO_3DS )
#include "audio.h"
#include <3ds.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <mutex>

namespace
{
    constexpr int maxChannels = 24;
    struct Channel
    {
        ndspWaveBuf wave{};
        void * data{ nullptr };
    };
    std::array<Channel, maxChannels> channels;
    std::mutex audioMutex;
    bool ready = false;
    bool muted = false;
    int channelCount = maxChannels;
    float volume = 1.0f;
    uint16_t read16( const uint8_t * ptr ) { return static_cast<uint16_t>( ptr[0] | ( ptr[1] << 8 ) ); }
    uint32_t read32( const uint8_t * ptr ) { return ptr[0] | ( uint32_t( ptr[1] ) << 8 ) | ( uint32_t( ptr[2] ) << 16 ) | ( uint32_t( ptr[3] ) << 24 ); }
    void release( int id )
    {
        ndspChnWaveBufClear( id );
        if ( channels[id].data ) linearFree( channels[id].data );
        channels[id] = {};
    }
    void applyVolume( int id )
    {
        float mix[12]{};
        mix[0] = mix[1] = muted ? 0.0f : volume;
        ndspChnSetMix( id, mix );
    }
}
namespace Audio
{
    void Init()
    {
        std::lock_guard<std::mutex> lock( audioMutex );
        if ( !ready ) ready = R_SUCCEEDED( ndspInit() );
    }
    void Quit()
    {
        std::lock_guard<std::mutex> lock( audioMutex );
        if ( !ready ) return;
        for ( int id = 0; id < maxChannels; ++id ) release( id );
        ndspExit(); ready = false;
    }
    bool isValid() { std::lock_guard<std::mutex> lock( audioMutex ); return ready; }
    void Mute() { std::lock_guard<std::mutex> lock( audioMutex ); muted = true; if ( ready ) for ( int id = 0; id < channelCount; ++id ) applyVolume( id ); }
    void Unmute() { std::lock_guard<std::mutex> lock( audioMutex ); muted = false; if ( ready ) for ( int id = 0; id < channelCount; ++id ) applyVolume( id ); }
}
namespace Mixer
{
    void SetChannels( int count ) { std::lock_guard<std::mutex> lock( audioMutex ); channelCount = std::clamp( count, 1, maxChannels ); }
    int getChannelCount() { std::lock_guard<std::mutex> lock( audioMutex ); return channelCount; }
    int Play( const uint8_t * ptr, uint32_t size, bool loop, const std::optional<std::pair<int16_t, uint8_t>> )
    {
        std::lock_guard<std::mutex> lock( audioMutex );
        if ( !ready || !ptr || size < 12 || std::memcmp( ptr, "RIFF", 4 ) || std::memcmp( ptr + 8, "WAVE", 4 ) ) return -1;
        uint16_t format = 0, bits = 0, count = 0;
        uint32_t rate = 0, pcmSize = 0;
        const uint8_t * pcm = nullptr;
        for ( uint32_t offset = 12; offset <= size - 8; ) {
            const uint32_t length = read32( ptr + offset + 4 );
            if ( length > size - offset - 8 ) return -1;
            if ( !std::memcmp( ptr + offset, "fmt ", 4 ) && length >= 16 ) {
                format = read16( ptr + offset + 8 ); count = read16( ptr + offset + 10 );
                rate = read32( ptr + offset + 12 ); bits = read16( ptr + offset + 22 );
            }
            else if ( !std::memcmp( ptr + offset, "data", 4 ) ) { pcm = ptr + offset + 8; pcmSize = length; }
            if ( length == UINT32_MAX || length + ( length & 1U ) > size - offset - 8 ) break;
            offset += 8 + length + ( length & 1U );
        }
        if ( format != 1 || !pcm || !pcmSize || !rate || ( count != 1 && count != 2 ) || ( bits != 8 && bits != 16 ) ) return -1;
        int id = 0;
        for ( ; id < channelCount; ++id ) if ( !channels[id].data || channels[id].wave.status == NDSP_WBUF_DONE ) break;
        if ( id == channelCount ) return -1;
        release( id );
        // NDSP consumes signed PCM8; WAV PCM8 uses an unsigned midpoint.
        channels[id].data = linearAlloc( pcmSize );
        if ( !channels[id].data ) return -1;
        std::memcpy( channels[id].data, pcm, pcmSize );
        if ( bits == 8 ) { auto * samples = static_cast<uint8_t *>( channels[id].data ); for ( uint32_t i = 0; i < pcmSize; ++i ) samples[i] ^= 128; }
        DSP_FlushDataCache( channels[id].data, pcmSize );
        ndspChnReset( id ); ndspChnSetInterp( id, NDSP_INTERP_LINEAR ); ndspChnSetRate( id, static_cast<float>( rate ) );
        const uint16_t ndspFormat = bits == 8 ? ( count == 1 ? NDSP_FORMAT_MONO_PCM8 : NDSP_FORMAT_STEREO_PCM8 )
                                             : ( count == 1 ? NDSP_FORMAT_MONO_PCM16 : NDSP_FORMAT_STEREO_PCM16 );
        ndspChnSetFormat( id, ndspFormat ); applyVolume( id );
        auto & wave = channels[id].wave;
        wave.data_vaddr = channels[id].data; wave.nsamples = pcmSize / ( count * ( bits / 8 ) ); wave.looping = loop;
        ndspChnWaveBufAdd( id, &wave ); return id;
    }
    void Stop( int id ) { std::lock_guard<std::mutex> lock( audioMutex ); if ( !ready ) return; if ( id < 0 ) for ( int i = 0; i < maxChannels; ++i ) release( i ); else if ( id < maxChannels ) release( id ); }
    bool isPlaying( int id ) { std::lock_guard<std::mutex> lock( audioMutex ); return ready && id >= 0 && id < maxChannels && channels[id].data && channels[id].wave.status != NDSP_WBUF_DONE; }
    void setVolume( int percent ) { std::lock_guard<std::mutex> lock( audioMutex ); volume = std::clamp( percent, 0, 100 ) / 100.0f; if ( ready ) for ( int id = 0; id < channelCount; ++id ) applyVolume( id ); }
    void setPosition( int, int16_t, uint8_t ) {}
}
// Music decoding (OGG/MP3/MIDI) needs a streaming decoder. Keep that limitation
// explicit; NDSP sound effects work independently of music support.
namespace Music
{
    bool Play( uint64_t, PlaybackMode ) { return false; }
    void Play( uint64_t, const std::vector<uint8_t> &, PlaybackMode ) {}
    void Play( uint64_t, const std::string &, PlaybackMode ) {}
    void setVolume( int ) {}
    void SetFadeInMs( int ) {}
    void Stop() {}
    bool isPlaying() { return false; }
    void setMidiSoundFonts( const ListFiles & ) {}
    void setMidiTimidityCfg( const std::string & ) {}
}
#endif
