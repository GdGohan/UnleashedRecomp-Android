#!/usr/bin/env python3

import xml.etree.ElementTree as ET
from pathlib import Path
import re

import shutil
import tempfile
import zipfile


SU_ZIP = Path(__file__).resolve().parent / "su.zip"

PROJECT_RES = ROOT / "android-apk/app/src/main/res"
PROJECT_JAVA = ROOT / "android-apk/app/src/main/java"


def copy_tree(src: Path, dst: Path):
    if not src.exists():
        return

    for item in src.rglob("*"):
        rel = item.relative_to(src)
        target = dst / rel

        if item.is_dir():
            target.mkdir(parents=True, exist_ok=True)
        else:
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(item, target)


def extract_resources():
    if not SU_ZIP.exists():
        print("su.zip não encontrado.")
        return

    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)

        with zipfile.ZipFile(SU_ZIP, "r") as z:
            z.extractall(tmp)

        res = tmp / "res"
        java = tmp / "java"

        print("Copiando res...")
        copy_tree(res, PROJECT_RES)

        print("Copiando java...")
        copy_tree(java, PROJECT_JAVA)

        print("Arquivos copiados.")
        
        
ROOT = Path(__file__).resolve().parent.parent

MANIFEST = ROOT / "android-apk/app/src/main/AndroidManifest.xml"

# Ajuste se o caminho for diferente
LAUNCHER = ROOT / "android-apk/app/src/main/java/org/libsdl/app/LauncherActivity.java"


ALIASES = r'''
        <activity-alias
            android:name=".DayIcon"
            android:enabled="true"
            android:exported="true"
            android:icon="@mipmap/ic_launcher"
            android:targetActivity="org.libsdl.app.LauncherActivity">
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
        </activity-alias>
        
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
            m = re.search(r"<manifest[^>]*>", text, re.S)

            if m:
                manifest_tag = m.group(0)
            
                permissions = "\n".join(
                    p.strip()
                    for p in PERMISSIONS.strip().splitlines()
                    if p.strip() not in text
                )
            
                if permissions:
                    text = text.replace(
                        manifest_tag,
                        manifest_tag + "\n\n" + permissions,
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
    if ".DayIcon" not in text:
        text = text.replace(
            "</application>",
            ALIASES + "\n\n</application>"
        )
        
    text = fix_manifest_classes(text)

    try:
        ET.fromstring(text)
    except ET.ParseError as e:
        print("Manifest inválido:", e)
        raise
    
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


PACKAGE = "com.sega.sonicunr"

PACKAGE2 = "org.libsdl.app"

GRADLE_FILES = [
    ROOT / "android-apk/app/build.gradle",
    ROOT / "android-apk/app/build.gradle.kts",
]


def patch_package_name():

    # Corrige Gradle
    for gradle in GRADLE_FILES:
        if gradle.exists():
            text = gradle.read_text()

            # text = re.sub(
            #    r'(namespace\s*=\s*")[^"]+(")',
            #    rf'\g<1>{PACKAGE}\g<2>',
            #    text
            # )

            text = re.sub(
                r'(applicationId\s*=\s*")[^"]+(")',
                rf'\g<1>{PACKAGE}\g<2>',
                text
            )

            gradle.write_text(text)


def fix_manifest_classes(text):

    COMPONENTS = (
        "activity",
        "activity-alias",
        "service",
        "receiver",
        "provider",
    )

    def fix_component(match):

        tag = match.group(1)
        body = match.group(2)

        def fix_name(name_match):

            value = name_match.group(1)

            # já correto
            # if value.startswith(PACKAGE2 + "."):
            #    return f'android:name="{value}"'

            # classe relativa: .MinhaActivity
            # if value.startswith("."):
            #    return f'android:name="{PACKAGE2}{value}"'

            # classes AndroidX, SDL, bibliotecas etc. ficam intactas
            # if value.startswith("android.") or value.startswith("androidx."):
            #    return f'android:name="{value}"'

            # if value.startswith("org.libsdl."):
            #    return f'android:name="{value}"'

            # qualquer classe sem pacote
            # if "." not in value:
            #    return f'android:name="{PACKAGE2}.{value}"'

            # outro pacote: deixa intacto
            return f'android:name="{value}"'


        body = re.sub(
            r'android:name="([^"]+)"',
            fix_name,
            body
        )

        return f"<{tag}{body}>"

    pattern = (
        r"<("
        + "|".join(COMPONENTS)
        + r")\b([^>]*)>"
    )

    return re.sub(
        pattern,
        fix_component,
        text
    )
    
    
if __name__ == "__main__":

    print("Aplicando patch Android...")
    
    extract_resources()
    
    patch_package_name()
    print("Pacote Gradle atualizado")

    patch_manifest()
    print("Manifest atualizado")

    patch_launcher()
    print("LauncherActivity atualizado")

    print("Concluído")