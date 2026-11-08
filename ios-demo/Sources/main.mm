// RavAva iOS demo: the same window as the Qt / Android native demo (tabs Main / Encoder / Music / Log), built in code.
// Everything audio is in libRavAva (VoiceSession.cpp is shared with the Android demo); this file only shows controls.
#import <UIKit/UIKit.h>
#import <AVFoundation/AVFoundation.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include <memory>
#include <string>
#include <vector>
#include <ravava/AudioDevice.h>
#include "VoiceSession.h"

static NSString *const kServerNames[] = {@"echo amazon server", @"kharej", @"iran", @"local server", @"custom"};
static NSString *const kServerIps[]   = {@"52.43.121.77", @"217.154.196.225", @"95.38.155.49", @"192.168.1.188", @""};
static NSString *const kBandNames[]   = {@"Narrowband - [4kHz 8kHz]", @"Medium-band - [6kHz 12kHz]", @"Wideband - [8kHz 16kHz]", @"Supper-wideband - [12kHz 24kHz]", @"fullband - [20kHz 48kHz]"};
static const int kBandValues[]        = {1101, 1102, 1103, 1104, 1105};
static NSString *const kBitrates[]    = {@"6000", @"8000", @"12000", @"16000", @"20000", @"24000", @"28000", @"32000", @"40000", @"48000", @"56000", @"64000", @"96000", @"128000", @"192000"};
static NSString *const kFrames[]      = {@"10", @"20", @"40", @"60", @"80", @"100", @"120"};
static NSString *const kLosses[]      = {@"0", @"10", @"20", @"30", @"40", @"50", @"60", @"70", @"80", @"90", @"100"};
static NSString *const kNoise[]       = {@"Noise: off", @"Noise: low", @"Noise: moderate", @"Noise: high", @"Noise: very high"};
static NSString *const kEcho[]        = {@"Echo: off", @"Echo: default", @"Echo: protect voice 1", @"Echo: protect voice 2", @"Echo: protect voice 3"};
#define ARR(a) ([NSArray arrayWithObjects:a count:sizeof(a) / sizeof(a[0])])

// ---------------------------------------------------------------- small UI helpers
@interface Picker : UIButton
@property (nonatomic, copy) NSArray<NSString *> *items;
@property (nonatomic) NSInteger index;
@property (nonatomic, copy) void (^onChange)(NSInteger);
- (void)setItems:(NSArray<NSString *> *)items selected:(NSInteger)sel;
@end
@implementation Picker
- (instancetype)init {
    self = [super initWithFrame:CGRectZero];
    self.contentHorizontalAlignment = UIControlContentHorizontalAlignmentLeading;
    self.layer.borderWidth = 1; self.layer.cornerRadius = 6; self.layer.borderColor = UIColor.systemGray3Color.CGColor;
    self.contentEdgeInsets = UIEdgeInsetsMake(8, 10, 8, 10);
    self.showsMenuAsPrimaryAction = YES;
    return self;
}
- (void)setItems:(NSArray<NSString *> *)items selected:(NSInteger)sel {
    _items = [items copy];
    _index = (sel >= 0 && sel < (NSInteger)items.count) ? sel : 0;
    [self rebuild];
}
- (void)setIndex:(NSInteger)i { _index = i; [self rebuild]; }
- (void)rebuild {
    [self setTitle:(_index < (NSInteger)_items.count ? _items[_index] : @"") forState:UIControlStateNormal];
    NSMutableArray *acts = [NSMutableArray array];
    __weak Picker *ws = self;
    for (NSInteger i = 0; i < (NSInteger)_items.count; ++i) {
        UIAction *a = [UIAction actionWithTitle:_items[i] image:nil identifier:nil handler:^(UIAction *) {
            Picker *s = ws; if (!s) return;
            s->_index = i; [s rebuild]; if (s.onChange) s.onChange(i);
        }];
        if (i == _index) a.state = UIMenuElementStateOn;
        [acts addObject:a];
    }
    self.menu = [UIMenu menuWithChildren:acts];
}
@end

