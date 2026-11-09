/*
 * RavAva - real-time voice library (Opus over any datagram transport)
 * Copyright (c) 2026 Mojtaba Kahvand. All rights reserved.
 * Author: Mojtaba Kahvand
 *
 * Proprietary software, NOT free / open source. Use, copying, modification and distribution are forbidden without a
 * written license from the author. See the LICENSE file in the repository root.
 * SPDX-License-Identifier: LicenseRef-RavAva-Proprietary
 */
// iOS only. This .cpp is compiled as Objective-C++ (CMake: -x objective-c++, the same way miniaudio.c is compiled as Objective-C).
#import <AVFoundation/AVFoundation.h>
#include <cstdio>
#include "IosAudioSession.h"

namespace ravava { namespace ios {

bool ActivateAudioSession()
{
    NSError *err = nil;
    const BOOL ok = [[AVAudioSession sharedInstance] setActive:YES error:&err];
    if(!ok)
        std::fprintf(stdout, "[ravava] AVAudioSession setActive failed: %s (code %ld)\n",
                     err ? err.localizedDescription.UTF8String : "?", err ? (long)err.code : 0L);
    return ok == YES;
}

} } // namespace ravava::ios
