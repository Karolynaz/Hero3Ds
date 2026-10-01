// SPDX-License-Identifier: GPL-2.0-or-later
#if defined( TARGET_NINTENDO_3DS )
#include "audio.h"
#include <3ds.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <map>
#include <memory>
#include <tremor/ivorbisfile.h>
#include "logging.h"
#include <cstring>
#include <mutex>

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
    int channelCount = maxChannels;
    float volume = 1.0f;
    float musicVolume = 1.0f;
    float musicFade = 1.0f;
    int fadeInMs = 0;
    void applyMusicVolume()
    {
        float mix[12]{};
        mix[0] = mix[1] = muted ? 0.0f : musicVolume * musicFade;
        ndspChnSetMix( musicChannel, mix );
    }

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
        {
            std::lock_guard<std::mutex> lock( audioMutex );
            if ( !ready ) return;
            // Block new playback before joining the decoder thread.
            ready = false;
        }
        Music::Stop();
        std::lock_guard<std::mutex> lock( audioMutex );
        for ( int id = 0; id < maxChannels; ++id ) release( id );
        ndspExit();
    }
    bool isValid() { std::lock_guard<std::mutex> lock( audioMutex ); return ready; }
    void Mute() { std::lock_guard<std::mutex> lock( audioMutex ); muted = true; if ( ready ) { for ( int id = 0; id < channelCount; ++id ) applyVolume( id ); applyMusicVolume(); } }
    void Unmute() { std::lock_guard<std::mutex> lock( audioMutex ); muted = false; if ( ready ) { for ( int id = 0; id < channelCount; ++id ) applyVolume( id ); applyMusicVolume(); } }
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
namespace
{
    constexpr size_t musicBufferBytes = 16 * 1024;
    constexpr size_t musicBufferCount = 3;
    struct MusicTrack
    {
        std::string path;
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
            if ( pcm ) linearFree( pcm );
            if ( decoderOpen ) ov_clear( &decoder );
        }
    };
    // API calls serialize start/stop/cache access; the worker never acquires this
    // mutex, so stop can join without deadlocking it. NDSP access uses audioMutex.
    std::mutex musicControlMutex;
    std::map<uint64_t, MusicTrack> musicTracks;
    std::unique_ptr<MusicStream> musicStream;

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
            bool anyQueued = false;
            for ( auto & buffer : stream.buffers ) {
                bool available = false;
                {
                    std::lock_guard<std::mutex> lock( audioMutex );
                    if ( !ready ) { stream.stop.store( true ); break; }
                    available = buffer.wave.status == NDSP_WBUF_DONE || buffer.wave.status == NDSP_WBUF_FREE;
                    if ( !available ) anyQueued = true;
                    if ( available && buffer.order > lastCompletedOrder ) {
                        lastCompletedOrder = buffer.order;
                        completedPosition = buffer.startFrame + buffer.wave.nsamples;
                    }
                }
                if ( !available || stream.stop.load() || eof ) continue;
                buffer.startFrame = ov_pcm_tell( &stream.decoder );
                size_t bytes = 0;
                int holes = 0;
                while ( bytes < musicBufferBytes && !stream.stop.load() ) {
                    int bitstream = 0;
                    const long decoded = ov_read( &stream.decoder, static_cast<char *>( buffer.wave.data_vaddr ) + bytes,
                                                  static_cast<int>( musicBufferBytes - bytes ), &bitstream );
                    if ( decoded > 0 ) {
                        const auto * info = ov_info( &stream.decoder, bitstream );
                        // Chained files that change format cannot share one NDSP queue.
                        if ( !info || info->channels != stream.channels || info->rate != stream.sampleRate ) { eof = true; break; }
                        bytes += static_cast<size_t>( decoded ); holes = 0;
                    }
                    else if ( decoded == OV_HOLE && ++holes <= 8 ) continue;
                    else if ( decoded == 0 ) {
                        if ( stream.mode == Music::PlaybackMode::PLAY_ONCE ) eof = true;
                        else if ( ov_pcm_seek( &stream.decoder, 0 ) != 0 ) eof = true;
                        // Submit the tail separately, keeping accurate resume positions.
                        if ( bytes || eof ) break;
                        buffer.startFrame = 0;
                        // Reject an empty stream instead of endlessly seeking its EOF.
                        if ( ++holes > 1 ) { eof = true; break; }
                    }
                    else { eof = true; break; }
                }
                bytes -= bytes % static_cast<size_t>( frameBytes );
                if ( bytes && !stream.stop.load() ) {
                    DSP_FlushDataCache( buffer.wave.data_vaddr, static_cast<uint32_t>( bytes ) );
                    std::lock_guard<std::mutex> lock( audioMutex );
                    if ( !ready ) { stream.stop.store( true ); break; }
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
            if ( eof && !anyQueued ) break;
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
                                      ? std::max<ogg_int64_t>( 0, completedPosition ) % stream.totalFrames : 0;
        stream.playing.store( false );
    }

    void stopMusicInternally()
    {
        if ( !musicStream ) return;
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
            if ( !ready ) return false;
        }
        FILE * file = std::fopen( track.path.c_str(), "rb" );
        if ( !file ) return false;
        auto stream = std::make_unique<MusicStream>();
        if ( ov_open( file, &stream->decoder, nullptr, 0 ) != 0 ) {
            std::fclose( file );
            return false;
        }
        stream->decoderOpen = true;
        const auto * info = ov_info( &stream->decoder, -1 );
        if ( !info || ( info->channels != 1 && info->channels != 2 ) || info->rate <= 0 || info->rate > 192000 || !ov_seekable( &stream->decoder ) ) return false;
        stream->totalFrames = ov_pcm_total( &stream->decoder, -1 );
        if ( stream->totalFrames <= 0 ) return false;
        stream->track = &track; stream->channels = info->channels; stream->sampleRate = info->rate; stream->mode = mode;
        if ( mode != Music::PlaybackMode::RESUME_AND_PLAY_INFINITE ) track.resumeFrame = 0;
        if ( track.resumeFrame > 0 && ov_pcm_seek( &stream->decoder, track.resumeFrame % stream->totalFrames ) != 0 ) track.resumeFrame = 0;
        stream->pcm = linearAlloc( musicBufferBytes * musicBufferCount );
        if ( !stream->pcm ) return false;
        for ( size_t i = 0; i < stream->buffers.size(); ++i ) {
            stream->buffers[i].wave.data_vaddr = static_cast<uint8_t *>( stream->pcm ) + i * musicBufferBytes;
            stream->buffers[i].wave.status = NDSP_WBUF_DONE;
        }
        {
            std::lock_guard<std::mutex> lock( audioMutex );
            if ( !ready ) return false;
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
        if ( !stream->thread ) return false;
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
    void Play( uint64_t, const std::vector<uint8_t> &, PlaybackMode )
    {
        // Original AGG music is MIDI: an OGG decoder cannot synthesize it.
        Stop();
    }
    void Play( const uint64_t uid, const std::string & file, const PlaybackMode mode )
    {
        std::lock_guard<std::mutex> lock( musicControlMutex );
        stopMusicInternally();
        auto & track = musicTracks[uid];
        if ( track.path != file ) { track.path = file; track.resumeFrame = 0; }
        if ( !startMusicInternally( track, mode ) ) ERROR_LOG( "Unable to stream OGG music: " << file )
    }
    void setVolume( int percent ) { std::lock_guard<std::mutex> lock( audioMutex ); musicVolume = std::clamp( percent, 0, 100 ) / 100.0f; if ( ready ) applyMusicVolume(); }
    void SetFadeInMs( int timeMs ) { std::lock_guard<std::mutex> lock( audioMutex ); fadeInMs = std::max( 0, timeMs ); }
    void Stop() { std::lock_guard<std::mutex> lock( musicControlMutex ); stopMusicInternally(); }
    bool isPlaying() { std::lock_guard<std::mutex> lock( musicControlMutex ); return musicStream && musicStream->playing.load(); }
    void setMidiSoundFonts( const ListFiles & ) {}
    void setMidiTimidityCfg( const std::string & ) {}
}
#endif
