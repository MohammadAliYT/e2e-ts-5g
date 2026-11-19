# e2e_ctrl - built as part of FlexRIC (this folder is symlinked to
# upstream/oai/openair2/E2AP/flexric/examples/xApp/c/e2e_ctrl and added there with
# add_subdirectory(e2e_ctrl); see Controller/patches/flexric-add-e2e-ctrl.patch).
# Link pattern from examples/xApp/c/rc_handover/CMakeLists.txt.

add_executable(e2e_ctrl
  main.c
  e2/nodes.c
  e2/f1_setup.c
  e2/kpm_sub.c
  e2/subscribe.c
  collect/mac.c
  collect/rlc.c
  collect/kpm_du.c
  state/ue_table.c
  output/csv.c
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
