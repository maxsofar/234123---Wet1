#include <unistd.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <signal.h>
#include "Commands.h"
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>
#include <algorithm>
#include <dirent.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <vector>

using namespace std;

const string WHITESPACE = " \n\r\t\f\v";

const set<std::string> SmallShell::RESERVED_KEYWORDS =
        {"chprompt", "showpid", "pwd", "cd", "jobs", "fg", "quit", "kill", "alias", "unalias", "listdir", ">", ">>", "getuser", "|", "watch"};

#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

string _ltrim(const string &s) {
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == string::npos) ? "" : s.substr(start);
}

string _rtrim(const string &s) {
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const string &s) {
    return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char *cmd_line, char **args) {
    FUNC_ENTRY()
    int i = 0;
    istringstream iss(_trim(string(cmd_line)));
    for (string s; iss >> s;) {
        args[i] = (char *) malloc(s.length() + 1);
        memset(args[i], 0, s.length() + 1);
        strcpy(args[i], s.c_str());
        args[++i] = NULL;
    }
    return i;

    FUNC_EXIT()
}

bool _isBackgroundComamnd(const char *cmd_line) {
    const string str(cmd_line);
    return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char *cmd_line) {
    const string str(cmd_line);
    // find last character other than spaces
    unsigned int idx = str.find_last_not_of(WHITESPACE);
    // if all characters are spaces then return
    if (idx == string::npos) {
        return;
    }
    // if the command line does not end with & then return
    if (cmd_line[idx] != '&') {
        return;
    }
    // replace the & (background sign) with space and then remove all tailing spaces.
    cmd_line[idx] = ' ';
    // truncate the command line string up to the last non-space character
    cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

bool isBackgroundCommand(const std::string& cmd_line) {
    std::string str = _trim(cmd_line);
    return !str.empty() && str.back() == '&';
}

void freeArgs(char** args, int num_args) {
    for (int i = 0; i < num_args; i++) {
        if (args[i]) {
            free(args[i]);
        }
    }
}


//-------------------------------------- Command --------------------------------------

Command::Command(const string& cmd_line) : cmd_line(cmd_line) {
    // Constructor implementation here
}

Command::~Command() {
    // Destructor implementation here
}

const string& Command::getCmdLine() const
{
    return cmd_line;
}


//---------------------------------- Built in commands ----------------------------------

BuiltInCommand::BuiltInCommand(const string& cmd_line) : Command(cmd_line)
{}

ChpromptCommand::ChpromptCommand(const string& cmd_line) : BuiltInCommand(cmd_line)
{}
void ChpromptCommand::execute() {
    SmallShell& shell = SmallShell::getInstance(); // Get the existing instance (singleton)
    char* args[COMMAND_MAX_LENGTH];
    int num_args = _parseCommandLine(cmd_line.c_str(), args);
    string newPrompt = args[1] ? args[1] : "smash";
    shell.setPrompt(newPrompt); // Change the prompt of the existing instance

    freeArgs(args, num_args);
}


ShowPidCommand::ShowPidCommand(const string& cmd_line) : BuiltInCommand(cmd_line)
{}
void ShowPidCommand::execute() {
    pid_t pid = getpid(); // Get the current process ID

    if (pid == -1) {
        // Error handling for failed getpid
        perror("smash error: getpid failed");
        exit(1); // Exit with an error code (optional)
    } else {
        cout << "smash pid is " << pid << endl;
    }
}


GetCurrDirCommand::GetCurrDirCommand(const string& cmd_line) : BuiltInCommand(cmd_line)
{}
void GetCurrDirCommand::execute() {
    char buf[PATH_MAX];
    if (getcwd(buf, PATH_MAX) == nullptr) {
        perror("smash error: getcwd failed");
    } else {
        cout << buf << endl;
    }
}


ChangeDirCommand::ChangeDirCommand(const string& cmd_line, char **plastPwd) : BuiltInCommand(cmd_line), lastPwd(plastPwd)
{}
void ChangeDirCommand::execute() {
    char* args[COMMAND_MAX_ARGS];
    int num_args = _parseCommandLine(cmd_line.c_str(), args);

    // Check if the number of arguments is valid
    if (num_args > 2) {
        cerr << "smash error: cd: too many arguments" << endl;
        freeArgs(args, num_args);
        return;
    }

    char* path = args[1];
    // Obtain the current working directory
    char* currPwd = getcwd(nullptr, 0);

    // If no path was provided, change the lastPwd to the current directory and return
    if (path == nullptr) {
        free(*lastPwd);
        *lastPwd = currPwd;
        freeArgs(args, num_args);
        return;
    }

    // If the path is "-", change to the previous working directory
    if (strcmp(path, "-") == 0) {
        if (*lastPwd == nullptr) {
            // If OLDPWD is not set, print an error message
            cerr << "smash error: cd: OLDPWD not set" << endl;
            freeArgs(args, num_args);
            free(currPwd);
            return;
        }
        path = *lastPwd;
    }

    if (currPwd == nullptr) {
        perror("smash error: getcwd failed");
        freeArgs(args, num_args);
        free(currPwd);
        return;
    }

    // Change the working directory
    if (chdir(path) == -1) {
        perror("smash error: chdir failed");
        freeArgs(args, num_args);
        free(currPwd);
        return;
    }

    free(*lastPwd);
    *lastPwd = currPwd;

    freeArgs(args, num_args);
}


JobsCommand::JobsCommand(const string& cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs(jobs)
{}
void JobsCommand::execute() {
    jobs->removeFinishedJobs();
    jobs->printJobsList();
}


ForegroundCommand::ForegroundCommand(const std::string& cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), jobs(jobs)
{}
void ForegroundCommand::execute() {
    char* args[COMMAND_MAX_ARGS];
    int numArgs = _parseCommandLine(cmd_line.c_str(), args);

    // Check if the number of arguments is valid
    if (numArgs > 2) {
        cerr << "smash error: fg: invalid arguments" << endl;
        freeArgs(args, numArgs);
        return;
    }

    int jobId = -1;
    if (args[1] != nullptr) {
        // If there is a second argument, it should be the job id
        string arg = args[1];
        if (all_of(arg.begin(), arg.end(), ::isdigit)) {
            jobId = stoi(arg);
            if (jobId <= 0) {
                cerr << "smash error: fg: invalid arguments" << endl;
                freeArgs(args, numArgs);
                return;
            }
        } else {
            cerr << "smash error: fg: invalid arguments" << endl;
            freeArgs(args, numArgs);
            return;
        }
    }

    JobsList::JobEntry *job;
    if (jobId == -1) {
        // If no job id was specified, get the job with the maximal job id
        int lastJobId;
        job = jobs->getLastJob(&lastJobId);
        if (job == nullptr) {
            cerr << "smash error: fg: jobs list is empty" << endl;
            freeArgs(args, numArgs);
            return;
        }
        jobId = lastJobId;
    } else {
        // If a job id was specified, get the job by its id
        job = jobs->getJobById(jobId);
        if (job == nullptr) {
            cerr << "smash error: fg: job-id " << jobId << " does not exist" << endl;
            freeArgs(args, numArgs);
            return;
        }
    }

    // Print the command line of the job along with its PID
    cout << job->getCmdLine() << "& " << job->getPid() << endl;

    // Update the PID of the foreground process
    SmallShell::getInstance().setFgPid(job->getPid());

    // Bring the process to the foreground by waiting for it
    if (waitpid(job->getPid(), nullptr, WCONTINUED | WUNTRACED) == -1) {
        perror("smash error: waitpid failed");
    }

    // Remove the job from the jobs list
    jobs->removeJobById(jobId);

    freeArgs(args, numArgs);
}


QuitCommand::QuitCommand(const string& cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs(jobs)
{}
void QuitCommand::execute() {
    char* args[COMMAND_MAX_ARGS];
    int numArgs = _parseCommandLine(cmd_line.c_str(), args);
    if (numArgs > 1 && strcmp(args[1], "kill") == 0) {
        SmallShell::getInstance().getJobs().killAllJobs();
    }
    freeArgs(args, numArgs);
    throw QuitException();
}


KillCommand::KillCommand(const string& cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs(jobs)
{}
void KillCommand::execute()
{
    char *args[COMMAND_MAX_ARGS];
    int num_args = _parseCommandLine(cmd_line.c_str(), args);

    if (num_args != 3 || args[1][0] != '-') {
        cerr << "smash error: kill: invalid arguments" << endl;
        freeArgs(args, num_args);
        return;
    }

    int signum;
    int jobId;
    try {
        signum = std::stoi(args[1] + 1); // skip the '-'
        jobId = std::stoi(args[2]);
    } catch (std::invalid_argument& e) {
        cerr << "smash error: kill: invalid arguments" << endl;
        freeArgs(args, num_args);
        return;
    }

    // Check if signum and jobId are positive
    if (signum <= 0 || jobId <= 0) {
        cerr << "smash error: kill: invalid arguments" << endl;
        freeArgs(args, num_args);
        return;
    }

    JobsList::JobEntry *job = jobs->getJobById(jobId);
    if (!job) {
        cerr << "smash error: kill: job-id " << jobId << " does not exist" << endl;
        freeArgs(args, num_args);
        return;
    }

    if (kill(job->getPid(), signum) == -1) {
        perror("smash error: kill failed");
        freeArgs(args, num_args);
        return;
    }

    cout << "signal number " << signum << " was sent to pid " << job->getPid() << endl;
    jobs->removeJobById(jobId);

    freeArgs(args, num_args);
}


aliasCommand::aliasCommand(const string& cmd_line) : BuiltInCommand(cmd_line)
{}
void aliasCommand::execute()
{
    SmallShell& smash = SmallShell::getInstance();

    // Remove the 'alias' part from the command line
    size_t pos = cmd_line.find("alias");
    if (pos != string::npos) {
        cmd_line.erase(0, pos + 5); // 5 is the length of 'alias'
    }

    // Trim the command line
    cmd_line = _trim(cmd_line);

    // If the command line is empty after removing 'alias', list all aliases
    if (cmd_line.empty()) {
        for (const auto& alias : smash.getAliases()) {
            cout << alias.first << "='" << alias.second << "'" << endl;
        }
        return;
    }

    // Check if there's an equal sign after the 'alias' keyword
    if (cmd_line.find('=') == string::npos) {
        cerr << "smash error: alias: Invalid command format" << endl;
        return;
    }

    string name = _trim(cmd_line.substr(0, cmd_line.find('=')));
    string command = _trim(cmd_line.substr(cmd_line.find('=') + 1));

    // Check if the command is surrounded by single quotes
    if (command.front() == '\'' && command.back() == '\'') {
        // Remove the single quotes from the command
        command = command.substr(1, command.length() - 2);
    } else {
        cerr << "smash error: alias: Invalid command format" << endl;
        return;
    }

    // Check if the name is a reserved keyword or already exists as an alias
    if (smash.isAlias(name) || SmallShell::RESERVED_KEYWORDS.find(name) != SmallShell::RESERVED_KEYWORDS.end()) {
        cerr << "smash error: alias: " << name << " already exists or is a reserved command" << endl;
    } else if (!isValidAlias(name, command)) {
        cerr << "smash error: alias: Invalid alias format" << endl;
    } else {
        smash.addAlias(name, command);
    }
}
bool aliasCommand::isValidAlias(const string& name, const string& command) {
    // Check if the name is empty
    if (name.empty()) {
        return false;
    }

    // Check if the command being aliased is one of the 3 commands '>', '>>', '|'
    if (command == ">" || command == ">>" || command == "|") {
        return false;
    }

    // Check if the name contains only valid characters
    for (char c : name) {
        if (!isalnum(c) && c != '_') {
            return false;
        }
    }
    return true;
}
const map<string, string>& SmallShell::getAliases() const
{
    return aliases;
}


unaliasCommand::unaliasCommand(const string& cmd_line) : BuiltInCommand(cmd_line)
{}
void unaliasCommand::execute()
{
    SmallShell& smash = SmallShell::getInstance();
    string cmd_str = _trim(cmd_line); // Remove leading and trailing whitespaces

    char* args[COMMAND_MAX_ARGS];
    int num_args = _parseCommandLine(cmd_str.c_str(), args); // Parse the command line

    if (num_args <= 1) { // No arguments provided
        cerr << "smash error: alias: Not enough arguments" << endl;
        freeArgs(args, num_args);
        return;
    }

    string name(args[1]);
    if (!smash.isAlias(name)) { // Alias does not exist
        cerr << "smash error: alias: " << name << " alias does not exist" << endl;
    } else {
        smash.removeAlias(name); // Remove the alias
    }

    freeArgs(args, num_args);
}

//---------------------------------- Special Commands ----------------------------------

RedirectionCommand::RedirectionCommand(const std::string& cmd_line): Command(cmd_line) {}
void RedirectionCommand::execute()
{
    // Split the command line into command and file
    std::string cmd_s = getCmdLine();
    size_t pos = cmd_s.find('>');
    std::string command = cmd_s.substr(0, pos);
    std::string file = cmd_s.substr(pos + 1);

    // Trim the file string
    file = _trim(file);

    // Determine the redirection mode
    int mode = (cmd_s[pos + 1] == '>') ? O_APPEND : O_TRUNC;

    // If append mode, remove the extra '>' from the file string
    if (mode == O_APPEND) {
        file = _trim(file.substr(1));
    }

    // Open the file
    int fd = open(file.c_str(), O_WRONLY | O_CREAT | mode, 0666);
    if (fd == -1) {
        perror("smash error: open failed");
        return;
    }

    // Save the original stdout
    int stdout_copy = dup(STDOUT_FILENO);

    // Redirect stdout to the file
    dup2(fd, STDOUT_FILENO);
    close(fd);

    // Execute the command
    SmallShell::getInstance().executeCommand(command);

    // Reset stdout
    dup2(stdout_copy, STDOUT_FILENO);
    close(stdout_copy);
}

ListDirCommand::ListDirCommand(const string& cmd_line) : BuiltInCommand(cmd_line)
{}

struct linux_dirent {
    long d_ino;
    off_t d_off;
    unsigned short d_reclen;
    char d_name[];
};

void ListDirCommand::execute() {
    char* args[COMMAND_MAX_ARGS];
    int num_args = _parseCommandLine(cmd_line.c_str(), args);

    if (num_args > 2) {
        std::cerr << "smash error: listdir: Too many arguments" << std::endl;
        freeArgs(args, num_args);
        return;
    }

    std::string dir_path = (num_args == 1) ? "." : args[1];
    int fd = open(dir_path.c_str(), O_RDONLY | O_DIRECTORY);
    if (fd == -1) {
        perror("smash error: open failed");
        freeArgs(args, num_args);
        return;
    }

    char buffer[8192];
    struct linux_dirent* dirent;
    off_t basep = 0;
    ssize_t bytes_read;

    std::vector<std::string> files;
    std::vector<std::string> directories;

    while ((bytes_read = syscall(SYS_getdents, fd, buffer, sizeof(buffer), &basep)) > 0) {
        for (int i = 0; i < bytes_read; i += dirent->d_reclen) {
            dirent = (struct linux_dirent*)(&buffer[i]);

            if (dirent->d_name[0] == '.') {
                continue;
            }

            std::string full_path = dir_path + "/" + dirent->d_name;

            struct stat entry_stat;
            if (stat(full_path.c_str(), &entry_stat) == -1) {
                perror("smash error: stat failed");
                continue;
            }

            if (S_ISREG(entry_stat.st_mode)) {
                files.push_back(dirent->d_name);
            } else if (S_ISDIR(entry_stat.st_mode)) {
                directories.push_back(dirent->d_name);
            }
        }
    }

    close(fd);

    std::sort(files.begin(), files.end());
    for (const auto& file : files) {
        std::cout << "File: " << file << std::endl;
    }

    std::sort(directories.begin(), directories.end());
    for (const auto& directory : directories) {
        std::cout << "Directory: " << directory << std::endl;
    }

    freeArgs(args, num_args);
}

GetUserCommand::GetUserCommand(const std::string &cmd_line) : BuiltInCommand(cmd_line)
{}
void GetUserCommand::execute()
{
    char* args[COMMAND_MAX_ARGS];
    int num_args = _parseCommandLine(cmd_line.c_str(), args);

    if (num_args != 2) {
        cerr << "smash error: getuser: invalid number of arguments" << endl;
        freeArgs(args, num_args);
        return;
    }

    pid_t pid = atoi(args[1]);
    struct passwd *pw;
    struct group *gr;

    // Read the UID of the process from /proc/<PID>/status
    string status_file = "/proc/" + to_string(pid) + "/status";
    FILE* status = fopen(status_file.c_str(), "r");
    if (status == nullptr) {
        perror("smash error: fopen failed");
        freeArgs(args, num_args);
        return;
    }

    char line[256];
    uid_t uid = numeric_limits<uid_t>::max(); // Initialize to maximum value
    while (fgets(line, sizeof(line), status)) {
        if (strncmp(line, "Uid:", 4) == 0) {
            sscanf(line, "Uid:\t%d", &uid);
            break;
        }
    }
    fclose(status);

    if (uid == numeric_limits<uid_t>::max()) {
        cerr << "smash error: getuser: failed to get UID of process " << pid << endl;
        freeArgs(args, num_args);
        return;
    }

    errno = 0;
    if ((pw = getpwuid(uid)) == nullptr) {
        if (errno == 0) {
            cerr << "smash error: getuser: process " << pid << " does not exist" << endl;
        } else {
            perror("smash error: getpwuid failed");
        }
        freeArgs(args, num_args);
        return;
    }

    if ((gr = getgrgid(pw->pw_gid)) == nullptr) {
        perror("smash error: getgrgid failed");
        freeArgs(args, num_args);
        return;
    }

    cout << "User: " << pw->pw_name << endl; // Print username on a new line
    cout << "Group: " << gr->gr_name << endl; // Print group on a new line

    freeArgs(args, num_args);
}


PipeCommand::PipeCommand(const string& cmd_line) : Command(cmd_line)
{}
void PipeCommand::execute() {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("smash error: pipe failed");
        return;
    }

    std::string cmd_s = getCmdLine();
    size_t pos = cmd_s.find('|');
    std::string cmd1 = cmd_s.substr(0, pos);
    std::string cmd2 = cmd_s.substr(pos + 1);

    bool isStderr = cmd_s[pos + 1] == '&';

    char* args1[COMMAND_MAX_ARGS];
    int num_args1 = _parseCommandLine(cmd1.c_str(), args1);

    pid_t pid1 = fork();
    if (pid1 < 0) {
        perror("smash error: fork failed");
        freeArgs(args1, num_args1);
        return;
    }

    if (pid1 == 0) {
        close(pipefd[0]);
        if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
            perror("smash error: dup2 failed");
            exit(1);
        }
        if (isStderr && dup2(pipefd[1], STDERR_FILENO) == -1) {
            perror("smash error: dup2 failed");
            exit(1);
        }
        if (execvp(args1[0], args1) < 0) {
            perror("smash error: execvp failed");
            exit(1);
        }
    }

    char* args2[COMMAND_MAX_ARGS];
    int num_args2 = _parseCommandLine(cmd2.c_str(), args2);

    pid_t pid2 = fork();
    if (pid2 < 0) {
        perror("smash error: fork failed");
        freeArgs(args1, num_args1);
        freeArgs(args2, num_args2);
        return;
    }

    if (pid2 == 0) {
        close(pipefd[1]);
        if (dup2(pipefd[0], STDIN_FILENO) == -1) {
            perror("smash error: dup2 failed");
            exit(1);
        }
        if (execvp(args2[0], args2) < 0) {
            perror("smash error: execvp failed");
            exit(1);
        }
    }

    close(pipefd[0]);
    close(pipefd[1]);
    int status;
    waitpid(pid1, &status, 0);
    waitpid(pid2, &status, 0);

    freeArgs(args1, num_args1);
    freeArgs(args2, num_args2);
}

WatchCommand::WatchCommand(const string& cmd_line) : Command(cmd_line)
{}
void WatchCommand::execute()
{
    // Add a signal handler for SIGINT
    signal(SIGINT, [](int signum) {
        exit(0);
    });

    char* args[COMMAND_MAX_ARGS];
    int num_args = _parseCommandLine(cmd_line.c_str(), args);

    if (num_args < 3) {
        cerr << "smash error: watch: command not specified" << endl;
        freeArgs(args, num_args);
        return;
    }

    int interval;
    try {
        interval = std::stoi(args[1]);
    } catch (std::invalid_argument& e) {
        cerr << "smash error: watch: invalid interval" << endl;
        freeArgs(args, num_args);
        return;
    }

    if (interval <= 0) {
        cerr << "smash error: watch: invalid interval" << endl;
        freeArgs(args, num_args);
        return;
    }

    string command = args[2];

    freeArgs(args, num_args);

    while (true) {
        // Execute the command
        SmallShell::getInstance().executeCommand(command);

        // Sleep for the given interval
        sleep(interval);
    }
}

//---------------------------------- Job List ----------------------------------

JobsList::JobsList() : maxJobId(1)
{}

void JobsList::removeFinishedJobs() {
    for (auto it = jobs.begin(); it != jobs.end(); ) {
        if (it->isFinished()) {
            it = jobs.erase(it);
        } else {
            ++it;
        }
    }
}

void JobsList::printJobsList() {
    for (const auto& job : jobs) {
        cout << "[" << job.getJobId() << "] "
                  << job.getCmd()->getCmdLine() << "&"
                  << endl;
    }
}

void JobsList::addJob(shared_ptr<Command> cmd, pid_t pid) {
    //TODO: check if jobID should be reused

    // Remove finished jobs from the jobs list
    removeFinishedJobs();
    int jobId = jobs.empty() ? 1 : maxJobId + 1;
    jobs.push_back(JobEntry(jobId, cmd, pid));
    maxJobId = jobId;  // Update the maximum job ID
}

void JobsList::killAllJobs() {
    cout << "smash: sending SIGKILL signal to " << jobs.size() << " jobs:" << endl;
    for (auto &job : jobs) {
        if (!job.isFinished()) {
            cout << job.getPid() << ": " << job.getCmdLine() << "&" << endl;
            if(kill(job.getPid(), SIGKILL) == -1) {
                perror("smash error: kill failed");
            }
        }
    }
}

JobsList::JobEntry* JobsList::getJobById(int jobId) {
    for (auto& job : jobs) {
        if (job.getJobId() == jobId) {
            return &job;
        }
    }
    return nullptr;  // No job with the given id was found
}

void JobsList::removeJobById(int jobId)
{
    for (auto it = jobs.begin(); it != jobs.end(); ++it) {
        if (it->getJobId() == jobId) {
            jobs.erase(it);
            return;
        }
    }
}

JobsList::JobEntry *JobsList::getLastJob(int *lastJobId) {
    if (jobs.empty()) {
        // If the jobs list is empty, return nullptr
        return nullptr;
    }

    // Get the last job in the list
    JobEntry *lastJob = &jobs.back();

    // If the lastJobId pointer is not null, assign the job id to it
    if (lastJobId != nullptr) {
        *lastJobId = lastJob->getJobId();
    }

    return lastJob;
}


//---------------------------------- External Command ----------------------------------

ExternalCommand::ExternalCommand(const string& cmd_line) : Command(cmd_line)
{}
void ExternalCommand::execute() {
    // Parse the command line into arguments
    char* args[COMMAND_MAX_ARGS];
    int num_args = _parseCommandLine(cmd_line.c_str(), args);

    // Ensure args is null-terminated
    args[num_args] = nullptr;

    // Check if the command line contains any special characters
    if (cmd_line.find('*') != string::npos || cmd_line.find('?') != string::npos) {
        // This is a complex command, run it using bash
        const char* bash_args[4];
        bash_args[0] = "/bin/bash";
        bash_args[1] = "-c";
        bash_args[2] = cmd_line.c_str();
        bash_args[3] = nullptr;
        if (execvp(bash_args[0], const_cast<char* const*>(bash_args)) < 0) {
            perror("smash error: execvp failed");
            exit(1);
        }
    } else {
        // This is a simple command, run it directly
        if (execvp(args[0], args) < 0) {
            perror("smash error: execvp failed");
            exit(1);
        }
    }

    freeArgs(args, num_args);
}


//---------------------------------- Small Shell ----------------------------------

SmallShell::SmallShell(): lastPwd(nullptr), fgPid(-1) {
// TODO: add your implementation
}

SmallShell::~SmallShell() {
// TODO: add your implementation
    if (lastPwd != nullptr) {
        free(lastPwd);
    }
}

shared_ptr<Command> SmallShell::CreateCommand(const std::string& cmd_line)
{
    std::string cmd_s = _trim(cmd_line);
    std::string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));


    if (firstWord == "chprompt") {
        return make_shared<ChpromptCommand>(cmd_line);
    } else if (firstWord == "showpid") {
        return make_shared<ShowPidCommand>(cmd_line);
    } else if (firstWord == "pwd") {
        return make_shared<GetCurrDirCommand>(cmd_line);
    } else if (firstWord == "cd") {
        return make_shared<ChangeDirCommand>(cmd_line, &lastPwd);
    } else if (firstWord == "jobs") {
        return make_shared<JobsCommand>(cmd_line, &jobs);
    } else if (firstWord == "alias") {
        return make_shared<aliasCommand>(cmd_line);
    } else if (firstWord == "unalias") {
        return make_shared<unaliasCommand>(cmd_line);
    } else if (firstWord == "quit") {
        return make_shared<QuitCommand>(cmd_line, &jobs);
    } else if (firstWord == "kill") {
        return make_shared<KillCommand>(cmd_line, &jobs);
    } else if (firstWord == "fg") {
        return make_shared<ForegroundCommand>(cmd_line, &jobs);
    } else if (firstWord == "listdir") {
        return make_shared<ListDirCommand>(cmd_line);
    } else if (firstWord == "getuser") {
        return make_shared<GetUserCommand>(cmd_line);
    } else if (firstWord == "watch") {
        return make_shared<WatchCommand>(cmd_line);
    } else if (cmd_s.find('|') != std::string::npos) { // Check if the command line contains '|'
        return make_shared<PipeCommand>(cmd_line);
    } else if (cmd_s.find('>') != std::string::npos) { // Check if the command line contains '>'
        return make_shared<RedirectionCommand>(cmd_line);
    } else {
        // Check if the first word is an alias
        if (isAlias(firstWord)) {
            // Replace the alias with the corresponding command
            string aliasCommand = getAlias(firstWord);
            cmd_s = cmd_s.replace(0, firstWord.length(), aliasCommand);
        }

        return make_shared<ExternalCommand>(cmd_s);
    }
}

