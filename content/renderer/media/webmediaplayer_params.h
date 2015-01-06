// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBMEDIAPLAYER_PARAMS_H_
#define CONTENT_RENDERER_MEDIA_WEBMEDIAPLAYER_PARAMS_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "content/renderer/media/crypto/encrypted_media_player_support.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {
class WebMediaPlayerClient;
}

namespace media {
class AudioHardwareConfig;
class AudioRendererSink;
class GpuVideoAcceleratorFactories;
class MediaLog;
}

namespace content {

// Holds parameters for constructing WebMediaPlayerImpl without having
// to plumb arguments through various abstraction layers.
class WebMediaPlayerParams {
 public:
  // Callback used to create EncryptedMediaPlayerSupport instances. This
  // callback must always return a valid EncryptedMediaPlayerSupport object.
  typedef base::Callback<scoped_ptr<EncryptedMediaPlayerSupport>(
      blink::WebMediaPlayerClient*)> EncryptedMediaPlayerSupportCreateCB;

  // |defer_load_cb|, |audio_renderer_sink|, and |compositor_task_runner| may be
  // null.
  WebMediaPlayerParams(
      const base::Callback<void(const base::Closure&)>& defer_load_cb,
      const scoped_refptr<media::AudioRendererSink>& audio_renderer_sink,
      const media::AudioHardwareConfig& audio_hardware_config,
      const scoped_refptr<media::MediaLog>& media_log,
      const scoped_refptr<media::GpuVideoAcceleratorFactories>& gpu_factories,
      const scoped_refptr<base::SingleThreadTaskRunner>&
          media_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>&
          compositor_task_runner,
      const EncryptedMediaPlayerSupportCreateCB&
          encrypted_media_player_support_cb);

  ~WebMediaPlayerParams();

  base::Callback<void(const base::Closure&)> defer_load_cb() const {
    return defer_load_cb_;
  }

  const scoped_refptr<media::AudioRendererSink>& audio_renderer_sink() const {
    return audio_renderer_sink_;
  }

  const media::AudioHardwareConfig& audio_hardware_config() const {
    return audio_hardware_config_;
  }

  const scoped_refptr<media::MediaLog>& media_log() const {
    return media_log_;
  }

  const scoped_refptr<media::GpuVideoAcceleratorFactories>&
  gpu_factories() const {
    return gpu_factories_;
  }

  const scoped_refptr<base::SingleThreadTaskRunner>&
  media_task_runner() const {
    return media_task_runner_;
  }

  const scoped_refptr<base::SingleThreadTaskRunner>&
  compositor_task_runner() const {
    return compositor_task_runner_;
  }

  scoped_ptr<EncryptedMediaPlayerSupport>
  CreateEncryptedMediaPlayerSupport(blink::WebMediaPlayerClient* client) const;

 private:
  base::Callback<void(const base::Closure&)> defer_load_cb_;
  scoped_refptr<media::AudioRendererSink> audio_renderer_sink_;
  const media::AudioHardwareConfig& audio_hardware_config_;
  scoped_refptr<media::MediaLog> media_log_;
  scoped_refptr<media::GpuVideoAcceleratorFactories> gpu_factories_;
  scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;
  EncryptedMediaPlayerSupportCreateCB encrypted_media_player_support_cb_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(WebMediaPlayerParams);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBMEDIAPLAYER_PARAMS_H_
