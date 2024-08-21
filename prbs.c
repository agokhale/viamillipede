#include "worker.h"
// pseudorandom generator suitable for no other purpose than to amuse me
#define salt 0x42F0E1EBA9EA3693 // from ECMA 182
#define prbs_sequence (permutation ^ (salt * i))
unsigned long *prbs_gen(unsigned long *payload, unsigned long permutation,
                        size_t size) {
  unsigned long *return_payload = payload;
  for (unsigned long i = 0; i < (size / sizeof(unsigned long)); i++) {
    payload[i] = prbs_sequence;
  }
  return (return_payload);
}
int prbs_verify(unsigned long *payload, unsigned long permutation,
                size_t size) {
  // exit 1 if  the payload verifies
  for (unsigned long i = 0; i < (size / sizeof(unsigned long)); i++) {
    if (payload[i] != prbs_sequence)
      return (0);
  }
  return (1);
}
