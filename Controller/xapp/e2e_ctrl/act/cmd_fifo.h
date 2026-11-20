/* act/cmd_fifo.h - manual commands to the controller through a named pipe (FIFO).
 *
 * Path: $E2E_CMD, else CMD_FIFO_DEFAULT (config.h). The controller creates the FIFO
 * with mode 0666, so a normal user can write to it. One command per line:
 *   ho <CU UE ID> <DU number>   hand the UE over to DU<n> (explicit target cell)
 *   show                        print the UE view (serving DU, RNTI, medians)
 * Example: echo 'ho 1 2' > /tmp/e2e_ctrl.cmd
 * A reader thread parses the lines and queues them. loop/tick.c takes them once per second.
 */
#ifndef E2E_CTRL_CMD_FIFO_H
#define E2E_CTRL_CMD_FIFO_H

#include <stddef.h>
#include <stdint.h>

typedef enum { CMD_HO, CMD_SHOW } cmd_type_t;

typedef struct {
  cmd_type_t type;
  uint32_t cu_ue_id; /* CMD_HO */
  int du_no;         /* CMD_HO: 1 = DU1 */
} cmd_t;

/* Create the FIFO and start the reader thread. Prints the path. */
void cmd_fifo_start(void);

/* Stop the reader thread and remove the FIFO. */
void cmd_fifo_stop(void);

/* Move up to max queued commands into out (oldest first). Returns the number. */
size_t cmd_fifo_take(cmd_t* out, size_t max);

#endif
