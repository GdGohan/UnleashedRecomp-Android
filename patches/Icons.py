#!/usr/bin/env python3

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parent.parent

MANIFEST = ROOT / "android-apk/app/src/main/AndroidManifest.xml"

# Ajuste se o caminho for diferente
LAUNCHER = ROOT / "android-apk/app/src/main/java/org/libsdl/app/LauncherActivity.java"


ALIASES = r'''
        <activity-alias
            android:name=".NightIcon"
            android:enabled="false"
            android:exported="true"
            android:icon="@mipmap/ic_launcher2"
            android:targetActivity="org.libsdl.app.LauncherActivity">
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
        </activity-alias>

        <activity-alias
            android:icon="@mipmap/ic_launcher3"
            android:name=".EclipseIcon"
            android:enabled="false"
            android:exported="true"
            android:targetActivity="org.libsdl.app.LauncherActivity">
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
        </activity-alias>

        <activity-alias
            android:icon="@mipmap/ic_launcher4"
            android:name=".EclipseIcon2"
            android:enabled="false"
            android:exported="true"
            android:targetActivity="org.libsdl.app.LauncherActivity">
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
        </activity-alias>

        <activity-alias
            android:icon="@mipmap/ic_launcher_hw"
            android:name=".HwDayIcon"
            android:enabled="false"
            android:exported="true"
            android:targetActivity="org.libsdl.app.LauncherActivity">
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
        </activity-alias>

        <activity-alias
            android:icon="@mipmap/ic_launcher_hw2"
            android:name=".HwNightIcon"
            android:enabled="false"
            android:exported="true"
            android:targetActivity="org.libsdl.app.LauncherActivity">
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
        </activity-alias>

        <receiver
            android:name=".BootReceiver"
            android:enabled="true"
            android:exported="true">
            <intent-filter>
                <action android:name="android.intent.action.BOOT_COMPLETED"/>
                <action android:name="android.intent.action.MY_PACKAGE_REPLACED"/>
            </intent-filter>
        </receiver>

        <receiver
            android:name=".AlarmReceiver"
            android:enabled="true"
            android:exported="false"/>
'''


PERMISSIONS = """
    <uses-permission android:name="android.permission.RECEIVE_BOOT_COMPLETED"/>
    <uses-permission android:name="android.permission.POST_NOTIFICATIONS"/>
    <uses-permission android:name="android.permission.SCHEDULE_EXACT_ALARM"/>
    <uses-permission android:name="android.permission.ACCESS_COARSE_LOCATION"/>
    <uses-permission android:name="android.permission.ACCESS_NETWORK_STATE"/>
"""


JAVA_METHODS = r'''

    private void checkNotificationPermission() {
        if (Build.VERSION.SDK_INT >= 33) {
            if (checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS)
                    != PackageManager.PERMISSION_GRANTED) {
                requestPermissions(
                        new String[]{Manifest.permission.POST_NOTIFICATIONS},
                        2001
                );
            }
        }
    }

    private void checkExactAlarmPermission() {
        if (Build.VERSION.SDK_INT >= 31) {
            AlarmManager alarmManager =
                    (AlarmManager) getSystemService(Context.ALARM_SERVICE);

            if (alarmManager != null &&
                    !alarmManager.canScheduleExactAlarms()) {

                Intent intent = new Intent(
                        android.provider.Settings.ACTION_REQUEST_SCHEDULE_EXACT_ALARM);

                intent.setData(
                        android.net.Uri.parse("package:" + getPackageName()));

                startActivity(intent);
            }
        }
    }

    private void checkExactLocPermission() {
        if (checkSelfPermission(
                Manifest.permission.ACCESS_COARSE_LOCATION)
                != PackageManager.PERMISSION_GRANTED) {

            requestPermissions(
                    new String[]{
                            Manifest.permission.ACCESS_COARSE_LOCATION
                    },
                    3001
            );
        }
    }

    @Override
    public void onRequestPermissionsResult(
            int requestCode,
            String[] permissions,
            int[] grantResults) {

        super.onRequestPermissionsResult(
                requestCode,
                permissions,
                grantResults);
    }
'''


ONCREATE_CALLS = """
        checkNotificationPermission();
        checkExactAlarmPermission();
        checkExactLocPermission();

        com.sega.sonicunr.LocationHelper.updateLastKnownLocation(this);

        com.sega.sonicunr.AlarmScheduler.locAdjust(this);
        com.sega.sonicunr.AlarmScheduler.applyCurrentIconState(this);
        com.sega.sonicunr.AlarmScheduler.scheduleAll(this);
        com.sega.sonicunr.EclipseScheduler.scheduleNextEclipse(this);
"""


def patch_manifest():

    text = MANIFEST.read_text()

    # permissões
    for p in PERMISSIONS.strip().split("\n"):
        if p.strip() not in text:
            text = text.replace(
                "<manifest",
                "<manifest\n" + p,
                1
            )

    # remove launcher intent original
    text = re.sub(
        r'<intent-filter>\s*'
        r'<action android:name="android.intent.action.MAIN".*?'
        r'<category android:name="android.intent.category.LAUNCHER".*?'
        r'</intent-filter>',
        '',
        text,
        flags=re.S
    )

    # aliases
    if ".NightIcon" not in text:
        text = text.replace(
            "</application>",
            ALIASES + "\n\n</application>"
        )

    MANIFEST.write_text(text)



def patch_launcher():

    text = LAUNCHER.read_text()

    if "checkNotificationPermission()" not in text:

        # adiciona chamadas no onCreate
        text = text.replace(
            "super.onCreate(savedInstanceState);",
            "super.onCreate(savedInstanceState);\n" +
            ONCREATE_CALLS,
            1
        )

        # adiciona métodos antes do último }
        pos = text.rfind("}")

        text = (
            text[:pos] +
            JAVA_METHODS +
            "\n" +
            text[pos:]
        )

    LAUNCHER.write_text(text)



if __name__ == "__main__":

    print("Aplicando patch Android...")

    patch_manifest()
    print("Manifest atualizado")

    patch_launcher()
    print("LauncherActivity atualizado")

    print("Concluído")