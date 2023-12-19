#ifndef USER_H
#define USER_H
#include "utils.h"

class PCB {
public:
  pid_t pid;
  int cpuBurst;
  PCB(pid_t pid, unsigned int cpuBurst) : pid(pid), cpuBurst(cpuBurst) {}
};

class UserProcess {
public:
  UserProcess(pid_t pid, int cpuBurst, int msgid_str, int msgid_int)
      : status(ProcessStatus::READY), pcb(pid, cpuBurst), msgid_str(msgid_str),
        msgid_int(msgid_int) {
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
  int msgid_str;
  int msgid_int;
  PCB pcb;

  void tickHandler() {
    while (status != ProcessStatus::SHUT_DOWN) {
      auto [command, additionalParams] =
          receiveCommand<int>(msgid_int, pcb.pid);
      if (command != -1) {
        std::cout << CHILD_LOG_PREFIX << "Child " << pcb.pid
                  << " received command " << command << std::endl;
        commandHandler(command);
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(TIME_TICK * 250));
      if (status == ProcessStatus::TERMINATED) {
        if (rebornTime > 0) {
          rebornTime--;
        } else {
          status = ProcessStatus::READY;
          pcb = PCB(pcb.pid, randomRange(MIN_CPU_BURST, MAX_CPU_BURST));
          std::cout << CHILD_LOG_PREFIX << "Child " << pcb.pid
                    << " send reborn signal with CPU Burst " << pcb.cpuBurst
                    << std::endl;
          sendCommand<int>(msgid_int, getppid(), UserCommand::REBORN,
                           {pcb.pid, pcb.cpuBurst});
        }
      }
    }
    std::cout << CHILD_LOG_PREFIX << "Child " << pcb.pid << " Terminated"
              << std::endl;
  }

  void commandHandler(int command) {
    if (command != -1) {
      switch (command) {
      case KernelCommand::SELECT_CPU:
        onSelectCPU();
        break;
      case KernelCommand::EXECUTE_CPU:
        onExecuteCPU();
        break;
      case KernelCommand::DESELECT:
        onDeselect();
        break;
      case KernelCommand::FORCE_QUIT:
        status = ProcessStatus::SHUT_DOWN;
        break;
      }
    }
  }

  void onSelectCPU() {
    std::cout << CHILD_LOG_PREFIX << "Child[onSelectCPU] ";
    logInfo();
    status = ProcessStatus::RUNNING;
  }

  void onExecuteCPU() {
    if (status != ProcessStatus::RUNNING) {
      std::cerr << CHILD_LOG_PREFIX
                << "This is never selected by parent, pid: " << pcb.pid
                << std::endl;
      return;
    }
    pcb.cpuBurst -= 1;
    if (pcb.cpuBurst >= 1) {
      std::vector<char> randomEmoji = randomString();
      sendCommand<char>(msgid_str, getppid(), UserCommand::MEMORY_REQUEST,
                        randomEmoji);
      std::cout << CHILD_LOG_PREFIX << "Child(" << pcb.pid << ") writes ";
      for (char c : randomEmoji) {
        std::cout << c;
      }
      std::cout << std::endl;
    }
  }

  void onDeselect() {
    status = ProcessStatus::TERMINATED;
    rebornTime = randomRange(MIN_REBORN_TICK, MAX_REBORN_TICK);
    std::cout << CHILD_LOG_PREFIX << "Child[onDeselect] " << pcb.pid
              << " will reborn after " << rebornTime << " ticks" << std::endl;
  }

  std::thread thread;
  int rebornTime = 0;
};

#endif // USER_H
