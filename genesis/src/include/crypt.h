
#define SHS_DIGEST_SIZE 20
#define SHS_OUTPUT_SIZE 120

char * shs_crypt(const unsigned char * pw,
                 const int pl,
                 const unsigned char * sp,
                 const int sl,
                 char * passwd);

cStr * strcrypt(cStr * str, cStr * seed);

Int match_crypted(cStr * encrypted, cStr * possible);

/*
// SHS stuff
*/

#ifdef _SHS_include_

typedef struct {
  unsigned long H[5];
  unsigned long W[80];
  int lenW;
  unsigned long sizeHi,sizeLo;
} SHS_CTX;

void shsInit(SHS_CTX *ctx);
void shsUpdate(SHS_CTX *ctx, const unsigned char *dataIn, int len);
void shsFinal(SHS_CTX *ctx, unsigned char hashOut[20]);
#ifdef LINT
void shsBlock(const unsigned char *dataIn, int len, unsigned char hashOut[20]);
#endif

#endif
