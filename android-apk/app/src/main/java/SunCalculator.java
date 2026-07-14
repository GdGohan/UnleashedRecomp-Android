package com.sega.sonicunr;

import java.util.Calendar;
import java.util.TimeZone;

public class SunCalculator {

    // Retorna [horaNascer, minutoNascer, horaPor, minutoPor] em horário local
    public static int[] calculate(double latitude, double longitude, Calendar date) {
        int dayOfYear = date.get(Calendar.DAY_OF_YEAR);
        double zenith = 90.833; // ângulo oficial de nascer/pôr do sol

        double sunrise = calcTime(dayOfYear, longitude, latitude, zenith, true, date);
        double sunset  = calcTime(dayOfYear, longitude, latitude, zenith, false, date);

        return new int[]{
            (int) sunrise, (int) ((sunrise - (int) sunrise) * 60),
            (int) sunset,  (int) ((sunset - (int) sunset) * 60)
        };
    }

    private static double calcTime(int dayOfYear, double lon, double lat, double zenith, boolean isSunrise, Calendar date) {
        double lngHour = lon / 15.0;
        double t = isSunrise ? dayOfYear + ((6 - lngHour) / 24.0) : dayOfYear + ((18 - lngHour) / 24.0);

        double M = (0.9856 * t) - 3.289;
        double L = M + (1.916 * Math.sin(Math.toRadians(M))) + (0.020 * Math.sin(Math.toRadians(2 * M))) + 282.634;
        L = normalize(L, 360);

        double RA = Math.toDegrees(Math.atan(0.91764 * Math.tan(Math.toRadians(L))));
        RA = normalize(RA, 360);
        double Lquadrant = (Math.floor(L / 90)) * 90;
        double RAquadrant = (Math.floor(RA / 90)) * 90;
        RA = RA + (Lquadrant - RAquadrant);
        RA = RA / 15;

        double sinDec = 0.39782 * Math.sin(Math.toRadians(L));
        double cosDec = Math.cos(Math.asin(sinDec));

        double cosH = (Math.cos(Math.toRadians(zenith)) - (sinDec * Math.sin(Math.toRadians(lat))))
                / (cosDec * Math.cos(Math.toRadians(lat)));

        double H = isSunrise ? 360 - Math.toDegrees(Math.acos(cosH)) : Math.toDegrees(Math.acos(cosH));
        H = H / 15;

        double T = H + RA - (0.06571 * t) - 6.622;
        double UT = normalize(T - lngHour, 24);

        TimeZone tz = TimeZone.getDefault();
        double offsetHours = tz.getRawOffset() / 3600000.0;
        return normalize(UT + offsetHours, 24);
    }

    private static double normalize(double value, double max) {
        while (value < 0) value += max;
        while (value >= max) value -= max;
        return value;
    }
}