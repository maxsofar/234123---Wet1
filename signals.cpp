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

    // Send the SIGKILL signal to the foreground process
    if (kill(fgPid, SIGKILL) == 0) {
        // Print the message
        cout << "smash: process " << fgPid << " was killed" << endl;
    } else {
        perror("smash error: failed to send signal");
    }
}
