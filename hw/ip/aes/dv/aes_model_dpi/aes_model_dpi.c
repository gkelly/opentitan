// Copyright lowRISC contributors.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#include "aes_model_dpi.h"

#include <stdio.h>
#include <stdlib.h>

#include <cstring>

#include "aes.h"
#include "crypto.h"
#include "svdpi.h"

void aes_crypt_dpi(const unsigned char impl_i, const unsigned char mode_i,
                   const svBitVecVal *key_len_i, const svOpenArrayHandle key_i,
                   const svOpenArrayHandle data_i,
                   const svOpenArrayHandle data_o) {
  // get input data from simulator
  unsigned char *key = aes_key_get(key_i);
  unsigned char *ref_in = aes_data_get(data_i);

  // key_len_i is one-hot encoded
  int key_len;
  if (*key_len_i == 0x1) {
    key_len = 16;
  } else if (*key_len_i == 0x2) {
    key_len = 24;
  } else {  // 0x4
    key_len = 32;
  }

  // We encrypt/decrypt one 16B block of data
  int num_elem_ref_out = 16;
  if (impl_i) {
    // OpenSSL/BoringSSL require space for one spare block
    num_elem_ref_out += 16;
  }

  // allocate memory
  unsigned char *ref_out =
      (unsigned char *)malloc(num_elem_ref_out * sizeof(unsigned char));
  if (!ref_out) {
    printf("ERROR: malloc() for aes_crypt_dpi failed");
    return;
  }

  if (impl_i == 0) {
    if (!mode_i) {
      aes_encrypt_block(ref_in, key, key_len, ref_out);
    } else {
      aes_decrypt_block(ref_in, key, key_len, ref_out);
    }
  } else {  // OpenSSL/BoringSSL
    unsigned char iv[16];
    memset(iv, 0, 16);

    // always do an encrypt first as crypto_decrypt() requires
    // the final spare block produced by crypto_encrypt()
    int len = crypto_encrypt(ref_out, iv, ref_in, 16, key, key_len);

    if (mode_i) {
      // prepare
      unsigned char *ref_in_crypto =
          (unsigned char *)malloc(num_elem_ref_out * sizeof(unsigned char));
      if (!ref_in_crypto) {
        printf("ERROR: malloc() for aes_crypt_dpi failed");
        return;
      }
      memcpy((void *)&ref_in_crypto[0], (const void *)&ref_in[0],
             (size_t)(len - 16));
      memcpy((void *)&ref_in_crypto[len - 16], (const void *)&ref_out[len - 16],
             (size_t)16);

      // do the decrypt
      crypto_decrypt(ref_out, iv, ref_in_crypto, len, key, key_len);

      // cleanup
      free(ref_in_crypto);
    }
  }

  // write output data back to simulator
  aes_data_put(data_o, ref_out);

  // free memory
  free(key);
  free(ref_in);

  return;
}

void aes_sub_bytes_dpi(const unsigned char mode_i,
                       const svOpenArrayHandle data_i,
                       const svOpenArrayHandle data_o) {
  // get input data from simulator
  unsigned char *data = aes_data_get(data_i);

  // perform shift rows
  if (!mode_i) {
    aes_sub_bytes(data);
  } else {
    aes_inv_sub_bytes(data);
  }

  // write output data back to simulator
  aes_data_put(data_o, data);

  return;
}

void aes_shift_rows_dpi(const unsigned char mode_i,
                        const svOpenArrayHandle data_i,
                        const svOpenArrayHandle data_o) {
  // get input data from simulator
  unsigned char *data = aes_data_get(data_i);

  // perform shift rows
  if (!mode_i) {
    aes_shift_rows(data);
  } else {
    aes_inv_shift_rows(data);
  }

  // write output data back to simulator
  aes_data_put(data_o, data);

  return;
}

