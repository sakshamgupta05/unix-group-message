#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <string.h>
#include "key.h"
#include "client.h"

#define MAX_GNAME 128
#define MAX_GMEMBERS 128

#define MAX_CLIENTS 4096
#define MAX_GRPS 128

struct msg_grp {
  int gid;
  char gname[MAX_GNAME];
  int numMembers;
  struct client *gmembers[MAX_GMEMBERS];
};

struct client *clients[MAX_CLIENTS];
struct msg_grp *grps[MAX_GRPS];

int num_clients;
int num_grps;

int find_grp(char *gname) {
  for (int i = 0; i < num_grps; i++) {
    if (strcmp(gname, grps[i] -> gname) == 0) {
      return i;
    }
  }
  return -1;
}

void list_groups(char *mtext) {
  for (int i = 0; i < num_grps; i++) {
    sprintf(mtext, "%s ", grps[i] -> gname);
    mtext += strlen(grps[i] -> gname) + 1;
  }
}

int main(void) {
  struct my_msgbuf buf;
  struct client *clt;
  struct msg_grp *grp;

  int msqid;
  key_t key;
  if ((key = ftok(MSGQ_PATH, 'D')) == -1) {
    perror("ftok");
    exit(1);
  }
  if ((msqid = msgget(key, IPC_CREAT | 0644)) == -1) {
    perror("msgget");
    exit(1);
  }
  printf("server: ready to receive messages\n");
  for (;;) {
    if (msgrcv(msqid, &(buf.mtype), sizeof(buf), 0, 0) == -1) {
      perror("msgrcv");
      exit(1);
    }
    if (buf.mtype == 1) {
      // type: hello from client

      clt = malloc(sizeof(struct client));
      sscanf(buf.mtext, "%ld$%s", &(clt -> cmsqid), clt -> cname);

      if (num_clients >= MAX_CLIENTS) {
        buf.mtype = 2;
        strcpy(buf.mtext, "max clients limit reached");
        if (msgsnd(clt -> cmsqid, &(buf.mtype), sizeof(buf), 0) == -1){
          perror("msgsnd");
        }
        free(clt);
      } else {
        buf.mtype = 1;
        clt -> cid = num_clients;
        sprintf(buf.mtext, "%d", clt -> cid);
        clients[num_clients++] = clt;
        if (msgsnd(clt -> cmsqid, &(buf.mtype), sizeof(buf), 0) == -1){
          perror("msgsnd");
        }
      }
    } else if (buf.mtype == 2) {
      // type: command

      clt = clients[buf.cid];
      if (strncmp(buf.mtext, "crgrp ", 6) != 0) {
        // command: create group
        char gname[MAX_GNAME];
        strcpy(gname, buf.mtext + 6);
        if (find_grp(gname) != -1) {
          buf.mtype = 1;
          strcpy(buf.mtext, "error: group already present");
          if (msgsnd(clt -> cmsqid, &(buf.mtype), sizeof(buf), 0) == -1) {
            perror("msgsnd");
          }
        } else if (num_grps >= MAX_GRPS) {
          buf.mtype = 1;
          strcpy(buf.mtext, "error: max groups limit reached");
          if (msgsnd(clt -> cmsqid, &(buf.mtype), sizeof(buf), 0) == -1){
            perror("msgsnd");
          }
        } else {
          grp = malloc(sizeof(struct msg_grp));
          strcpy(grp -> gname, gname);
          grp -> numMembers = 0;
          grp -> gid = num_grps;
          grps[num_grps++] = grp;

          buf.mtype = 1;
          strcpy(buf.mtext, "success: group created");
          if (msgsnd(clt -> cmsqid, &(buf.mtype), sizeof(buf), 0) == -1) {
            perror("msgsnd");
          }
        }
      } else if (strcmp(buf.mtext, "lsgrp") != 0) {
        // command: list groups
        buf.mtype = 1;
        list_groups(buf.mtext);
        if (msgsnd(clt -> cmsqid, &(buf.mtype), sizeof(buf), 0) == -1) {
          perror("msgsnd");
        }
      } else if (strncmp(buf.mtext, "jngrp ", 6) != 0) {
        // command: join group
        char gname[MAX_GNAME];
        strcpy(gname, buf.mtext + 6);
        int gid = find_grp(gname);
        if (gid == -1) {
          buf.mtype = 1;
          strcpy(buf.mtext, "error: group not present");
          if (msgsnd(clt -> cmsqid, &(buf.mtype), sizeof(buf), 0) == -1) {
            perror("msgsnd");
          }
        } else {
          grp = grps[gid];
          if (grp -> numMembers >= MAX_GMEMBERS) {
            buf.mtype = 1;
            strcpy(buf.mtext, "error: max members in group limit reached");
            if (msgsnd(clt -> cmsqid, &(buf.mtype), sizeof(buf), 0) == -1){
              perror("msgsnd");
            }
          } else {
            (grp -> gmembers)[grp -> numMembers++] = clt;
            buf.mtype = 2;
            sprintf(buf.mtext, "group (%s) joined successfully!", grp -> gname);
            if (msgsnd(clt -> cmsqid, &(buf.mtype), sizeof(buf), 0) == -1){
              perror("msgsnd");
            }
          }
        }
      } else {
        // invalid command
        buf.mtype = 1;
        strcpy(buf.mtext, "error: invalid command");
        if (msgsnd(clt -> cmsqid, &(buf.mtype), sizeof(buf), 0) == -1) {
          perror("msgsnd");
        }
      }
    } else if (buf.mtype == 2) {
      // type: chat

      // TODO: chat
    }
    printf("server: \"%s\"\n", buf.mtext);
  }
  return 0;
}