static UILabel *Lbl(NSString *t) { UILabel *l = [UILabel new]; l.text = t; l.numberOfLines = 0; l.font = [UIFont systemFontOfSize:14]; l.textColor = UIColor.secondaryLabelColor; return l; }
static UIButton *Btn(NSString *t, id target, SEL sel) {
    UIButton *b = [UIButton buttonWithType:UIButtonTypeSystem];
    [b setTitle:t forState:UIControlStateNormal]; b.titleLabel.font = [UIFont boldSystemFontOfSize:16];
    b.layer.cornerRadius = 6; b.layer.borderWidth = 1; b.layer.borderColor = UIColor.systemBlueColor.CGColor;
    b.contentEdgeInsets = UIEdgeInsetsMake(8, 12, 8, 12);
    [b addTarget:target action:sel forControlEvents:UIControlEventTouchUpInside]; return b;
}
static UIStackView *Row(NSArray *views) {
    UIStackView *s = [[UIStackView alloc] initWithArrangedSubviews:views];
    s.axis = UILayoutConstraintAxisHorizontal; s.spacing = 8; s.distribution = UIStackViewDistributionFill; s.alignment = UIStackViewAlignmentCenter; return s;
}

@interface Form : UIViewController
@property (nonatomic, strong) UIStackView *col;
@end
@implementation Form
- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = UIColor.systemBackgroundColor;
    UIScrollView *sv = [UIScrollView new]; sv.translatesAutoresizingMaskIntoConstraints = NO; sv.keyboardDismissMode = UIScrollViewKeyboardDismissModeOnDrag;
    [self.view addSubview:sv];
    _col = [UIStackView new]; _col.axis = UILayoutConstraintAxisVertical; _col.spacing = 10; _col.translatesAutoresizingMaskIntoConstraints = NO;
    [sv addSubview:_col];
    UILayoutGuide *g = self.view.safeAreaLayoutGuide;
    [NSLayoutConstraint activateConstraints:@[
        [sv.topAnchor constraintEqualToAnchor:g.topAnchor], [sv.bottomAnchor constraintEqualToAnchor:g.bottomAnchor],
        [sv.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor], [sv.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
        [_col.topAnchor constraintEqualToAnchor:sv.topAnchor constant:12], [_col.bottomAnchor constraintEqualToAnchor:sv.bottomAnchor constant:-12],
        [_col.leadingAnchor constraintEqualToAnchor:sv.leadingAnchor constant:16], [_col.trailingAnchor constraintEqualToAnchor:sv.trailingAnchor constant:-16],
        [_col.widthAnchor constraintEqualToAnchor:sv.widthAnchor constant:-32]]];
}
- (void)add:(UIView *)v { [_col addArrangedSubview:v]; }
@end

static UISwitch *Sw(BOOL on) { UISwitch *s = [UISwitch new]; s.on = on; return s; }
static UIStackView *SwRow(NSString *t, UISwitch *s) { UILabel *l = [UILabel new]; l.text = t; l.numberOfLines = 0; [l setContentHuggingPriority:UILayoutPriorityDefaultLow forAxis:UILayoutConstraintAxisHorizontal]; return Row(@[l, s]); }
static UISlider *Sl(float mx, float v) { UISlider *s = [UISlider new]; s.minimumValue = 0; s.maximumValue = mx; s.value = v; return s; }
static UIProgressView *Bar() { UIProgressView *p = [UIProgressView new]; return p; }

// ---------------------------------------------------------------- the app
@interface Root : UITabBarController <UIDocumentPickerDelegate, UITextFieldDelegate>
@end

