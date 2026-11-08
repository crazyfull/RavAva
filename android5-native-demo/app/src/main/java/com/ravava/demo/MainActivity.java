package com.ravava.demo;

import android.Manifest;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.graphics.Typeface;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.text.InputType;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.ScrollView;
import android.widget.SeekBar;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;
import android.widget.ToggleButton;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import com.google.android.material.tabs.TabLayout;

import java.util.ArrayList;
import java.util.List;

/**
 * The same window as the Qt demo (mainwindow.ui), built in code: tabs Main / Encoder / Music / Log.
 * Everything audio is in the native library; this class only shows controls and forwards them.
 */
public class MainActivity extends AppCompatActivity {

    private static final int REQ_PERMISSIONS = 1001;
    private static final String[] SERVER_NAMES = {"echo amazon server", "kharej", "iran", "local server", "custom"};
    private static final String[] SERVER_IPS = {"52.43.121.77", "217.154.196.225", "95.38.155.49", "192.168.1.188", ""};
    private static final String[] BANDWIDTH_NAMES = {"Narrowband - [4kHz 8kHz]", "Medium-band - [6kHz 12kHz]", "Wideband - [8kHz 16kHz]",
            "Supper-wideband - [12kHz 24kHz]", "fullband - [20kHz 48kHz]"};
    private static final int[] BANDWIDTH_VALUES = {1101, 1102, 1103, 1104, 1105};
    private static final String[] BITRATES = {"6000", "8000", "12000", "16000", "20000", "24000", "28000", "32000", "40000", "48000", "56000", "64000", "96000", "128000", "192000"};
    private static final String[] FRAMES = {"10", "20", "40", "60", "80", "100", "120"};
    private static final String[] LOSSES = {"0", "10", "20", "30", "40", "50", "60", "70", "80", "90", "100"};
    private static final String[] NOISE = {"Noise: off", "Noise: low", "Noise: moderate", "Noise: high", "Noise: very high"};
    private static final String[] ECHO = {"Echo: off", "Echo: default", "Echo: protect voice 1", "Echo: protect voice 2", "Echo: protect voice 3"};

    private VoiceService service;
    private boolean bound = false;
    private boolean running = false;
    private boolean webrtc = false;

    // controls
    private CheckBox cbLoopback, cbAgc, cbCallMode, cbAdaptive, cbHpf, cbDtx, cbMusicMode, cbMonitor;
    private Spinner spServer, spInput, spOutput, spBitrate, spBandwidth, spFrame, spLoss, spNoise, spEcho;
    private EditText edCustomIp, edPort, edMusicPath;
    private RadioButton rbMono, rbStereo;
    private SeekBar sbVolume, sbMicGain, sbMusicGain, sbMonitor;
    private TextView tvVolume, tvMicGain, tvLog, tvRate;
    private ProgressBar pbReceived, pbMine;
    private Button btnStart, btnStop, btnPlay, btnPause, btnResume, btnMusicStop;
    private ToggleButton btnTalk;
    private final List<String> inIds = new ArrayList<>(), outIds = new ArrayList<>();
    private View[] pages;
    private TabLayout tabLayout;
    private SharedPreferences prefs;

    private final Handler ui = new Handler(Looper.getMainLooper());
    private final Runnable ticker = new Runnable() {
        @Override public void run() {
            if (bound && service != null && running) {
                pbReceived.setProgress(service.getSpkLevel());
                pbMine.setProgress(service.getMicLevel());
                tvLog.setText(service.getStatsText());
                tvRate.setText(service.getRateText());
                updateMusicButtons();
            }
            ui.postDelayed(this, 100);
        }
    };

    private final ServiceConnection connection = new ServiceConnection() {
        @Override public void onServiceConnected(ComponentName name, IBinder binder) {
            service = ((VoiceService.LocalBinder) binder).getService();
            bound = true;
            running = service.isRunning();
            updateRunning();
        }
        @Override public void onServiceDisconnected(ComponentName name) { bound = false; service = null; }
    };

