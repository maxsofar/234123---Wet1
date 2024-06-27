#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlCHandler(int sig_num) {
    // Print the message
    cout << "smash: got ctrl-C" << endl;

    // Get the instance of SmallShell
    SmallShell& shell = SmallShell::getInstance();

    // Get the PID of the foreground process
    pid_t fgPid = shell.getFgPid();

    // Check if fgPid is 0 or -1
    if (fgPid == 0 || fgPid == -1) {
        return;
    }

    // Send the SIGINT signal to the foreground process
    if (kill(fgPid, SIGINT) == 0) {
        // Print the message
        cout << "smash: process " << fgPid << " was killed" << endl;
    }

    // Reset fgPid to -1
    shell.setFgPid(-1);
}
