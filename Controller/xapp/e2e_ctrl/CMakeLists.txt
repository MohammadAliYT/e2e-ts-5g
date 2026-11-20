# e2e_ctrl - built as part of FlexRIC (this folder is symlinked to
# upstream/oai/openair2/E2AP/flexric/examples/xApp/c/e2e_ctrl and added there with
# add_subdirectory(e2e_ctrl); see Controller/patches/flexric-add-e2e-ctrl.patch).
# Link pattern from examples/xApp/c/rc_handover/CMakeLists.txt.

add_executable(e2e_ctrl
  main.c
  e2/nodes.c
  e2/f1_setup.c
  e2/ran_func.c
  e2/kpm_sub.c
  e2/subscribe_du.c
  e2/subscribe_cu.c
  e2/rc_handover.c
  e2/rc_meas_sub.c
  collect/mac.c
  collect/rlc.c
  collect/kpm_values.c
  collect/kpm_du.c
  collect/kpm_cu.c
  collect/cu_ue_id.c
  collect/meas_report_dec.c
  collect/rc_meas.c
  state/ue_table.c
  state/cu_table.c
  state/cu_meas.c
  state/median.c
  state/cell_view.c
  state/ue_view.c
  act/cmd_fifo.c
  act/act.c
  loop/tick.c
  output/csv.c
  output/csv_row.c
  output/state_csv.c
  )

# lets every module include "config.h", "e2/nodes.h", ... from this folder
target_include_directories(e2e_ctrl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(e2e_ctrl
                      PUBLIC
                      e42_xapp
                      asn1_f1ap
                      -pthread
                      -lsctp
                      -ldl
                      -lm
                      )
