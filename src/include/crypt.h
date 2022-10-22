#ifndef cdc_crypt_h
#define cdc_crypt_h

#define SHS_DIGEST_SIZE 20
#define SHS_OUTPUT_SIZE 120

char * shs_crypt(const unsigned char * pw,
                 const Int pl,
                 const unsigned char * sp,
                 const Int sl,
                 char * passwd);

cStr * strcrypt(cStr * str, cStr * seed);

Int match_crypted(cStr * encrypted, cStr * possible);

/*
// SHS stuff
*/

typedef struct {
  uInt H[5];
  uInt W[80];
  Int lenW;
  uInt sizeHi,sizeLo;
} SHS_CTX;

extern void shsInit(SHS_CTX *ctx);
extern void shsUpdate(SHS_CTX *ctx, const unsigned char *dataIn, Int len);
extern void shsFinal(SHS_CTX *ctx, unsigned char hashOut[20]);
#ifdef LINT
extern void shsBlock(const unsigned char *dataIn, Int len, unsigned char hashOut[20]);
#endif

#endif
