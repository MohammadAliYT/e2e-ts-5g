// rfsim-hub.cpp - RFsim relay hub with a per-link path-loss matrix (thesis option 3c).
//
// All OAI nodes connect to the hub as RFsim CLIENTS; the hub is the only RFsim server.
// Every node has its own TCP port, so the hub knows who is who:
//   DU j -> --du <port> (names du1, du2, ...)      UE k -> --ue <port> (names ue1, ue2, ...)
// Downlink to UE k = sum_j g[k][j] * DU_j + noise_k ; uplink to DU j = sum_k g[k][j] * UE_k + noise_j
// g = 10^(-loss_dB/20); one loss per UE-DU pair, same in both directions, changeable at run time.
//
// Wire format = OAI 2026.w46 radio/rfsimulator/simulator.cpp:
//   samplesBlockHeader_t (24 bytes) | nbAnt x float (sender gain, dB) | nbAnt x size x c16 (antenna after antenna)
// Pacing = same rule as an RFsim node: an interval is sent to a destination only when every connected
// source of that destination has delivered it. A new client gets the current hub time (1-sample block),
// and until it sends data it counts as silence up to that time (as add_client() does in OAI).
// Only antenna 0 of each node is used (the lab is 1x1).
//
// Build: g++ -O3 -march=native -std=c++17 -Wall -o rfsim-hub rfsim-hub.cpp
// Run (in the RAN namespace):
//   rfsim-hub --du 4043 --du 4044 --ue 4051 --ue 4052 --ctrl 4099 [--rate 61440000]
// Control (text, one command per line), e.g.  echo show | nc -q1 127.0.0.1 4099
//   loss <ueN> <duN> <dB>     path loss of one UE-DU link (both directions)
//   noise <node> <dBFS>|off   receiver noise of one node (std per I/Q = 32767*10^(dBFS/20), as in OAI)
//   show                      links, nodes, counters

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdarg>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <random>
#include <sstream>
#include <string>
#include <vector>

struct Header {
  uint32_t size;  // samples per antenna
  uint32_t nbAnt;
  uint64_t timestamp;  // sample time of the first sample
  uint32_t option_value;
  uint32_t option_flag;
};
static_assert(sizeof(Header) == 24, "samplesBlockHeader_t must be 24 bytes");
struct C16 {
  int16_t r, i;
};

static const uint64_t RING = 1ull << 22;      // samples kept per source (~68 ms at 61.44 MS/s)
static const uint32_t MAX_OUT_BLOCK = 30720;  // samples per outgoing message
static const size_t MAX_OUTQ = 64u << 20;     // bytes queued for one destination before releasing pauses
static const uint32_t MAX_IN_BLOCK = 1u << 22;

enum Kind { DU, UE };
enum Tag : uint32_t { T_LISTEN = 1, T_CONN = 2, T_CTRL_LISTEN = 3, T_CTRL_CONN = 4 };

struct Node {
  std::string name;
  Kind kind;
  int idx;  // index within its kind
  int port;
  int lfd = -1, fd = -1;
  // receive state machine
  bool hdr_mode = true;
  Header th{};
  std::vector<char> rbuf;
  size_t rgot = 0;
  // as a source
  std::vector<C16> ring;
  bool has_data = false;
  uint64_t first_ts = 0, last_ts = 0, sync_ts = 0;
  // as a destination
  uint64_t sent_ts = 0;
  std::vector<char> outq;
  size_t outoff = 0;
  bool want_out = false;
  bool silence = false;
  double sil_wall = 0;
  uint64_t sil_ts = 0;
  double noise_std = 0;  // 0 = off
  double noise_dbfs = 0;
  // counters
  uint64_t rx_blocks = 0, tx_blocks = 0, rx_bytes = 0, tx_bytes = 0;
};

static std::vector<Node> nodes;
static std::vector<std::vector<double>> loss_db;  // [ue][du]
static int ep = -1;
static double rate = 61440000.0;
static uint64_t frontier = 0;  // latest sample time seen or sent; time base for new clients
static volatile sig_atomic_t stop_flag = 0;
static std::vector<float> gauss;
static uint32_t rng_state = 2463534242u;

