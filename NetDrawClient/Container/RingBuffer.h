#pragma once

#include <stdint.h>

class RingBuffer
{
public:
	RingBuffer();
	RingBuffer(int size);

	//void Resize(int size);

	int GetBufferSize()
	{
		return m_bufferSize;
	}

	int GetUseSize();
	int GetFreeSize();

	int DirectEnqueueSize();
	int DirectDequeueSize();

	int Enqueue(const void* data, int size);
	int Dequeue(void* out_data, int size);

	int Peek(void* out_data, int size);

	int MoveRear(int size);
	int MoveFront(int size);

	void ClearBuffer()
	{
		m_pFront = m_buffer;
		m_pRear = m_buffer;
		// m_pFront = m_pRear;
	}

	char* GetFrontPtr()
	{
		return m_pFront;
	}
	char* GetRearPtr()
	{
		return m_pRear;
	}

private:
	void Init(int size);

private:
	static constexpr int DEFAULT_BUFFER_SIZE = 10000;

	char* m_pFront;
	char* m_pRear;
	char* m_pEndOfBuffer;

	int m_bufferSize;
	char* m_buffer;
};
