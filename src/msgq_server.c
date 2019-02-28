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
  sprintf(mtext, "groups: ");
  mtext += 8;
  for (int i = 0; i < num_grps; i++) {
    sprintf(mtext, "%s ", grps[i] -> gname);
    mtext += strlen(grps[i] -> gname) + 1;
  }
}

int find_client(char *cname) {
  for (int i = 0; i < num_clients; i++) {
    if (strcmp(cname, clients[i] -> cname) == 0) {
      return i;
    }
  }
  return -1;
}

int gmember(int gid, int cid) {
  struct msg_grp *grp = grps[gid];
  for (int i = 0; i < grp -> numMembers; i++) {
    if ((grp -> gmembers)[i] -> cid == cid) {
      return 1;
    }
  }
  return 0;
}

int main(void) {
  struct my_msgbuf buf;
  struct client *clt;
  struct msg_grp *grp;

  int msqid;
  key_t key;
  if ((key = ftok(MSGQ_PATH, 'B')) == -1) {
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
    if (buf.mtype == TP_HELLO) {
      // type: hello from client
      printf("[LOG] (%ld) login: \"%s\"\n", buf.mtype, buf.mtext);

      clt = malloc(sizeof(struct client));
      sscanf(buf.mtext, "%ld$%[^\n]", &(clt -> cmsqid), clt -> cname);
      clt -> cid = find_client(clt -> cname);

      if (clt -> cid != -1) {
        // existing user
        buf.err = 0;
        clients[clt -> cid] -> cmsqid = clt -> cmsqid;
        sprintf(buf.mtext, "%d", clt -> cid);
        if (msgsnd(clt -> cmsqid, &(buf.mtype), sizeof(buf), 0) == -1){
          perror("msgsnd");
        }
        free(clt);
      } else if (num_clients >= MAX_CLIENTS) {
        // max clients limit reached
        buf.err = 1;
        strcpy(buf.mtext, "max clients limit reached");
        if (msgsnd(clt -> cmsqid, &(buf.mtype), sizeof(buf), 0) == -1){
          perror("msgsnd");
        }
        free(clt);
      } else {
        // add new client
        buf.err = 0;
        clt -> cid = num_clients;
        sprintf(buf.mtext, "%d", clt -> cid);
        clients[num_clients++] = clt;
        if (msgsnd(clt -> cmsqid, &(buf.mtype), sizeof(buf), 0) == -1){
          perror("msgsnd");
        }
      }
    } else if (buf.mtype == TP_CRGRP) {
      // type: create group
      clt = clients[buf.cid];
      printf("[LOG] (%ld) cmd (c:%s): \"%s\"\n", buf.mtype, clt -> cname, buf.mtext);

      char gname[MAX_GNAME];
      strcpy(gname, buf.mtext + 6);
      if (find_grp(gname) != -1) {
        buf.err = 1;
        strcpy(buf.mtext, "error: group already present");
        if (msgsnd(clt -> cmsqid, &(buf.mtype), sizeof(buf), 0) == -1) {
          perror("msgsnd");
        }
      } else if (num_grps >= MAX_GRPS) {
        buf.err = 1;
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

        buf.err = 0;
        strcpy(buf.mtext, "success: group created");
        if (msgsnd(clt -> cmsqid, &(buf.mtype), sizeof(buf), 0) == -1) {
          perror("msgsnd");
        }
      }
    } else if (buf.mtype == TP_LSGRP) {
      // type: list group
      clt = clients[buf.cid];
      printf("[LOG] (%ld) cmd (c:%s): \"%s\"\n", buf.mtype, clt -> cname, buf.mtext);

      buf.err = 0;
      list_groups(buf.mtext);
      if (msgsnd(clt -> cmsqid, &(buf.mtype), sizeof(buf), 0) == -1) {
        perror("msgsnd");
      }
    } else if (buf.mtype == TP_JNGRP) {
      // type: join group
      clt = clients[buf.cid];
      printf("[LOG] (%ld) cmd (c:%s): \"%s\"\n", buf.mtype, clt -> cname, buf.mtext);

      char gname[MAX_GNAME];
      strcpy(gname, buf.mtext + 6);
      int gid = find_grp(gname);
      if (gid == -1) {
        buf.err = 1;
        strcpy(buf.mtext, "error: group not present");
        if (msgsnd(clt -> cmsqid, &(buf.mtype), sizeof(buf), 0) == -1) {
          perror("msgsnd");
        }
      } else {
        grp = grps[gid];
        if (gmember(grp -> gid, clt -> cid)) {
          buf.err = 0;
          sprintf(buf.mtext, "group (%s)", grp -> gname);
          buf.gid = grp -> gid;
          if (msgsnd(clt -> cmsqid, &(buf.mtype), sizeof(buf), 0) == -1){
            perror("msgsnd");
          }
        } else if (grp -> numMembers >= MAX_GMEMBERS) {
          buf.err = 1;
          strcpy(buf.mtext, "error: max members in group limit reached");
          if (msgsnd(clt -> cmsqid, &(buf.mtype), sizeof(buf), 0) == -1){
            perror("msgsnd");
          }
        } else {
          (grp -> gmembers)[grp -> numMembers++] = clt;
          buf.err = 0;
          sprintf(buf.mtext, "group (%s) joined successfully!", grp -> gname);
          buf.gid = grp -> gid;
          if (msgsnd(clt -> cmsqid, &(buf.mtype), sizeof(buf), 0) == -1){
            perror("msgsnd");
          }
        }
      }
    } else if (buf.mtype == TP_CHAT) {
      // type: chat
      clt = clients[buf.cid];
      grp = grps[buf.gid];
      printf("[LOG] (%ld) grp (c:%s)(g:%s): \"%s\"\n", buf.mtype, clt -> cname, grp -> gname, buf.mtext);

      buf.err = 0;
      char tmp[MAX_MTEXT];
      strcpy(tmp, buf.mtext);
      sprintf(buf.mtext, "[%s] %s", clt -> cname, tmp);

      for (int i = 0; i < grp -> numMembers; i++) {
        if (grp -> gmembers[i] -> cid != clt -> cid) {
          if (msgsnd(grp -> gmembers[i] -> cmsqid, &(buf.mtype), sizeof(buf), 0) == -1) {
            perror("msgsnd");
          }
        }
      }
    }
  }
  return 0;
}