static double now_s()
{
  timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static void hlog(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void hlog(const char *fmt, ...)
{
  timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  struct tm tm;
  localtime_r(&ts.tv_sec, &tm);
  char tbuf[32];
  strftime(tbuf, sizeof(tbuf), "%H:%M:%S", &tm);
  printf("%s.%03ld [hub] ", tbuf, ts.tv_nsec / 1000000);
  va_list ap;
  va_start(ap, fmt);
  vprintf(fmt, ap);
  va_end(ap);
  fflush(stdout);
}

static inline float gauss_next()
{
  rng_state ^= rng_state << 13;
  rng_state ^= rng_state >> 17;
  rng_state ^= rng_state << 5;
  return gauss[rng_state & (gauss.size() - 1)];
}

static int listen_on(int port)
{
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    perror("socket");
    exit(1);
  }
  int one = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  sockaddr_in a{};
  a.sin_family = AF_INET;
  a.sin_port = htons(port);
  a.sin_addr.s_addr = INADDR_ANY;
  if (bind(fd, (sockaddr *)&a, sizeof(a)) < 0) {
    fprintf(stderr, "bind port %d: %s\n", port, strerror(errno));
    exit(1);
  }
  if (listen(fd, 4) < 0) {
    perror("listen");
    exit(1);
  }
  fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
  return fd;
}

static void ep_add(int fd, uint32_t events, uint64_t tag)
{
  epoll_event e{};
  e.events = events;
  e.data.u64 = tag;
  if (epoll_ctl(ep, EPOLL_CTL_ADD, fd, &e) < 0) {
    perror("epoll_ctl add");
    exit(1);
  }
}

static void ep_mod(int fd, uint32_t events, uint64_t tag)
{
  epoll_event e{};
  e.events = events;
  e.data.u64 = tag;
  epoll_ctl(ep, EPOLL_CTL_MOD, fd, &e);
}

static uint64_t tag_of(Tag t, uint32_t i) { return ((uint64_t)t << 32) | i; }

static double gain_lin(const Node &dst, const Node &src)
{
  double l = dst.kind == UE ? loss_db[dst.idx][src.idx] : loss_db[src.idx][dst.idx];
  return pow(10.0, -l / 20.0);
}

static void bump_frontier(uint64_t ts)
{
  if (ts > frontier)
    frontier = ts;
}

static void queue_block(Node &d, uint64_t ts, const C16 *s, uint32_t n)
{
  Header h{n, 1, ts, 0, 0};
  float g = 0.0f;
  size_t add = sizeof(h) + sizeof(g) + n * sizeof(C16);
  if (d.outoff > 0 && d.outoff == d.outq.size()) {
    d.outq.clear();
    d.outoff = 0;
  }
  size_t old = d.outq.size();
  d.outq.resize(old + add);
  char *p = d.outq.data() + old;
  memcpy(p, &h, sizeof(h));
  memcpy(p + sizeof(h), &g, sizeof(g));
  memcpy(p + sizeof(h) + sizeof(g), s, n * sizeof(C16));
  d.tx_blocks++;
}

static void drop_node(Node &n, const char *why)
{
  if (n.fd < 0)
    return;
  epoll_ctl(ep, EPOLL_CTL_DEL, n.fd, nullptr);
  close(n.fd);
  n.fd = -1;
  n.has_data = false;
  n.hdr_mode = true;
  n.rgot = 0;
  n.outq.clear();
  n.outoff = 0;
  n.want_out = false;
  n.silence = false;
  hlog("%s disconnected (%s)\n", n.name.c_str(), why);
}

static void accept_node(uint32_t i)
{
  Node &n = nodes[i];
  int fd = accept(n.lfd, nullptr, nullptr);
  if (fd < 0)
    return;
  if (n.fd >= 0) {
    hlog("%s: second connection on port %d refused\n", n.name.c_str(), n.port);
    close(fd);
    return;
  }
  fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
  int one = 1, buf = 16 << 20;
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
  setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &buf, sizeof(buf));
  setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &buf, sizeof(buf));
  n.fd = fd;
  n.hdr_mode = true;
  n.rgot = 0;
  n.has_data = false;
  n.sync_ts = frontier;
  n.sent_ts = frontier;
  n.silence = false;
  n.rx_blocks = n.tx_blocks = n.rx_bytes = n.tx_bytes = 0;
  ep_add(fd, EPOLLIN | EPOLLRDHUP, tag_of(T_CONN, i));
  // time base for the new client: 1-sample block (OAI add_client())
  C16 zero{0, 0};
  queue_block(n, frontier, &zero, 1);
  n.tx_blocks = 0;
  hlog("%s connected on port %d, time base %lu\n", n.name.c_str(), n.port, (unsigned long)frontier);
}

