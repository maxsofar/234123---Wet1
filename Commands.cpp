#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include <signal.h>
#include "Commands.h"

using namespace std;

const std::string WHITESPACE = " \n\r\t\f\v";

#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

string _ltrim(const std::string &s) {
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string &s) {
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string &s) {
    return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char *cmd_line, char **args) {
    FUNC_ENTRY()
    int i = 0;
    std::istringstream iss(_trim(string(cmd_line)).c_str());
    for (std::string s; iss >> s;) {
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

// TODO: Add your implementation for classes in Commands.h 

SmallShell::SmallShell(): lastPwd(nullptr) {
// TODO: add your implementation
}

SmallShell::~SmallShell() {
// TODO: add your implementation
    if (lastPwd != nullptr) {
        free(lastPwd);
    }
}

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
    string cmd_s = string(cmd_line);
    size_t pos = cmd_s.find_first_of(" \n");
    string newPrompt = (pos == string::npos) ? "smash" : cmd_s.substr(pos+1);
    shell.setPrompt(newPrompt); // Change the prompt of the existing instance
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
        std::cout << "smash error: cd: too many arguments" << std::endl;
        return;
    }
    char* path = args[1];
    if (path == nullptr) {
        return;
    }

    if (strcmp(path, "-") == 0) {
        if (*lastPwd == nullptr) {
            std::cout << "smash error: cd: OLDPWD not set" << std::endl;
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
    _parseCommandLine(cmd_line.c_str(), args);

    int jobId = -1;
    if (args[1] != nullptr) {
        // If there is a second argument, it should be the job id
        jobId = std::stoi(args[1]);
    }

    JobsList::JobEntry *job;
    if (jobId == -1) {
        // If no job id was specified, get the job with the maximal job id
        int lastJobId;
        job = jobs->getLastJob(&lastJobId);
        if (job == nullptr) {
            std::cerr << "smash error: fg: no jobs currently running" << std::endl;
            return;
        }
        jobId = lastJobId;
    } else {
        // If a job id was specified, get the job by its id
        job = jobs->getJobById(jobId);
        if (job == nullptr) {
            std::cerr << "smash error: fg: job-id " << jobId << " does not exist" << std::endl;
            return;
        }
    }

    // Print the command line of the job along with its PID
    std::cout << job->getCmdLine() << "& " << job->getPid() << std::endl;

    // Bring the process to the foreground by waiting for it
    int status;
    if (waitpid(job->getPid(), &status, WCONTINUED | WUNTRACED) == -1) {
        perror("smash error: waitpid failed");
    }

    // Remove the job from the jobs list
    jobs->removeJobById(jobId);
}


aliasCommand::aliasCommand(const string& cmd_line) : BuiltInCommand(cmd_line)
{}
void aliasCommand::execute()
{
    SmallShell& smash = SmallShell::getInstance();

    // Remove the 'alias' part from the command line
    size_t pos = cmd_line.find("alias");
    if (pos != std::string::npos) {
        cmd_line.erase(0, pos + 5); // 5 is the length of 'alias'
    }
    if (cmd_line.find('=') == std::string::npos) { // No '=' found, list all aliases
        for (const auto& alias : smash.getAliases()) {
            std::cout << alias.first << "='" << alias.second << "'" << std::endl;
        }
    } else { // '=' found, add new alias
        std::string name = _trim(cmd_line.substr(0, cmd_line.find('=')));
        std::string command = _trim(cmd_line.substr(cmd_line.find('=') + 1));

        // Remove the single quotes from the command
        command.erase(std::remove(command.begin(), command.end(), '\''), command.end());

        if (smash.isAlias(name)) {
            std::cerr << "smash error: alias: " << name << " already exists or is a reserved command" << std::endl;
        } else if (!isValidAlias(name)) {
            std::cerr << "smash error: alias: Invalid alias format" << std::endl;
        } else {
            smash.addAlias(name, command);
        }
    }
}
bool aliasCommand::isValidAlias(const string& name) {
    for (char c : name) {
        if (!std::isalnum(c) && c != '_') {
            return false;
        }
    }
    return true;
}
const std::unordered_map<string, string>& SmallShell::getAliases() const
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
        std::cerr << "smash error: alias: Not enough arguments" << std::endl;
        return;
    }

    for (int i = 1; i < num_args; ++i) {
        string name(args[i]);
        if (!smash.isAlias(name)) { // Alias does not exist
            std::cerr << "smash error: alias: " << name << " alias does not exist" << std::endl;
            break;
        }
        smash.removeAlias(name); // Remove the alias
        free(args[i]); // Don't forget to free the memory
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
        std::cerr << "smash error: kill: invalid arguments" << std::endl;
        return;
    }

    int signum = atoi(args[1] + 1); // skip the '-'
    int jobid = atoi(args[2]);

    JobsList::JobEntry *job = jobs->getJobById(jobid);
    if (!job) {
        std::cerr << "smash error: kill: job-id " << jobid << " does not exist" << std::endl;
        return;
    }

    if (kill(job->getPid(), signum) == -1) {
        perror("smash error: kill failed");
        return;
    }

    std::cout << "signal number " << signum << " was sent to pid " << job->getPid() << std::endl;
    jobs->removeJobById(jobid);

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
        std::cout << "[" << job.getJobId() << "] "
                  << job.getCmd()->getCmdLine() << "&"
                  << std::endl;
    }
}

