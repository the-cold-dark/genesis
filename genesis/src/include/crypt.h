
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

#ifdef _SHS_include_

typedef struct {
  uInt H[5];
  uInt W[80];
  Int lenW;
  uInt sizeHi,sizeLo;
} SHS_CTX;

void shsInit(SHS_CTX *ctx);
void shsUpdate(SHS_CTX *ctx, const uChar *dataIn, Int len);
void shsFinal(SHS_CTX *ctx, uChar hashOut[20]);
#ifdef LINT
void shsBlock(const uChar *dataIn, Int len, uChar hashOut[20]);
#endif

#endif
