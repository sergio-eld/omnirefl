#include <omnirefl_driver_working_directory.h>

#if !defined(OMNI_DRIVER_WORKING_DIRECTORY)
#  error "relative GCC toolchain was not resolved from the working directory"
#endif

struct driver_working_directory_record {
  int value;
};
