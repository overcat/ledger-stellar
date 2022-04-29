
/*******************************************************************************
 *   Ledger Nano S - Secure firmware
 *   (c) 2019 Ledger
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 ********************************************************************************/

#ifndef LCX_ECFP_H
#define LCX_ECFP_H

/**
 *
 */
#define CX_ECCINFO_PARITY_ODD 1
#define CX_ECCINFO_xGTn       2

/** List of supported elliptic curves */
enum cx_curve_e {
    CX_CURVE_NONE,
    /* ------------------------ */
    /* --- Type Weierstrass --- */
    /* ------------------------ */
    /** Low limit (not included) of Weierstrass curve ID */
    CX_CURVE_WEIERSTRASS_START = 0x20,

    /** Secp.org */
    CX_CURVE_SECP256K1,
    CX_CURVE_SECP256R1,
#define CX_CURVE_256K1 CX_CURVE_SECP256K1
#define CX_CURVE_256R1 CX_CURVE_SECP256R1
    CX_CURVE_SECP384R1,
    CX_CURVE_SECP521R1,

    /** BrainPool */
    CX_CURVE_BrainPoolP256T1,
    CX_CURVE_BrainPoolP256R1,
    CX_CURVE_BrainPoolP320T1,
    CX_CURVE_BrainPoolP320R1,
    CX_CURVE_BrainPoolP384T1,
    CX_CURVE_BrainPoolP384R1,
    CX_CURVE_BrainPoolP512T1,
    CX_CURVE_BrainPoolP512R1,

/* NIST P256 curve*/
#define CX_CURVE_NISTP256 CX_CURVE_SECP256R1
#define CX_CURVE_NISTP384 CX_CURVE_SECP384R1
#define CX_CURVE_NISTP521 CX_CURVE_SECP521R1

    /* ANSSI P256 */
    CX_CURVE_FRP256V1,

    /* STARK */
    CX_CURVE_Stark256,

    /* BLS */
    CX_CURVE_BLS12_381_G1,

    /** High limit (not included) of Weierstrass curve ID */
    CX_CURVE_WEIERSTRASS_END,

    /* --------------------------- */
    /* --- Type Twister Edward --- */
    /* --------------------------- */
    /** Low limit (not included) of  Twister Edward curve ID */
    CX_CURVE_TWISTED_EDWARD_START = 0x40,

    /** Ed25519 curve */
    CX_CURVE_Ed25519,
    CX_CURVE_Ed448,

    CX_CURVE_TWISTED_EDWARD_END,
    /** High limit (not included) of Twister Edward  curve ID */

    /* ----------------------- */
    /* --- Type Montgomery --- */
    /* ----------------------- */
    /** Low limit (not included) of Montgomery curve ID */
    CX_CURVE_MONTGOMERY_START = 0x60,

    /** Curve25519 curve */
    CX_CURVE_Curve25519,
    CX_CURVE_Curve448,

    CX_CURVE_MONTGOMERY_END
    /** High limit (not included) of Montgomery curve ID */
};
/** Convenience type. See #cx_curve_e. */
typedef enum cx_curve_e cx_curve_t;

/** Return true if curve type is short weierstrass curve */
#define CX_CURVE_IS_WEIRSTRASS(c) \
    (((c) > CX_CURVE_WEIERSTRASS_START) && ((c) < CX_CURVE_WEIERSTRASS_END))

/** Return true if curve type is short weierstrass curve */
#define CX_CURVE_IS_TWISTED_EDWARD(c) \
    (((c) > CX_CURVE_TWISTED_EDWARD_START) && ((c) < CX_CURVE_TWISTED_EDWARD_END))

/** Return true if curve type is short weierstrass curve */
#define CX_CURVE_IS_MONTGOMERY(c) \
    (((c) > CX_CURVE_MONTGOMERY_START) && ((c) < CX_CURVE_MONTGOMERY_END))

