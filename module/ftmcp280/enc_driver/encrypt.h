#ifndef _AES_ENCRYPT_
#define _AES_ENCRYPT_

#define AES_KEY "Grain Media Incorporation"
#define AES_SEI_TYPE    0x55
#define AES_SEI_LENGTH  0x2A

// AES context structure
typedef struct 
{
    unsigned int Ek[44];
    unsigned char Nr;
} EncCtx;
extern int EncCtxIni(EncCtx *pCtx, unsigned char *pKey);
extern int Encrypt(EncCtx *pCtx, unsigned char *pt, unsigned char *ct);

#endif