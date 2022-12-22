#pragma once
#include "CTRPluginFramework.hpp"

namespace CTRPluginFramework {

	struct MK7NetworkBuffer
	{
		u8	bufferType;
		u8* dataPtr;
		u32 dataSize;
		u32 currentSize;

		MK7NetworkBuffer(u8 buffertype, u8* dataPtr, u32 dataSize)
		{
			bufferType = buffertype;
			dataPtr = dataPtr;
			dataSize = dataSize;
			currentSize = 0;
			if (dataPtr) memset(dataPtr, 0, dataSize);
		}

		void Clear()
		{
			if (!dataPtr)
				return;
			currentSize = 0;
			memset(dataPtr, 0, dataSize);
		}

		void Set(u8* newData, u32 newDataSize)
		{
			if (!dataPtr || !newData || newDataSize > dataSize)
				return;
			memcpy(dataPtr, newData, newDataSize);
			currentSize = newDataSize;
		}
		void Add(u8* newData, u32 newDataSize)
		{
			if (!dataPtr || !newData || currentSize + newDataSize > dataSize)
				return;
			memcpy(dataPtr + currentSize, newData, newDataSize);
			currentSize += newDataSize;
		}
	};
}