@implementation Root {
    std::unique_ptr<VoiceSession> session;
    BOOL running, webrtc;
    NSTimer *timer; int tick;
    // controls
    UISwitch *swLoop, *swDtx, *swMusicMode, *swAdaptive, *swCall, *swAgc, *swHpf, *swMonitor;
    Picker *pServer, *pIn, *pOut, *pBitrate, *pBand, *pFrame, *pLoss, *pNoise, *pEcho;
    UITextField *tfIp, *tfPort;
    UISegmentedControl *segCh;
    UISlider *slVol, *slMic, *slMusic, *slMon;
    UILabel *lbVol, *lbMic, *lbMusicFile, *lbRate, *lbLog, *lbWebrtc;
    UIProgressView *barRx, *barMine;
    UIButton *btnStart, *btnStop, *btnTalk;
    BOOL talk;
    std::vector<std::string> inIds, outIds;
    NSString *musicPath;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    session = std::make_unique<VoiceSession>();
    webrtc = VoiceSession::WebrtcAvailable();
    talk = YES;
    Form *a = [Form new], *b = [Form new], *c = [Form new], *d = [Form new];
    a.title = @"Main"; b.title = @"Encoder"; c.title = @"Music"; d.title = @"Log";
    for (Form *f in @[a, b, c, d]) f.view.hidden = NO;   // forces viewDidLoad so col exists
    [self buildMain:a]; [self buildEncoder:b]; [self buildMusic:c]; [self buildLog:d];
    NSArray *names = @[@"slider.horizontal.3", @"waveform", @"music.note", @"doc.text"];
    NSArray *forms = @[a, b, c, d];
    NSMutableArray *vcs = [NSMutableArray array];
    for (int i = 0; i < 4; ++i) {
        UIViewController *f = forms[i];
        f.tabBarItem = [[UITabBarItem alloc] initWithTitle:f.title image:[UIImage systemImageNamed:names[i]] tag:i];
        [vcs addObject:f];
    }
    self.viewControllers = vcs;
    [self loadPrefs];
    [self refreshDevices];
    timer = [NSTimer scheduledTimerWithTimeInterval:0.1 target:self selector:@selector(onTick) userInfo:nil repeats:YES];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(refreshDevices) name:AVAudioSessionRouteChangeNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(refreshDevices) name:UIApplicationWillEnterForegroundNotification object:nil];
    [self showDeviceCounts];
}

// ---- layout
- (void)buildMain:(Form *)f {
    swLoop = Sw(YES);
    [f add:SwRow(@"Loopback", swLoop)];
    [f add:Lbl(@"Server (UDP)")];
    pServer = [Picker new]; [pServer setItems:ARR(kServerNames) selected:0]; [f add:pServer];
    tfIp = [UITextField new]; tfIp.placeholder = @"custom server IP / host"; tfIp.borderStyle = UITextBorderStyleRoundedRect; tfIp.autocapitalizationType = UITextAutocapitalizationTypeNone; tfIp.autocorrectionType = UITextAutocorrectionTypeNo; tfIp.delegate = self;
    tfPort = [UITextField new]; tfPort.text = @"10001"; tfPort.placeholder = @"port"; tfPort.borderStyle = UITextBorderStyleRoundedRect; tfPort.keyboardType = UIKeyboardTypeNumberPad; tfPort.delegate = self;
    [tfPort.widthAnchor constraintEqualToConstant:90].active = YES;
    UIStackView *r = Row(@[tfIp, tfPort]); [f add:r];
    tfIp.hidden = YES; pServer.enabled = NO;
    [swLoop addTarget:self action:@selector(onNet) forControlEvents:UIControlEventValueChanged];
    __weak Root *ws = self;
    pServer.onChange = ^(NSInteger i) { Root *s = ws; s->tfIp.hidden = (i != 4); [s onNet]; };

    [f add:Lbl(@"Microphone")]; pIn = [Picker new]; [pIn setItems:@[@"System default"] selected:0]; [f add:pIn];
    [f add:Lbl(@"Speaker")];    pOut = [Picker new]; [pOut setItems:@[@"System default"] selected:0]; [f add:pOut];
    [f add:Btn(@"Refresh device lists", self, @selector(onRefresh))];
    pIn.onChange  = ^(NSInteger i) { Root *s = ws; if (s->session && s->running) s->session->SetInputDevice(i > 0 && i - 1 < (NSInteger)s->inIds.size() ? s->inIds[i - 1] : ""); };
    pOut.onChange = ^(NSInteger i) { Root *s = ws; if (s->session && s->running) s->session->SetOutputDevice(i > 0 && i - 1 < (NSInteger)s->outIds.size() ? s->outIds[i - 1] : ""); };

    lbVol = Lbl(@"Volume (received): 100"); [f add:lbVol];
    slVol = Sl(100, 100); [slVol addTarget:self action:@selector(onVol) forControlEvents:UIControlEventValueChanged]; [f add:slVol];
    barRx = Bar(); [f add:barRx];
    lbMic = Lbl(@"Microphone gain: 100"); [f add:lbMic];
    slMic = Sl(200, 100); [slMic addTarget:self action:@selector(onMicGain) forControlEvents:UIControlEventValueChanged]; [f add:slMic];
    barMine = Bar(); [f add:barMine];

    btnStart = Btn(@"Start", self, @selector(onStart));
    btnStop = Btn(@"Stop", self, @selector(onStop)); btnStop.enabled = NO;
    btnTalk = Btn(@"Talk: ON", self, @selector(onTalk));
    [f add:Row(@[btnStart, btnStop, btnTalk])];
    lbRate = Lbl(@""); [f add:lbRate];
}

