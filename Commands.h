#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <utility>
#include <list>
#include <map>
#include <memory>
#include <sys/wait.h>
#include <set>


#define COMMAND_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)

class Command {
protected:
    std::string cmd_line;
public:
    explicit Command(const std::string& cmd_line);
    virtual ~Command();

    virtual void execute() = 0;
    //virtual void prepare();
    //virtual void cleanup();
    const std::string& getCmdLine() const;

};

class JobsList {
public:
    class JobEntry {
        int jobId;
        std::shared_ptr<Command> cmd;
        bool isStopped;
        pid_t pid;
    public:
        JobEntry(int jobId, std::shared_ptr<Command> cmd, bool isStopped, pid_t pid)
                : jobId(jobId), cmd(std::move(cmd)), isStopped(isStopped), pid(pid)  {}

        int getJobId() const {
            return jobId;
        }

        pid_t getPid() const {
            return pid;
        }

        std::string getCmdLine() const {
            return cmd->getCmdLine();
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

    ~JobsList() = default;

    void addJob(std::shared_ptr<Command> cmd, bool isStopped, pid_t pid);

    void printJobsList();

    void killAllJobs();

    void removeFinishedJobs();

    JobEntry *getJobById(int jobId);

    void removeJobById(int jobId);

    JobEntry *getLastJob(int *lastJobId);

//    JobEntry *getLastStoppedJob(int *jobId);
};

class SmallShell {
private:
    // members
    char* lastPwd;
    std::string prompt = "smash";
    JobsList jobs;
    std::map<std::string, std::string> aliases;
    pid_t fgPid;

    // methods
    SmallShell();
    void executeExternalCommand(const std::shared_ptr<Command>& cmd, bool isBackground);

public:
    static const std::set<std::string> RESERVED_KEYWORDS;

    std::shared_ptr<Command> CreateCommand(const std::string& cmd_line);
    SmallShell(SmallShell const &) = delete; // disable copy ctor
    void operator=(SmallShell const &) = delete; // disable = operator
    static SmallShell &getInstance() // make SmallShell singleton
    {
        static SmallShell instance; // Guaranteed to be destroyed.
        // Instantiated on first use.
        return instance;
    }
    ~SmallShell();

    void executeCommand(const std::string& cmd_line);

    //prompt
    const std::string& getPrompt() const;
    void setPrompt(const std::string& newPrompt);

    //aliases
    bool isAlias(const std::string& name);
    const std::string& getAlias(const std::string& name);
    void addAlias(const std::string& name, const std::string& command);
    void removeAlias(const std::string& name);
    const std::map<std::string, std::string>& getAliases() const;

    JobsList& getJobs();

    pid_t getFgPid() const;
    void setFgPid(pid_t fgPid);
};

//------------------------------ Built in commands ------------------------------

class BuiltInCommand : public Command {
public:
    explicit BuiltInCommand(const std::string& cmd_line);

    ~BuiltInCommand() override = default;
};

class ChpromptCommand : public BuiltInCommand {
public:
    explicit ChpromptCommand(const std::string& cmd_line);

    ~ChpromptCommand() override = default;

    void execute() override;

};

class ShowPidCommand : public BuiltInCommand {
public:
    explicit ShowPidCommand(const std::string& cmd_line);

    ~ShowPidCommand() override = default;

    void execute() override;

};

class GetCurrDirCommand : public BuiltInCommand {
public:
    explicit GetCurrDirCommand(const std::string& cmd_line);

    ~GetCurrDirCommand() override = default;

    void execute() override;
};

class ChangeDirCommand : public BuiltInCommand {
    char** lastPwd;
public:
    ChangeDirCommand(const std::string& cmd_line, char **plastPwd);

    ~ChangeDirCommand() override = default;

    void execute() override;
};

class JobsCommand : public BuiltInCommand {
    JobsList* jobs;
public:
    JobsCommand(const std::string& cmd_line, JobsList *jobs);

    ~JobsCommand() override = default;

    void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
    JobsList* jobs;
public:
    ForegroundCommand(const std::string& cmd_line, JobsList *jobs);

    ~ForegroundCommand() override = default;

    void execute() override;
};

class QuitCommand : public BuiltInCommand {
    JobsList* jobs;
public:
    QuitCommand(const std::string& cmd_line, JobsList *jobs);

    ~QuitCommand() override = default;

    void execute() override;
};

class KillCommand : public BuiltInCommand {
    JobsList* jobs;
public:
    KillCommand(const std::string&, JobsList *jobs);

    ~KillCommand() override = default;

    void execute() override;
};

class aliasCommand : public BuiltInCommand {
public:
    explicit aliasCommand(const std::string& cmd_line);

    ~aliasCommand() override = default;

    void execute() override;

private:
    static bool isValidAlias(const std::string& name, const std::string& command);
};

class unaliasCommand : public BuiltInCommand {
public:
    explicit unaliasCommand(const std::string& cmd_line);

    ~unaliasCommand() override = default;

    void execute() override;
};

class ListDirCommand : public BuiltInCommand {
public:
    explicit ListDirCommand(const std::string& cmd_line);

    ~ListDirCommand() override = default;

    void execute() override;
};

class GetUserCommand : public BuiltInCommand {
public:
    explicit GetUserCommand(const std::string& cmd_line);

    ~GetUserCommand() override = default;

    void execute() override;
};

//------------------------------ External Commands ------------------------------

class ExternalCommand : public Command {
public:
    explicit ExternalCommand(const std::string& cmd_line);

    ~ExternalCommand() override = default;

    void execute() override;
};

class RedirectionCommand : public Command {
public:
    explicit RedirectionCommand(const std::string& cmd_line);

    ~RedirectionCommand() override = default;

    void execute() override;
};

class PipeCommand : public Command {
public:
    explicit PipeCommand(const std::string& cmd_line);

    virtual ~PipeCommand() = default;

    void execute() override;
};

class WatchCommand : public Command {
public:
    WatchCommand(const std::string& cmd_line);

    virtual ~WatchCommand() = default;

    void execute() override;
};






#endif //SMASH_COMMAND_H_
