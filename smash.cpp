#include <iostream>
//#include <unistd.h>
//#include <sys/wait.h>
#include <signal.h>
#include <algorithm>
#include "Commands.h"
#include "signals.h"


int main(int argc, char *argv[]) {
    if (signal(SIGINT, ctrlCHandler) == SIG_ERR) {
        perror("smash error: failed to set ctrl-C handler");
    }

    //TODO: setup sig alarm handler

    SmallShell &smash = SmallShell::getInstance();
    while (true) {
        try {
            std::cout << smash.getPrompt() << "> ";
            std::string cmd_line;
            std::getline(std::cin, cmd_line);

            // Check if cmd_line is empty or contains only whitespace
            if (!cmd_line.empty() && !std::all_of(cmd_line.begin(), cmd_line.end(), ::isspace)) {
                smash.executeCommand(cmd_line);
            }
        } catch (const QuitException &e) {
            break;
        }

    }
    return 0;
}