- (void)buildEncoder:(Form *)f {
    pBitrate = [Picker new]; [pBitrate setItems:ARR(kBitrates) selected:11];
    pBand = [Picker new];    [pBand setItems:ARR(kBandNames) selected:4];
    pFrame = [Picker new];   [pFrame setItems:ARR(kFrames) selected:1];
    pLoss = [Picker new];    [pLoss setItems:ARR(kLosses) selected:2];
    [f add:Lbl(@"Bitrate (bit/s)")]; [f add:pBitrate];
    [f add:Lbl(@"Bandwidth")]; [f add:pBand];
    [f add:Lbl(@"Frame (ms)")]; [f add:pFrame];
    [f add:Lbl(@"Expected packet loss (%)")]; [f add:pLoss];
    segCh = [[UISegmentedControl alloc] initWithItems:@[@"Mono", @"Stereo"]]; segCh.selectedSegmentIndex = 0;
    [f add:Lbl(@"Channels")]; [f add:segCh];
    swDtx = Sw(YES); swMusicMode = Sw(NO); swAdaptive = Sw(NO); swCall = Sw(YES); swAgc = Sw(NO); swHpf = Sw(NO);
    [f add:SwRow(@"useDtx", swDtx)];
    [f add:SwRow(@"musicMode (Opus tuned for music)", swMusicMode)];
    [f add:SwRow(@"Adaptive (playout buffer)", swAdaptive)];
    [swAdaptive addTarget:self action:@selector(onAdaptive) forControlEvents:UIControlEventValueChanged];
    [f add:SwRow(@"Call mode (kept for parity with Android; iOS always uses play-and-record)", swCall)];
    pNoise = [Picker new]; [pNoise setItems:ARR(kNoise) selected:0];
    pEcho = [Picker new];  [pEcho setItems:ARR(kEcho) selected:0];
    [f add:SwRow(@"Auto gain (webrtc AGC2)", swAgc)];
    [f add:SwRow(@"Low-cut (webrtc high-pass ~100 Hz)", swHpf)];
    [f add:pNoise]; [f add:pEcho];
    lbWebrtc = Lbl(webrtc ? @"webrtc audio processing is available. Settings here are applied on Start."
                          : @"This iOS build has no webrtc-audio-processing: echo / noise / auto gain / low-cut are disabled. Use a headset to avoid echo.");
    [f add:lbWebrtc];
    if (!webrtc) for (UIControl *v in @[swAgc, swHpf, pNoise, pEcho]) v.enabled = NO;
}

- (void)buildMusic:(Form *)f {
    lbMusicFile = Lbl(@"No file chosen"); [f add:lbMusicFile];
    [f add:Btn(@"Choose music file…", self, @selector(onChooseMusic))];
    [f add:Row(@[Btn(@"Play", self, @selector(onPlay)), Btn(@"Pause", self, @selector(onPause))])];
    [f add:Row(@[Btn(@"Resume", self, @selector(onResume)), Btn(@"Stop", self, @selector(onMusicStop))])];
    [f add:Lbl(@"Music volume (sent)")];
    slMusic = Sl(200, 100); [slMusic addTarget:self action:@selector(onMusicVol) forControlEvents:UIControlEventValueChanged]; [f add:slMusic];
    swMonitor = Sw(NO); [swMonitor addTarget:self action:@selector(onMonitor) forControlEvents:UIControlEventValueChanged];
    [f add:SwRow(@"Hear music here too (use headphones)", swMonitor)];
    [f add:Lbl(@"Monitor volume")];
    slMon = Sl(200, 100); [slMon addTarget:self action:@selector(onMonVol) forControlEvents:UIControlEventValueChanged]; [f add:slMon];
}

- (void)buildLog:(Form *)f {
    [f add:Btn(@"Copy log", self, @selector(onCopyLog))];
    lbLog = [UILabel new]; lbLog.numberOfLines = 0; lbLog.font = [UIFont monospacedSystemFontOfSize:11 weight:UIFontWeightRegular]; lbLog.text = @"Idle";
    [f add:lbLog];
}

