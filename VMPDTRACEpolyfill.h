#ifndef VMPDTRACE
//Polyfill for missing dtrace probes
  #undef DTRACE_PROBE // undef these for testing XXXX
  #undef DTRACE_PROBE1
  #undef DTRACE_PROBE2
  #define DTRACE_PROBE(WHO, PROBE )
  #define DTRACE_PROBE1(WHO, PROBE, ARG1) 
  #define DTRACE_PROBE2(WHO, PROBE, ARG1,ARG2)
#endif
