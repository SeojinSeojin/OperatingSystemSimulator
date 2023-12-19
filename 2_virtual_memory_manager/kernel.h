#ifndef KERNEL_H
#define KERNEL_H
#include "mm.h"
#include "utils.h"

class KernelProcess {
public:
  KernelProcess(std::vector<PartialUserProcess *> userProcess, int msgid_str,
                int msgid_int)
      : msgid_str(msgid_str), msgid_int(msgid_int), userProcesses(userProcess),
        memoryManager() {
    for (auto &user : userProcess) {
      readyQueue.push(user);
    }
  }

  void printQueue(std::queue<PartialUserProcess *> queue) {
    std::queue<PartialUserProcess *> queueCopy = queue;
    while (!queueCopy.empty()) {
      PartialUserProcess *process = queueCopy.front();
      printProcess(process);
      queueCopy.pop();
    }
  }

  void logInfo() {
    std::cout << std::endl;
    std::cout << "-+-" << std::endl;
    std::cout << " | ime Tick T :" << totalTimePassed << std::endl;
    std::cout << " | " << std::endl;
    if (currentCpuProcess)
      std::cout << "[PID of Process in Running State] "
                << currentCpuProcess->pid << std::endl;

    std::cout << "[Running Processes Info]";
    if (currentCpuProcess) {
      printTableHeader();
      printProcess(currentCpuProcess);
      printTableFooter();
    } else {
      std::cout << " : None" << std::endl;
    }

    std::cout << "[Ready Queue Info]";
    if (!readyQueue.empty()) {
      printTableHeader();
      printQueue(readyQueue);
      printTableFooter();
    } else {
      std::cout << " : Empty" << std::endl;
    }
  }

  void commandStrHandler(int command, std::vector<char> additionalParams = {}) {
    switch (command) {
    case UserCommand::MEMORY_REQUEST:
      memoryManager.writeToVirtualAddress(additionalParams,
                                          currentCpuProcess->pid);
      break;
    }
  }
  void commandIntHandler(int command, std::vector<int> additionalParams = {}) {
    switch (command) {
    case UserCommand::REBORN:
      PartialUserProcess *rebornProcess = new PartialUserProcess(
          {additionalParams.at(0), additionalParams.at(1)});
      readyQueue.push(rebornProcess);
    }
  }

  void cpuHandlerOnTick() {
    if (currentCpuProcess && currentCpuProcess->remainingCpuBurst <= 0) {
      sendCommand<int>(msgid_int, currentCpuProcess->pid,
                       KernelCommand::DESELECT);
      currentCpuProcess = NULL;
    }
    if (currentCpuProcess) {
      currentCpuProcess->remainingCpuBurst--;
      sendCommand<int>(msgid_int, currentCpuProcess->pid,
                       KernelCommand::EXECUTE_CPU);
    }
    if (!readyQueue.empty() && currentCpuProcess == NULL) {
      currentCpuProcess = readyQueue.front();
      readyQueue.pop();
      int va = memoryManager.getVirtualAddress(currentCpuProcess->pid);
      std::cout << "CONTEXT SWITCH! New CPU Process PID : "
                << currentCpuProcess->pid << " , with VA" << va << std::endl;
      sendCommand<int>(msgid_int, currentCpuProcess->pid,
                       KernelCommand::SELECT_CPU);
      currentCpuTimePassed = 0;
    }
  }

  void msgIntHandlerOnTick() {
    auto [command, additionalParams] = receiveCommand<int>(msgid_int, getpid());
    if (command != -1) {
      std::cout << "[1] Parent received command " << command << std::endl;
      commandIntHandler(command, additionalParams);
    }
  }

  void msgStrHandlerOnTick() {
    auto [command, additionalParams] =
        receiveCommand<char>(msgid_str, getpid());
    if (command != -1) {
      std::cout << "[2] Parent received command " << command << std::endl;
      commandStrHandler(command, additionalParams);
    }
  }

  void tickHandler() {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::cout << "Parent Process INIT" << std::endl;
    logInfo();
    while (totalTimePassed < MAX_TIME_TICK) {
      msgIntHandlerOnTick();
      msgStrHandlerOnTick();
      cpuHandlerOnTick();

      totalTimePassed++;
      currentCpuTimePassed++;

      logInfo();
      memoryManager.logMemoryMapping();
      std::this_thread::sleep_for(std::chrono::milliseconds(TIME_TICK * 250));
    }
    std::cout << MAX_TIME_TICK << " Passed!" << std::endl;
    memoryManager.logPageFaultCnt();
  }

  void run() {
    thread = std::thread(&KernelProcess::tickHandler, this);
    thread.join();
  }

  void exit() {
    for (auto &user : userProcesses) {
      sendCommand<int>(msgid_int, user->pid, KernelCommand::FORCE_QUIT);
      waitpid(user->pid, nullptr, 0);
    }
  }

private:
  MemoryManager memoryManager;
  unsigned totalTimePassed = 0;
  unsigned currentCpuTimePassed = 0;
  PartialUserProcess *currentCpuProcess = NULL;
  std::queue<PartialUserProcess *> readyQueue;
  std::vector<PartialUserProcess *> userProcesses;
  int msgid_str;
  int msgid_int;
  std::thread thread;
};

#endif // KERNEL_H