// ---- helpers
static std::string S(NSString *s) { return s ? std::string(s.UTF8String) : std::string(); }
static NSString *N(const std::string &s) { return [NSString stringWithUTF8String:s.c_str()] ?: @""; }

- (void)loadPrefs {
    NSUserDefaults *d = NSUserDefaults.standardUserDefaults;
    if ([d objectForKey:@"loop"]) swLoop.on = [d boolForKey:@"loop"];
    NSInteger si = [d integerForKey:@"server"]; if (si >= 0 && si < 5) pServer.index = si;
    tfIp.text = [d stringForKey:@"ip"]; if ([d stringForKey:@"port"]) tfPort.text = [d stringForKey:@"port"];
    tfIp.hidden = pServer.index != 4; pServer.enabled = !swLoop.on;
}
- (void)savePrefs {
    NSUserDefaults *d = NSUserDefaults.standardUserDefaults;
    [d setBool:swLoop.on forKey:@"loop"]; [d setInteger:pServer.index forKey:@"server"];
    [d setObject:tfIp.text ?: @"" forKey:@"ip"]; [d setObject:tfPort.text ?: @"10001" forKey:@"port"];
}
- (std::string)serverHost { return pServer.index == 4 ? S([tfIp.text stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceCharacterSet]) : S(kServerIps[pServer.index]); }
- (int)port { int p = tfPort.text.intValue; return p > 0 && p < 65536 ? p : 10001; }

- (void)refreshDevices {
    auto ins = ravava::ListInputDevices(), outs = ravava::ListOutputDevices();
    NSMutableArray *ni = [NSMutableArray arrayWithObject:@"System default"], *no = [NSMutableArray arrayWithObject:@"System default"];
    NSString *curIn = pIn.index < (NSInteger)pIn.items.count ? pIn.items[pIn.index] : nil;
    NSString *curOut = pOut.index < (NSInteger)pOut.items.count ? pOut.items[pOut.index] : nil;
    inIds.clear(); outIds.clear();
    for (auto &d : ins)  { [ni addObject:N(d.name)]; inIds.push_back(d.id); }
    for (auto &d : outs) { [no addObject:N(d.name)]; outIds.push_back(d.id); }
    NSInteger ii = curIn ? [ni indexOfObject:curIn] : 0, oi = curOut ? [no indexOfObject:curOut] : 0;
    [pIn setItems:ni selected:ii == NSNotFound ? 0 : ii];
    [pOut setItems:no selected:oi == NSNotFound ? 0 : oi];
}
- (void)showDeviceCounts {
    if (running) return;
    lbLog.text = [NSString stringWithFormat:@"Idle\n%zu input device(s) listed, %zu output route(s) listed", inIds.size(), outIds.size()];
}
- (void)onRefresh { [self refreshDevices]; [self showDeviceCounts]; }

// ---- actions
- (BOOL)textFieldShouldReturn:(UITextField *)t { [t resignFirstResponder]; [self onNet]; return YES; }
- (void)textFieldDidEndEditing:(UITextField *)t { [self onNet]; }
- (void)onNet { [self savePrefs]; pServer.enabled = !swLoop.on; if (running) session->SetNetwork(swLoop.on, [self serverHost], [self port]); }
- (void)onVol { int v = (int)slVol.value; lbVol.text = [NSString stringWithFormat:@"Volume (received): %d", v]; if (running) session->SetVolume(v); }
- (void)onMicGain { int v = (int)slMic.value; lbMic.text = [NSString stringWithFormat:@"Microphone gain: %d", v]; if (running) session->SetMicGain(v); }
- (void)onTalk { talk = !talk; [btnTalk setTitle:talk ? @"Talk: ON" : @"Talk: OFF" forState:UIControlStateNormal]; if (running) session->SetTalk(talk); }
- (void)onAdaptive { if (running) session->SetAdaptive(swAdaptive.on); }
- (void)onMusicVol { if (running) session->SetMusicVolume((int)slMusic.value); }
- (void)onMonitor { if (running) session->SetMonitor(swMonitor.on); }
- (void)onMonVol { if (running) session->SetMonitorVolume((int)slMon.value); }
- (void)onCopyLog { UIPasteboard.generalPasteboard.string = lbLog.text; }

