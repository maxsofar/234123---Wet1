#include <unistd.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include <signal.h>
#include "Commands.h"
#include <fcntl.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>

using namespace std;

const string WHITESPACE = " \n\r\t\f\v";

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
    istringstream iss(_trim(string(cmd_line)).c_str());
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
    cout << "smash pid is " << getpid() << endl;
}


GetCurrDirCommand::GetCurrDirCommand(const string& cmd_line) : BuiltInCommand(cmd_line)
{}
void GetCurrDirCommand::execute() {
    char buf[PATH_MAX];
    if (getcwd(buf, PATH_MAX) == NULL) {
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
    if (num_args > 2) {
        cout << "smash error: cd: too many arguments" << endl;
        return;
    }
    char* path = args[1];
    if (path == nullptr) {
        return;
    }

    if (strcmp(path, "-") == 0) {
        if (*lastPwd == nullptr) {
            cout << "smash error: cd: OLDPWD not set" << endl;
            return;
        }
        path = *lastPwd;
    }

    //TODO: implement not exist path error handling (Error Handling section)
    char* newPwd = getcwd(NULL, 0);
    if (chdir(path) == -1) {
        perror("smash error");
        free(newPwd);
        return;
    }

    free(*lastPwd);
    *lastPwd = newPwd;
}


JobsCommand::JobsCommand(const string& cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs(jobs)
{}
void JobsCommand::execute() {
    jobs->printJobsList();
}


ForegroundCommand::ForegroundCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), jobs(jobs)
{}
void ForegroundCommand::execute() {
    char* args[COMMAND_MAX_ARGS];
    int numArgs = _parseCommandLine(cmd_line.c_str(), args);

    // Check if the number of arguments is valid
    if (numArgs > 2) {
        cout << "smash error: fg: invalid arguments" << endl;
        return;
    }

    int jobId = -1;
    if (args[1] != nullptr) {
        // If there is a second argument, it should be the job id
        string arg = args[1];
        if (all_of(arg.begin(), arg.end(), ::isdigit)) {
            jobId = stoi(arg);
            if (jobId <= 0) {
                cout << "smash error: fg: invalid arguments" << endl;
                return;
            }
        } else {
            cout << "smash error: fg: invalid arguments" << endl;
            return;
        }
    }

    JobsList::JobEntry *job;
    if (jobId == -1) {
        // If no job id was specified, get the job with the maximal job id
        int lastJobId;
        job = jobs->getLastJob(&lastJobId);
        if (job == nullptr) {
            cout << "smash error: fg: jobs list is empty" << endl;
            return;
        }
        jobId = lastJobId;
    } else {
        // If a job id was specified, get the job by its id
        job = jobs->getJobById(jobId);
        if (job == nullptr) {
            cout << "smash error: fg: job-id " << jobId << " does not exist" << endl;
            return;
        }
    }

    // Print the command line of the job along with its PID
    cout << job->getCmdLine() << "& " << job->getPid() << endl;

    // Bring the process to the foreground by waiting for it
    int status;
    if (waitpid(job->getPid(), &status, WCONTINUED | WUNTRACED) == -1) {
        perror("smash error: waitpid failed");
    }

    // Remove the job from the jobs list
    jobs->removeJobById(jobId);
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
        cout << "smash error: kill: invalid arguments" << endl;
        return;
    }

    int signum = atoi(args[1] + 1); // skip the '-'
    int jobid = atoi(args[2]);

    // Check if signum and jobid are positive
    if (signum <= 0 || jobid <= 0) {
        cout << "smash error: kill: invalid arguments" << endl;
        return;
    }


    JobsList::JobEntry *job = jobs->getJobById(jobid);
    if (!job) {
        cout << "smash error: kill: job-id " << jobid << " does not exist" << endl;
        return;
    }

    if (kill(job->getPid(), signum) == -1) {
        perror("smash error: kill failed");
        return;
    }

    cout << "signal number " << signum << " was sent to pid " << job->getPid() << endl;
    jobs->removeJobById(jobid);

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
    if (cmd_line.find('=') == string::npos) { // No '=' found, list all aliases
        for (const auto& alias : smash.getAliases()) {
            cout << alias.first << "='" << alias.second << "'" << endl;
        }
    } else { // '=' found, add new alias
        string name = _trim(cmd_line.substr(0, cmd_line.find('=')));
        string command = _trim(cmd_line.substr(cmd_line.find('=') + 1));

        // Remove the single quotes from the command
        command.erase(remove(command.begin(), command.end(), '\''), command.end());

        if (smash.isAlias(name)) {
            cout << "smash error: alias: " << name << " already exists or is a reserved command" << endl;
        } else if (!isValidAlias(name)) {
            cout << "smash error: alias: Invalid alias format" << endl;
        } else {
            smash.addAlias(name, command);
        }
    }
}
bool aliasCommand::isValidAlias(const string& name) {
    for (char c : name) {
        if (!isalnum(c) && c != '_') {
            return false;
        }
    }
    return true;
}
const unordered_map<string, string>& SmallShell::getAliases() const
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
        cout << "smash error: alias: Not enough arguments" << endl;
        return;
    }

    for (int i = 1; i < num_args; ++i) {
        string name(args[i]);
        if (!smash.isAlias(name)) { // Alias does not exist
            cout << "smash error: alias: " << name << " alias does not exist" << endl;
            break;
        }
        smash.removeAlias(name); // Remove the alias
        free(args[i]); // Don't forget to free the memory
    }
}

