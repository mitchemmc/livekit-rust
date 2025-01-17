// Copyright 2023 LiveKit, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

use livekit_webrtc::native::frame_cryptor as fc;

use crate::id::ParticipantIdentity;

const DEFAULT_RATCHET_SALT: &str = "LKFrameEncryptionKey";
const DEFAULT_MAGIC_BYTES: &str = "LK-ROCKS";
const DEFAULT_RATCHET_WINDOW_SIZE: i32 = 16;

#[derive(Clone)]
pub struct KeyProviderOptions {
    pub ratchet_window_size: i32,
    pub ratchet_salt: Vec<u8>,
    pub uncrypted_magic_bytes: Vec<u8>,
}

impl Default for KeyProviderOptions {
    fn default() -> Self {
        Self {
            ratchet_window_size: DEFAULT_RATCHET_WINDOW_SIZE,
            ratchet_salt: DEFAULT_RATCHET_SALT.to_owned().into_bytes(),
            uncrypted_magic_bytes: DEFAULT_MAGIC_BYTES.to_owned().into_bytes(),
        }
    }
}

#[derive(Clone)]
pub struct KeyProvider {
    pub(crate) handle: fc::KeyProvider,
}

impl KeyProvider {
    /// By default, the key provider is not shared
    pub fn new(options: KeyProviderOptions) -> Self {
        Self {
            handle: fc::KeyProvider::new(fc::KeyProviderOptions {
                shared_key: false,
                ratchet_window_size: options.ratchet_window_size,
                ratchet_salt: options.ratchet_salt,
                uncrypted_magic_bytes: options.uncrypted_magic_bytes,
            }),
        }
    }

    pub fn with_shared_key(options: KeyProviderOptions, shared_key: Vec<u8>) -> Self {
        let handle = fc::KeyProvider::new(fc::KeyProviderOptions {
            shared_key: true,
            ratchet_window_size: options.ratchet_window_size,
            ratchet_salt: options.ratchet_salt,
            uncrypted_magic_bytes: options.uncrypted_magic_bytes,
        });
        handle.set_shared_key(0, shared_key);
        Self { handle }
    }

    pub fn set_shared_key(&self, shared_key: Vec<u8>, key_index: i32) {
        self.handle.set_shared_key(key_index, shared_key);
    }

    pub fn ratchet_shared_key(&self, key_index: i32) -> Option<Vec<u8>> {
        self.handle.ratchet_shared_key(key_index)
    }

    pub fn get_shared_key(&self, key_index: i32) -> Option<Vec<u8>> {
        self.handle.get_shared_key(key_index)
    }

    pub fn set_key(&self, identity: &ParticipantIdentity, key_index: i32, key: Vec<u8>) -> bool {
        self.handle.set_key(identity.to_string(), key_index, key)
    }

    pub fn ratchet_key(&self, identity: &ParticipantIdentity, key_index: i32) -> Option<Vec<u8>> {
        self.handle.ratchet_key(identity.to_string(), key_index)
    }

    pub fn get_key(&self, identity: &ParticipantIdentity, key_index: i32) -> Option<Vec<u8>> {
        self.handle.get_key(identity.to_string(), key_index)
    }
}
