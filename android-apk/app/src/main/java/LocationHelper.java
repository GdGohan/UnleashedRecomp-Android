package com.sega.sonicunr;

import android.Manifest;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.location.Location;
import android.location.LocationManager;

public class LocationHelper {

    private static final String PREFS = "location_prefs";

    public static void updateLastKnownLocation(Context context) {
        if (context.checkSelfPermission(Manifest.permission.ACCESS_COARSE_LOCATION)
                != PackageManager.PERMISSION_GRANTED) {
            return; // sem permissão, mantém fallback fixo
        }

        LocationManager lm = (LocationManager) context.getSystemService(Context.LOCATION_SERVICE);
        if (lm == null) return;

        Location best = null;
        for (String provider : lm.getProviders(true)) {
            try {
                Location loc = lm.getLastKnownLocation(provider);
                if (loc != null && (best == null || loc.getAccuracy() < best.getAccuracy())) {
                    best = loc;
                }
            } catch (SecurityException ignored) { }
        }

        if (best != null) {
            SharedPreferences.Editor editor = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE).edit();
            editor.putFloat("lat", (float) best.getLatitude());
            editor.putFloat("lon", (float) best.getLongitude());
            editor.apply();
        }
    }

    public static boolean hasLocation(Context context) {
        return context.getSharedPreferences(PREFS, Context.MODE_PRIVATE).contains("lat");
    }

    public static double getLat(Context context) {
        return context.getSharedPreferences(PREFS, Context.MODE_PRIVATE).getFloat("lat", 0f);
    }

    public static double getLon(Context context) {
        return context.getSharedPreferences(PREFS, Context.MODE_PRIVATE).getFloat("lon", 0f);
    }
}