// Parse a libconfig file with the C library (same as OAI) and print the exact error.
// Build: gcc tools/cfgcheck.c -o tools/cfgcheck -lconfig
// Use:   tools/cfgcheck <file> [include_dir]
#include <libconfig.h>
#include <stdio.h>
int main(int argc, char **argv) {
  config_t cfg; config_init(&cfg);
  if (argc > 2) config_set_include_dir(&cfg, argv[2]);
  if (!config_read_file(&cfg, argv[1])) {
    const char *f = config_error_file(&cfg);
    printf("ERROR %s:%d: %s\n", f ? f : argv[1], config_error_line(&cfg), config_error_text(&cfg));
    return 1;
  }
  printf("OK\n"); return 0;
}