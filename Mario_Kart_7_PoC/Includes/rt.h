#ifndef _RT_H
#define _RT_H
#ifdef __cplusplus
extern "C" {
#endif
typedef struct _RT_HOOK {
	u32 model;
	u32 isEnabled;
	u32 funcAddr;
	u32 bakCode[16];
	u32 jmpCode[16];
	u32 callCode[16];
} RT_HOOK;
void rtInitHook(RT_HOOK* hook, u32 funcAddr, u32 callbackAddr);
void rtEnableHook(RT_HOOK* hook);
void rtDisableHook(RT_HOOK* hook);
u32 rtFlushInstructionCache(void* ptr, u32 size);
u32 rtGenerateJumpCode(u32 dst, u32* buf);
#ifdef __cplusplus
}
#endif
#endif