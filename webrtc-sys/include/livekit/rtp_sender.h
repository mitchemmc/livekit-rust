#pragma once

#include <memory>

#include "api/rtp_sender_interface.h"
#include "livekit/media_stream.h"
#include "livekit/rtc_error.h"
#include "livekit/rtp_parameters.h"
#include "rust/cxx.h"

namespace livekit {
class RtpSender;
}
#include "webrtc-sys/src/rtp_sender.rs.h"

namespace livekit {

// TODO(theomonnom): FrameTransformer & FrameEncryptor interface
class RtpSender {
 public:
  explicit RtpSender(rtc::scoped_refptr<webrtc::RtpSenderInterface> sender);

  bool set_track(std::shared_ptr<MediaStreamTrack> track) const;

  std::shared_ptr<MediaStreamTrack> track() const;

  uint32_t ssrc() const;

  MediaType media_type() const;

  rust::String id() const;

  rust::Vec<rust::String> stream_ids() const;

  void set_streams(const rust::Vec<rust::String>& stream_ids) const;

  rust::Vec<RtpEncodingParameters> init_send_encodings() const;

  RtpParameters get_parameters() const;

  void set_parameters(RtpParameters params) const;

  rtc::scoped_refptr<webrtc::RtpSenderInterface> get() const { return sender_; }

 private:
  rtc::scoped_refptr<webrtc::RtpSenderInterface> sender_;
};

static std::shared_ptr<RtpSender> _shared_rtp_sender() {
  return nullptr;  // Ignore
}
}  // namespace livekit