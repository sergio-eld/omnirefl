#if !defined UNIQUE_TARGET
#  error UNIQUE_TARGET must be defined
#endif

int unique_value() {
  return UNIQUE_TARGET;
}
