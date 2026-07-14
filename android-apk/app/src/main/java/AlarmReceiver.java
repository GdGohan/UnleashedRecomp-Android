package com.sega.sonicunr;

import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;

public class AlarmReceiver extends BroadcastReceiver {

    private static final String PACKAGE = "com.sega.sonicunr";

    @Override
    public void onReceive(Context context, Intent intent) {
        String action = intent.getAction();
        if (action == null) return;

        switch (action) {
            case AlarmScheduler.ACTION_WARN_NIGHT_CURRENT:
                // faltam 5 min: mostra estado ATUAL (dia/Sun)
                NotificationHelper.showOrUpdate(context, "Sonic is losing control...", R.mipmap.sunmedalmonochrome, R.mipmap.sunmedal);
                break;

            case AlarmScheduler.ACTION_WARN_NIGHT_NEXT:
                // faltam 2 min: mostra PRÓXIMO estado (noite/Moon)
                NotificationHelper.showOrUpdate(context, "Werehog is awakening", R.mipmap.moonmedalmonochrome, R.mipmap.moonmedal);
                break;

            case AlarmScheduler.ACTION_SWITCH_NIGHT:
                switchIcon(context, false); // ativa ícone de noite
                NotificationHelper.cancel(context);
                break;

            case AlarmScheduler.ACTION_WARN_DAY_CURRENT:
                // faltam 5 min: mostra estado ATUAL (noite/Moon)
                NotificationHelper.showOrUpdate(context, "The day is dawning", R.mipmap.moonmedalmonochrome, R.mipmap.moonmedal);
                break;

            case AlarmScheduler.ACTION_WARN_DAY_NEXT:
                // faltam 2 min: mostra PRÓXIMO estado (dia/Sun)
                NotificationHelper.showOrUpdate(context, "Sonic is returning to normal...", R.mipmap.sunmedalmonochrome, R.mipmap.sunmedal);
                break;

            case AlarmScheduler.ACTION_SWITCH_DAY:
                switchIcon(context, true); // ativa ícone de dia
                NotificationHelper.cancel(context);
                break;
            
            case EclipseScheduler.ACTION_ECLIPSE_START:
                EclipseScheduler.activateEclipseIcon(context, "");
                NotificationHelper.showOrUpdate(context, "A lunar eclipse is happening...", R.mipmap.eclipsemedalmono, R.mipmap.eclipsemedal);
                break;
            
            case EclipseScheduler.ACTION_ECLIPSE_END:
                EclipseScheduler.deactivateEclipseIcon(context);
                NotificationHelper.cancel(context);
                break;
            
            case EclipseScheduler.ACTION_ECLIPSE_START2:
                EclipseScheduler.activateEclipseIcon(context, "2");
                NotificationHelper.showOrUpdate(context, "A solar eclipse is happening...", R.mipmap.eclipsemedalmono, R.mipmap.eclipsemedal);
                break;
            
            case EclipseScheduler.ACTION_ECLIPSE_END2:
                EclipseScheduler.deactivateEclipseIcon(context);
                NotificationHelper.cancel(context);
                break;
        }

        // Autossustentável: reagenda tudo de novo pro dia seguinte
        AlarmScheduler.scheduleAll(context);
    }

    private void switchIcon(Context context, boolean activateDay) {
        PackageManager pm = context.getPackageManager();
        ComponentName dayIcon = new ComponentName(PACKAGE, PACKAGE + ".DayIcon");
        ComponentName nightIcon = new ComponentName(PACKAGE, PACKAGE + ".NightIcon");

        if (activateDay) {
            pm.setComponentEnabledSetting(dayIcon, PackageManager.COMPONENT_ENABLED_STATE_ENABLED, PackageManager.DONT_KILL_APP);
            pm.setComponentEnabledSetting(nightIcon, PackageManager.COMPONENT_ENABLED_STATE_DISABLED, PackageManager.DONT_KILL_APP);
        } else {
            pm.setComponentEnabledSetting(nightIcon, PackageManager.COMPONENT_ENABLED_STATE_ENABLED, PackageManager.DONT_KILL_APP);
            pm.setComponentEnabledSetting(dayIcon, PackageManager.COMPONENT_ENABLED_STATE_DISABLED, PackageManager.DONT_KILL_APP);
        }
    }
}