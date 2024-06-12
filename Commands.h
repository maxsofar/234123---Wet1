#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <list>

#define COMMAND_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)

class Command {
// TODO: Add your data members
protected:
    std::string cmd_line;
public:
    Command(const std::string& cmd_line);

    virtual ~Command();

    virtual void execute() = 0;
    //virtual void prepare();
    //virtual void cleanup();
    // TODO: Add your extra methods if needed
    const std::string& getCmdLine() const;

};

class JobsList {
public:
    class JobEntry {
        int jobId;
        std::shared_ptr<Command> cmd;
        bool isStopped;
        pid_t pid;
        // TODO: Add your data members
    public:
        JobEntry(int jobId, std::shared_ptr<Command> cmd, bool isStopped, pid_t pid)
                : jobId(jobId), cmd(cmd), isStopped(isStopped), pid(pid)  {}
        // TODO: Add your methods

        int getJobId() const {
            return jobId;
        }

        std::shared_ptr<Command> getCmd() const {
            return cmd;
        }

        bool isFinished() const {
            int status;
            pid_t result = waitpid(pid, &status, WNOHANG);
            return result != 0;
        }
    };

private:
    std::list<JobEntry> jobs;
    int maxJobId;


public:
    JobsList();

    ~JobsList();

    void addJob(std::shared_ptr<Command> cmd, bool isStopped, pid_t pid);

    void printJobsList();

    void killAllJobs();

    void removeFinishedJobs();

    JobEntry *getJobById(int jobId);

    void removeJobById(int jobId);

    JobEntry *getLastJob(int *lastJobId);

    JobEntry *getLastStoppedJob(int *jobId);
    // TODO: Add extra methods or modify exisitng ones as needed
};

class SmallShell {
private:
    // TODO: Add your data members
    char* lastPwd;
    std::string prompt = "smash";
    JobsList jobs;
    std::unordered_map<std::string, std::string> aliases;
    SmallShell();

public:
    std::shared_ptr<Command> CreateCommand(const char *cmd_line);

    SmallShell(SmallShell const &) = delete; // disable copy ctor
    void operator=(SmallShell const &) = delete; // disable = operator
    static SmallShell &getInstance() // make SmallShell singleton
    {
        static SmallShell instance; // Guaranteed to be destroyed.
        // Instantiated on first use.
        return instance;
    }

    ~SmallShell();

    void executeCommand(const char *cmd_line);
    // TODO: add extra methods as needed

    const std::string& getPrompt() const;

    void setPrompt(const std::string& newPrompt);

    bool isAlias(const std::string& name);

    const std::string& getAlias(const std::string& name);

    void addAlias(const std::string& name, const std::string& command);

    void removeAlias(const std::string& name);

    const std::unordered_map<std::string, std::string>& getAliases() const;

    JobsList& getJobs();
};

//------------------------------ Built in commands ------------------------------

class BuiltInCommand : public Command {
public:
    BuiltInCommand(const std::string& cmd_line);

    virtual ~BuiltInCommand() {}
};

class ChpromptCommand : public BuiltInCommand {
public:
    ChpromptCommand(const std::string& cmd_line);

    virtual ~ChpromptCommand() {}

    void execute() override;

};

class ShowPidCommand : public BuiltInCommand {
public:
    ShowPidCommand(const std::string& cmd_line);

    virtual ~ShowPidCommand() {}

    void execute() override;

};

class GetCurrDirCommand : public BuiltInCommand {
public:
    GetCurrDirCommand(const std::string& cmd_line);

    virtual ~GetCurrDirCommand() {}

    void execute() override;
};

class ChangeDirCommand : public BuiltInCommand {
// TODO: Add your data members
    char** lastPwd;
public:
    ChangeDirCommand(const std::string& cmd_line, char **plastPwd);

    virtual ~ChangeDirCommand() {}

    void execute() override;
};

class JobsList;

class JobsCommand : public BuiltInCommand {
    // TODO: Add your data members
    JobsList* jobs;
public:
    JobsCommand(const std::string& cmd_line, JobsList *jobs);

    virtual ~JobsCommand() {}

    void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
    // TODO: Add your data members
public:
    ForegroundCommand(const char *cmd_line, JobsList *jobs);

    virtual ~ForegroundCommand() {}

    void execute() override;
};

class aliasCommand : public BuiltInCommand {
public:
    aliasCommand(const std::string& cmd_line);

    virtual ~aliasCommand() {}

    void execute() override;

private:
    bool isValidAlias(const std::string& name);
};

class unaliasCommand : public BuiltInCommand {
public:
    unaliasCommand(const std::string& cmd_line);

    virtual ~unaliasCommand() {}

    void execute() override;
};

//------------------------------ External Commands ------------------------------

class ExternalCommand : public Command {
public:
    ExternalCommand(const std::string& cmd_line);

    virtual ~ExternalCommand() {}

    void execute() override;
};

class PipeCommand : public Command {
    // TODO: Add your data members
public:
    PipeCommand(const char *cmd_line);

    virtual ~PipeCommand() {}

    void execute() override;
};

class WatchCommand : public Command {
    // TODO: Add your data members
public:
    WatchCommand(const char *cmd_line);

    virtual ~WatchCommand() {}

    void execute() override;
};

class RedirectionCommand : public Command {
    // TODO: Add your data members
public:
    explicit RedirectionCommand(const char *cmd_line);

    virtual ~RedirectionCommand() {}

    void execute() override;
};

class QuitCommand : public BuiltInCommand {
// TODO: Add your data members public:
    QuitCommand(const char *cmd_line, JobsList *jobs);

    virtual ~QuitCommand() {}

    void execute() override;
};

class KillCommand : public BuiltInCommand {
    // TODO: Add your data members
public:
    KillCommand(const char *cmd_line, JobsList *jobs);

    virtual ~KillCommand() {}

    void execute() override;
};

class ListDirCommand : public BuiltInCommand {
public:
    ListDirCommand(const char *cmd_line);

    virtual ~ListDirCommand() {}

    void execute() override;
};

class GetUserCommand : public BuiltInCommand {
public:
    GetUserCommand(const char *cmd_line);

    virtual ~GetUserCommand() {}

    void execute() override;
};


#endif //SMASH_COMMAND_H_