    // ---------------------------------------------------------------- small view helpers
    private int dp(int v) { return Math.round(v * getResources().getDisplayMetrics().density); }

    private LinearLayout column() {
        LinearLayout l = new LinearLayout(this);
        l.setOrientation(LinearLayout.VERTICAL);
        l.setPadding(dp(12), dp(8), dp(12), dp(8));
        return l;
    }

    private LinearLayout row(View... views) {
        LinearLayout l = new LinearLayout(this);
        l.setOrientation(LinearLayout.HORIZONTAL);
        for (View v : views) l.addView(v, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        return l;
    }

    private TextView label(String t) {
        TextView v = new TextView(this);
        v.setText(t);
        v.setPadding(0, dp(8), 0, dp(2));
        v.setTypeface(null, Typeface.BOLD);
        return v;
    }

    private CheckBox check(String t, boolean on) {
        CheckBox c = new CheckBox(this);
        c.setText(t);
        c.setChecked(on);
        return c;
    }

    private Button button(String t, View.OnClickListener l) {
        Button b = new Button(this);
        b.setText(t);
        b.setAllCaps(false);
        b.setOnClickListener(l);
        return b;
    }

    private Spinner spinner(String[] items, int sel) {
        Spinner s = new Spinner(this);
        s.setAdapter(new ArrayAdapter<>(this, android.R.layout.simple_spinner_dropdown_item, items));
        s.setSelection(sel);
        return s;
    }

    private SeekBar seek(int max, int value) {
        SeekBar s = new SeekBar(this);
        s.setMax(max);
        s.setProgress(value);
        return s;
    }

    private ProgressBar bar() {
        ProgressBar p = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        p.setMax(100);
        return p;
    }

    private EditText edit(String text, boolean number) {
        EditText e = new EditText(this);
        e.setText(text);
        e.setSingleLine(true);
        if (number) e.setInputType(InputType.TYPE_CLASS_NUMBER);
        return e;
    }

    private interface Changed { void on(int v); }

    private void onSeek(SeekBar s, Changed c) {
        s.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override public void onProgressChanged(SeekBar sb, int p, boolean user) { c.on(p); }
            @Override public void onStartTrackingTouch(SeekBar sb) {}
            @Override public void onStopTrackingTouch(SeekBar sb) {}
        });
    }

    private void onSelect(Spinner s, Changed c) {
        s.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override public void onItemSelected(AdapterView<?> a, View v, int pos, long id) { c.on(pos); }
            @Override public void onNothingSelected(AdapterView<?> a) {}
        });
    }

    // ---------------------------------------------------------------- the window
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        VoiceService.init(this);
        webrtc = VoiceService.webrtcAvailable();

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);

        prefs = getSharedPreferences("ui", MODE_PRIVATE);
        TabLayout tabs = new TabLayout(this);
        tabLayout = tabs;
        for (String t : new String[]{"Main", "Encoder", "Music", "Log"}) tabs.addTab(tabs.newTab().setText(t));
        root.addView(tabs);

        pages = new View[]{buildMain(), buildEncoder(), buildMusic(), buildLog()};
        for (View p : pages) {
            ScrollView sv = null;
            if (p != pages[3]) { sv = new ScrollView(this); sv.addView(p); }
            root.addView(sv != null ? sv : p, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f));
            // remember the container to show / hide
            p.setTag(sv != null ? sv : p);
        }
        tvRate = new TextView(this);
        tvRate.setPadding(dp(12), dp(4), dp(12), dp(4));
        tvRate.setTextSize(12);
        root.addView(tvRate);
        setContentView(root);

        showPage(0);
        tabs.addOnTabSelectedListener(new TabLayout.OnTabSelectedListener() {
            @Override public void onTabSelected(TabLayout.Tab t) { showPage(t.getPosition()); }
            @Override public void onTabUnselected(TabLayout.Tab t) {}
            @Override public void onTabReselected(TabLayout.Tab t) {}
        });
        refreshDevices();
        restoreState();                 // the window comes back as it was (minimize, rotation, the system recreating the activity)
        updateRunning();
        showDeviceCounts();
    }

    // ---------------------------------------------------------------- the window state survives minimize / recreate / restart
    private void saveState() {
        if (prefs == null || spInput == null) return;
        SharedPreferences.Editor e = prefs.edit();
        e.putInt("tab", tabLayout.getSelectedTabPosition());
        e.putBoolean("loopback", cbLoopback.isChecked());
        e.putInt("server", spServer.getSelectedItemPosition());
        e.putString("customIp", edCustomIp.getText().toString());
        e.putString("port", edPort.getText().toString());
        e.putString("inDev", idAt(inIds, spInput.getSelectedItemPosition()));
        e.putString("outDev", idAt(outIds, spOutput.getSelectedItemPosition()));
        e.putInt("volume", sbVolume.getProgress());
        e.putInt("micGain", sbMicGain.getProgress());
        e.putBoolean("talk", btnTalk.isChecked());
        e.putInt("bitrate", spBitrate.getSelectedItemPosition());
        e.putInt("bandwidth", spBandwidth.getSelectedItemPosition());
        e.putInt("frame", spFrame.getSelectedItemPosition());
        e.putInt("loss", spLoss.getSelectedItemPosition());
        e.putBoolean("stereo", rbStereo.isChecked());
        e.putBoolean("dtx", cbDtx.isChecked());
        e.putBoolean("musicMode", cbMusicMode.isChecked());
        e.putBoolean("adaptive", cbAdaptive.isChecked());
        e.putBoolean("callMode", cbCallMode.isChecked());
        e.putBoolean("agc", cbAgc.isChecked());
        e.putBoolean("hpf", cbHpf.isChecked());
        e.putInt("noise", spNoise.getSelectedItemPosition());
        e.putInt("echo", spEcho.getSelectedItemPosition());
        e.putString("musicPath", edMusicPath.getText().toString());
        e.putInt("musicGain", sbMusicGain.getProgress());
        e.putBoolean("monitor", cbMonitor.isChecked());
        e.putInt("monitorGain", sbMonitor.getProgress());
        e.apply();
    }

    private static int pick(int saved, int count) { return saved >= 0 && saved < count ? saved : 0; }

    private void restoreState() {
        SharedPreferences p = prefs;
        if (p == null || !p.contains("tab")) { showPage(0); return; }
        cbLoopback.setChecked(p.getBoolean("loopback", true));
        spServer.setSelection(pick(p.getInt("server", 0), SERVER_NAMES.length));
        edCustomIp.setText(p.getString("customIp", ""));
        edPort.setText(p.getString("port", "10001"));
        spInput.setSelection(Math.max(0, inIds.indexOf(p.getString("inDev", ""))), false);
        spOutput.setSelection(Math.max(0, outIds.indexOf(p.getString("outDev", ""))), false);
        sbVolume.setProgress(p.getInt("volume", 100));
        sbMicGain.setProgress(p.getInt("micGain", 100));
        btnTalk.setChecked(p.getBoolean("talk", true));
        spBitrate.setSelection(pick(p.getInt("bitrate", 11), BITRATES.length));
        spBandwidth.setSelection(pick(p.getInt("bandwidth", 4), BANDWIDTH_NAMES.length));
        spFrame.setSelection(pick(p.getInt("frame", 1), FRAMES.length));
        spLoss.setSelection(pick(p.getInt("loss", 2), LOSSES.length));
        if (p.getBoolean("stereo", false)) rbStereo.setChecked(true); else rbMono.setChecked(true);
        cbDtx.setChecked(p.getBoolean("dtx", true));
        cbMusicMode.setChecked(p.getBoolean("musicMode", false));
        cbAdaptive.setChecked(p.getBoolean("adaptive", false));
        cbCallMode.setChecked(p.getBoolean("callMode", true));
        cbAgc.setChecked(p.getBoolean("agc", false) && webrtc);
        cbHpf.setChecked(p.getBoolean("hpf", false) && webrtc);
        spNoise.setSelection(webrtc ? pick(p.getInt("noise", 0), NOISE.length) : 0);
        spEcho.setSelection(webrtc ? pick(p.getInt("echo", 0), ECHO.length) : 0);
        edMusicPath.setText(p.getString("musicPath", "/sdcard/Music/test.mp3"));
        sbMusicGain.setProgress(p.getInt("musicGain", 100));
        cbMonitor.setChecked(p.getBoolean("monitor", false));
        sbMonitor.setProgress(p.getInt("monitorGain", 100));
        TabLayout.Tab t = tabLayout.getTabAt(pick(p.getInt("tab", 0), 4));
        if (t != null) t.select();
        showPage(tabLayout.getSelectedTabPosition());
    }

    private void showDeviceCounts() {
        // the Log tab says what the device lists hold (while idle), so an empty list is visible
        if (running) return;
        tvLog.setText("Idle\nDevices listed by the library: " + (inIds.size() - 1) + " microphone(s), " + (outIds.size() - 1) + " speaker(s)"
                + ((inIds.size() <= 1 && outIds.size() <= 1) ? "\n(none: see  adb logcat -s ravava  for the reason)" : ""));
    }

    private void showPage(int i) {
        for (int k = 0; k < pages.length; k++) ((View) pages[k].getTag()).setVisibility(k == i ? View.VISIBLE : View.GONE);
    }

    private View buildMain() {
        LinearLayout c = column();

        cbLoopback = check("Loopback", true);
        c.addView(cbLoopback);
        c.addView(label("Server (UDP)"));
        spServer = spinner(SERVER_NAMES, 0);
        c.addView(spServer);
        edCustomIp = edit("", false);
        edCustomIp.setHint("custom server IP / host");
        edPort = edit("10001", true);
        edPort.setHint("port");
        c.addView(row(edCustomIp, edPort));
        cbLoopback.setOnCheckedChangeListener((b, on) -> { spServer.setEnabled(!on); applyNetwork(); });
        onSelect(spServer, i -> { edCustomIp.setVisibility(i == SERVER_NAMES.length - 1 ? View.VISIBLE : View.GONE); applyNetwork(); });
        edCustomIp.setVisibility(View.GONE);
        spServer.setEnabled(false);
        edPort.setOnFocusChangeListener((v, f) -> { if (!f) applyNetwork(); });
        edCustomIp.setOnFocusChangeListener((v, f) -> { if (!f) applyNetwork(); });

        c.addView(label("Microphone"));
        spInput = spinner(new String[]{"System default"}, 0);
        c.addView(spInput);
        c.addView(label("Speaker"));
        spOutput = spinner(new String[]{"System default"}, 0);
        c.addView(spOutput);
        // a headset that was just connected shows up with this button (and on every Start / when the app comes back)
        c.addView(button("Refresh device lists", v -> { refreshDevices(); showDeviceCounts(); }));
        spInput.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override public void onItemSelected(AdapterView<?> a, View v, int pos, long id) { if (bound && service != null) service.setDevice(true, idAt(inIds, pos)); }
            @Override public void onNothingSelected(AdapterView<?> a) {}
        });
        spOutput.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override public void onItemSelected(AdapterView<?> a, View v, int pos, long id) { if (bound && service != null) service.setDevice(false, idAt(outIds, pos)); }
            @Override public void onNothingSelected(AdapterView<?> a) {}
        });

        tvVolume = new TextView(this);
        sbVolume = seek(100, 100);
        pbReceived = bar();
        c.addView(label("Volume (received)"));
        c.addView(row(sbVolume, tvVolume));
        c.addView(pbReceived);
        onSeek(sbVolume, v -> { tvVolume.setText(String.valueOf(v)); set(0, v); });
        tvVolume.setText("100");

        tvMicGain = new TextView(this);
        sbMicGain = seek(200, 100);
        pbMine = bar();
        c.addView(label("Microphone gain"));
        c.addView(row(sbMicGain, tvMicGain));
        c.addView(pbMine);
        onSeek(sbMicGain, v -> { tvMicGain.setText(String.valueOf(v)); set(1, v); });
        tvMicGain.setText("100");

        btnStart = button("Start", v -> { if (checkPermissions()) startSession(); });
        btnStop = button("Stop", v -> stopSession());
        btnTalk = new ToggleButton(this);
        btnTalk.setTextOn("Talk: ON");
        btnTalk.setTextOff("Talk: OFF");
        btnTalk.setChecked(true);
        btnTalk.setOnCheckedChangeListener((b, on) -> set(2, on ? 1 : 0));
        c.addView(row(btnStart, btnStop, btnTalk));
        return c;
    }

    private View buildEncoder() {
        LinearLayout c = column();
        spBitrate = spinner(BITRATES, 11);                  // 64000
        spBandwidth = spinner(BANDWIDTH_NAMES, 4);
        spFrame = spinner(FRAMES, 1);                       // 20 ms
        spLoss = spinner(LOSSES, 2);                        // 20 %
        c.addView(label("Bitrate (bit/s)")); c.addView(spBitrate);
        c.addView(label("Bandwidth")); c.addView(spBandwidth);
        c.addView(label("Frame (ms)")); c.addView(spFrame);
        c.addView(label("Expected packet loss (%)")); c.addView(spLoss);

        RadioGroup rg = new RadioGroup(this);
        rg.setOrientation(RadioGroup.HORIZONTAL);
        rbMono = new RadioButton(this); rbMono.setText("Mono"); rbMono.setId(View.generateViewId());
        rbStereo = new RadioButton(this); rbStereo.setText("Stereo"); rbStereo.setId(View.generateViewId());
        rg.addView(rbMono); rg.addView(rbStereo);
        rbMono.setChecked(true);
        c.addView(label("Channels")); c.addView(rg);

        cbDtx = check("useDtx", true);
        cbMusicMode = check("musicMode (Opus tuned for music)", false);
        cbAdaptive = check("Adaptive (playout buffer)", false);
        cbCallMode = check("Call mode (Android: like WhatsApp, Bluetooth handsfree; off = media mode)", true);
        cbAgc = check("Auto gain (webrtc AGC2)", false);
        cbHpf = check("Low-cut (webrtc high-pass ~100 Hz)", false);
        spNoise = spinner(NOISE, 0);
        spEcho = spinner(ECHO, 0);
        for (View v : new View[]{cbDtx, cbMusicMode, cbAdaptive, cbCallMode, cbAgc, cbHpf, spNoise, spEcho}) c.addView(v);
        cbAdaptive.setOnCheckedChangeListener((b, on) -> set(6, on ? 1 : 0));      // also live

        TextView note = new TextView(this);
        if (!webrtc) {
            note.setText("This build has no webrtc-audio-processing: echo / noise / auto gain / low-cut are disabled.");
            for (View v : new View[]{cbAgc, cbHpf, spNoise, spEcho}) v.setEnabled(false);
        } else {
            note.setText("webrtc audio processing is available. Settings here are applied on Start.");
        }
        c.addView(note);
        return c;
    }

    private View buildMusic() {
        LinearLayout c = column();
        edMusicPath = edit("/sdcard/Music/test.mp3", false);
        c.addView(label("Music file (path)"));
        c.addView(edMusicPath);
        btnPlay = button("Play", v -> {
            if (Build.VERSION.SDK_INT >= 23 && Build.VERSION.SDK_INT < 33
                    && ContextCompat.checkSelfPermission(this, Manifest.permission.READ_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {
                ActivityCompat.requestPermissions(this, new String[]{Manifest.permission.READ_EXTERNAL_STORAGE}, REQ_PERMISSIONS + 1);
                Toast.makeText(this, "Allow reading files, then press Play again", Toast.LENGTH_SHORT).show();
                return;
            }
            if (bound && service != null) service.music(0, edMusicPath.getText().toString().trim());
        });
        btnPause = button("Pause", v -> { if (bound && service != null) service.music(1, ""); });
        btnResume = button("Resume", v -> { if (bound && service != null) service.music(2, ""); });
        btnMusicStop = button("Stop", v -> { if (bound && service != null) service.music(3, ""); });
        c.addView(row(btnPlay, btnPause));
        c.addView(row(btnResume, btnMusicStop));

        sbMusicGain = seek(200, 100);
        c.addView(label("Music volume (sent)"));
        c.addView(sbMusicGain);
        onSeek(sbMusicGain, v -> set(3, v));

        cbMonitor = check("Hear music here too (use headphones)", false);
        cbMonitor.setOnCheckedChangeListener((b, on) -> set(4, on ? 1 : 0));
        c.addView(cbMonitor);
        sbMonitor = seek(200, 100);
        c.addView(label("Monitor volume"));
        c.addView(sbMonitor);
        onSeek(sbMonitor, v -> set(5, v));
        return c;
    }

    private View buildLog() {
        LinearLayout c = column();
        c.addView(button("Copy log", v -> {
            ClipboardManager cm = (ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
            if (cm != null) cm.setPrimaryClip(ClipData.newPlainText("RavAva log", tvLog.getText()));
        }));
        tvLog = new TextView(this);
        tvLog.setTypeface(Typeface.MONOSPACE);
        tvLog.setTextSize(12);
        tvLog.setTextIsSelectable(true);
        tvLog.setText("Idle");
        ScrollView sv = new ScrollView(this);
        sv.addView(tvLog);
        c.addView(sv, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f));
        return c;
    }

    // ---------------------------------------------------------------- logic
    private static String idAt(List<String> ids, int pos) { return pos >= 0 && pos < ids.size() ? ids.get(pos) : ""; }

    private void fillDevices(Spinner sp, boolean capture, List<String> ids) {
        final String keep = idAt(ids, sp.getSelectedItemPosition());
        String[] raw = VoiceService.listDevices(capture);
        List<String> names = new ArrayList<>();
        ids.clear();
        names.add("System default"); ids.add("");
        if (raw != null) for (String e : raw) {
            String[] p = e.split("\n", -1);
            names.add(p[0] + (p.length > 2 && "1".equals(p[2]) ? " (default)" : ""));
            ids.add(p.length > 1 ? p[1] : "");
        }
        // refilling is not a user choice: no device switch (the listeners read ids by position, so keep the position by id)
        AdapterView.OnItemSelectedListener l = sp.getOnItemSelectedListener();
        sp.setOnItemSelectedListener(null);
        sp.setAdapter(new ArrayAdapter<>(this, android.R.layout.simple_spinner_dropdown_item, names));
        int idx = ids.indexOf(keep);
        sp.setSelection(idx >= 0 ? idx : 0, false);
        sp.setOnItemSelectedListener(l);
    }

    private void refreshDevices() {
        fillDevices(spInput, true, inIds);
        fillDevices(spOutput, false, outIds);
    }

    private void set(int what, int v) { if (bound && service != null) service.set(what, v); }

    private String serverIp() {
        int i = spServer.getSelectedItemPosition();
        return i == SERVER_NAMES.length - 1 ? edCustomIp.getText().toString().trim() : SERVER_IPS[i];
    }

    private int serverPort() {
        try { return Integer.parseInt(edPort.getText().toString().trim()); } catch (Exception e) { return 10001; }
    }

    private void applyNetwork() {
        if (bound && service != null && running) service.setNetwork(cbLoopback.isChecked(), serverIp(), serverPort());
    }

    // microphone, and on Android 12+ the Bluetooth permission (without it a connected headset is not listed / usable)
    private boolean checkPermissions() {
        List<String> need = new ArrayList<>();
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED)
            need.add(Manifest.permission.RECORD_AUDIO);
        if (Build.VERSION.SDK_INT >= 31
                && ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED)
            need.add(Manifest.permission.BLUETOOTH_CONNECT);
        if (need.isEmpty()) return true;
        ActivityCompat.requestPermissions(this, need.toArray(new String[0]), REQ_PERMISSIONS);
        return false;
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == REQ_PERMISSIONS) {
            boolean mic = ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED;
            if (mic) { refreshDevices(); startSession(); }       // (a refused Bluetooth permission only means no headset in the lists)
            else Toast.makeText(this, R.string.permission_required, Toast.LENGTH_LONG).show();
        }
    }

    private void startSession() {
        if (!bound || service == null) { Toast.makeText(this, "Service not ready", Toast.LENGTH_SHORT).show(); return; }
        refreshDevices();       // devices may have been plugged in since the last Start
        int[] p = {
                cbLoopback.isChecked() ? 1 : 0,
                Integer.parseInt(BITRATES[spBitrate.getSelectedItemPosition()]),
                BANDWIDTH_VALUES[spBandwidth.getSelectedItemPosition()],
                Integer.parseInt(FRAMES[spFrame.getSelectedItemPosition()]),
                Integer.parseInt(LOSSES[spLoss.getSelectedItemPosition()]),
                rbStereo.isChecked() ? 2 : 1,
                cbAgc.isChecked() ? 1 : 0,
                cbCallMode.isChecked() ? 1 : 0,
                cbAdaptive.isChecked() ? 1 : 0,
                cbHpf.isChecked() ? 1 : 0,
                cbDtx.isChecked() ? 1 : 0,
                cbMusicMode.isChecked() ? 1 : 0,
                spNoise.getSelectedItemPosition(),
                spEcho.getSelectedItemPosition(),
                sbVolume.getProgress(), sbMicGain.getProgress(),
                btnTalk.isChecked() ? 1 : 0,
                sbMusicGain.getProgress(),
                cbMonitor.isChecked() ? 1 : 0,
                sbMonitor.getProgress()};
        Intent svc = new Intent(this, VoiceService.class);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) startForegroundService(svc); else startService(svc);

        boolean ok = service.startSession(p, serverIp(), serverPort(),
                idAt(inIds, spInput.getSelectedItemPosition()), idAt(outIds, spOutput.getSelectedItemPosition()));
        if (!ok) { Toast.makeText(this, "Failed to start (see logcat -s ravava)", Toast.LENGTH_LONG).show(); return; }
        running = true;
        updateRunning();
    }

    private void stopSession() {
        if (bound && service != null) service.stopSession();
        running = false;
        pbReceived.setProgress(0);
        pbMine.setProgress(0);
        tvRate.setText("");
        updateRunning();
        showDeviceCounts();
    }

    private void updateRunning() {
        if (btnStart == null) return;
        btnStart.setEnabled(!running);
        btnStop.setEnabled(running);
        updateMusicButtons();
    }

    private void updateMusicButtons() {
        int st = bound && service != null && running ? service.musicState() : 0;
        btnPlay.setEnabled(running);
        btnPause.setEnabled(running && st == 1);
        btnResume.setEnabled(running && st == 2);
        btnMusicStop.setEnabled(running && st != 0);
    }

    @Override protected void onStart() {
        super.onStart();
        bindService(new Intent(this, VoiceService.class), connection, Context.BIND_AUTO_CREATE);
        ui.post(ticker);
    }

    @Override protected void onResume() {
        super.onResume();
        if (!running && prefs != null && spInput != null) { refreshDevices(); showDeviceCounts(); }   // a headset may have been connected meanwhile
    }

    @Override protected void onPause() {
        saveState();
        super.onPause();
    }

    @Override protected void onStop() {
        saveState();
        ui.removeCallbacks(ticker);
        if (bound) { unbindService(connection); bound = false; }
        super.onStop();
    }
}
