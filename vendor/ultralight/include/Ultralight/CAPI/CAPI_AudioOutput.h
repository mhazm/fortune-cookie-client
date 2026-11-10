///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///

///
/// @file CAPI_AudioOutput.h
///
/// User-defined audio output interface.
///
/// `#include <Ultralight/CAPI/CAPI_AudioOutput.h>`
///
/// The library uses this to play the sound of `<video>` and `<audio>` elements. It decodes the
/// audio and pushes the samples to your callbacks, which play them on a device.
///
/// A platform-specific implementation is provided for you when you call ulCreateApp(). If you are
/// using ulCreateRenderer() and don't provide one, media plays without sound.
///
/// Each callback matches the method of the same name in <Ultralight/platform/AudioOutput.h>, and
/// the rules documented there apply here too. Read that header before implementing this one.
///
/// \parblock
/// @warning  The library calls these callbacks from several of its own threads. `push_samples` can
///           still be running when `flush` or `stop` is called for the same stream, so protect
///           each stream's state.
/// \endparblock
///
/// \parblock
/// @warning  When your buffer is full, `push_samples` should wait for room instead of dropping
///           samples. A waiting `push_samples` must return as soon as `flush` or `stop` is called
///           for its stream, or the library can hang.
/// \endparblock
///
/// @note  `get_playback_position` is optional. Implement it for the most accurate audio-video
///        sync.
///
/// @pre  Requires the Pro edition or higher.
///
/// @see  ulPlatformSetAudioOutput()
///
#ifndef ULTRALIGHT_CAPI_AUDIOOUTPUT_H
#define ULTRALIGHT_CAPI_AUDIOOUTPUT_H

#include <Ultralight/CAPI/CAPI_Defines.h>

#if UL_HAS(MEDIA)

#ifdef __cplusplus
extern "C" {
#endif

///
/// An audio sample format (channel count and sample rate).
///
typedef struct {
  unsigned int channels;     ///< Number of channels (1 = mono, 2 = stereo, 6 = 5.1, 8 = 7.1).
  unsigned int sample_rate;  ///< Sample rate in Hz (eg, 48000).
} ULAudioFormat;

///
/// The callback invoked to create a new audio stream (before its first samples are pushed).
///
/// Samples will arrive in `format`. If you can't create the stream, ignore later calls with its id.
///
typedef void (*ULAudioOutputCreateStreamCallback)(void* user_data, unsigned int stream_id,
                                                  ULAudioFormat format);

///
/// The callback invoked to push decoded PCM samples into a stream (`num_frames * channels`
/// interleaved floats). When your buffer is full, wait for room instead of dropping samples.
///
typedef void (*ULAudioOutputPushSamplesCallback)(void* user_data, unsigned int stream_id,
                                                 const float* samples, unsigned int num_frames);

///
/// The callback invoked to set a stream's volume, from 0.0 (silent) to 1.0 (full volume).
///
typedef void (*ULAudioOutputSetVolumeCallback)(void* user_data, unsigned int stream_id,
                                               float volume);

///
/// The callback invoked to pause or resume a stream. Keep the buffered samples while paused.
///
typedef void (*ULAudioOutputSetPausedCallback)(void* user_data, unsigned int stream_id,
                                               bool paused);

///
/// The callback invoked when the media seeks, to discard a stream's buffered audio. Leave a paused
/// stream paused.
///
typedef void (*ULAudioOutputFlushCallback)(void* user_data, unsigned int stream_id);

///
/// The callback invoked when no more samples are coming for a stream. The stream stays valid, and
/// a `push_samples` call waiting for room must return.
///
typedef void (*ULAudioOutputStopCallback)(void* user_data, unsigned int stream_id);

///
/// The callback invoked to destroy a stream and free its resources.
///
typedef void (*ULAudioOutputDestroyStreamCallback)(void* user_data, unsigned int stream_id);

///
/// The callback invoked to get the number of frames of a stream the device has played since the
/// last flush (or since the stream was created). The count must not advance while the stream is
/// paused. Return 0 if your backend can't report this.
///
typedef unsigned long long (*ULAudioOutputGetPlaybackPositionCallback)(void* user_data,
                                                                       unsigned int stream_id);

///
/// The callback invoked to get the device's preferred audio format.
///
typedef ULAudioFormat (*ULAudioOutputGetPreferredOutputFormatCallback)(void* user_data);

///
/// User-defined audio output interface.
///
/// You should implement each of these callbacks, then pass an instance of this struct containing
/// your callbacks to ulPlatformSetAudioOutput().
///
typedef struct {
  ///
  /// A user-defined pointer passed to every callback (can be NULL).
  ///
  void* user_data;

  ///
  /// Called to create a stream.
  ///
  ULAudioOutputCreateStreamCallback create_stream;

  ///
  /// Called to push samples into a stream.
  ///
  ULAudioOutputPushSamplesCallback push_samples;

  ///
  /// Called to set a stream's volume.
  ///
  ULAudioOutputSetVolumeCallback set_volume;

  ///
  /// Called to pause or resume a stream.
  ///
  ULAudioOutputSetPausedCallback set_paused;

  ///
  /// Called to flush a stream on seek.
  ///
  ULAudioOutputFlushCallback flush;

  ///
  /// Called when no more samples are coming for a stream.
  ///
  ULAudioOutputStopCallback stop;

  ///
  /// Called to destroy a stream.
  ///
  ULAudioOutputDestroyStreamCallback destroy_stream;

  ///
  /// Called to read a stream's audio clock (can be NULL, in which case the position reads 0).
  ///
  ULAudioOutputGetPlaybackPositionCallback get_playback_position;

  ///
  /// Called to read the device's preferred format (can be NULL, in which case it is stereo at
  /// 48000 Hz).
  ///
  ULAudioOutputGetPreferredOutputFormatCallback get_preferred_output_format;
} ULAudioOutput;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // UL_HAS(MEDIA)

#endif // ULTRALIGHT_CAPI_AUDIOOUTPUT_H
