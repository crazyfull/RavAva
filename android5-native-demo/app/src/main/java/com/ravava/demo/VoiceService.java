package com.ravava.demo;

import android.app.Notification;
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

    // Native methods
    private static native boolean nativeInit(Object context);
    private static native boolean nativeStart(boolean udpMode, int localPort,
                                              String remoteIp, int remotePort);
    private static native void nativeStop();
    private static native int nativeMicLevel();
    private static native int nativeSpkLevel();
    private static native String nativeStats();

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

    public boolean startSession(boolean udpMode, int localPort, String remoteIp, int remotePort) {
        if (sessionRunning) return true;
        boolean ok = nativeStart(udpMode, localPort, remoteIp, remotePort);
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

    public int getMicLevel() {
        return sessionRunning ? nativeMicLevel() : 0;
    }

    public int getSpkLevel() {
        return sessionRunning ? nativeSpkLevel() : 0;
    }

    public String getStatsText() {
        return sessionRunning ? nativeStats() : getString(R.string.status_idle);
    }

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
