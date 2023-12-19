#ifndef USER_H
#define USER_H
#include "utils.h"

class IOBurst {
public:
  IOBurst(unsigned int maxStartTime)
      : startTime(randomRange(1, maxStartTime)),
        executeTime(randomRange(1, 20)), requested(false) {
    std::cout << CHILD_LOG_PREFIX << "IO Burst Created!" << std::endl;
    std::cout << CHILD_LOG_PREFIX << "\t Start Time : " << startTime
              << std::endl;
    std::cout << CHILD_LOG_PREFIX << "\t Execute Time : " << executeTime
              << std::endl;
  }
  int startTime;
  int executeTime;
  bool requested;
};

class PCB {
public:
  pid_t pid;
  int cpuBurst;
  std::optional<IOBurst> ioBurst;
  PCB(pid_t pid, unsigned int cpuBurst) : pid(pid), cpuBurst(cpuBurst) {}
};

class UserProcess {
public:
  UserProcess(pid_t pid, int cpuBurst, int msgid)
      : status(ProcessStatus::READY), pcb(pid, cpuBurst), msgid(msgid) {
    std::cout << CHILD_LOG_PREFIX << "Child Process Created!" << std::endl;
    std::cout << CHILD_LOG_PREFIX << "\t PID : " << pcb.pid << std::endl;
    std::cout << CHILD_LOG_PREFIX << "\t CPU Burst : " << pcb.cpuBurst
              << std::endl;
  }

  void logInfo() {
    std::cout << "pid(" << pcb.pid << ") : "
              << "[" << this->status << "] " << pcb.cpuBurst << std::endl;
  }

  void run() {
    std::cout << CHILD_LOG_PREFIX << "Child Process " << pcb.pid
              << " listen to message queue" << std::endl;
    thread = std::thread(&UserProcess::tickHandler, this);
    thread.join();
  }

private:
  int status;
  int msgid;
  PCB pcb;

  void tickHandler() {
    while (status != ProcessStatus::TERMINATED) {
      std::vector<int> newMessage = receiveCommand(msgid, pcb.pid);
      if (!newMessage.empty()) {
        std::cout << CHILD_LOG_PREFIX << "Child " << pcb.pid
                  << " received command " << newMessage.front() << std::endl;
        std::vector<int> additionalParams(newMessage.begin() + 1,
                                          newMessage.end());
        commandHandler(newMessage.front(), additionalParams);
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(TIME_TICK * 80));
    }
    std::cout << CHILD_LOG_PREFIX << "Child " << pcb.pid << " Terminated"
              << std::endl;
  }

  void commandHandler(int command, std::vector<int> additionalParams = {}) {
    if (command != -1) {
      switch (command) {
      case ParentCommand::SELECT_CPU:
        onSelectCPU();
        break;
      case ParentCommand::EXECUTE_CPU:
        onExecuteCPU(additionalParams.front());
        break;
      case ParentCommand::EXECUTE_IO:
        onExecuteIO(additionalParams.front());
        break;
      case ParentCommand::DESELECT:
        onDeselect();
        break;
      }
    }
  }

  void randomGenerateIoBurst() {
    if (pcb.ioBurst == std::nullopt &&
        randomProbability(IO_BURST_PROBABILITY)) {
      pcb.ioBurst = IOBurst(std::min(pcb.cpuBurst, TIME_QUANTUM) - 2);
    }
  }

  void onSelectCPU() {
    std::cout << CHILD_LOG_PREFIX << "Child[onSelectCPU] ";
    logInfo();
    status = ProcessStatus::RUNNING;
    if (pcb.cpuBurst > 2) {
      randomGenerateIoBurst();
    }
  }

  void onExecuteCPU(int time) { executeCPU(time); }

  void executeCPU(int time = TIME_TICK) {
    if (status != ProcessStatus::RUNNING) {
      std::cerr << ERROR_LOG_PREFIX
                << "This is never selected by parent, pid: " << pcb.pid
                << std::endl;
      return;
    }
    int runningTime = std::min(time, pcb.cpuBurst);
    pcb.cpuBurst -= runningTime;

    if (pcb.ioBurst) {
      pcb.ioBurst->startTime -= runningTime;
      if (pcb.ioBurst->startTime <= 0 && !pcb.ioBurst->requested) {
        std::cout << CHILD_LOG_PREFIX
                  << "Send IO Burst Request, pid: " << pcb.pid << std::endl;
        sendCommand(msgid, getppid(), ChildCommand::IO_REQUEST,
                    {pcb.ioBurst->executeTime});
        pcb.ioBurst->requested = true;
      }
    }
    if (runningTime != time) {
      sendCommand(msgid, getppid(), ChildCommand::EARLY_CPU_FINISH);
    }
    if (pcb.ioBurst == std::nullopt && pcb.cpuBurst <= 0) {
      status = ProcessStatus::TERMINATED;
    }
  }

  void onExecuteIO(int time = TIME_TICK) {
    status = ProcessStatus::WAITING;
    executeIO(time);
  }

  void executeIO(int time) {
    if (pcb.ioBurst == std::nullopt) {
      std::cerr << ERROR_LOG_PREFIX
                << "IO Burst Executed without IO Burst, pid: " << pcb.pid
                << std::endl;
      return;
    }
    int runningTime = std::min(time, pcb.ioBurst->executeTime);
    pcb.ioBurst->executeTime -= runningTime;

    if (pcb.ioBurst->executeTime <= 0) {
      std::cout << "IO Burst Finished, pid: " << pcb.pid << std::endl;
      pcb.ioBurst = std::nullopt;
      if (pcb.cpuBurst <= 0) {
        status = ProcessStatus::TERMINATED;
      } else {
        status = ProcessStatus::READY;
      }
    }

    if (runningTime != time) {
      sendCommand(msgid, getppid(), ChildCommand::EARLY_IO_FINISH);
    }
  }

  void onDeselect() {
    if (pcb.cpuBurst <= 0 && pcb.ioBurst == std::nullopt) {
      status = ProcessStatus::TERMINATED;
    } else {
      status = ProcessStatus::READY;
    }
  }

  std::thread thread;
};

#endif // USER_H
