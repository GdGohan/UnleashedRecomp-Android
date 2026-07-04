#include <os/process.h>

#include <signal.h>
#include <unistd.h>
#include <limits.h>

std::filesystem::path os::process::GetExecutablePath()
{
    char exePath[PATH_MAX] = {};
    if (readlink("/proc/self/exe", exePath, PATH_MAX) > 0)
    {
        return std::filesystem::path(std::u8string_view((const char8_t*)(exePath)));
    }
    else
    {
        return std::filesystem::path();
    }
}

std::filesystem::path os::process::GetExecutableRoot()
{
    return GetExecutablePath().remove_filename();
}

std::filesystem::path os::process::GetWorkingDirectory()
{
    char cwd[PATH_MAX] = {};
    char *res = getcwd(cwd, sizeof(cwd));
    if (res != nullptr)
    {
        return std::filesystem::path(std::u8string_view((const char8_t*)(cwd)));
    }
    else
    {
        return std::filesystem::path();
    }
}

bool os::process::SetWorkingDirectory(const std::filesystem::path& path)
{
    return chdir(path.c_str()) == 0;
}

bool os::process::StartProcess(const std::filesystem::path& path, const std::vector<std::string>& args, std::filesystem::path work)
{
    // Android's app sandbox generally does not permit fork()/exec() of arbitrary
    // binaries; this is a best-effort implementation kept for source parity with
    // Linux and is not expected to be exercised in the Android build.
    pid_t pid = fork();
    if (pid < 0)
        return false;

    if (pid == 0)
    {
        setsid();

        std::u8string workU8 = work.u8string();
        chdir((const char*)(workU8.c_str()));

        std::u8string pathU8 = path.u8string();
        std::vector<char*> argStrs;
        argStrs.push_back((char*)(pathU8.c_str()));
        for (const std::string& arg : args)
            argStrs.push_back((char *)(arg.c_str()));

        argStrs.push_back(nullptr);
        execvp((const char*)(pathU8.c_str()), argStrs.data());
        raise(SIGKILL);
    }

    return true;
}

void os::process::CheckConsole()
{
    // No console on Android; all logging goes through logcat (see logger_android.cpp).
    g_consoleVisible = false;
}

void os::process::ShowConsole()
{
    // Unsupported on Android.
}