static void store_packet(Node &n)
{
  const Header &h = n.th;
  const float g_db = *reinterpret_cast<const float *>(n.rbuf.data());
  const C16 *s = reinterpret_cast<const C16 *>(n.rbuf.data() + h.nbAnt * sizeof(float));  // antenna 0
  const uint64_t ts = h.timestamp;
  const uint32_t cnt = h.size;
  if (!n.has_data) {
    n.has_data = true;
    n.first_ts = ts;
    n.last_ts = ts;
    hlog("%s first data at %lu (time base was %lu)\n", n.name.c_str(), (unsigned long)ts, (unsigned long)n.sync_ts);
  }
  if (ts > n.last_ts) {  // gap in the transmit stream = silence
    uint64_t gap = ts - n.last_ts;
    if (gap >= RING) {
      n.first_ts = ts;
    } else {
      for (uint64_t t = n.last_ts; t < ts; t++)
        n.ring[t & (RING - 1)] = C16{0, 0};
    }
  }
  if (g_db == 0.0f) {
    for (uint32_t k = 0; k < cnt; k++)
      n.ring[(ts + k) & (RING - 1)] = s[k];
  } else {
    float g = powf(10.0f, g_db / 20.0f);
    for (uint32_t k = 0; k < cnt; k++)
      n.ring[(ts + k) & (RING - 1)] = C16{(int16_t)lrintf(s[k].r * g), (int16_t)lrintf(s[k].i * g)};
  }
  if (ts + cnt > n.last_ts)
    n.last_ts = ts + cnt;
  if (ts < n.first_ts)
    n.first_ts = ts;
  bump_frontier(n.last_ts);
  n.rx_blocks++;
}

static void read_node(Node &n)
{
  for (;;) {
    char *dst;
    size_t want;
    if (n.hdr_mode) {
      dst = reinterpret_cast<char *>(&n.th) + n.rgot;
      want = sizeof(Header) - n.rgot;
    } else {
      dst = n.rbuf.data() + n.rgot;
      want = n.rbuf.size() - n.rgot;
    }
    ssize_t r = recv(n.fd, dst, want, MSG_DONTWAIT);
    if (r == 0) {
      drop_node(n, "closed by peer");
      return;
    }
    if (r < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        return;
      if (errno == EINTR)
        continue;
      drop_node(n, strerror(errno));
      return;
    }
    n.rx_bytes += r;
    n.rgot += r;
    if ((size_t)r < want)
      continue;
    n.rgot = 0;
    if (n.hdr_mode) {
      if (n.th.nbAnt == 0 || n.th.nbAnt > 8 || n.th.size > MAX_IN_BLOCK) {
        hlog("%s: bad header (size %u, nbAnt %u)\n", n.name.c_str(), n.th.size, n.th.nbAnt);
        drop_node(n, "protocol error");
        return;
      }
      n.rbuf.resize(n.th.nbAnt * sizeof(float) + (size_t)n.th.size * n.th.nbAnt * sizeof(C16));
      n.hdr_mode = false;
    } else {
      store_packet(n);
      n.hdr_mode = true;
    }
  }
}