void JobsList::addJob(std::shared_ptr<Command> cmd, bool isStopped, pid_t pid) {
    //TODO: check if jobID should be reused
    int jobId = jobs.empty() ? 1 : maxJobId + 1;
    jobs.push_back(JobEntry(jobId, cmd, isStopped, pid));
    maxJobId = jobId;  // Update the maximum job ID
}


void JobsList::killAllJobs() {
    std::cout << "smash: sending SIGKILL signal to " << jobs.size() << " jobs:" << std::endl;
    for (auto &job : jobs) {
        if (!job.isFinished()) {
            std::cout << job.getPid() << ": " << job.getCmdLine() << std::endl;
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

    // Initialize the last job with the first job in the list
    JobEntry *lastJob = &jobs.front();

    // Iterate over the jobs list to find the job with the maximal job id
    for (auto &job : jobs) {
        if (job.getJobId() > maxJobId) {
            lastJob = &job;
            maxJobId = job.getJobId();
        }
    }

    // If the lastJobId pointer is not null, assign the maximal job id to it
    if (lastJobId != nullptr) {
        *lastJobId = maxJobId;
    }

    return lastJob;
}



//---------------------------------- External Command ----------------------------------

ExternalCommand::ExternalCommand(const string& cmd_line) : Command(cmd_line)
{}

/*void ExternalCommand::execute()
{
    // Parse the command line into arguments
    char* args[COMMAND_MAX_ARGS];
    int num_args = _parseCommandLine(cmd_line.c_str(), args);

    // Check if the command should be run in the background
    bool isBackground = _isBackgroundComamnd(cmd_line.c_str());
    if (isBackground) {
        // Remove the '&' argument
        _removeBackgroundSign(const_cast<char*>(cmd_line.c_str()));
        // Re-parse the command line into arguments
        num_args = _parseCommandLine(cmd_line.c_str(), args);
    }

    // Fork a new process
    pid_t pid = fork();

    if (pid < 0) {
        // Fork failed
        std::cerr << "Fork failed" << std::endl;
        return;
    } else if (pid == 0) {
        // This is the child process
        // Execute the external command
        if (execvp(args[0], args) < 0) {
            std::cerr << "Exec failed" << std::endl;
        }
        exit(0);
    } else {
        // This is the parent process
        if (isBackground) {
            // Don't wait for the child process to finish
            // Add the job to the jobs list
            std::shared_ptr<Command> cmd = std::make_shared<ExternalCommand>(cmd_line);
            SmallShell::getInstance().getJobs().addJob(cmd, false, pid);  // Pass the PID to addJob
        } else {
            // Wait for the child process to finish
            int status;
            waitpid(pid, &status, 0);
        }
    }

    // Free the memory allocated for the arguments
    for (int i = 0; i < num_args; ++i) {
        free(args[i]);
    }
}*/
void ExternalCommand::execute() {
    // Parse the command line into arguments
    char* args[COMMAND_MAX_ARGS];
    int num_args = _parseCommandLine(cmd_line.c_str(), args);

    // Execute the external command
    if (execvp(args[0], args) < 0) {
        std::cerr << "Exec failed" << std::endl;
    }

    // Free the memory allocated for the arguments
    for (int i = 0; i < num_args; ++i) {
        free(args[i]);
    }
}



//---------------------------------- Small Shell ----------------------------------

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
std::shared_ptr<Command> SmallShell::CreateCommand(const char *cmd_line)
{
    string cmd_s = _trim(string(cmd_line));
    string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

    if (firstWord.compare("chprompt") == 0) {
        return std::make_shared<ChpromptCommand>(cmd_line);
    } else if (firstWord.compare("showpid") == 0) {
        return std::make_shared<ShowPidCommand>(cmd_line);
    } else if (firstWord.compare("pwd") == 0) {
        return std::make_shared<GetCurrDirCommand>(cmd_line);
    } else if (firstWord.compare("cd") == 0) {
        return std::make_shared<ChangeDirCommand>(cmd_line, &lastPwd);
    } else if (firstWord.compare("jobs") == 0) {
        return std::make_shared<JobsCommand>(cmd_line, &jobs);
    } else if (firstWord.compare("alias") == 0) {
        return std::make_shared<aliasCommand>(cmd_line);
    } else if (firstWord.compare("unalias") == 0) {
        return std::make_shared<unaliasCommand>(cmd_line);
    } else if (firstWord.compare("quit") == 0) {
        return std::make_shared<QuitCommand>(cmd_line, &jobs);
    } else if (firstWord.compare("kill") == 0) {
        return std::make_shared<KillCommand>(cmd_line, &jobs);
    } else if (firstWord.compare("fg") == 0) {
        return std::make_shared<ForegroundCommand>(cmd_line, &jobs);
    } else {
        // Check if the first word is an alias
        if (isAlias(firstWord)) {
            // Replace the alias with the corresponding command
            string aliasCommand = getAlias(firstWord);
            cmd_s = cmd_s.replace(0, firstWord.length(), aliasCommand);
        }
        return std::make_shared<ExternalCommand>(cmd_s);
    }
}

void SmallShell::executeCommand(const char *cmd_line) {
    bool isBackground = _isBackgroundComamnd(cmd_line);
    if (isBackground) {
        // Remove the '&' argument
        _removeBackgroundSign(const_cast<char*>(cmd_line));
    }
    std::shared_ptr<Command> cmd = CreateCommand(cmd_line);

    // Check if the command is an external command
    if (dynamic_cast<ExternalCommand*>(cmd.get())) {
        // Fork a new process
        pid_t pid = fork();

        if (pid < 0) {
            // Fork failed
            std::cerr << "Fork failed" << std::endl;
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
    } else {
        // For built-in commands, just execute the command
        cmd->execute();
    }
}


/*void SmallShell::executeCommand(const char *cmd_line)
{
    // TODO: Add your implementation here
    std::shared_ptr<Command> cmd = CreateCommand(cmd_line);

    //fake jobs
//    std::shared_ptr<Command> cmd1 = std::make_shared<ChpromptCommand>("chprompt");
//    jobs.addJob(cmd1, false);
//
//    std::shared_ptr<Command> cmd2 = std::make_shared<ShowPidCommand>("showpid");
//    jobs.addJob(cmd2, false);
//
//    std::shared_ptr<Command> cmd3 = std::make_shared<GetCurrDirCommand>("pwd");
//    jobs.addJob(cmd3, false);


    cmd->execute();
    // Please note that you must fork smash process for some commands (e.g., external commands....)
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

