#ifndef KEY_H_
#define KEY_H_

#define MSGQ_PATH "/home"
#define MAX_MTEXT 1024

struct my_msgbuf {
  long mtype;
  char mtext[MAX_MTEXT];
  int cid;
};

#endif