static void flush_node(uint32_t i)
{
  Node &n = nodes[i];
  while (n.fd >= 0 && n.outoff < n.outq.size()) {
    ssize_t w = send(n.fd, n.outq.data() + n.outoff, n.outq.size() - n.outoff, MSG_DONTWAIT | MSG_NOSIGNAL);
    if (w < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        break;
      if (errno == EINTR)
        continue;
      drop_node(n, strerror(errno));
      return;
    }
    n.outoff += w;
    n.tx_bytes += w;
  }
  if (n.fd < 0)
    return;
  if (n.outoff == n.outq.size()) {
    n.outq.clear();
    n.outoff = 0;
  } else if (n.outoff > (8u << 20)) {
    n.outq.erase(n.outq.begin(), n.outq.begin() + n.outoff);
    n.outoff = 0;
  }
  bool pending = n.outoff < n.outq.size();
  if (pending != n.want_out) {
    n.want_out = pending;
    ep_mod(n.fd, EPOLLIN | EPOLLRDHUP | (pending ? (uint32_t)EPOLLOUT : 0u), tag_of(T_CONN, i));
  }
}

static void release(Node &d, double now)
{
  if (d.fd < 0 || d.outq.size() - d.outoff > MAX_OUTQ)
    return;
  const Kind src_kind = d.kind == UE ? DU : UE;
  bool any = false;
  uint64_t R = UINT64_MAX;
  for (const Node &s : nodes) {
    if (s.kind != src_kind || s.fd < 0)
      continue;
    any = true;
    R = std::min(R, s.has_data ? s.last_ts : s.sync_ts);
  }
  if (!any) {  // nobody to listen to: silence at real-time speed, 2 ms ahead
    if (!d.silence) {
      d.silence = true;
      d.sil_wall = now;
      d.sil_ts = d.sent_ts;
    }
    R = d.sil_ts + (uint64_t)((now - d.sil_wall) * rate) + (uint64_t)(0.002 * rate);
  } else {
    d.silence = false;
  }
  if (R <= d.sent_ts)
    return;

  static std::vector<float> acc_r, acc_i;
  static std::vector<C16> out;
  for (uint64_t t = d.sent_ts; t < R;) {
    uint32_t cnt = (uint32_t)std::min<uint64_t>(R - t, MAX_OUT_BLOCK);
    acc_r.assign(cnt, 0.0f);
    acc_i.assign(cnt, 0.0f);
    for (const Node &s : nodes) {
      if (s.kind != src_kind || s.fd < 0 || !s.has_data)
        continue;
      uint64_t a = std::max(t, s.first_ts), b = std::min(t + cnt, s.last_ts);
      if (a >= b)
        continue;
      float g = (float)gain_lin(d, s);
      for (uint64_t u = a; u < b; u++) {
        const C16 &v = s.ring[u & (RING - 1)];
        acc_r[u - t] += g * v.r;
        acc_i[u - t] += g * v.i;
      }
    }
    out.resize(cnt);
    const float ns = (float)d.noise_std;
    for (uint32_t k = 0; k < cnt; k++) {
      float r = acc_r[k], im = acc_i[k];
      if (ns > 0) {
        r += ns * gauss_next();
        im += ns * gauss_next();
      }
      out[k].r = (int16_t)std::max(-32768.0f, std::min(32767.0f, rintf(r)));
      out[k].i = (int16_t)std::max(-32768.0f, std::min(32767.0f, rintf(im)));
    }
    queue_block(d, t, out.data(), cnt);
    t += cnt;
  }
  d.sent_ts = R;
  bump_frontier(R);
}

static Node *find_node(const std::string &name)
{
  for (Node &n : nodes)
    if (n.name == name)
      return &n;
  return nullptr;
}

