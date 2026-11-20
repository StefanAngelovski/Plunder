#include "include/Process.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace Process {

int Run(const std::vector<std::string>& args, std::string* out, bool quiet) {
    if (args.empty()) return -1;
    // Build argv before spawning (posix_spawn is safe in multithreaded programs, unlike fork+malloc).
    std::vector<char*> argv;
    for (const auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);

    int pipefd[2] = {-1, -1};
    if (out && pipe(pipefd) != 0) return -1;

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    if (out) {
        posix_spawn_file_actions_adddup2(&actions, pipefd[1], STDOUT_FILENO);
        posix_spawn_file_actions_addclose(&actions, pipefd[0]);
        posix_spawn_file_actions_addclose(&actions, pipefd[1]);
    } else if (quiet) {
        posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
    }
    if (quiet) posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);

    pid_t pid = -1;
    int rc = posix_spawnp(&pid, argv[0], &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    if (out) close(pipefd[1]);
    if (rc != 0) {
        if (out) close(pipefd[0]);
        return -1;
    }
    if (out) {
        char buf[16384];
        ssize_t n;
        while ((n = read(pipefd[0], buf, sizeof buf)) > 0) out->append(buf, (size_t)n);
        close(pipefd[0]);
    }
    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) return -1;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

bool Exists(const std::string& program) {
    if (program.find('/') != std::string::npos) return access(program.c_str(), X_OK) == 0;
    const char* path = std::getenv("PATH");
    std::string paths = path ? path : "/usr/local/bin:/usr/bin:/bin";
    size_t start = 0;
    while (start <= paths.size()) {
        size_t end = paths.find(':', start);
        std::string dir = paths.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (!dir.empty() && access((dir + "/" + program).c_str(), X_OK) == 0) return true;
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return false;
}

}  // namespace Process
