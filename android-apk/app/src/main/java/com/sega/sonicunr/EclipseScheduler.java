package com.sega.sonicunr;

import android.app.AlarmManager;
import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import java.util.Calendar;

public class EclipseScheduler {

    public static final String ACTION_ECLIPSE_START = "com.sega.sonicunr.ECLIPSE_START";
    public static final String ACTION_ECLIPSE_END   = "com.sega.sonicunr.ECLIPSE_END";
    public static final String ACTION_ECLIPSE_START2 = "com.sega.sonicunr.ECLIPSE_START2";
    public static final String ACTION_ECLIPSE_END2   = "com.sega.sonicunr.ECLIPSE_END2";

    public static void enableDarkGaia(Context ctx) {
        //activateEclipseIcon(ctx);
    }

    public static void disableDarkGaia(Context ctx) {
        deactivateEclipseIcon(ctx);
    }

    public static void scheduleNextEclipse(Context context) {
        EclipseApiService.fetchNextEclipse(context, new EclipseApiService.Callback() {
            @Override
            public void onResult(long[] nextEclipseMillis) {
                scheduleFromMillis(context, nextEclipseMillis[0], nextEclipseMillis[1], true);
            }

            @Override
            public void onError(String message) {
            }
        });
        EclipseApiService.fetchNextEclipse2(context, new EclipseApiService.Callback() {
            @Override
            public void onResult(long[] nextEclipseMillis) {
                scheduleFromMillis(context, nextEclipseMillis[0], nextEclipseMillis[1], false);
            }

            @Override
            public void onError(String message) {
            }
        });
    }

    private static void scheduleFromMillis(Context context, long eclipseMillis, long endMillis, boolean z) {

        AlarmManager alarm =
                (AlarmManager) context.getSystemService(Context.ALARM_SERVICE);

        Intent start = new Intent(context, AlarmReceiver.class);
        start.setAction(z ? ACTION_ECLIPSE_START : ACTION_ECLIPSE_START2);

        PendingIntent piStart = PendingIntent.getBroadcast(
                context,
                100,
                start,
                PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE
        );

        alarm.setExactAndAllowWhileIdle(
                AlarmManager.RTC_WAKEUP,
                eclipseMillis,
                piStart
        );

        Intent end = new Intent(context, AlarmReceiver.class);
        end.setAction(z ? ACTION_ECLIPSE_END : ACTION_ECLIPSE_END2);

        PendingIntent piEnd = PendingIntent.getBroadcast(
                context,
                101,
                end,
                PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE
        );

        alarm.setExactAndAllowWhileIdle(
                AlarmManager.RTC_WAKEUP,
                endMillis,
                piEnd
        );
    }

    public static void activateEclipseIcon(Context context, String str) {
        PackageManager pm = context.getPackageManager();
        pm.setComponentEnabledSetting(new ComponentName(context, "com.sega.sonicunr.EclipseIcon" + str),
                PackageManager.COMPONENT_ENABLED_STATE_ENABLED, PackageManager.DONT_KILL_APP);
        pm.setComponentEnabledSetting(new ComponentName(context, "com.sega.sonicunr.DayIcon"),
                PackageManager.COMPONENT_ENABLED_STATE_DISABLED, PackageManager.DONT_KILL_APP);
        pm.setComponentEnabledSetting(new ComponentName(context, "com.sega.sonicunr.NightIcon"),
                PackageManager.COMPONENT_ENABLED_STATE_DISABLED, PackageManager.DONT_KILL_APP);
    }

    public static void deactivateEclipseIcon(Context context) {
        // volta ao estado normal (dia ou noite, conforme a hora atual)
        AlarmScheduler.applyCurrentIconState(context);
    }
}