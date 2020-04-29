#ifndef SPEEDTOFLY 
#define SPEEDTOFLY

#include <math.h>
	      
#define RHO 1.225
#define POL_A 0.000164
#define POL_B -0.025714
#define POL_C 1.668750
#define POL_W 1.0

typedef struct{
  float a, b, c;
  float w;
} t_polar;


float getSTF(float v_sink);
float getPlaneSink(float ias);
float getNet(float v_sink, float ias);
float getIAS(float q);
void setMC(float mc); 
void setPolar(float a, float b, float c, float w);
void setIdealPolar(float a, float b, float c);
void setRealPolar(float a, float b, float c);
void setBallast(float b);
void setDegradation(float d);
void initSTF();
#endif
