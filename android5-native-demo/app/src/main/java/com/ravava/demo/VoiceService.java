package com.ravava.demo;

import android.app.Notification;
import android.content.Context;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Intent;
import android.os.Binder;
import android.os.Build;
import android.os.IBinder;

import androidx.core.app.NotificationCompat;

/**
 * Foreground service that keeps the native voice session alive when the app is minimized.
 * All heavy work lives in the native library (RavAva).
 */
public class VoiceService extends Service {

    private static final String CHANNEL_ID = "ravava_voice";
    private static final int NOTIF_ID = 42;

    private final IBinder binder = new LocalBinder();
    private boolean sessionRunning = false;

    static {
        System.loadLibrary("ravava_demo");
    }

    // Native methods (cpp/native-lib.cpp)
    private static native boolean nativeInit(Object context);
    private static native boolean nativeWebrtc();
    // p: loopback, bitrate, bandwidth, frameMs, lossPct, channels, agc, callMode, adaptive, hpf, dtx, musicMode, noise, echo,
    //    volume, micGain, talk, musicVolume, monitor, monitorVolume  (booleans as 0 / 1)
    private static native boolean nativeStart(int[] p, String server, int port, String inDev, String outDev);
    private static native void nativeStop();
    private static native void nativeSet(int what, int value);       // 0 volume, 1 mic gain, 2 talk, 3 music vol, 4 monitor, 5 monitor vol, 6 adaptive
    private static native void nativeSetDevice(boolean capture, String id);
    private static native void nativeSetNetwork(boolean loopback, String server, int port);
    private static native void nativeMusic(int op, String path);      // 0 play, 1 pause, 2 resume, 3 stop
    private static native int nativeMusicState();                     // 0 stopped, 1 playing, 2 paused, 3 finished
    private static native String[] nativeListDevices(boolean capture);   // "name\nid\ndefault"
    private static native int nativeMicLevel();
    private static native int nativeSpkLevel();
    private static native String nativeStats();
    private static native String nativeRate();

    public class LocalBinder extends Binder {
        VoiceService getService() {
            return VoiceService.this;
        }
    }

    @Override
    public void onCreate() {
        super.onCreate();
        createNotificationChannel();
        // Give the library the Application context + JavaVM (once)
        nativeInit(getApplicationContext());
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        startForeground(NOTIF_ID, buildNotification());
        return START_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return binder;
    }

    @Override
    public void onDestroy() {
        if (sessionRunning) {
            nativeStop();
            sessionRunning = false;
        }
        super.onDestroy();
    }

    public boolean startSession(int[] p, String server, int port, String inDev, String outDev) {
        if (sessionRunning) return true;
        boolean ok = nativeStart(p, server, port, inDev, outDev);
        sessionRunning = ok;
        return ok;
    }

    public void stopSession() {
        if (!sessionRunning) return;
        nativeStop();
        sessionRunning = false;
        stopForeground(true);
        stopSelf();
    }

    // Gives the library the Application context + JavaVM (needed before the device lists work); safe to call again.
    public static void init(Context c) { nativeInit(c.getApplicationContext()); }

    public boolean isRunning() { return sessionRunning; }
    public static boolean webrtcAvailable() { return nativeWebrtc(); }
    public void set(int what, int value) { if (sessionRunning) nativeSet(what, value); }
    public void setDevice(boolean capture, String id) { if (sessionRunning) nativeSetDevice(capture, id); }
    public void setNetwork(boolean loopback, String server, int port) { if (sessionRunning) nativeSetNetwork(loopback, server, port); }
    public void music(int op, String path) { if (sessionRunning) nativeMusic(op, path); }
    public int musicState() { return sessionRunning ? nativeMusicState() : 0; }
    public static String[] listDevices(boolean capture) { return nativeListDevices(capture); }
    public int getMicLevel() { return sessionRunning ? nativeMicLevel() : 0; }
    public int getSpkLevel() { return sessionRunning ? nativeSpkLevel() : 0; }
    public String getStatsText() { return sessionRunning ? nativeStats() : getString(R.string.status_idle); }
    public String getRateText() { return sessionRunning ? nativeRate() : ""; }

    private void createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationChannel ch = new NotificationChannel(
                    CHANNEL_ID,
                    "RavAva Voice",
                    NotificationManager.IMPORTANCE_LOW);
            ch.setDescription("Keeps the voice session alive");
            NotificationManager nm = getSystemService(NotificationManager.class);
            if (nm != null) nm.createNotificationChannel(ch);
        }
    }

    private Notification buildNotification() {
        Intent open = new Intent(this, MainActivity.class);
        PendingIntent pi = PendingIntent.getActivity(
                this, 0, open,
                PendingIntent.FLAG_UPDATE_CURRENT |
                        (Build.VERSION.SDK_INT >= 23 ? PendingIntent.FLAG_IMMUTABLE : 0));

        return new NotificationCompat.Builder(this, CHANNEL_ID)
                .setContentTitle(getString(R.string.notification_title))
                .setContentText(getString(R.string.notification_text))
                .setSmallIcon(android.R.drawable.ic_btn_speak_now)
                .setContentIntent(pi)
                .setOngoing(true)
                .build();
    }
}
