/*
 * RavAva - real-time voice library (Opus over any datagram transport)
 * Copyright (c) 2026 Mojtaba Kahvand. All rights reserved.
 * Author: Mojtaba Kahvand
 *
 * Proprietary software, NOT free / open source. Use, copying, modification and distribution are forbidden without a
 * written license from the author. See the LICENSE file in the repository root.
 * SPDX-License-Identifier: LicenseRef-RavAva-Proprietary
 */
#ifndef RAVAVA_PLATFORM_IOSAUDIOSESSION_H
#define RAVAVA_PLATFORM_IOSAUDIOSESSION_H

namespace ravava { namespace ios {

// (Re)activates the app's AVAudioSession. After a phone call / Siri / another app took the audio (an "interruption") iOS leaves the
// session INACTIVE: a device that is opened again then starts but stays silent. miniaudio activates the session only once, when
// its context is created, so RavAva calls this when an interruption ends and before every device (re)open.
// Returns false when iOS refused (a call is still going on). Safe to call any time and from any thread.
bool ActivateAudioSession();

} } // namespace ravava::ios

#endif