- (void)onStart {
    if (running) return;
    [self.view endEditing:YES];
    AVAudioSession *as = AVAudioSession.sharedInstance;
    [as requestRecordPermission:^(BOOL granted) {
        dispatch_async(dispatch_get_main_queue(), ^{
            if (!granted) { lbLog.text = @"Microphone permission denied. Allow it in Settings > RavAva > Microphone."; return; }
            [self startSession];
        });
    }];
}
- (void)startSession {
    SessionSettings s;
    s.loopback = swLoop.on; s.server = [self serverHost]; s.port = [self port];
    s.bitrate = kBitrates[pBitrate.index].intValue; s.bandwidth = kBandValues[pBand.index];
    s.frameMs = kFrames[pFrame.index].intValue; s.lossPct = kLosses[pLoss.index].intValue;
    s.channels = segCh.selectedSegmentIndex == 1 ? 2 : 1;
    s.agc = swAgc.on; s.callMode = swCall.on; s.adaptive = swAdaptive.on; s.hpf = swHpf.on; s.dtx = swDtx.on; s.musicMode = swMusicMode.on;
    s.noise = (int)pNoise.index; s.echo = (int)pEcho.index;
    s.volume = (int)slVol.value; s.micGain = (int)slMic.value; s.talk = talk;
    s.musicVolume = (int)slMusic.value; s.monitor = swMonitor.on; s.monitorVolume = (int)slMon.value;
    s.inputDevice  = pIn.index > 0 && pIn.index - 1 < (NSInteger)inIds.size() ? inIds[pIn.index - 1] : "";
    s.outputDevice = pOut.index > 0 && pOut.index - 1 < (NSInteger)outIds.size() ? outIds[pOut.index - 1] : "";
    [self savePrefs];
    if (!session->Start(s)) { lbLog.text = @"Start failed (see the settings)."; return; }
    running = YES; btnStart.enabled = NO; btnStop.enabled = YES;
    UIApplication.sharedApplication.idleTimerDisabled = YES;
}
- (void)onStop {
    if (!running) return;
    session->Stop(); running = NO; btnStart.enabled = YES; btnStop.enabled = NO;
    barRx.progress = 0; barMine.progress = 0; lbRate.text = @"";
    UIApplication.sharedApplication.idleTimerDisabled = NO;
    [self refreshDevices]; [self showDeviceCounts];
}
- (void)onTick {
    if (!running) return;
    barMine.progress = session->MicLevel() / 100.0f;
    barRx.progress = session->SpkLevel() / 100.0f;
    if (++tick % 10 == 0) {
        lbLog.text = N(session->StatsText());
        lbRate.text = N(session->RateText());
    }
}

// ---- music
- (void)onChooseMusic {
    UIDocumentPickerViewController *p = [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:@[UTTypeAudio] asCopy:YES];
    p.delegate = self; [self presentViewController:p animated:YES completion:nil];
}
- (void)documentPicker:(UIDocumentPickerViewController *)c didPickDocumentsAtURLs:(NSArray<NSURL *> *)urls {
    if (!urls.count) return;
    musicPath = urls[0].path; lbMusicFile.text = urls[0].lastPathComponent;
}
- (void)onPlay { if (!running) { lbMusicFile.text = @"Press Start first (music is mixed into the running stream)"; return; }
                 if (!musicPath) { lbMusicFile.text = @"Choose a music file first"; return; } session->Music(0, S(musicPath)); }
- (void)onPause { if (running) session->Music(1, ""); }
- (void)onResume { if (running) session->Music(2, ""); }
- (void)onMusicStop { if (running) session->Music(3, ""); }
@end

@interface AppDelegate : UIResponder <UIApplicationDelegate>
@property (nonatomic, strong) UIWindow *window;
@end
@implementation AppDelegate
- (BOOL)application:(UIApplication *)app didFinishLaunchingWithOptions:(NSDictionary *)o {
    self.window = [[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
    self.window.rootViewController = [Root new];
    [self.window makeKeyAndVisible];
    return YES;
}
@end

int main(int argc, char *argv[]) {
    @autoreleasepool { return UIApplicationMain(argc, argv, nil, NSStringFromClass(AppDelegate.class)); }
}
