#ifndef SPEEDTOFLY 
#define SPEEDTOFLY

#include <math.h>
	      
typedef struct{
  float a, b, c;
} t_polar;


float getSTF(t_polar p, float v_sink, float mc);


#endif
