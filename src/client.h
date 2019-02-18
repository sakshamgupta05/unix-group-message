#ifndef CLIENT_H_
#define CLIENT_H_

#define MAX_CNAME 128

struct client {
  int cid;
  char cname[MAX_CNAME];
  long cmsqid;
};

#endif