#define CX_CURVE_HEADER                                     \
    /** Curve Identifier. See #cx_curve_e */                \
    cx_curve_t curve;                                       \
    /** Curve size in bits */                               \
    unsigned int bit_size;                                  \
    /** component lenth in bytes */                         \
    unsigned int length;                                    \
    /** Curve field */                                      \
    unsigned char WIDE *p;                                  \
    /** @internal 2nd Mongtomery constant for Field */      \
    unsigned char WIDE *Hp;                                 \
    /** Point Generator x coordinate*/                      \
    unsigned char WIDE *Gx;                                 \
    /** Point Generator y coordinate*/                      \
    unsigned char WIDE *Gy;                                 \
    /** Curve order*/                                       \
    unsigned char WIDE *n;                                  \
    /** @internal 2nd Mongtomery constant for Curve order*/ \
    unsigned char WIDE *Hn;                                 \
    /**  cofactor */                                        \
    int h

/**
 * Weirstrass curve :     y^3=x^2+a*x+b        over F(p)
 *
 */
struct cx_curve_weierstrass_s {
    CX_CURVE_HEADER;
    /**  a coef */
    unsigned char WIDE *a;
    /**  b coef */
    unsigned char WIDE *b;
};
/** Convenience type. See #cx_curve_weierstrass_s. */
typedef struct cx_curve_weierstrass_s cx_curve_weierstrass_t;

/*
 * Twisted Edward curve : a*x^2+y^2=1+d*x2*y2  over F(q)
 */
struct cx_curve_twisted_edward_s {
    CX_CURVE_HEADER;
    /**  a coef */
    unsigned char WIDE *a;
    /**  d coef */
    unsigned char WIDE *d;
    /** @internal Square root of -1 or zero */
    unsigned char WIDE *I;
    /** @internal  (q+3)/8 or (q+1)/4*/
    unsigned char WIDE *Qq;
};
/** Convenience type. See #cx_curve_twisted_edward_s. */
typedef struct cx_curve_twisted_edward_s cx_curve_twisted_edward_t;

/*
 * Twisted Edward curve : a*x??+y??=1+d*x??*y??  over F(q)
 */
struct cx_curve_montgomery_s {
    CX_CURVE_HEADER;
    /**  a coef */
    unsigned char WIDE *a;
    /**  b coef */
    unsigned char WIDE *b;
    /** @internal (a + 2) / 4*/
    unsigned char WIDE *A24;
    /** @internal  (p-1)/2 */
    unsigned char WIDE *P1;
};
/** Convenience type. See #cx_curve_montgomery_s. */
typedef struct cx_curve_montgomery_s cx_curve_montgomery_t;

/** Abstract type for elliptic curve domain */
struct cx_curve_domain_s {
    CX_CURVE_HEADER;
};
/** Convenience type. See #cx_curve_domain_s. */
typedef struct cx_curve_domain_s cx_curve_domain_t;

/** Retrieve domain parameters
 *
 * @param curve curve ID #cx_curve_e
 *
 * @return curve parameters
 */
const cx_curve_domain_t WIDE *cx_ecfp_get_domain(cx_curve_t curve);

/** Public Elliptic Curve key */
struct cx_ecfp_public_key_s {
    /** curve ID #cx_curve_e */
    cx_curve_t curve;
    /** Public key length in bytes */
    unsigned int W_len;
    /** Public key value starting at offset 0 */
    unsigned char W[1];
};
/** Private Elliptic Curve key */
struct cx_ecfp_private_key_s {
    /** curve ID #cx_curve_e */
    cx_curve_t curve;
    /** Public key length in bytes */
    unsigned int d_len;
    /** Public key value starting at offset 0 */
    unsigned char d[1];
};
// temporary typedef for scc check
typedef struct cx_ecfp_private_key_s __cx_ecfp_private_key_t;
typedef struct cx_ecfp_public_key_s __cx_ecfp_public_key_t;

/** Up to 256 bits Public Elliptic Curve key */
struct cx_ecfp_256_public_key_s {
    /** curve ID #cx_curve_e */
    cx_curve_t curve;
    /** Public key length in bytes */
    unsigned int W_len;
    /** Public key value starting at offset 0 */
    unsigned char W[65];
};
/** Up to 256 bits Private Elliptic Curve key */
struct cx_ecfp_256_private_key_s {
    /** curve ID #cx_curve_e */
    cx_curve_t curve;
    /** Public key length in bytes */
    unsigned int d_len;
    /** Public key value starting at offset 0 */
    unsigned char d[32];
};
/** Up to 256 bits Extended Private Elliptic Curve key */
struct cx_ecfp_256_extended_private_key_s {
    /** curve ID #cx_curve_e */
    cx_curve_t curve;
    /** Public key length in bytes */
    unsigned int d_len;
    /** Public key value starting at offset 0 */
    unsigned char d[64];
};
/** Convenience type. See #cx_ecfp_256_public_key_s. */
typedef struct cx_ecfp_256_public_key_s cx_ecfp_256_public_key_t;
/** temporary def type. See #cx_ecfp_256_private_key_s. */
typedef struct cx_ecfp_256_private_key_s cx_ecfp_256_private_key_t;
/** Convenience type. See #cx_ecfp_256_extended_private_key_s. */
typedef struct cx_ecfp_256_extended_private_key_s cx_ecfp_256_extended_private_key_t;