void SmallShell::executeExternalCommand(const shared_ptr<Command>& cmd, bool isBackground)
{
    pid_t pid = fork();

    if (pid < 0) {
        // Fork failed
        perror("smash error: fork failed");
        return;
    } else if (pid == 0) {
        // This is the child process
        // Call setpgrp to create a new process group
        if(setpgrp() == -1) {
            perror("smash error: setpgrp failed");
            exit(1);
        }
        // Execute the command
        cmd->execute();
        exit(0);
    } else {
        // This is the parent process
        if (isBackground) {
            // Don't wait for the child process to finish
            // Add the job to the jobs list
            jobs.addJob(cmd, pid);
        } else {
            // Wait for the child process to finish
            fgPid = pid; // Update the PID of the foreground process
            int status;
            if(waitpid(pid, &status, 0) == -1) {
                perror("smash error: waitpid failed");
            }
        }
    }
}

void SmallShell::executeCommand(const std::string& cmd_line)
{
    jobs.removeFinishedJobs();

    bool isBackground = isBackgroundCommand(cmd_line);
    string cmd_line_copy = cmd_line;
    if (isBackground) {
        // Remove the '&' from the command line copy
        cmd_line_copy.pop_back();
        // Trim trailing spaces
        cmd_line_copy = _rtrim(cmd_line_copy);
    }



    shared_ptr<Command> cmd = CreateCommand(cmd_line_copy);
    jobs.removeFinishedJobs();

    // Check if the command is a built-in command
    if (!dynamic_cast<ExternalCommand*>(cmd.get()) && !dynamic_cast<WatchCommand*>(cmd.get())) {
        cmd->execute();
    } else {
        executeExternalCommand(cmd, isBackground);
    }
}

const string& SmallShell::getPrompt() const
{
    return prompt;
}

void SmallShell::setPrompt(const string& newPrompt)
{
    prompt = newPrompt;
}

bool SmallShell::isAlias(const string& name)
{
    return aliases.count(name) > 0;
}

const string& SmallShell::getAlias(const string& name)
{
    return aliases.at(name);
}

void SmallShell::addAlias(const string& name, const string& command)
{
    aliases[name] = command;
}

void SmallShell::removeAlias(const string& name) {
    // Check if the alias exists
    if (aliases.find(name) != aliases.end()) {
        // Remove the alias from the map
        aliases.erase(name);
    }
}

JobsList& SmallShell::getJobs()
{
    return jobs;
}

pid_t SmallShell::getFgPid() const
{
    return fgPid;
}

void SmallShell::setFgPid(pid_t fgPid)
{
    this->fgPid = fgPid;
}

