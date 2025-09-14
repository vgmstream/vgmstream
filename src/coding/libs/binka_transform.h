#ifndef _BINKA_TRANSFORM_
#define _BINKA_TRANSFORM_

void transform_dct(float* coefs, int frame_samples, int transform_size, const float* table);
void transform_rdft(float* coefs, int frame_samples, int transform_size, const float* table);

#endif