/* Do not use those types anymore for declaration, they will become abstract */
typedef struct cx_ecfp_256_public_key_s cx_ecfp_public_key_t;
typedef struct cx_ecfp_256_private_key_s cx_ecfp_private_key_t;

/** Up to 384 bits Public Elliptic Curve key */
struct cx_ecfp_384_public_key_s {
    /** curve ID #cx_curve_e */
    cx_curve_t curve;
    /** Public key length in bytes */
    unsigned int W_len;
    /** Public key value starting at offset 0 */
    unsigned char W[97];
};
/** Up to 384 bits Private Elliptic Curve key */
struct cx_ecfp_384_private_key_s {
    /** curve ID #cx_curve_e */
    cx_curve_t curve;
    /** Public key length in bytes */
    unsigned int d_len;
    /** Public key value starting at offset 0 */
    unsigned char d[48];
};
/** Convenience type. See #cx_ecfp_384_public_key_s. */
typedef struct cx_ecfp_384_private_key_s cx_ecfp_384_private_key_t;
/** Convenience type. See #cx_ecfp_384_private_key_s. */
typedef struct cx_ecfp_384_public_key_s cx_ecfp_384_public_key_t;

/** Up to 512 bits Public Elliptic Curve key */
struct cx_ecfp_512_public_key_s {
    /** curve ID #cx_curve_e */
    cx_curve_t curve;
    /** Public key length in bytes */
    unsigned int W_len;
    /** Public key value starting at offset 0 */
    unsigned char W[129];
};
/** Up to 512 bits Private Elliptic Curve key */
struct cx_ecfp_512_private_key_s {
    /** curve ID #cx_curve_e */
    cx_curve_t curve;
    /** Public key length in bytes */
    unsigned int d_len;
    /** Public key value starting at offset 0 */
    unsigned char d[64];
};
/** Up to 512 bits Extended Private Elliptic Curve key */
struct cx_ecfp_512_extented_private_key_s {
    /** curve ID #cx_curve_e */
    cx_curve_t curve;
    /** Public key length in bytes */
    unsigned int d_len;
    /** Public key value starting at offset 0 */
    unsigned char d[128];
};
/** Convenience type. See #cx_ecfp_512_public_key_s. */
typedef struct cx_ecfp_512_public_key_s cx_ecfp_512_public_key_t;
/** Convenience type. See #cx_ecfp_512_private_key_s. */
typedef struct cx_ecfp_512_private_key_s cx_ecfp_512_private_key_t;
/** Convenience type. See #cx_ecfp_512_extented_private_key_s. */
typedef struct cx_ecfp_512_extented_private_key_s cx_ecfp_512_extented_private_key_t;

/** Up to 640 bits Public Elliptic Curve key */
struct cx_ecfp_640_public_key_s {
    /** curve ID #cx_curve_e */
    cx_curve_t curve;
    /** Public key length in bytes */
    unsigned int W_len;
    /** Public key value starting at offset 0 */
    unsigned char W[161];
};
/** Up to 640 bits Private Elliptic Curve key */
struct cx_ecfp_640_private_key_s {
    /** curve ID #cx_curve_e */
    cx_curve_t curve;
    /** Public key length in bytes */
    unsigned int d_len;
    /** Public key value starting at offset 0 */
    unsigned char d[80];
};
/** Convenience type. See #cx_ecfp_640_public_key_s. */
typedef struct cx_ecfp_640_public_key_s cx_ecfp_640_public_key_t;
/** Convenience type. See #cx_ecfp_640_private_key_s. */
typedef struct cx_ecfp_640_private_key_s cx_ecfp_640_private_key_t;
#endif