ListDirCommand::ListDirCommand(const string& cmd_line) : BuiltInCommand(cmd_line)
{}
void ListDirCommand::execute() {
    char* args[COMMAND_MAX_ARGS];
    int num_args = _parseCommandLine(cmd_line.c_str(), args);

    // Check the number of arguments
    if (num_args > 2) {
        cout << "smash error: listdir: Too many arguments" << endl;
        return;
    }

    // Get the directory path
    string dir_path = (num_args == 1) ? "." : args[1];

    // Open the directory
    DIR* dir = opendir(dir_path.c_str());
    if (dir == NULL) {
        perror("smash error: listdir failed");
        return;
    }

    // Read the entries in the directory
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        string type;
        if (entry->d_type == DT_REG) {
            type = "file";
        } else if (entry->d_type == DT_DIR) {
            type = "directory";
        } else if (entry->d_type == DT_LNK) {
            type = "link";
        } else {
            type = "other";
        }
        cout << type << ": " << entry->d_name << endl;
    }

    // Close the directory
    closedir(dir);

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

    if (num_args > 2) {
        cout << "smash error: getuser: Too many arguments" << endl;
        return;
    }

    pid_t pid = atoi(args[1]);
    struct passwd *pw;
    struct group *gr;

    errno = 0;
    if ((pw = getpwuid(pid)) == NULL) {
        if (errno == 0) {
            cout << "smash error: getuser: process " << pid << " does not exist" << endl;
        } else {
            perror("smash error: getpwuid failed");
        }
        return;
    }

    if ((gr = getgrgid(pw->pw_gid)) == NULL) {
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

JobsList::~JobsList()
{}

void JobsList::printJobsList() {
    removeFinishedJobs();
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
            cout << job.getPid() << ": " << job.getCmdLine() << endl;
            kill(job.getPid(), SIGKILL);
        }
    }
}

