#ifndef SPEEDTOFLY 
#define SPEEDTOFLY

#include <math.h>
	      
typedef struct{
  float a, b, c;
  float w;
} t_polar;


float getSTF(float v_sink);
float getNet(float v_sink, float tas);
void setMC(float mc); 
void setPolar(float a, float b, float c, float w);
void setBallast(float b);
void setDegradation(float d);
#endif
