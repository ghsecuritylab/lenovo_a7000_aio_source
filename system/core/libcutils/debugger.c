/*
* Copyright (C) 2014 MediaTek Inc.
* Modification based on code covered by the mentioned copyright
* and/or permission notice(s).
*/
/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdbool.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>

#include <cutils/debugger.h>
#include <cutils/sockets.h>

#if defined(__LP64__)
#include <elf.h>

static int is_process(int pid, const char *name)
{
  char path[64];
  char threadnamebuf[1024];
  char* threadname = NULL;
  FILE *fp;

  snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
  if ((fp = fopen(path, "r"))) {
    threadname = fgets(threadnamebuf, sizeof(threadnamebuf), fp);
    fclose(fp);
    if (threadname) {
      size_t len = strlen(threadname);
      if (len && threadname[len - 1] == '\n') {
        threadname[len - 1] = '\0';
      }
    }
  }

  if (threadname && name && strstr(threadname, name))
    return 1; // yes
  else
    return 0; // no
}

static bool is32bit(pid_t tid) {
  char* exeline;
  if (asprintf(&exeline, "/proc/%d/exe", tid) == -1) {
    return false;
  }
  int fd = open(exeline, O_RDONLY | O_CLOEXEC);
  free(exeline);
  if (fd == -1) {
    if (is_process(tid, "/system/bin/mediaserver") || is_process(tid, "com.android.gallery3d"))
      return true;
    return false;
  }

  char ehdr[EI_NIDENT];
  ssize_t bytes = read(fd, &ehdr, sizeof(ehdr));
  close(fd);
  if (bytes != (ssize_t) sizeof(ehdr) || memcmp(ELFMAG, ehdr, SELFMAG) != 0) {
    return false;
  }
  if (ehdr[EI_CLASS] == ELFCLASS32) {
    return true;
  }
  return false;
}
#endif

static int send_request(int sock_fd, void* msg_ptr, size_t msg_len) {
  int result = 0;

  int status = 0;
  struct pollfd pollfds[1];
  pollfds[0].fd = sock_fd;
  pollfds[0].events = POLLRDHUP;
  pollfds[0].revents = 0;
  do {
    status = TEMP_FAILURE_RETRY(poll(pollfds, 1, 0));
    if ((status < 0 && errno != EINTR) || (pollfds[0].revents & POLLRDHUP)) {
      result = -1;
      // stream socket peer closed connection, or shut down writing half of connection, or error
      return result;
    }
  } while(status < 0 && errno == EINTR);

  if (TEMP_FAILURE_RETRY(write(sock_fd, msg_ptr, msg_len)) != (ssize_t) msg_len) {
    result = -1;
  } else {
    char ack;
    if (TEMP_FAILURE_RETRY(read(sock_fd, &ack, 1)) != 1) {
      result = -1;
    }
  }
  return result;
}

static int make_dump_request(debugger_action_t action, pid_t tid) {
  const char* socket_name;
  debugger_msg_t msg;
  size_t msg_len;
  void* msg_ptr;

#if defined(__LP64__)
  debugger32_msg_t msg32;
  if (is32bit(tid)) {
    msg_len = sizeof(debugger32_msg_t);
    memset(&msg32, 0, msg_len);
    msg32.tid = tid;
    msg32.action = action;
    msg_ptr = &msg32;

    socket_name = DEBUGGER32_SOCKET_NAME;
  } else
#endif
  {
    msg_len = sizeof(debugger_msg_t);
    memset(&msg, 0, msg_len);
    msg.tid = tid;
    msg.action = action;
    msg_ptr = &msg;

    socket_name = DEBUGGER_SOCKET_NAME;
  }

  int sock_fd = socket_local_client(socket_name, ANDROID_SOCKET_NAMESPACE_ABSTRACT,
      SOCK_STREAM | SOCK_CLOEXEC);
  if (sock_fd < 0) {
    return -1;
  }

  if (send_request(sock_fd, msg_ptr, msg_len) < 0) {
    TEMP_FAILURE_RETRY(close(sock_fd));
    return -1;
  }

  return sock_fd;
}

int dump_backtrace_to_file(pid_t tid, int fd) {
  int sock_fd = make_dump_request(DEBUGGER_ACTION_DUMP_BACKTRACE, tid);
  if (sock_fd < 0) {
    return -1;
  }

  /* Write the data read from the socket to the fd. */
  int result = 0;
  char buffer[1024];
  ssize_t n;
  while ((n = TEMP_FAILURE_RETRY(read(sock_fd, buffer, sizeof(buffer)))) > 0) {
    if (TEMP_FAILURE_RETRY(write(fd, buffer, n)) != n) {
      result = -1;
      break;
    }
  }
  TEMP_FAILURE_RETRY(close(sock_fd));
  return result;
}

int dump_tombstone(pid_t tid, char* pathbuf, size_t pathlen) {
  int sock_fd = make_dump_request(DEBUGGER_ACTION_DUMP_TOMBSTONE, tid);
  if (sock_fd < 0) {
    return -1;
  }

  /* Read the tombstone file name. */
  char buffer[100]; /* This is larger than the largest tombstone path. */
  int result = 0;
  ssize_t n = TEMP_FAILURE_RETRY(read(sock_fd, buffer, sizeof(buffer) - 1));
  if (n <= 0) {
    result = -1;
  } else {
    if (pathbuf && pathlen) {
      if (n >= (ssize_t) pathlen) {
        n = pathlen - 1;
      }
      buffer[n] = '\0';
      memcpy(pathbuf, buffer, n + 1);
    }
  }
  TEMP_FAILURE_RETRY(close(sock_fd));
  return result;
}
