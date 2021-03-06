#include <math.h>
#include "filtering.h"

Buffer::Buffer(int buf_size): BUFSIZE(buf_size) {
    front = -1;
    back = -1;
}

int Buffer::length()
{
    if (front == -1) return 0;
    return (front - back + BUFSIZE) % BUFSIZE + 1;
}

void Buffer::reset() {
    front = back = -1;
}

float Buffer::difference(int idx_a, int idx_b) const
{
  float a = getFloatAtOffset(idx_a);
  float b = getFloatAtOffset(idx_b);
  a = a - b;
  if (fabs(a) >= 0.0001) return a;
  return 0.0f;
}

float Buffer::distance(int idx_a, int idx_b) const
{
  float a = difference(idx_a, idx_b);
  a = fabs(a);
  if (a<0.0001) a = 0.0f;
  return a;
}

float Buffer::average(int n)
{
  float res = 0.0f;
  if (front == -1) return 0.0f; // empty buffer
  if (n == 0) return 0.0f;
  int len = length();
  if (len <= 0) return 0.0f;
  if (len < n) n = len;
  int i = (front + BUFSIZE - n + 1) % BUFSIZE;
  while (i != front)
  {
#ifdef TESTING
    std::cout << i << ":" <<getFloatAtIndex(i) << " ";
#endif
    res += getFloatAtIndex(i);
    i = (i+1) % BUFSIZE;
  }
#ifdef TESTING
  std::cout << i << ":" <<getFloatAtIndex(i) << "\n";
#endif
  res += getFloatAtIndex(i);
  return res / n;
}

void LongBuffer::append(long val)
{
    front = (front + 1) % BUFSIZE;
    buf[front] = val;
    if (front == back || back == -1)
        back = (back + 1) % BUFSIZE;
}
long LongBuffer::get(unsigned int n) const
{
    return buf[ (front + BUFSIZE - n) % BUFSIZE];
}
void LongBuffer::set(unsigned int n, long value)
{
    buf[ (front + BUFSIZE - n) % BUFSIZE] = value;
}


float LongBuffer::getFloatAtOffset(int offset) const
{
    return get(offset);
}

float LongBuffer::getFloatAtIndex(int idx) const
{
    return buf[idx];
}

float FloatBuffer::getFloatAtOffset(int offset) const
{
    return get(offset);
}

float FloatBuffer::getFloatAtIndex(int idx) const
{
    return buf[idx];
}

void FloatBuffer::append( float val)
{
    front = (front + 1) % BUFSIZE;
    buf[front] = val;
    if (front == back || back == -1)
      back = (back + 1) % BUFSIZE;
}

float FloatBuffer::get(unsigned int n) const
{
    return buf[ (front + BUFSIZE - n) % BUFSIZE];
}

void FloatBuffer::set(unsigned int n, float value)
{
    buf[ (front + BUFSIZE - n) % BUFSIZE] = value;
}

float FloatBuffer::slopeFromLeastSquaresFit(const LongBuffer &time_buf)
{
  float sumX = 0.0f, sumY = 0.0f, sumXY = 0.0f;
  float sumXsquared = 0.0f, sumYsquared = 0.0f;
  int n = length()-1;
	if (n<=0) return 0;
  float t0 = time_buf.get(n);
  for (int i = n; i>0; i--)
  {
    float y = (get(i) - get(n));
    float x = time_buf.get(i) - t0;
    sumX += x;
    sumY += y;
    sumXsquared += x*x;
    sumYsquared += y*y;
    sumXY += x*y;
  }
  float denom = (float)n*sumXsquared - sumX*sumX;
	if (fabs(denom) < 0.00001f ) return 0.0f;
  float m = ((float)n * sumXY - sumX*sumY) / denom;
  //float c = (sumXsquared * sumY - sumXY * sumX) / denom;
  return m;
}