static std::string show()
{
  std::ostringstream o;
  o << "links (path loss dB):\n      ";
  int ndu = 0, nue = 0;
  for (const Node &n : nodes)
    (n.kind == DU ? ndu : nue)++;
  for (int j = 0; j < ndu; j++)
    o << "  du" << j + 1 << "  ";
  o << "\n";
  for (int k = 0; k < nue; k++) {
    o << "ue" << k + 1 << "  ";
    for (int j = 0; j < ndu; j++) {
      char b[32];
      snprintf(b, sizeof(b), "%7.1f", loss_db[k][j]);
      o << b;
    }
    o << "\n";
  }
  o << "nodes:\n";
  for (const Node &n : nodes) {
    char b[256];
    snprintf(b,
             sizeof(b),
             "%-4s port %d %-12s rx %lu blk  tx %lu blk  last_rx_ts %lu  sent_ts %lu  noise %s\n",
             n.name.c_str(),
             n.port,
             n.fd >= 0 ? (n.silence ? "conn(silence)" : "connected") : "-",
             (unsigned long)n.rx_blocks,
             (unsigned long)n.tx_blocks,
             (unsigned long)(n.has_data ? n.last_ts : 0),
             (unsigned long)n.sent_ts,
             n.noise_std > 0 ? (std::to_string(n.noise_dbfs) + " dBFS").c_str() : "off");
    o << b;
  }
  return o.str();
}

static std::string control(const std::string &line)
{
  std::istringstream in(line);
  std::string cmd;
  in >> cmd;
  if (cmd.empty())
    return "";
  if (cmd == "show")
    return show();
  if (cmd == "loss") {
    std::string ue, du;
    double db;
    if (!(in >> ue >> du >> db))
      return "usage: loss <ueN> <duN> <dB>\n";
    Node *u = find_node(ue), *d = find_node(du);
    if (!u || !d || u->kind != UE || d->kind != DU)
      return "unknown ue/du\n";
    loss_db[u->idx][d->idx] = db;
    hlog("loss %s-%s = %.1f dB\n", ue.c_str(), du.c_str(), db);
    return "ok\n";
  }
  if (cmd == "noise") {
    std::string nm, v;
    if (!(in >> nm >> v))
      return "usage: noise <node> <dBFS>|off\n";
    Node *n = find_node(nm);
    if (!n)
      return "unknown node\n";
    if (v == "off") {
      n->noise_std = 0;
    } else {
      n->noise_dbfs = atof(v.c_str());
      n->noise_std = 32767.0 * pow(10.0, n->noise_dbfs / 20.0);
    }
    hlog("noise %s = %s\n", nm.c_str(), v.c_str());
    return "ok\n";
  }
  return "commands: loss <ueN> <duN> <dB> | noise <node> <dBFS>|off | show\n";
}

static void usage()
{
  fprintf(stderr,
          "usage: rfsim-hub --du <port> [--du <port> ...] --ue <port> [--ue <port> ...] [--ctrl <port>] [--rate <S/s>]\n");
  exit(1);
}

