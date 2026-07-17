#if !defined(OMNI_RESPONSE_TOKENIZED)
#  error "response file was not expanded"
#endif

#if !defined(OMNI_RESPONSE_TOKENIZED_SIZE)
#  error "response tokenization expectation is missing"
#endif

#define OMNI_STRINGIZE_IMPL(value) #value
#define OMNI_STRINGIZE(value) OMNI_STRINGIZE_IMPL(value)

static_assert(OMNI_RESPONSE_TOKENIZED_SIZE
  == sizeof(OMNI_STRINGIZE(OMNI_RESPONSE_TOKENIZED)));

int response_tokenization_control;
