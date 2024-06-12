#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
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

Command::Command(const char *cmd_line) : cmd_line(cmd_line) {
    // Constructor implementation here
}

Command::~Command() {
    // Destructor implementation here
}

const char *Command::getCmdLine() const
{
    return cmd_line;
}



//---------------------------------- Built in commands ----------------------------------

BuiltInCommand::BuiltInCommand(const char *cmd_line) : Command(cmd_line)
{}

ChpromptCommand::ChpromptCommand(const char *cmd_line) : BuiltInCommand(cmd_line)
{}

void ChpromptCommand::execute() {
    SmallShell& shell = SmallShell::getInstance(); // Get the existing instance (singleton)
    string cmd_s = string(cmd_line);
    size_t pos = cmd_s.find_first_of(" \n");
    string newPrompt = (pos == string::npos) ? "smash" : cmd_s.substr(pos+1);
    shell.setPrompt(newPrompt); // Change the prompt of the existing instance
}

ShowPidCommand::ShowPidCommand(const char *cmd_line) : BuiltInCommand(cmd_line)
{}

void ShowPidCommand::execute() {
    cout << "smash pid is " << getpid() << endl;
}

GetCurrDirCommand::GetCurrDirCommand(const char *cmd_line) : BuiltInCommand(cmd_line)
{}

void GetCurrDirCommand::execute() {
    char buf[PATH_MAX];
    if (getcwd(buf, PATH_MAX) == NULL) {
        perror("smash error: getcwd failed");
    } else {
        cout << buf << endl;
    }
}

ChangeDirCommand::ChangeDirCommand(const char *cmd_line, char **plastPwd) : BuiltInCommand(cmd_line), lastPwd(plastPwd)
{}

void ChangeDirCommand::execute() {
    char* args[COMMAND_MAX_ARGS];
    int num_args = _parseCommandLine(cmd_line, args);
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

JobsCommand::JobsCommand(const char *cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs(jobs)
{}

void JobsCommand::execute() {
    jobs->printJobsList();
}



aliasCommand::aliasCommand(const char *cmd_line) : BuiltInCommand(cmd_line)
{}

void aliasCommand::execute()
{
    SmallShell& smash = SmallShell::getInstance();
    std::string cmd_str(cmd_line);

    // Remove the 'alias' part from the command line
    size_t pos = cmd_str.find("alias");
    if (pos != std::string::npos) {
        cmd_str.erase(0, pos + 5); // 5 is the length of 'alias'
    }
    if (cmd_str.find('=') == std::string::npos) { // No '=' found, list all aliases
        for (const auto& alias : smash.getAliases()) {
            std::cout << alias.first << "='" << alias.second << "'" << std::endl;
        }
    } else { // '=' found, add new alias
        std::string name = _trim(cmd_str.substr(0, cmd_str.find('=')));
        std::string command = _trim(cmd_str.substr(cmd_str.find('=') + 1));

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

bool aliasCommand::isValidAlias(const std::string& name) {
    for (char c : name) {
        if (!std::isalnum(c) && c != '_') {
            return false;
        }
    }
    return true;
}

const std::unordered_map<std::string, std::string>& SmallShell::getAliases() const
{
    return aliases;
}

unaliasCommand::unaliasCommand(const char *cmd_line) : BuiltInCommand(cmd_line)
{}

void unaliasCommand::execute()
{
    SmallShell& smash = SmallShell::getInstance();
    std::string cmd_str(cmd_line);
    cmd_str = _trim(cmd_str); // Remove leading and trailing whitespaces

    char* args[COMMAND_MAX_ARGS];
    int num_args = _parseCommandLine(cmd_str.c_str(), args); // Parse the command line

    if (num_args <= 1) { // No arguments provided
        std::cerr << "smash error: alias: Not enough arguments" << std::endl;
        return;
    }

    for (int i = 1; i < num_args; ++i) {
        std::string name(args[i]);
        if (!smash.isAlias(name)) { // Alias does not exist
            std::cerr << "smash error: alias: " << name << " alias does not exist" << std::endl;
            break;
        }
        smash.removeAlias(name); // Remove the alias
    }
}


//---------------------------------- Job List ----------------------------------

JobsList::JobsList() : maxJobId(0)
{}

JobsList::~JobsList()
{}

void JobsList::printJobsList() {
    for (const auto &job : jobs) {
        std::cout << "[" << job.getJobId() << "] "
                  << job.getCmd()->getCmdLine() << " &"
                  << std::endl;
    }
}

void JobsList::addJob(std::shared_ptr<Command> cmd, bool isStopped)
{
    jobs.push_back(JobEntry(++maxJobId, cmd, isStopped));
}

//---------------------------------- External Command ----------------------------------

ExternalCommand::ExternalCommand(const char *cmd_line) : Command(cmd_line)
{}

void ExternalCommand::execute()
{
    // Parse the command line into arguments
    char* args[COMMAND_MAX_ARGS];
    int num_args = _parseCommandLine(cmd_line, args);

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
        // Wait for the child process to finish
        int status;
        waitpid(pid, &status, 0);
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
    } else {
        // Check if the first word is an alias
        if (isAlias(firstWord)) {
            // Replace the alias with the corresponding command
            string aliasCommand = getAlias(firstWord);
            cmd_s = cmd_s.replace(0, firstWord.length(), aliasCommand);
        }
        return std::make_shared<ExternalCommand>(cmd_s.c_str());
    }
}


void SmallShell::executeCommand(const char *cmd_line)
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
}

const std::string& SmallShell::getPrompt() const
{
    return prompt;
}

void SmallShell::setPrompt(const std::string& newPrompt)
{
    prompt = newPrompt;
}

bool SmallShell::isAlias(const std::string& name)
{
    return aliases.count(name) > 0;
}

const std::string& SmallShell::getAlias(const std::string& name)
{
    return aliases.at(name);
}

void SmallShell::addAlias(const std::string& name, const std::string& command)
{
    aliases[name] = command;
}

void SmallShell::removeAlias(const std::string& name) {
    // Check if the alias exists
    if (aliases.find(name) != aliases.end()) {
        // Remove the alias from the map
        aliases.erase(name);
    }
}