int main(int argc, char **argv)
{
  signal(SIGPIPE, SIG_IGN);
  signal(SIGINT, [](int) { stop_flag = 1; });
  signal(SIGTERM, [](int) { stop_flag = 1; });
  int ctrl_port = 0, ndu = 0, nue = 0;
  for (int a = 1; a < argc; a++) {
    std::string o = argv[a];
    if (a + 1 >= argc)
      usage();
    if (o == "--du" || o == "--ue") {
      Node n;
      n.kind = o == "--du" ? DU : UE;
      n.idx = n.kind == DU ? ndu++ : nue++;
      n.name = (n.kind == DU ? "du" : "ue") + std::to_string(n.idx + 1);
      n.port = atoi(argv[++a]);
      nodes.push_back(n);
    } else if (o == "--ctrl") {
      ctrl_port = atoi(argv[++a]);
    } else if (o == "--rate") {
      rate = atof(argv[++a]);
    } else {
      usage();
    }
  }
  if (ndu == 0 || nue == 0)
    usage();
  loss_db.assign(nue, std::vector<double>(ndu, 0.0));
  std::mt19937 gen(12345);
  std::normal_distribution<float> nd(0.0f, 1.0f);
  gauss.resize(1 << 20);
  for (float &x : gauss)
    x = nd(gen);

  ep = epoll_create1(0);
  for (uint32_t i = 0; i < nodes.size(); i++) {
    nodes[i].ring.assign(RING, C16{0, 0});
    nodes[i].lfd = listen_on(nodes[i].port);
    ep_add(nodes[i].lfd, EPOLLIN, tag_of(T_LISTEN, i));
  }
  int ctrl_lfd = -1;
  std::vector<int> ctrl_fds;
  std::vector<std::string> ctrl_bufs;
  if (ctrl_port) {
    ctrl_lfd = listen_on(ctrl_port);
    ep_add(ctrl_lfd, EPOLLIN, tag_of(T_CTRL_LISTEN, 0));
  }
  hlog("listening: ");
  for (const Node &n : nodes)
    printf("%s:%d ", n.name.c_str(), n.port);
  printf("ctrl:%d rate %.0f S/s\n", ctrl_port, rate);
  fflush(stdout);

  double last_stats = now_s();
  epoll_event evs[64];
  while (!stop_flag) {
    int ne = epoll_wait(ep, evs, 64, 1);
    for (int e = 0; e < ne; e++) {
      Tag t = (Tag)(evs[e].data.u64 >> 32);
      uint32_t i = (uint32_t)evs[e].data.u64;
      if (t == T_LISTEN) {
        accept_node(i);
      } else if (t == T_CONN) {
        Node &n = nodes[i];
        if (evs[e].events & EPOLLIN)
          read_node(n);
        if (n.fd >= 0 && (evs[e].events & (EPOLLHUP | EPOLLERR | EPOLLRDHUP))) {
          read_node(n);  // drain what is left
          drop_node(n, "hang-up");
        }
        if (n.fd >= 0 && (evs[e].events & EPOLLOUT))
          flush_node(i);
      } else if (t == T_CTRL_LISTEN) {
        int fd = accept(ctrl_lfd, nullptr, nullptr);
        if (fd >= 0) {
          fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
          ctrl_fds.push_back(fd);
          ctrl_bufs.emplace_back();
          ep_add(fd, EPOLLIN | EPOLLRDHUP, tag_of(T_CTRL_CONN, (uint32_t)(ctrl_fds.size() - 1)));
        }
      } else if (t == T_CTRL_CONN) {
        int fd = ctrl_fds[i];
        if (fd < 0)
          continue;
        char b[512];
        ssize_t r = recv(fd, b, sizeof(b), MSG_DONTWAIT);
        if (r > 0) {
          ctrl_bufs[i].append(b, r);
          size_t p;
          while ((p = ctrl_bufs[i].find('\n')) != std::string::npos) {
            std::string line = ctrl_bufs[i].substr(0, p);
            if (!line.empty() && line.back() == '\r')
              line.pop_back();
            ctrl_bufs[i].erase(0, p + 1);
            std::string reply = control(line);
            if (!reply.empty() && send(fd, reply.data(), reply.size(), MSG_NOSIGNAL) < 0)
              break;
          }
        } else if (r == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
          epoll_ctl(ep, EPOLL_CTL_DEL, fd, nullptr);
          close(fd);
          ctrl_fds[i] = -1;
        }
      }
    }
    double now = now_s();
    for (uint32_t i = 0; i < nodes.size(); i++) {
      release(nodes[i], now);
      flush_node(i);
    }
    if (now - last_stats >= 10.0) {
      last_stats = now;
      for (const Node &n : nodes)
        if (n.fd >= 0)
          hlog("%s rx %lu blk tx %lu blk last_rx_ts %lu sent_ts %lu%s\n",
               n.name.c_str(),
               (unsigned long)n.rx_blocks,
               (unsigned long)n.tx_blocks,
               (unsigned long)(n.has_data ? n.last_ts : 0),
               (unsigned long)n.sent_ts,
               n.silence ? " (silence)" : "");
    }
  }
  hlog("stopping\n");
  return 0;
}