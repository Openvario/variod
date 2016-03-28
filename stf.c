#include "stf.h"
#include <stdio.h>

float mc_val;
t_polar polar;
float ballast;
float degradation;

float getSTF(float v_sink){
	//printf("polar: %f,%f,%f, mcc: %f\n", polar.a, polar.b, polar.c, mc_val);
	float stfsq, speed;
	stfsq = (polar.c+mc_val+v_sink)/polar.a;
	speed=(stfsq > 0)? sqrt(stfsq):0;

  	return speed/3.6;
}


float getNet(float v_sink, float ias){
	//printf("polar: %f,%f,%f, mcc: %f\n", polar.a, polar.b, polar.c, mc_val);
	ias*=3.6;
	return v_sink-(polar.a*ias*ias+polar.b*ias+polar.c);
}

float getIAS(float q){
	return sqrt(2*q / RHO);
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

void initSTF(){
 setPolar(POL_A,POL_B,POL_C,POL_W);
}
