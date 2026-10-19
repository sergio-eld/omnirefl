#if defined ALPHA_TARGET
int shared_value() {
  return ALPHA_TARGET;
}
#elif defined BETA_TARGET
int shared_value() {
  return BETA_TARGET;
}
#else
#  error expected ALPHA_TARGET or BETA_TARGET
#endif
