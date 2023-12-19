#ifndef UTIL_H
#define UTIL_H

#include "emojis.h"
#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <map>
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
#include <unordered_map>
#include <vector>

#define TIME_TICK 1
#define CHILD_LOG_PREFIX "|>\tCHILD PROCESS LOG : "
#define MAX_TIME_TICK 500
#define MIN_CPU_BURST 3
#define MAX_CPU_BURST 6
#define MIN_REBORN_TICK 2
#define MAX_REBORN_TICK 20
#define MEMORY_SIZE 5
#define DISK_SIZE 256

enum KernelCommand {
  EXECUTE_CPU = 0,
  SELECT_CPU = 1,
  DESELECT = 2,
  FORCE_QUIT = 3
};

enum UserCommand {
  MEMORY_REQUEST = 0,
  REBORN = 1,
};

enum ProcessStatus {
  NEW = 0,
  READY = 1,
  RUNNING = 2,
  TERMINATED = 3,
  SHUT_DOWN = 4,
};

struct PartialUserProcess {
  int pid;
  int remainingCpuBurst;
};

unsigned int randomRange(unsigned int min, unsigned int max) {
  if (min > max) {
    std::cerr << "min > max, return 0;" << std::endl
              << "Make sure min <= max when you call this function after."
              << std::endl;
    return 0;
  }

  static std::random_device rd;
  static std::mt19937 rng(rd());
  std::uniform_int_distribution<unsigned int> uni(min, max);

  return uni(rng);
}

std::vector<char> stringToCharVector(const std::string &str) {
  std::vector<char> charVector;
  for (char c : str) {
    charVector.push_back(c);
  }
  return charVector;
}

std::vector<char> randomString() {
  std::string randomEmoji = basicEmojis_4[randomRange(0, 51)];
  return stringToCharVector(randomEmoji);
}

struct message {
  long mtype;
  char mtext[40];
};

template <typename T>
void sendCommand(int msgid, pid_t receiver, int command,
                 const std::vector<T> &additionalParams = {}) {
  message msg;
  memset(msg.mtext, 0, sizeof(msg.mtext));
  msg.mtype = receiver;
  std::ostringstream oss;
  oss << command;
  for (const T &param : additionalParams) {
    oss << " " << param;
  }
  strncpy(msg.mtext, oss.str().c_str(), sizeof(msg.mtext) - 1);
  msg.mtext[sizeof(msg.mtext) - 1] = '\0';

  size_t messageLength = strlen(msg.mtext);
  if (msgsnd(msgid, &msg, messageLength, IPC_NOWAIT) == -1) {
    std::cerr << msgid << " Error sending message. Error code: " << errno
              << std::endl;
    std::cout << msgid << " Error sending message. Error code: " << errno
              << std::endl;
  }
}

template <typename T>
std::pair<int, std::vector<T>> receiveCommand(int msgid, long myPID) {
  message receivedMessage;
  if (msgrcv(msgid, &receivedMessage, sizeof(receivedMessage.mtext), myPID,
             IPC_NOWAIT) < 0) {
    if (errno != ENOMSG)
      std::cerr << "Error receiving message. Error code: " << errno
                << std::endl;
    return std::make_pair(-1, std::vector<T>{});
  }

  std::istringstream iss(receivedMessage.mtext);
  int command;
  if (!(iss >> command)) {
    std::cerr << "Error parsing message." << std::endl;
    return std::make_pair(-1, std::vector<T>{});
  }

  std::vector<T> additionalParams;
  T param;
  while (iss >> param) {
    additionalParams.push_back(param);
  }

  return std::make_pair(command, additionalParams);
}

void clearMessageQueue(int msgid) {
  std::cout << "Clear Message Queue " << msgid << std::endl;
  message msg;
  while (msgrcv(msgid, &msg, sizeof(msg.mtext), 0, IPC_NOWAIT) != -1) {
  }

  if (errno != ENOMSG) {
    std::cerr << "Error in clearing message queue: " << strerror(errno)
              << std::endl;
  }
}

void printTableHeader() {
  printf("\n%-10s | %-10s\n", "PID", "CPU Burst");
  printf("----------------------------------------\n");
}

void printProcess(PartialUserProcess *process) {
  printf("%-10d | %-10d\n", process->pid, process->remainingCpuBurst);
}

void printTableFooter() {
  printf("----------------------------------------\n");
}

#endif // UTIL_H