void JobsList::removeFinishedJobs() {
    for (auto it = jobs.begin(); it != jobs.end(); ) {
        if (it->isFinished()) {
            it = jobs.erase(it);
        } else {
            ++it;
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
    args[num_args] = NULL;

    // Execute the external command
    if (execvp(args[0], args) < 0) {
        cerr << "Exec failed" << endl;
    }

    // Free the memory allocated for the arguments
    for (int i = 0; i < num_args; ++i) {
        free(args[i]);
    }
}

RedirectionCommand::RedirectionCommand(const char *cmd_line): Command(cmd_line) {}

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
    SmallShell::getInstance().executeCommand(command.c_str());

    // Reset stdout
    dup2(stdout_copy, STDOUT_FILENO);
    close(stdout_copy);
}


//---------------------------------- Small Shell ----------------------------------

SmallShell::SmallShell(): lastPwd(nullptr) {
// TODO: add your implementation
}

SmallShell::~SmallShell() {
// TODO: add your implementation
    if (lastPwd != nullptr) {
        free(lastPwd);
    }
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
shared_ptr<Command> SmallShell::CreateCommand(const char *cmd_line)
{
    string cmd_s = _trim(string(cmd_line));
    string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

    // Check for redirection in the command line
    if (cmd_s.find('>') != string::npos) {
        return make_shared<RedirectionCommand>(cmd_line);
    }

    // getuser command
    if (firstWord.compare("getuser") == 0) {
        return make_shared<GetUserCommand>(cmd_line);
    }

    if (firstWord.compare("chprompt") == 0) {
        return make_shared<ChpromptCommand>(cmd_line);
    } else if (firstWord.compare("showpid") == 0) {
        return make_shared<ShowPidCommand>(cmd_line);
    } else if (firstWord.compare("pwd") == 0) {
        return make_shared<GetCurrDirCommand>(cmd_line);
    } else if (firstWord.compare("cd") == 0) {
        return make_shared<ChangeDirCommand>(cmd_line, &lastPwd);
    } else if (firstWord.compare("jobs") == 0) {
        return make_shared<JobsCommand>(cmd_line, &jobs);
    } else if (firstWord.compare("alias") == 0) {
        return make_shared<aliasCommand>(cmd_line);
    } else if (firstWord.compare("unalias") == 0) {
        return make_shared<unaliasCommand>(cmd_line);
    } else if (firstWord.compare("quit") == 0) {
        return make_shared<QuitCommand>(cmd_line, &jobs);
    } else if (firstWord.compare("kill") == 0) {
        return make_shared<KillCommand>(cmd_line, &jobs);
    } else if (firstWord.compare("fg") == 0) {
        return make_shared<ForegroundCommand>(cmd_line, &jobs);
    } else if (firstWord.compare("listdir") == 0) { // Add this condition
        return make_shared<ListDirCommand>(cmd_line); // Create a new ListDirCommand instance
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

void SmallShell::handleBackgroundCommand(char* &cmd_line_copy, const char *cmd_line) {
    bool isBackground = _isBackgroundComamnd(cmd_line);
    cmd_line_copy = strdup(cmd_line); // Create a copy of cmd_line to modify
    if (isBackground) {
        // Remove the '&' argument
        _removeBackgroundSign(cmd_line_copy);
    }
}

shared_ptr<Command> SmallShell::createCommandAndRemoveJobs(char* cmd_line_copy) {
    shared_ptr<Command> cmd = CreateCommand(cmd_line_copy);
    jobs.removeFinishedJobs();
    return cmd;
}

void SmallShell::executeExternalCommand(shared_ptr<Command> cmd, bool isBackground) {
    // Fork a new process
    pid_t pid = fork();

    if (pid < 0) {
        // Fork failed
        cerr << "Fork failed" << endl;
        return;
    } else if (pid == 0) {
        // This is the child process
        // Call setpgrp to create a new process group
        setpgrp();
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
            waitpid(pid, &status, 0);
        }
    }
}

void SmallShell::executeCommand(const char *cmd_line) {
    char* cmd_line_copy;
    handleBackgroundCommand(cmd_line_copy, cmd_line);
    shared_ptr<Command> cmd = createCommandAndRemoveJobs(cmd_line_copy);

    // Check if the command is a built-in command
    if (!dynamic_cast<ExternalCommand*>(cmd.get())) {
        cmd->execute();
    } else {
        executeExternalCommand(cmd, _isBackgroundComamnd(cmd_line));
    }
    free(cmd_line_copy); // Free the allocated memory
}

/*void SmallShell::executeCommand(const char *cmd_line) {
    bool isBackground = _isBackgroundComamnd(cmd_line);
    char* cmd_line_copy = strdup(cmd_line); // Create a copy of cmd_line to modify
    if (isBackground) {
        // Remove the '&' argument
        _removeBackgroundSign(cmd_line_copy);
    }
    shared_ptr<Command> cmd = CreateCommand(cmd_line_copy);

    jobs.removeFinishedJobs();
    // Check if the command is a built-in command
    if (!dynamic_cast<ExternalCommand*>(cmd.get())) {
        // Save the original stdout
        int stdout_copy = dup(STDOUT_FILENO);

        // Check for redirection
        char* redirect = strchr(cmd_line_copy, '>');
        if (redirect != NULL) {
            // There is a redirection in the command
            // Split the command and the file
            *redirect = '\0';
            char* file = redirect + 1;
            int fd;

            // Convert char* to string for trimming
            string file_str(file);

            if (*file == '>') {
                // Append to the file
                file++;
                file_str = file; // Update string to new file pointer position
                file_str = _trim(file_str); // Trim the string
                fd = open(file_str.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0666);
            } else {
                // Overwrite the file
                file_str = _trim(file_str); // Trim the string
                fd = open(file_str.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
            }

            if (fd == -1) {
                perror("open");
                return;
            }

            // Redirect stdout to the file
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }

        // Execute the command
        cmd->execute();

        // Reset stdout
        if (redirect != NULL) {
            dup2(stdout_copy, STDOUT_FILENO);
            close(stdout_copy);
        }
    } else {
        // Fork a new process
        pid_t pid = fork();

        if (pid < 0) {
            // Fork failed
            cerr << "Fork failed" << endl;
            return;
        } else if (pid == 0) {
            // This is the child process
            // Call setpgrp to create a new process group
            setpgrp();
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
                int status;
                waitpid(pid, &status, 0);
            }
        }
    }
    free(cmd_line_copy); // Free the allocated memory
}*/

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