void aes_mix_columns_dpi(const unsigned char mode_i,
                         const svOpenArrayHandle data_i,
                         const svOpenArrayHandle data_o) {
  // get input data from simulator
  unsigned char *data = aes_data_get(data_i);

  // perform shift rows
  if (!mode_i) {
    aes_mix_columns(data);
  } else {
    aes_inv_mix_columns(data);
  }

  // write output data back to simulator
  aes_data_put(data_o, data);

  return;
}

void aes_key_expand_dpi(const unsigned char mode_i, const svBitVecVal *rcon_old,
                        const svBitVecVal *round_i,
                        const svBitVecVal *key_len_i,
                        const svOpenArrayHandle key_i,
                        const svOpenArrayHandle key_o) {
  unsigned char round_key[16];  // just used by model

  // get input data
  unsigned char *key = aes_key_get(key_i);
  unsigned char rcon = (unsigned char)*rcon_old;
  int rnd = (int)*round_i;

  // key_len_i is one-hot encoded
  int key_len;
  if (*key_len_i == 0x1) {
    key_len = 16;
  } else if (*key_len_i == 0x2) {
    key_len = 24;
  } else {  // 0x4
    key_len = 32;
  }

  // perform shift rows
  if (!mode_i) {
    aes_key_expand(round_key, key, key_len, &rcon, rnd);
  } else {
    aes_inv_key_expand(round_key, key, key_len, &rcon, rnd);
  }

  // write output key back to simulator
  aes_key_put(key_o, key);

  return;
}

unsigned char *aes_data_get(const svOpenArrayHandle data_i) {
  unsigned char *data;
  int len;
  svBitVecVal value;

  // alloc data buffer
  len = svSize(data_i, 1);
  data = (unsigned char *)malloc(len * sizeof(unsigned char));
  if (!data) {
    printf("ERROR: malloc() for aes_data_get failed");
    return 0;
  }

  // get data from simulator
  for (int i = 0; i < len; i++) {
    svGetBitArrElem1VecVal(&value, data_i, i);
    data[i] = (unsigned char)value;
  }

  return data;
}

void aes_data_put(const svOpenArrayHandle data_o, unsigned char *data) {
  int len;
  svBitVecVal value;

  // get size of data buffer
  len = svSize(data_o, 1);

  // write output data to simulation
  for (int i = 0; i < len; i++) {
    value = (svBitVecVal)data[i];
    svPutBitArrElem1VecVal(data_o, &value, i);
  }

  // free data
  free(data);

  return;
}

unsigned char *aes_key_get(const svOpenArrayHandle key_i) {
  unsigned char *key;
  int len;
  svBitVecVal value;

  // alloc data buffer
  len = svSize(key_i, 1);
  key = (unsigned char *)malloc(len * 4 * sizeof(unsigned char));
  if (!key) {
    printf("ERROR: malloc() for aes_key_get failed");
    return 0;
  }

  // get data from simulator
  for (int i = 0; i < len; i++) {
    svGetBitArrElem1VecVal(&value, key_i, i);
    key[4 * i + 0] = (unsigned char)(value >> 0);
    key[4 * i + 1] = (unsigned char)(value >> 8);
    key[4 * i + 2] = (unsigned char)(value >> 16);
    key[4 * i + 3] = (unsigned char)(value >> 24);
  }

  return key;
}

void aes_key_put(const svOpenArrayHandle key_o, unsigned char *key) {
  int len;
  svBitVecVal value;

  // get size of data buffer
  len = svSize(key_o, 1);

  // write output data to simulation
  for (int i = 0; i < len; i++) {
    value = (svBitVecVal)(((key[4 * i + 0] << 0) & 0xFF) +
                          ((key[4 * i + 1] << 8) & 0xFF00) +
                          ((key[4 * i + 2] << 16) & 0xFF0000) +
                          ((key[4 * i + 3] << 24) & 0xFF000000));
    svPutBitArrElem1VecVal(key_o, &value, i);
  }

  // free data
  free(key);

  return;
}
