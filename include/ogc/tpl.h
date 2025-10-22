#ifndef __OGC_TPL_H__
#define __OGC_TPL_H__

#include <stdio.h>
#include "gx.h"

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

// tdf file
typedef struct _tplfile {
	int type;
	int ntextures;
	void *texdesc;
	FILE *tpl_file;
} TPLFile;

s32 TPL_OpenTPLFromFile(TPLFile* tdf, const char* file_name);
s32 TPL_OpenTPLFromHandle(TPLFile* tdf, FILE *handle);
s32 TPL_OpenTPLFromMemory(TPLFile* tdf, void *memory,u32 len);
s32 TPL_GetTexture(TPLFile *tdf,s32 id,GXTexObj *texObj);
s32 TPL_GetTextureCI(TPLFile *tdf,s32 id,GXTexObj *texObj,GXTlutObj *tlutObj,u8 tluts);
s32 TPL_GetTextureInfo(TPLFile *tdf,s32 id,u32 *fmt,u16 *width,u16 *height);
void TPL_CloseTPLFile(TPLFile *tdf);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
