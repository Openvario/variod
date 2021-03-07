#ifndef SPEEDTOFLY 
#define SPEEDTOFLY

#include <math.h>
	      
#define RHO 1.225
#define POL_A 0.000164
#define POL_B -0.025714
#define POL_C 1.668750
#define POL_W 355

typedef struct{
  float a, b, c;
  float w;
} t_polar;


float getSTF(float v_sink);
float getPlaneSink(float ias);
float getNet(float v_sink, float ias);
float getIAS(float q);
void setMC(float mc);
float getMC();
void setPolar(float a, float b, float c, float w);
t_polar* getPolar();
t_polar* getIdealPolar();
void setRealPolar(float a, float b, float c);
void setBallast(float b);
float getBallast();
void setDegradation(float d);
float getDegradation();
void initSTF();
#endif
