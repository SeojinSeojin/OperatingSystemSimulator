#ifndef KERNEL_H
#define KERNEL_H

#include "utils.h"

class IoHandler {
    public: 
        IoHandler(int msgid): msgid(msgid) {};

        void addProcess(PartialUserProcess* process) {
            ioQueue.push(process);
        }

        std::queue<PartialUserProcess*> ioHandlerOnTick() {
            std::queue<PartialUserProcess*> queueTemp;
            std::queue<PartialUserProcess*> ioFinishedQueue;
            while(!ioQueue.empty()) {
                PartialUserProcess *targetIoProcess = ioQueue.front();
                ioQueue.pop();
                targetIoProcess->remainingIoBurst--;
                sendCommand(msgid, targetIoProcess->pid, ParentCommand::EXECUTE_IO,{TIME_TICK});
                if(targetIoProcess->remainingIoBurst > 0) {
                    queueTemp.push(targetIoProcess);
                } else {
                    sendCommand(msgid, targetIoProcess->pid, ParentCommand::DESELECT);
                    if(targetIoProcess->remainingCpuBurst>0) ioFinishedQueue.push(targetIoProcess);
                }
            }
            ioQueue = queueTemp;
            return ioFinishedQueue;
        }

        void logInfo() {
            std::cout << "[IO Queue Info]";
            if(!ioQueue.empty()) {
                printTableHeader();
                printQueue(ioQueue);
                printTableFooter();
            } else {
                std::cout << " : Empty" << std::endl;
            }
        }

        bool isEmpty() {
            return ioQueue.empty();
        }

    private :
        int msgid;
        std::queue<PartialUserProcess*> ioQueue;
};

class KernelProcess {
public:
    KernelProcess(int timeQuantum, std::vector<PartialUserProcess*> childProcesses, int msgid) : timeQuantum(timeQuantum), msgid(msgid), childProcesses(childProcesses), ioHandler(IoHandler(msgid)) {
        for(auto& child : childProcesses) {
            readyQueue.push(child);
        }
     }

    void logInfo() {
        std::cout << std::endl;
        std::cout << "-+-" << std::endl;
        std::cout << " | ime Tick T :" << totalTimePassed << std::endl;
        std::cout << " |" << std::endl;
        if(currentCpuProcess) std::cout << "[PID of Process in Running State] " << currentCpuProcess->pid << std::endl;
        
        std::cout << "[Running Processes Info]";
        if(currentCpuProcess) {
            printTableHeader();
            printProcess(currentCpuProcess);
            printTableFooter();
        } else {
            std::cout << " : None" << std::endl;
        }
                
        std::cout << "[Ready Queue Info]";
        if(!readyQueue.empty()) {
            printTableHeader();
            printQueue(readyQueue);
            printTableFooter();
        } else {
            std::cout << " : Empty" << std::endl;
        }

        ioHandler.logInfo();
    }

    void tickHandler() {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        std::cout << "--Parent Process INIT--" << std::endl;
        logInfo();

        while(!readyQueue.empty() || !ioHandler.isEmpty() || currentCpuProcess) {
            msgHandlerOnTick();
            cpuHandlerOnTick();
            ioHandlerOnTick();

            totalTimePassed++;
            currentCpuTimePassed++;

            logInfo();
            std::this_thread::sleep_for(std::chrono::milliseconds(TIME_TICK*80));
        }
        std::cout << "All Task Finished!" << std::endl;
    }

    void run() {
        thread = std::thread(&KernelProcess::tickHandler, this);
        thread.join();
    }

    void stat() {
        std::cout << "--------------------STAT--------------------" << std::endl;
        printf("\t%-20s | %-10d\n", "Average Response Time", totalResponseTime / NUM_CHILD_PROCESSES);
        printf("\t%-20s | %-10d\n", "Total Execution Time ", totalTimePassed);
        std::cout << "--------------------------------------------" << std::endl;
    }

    void exit () {
        for (auto& child : childProcesses) {
            waitpid(child->pid, nullptr, 0);
        }
        stat();
    }

private:
    unsigned totalTimePassed=0;
    unsigned totalResponseTime=0;
    unsigned currentCpuTimePassed=0;

    PartialUserProcess* currentCpuProcess=NULL;
    std::queue<PartialUserProcess*> readyQueue;
    std::vector<PartialUserProcess*> childProcesses;
    int timeQuantum;
    int msgid;
    std::thread thread;
    IoHandler ioHandler;
    
    void commandHandler(int command, std::vector<int> additionalParams={}) {
        switch(command) {
            case ChildCommand::EARLY_CPU_FINISH:
            case ChildCommand::EARLY_IO_FINISH:
                break;
            case ChildCommand::IO_REQUEST:
                if(additionalParams.empty()) break;
                int ioBurst = additionalParams.front();
                std::cout << "IO Burst(" << ioBurst << ") Requested by  : " << currentCpuProcess->pid << std::endl;
                currentCpuProcess->remainingIoBurst = ioBurst;
                ioHandler.addProcess(currentCpuProcess);
                currentCpuProcess = NULL;
                break;
        }
    }
    
    void cpuHandlerOnTick() {
        if(currentCpuProcess && currentCpuProcess->remainingCpuBurst<=0) {
            sendCommand(msgid, currentCpuProcess->pid, ParentCommand::DESELECT);
            currentCpuProcess = NULL;
        }
        if(currentCpuProcess && currentCpuTimePassed % timeQuantum == 0) {
            std::cout << "current cpu time " << currentCpuTimePassed << " passed. switch from pid: " << currentCpuProcess->pid << std::endl;
            sendCommand(msgid, currentCpuProcess->pid, ParentCommand::DESELECT);
            if(currentCpuProcess->remainingCpuBurst > 0) {
                readyQueue.push(currentCpuProcess);
            }
            currentCpuProcess = NULL;
            currentCpuTimePassed = 0;
        }
        if(currentCpuProcess) {
            currentCpuProcess->remainingCpuBurst--;
            sendCommand(msgid, currentCpuProcess->pid, ParentCommand::EXECUTE_CPU, {TIME_TICK});
            totalResponseTime += readyQueue.size(); // readyQueue.size() == number of processes waiting for CPU
        }
        if(!readyQueue.empty() && currentCpuProcess == NULL) {
            currentCpuProcess = readyQueue.front(); 
            readyQueue.pop();
            std::cout << "Switch Current CPU Process PID : " << currentCpuProcess->pid << std::endl;
            sendCommand(msgid, currentCpuProcess->pid, ParentCommand::SELECT_CPU);
            currentCpuTimePassed = 0;
        }
    }

    void ioHandlerOnTick() {
        std::queue<PartialUserProcess*> ioFinishedProcesses = ioHandler.ioHandlerOnTick();
        while(!ioFinishedProcesses.empty()) {
            readyQueue.push(ioFinishedProcesses.front());
            ioFinishedProcesses.pop();
        }
    }

    void msgHandlerOnTick() {
        std::vector<int> newMessage = receiveCommand(msgid, getpid());
        if(!newMessage.empty()) { 
            std::cout << "Parent received command " << newMessage.front() << std::endl;
            std::vector<int> additionalParams(newMessage.begin() + 1, newMessage.end());
            commandHandler(newMessage.front(), additionalParams); 
        }
    }
};

#endif // KERNEL_H
