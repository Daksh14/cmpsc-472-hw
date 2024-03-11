// Process manager by Daksh Upadhyay
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <pthread.h>
#include <stdlib.h>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

std::vector<std::string> process_names;

pthread_mutex_t lock;

// Create an individual process
struct Process {
  int num_of_threads;
  // 3 threads per process
  pthread_t threads[4];
};

// Create the manager struct
struct ProcessManager {
  // max 2 processes
  Process process[2];
  // pid(s) for the processes
  pid_t pid[2];
  // the callback for the thread
  static void *thread_callback(void *i) {
    std::cout << "Simulating shared memory with mutex" << std::endl;
    pthread_mutex_lock(&lock);
    char buff[100];

    snprintf(buff, sizeof(buff), "From thread: %p\n", pthread_self());

    std::string buffAsStdStr = buff;

    process_names.push_back(buffAsStdStr);

    pthread_mutex_unlock(&lock);
  };

  static void *thread_callback_with_message_passed(void *i) {
    int read_descriptor = *((int *)i);

    char concat_str[100];
    // wait for message to be sent
    sleep(1);
    read(read_descriptor, concat_str, 100);
    std::cout << "Recieved from message passing: " << concat_str << std::endl;
    // close reading end
    close(read_descriptor);
    // de allocate the read descriptor
    free(i);
  }

  int ProcessManager_create(int num_of_threads) {
    // create TWO child processes to keep IPC as simple as possible
    if (fork() == 0) {
      pid_t current_pid = getpid();
      pid[0] = current_pid;
      // child process
      // record time
      std::cout << "Init Child process, PID from fork: " << current_pid
                << std::endl;

      for (int i = 0; i < num_of_threads; i++) {
        auto t0 = std::chrono::steady_clock::now();
        pthread_create(&process[0].threads[i], NULL,
                       ProcessManager::thread_callback, NULL);

        pthread_join(process[0].threads[i], NULL);
        auto t1 = std::chrono::steady_clock::now();
        std::chrono::duration<uint64_t, std::nano> elapsed = t1 - t0;
        uint64_t duration = elapsed.count();

        std::cout << "Thread ended, took duration: with shared memory "
                  << duration << std::endl;
      }
      std::cout << "Allocated memory for child process one " << std::endl;

      for (auto const &c : process_names)
        std::cout << c << ' ';

      exit(0);
    } else {
      if (fork() == 0) {

        pid_t current_pid = getpid();
        pid[1] = current_pid;

        std::cout << "Init Second Child process, PID from fork: " << current_pid
                  << std::endl;
        for (int i = 0; i < num_of_threads; i++) {
          auto t0 = std::chrono::steady_clock::now();
          int fd[2];
          pipe(fd);

          int *args = (int *)malloc(sizeof(*args));
          *args = fd[0];

          pthread_create(&process[1].threads[i], NULL,
                         ProcessManager::thread_callback_with_message_passed,
                         args);

          char *input_str = "This is a message sent";

          write(fd[1], input_str, strlen(input_str) + 1);
          close(fd[1]);

          pthread_join(process[1].threads[i], NULL);

          auto t1 = std::chrono::steady_clock::now();
          std::chrono::duration<uint64_t, std::nano> elapsed = t1 - t0;
          uint64_t duration = elapsed.count();

          std::cout << "Thread ended, took duration: with message passing: "
                    << duration << std::endl;
        }
        exit(0);
      }
    }

    // wait for the processes to finish
    while (wait(NULL) > 0)
      ;

    return 0;
  }

  int ProcessManager_destroy() {
    // destroy the processes
    for (int i = 0; i < 2; i++) {
      for (int j = 0; j < 3; j++) {
        pthread_cancel(process[i].threads[j]);
      }
    }

    return 0;
  }
};

int main(int argc, char *argv[]) {

  ProcessManager pm;
  // create the process manager with 2 processes and 3 threads for each process
  pm.ProcessManager_create(3);

  std::cout << "type ctrl-c to exit anytime" << std::endl;

  std::cout << "Sleeping for 10 seconds to wait for process to finish then "
               "calling destory"
            << std::endl;

  sleep(10);

  pm.ProcessManager_destroy();

  std::cout << "Type name of file to process parallely" << std::endl;

  std::string line;

  std::getline(std::cin, line);

  FILE *pFile;
  long lSize;
  char *buffer;
  size_t result;

  pFile = fopen(line.c_str(), "rb");
  if (pFile == NULL) {
    fputs("File error", stderr);
    exit(1);
    return 0;
  }

  // obtain file size:
  fseek(pFile, 0, SEEK_END);
  lSize = ftell(pFile);
  rewind(pFile);

  // allocate memory to contain the whole file:
  buffer = (char *)malloc(sizeof(char) * lSize);
  if (buffer == NULL) {
    fputs("Memory error", stderr);
    exit(2);
  }

  // copy the file into the buffer:
  result = fread(buffer, 1, lSize, pFile);
  if (result != lSize) {
    fputs("Reading error", stderr);
    exit(3);
  }

  std::cout << "File loaded parallely using fread" << std::endl;

  std::cout << "Exiting, thank you" << std::endl;

  // terminate
  fclose(pFile);
  free(buffer);

  return 0;
}
