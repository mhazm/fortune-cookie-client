///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/Defines.h>

#if UL_HAS(MEDIA)

#include <cstdint>

namespace ultralight {

///
/// An audio sample format (channel count and sample rate).
///
struct AudioFormat {
  uint32_t channels;      ///< Number of channels (1 = mono, 2 = stereo, 6 = 5.1, 8 = 7.1).
  uint32_t sample_rate;   ///< Sample rate in Hz (eg, 48000).
};

///
/// The format of a new audio stream (the same as AudioFormat).
///
using AudioStreamFormat = AudioFormat;

///
/// User-defined audio output interface.
///
/// The library uses this to play the sound of `<video>` and `<audio>` elements. It decodes the
/// audio and pushes the samples to your implementation, which plays them on a device.
///
/// You can provide the library with your own AudioOutput implementation so that media audio plays
/// through your own audio engine.
///
/// ## Default Implementation
///
/// A platform-specific implementation of AudioOutput is provided for you when you call
/// App::Create(). Its source is in the SDK's `platform` folder if you want a starting point.
///
/// If you are using Renderer::Create() and don't provide one, media plays without sound.
///
/// ## Setting the Audio Output
///
/// To provide your own custom AudioOutput implementation, you should inherit from this class,
/// handle the virtual member functions, and then pass an instance of your class to
/// Platform::set_audio_output() before calling Renderer::Create() or App::Create(). It must stay
/// alive until the Renderer is destroyed.
///
/// ## Implementing an Audio Output
///
/// Each loaded media file plays through its own stream, identified by a `stream_id`. A stream goes
/// through these calls:
///
/// 1. CreateStream() gives you the stream's sample rate and channel count.
/// 2. PushSamples() delivers audio as it's decoded. SetVolume() and SetPaused() can come at any
///    time.
/// 3. Flush() discards the buffered audio when the media seeks, and new samples follow.
/// 4. Stop() tells you no more samples are coming. A later Flush() can start the stream again (eg,
///    when the media loops).
/// 5. DestroyStream() frees the stream. No more calls use its `stream_id`.
///
/// Samples are interleaved 32-bit floats in the range [-1.0, 1.0]. If your device wants a different
/// sample rate or channel count, resample and downmix in your implementation.
///
/// \parblock
/// @warning  The library calls these methods from several of its own threads, not only the
///           Renderer's thread. PushSamples() can still be running when Flush() or Stop() is
///           called for the same stream, so protect each stream's state.
/// \endparblock
///
/// \parblock
/// @warning  When your buffer is full, PushSamples() should wait for room instead of dropping
///           samples (the wait keeps decoding in step with playback). A waiting PushSamples() must
///           return as soon as Flush() or Stop() is called for its stream, or the library can hang.
/// \endparblock
///
/// @note  GetPlaybackPosition() is optional. Implement it for the most accurate audio-video sync.
///
/// @pre  Requires the Pro edition or higher.
///
/// @see  Platform::set_audio_output()
///
class UExport AudioOutput {
 public:
  virtual ~AudioOutput();

  ///
  /// Create a new audio stream.
  ///
  /// @param  stream_id  The id the library uses for this stream in every later call.
  ///
  /// @param  format     The sample rate and channel count of the samples you'll receive.
  ///
  /// @note  There's no way to report a failure. If you can't create the stream, ignore later calls
  ///        with this `stream_id`.
  ///
  virtual void CreateStream(uint32_t stream_id, const AudioStreamFormat& format) = 0;

  ///
  /// Push decoded samples into a stream.
  ///
  /// @param  stream_id   The stream to push samples to.
  ///
  /// @param  samples     `num_frames * channels` interleaved float values.
  ///
  /// @param  num_frames  The number of frames (one sample per channel).
  ///
  /// @warning  When your buffer is full, wait for room instead of dropping samples. Return as soon
  ///           as Flush() or Stop() is called for this stream.
  ///
  virtual void PushSamples(uint32_t stream_id, const float* samples,
                           uint32_t num_frames) = 0;

  ///
  /// Set the volume of a stream.
  ///
  /// @param  stream_id  The stream to change.
  ///
  /// @param  volume     The volume, from 0.0 (silent) to 1.0 (full volume).
  ///
  virtual void SetVolume(uint32_t stream_id, float volume) = 0;

  ///
  /// Pause or resume a stream.
  ///
  /// Keep the buffered samples while paused so playback resumes where it left off.
  ///
  /// @param  stream_id  The stream to pause or resume.
  ///
  /// @param  paused     `true` to pause, `false` to resume.
  ///
  virtual void SetPaused(uint32_t stream_id, bool paused) = 0;

  ///
  /// Discard a stream's buffered audio.
  ///
  /// Called when the media seeks. The next pushed sample is from the new position.
  ///
  /// @param  stream_id  The stream to flush.
  ///
  /// @note  Leave a paused stream paused. The library resumes it when the new position is ready.
  ///
  virtual void Flush(uint32_t stream_id) = 0;

  ///
  /// Tell a stream that no more samples are coming.
  ///
  /// Samples already in your buffer can finish playing. The stream stays valid until
  /// DestroyStream().
  ///
  /// @param  stream_id  The stream to stop.
  ///
  /// @warning  A PushSamples() call waiting for room must return without writing its samples.
  ///
  virtual void Stop(uint32_t stream_id) = 0;

  ///
  /// Destroy a stream and free its resources.
  ///
  /// The library calls Stop() first and waits for its pushes to finish, so no PushSamples() call is
  /// running for this stream.
  ///
  /// @param  stream_id  The stream to destroy.
  ///
  virtual void DestroyStream(uint32_t stream_id) = 0;

  ///
  /// Get the number of frames of a stream that have been played.
  ///
  /// The library uses this to keep the video in sync with the audio. Follow these rules:
  ///
  /// - **Count played frames.** Count frames the device has played (after its output latency, if
  ///   your backend reports it), not frames you've pushed or buffered.
  /// - **Reset on Flush().** Flush() sets the count to zero.
  /// - **Hold while paused.** The count must not advance while the stream is paused.
  ///
  /// @param  stream_id  The stream to query.
  ///
  /// @return  Returns the frame count, or 0 if your backend can't report it (the library then
  ///          times the video by the wall clock, which is less precise).
  ///
  virtual uint64_t GetPlaybackPosition(uint32_t stream_id) { return 0; }

  ///
  /// Get the device's preferred audio format.
  ///
  /// @return  Returns the device's preferred channel count and sample rate.
  ///
  virtual AudioFormat preferred_output_format() const { return { 2, 48000 }; }
};

} // namespace ultralight

#endif // UL_HAS(MEDIA)
