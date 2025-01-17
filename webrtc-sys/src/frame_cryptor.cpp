/*
 * Copyright 2023 LiveKit
 *
 * Licensed under the Apache License, Version 2.0 (the “License”);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an “AS IS” BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "livekit/frame_cryptor.h"

#include <memory>

#include "absl/types/optional.h"
#include "webrtc-sys/src/frame_cryptor.rs.h"

namespace livekit {

webrtc::FrameCryptorTransformer::Algorithm AlgorithmToFrameCryptorAlgorithm(
    Algorithm algorithm) {
  switch (algorithm) {
    case Algorithm::AesGcm:
      return webrtc::FrameCryptorTransformer::Algorithm::kAesGcm;
    case Algorithm::AesCbc:
      return webrtc::FrameCryptorTransformer::Algorithm::kAesCbc;
    default:
      return webrtc::FrameCryptorTransformer::Algorithm::kAesGcm;
  }
}

KeyProvider::KeyProvider(KeyProviderOptions options) {
  webrtc::KeyProviderOptions rtc_options;
  rtc_options.shared_key = options.shared_key;

  std::vector<uint8_t> ratchet_salt;
  std::copy(options.ratchet_salt.begin(), options.ratchet_salt.end(),
            std::back_inserter(ratchet_salt));

  rtc_options.ratchet_salt = ratchet_salt;

  std::vector<uint8_t> uncrypted_magic_bytes;
  std::copy(options.uncrypted_magic_bytes.begin(),
            options.uncrypted_magic_bytes.end(),
            std::back_inserter(uncrypted_magic_bytes));

  rtc_options.uncrypted_magic_bytes = uncrypted_magic_bytes;
  rtc_options.ratchet_window_size = options.ratchet_window_size;
  impl_ =
      new rtc::RefCountedObject<webrtc::DefaultKeyProviderImpl>(rtc_options);
}

FrameCryptor::FrameCryptor(
    const std::string participant_id,
    webrtc::FrameCryptorTransformer::Algorithm algorithm,
    rtc::scoped_refptr<webrtc::KeyProvider> key_provider,
    rtc::scoped_refptr<webrtc::RtpSenderInterface> sender)
    : participant_id_(participant_id),
      key_provider_(key_provider),
      sender_(sender) {
  auto mediaType =
      sender->track()->kind() == "audio"
          ? webrtc::FrameCryptorTransformer::MediaType::kAudioFrame
          : webrtc::FrameCryptorTransformer::MediaType::kVideoFrame;
  e2ee_transformer_ = rtc::scoped_refptr<webrtc::FrameCryptorTransformer>(
      new webrtc::FrameCryptorTransformer(participant_id, mediaType, algorithm,
                                          key_provider_));
  sender->SetEncoderToPacketizerFrameTransformer(e2ee_transformer_);
  e2ee_transformer_->SetEnabled(false);
}

FrameCryptor::FrameCryptor(
    const std::string participant_id,
    webrtc::FrameCryptorTransformer::Algorithm algorithm,
    rtc::scoped_refptr<webrtc::KeyProvider> key_provider,
    rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver)
    : participant_id_(participant_id),
      key_provider_(key_provider),
      receiver_(receiver) {
  auto mediaType =
      receiver->track()->kind() == "audio"
          ? webrtc::FrameCryptorTransformer::MediaType::kAudioFrame
          : webrtc::FrameCryptorTransformer::MediaType::kVideoFrame;
  e2ee_transformer_ = rtc::scoped_refptr<webrtc::FrameCryptorTransformer>(
      new webrtc::FrameCryptorTransformer(participant_id, mediaType, algorithm,
                                          key_provider_));
  receiver->SetDepacketizerToDecoderFrameTransformer(e2ee_transformer_);
  e2ee_transformer_->SetEnabled(false);
}

FrameCryptor::~FrameCryptor() {}

void FrameCryptor::register_observer(
    rust::Box<RtcFrameCryptorObserverWrapper> observer) const {
  webrtc::MutexLock lock(&mutex_);
  observer_ =
      std::make_unique<NativeFrameCryptorObserver>(std::move(observer), this);
  e2ee_transformer_->SetFrameCryptorTransformerObserver(observer_.get());
}

void FrameCryptor::unregister_observer() const {
  webrtc::MutexLock lock(&mutex_);
  observer_ = nullptr;
  e2ee_transformer_->SetFrameCryptorTransformerObserver(nullptr);
}

NativeFrameCryptorObserver::NativeFrameCryptorObserver(
    rust::Box<RtcFrameCryptorObserverWrapper> observer,
    const FrameCryptor* fc)
    : observer_(std::move(observer)), fc_(fc) {}

NativeFrameCryptorObserver::~NativeFrameCryptorObserver() {
  fc_->unregister_observer();
}

void NativeFrameCryptorObserver::OnFrameCryptionStateChanged(
    const std::string participant_id,
    webrtc::FrameCryptionState state) {
  observer_->on_frame_cryption_state_change(
      participant_id, static_cast<FrameCryptionState>(state));
}

void FrameCryptor::set_enabled(bool enabled) const {
  webrtc::MutexLock lock(&mutex_);
  e2ee_transformer_->SetEnabled(enabled);
}

bool FrameCryptor::enabled() const {
  webrtc::MutexLock lock(&mutex_);
  return e2ee_transformer_->enabled();
}

void FrameCryptor::set_key_index(int32_t index) const {
  webrtc::MutexLock lock(&mutex_);
  e2ee_transformer_->SetKeyIndex(index);
}

int32_t FrameCryptor::key_index() const {
  webrtc::MutexLock lock(&mutex_);
  return e2ee_transformer_->key_index();
}

std::shared_ptr<KeyProvider> new_key_provider(KeyProviderOptions options) {
  return std::make_shared<KeyProvider>(options);
}

std::shared_ptr<FrameCryptor> new_frame_cryptor_for_rtp_sender(
    const ::rust::String participant_id,
    Algorithm algorithm,
    std::shared_ptr<KeyProvider> key_provider,
    std::shared_ptr<RtpSender> sender) {
  return std::make_shared<FrameCryptor>(
      std::string(participant_id.data(), participant_id.size()),
      AlgorithmToFrameCryptorAlgorithm(algorithm),
      key_provider->rtc_key_provider(), sender->rtc_sender());
}

std::shared_ptr<FrameCryptor> new_frame_cryptor_for_rtp_receiver(
    const ::rust::String participant_id,
    Algorithm algorithm,
    std::shared_ptr<KeyProvider> key_provider,
    std::shared_ptr<RtpReceiver> receiver) {
  return std::make_shared<FrameCryptor>(
      std::string(participant_id.data(), participant_id.size()),
      AlgorithmToFrameCryptorAlgorithm(algorithm),
      key_provider->rtc_key_provider(), receiver->rtc_receiver());
}

}  // namespace livekit
