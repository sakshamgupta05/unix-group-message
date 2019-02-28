#define _POSIX_SOURCE
#include <signal.h> 
#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include "key.h"
#include "client.h"

int main(void) {
  struct my_msgbuf buf;
  struct client clt;

  // establish connection with server
  int msqid;
  key_t key;
  if ((key = ftok(MSGQ_PATH, 'B')) == -1) {
    perror("ftok");
    exit(1);
  }
  if ((msqid = msgget(key, 0)) == -1) {
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
  buf.mtype = TP_HELLO;
  sprintf(buf.mtext, "%ld$%s", clt.cmsqid, clt.cname);
  if (msgsnd(msqid, &(buf.mtype), sizeof(buf), 0) == -1) {
    perror("msgsnd");
    msgctl(msqid, IPC_RMID, NULL);
    exit(1);
  }
  if (msgrcv(clt.cmsqid, &(buf.mtype), sizeof(buf), TP_HELLO, 0) == -1) {
    perror("msgrcv");
    msgctl(msqid, IPC_RMID, NULL);
    exit(1);
  }
  if (buf.err) {
    printf("server handshake error: %s", buf.mtext);
    exit(1);
  } else {
    sscanf(buf.mtext, "%d", &(clt.cid));
    buf.cid = clt.cid;
    printf("Successfully connected to server\n");
  }

  // start communication with server (command mode)
  printf("Enter commands (crgrp, lsgrp, jngrp), ^D to quit:\n");
  fgets(buf.mtext, MAX_MTEXT, stdin);
  while (fgets(buf.mtext, MAX_MTEXT, stdin) != NULL) {
    buf.mtext[strcspn(buf.mtext, "\n")] = 0;  // remove trailing newline char

    // identify command
    if (strncmp(buf.mtext, "crgrp ", 6) == 0) {
      buf.mtype = TP_CRGRP;
    } else if (strcmp(buf.mtext, "lsgrp") == 0) {
      buf.mtype = TP_LSGRP;
    } else if (strncmp(buf.mtext, "jngrp ", 6) == 0) {
      buf.mtype = TP_JNGRP;
    } else {
      printf("error: invalid command\n");
      continue;
    }

    // send command
    if (msgsnd(msqid, &(buf.mtype), sizeof(buf), 0) == -1) {
      perror("msgsnd");
      continue;
    }
    if (msgrcv(clt.cmsqid, &(buf.mtype), sizeof(buf), buf.mtype, 0) == -1) {
      perror("msgrcv");
      continue;
    }
    printf("%s\n", buf.mtext);
    if (buf.mtype == TP_JNGRP && !buf.err) {
      // start communication with server (chat mode)
      printf("Type a message and hit enter to send. Use '$quit' to quit group\n");
      pid_t pid = fork();
      if (pid < 0) {
        perror("fork failed");
        exit(1);
      } else if (pid == 0) {
        for (;;) {
          if (msgrcv(clt.cmsqid, &(buf.mtype), sizeof(buf), TP_CHAT, 0) == -1) {
            perror("msgrcv");
            exit(1);
          }
          printf("%s\n", buf.mtext);
        }
      }
      while (fgets(buf.mtext, MAX_MTEXT, stdin) != NULL) {
        buf.mtext[strcspn(buf.mtext, "\n")] = 0;  // remove trailing newline char
        if (strcmp(buf.mtext, "$quit") == 0) break;
        buf.mtype = TP_CHAT;
        if (msgsnd(msqid, &(buf.mtype), sizeof(buf), 0) == -1) {
          perror("msgsnd");
        }
      }
      kill(pid, SIGKILL);
      printf("Enter commands (crgrp, lsgrp, jngrp), ^D to quit:\n");
    }
  }
  if (msgctl(clt.cmsqid, IPC_RMID, NULL) == -1) {
    perror("msgctl");
    exit(1);
  }
  return 0;
}
