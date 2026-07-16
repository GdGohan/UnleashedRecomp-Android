# CI: building and publishing the Android APK

The workflow at `.github/workflows/android.yml` validates the Android Java code
on pull requests and builds the complete APK on every `v*` tag push or manual
dispatch. Full builds run on `ubuntu-24.04` and require the private game inputs
described below.

## Required private game inputs

The native build needs `default.xex`, `default.xexp` and `shader.ar`. These files
must never be committed to this public repository. Keep them at the root of a
private repository and configure:

| Type | Name | Value |
| --- | --- | --- |
| Repository variable | `GAME_FILES_REPO` | Private repository in `owner/name` form |
| Repository secret | `GAME_FILES_TOKEN` | Fine-grained token with read-only Contents access to that repository |

Private storage limits distribution but does not change the legal status of
game-derived data. Keep access to it minimal.

## Release signing

The workflow always runs `assembleRelease`; it never publishes a debug APK. A
manual build with publish mode `none` may produce an unsigned release artifact.
Publishing requires every secret below and fails closed if one is absent:

| Secret | Value |
| --- | --- |
| `ANDROID_KEYSTORE_BASE64` | Keystore file encoded with `base64 -w0 keystore.jks` |
| `ANDROID_KEYSTORE_PASSWORD` | Keystore password |
| `ANDROID_KEY_ALIAS` | Key alias |
| `ANDROID_KEY_PASSWORD` | Key password |

Android only updates an existing install when the signing key stays the same.
The workflow verifies the certificate fingerprint against the key used for the
published GdGohan `v0.5.2-2` APK before it creates a release. If the key is lost,
existing installations cannot be upgraded in place.

## Versioning and publishing

`versionCode` and `versionName` in `android-apk/app/build.gradle` are the source
of truth. Increase `versionCode` for every public APK. The release tag must be
`v` followed by the exact `versionName` (for example, `v0.5.2-3`). A mismatched
tag fails before the native build begins.

Manual dispatch defaults to publish mode `none`. Select `release` or
`prerelease` only after committing the intended version bump. The release job
downloads the already-verified artifact, creates a draft, uploads the APK, and
only then makes the release visible.

## What the workflow does

1. Pull requests run Java unit tests and Android Lint without private inputs.
2. Tag/manual builds check out the source and private game-files repository.
3. Host code-generation tools are built natively for the runner.
4. vcpkg and NDK r29 cross-compile `libmain.so` and the AdrenoTools hook
   libraries for `arm64-v8a`/Android 29.
5. Gradle runs `assembleRelease`.
6. The workflow checks package name, `versionCode`, `versionName`, `libmain.so`,
   the absence of `debuggable`, the APK signature, and the established signing
   certificate fingerprint.
7. The verified APK is uploaded as an artifact. When publishing is enabled, a
   separate write-scoped job creates a draft release, uploads the APK, and makes
   it visible only after the upload succeeds.

Both CMake passes use ccache and the vcpkg binary cache. The first run is slow
because the generated `ppc_recomp.*.cpp` translation units dominate; repeat
runs are substantially faster.

## Secrets and fork pull requests

GitHub does not expose secrets to workflows triggered by pull requests from
forks. Pull requests therefore run only Java tests and Lint; the native build is
skipped. Private game files and signing secrets are used only by tag/manual full
builds, keeping `GAME_FILES_TOKEN` unavailable to untrusted pull-request code.
