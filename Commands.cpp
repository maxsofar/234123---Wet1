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
    _parseCommandLine(cmd_line.c_str(), args);
    string newPrompt = args[1] ? args[1] : "smash";
    shell.setPrompt(newPrompt); // Change the prompt of the existing instance

    // Don't forget to free the memory allocated by _parseCommandLine
    for (int i = 0; args[i]; i++) {
        free(args[i]);
    }
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
        return;
    }

    char* path = args[1];

    // If no path was provided, do nothing
    if (path == nullptr) {
        return;
    }

    // If the path is "-", change to the previous working directory
    if (strcmp(path, "-") == 0) {
        if (*lastPwd == nullptr) {
            // If OLDPWD is not set, print an error message
            cerr << "smash error: cd: OLDPWD not set" << endl;
            return;
        }
        path = *lastPwd;
    }

    // Obtain the current working directory
    char* currPwd = getcwd(nullptr, 0);
    if (currPwd == nullptr) {
        perror("smash error: getcwd failed");
        return;
    }

    // Change the working directory
    if (chdir(path) == -1) {
        perror("smash error: chdir failed");
        free(currPwd);
        return;
    }

    free(*lastPwd);
    *lastPwd = currPwd;

    for (int i = 0; i < num_args; i++) {
        free(args[i]);
    }
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
                return;
            }
        } else {
            cerr << "smash error: fg: invalid arguments" << endl;
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
            return;
        }
        jobId = lastJobId;
    } else {
        // If a job id was specified, get the job by its id
        job = jobs->getJobById(jobId);
        if (job == nullptr) {
            cerr << "smash error: fg: job-id " << jobId << " does not exist" << endl;
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

    for (int i = 0; i < numArgs; i++) {
        free(args[i]);
    }
}


QuitCommand::QuitCommand(const string& cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs(jobs)
{}
void QuitCommand::execute() {
    char* args[COMMAND_MAX_ARGS];
    int numArgs = _parseCommandLine(cmd_line.c_str(), args);
    if (numArgs > 1 && strcmp(args[1], "kill") == 0) {
        SmallShell::getInstance().getJobs().killAllJobs();
    }
    // Don't forget to free the memory allocated by _parseCommandLine
    for (int i = 0; i < numArgs; i++) {
        free(args[i]);
    }
    exit(0);
}


KillCommand::KillCommand(const string& cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs(jobs)
{}
void KillCommand::execute()
{
    char *args[COMMAND_MAX_ARGS];
    int num_args = _parseCommandLine(cmd_line.c_str(), args);

    if (num_args != 3 || args[1][0] != '-') {
        cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }

    int signum;
    int jobId;
    try {
        signum = std::stoi(args[1] + 1); // skip the '-'
        jobId = std::stoi(args[2]);
    } catch (std::invalid_argument& e) {
        cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }

    // Check if signum and jobId are positive
    if (signum <= 0 || jobId <= 0) {
        cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }

    JobsList::JobEntry *job = jobs->getJobById(jobId);
    if (!job) {
        cerr << "smash error: kill: job-id " << jobId << " does not exist" << endl;
        return;
    }

    if (kill(job->getPid(), signum) == -1) {
        perror("smash error: kill failed");
        return;
    }

    cout << "signal number " << signum << " was sent to pid " << job->getPid() << endl;
    jobs->removeJobById(jobId);

    // Don't forget to free the memory allocated by _parseCommandLine
    for (int i = 0; i < num_args; i++) {
        free(args[i]);
    }
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
        return;
    }

    for (int i = 1; i < num_args; ++i) {
        string name(args[i]);
        if (!smash.isAlias(name)) { // Alias does not exist
            cerr << "smash error: alias: " << name << " alias does not exist" << endl;
            break;
        }
        smash.removeAlias(name); // Remove the alias
        free(args[i]); // Don't forget to free the memory
    }
}

//---------------------------------- Special Commands ----------------------------------

ListDirCommand::ListDirCommand(const string& cmd_line) : BuiltInCommand(cmd_line)
{}
//void ListDirCommand::execute() {
//    char* args[COMMAND_MAX_ARGS];
//    int num_args = _parseCommandLine(cmd_line.c_str(), args);
//
//    // Check the number of arguments
//    if (num_args > 2) {
//        cerr << "smash error: listdir: Too many arguments" << endl;
//        return;
//    }
//
//    // Get the directory path
//    string dir_path = (num_args == 1) ? "." : args[1];
//
//    // Open the directory
//    DIR* dir = opendir(dir_path.c_str());
//    if (dir == NULL) {
//        perror("smash error: listdir failed");
//        return;
//    }
//
//    // Read the entries in the directory
//    struct dirent* entry;
//    while ((entry = readdir(dir)) != NULL) {
//        string type;
//        if (entry->d_type == DT_REG) {
//            type = "file";
//        } else if (entry->d_type == DT_DIR) {
//            type = "directory";
//        } else if (entry->d_type == DT_LNK) {
//            type = "link";
//        } else {
//            type = "other";
//        }
//        cout << type << ": " << entry->d_name << endl;
//    }
//
//    // Close the directory
//    closedir(dir);
//
//    // Free the memory allocated for the arguments
//    for (int i = 0; i < num_args; i++) {
//        free(args[i]);
//    }
//}

void ListDirCommand::execute() {
    char* args[COMMAND_MAX_ARGS];
    int num_args = _parseCommandLine(cmd_line.c_str(), args);

    // Check the number of arguments
    if (num_args > 2) {
        cerr << "smash error: listdir: Too many arguments" << endl;
        return;
    }

    // Get the directory path
    string dir_path = (num_args == 1) ? "." : args[1];
    // Open the directory
    int fd = open(dir_path.c_str(), O_RDONLY | O_DIRECTORY);
    if (fd == -1) {
        perror("Could not open directory");
        return;
    }

    char buffer[8192];
    struct dirent* dirent;
    off_t basep = 0;
    ssize_t bytes_read;

    vector<string> files;
    vector<string> directories;

    while ((bytes_read = getdirentries(fd, buffer, sizeof(buffer), &basep)) > 0) {
        for (int i = 0; i < bytes_read; i += dirent->d_reclen) {
            dirent = (struct dirent *) (&buffer[i]);

            // Ignore hidden files and directories
            if (dirent->d_name[0] == '.') {
                continue;
            }

            // Get the full path of the entry
            string full_path = dir_path + "/" + dirent->d_name;

            // Get information about the entry
            struct stat entry_stat;
            if (stat(full_path.c_str(), &entry_stat) == -1) {
                perror("Could not get entry information");
                continue;
            }

            // Check if the entry is a file or a directory
            if (S_ISREG(entry_stat.st_mode)) {
                files.push_back(dirent->d_name);
            } else if (S_ISDIR(entry_stat.st_mode)) {
                directories.push_back(dirent->d_name);
            }
        }
    }

    if (bytes_read == -1) {
        perror("Could not read directory entries");
    }

    close(fd);

    // Sort and print files
    sort(files.begin(), files.end());
    for (const auto& file : files) {
        std::cout << "File: " << file << std::endl;
    }

    // Sort and print directories
    sort(directories.begin(), directories.end());
    for (const auto& directory : directories) {
        std::cout << "Directory: " << directory << std::endl;
    }

    // Free the memory allocated for the arguments
    for (int i = 0; i < num_args; i++) {
        free(args[i]);
    }
}


GetUserCommand::GetUserCommand(const std::string &cmd_line) : BuiltInCommand(cmd_line)
{}
void GetUserCommand::execute()
{
    char* args[COMMAND_MAX_ARGS];
    int num_args = _parseCommandLine(cmd_line.c_str(), args);

    if (num_args != 2) {
        cerr << "smash error: getuser: invalid number of arguments" << endl;
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
        return;
    }

    char line[256];
    uid_t uid = -1;
    while (fgets(line, sizeof(line), status)) {
        if (strncmp(line, "Uid:", 4) == 0) {
            sscanf(line, "Uid:\t%d", &uid);
            break;
        }
    }
    fclose(status);

    if (uid == -1) {
        cerr << "smash error: getuser: failed to get UID of process " << pid << endl;
        return;
    }

    errno = 0;
    if ((pw = getpwuid(uid)) == nullptr) {
        if (errno == 0) {
            cerr << "smash error: getuser: process " << pid << " does not exist" << endl;
        } else {
            perror("smash error: getpwuid failed");
        }
        return;
    }

    if ((gr = getgrgid(pw->pw_gid)) == nullptr) {
        perror("smash error: getgrgid failed");
        return;
    }

    cout << "User: " << pw->pw_name << endl; // Print username on a new line
    cout << "Group: " << gr->gr_name << endl; // Print group on a new line

    // Don't forget to free the memory allocated by _parseCommandLine
    for (int i = 0; i < num_args; i++) {
        free(args[i]);
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

void JobsList::addJob(shared_ptr<Command> cmd, bool isStopped, pid_t pid) {
    //TODO: check if jobID should be reused

    // Remove finished jobs from the jobs list
    removeFinishedJobs();
    int jobId = jobs.empty() ? 1 : maxJobId + 1;
    jobs.push_back(JobEntry(jobId, cmd, isStopped, pid));
    maxJobId = jobId;  // Update the maximum job ID
}

void JobsList::killAllJobs() {
    cout << "smash: sending SIGKILL signal to " << jobs.size() << " jobs:" << endl;
    for (auto &job : jobs) {
        if (!job.isFinished()) {
            cout << job.getPid() << ": " << job.getCmdLine() << "&" << endl;
            kill(job.getPid(), SIGKILL);
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
            cerr << "Exec failed" << endl;
        }
    } else {
        // This is a simple command, run it directly
        if (execvp(args[0], args) < 0) {
            cerr << "Exec failed" << endl;
        }
    }

    // Free the memory allocated for the arguments
    for (int i = 0; i < num_args; ++i) {
        free(args[i]);
    }
}

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
        perror("open");
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
    } else if (firstWord == "listdir") { // Add this condition
        return make_shared<ListDirCommand>(cmd_line); // Create a new ListDirCommand instance
    } else if (firstWord == "getuser") {
        return make_shared<GetUserCommand>(cmd_line);
    } else {
        // Check if the first word is an alias
        if (isAlias(firstWord)) {
            // Replace the alias with the corresponding command
            string aliasCommand = getAlias(firstWord);
            cmd_s = cmd_s.replace(0, firstWord.length(), aliasCommand);
        }

        // Check for redirection in the command line
        if (cmd_s.find('>') != string::npos) {
            return make_shared<RedirectionCommand>(cmd_line);
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
            jobs.addJob(cmd, false, pid);
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
    if (!dynamic_cast<ExternalCommand*>(cmd.get())) {
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

