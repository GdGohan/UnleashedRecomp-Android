package com.sega.sonicunr;

import android.content.Context;
import android.content.SharedPreferences;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.URL;
import java.text.SimpleDateFormat;
import java.util.Calendar;
import java.util.Locale;
import java.util.TimeZone;

import org.json.JSONArray;
import org.json.JSONObject;

public class EclipseApiService {

    private static final String PREFS = "eclipse_prefs";
    private static final String KEY_NEXT_ECLIPSE_MILLIS = "next_eclipse_millis";
    private static final String KEY_LAST_CHECK = "last_check_millis";

    // IMPORTANTE: testar essa URL manualmente antes (navegador ou curl) pra confirmar o formato exato de resposta
    private static final String API_URL = "https://opale.imcce.fr/api/v1/phenomena/eclipses/301/";
    private static final String API_URL2 = "https://opale.imcce.fr/api/v1/phenomena/eclipses/10/";

    public interface Callback {
        void onResult(long[] nextEclipseMillis);
        void onError(String message);
    }

    public static void fetchNextEclipse(Context context, Callback callback) {
        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
    
                    Calendar cal = Calendar.getInstance(TimeZone.getTimeZone("UTC"));
    
                    int year = cal.get(Calendar.YEAR);
    
                    long[] next = findNextEclipseInYear(year, true);

                    if (next != null) {
                        saveToCache(context, next);
                        callback.onResult(next);
                        return;
                    }
    
                    callback.onError("");
    
                } catch (Exception e) {
    
                    long cached = getCachedEclipse(context);
    
                    if (cached > 0)
                        callback.onResult(new long[]{cached});
                    else
                        callback.onError(e.getMessage());
                }
            }
        }).start();
    }
    
    public static void fetchNextEclipse2(Context context, Callback callback) {
        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
    
                    Calendar cal = Calendar.getInstance(TimeZone.getTimeZone("UTC"));
    
                    int year = cal.get(Calendar.YEAR);
    
                    long[] next = findNextEclipseInYear(year, false);

                    if (next != null) {
                        saveToCache(context, next);
                        callback.onResult(next);
                        return;
                    }
    
                    callback.onError("");
    
                } catch (Exception e) {
    
                    long cached = getCachedEclipse(context);
    
                    if (cached > 0)
                        callback.onResult(new long[]{cached});
                    else
                        callback.onError(e.getMessage());
                }
            }
        }).start();
    }
    
    private static long[] findNextEclipseInYear(int year, boolean z) throws Exception {
    
        URL url = new URL((z ? API_URL : API_URL2) + year);
    
        HttpURLConnection conn = (HttpURLConnection) url.openConnection();
        conn.setRequestMethod("GET");
        conn.setConnectTimeout(10000);
        conn.setReadTimeout(10000);
    
        BufferedReader reader =
                new BufferedReader(new InputStreamReader(conn.getInputStream()));
    
        StringBuilder json = new StringBuilder();
        String line;
    
        while ((line = reader.readLine()) != null) {
            json.append(line);
        }
    
        reader.close();
    
        JSONObject root = new JSONObject(json.toString());
    
        JSONArray eclipses = z ? root
                .getJSONObject("response")
                .getJSONArray("lunareclipse") : 
                                    root
                .getJSONObject("response")
                .getJSONArray("data");
    
        SimpleDateFormat sdf =
                new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss", Locale.US);
        sdf.setTimeZone(TimeZone.getTimeZone("UTC"));
    
        long now = System.currentTimeMillis();
    
        for (int i = 0; i < eclipses.length(); i++) {
    
            JSONObject eclipse = eclipses.getJSONObject(i);
            JSONObject events = eclipse.getJSONObject("events");
    
            long startMillis = sdf.parse(events.getJSONObject("P1").getString("date")).getTime();
            long endMillis = sdf.parse(events.getJSONObject(z ? "P2" : "P4").getString("date")).getTime();
    
            // Eclipse em andamento
            if (now >= startMillis && now < endMillis) {
                return new long[]{startMillis, endMillis};
            }
    
            // Próximo eclipse
            if (startMillis > now) {
                return new long[]{startMillis, endMillis};
            }
        }
    
        return null;
    }

    private static long parseNextEclipseDate(String json) throws Exception {
    
        JSONObject root = new JSONObject(json);
    
        JSONArray eclipses = root
                .getJSONObject("response")
                .getJSONArray("lunareclipse");
    
        if (eclipses.length() == 0)
            throw new Exception("Nenhum eclipse encontrado.");
    
        JSONObject eclipse = eclipses.getJSONObject(0);
    
        JSONObject events = eclipse.getJSONObject("events");
    
        String date = events.has("P1")
                ? events.getJSONObject("P1").getString("date")
                : events.getJSONObject("greatest").getString("date");
    
        SimpleDateFormat sdf =
                new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss", Locale.US);
    
        sdf.setTimeZone(TimeZone.getTimeZone("UTC"));
    
        return sdf.parse(date).getTime();
    }

    private static String extractJsonField(String json, String field) {
        // Parsing simples sem lib externa (org.json já vem no Android por padrão)
        try {
            org.json.JSONObject obj = new org.json.JSONObject(json);
            return obj.getString(field);
        } catch (Exception e) {
            return null;
        }
    }

    private static void saveToCache(Context context, long[] millis) {
        SharedPreferences.Editor editor = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE).edit();
        editor.putLong(KEY_NEXT_ECLIPSE_MILLIS, millis[0]);
        editor.putLong(KEY_LAST_CHECK, System.currentTimeMillis());
        editor.apply();
    }

    public static long getCachedEclipse(Context context) {
        return context.getSharedPreferences(PREFS, Context.MODE_PRIVATE).getLong(KEY_NEXT_ECLIPSE_MILLIS, -1);
    }
}