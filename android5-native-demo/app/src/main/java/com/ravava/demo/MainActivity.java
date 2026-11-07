package com.ravava.demo;

import android.Manifest;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.view.View;
import android.widget.Button;
import android.widget.ProgressBar;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import com.google.android.material.textfield.TextInputEditText;

public class MainActivity extends AppCompatActivity {

    private static final int REQ_PERMISSIONS = 1001;

    private RadioGroup modeGroup;
    private RadioButton radioLoopback, radioUdp;
    private View udpSettings;
    private TextInputEditText editLocalPort, editRemoteIp, editRemotePort;
    private Button btnStartStop;
    private ProgressBar levelMic, levelSpk;
    private TextView txtStats;

    private VoiceService voiceService;
    private boolean bound = false;
    private boolean running = false;

    private final Handler uiHandler = new Handler(Looper.getMainLooper());
    private final Runnable levelTicker = new Runnable() {
        @Override
        public void run() {
            if (bound && voiceService != null && running) {
                levelMic.setProgress(voiceService.getMicLevel());
                levelSpk.setProgress(voiceService.getSpkLevel());
                txtStats.setText(voiceService.getStatsText());
            }
            uiHandler.postDelayed(this, 150);
        }
    };

    private final ServiceConnection connection = new ServiceConnection() {
        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            VoiceService.LocalBinder binder = (VoiceService.LocalBinder) service;
            voiceService = binder.getService();
            bound = true;
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
            bound = false;
            voiceService = null;
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        modeGroup = findViewById(R.id.modeGroup);
        radioLoopback = findViewById(R.id.radioLoopback);
        radioUdp = findViewById(R.id.radioUdp);
        udpSettings = findViewById(R.id.udpSettings);
        editLocalPort = findViewById(R.id.editLocalPort);
        editRemoteIp = findViewById(R.id.editRemoteIp);
        editRemotePort = findViewById(R.id.editRemotePort);
        btnStartStop = findViewById(R.id.btnStartStop);
        levelMic = findViewById(R.id.levelMic);
        levelSpk = findViewById(R.id.levelSpk);
        txtStats = findViewById(R.id.txtStats);

        modeGroup.setOnCheckedChangeListener((group, checkedId) -> {
            boolean udp = checkedId == R.id.radioUdp;
            udpSettings.setVisibility(udp ? View.VISIBLE : View.GONE);
        });

        btnStartStop.setOnClickListener(v -> {
            if (running) {
                stopSession();
            } else {
                if (checkPermissions()) {
                    startSession();
                }
            }
        });
    }

    @Override
    protected void onStart() {
        super.onStart();
        Intent intent = new Intent(this, VoiceService.class);
        bindService(intent, connection, Context.BIND_AUTO_CREATE);
        uiHandler.post(levelTicker);
    }

    @Override
    protected void onStop() {
        uiHandler.removeCallbacks(levelTicker);
        if (bound) {
            unbindService(connection);
            bound = false;
        }
        super.onStop();
    }

    private boolean checkPermissions() {
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO)
                != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this,
                    new String[]{Manifest.permission.RECORD_AUDIO}, REQ_PERMISSIONS);
            return false;
        }
        return true;
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions,
                                           @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == REQ_PERMISSIONS) {
            if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                startSession();
            } else {
                Toast.makeText(this, R.string.permission_required, Toast.LENGTH_LONG).show();
            }
        }
    }

    private void startSession() {
        if (!bound || voiceService == null) {
            Toast.makeText(this, "Service not ready", Toast.LENGTH_SHORT).show();
            return;
        }

        boolean udpMode = radioUdp.isChecked();
        int localPort = 5000;
        String remoteIp = "127.0.0.1";
        int remotePort = 5000;

        if (udpMode) {
            try {
                localPort = Integer.parseInt(editLocalPort.getText().toString().trim());
                remotePort = Integer.parseInt(editRemotePort.getText().toString().trim());
                remoteIp = editRemoteIp.getText().toString().trim();
            } catch (Exception e) {
                Toast.makeText(this, "Invalid port / IP", Toast.LENGTH_SHORT).show();
                return;
            }
        }

        Intent svc = new Intent(this, VoiceService.class);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            startForegroundService(svc);
        } else {
            startService(svc);
        }

        boolean ok = voiceService.startSession(udpMode, localPort, remoteIp, remotePort);
        if (!ok) {
            Toast.makeText(this, "Failed to start (see logcat -s ravava)", Toast.LENGTH_LONG).show();
            return;
        }

        running = true;
        btnStartStop.setText(R.string.stop);
        radioLoopback.setEnabled(false);
        radioUdp.setEnabled(false);
        setUdpEnabled(false);
    }

    private void stopSession() {
        if (bound && voiceService != null) {
            voiceService.stopSession();
        }
        running = false;
        btnStartStop.setText(R.string.start);
        radioLoopback.setEnabled(true);
        radioUdp.setEnabled(true);
        setUdpEnabled(radioUdp.isChecked());
        levelMic.setProgress(0);
        levelSpk.setProgress(0);
        txtStats.setText(R.string.status_idle);
    }

    private void setUdpEnabled(boolean enabled) {
        editLocalPort.setEnabled(enabled);
        editRemoteIp.setEnabled(enabled);
        editRemotePort.setEnabled(enabled);
    }
}
