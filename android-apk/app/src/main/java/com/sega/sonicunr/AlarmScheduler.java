package com.sega.sonicunr;

import android.app.AlarmManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import java.util.Calendar;
import android.content.ComponentName;
import android.content.pm.PackageManager;

public class AlarmScheduler {

    public static final String ACTION_WARN_NIGHT_CURRENT = "com.sega.sonicunr.WARN_NIGHT_CURRENT";
    public static final String ACTION_WARN_NIGHT_NEXT    = "com.sega.sonicunr.WARN_NIGHT_NEXT";
    public static final String ACTION_SWITCH_NIGHT       = "com.sega.sonicunr.SWITCH_NIGHT";
    public static final String ACTION_WARN_DAY_CURRENT   = "com.sega.sonicunr.WARN_DAY_CURRENT";
    public static final String ACTION_WARN_DAY_NEXT      = "com.sega.sonicunr.WARN_DAY_NEXT";
    public static final String ACTION_SWITCH_DAY         = "com.sega.sonicunr.SWITCH_DAY";

    private static final int REQ_WARN_NIGHT_CURRENT = 101;
    private static final int REQ_WARN_NIGHT_NEXT    = 102;
    private static final int REQ_SWITCH_NIGHT       = 103;
    private static final int REQ_WARN_DAY_CURRENT   = 104;
    private static final int REQ_WARN_DAY_NEXT      = 105;
    private static final int REQ_SWITCH_DAY         = 106;

    // Horário em que a troca de fato acontece (ajuste como quiser)
    private static final int NIGHT_SWITCH_HOUR = 18;
    private static final int NIGHT_SWITCH_MIN  = 0;
    private static final int DAY_SWITCH_HOUR   = 6;
    private static final int DAY_SWITCH_MIN    = 0;

    private static int nightHour = NIGHT_SWITCH_HOUR;
    private static int dayHour = DAY_SWITCH_HOUR;
    private static int nightMin = NIGHT_SWITCH_MIN;
    private static int dayMin = DAY_SWITCH_MIN;

    static class IconTheme {
        public static String DEFAULT = "com.sega.sonicunr.";
        public static String HALLOWEEN = "com.sega.sonicunr.Hw";
    }

    private static String getCurrentTheme() {
        Calendar c = Calendar.getInstance();

        int month = c.get(Calendar.MONTH) + 1;
        int day = c.get(Calendar.DAY_OF_MONTH);

        if (month == 10 && day >= 25 && day <= 31)
            return IconTheme.HALLOWEEN;

        //if (month == 12 && day >= 20 && day <= 26)
            //return IconTheme.CHRISTMAS;

        return IconTheme.DEFAULT;
    }

    public static void locAdjust(Context context) {
        if (LocationHelper.hasLocation(context)) {
            double lat = LocationHelper.getLat(context);
            double lon = LocationHelper.getLon(context);
            int[] sun = SunCalculator.calculate(lat, lon, Calendar.getInstance());
            dayHour = sun[0];
            dayMin = sun[1];
            nightHour = sun[2];
            nightMin = sun[3];
        }
    }

    public static void scheduleAll(Context context) {
        scheduleAlarm(context, nightHour - 1, 55, ACTION_WARN_NIGHT_CURRENT, REQ_WARN_NIGHT_CURRENT);
        scheduleAlarm(context, nightHour - 1, 58, ACTION_WARN_NIGHT_NEXT, REQ_WARN_NIGHT_NEXT);
        scheduleAlarm(context, nightHour, nightMin, ACTION_SWITCH_NIGHT, REQ_SWITCH_NIGHT);

        scheduleAlarm(context, dayHour - 1, 55, ACTION_WARN_DAY_CURRENT, REQ_WARN_DAY_CURRENT);
        scheduleAlarm(context, dayHour - 1, 58, ACTION_WARN_DAY_NEXT, REQ_WARN_DAY_NEXT);
        scheduleAlarm(context, dayHour, dayMin, ACTION_SWITCH_DAY, REQ_SWITCH_DAY);

    }

    public static void applyCurrentIconState(Context context) {
        Calendar now = Calendar.getInstance();
        int hour = now.get(Calendar.HOUR_OF_DAY);
        int minute = now.get(Calendar.MINUTE);
        int nowMinutes = hour * 60 + minute;

        int nightSwitchMinutes = nightHour * 60 + nightMin; // 18:00
        int daySwitchMinutes = dayHour * 60 + dayMin;       // 06:00

        boolean isNight;
        if (daySwitchMinutes < nightSwitchMinutes) {
            // dia começa antes da noite no relógio (caso normal: 06:00 dia, 18:00 noite)
            isNight = nowMinutes >= nightSwitchMinutes || nowMinutes < daySwitchMinutes;
        } else {
            isNight = nowMinutes >= nightSwitchMinutes && nowMinutes < daySwitchMinutes;
        }

        setIconState(context, isNight);
    }

    private static void setIconState(Context context, boolean isNight) {
        PackageManager pm = context.getPackageManager();

        ComponentName dayIcon = new ComponentName(context, getCurrentTheme() + "DayIcon");
        ComponentName nightIcon = new ComponentName(context, getCurrentTheme() + "NightIcon");

        if (isNight) {
            pm.setComponentEnabledSetting(nightIcon, PackageManager.COMPONENT_ENABLED_STATE_ENABLED, PackageManager.DONT_KILL_APP);
            pm.setComponentEnabledSetting(dayIcon, PackageManager.COMPONENT_ENABLED_STATE_DISABLED, PackageManager.DONT_KILL_APP);
        } else {
            pm.setComponentEnabledSetting(dayIcon, PackageManager.COMPONENT_ENABLED_STATE_ENABLED, PackageManager.DONT_KILL_APP);
            pm.setComponentEnabledSetting(nightIcon, PackageManager.COMPONENT_ENABLED_STATE_DISABLED, PackageManager.DONT_KILL_APP);
        }
    }

    private static void scheduleAlarm(Context context, int hour, int minute, String action, int requestCode) {
        AlarmManager alarmManager = (AlarmManager) context.getSystemService(Context.ALARM_SERVICE);
        if (alarmManager == null) return;

        Calendar cal = Calendar.getInstance();
        cal.set(Calendar.HOUR_OF_DAY, hour);
        cal.set(Calendar.MINUTE, minute);
        cal.set(Calendar.SECOND, 0);
        cal.set(Calendar.MILLISECOND, 0);

        if (cal.getTimeInMillis() <= System.currentTimeMillis()) {
            cal.add(Calendar.DAY_OF_YEAR, 1);
        }

        Intent intent = new Intent(context, AlarmReceiver.class);
        intent.setAction(action);

        PendingIntent pendingIntent = PendingIntent.getBroadcast(
                context,
                requestCode,
                intent,
                PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE
        );

        // A partir da API 31, precisa checar permissão antes de usar alarme exato
        if (android.os.Build.VERSION.SDK_INT >= 31 && !alarmManager.canScheduleExactAlarms()) {
            // Sem permissão -> usa versão inexata pra não crashar
            alarmManager.setAndAllowWhileIdle(
                    AlarmManager.RTC_WAKEUP,
                    cal.getTimeInMillis(),
                    pendingIntent
            );
            return;
        }

        alarmManager.setExactAndAllowWhileIdle(
                AlarmManager.RTC_WAKEUP,
                cal.getTimeInMillis(),
                pendingIntent
        );
    }
}