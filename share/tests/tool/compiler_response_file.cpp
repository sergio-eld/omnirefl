#if !defined(OMNI_RESPONSE_FILE_EXPANDED)
#  error "compiler response file was not expanded"
#endif
#if !defined(OMNI_RESPONSE_TEXT)
#  error "quoted response-file argument was not expanded"
#endif

static_assert(11 == sizeof(OMNI_RESPONSE_TEXT));

int compiler_response_file_control;
