/* act/cmd_fifo.c - FIFO reader thread and command queue. See cmd_fifo.h. */
#include "act/cmd_fifo.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "config.h"

enum { CMD_QUEUE = 16, LINE_MAX_LEN = 128 };

static char g_path[256];
static int g_fd = -1;
static pthread_t g_thread;
static atomic_bool g_stop;
static pthread_mutex_t g_mtx = PTHREAD_MUTEX_INITIALIZER;
static cmd_t g_q[CMD_QUEUE];
static size_t g_qn;

static void push(cmd_t const* c)
{
  pthread_mutex_lock(&g_mtx);
  if (g_qn < CMD_QUEUE)
    g_q[g_qn++] = *c;
  else
    fprintf(stderr, "[act] command queue full, command ignored\n");
  pthread_mutex_unlock(&g_mtx);
}

/* "ho <id> <du>" or "show"; anything else is rejected with a message */
static void parse_line(char* line)
{
  char word[16] = {0};
  unsigned id = 0;
  int du = 0;
  if (sscanf(line, "%15s", word) != 1)
    return; /* empty line */
  if (strcmp(word, "show") == 0) {
    push(&(cmd_t){.type = CMD_SHOW});
  } else if (strcmp(word, "ho") == 0 && sscanf(line, "%15s %u %d", word, &id, &du) == 3) {
    push(&(cmd_t){.type = CMD_HO, .cu_ue_id = id, .du_no = du});
  } else {
    fprintf(stderr, "[act] unknown command '%s' (use: ho <CU UE ID> <DU number> | show)\n", line);
  }
}

static void* reader(void* arg)
{
  (void)arg;
  char line[LINE_MAX_LEN];
  size_t n = 0;
  while (!atomic_load(&g_stop)) {
    struct pollfd p = {.fd = g_fd, .events = POLLIN};
    if (poll(&p, 1, 200) <= 0)
      continue; /* timeout: check g_stop again */
    char buf[64];
    ssize_t const r = read(g_fd, buf, sizeof(buf));
    for (ssize_t i = 0; i < r; i++) {
      if (buf[i] == '\n' || n == LINE_MAX_LEN - 1) {
        line[n] = '\0';
        parse_line(line);
        n = 0;
      } else {
        line[n++] = buf[i];
      }
    }
  }
  return NULL;
}

void cmd_fifo_start(void)
{
  const char* p = getenv(CMD_FIFO_ENV);
  snprintf(g_path, sizeof(g_path), "%s", (p != NULL && p[0] != '\0') ? p : CMD_FIFO_DEFAULT);

  if (mkfifo(g_path, 0666) != 0 && errno != EEXIST) {
    perror(g_path);
    return;
  }
  chmod(g_path, 0666); /* umask removes write for others; the user must be able to write */
  /* O_RDWR: the FIFO stays open with no writer, so read() never returns EOF */
  g_fd = open(g_path, O_RDWR | O_NONBLOCK);
  if (g_fd < 0) {
    perror(g_path);
    return;
  }
  pthread_create(&g_thread, NULL, reader, NULL);
  printf("[e2e_ctrl] commands: echo 'ho <CU UE ID> <DU number>' > %s   (or 'show')\n", g_path);
}

void cmd_fifo_stop(void)
{
  if (g_fd < 0)
    return;
  atomic_store(&g_stop, true);
  pthread_join(g_thread, NULL);
  close(g_fd);
  g_fd = -1;
  unlink(g_path);
}

size_t cmd_fifo_take(cmd_t* out, size_t max)
{
  pthread_mutex_lock(&g_mtx);
  size_t const n = g_qn < max ? g_qn : max;
  memcpy(out, g_q, n * sizeof(cmd_t));
  memmove(g_q, g_q + n, (g_qn - n) * sizeof(cmd_t));
  g_qn -= n;
  pthread_mutex_unlock(&g_mtx);
  return n;
}
