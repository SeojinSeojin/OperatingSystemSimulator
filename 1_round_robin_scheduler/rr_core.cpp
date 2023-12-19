#include "kernel.h"
#include "user.h"
#include "utils.h"

int main() {
  key_t key = ftok(MESSAGE_QUEUE_NAME, 65);

  int msgid = msgget(key, 0666 | IPC_CREAT);
  clearMessageQueue(msgid);

  std::vector<PartialUserProcess *> userProcesses;

  pid_t pid;
  for (int i = 0; i < NUM_CHILD_PROCESSES; i++) {
    int randomCpuBurst = randomRange(5, 30);
    pid = fork();
    if (pid == 0) { // child process
      UserProcess userProcess(getpid(), randomCpuBurst, msgid);
      userProcess.run();
      exit(0);
    } else if (pid > 0) { // parent process
      PartialUserProcess *partialUserProcess =
          new PartialUserProcess{pid, randomCpuBurst, 0};
      userProcesses.push_back(partialUserProcess);
    } else {
      std::cerr << ERROR_LOG_PREFIX << "Fork failed" << std::endl;
    }
  }

  KernelProcess kernel(TIME_QUANTUM, userProcesses, msgid);
  kernel.run();
  kernel.exit();

  for (auto *user : userProcesses) {
    delete user;
  }
  clearMessageQueue(msgid);

  return 0;
}