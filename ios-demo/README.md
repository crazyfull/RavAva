# RavAva iOS demo

Same window as the Android native demo (tabs Main / Encoder / Music / Log). The audio pipeline is `VoiceSession.cpp`,
shared with `android5-native-demo`. No echo cancellation on iOS (no webrtc): use a headset.

Build: GitHub Actions workflow **Build Demo iOS** -> artifact `RavAva-iOS-unsigned-ipa` (unsigned `.ipa`).
Install: sign on the PC with Sideloadly (free Apple ID, 7 days) - see the chat instructions.
