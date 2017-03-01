#include "info.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

FILE *writeFile;
int shmid;
int *numbers;
int size;

void process(int start, int end, info_struct *info, int depth){
  fprintf(writeFile, "hi, i\'m process %d and my parent is %d\n", getpid(), getppid());
  
  if (depth <= 0){
    while(start <= end){
      int number = numbers[start];
      if (number < info->min) info->min = number;
      if (number > info->max) info->max = number;
      info->sum += number;
      info->count++;
      start++;
    }
    return;
  }

  int cutoff = (start+end)/2;

  int fd[2];
  pipe(fd);
  pid_t childpid;

  fflush(writeFile);
  childpid = fork();

  if (childpid == 0){
    close(fd[0]);
    process(cutoff+1, end, info, depth-1);
    write(fd[1], info, sizeof(info_struct));
    exit(0);
  }
  else{
    close(fd[1]);
    process(start, cutoff, info, depth-1);

    info_struct new_info;
    read(fd[0], &new_info, sizeof(info_struct));
    if (new_info.min < info->min) info->min = new_info.min;
    if (new_info.max > info->max) info->max = new_info.max;
    info->sum += new_info.sum;
    info->count += new_info.count;

    waitpid(childpid);
  }
}

int main(int argc, char *argv[]){
  struct timeval start,end;
  gettimeofday(&start,NULL);

  shmid = shmget(IPC_PRIVATE, 1000000*sizeof(int), 0666 | IPC_CREAT);
  numbers = shmat(shmid, 0, 0);

  FILE *readFile = fopen(argv[1], "r");
  writeFile = fopen(argv[2], "w+");
  int depth = atoi(argv[3]);
  int numProcesses = 1;
  int iter = depth-1;
  while(iter > 0){
    numProcesses *= 2;
    iter--;
  }

  size = 0;
  while(fscanf(readFile, "%d\n", &numbers[size++]) != EOF);

  info_struct info = {.min = INT_MAX, .max = INT_MIN, .sum = 0, .count = 0};
  process(0, size-1, &info, depth-1);

  fprintf(writeFile, "min: %d\n", info.min);
  fprintf(writeFile, "max: %d\n", info.max);
  fprintf(writeFile, "sum: %d\n", info.sum);

  fclose(readFile);
  fclose(writeFile);

  shmdt((void *) numbers);
  shmctl(numbers, IPC_RMID, NULL);

  gettimeofday(&end,NULL);
  printf("%ld microseconds\n", (end.tv_sec*100000 + end.tv_usec) - (start.tv_sec*100000 + start.tv_usec));

  return 0;
}