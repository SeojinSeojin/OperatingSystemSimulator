#include "utils.h"
#include "kernel.h"
#include "user.h"


int main () {
    int msgid_str = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);
    clearMessageQueue(msgid_str);
    int msgid_int = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);
    clearMessageQueue(msgid_int);
    
    std::vector<PartialUserProcess*> childProcesses;

    std::cout << "msgid_str" << msgid_str << "msgid_int" << msgid_int << std::endl;
    
    pid_t pid;
    for (int i = 0; i < 10; i++) {
        int randomCpuBurst = randomRange(MIN_CPU_BURST, MAX_CPU_BURST);
        pid = fork();
        if (pid == 0) { // child process
            UserProcess userProcess(getpid(), randomCpuBurst, msgid_str, msgid_int);
            userProcess.run();
            exit(0);
        } else if (pid > 0) { // kernel process
            PartialUserProcess* partialChildProcess = new PartialUserProcess{pid, randomCpuBurst};
            childProcesses.push_back(partialChildProcess);
        } else {
            std::cerr << "Fork failed" << std::endl;
        }
    }
    
    KernelProcess kernel(childProcesses, msgid_str, msgid_int);
    kernel.run();
    kernel.exit();

    for (auto* child : childProcesses) {
        delete child;
    }
    clearMessageQueue(msgid_str);
    clearMessageQueue(msgid_int);
    msgctl(msgid_str, IPC_RMID, NULL);
    msgctl(msgid_int, IPC_RMID, NULL);

    return 0;
}