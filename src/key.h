#ifndef KEY_H_
#define KEY_H_

#define MSGQ_PATH "/home"
#define MAX_MTEXT 1024

#define TP_HELLO 1
#define TP_CRGRP 2
#define TP_LSGRP 3
#define TP_JNGRP 4
#define TP_CHAT 51

struct my_msgbuf {
  long mtype;
  char mtext[MAX_MTEXT];
  int err;
  int cid;
  int gid;
};

#endif
