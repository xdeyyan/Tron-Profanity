#ifndef HPP_KERNEL_SHA256
#define HPP_KERNEL_SHA256

#include <string>

const std::string kernel_sha256 = R"(
#define H0 0x6a09e667
#define H1 0xbb67ae85
#define H2 0x3c6ef372
#define H3 0xa54ff53a
#define H4 0x510e527f
#define H5 0x9b05688c
#define H6 0x1f83d9ab
#define H7 0x5be0cd19

#define STR(s) #s
#define XSTR(s) STR(s)

inline uint rotr(uint x, int n) {
  return (x >> n) | (x << (32 - n));
}

inline uint ch(uint x, uint y, uint z) {
  return (x & y) ^ (~x & z);
}

inline uint maj(uint x, uint y, uint z) {
  return (x & y) ^ (x & z) ^ (y & z);
}

inline uint sig0(uint x) { return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3); }
inline uint sig1(uint x) { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); }
inline uint csig0(uint x) { return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22); }
inline uint csig1(uint x) { return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25); }

void sha256(const uint len, const uchar *key, uchar *output) {
  int qua;                     // Message schedule step quantity
  int mod;                     // Message schedule step modulus
  uint A, B, C, D, E, F, G, H; // Compression targets
  uint T1, T2;                 // Compression temp
  uint W[80];                  // Message schedule
  const uint K[64] =           // Cube roots of first 64 primes
      {0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
       0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
       0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
       0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
       0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
       0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
       0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
       0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
       0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
       0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
       0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

#pragma unroll
  for (int i = 0; i < 80; i++) {
    W[i] = 0x00000000;
  }

  qua = len / 4;

  mod = len % 4;
  for (int i = 0; i < qua; i++) {
    W[i] = (key[i * 4 + 0]) << 24;
    W[i] |= (key[i * 4 + 1]) << 16;
    W[i] |= (key[i * 4 + 2]) << 8;
    W[i] |= (key[i * 4 + 3]);
  }

  if (mod == 0) {
    W[qua] = 0x80000000;
  } else if (mod == 1) {
    W[qua] = (key[qua * 4]) << 24;
    W[qua] |= 0x800000;
  } else if (mod == 2) {
    W[qua] = (key[qua * 4]) << 24;
    W[qua] |= (key[qua * 4 + 1]) << 16;
    W[qua] |= 0x8000;
  } else {
    W[qua] = (key[qua * 4]) << 24;
    W[qua] |= (key[qua * 4 + 1]) << 16;
    W[qua] |= (key[qua * 4 + 2]) << 8;
    W[qua] |= 0x80;
  }

  W[15] = len * 8;

#pragma unroll
  for (int i = 16; i < 64; i++) {
    W[i] = sig1(W[i - 2]) + W[i - 7] + sig0(W[i - 15]) + W[i - 16];
  }

  A = H0;
  B = H1;
  C = H2;
  D = H3;
  E = H4;
  F = H5;
  G = H6;
  H = H7;

#pragma unroll
  for (int i = 0; i < 64; i++) {
    T1 = H + csig1(E) + ch(E, F, G) + K[i] + W[i];
    T2 = csig0(A) + maj(A, B, C);
    H = G;
    G = F;
    F = E;
    E = D + T1;
    D = C;
    C = B;
    B = A;
    A = T1 + T2;
  }

  W[0] = A + H0;
  W[1] = B + H1;
  W[2] = C + H2;
  W[3] = D + H3;
  W[4] = E + H4;
  W[5] = F + H5;
  W[6] = G + H6;
  W[7] = H + H7;
  for (int i = 0; i < 8; i++) {
    output[i * 4 + 0] = (W[i] >> 24) & 0xFF;
    output[i * 4 + 1] = (W[i] >> 16) & 0xFF;
    output[i * 4 + 2] = (W[i] >> 8) & 0xFF;
    output[i * 4 + 3] = W[i] & 0xFF;
  }
}

void ethhash_to_tronhash(const uchar *ethhash, uchar *tronhash) {
  uchar hash0[21];
  uchar hash1[32];
  uchar hash2[32];
  for (uint i = 0; i < 20; i++) {
    hash0[i + 1] = ethhash[i];
  }
  hash0[0] = 65;
  sha256(sizeof(hash0), hash0, hash1);
  sha256(sizeof(hash1), hash1, hash2);
  for (uint i = 0; i < 21; i++) {
    tronhash[i] = hash0[i];
  }
  tronhash[21] = hash2[0];
  tronhash[22] = hash2[1];
  tronhash[23] = hash2[2];
  tronhash[24] = hash2[3];
}

__constant char alphabet[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

void base58_encode(const uchar *input, char *output, const int input_len) {
  __private uint digits[32] = {0};
  int digit_count = 1;
  for (int i = 0; i < input_len; i++) {
    uint carry = input[i];
    for (int j = 0; j < digit_count; j++) {
      carry += digits[j] << 8;
      digits[j] = carry % 58;
      carry /= 58;
    }
    while (carry) {
      digits[digit_count++] = carry % 58;
      carry /= 58;
    }
  }

  int zero_count = 0;
  while (zero_count < input_len && input[zero_count] == 0) {
    zero_count++;
  }
  int output_idx = 0;
  output[output_idx++] = alphabet[digits[digit_count - 1]];
  for (int i = digit_count - 2; i >= 0; i--) {
    if (zero_count > 0) {
      zero_count--;
    } else {
      output[output_idx++] = alphabet[digits[i]];
    }
  }
  output[output_idx] = '\0';
}
)";

#endif /* HPP_KERNEL_SHA256 */
