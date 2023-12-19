#ifndef UTIL_H
#define UTIL_H

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <optional>
#include <queue>
#include <random>
#include <sstream>
#include <string>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/time.h>
#include <thread>
#include <unistd.h>
#include <vector>

#define IO_BURST_PROBABILITY 80
#define NUM_CHILD_PROCESSES 10
#define TIME_QUANTUM 10
#define TIME_TICK 1
#define MESSAGE_QUEUE_NAME  "/message_queue"
#define CHILD_LOG_PREFIX "|>\tCHILD PROCESS LOG : "
#define ERROR_LOG_PREFIX "xxx ERROR xxx : "

enum ParentCommand {
    EXECUTE_CPU = 0,
    EXECUTE_IO = 1,
    SELECT_CPU = 2,
    DESELECT = 4
};

enum ChildCommand {
    EARLY_CPU_FINISH = 0,
    IO_REQUEST = 1,
    EARLY_IO_FINISH = 2,
};

enum ProcessStatus {
    NEW = 0,
    READY = 1,
    RUNNING = 2,
    TERMINATED = 3,
    WAITING = 4
};

struct PartialUserProcess {
    int pid;
    int remainingCpuBurst;
    int remainingIoBurst;
};

unsigned int randomRange(unsigned int min, unsigned int max) {
    if (min > max) {
        std::cerr << ERROR_LOG_PREFIX << "min > max, return 0;" << std::endl
                  << ERROR_LOG_PREFIX << "Make sure min <= max when you call this function after." << std::endl;
        return 0;
    }

    static std::random_device rd;
    static std::mt19937 rng(rd());
    std::uniform_int_distribution<unsigned int> uni(min, max);

    return uni(rng);
}

bool randomProbability(int probability) {
    unsigned int randomNum = randomRange(0,100);

    return randomNum < probability;
}

struct message {
    long mtype; 
    char mtext[100]; 
};

void sendCommand(int msgid, pid_t receiver, int command, const std::vector<int>& additionalParams = {}) {
    message msg;
    memset(msg.mtext, 0, sizeof(msg.mtext)); 
    msg.mtype = receiver;
    std::ostringstream oss;
    oss << command; 
    for (int param : additionalParams) {
        oss << " " << param; 
    }
    strncpy(msg.mtext, oss.str().c_str(), sizeof(msg.mtext) - 1);
    msg.mtext[sizeof(msg.mtext) - 1] = '\0';

    size_t messageLength = strlen(msg.mtext);
    if (msgsnd(msgid, &msg, messageLength, IPC_NOWAIT) == -1) {
        std::cerr << ERROR_LOG_PREFIX << "Error sending message. Error code: " << errno << std::endl;
    }
}

std::vector<int> receiveCommand(int msgid, long myPID) {
    message receivedMessage;
    if (msgrcv(msgid, &receivedMessage, sizeof(receivedMessage.mtext), myPID, IPC_NOWAIT) < 0) {
        if(errno != ENOMSG) std::cerr << ERROR_LOG_PREFIX << "Error receiving message. Error code: " << errno << std::endl;
        return {};
    }

    std::istringstream iss(receivedMessage.mtext);
    int command;
    if (!(iss >> command)) {
        std::cerr << ERROR_LOG_PREFIX << "Error parsing message." << std::endl;
        return {};
    }

    std::vector<int> allParams = {command};
    int param;
    while (iss >> param) {
        allParams.push_back(param);
    }
    return allParams;
}

void clearMessageQueue(int msgid) {
    message msg;
    while (msgrcv(msgid, &msg, sizeof(msg.mtext), 0, IPC_NOWAIT) != -1) { }

    if (errno != ENOMSG) {
        std::cerr << ERROR_LOG_PREFIX << "Error in clearing message queue: " << strerror(errno) << std::endl;
    }
}

void printTableHeader() {
    printf("\n%-10s | %-10s | %-10s\n", "PID", "CPU Burst", "IO Burst");
    printf("----------------------------------------\n");
}

void printProcess(PartialUserProcess* process) {
    printf("%-10d | %-10d | %-10d\n", process->pid, process->remainingCpuBurst, process->remainingIoBurst);
}

void printTableFooter() {
    printf("----------------------------------------\n");
}

void printQueue(std::queue<PartialUserProcess*> queue) {
    std::queue<PartialUserProcess*> queueCopy = queue;
    while(!queueCopy.empty()) {
        PartialUserProcess *process = queueCopy.front();
        printProcess(process);
        queueCopy.pop();
    }
}

#endif // UTIL_H
