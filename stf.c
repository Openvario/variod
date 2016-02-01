#include "stf.h"

float mc_val;
t_polar polar;
float ballast;
float degradation;

float getSTF(float v_sink){
//	ddebug_print ("getSTF, Polar: %3f, %3f, %3f, %3f\n", polar.a, polar.b, polar.c, polar.w);
  return sqrt((polar.c+mc_val+v_sink)/polar.a);
}


float getNet(float v_sink, float tas){
	return v_sink-(polar.a*tas*tas+polar.b*tas+polar.c);
}

void setMC(float mc){
  mc_val=mc; 
}
void setPolar(float a, float b, float c, float w){
  polar.a=a;
  polar.b=b;
  polar.c=c;
  polar.w=w;
} 

void setBallast(float b){
	ballast=b;
}

void setDegradation(float d){
	degradation=d;
}
