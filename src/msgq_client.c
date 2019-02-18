#include <signal.h> 
#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include "key.h"
#include "client.h"

void sigquit() { 
    exit(0); 
} 

int main(void) {
  struct my_msgbuf buf;
  struct client clt;

  // establish connection with server
  int msqid;
  key_t key;
  if ((key = ftok(MSGQ_PATH, 'D')) == -1) {
    perror("ftok");
    exit(1);
  }
  // FIXME: remove IPC_CREAT
  if ((msqid = msgget(key, IPC_CREAT | 0644)) == -1) {
    perror("msgget");
    exit(1);
  }
  printf("Connected to server message queue\n");

  // init client
  printf("Enter your name: ");
  scanf("%s", clt.cname);
  if ((clt.cmsqid = msgget(IPC_PRIVATE, IPC_CREAT | 0644)) == -1) {
    perror("msgget");
    exit(1);
  }

  // server handshake
  buf.mtype = 1;
  sprintf(buf.mtext, "%ld$%s", clt.cmsqid, clt.cname);
  if (msgsnd(msqid, &(buf.mtype), sizeof(buf), 0) == -1) {
    perror("msgsnd");
    msgctl(msqid, IPC_RMID, NULL);
    exit(1);
  }
  if (msgrcv(clt.cmsqid, &(buf.mtype), sizeof(buf), 0, 0) == -1) {
    perror("msgrcv");
    msgctl(msqid, IPC_RMID, NULL);
    exit(1);
  }
  if (buf.mtype != 1) {
    printf("server handshake error: %s", buf.mtext);
    exit(1);
  } else {
    sscanf(buf.mtext, "%d", &(clt.cid));
    buf.cid = clt.cid;
    printf("Successfully connected to server\n");
  }

  // start communication with server (command mode)
  buf.mtype = 2;
  printf("Enter commands (crgrp, lsgrp, jngrp), ^D to quit:\n");
  while (fgets(buf.mtext, MAX_MTEXT, stdin) != NULL) {
    buf.mtext[strcspn(buf.mtext, "\n")] = 0;  // remove trailing newline char
    if (msgsnd(msqid, &(buf.mtype), sizeof(buf), 0) == -1) {
      perror("msgsnd");
      continue;
    }
    if (msgrcv(clt.cmsqid, &(buf.mtype), sizeof(buf), 0, 0) == -1) {
      perror("msgrcv");
      continue;
    }
    if (buf.mtype == 1) {
      printf("%s\n", buf.mtext);
    } else if (buf.mtype == 2) {
      sscanf(buf.mtext, "%d$%s", &(buf.gid), buf.mtext);
      printf("%s\n", buf.mtext);

      // start communication with server (chat mode)
      printf("Type a message and hit enter to send. Use ^D to quit group");
      pid_t pid = fork();
      if (pid < 0) {
        perror("fork failed");
        exit(1);
      } else if (pid == 0) {
        signal(SIGQUIT, sigquit);
        for (;;) {
          if (msgrcv(clt.cmsqid, &(buf.mtype), sizeof(buf), 0, 0) == -1) {
            perror("msgrcv");
            exit(1);
          }
          if (buf.mtype == 3) {
            printf("%s", buf.mtext);
          }
        }
        // TODO: exit on signal
      }
      buf.mtype = 2;
      while (fgets(buf.mtext, MAX_MTEXT, stdin) != NULL) {
        buf.mtext[strcspn(buf.mtext, "\n")] = 0;  // remove trailing newline char
        if (msgsnd(msqid, &(buf.mtype), sizeof(buf), 0) == -1) {
          perror("msgsnd");
        }
      }
      kill(pid, SIGQUIT);
    }
  }
  // TODO: tell server
  if (msgctl(clt.cmsqid, IPC_RMID, NULL) == -1) {
    perror("msgctl");
    exit(1);
  }
  return 0;
}
