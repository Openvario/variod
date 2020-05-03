#include "stf.h"
#include <stdio.h>

float mc_val = 1.0;
t_polar polar, ideal_polar;
bool real_polar_valid = false;
float ballast = 1.0;
float degradation = 1.0 ;

float getSTF(float v_sink){
	float stfsq, speed;
	stfsq = (polar.c+mc_val+v_sink)/polar.a;
	speed=(stfsq > 0)? sqrt(stfsq):0;

  	return speed/3.6;
}


float getPlaneSink(float ias){
	return polar.a*ias*ias+polar.b*ias+polar.c;
}

float getNet(float v_sink, float ias){
	//printf("polar: %f,%f,%f, mcc: %f\n", polar.a, polar.b, polar.c, mc_val);
	ias*=3.6;
	return v_sink-getPlaneSink(ias);
}



float getIAS(float q){
	return sqrt(2*q / RHO);
}

void setMC(float mc){
  mc_val=mc; 
}

static void updateRealPolar(){
  // we don't use Ideal Polar and degradation and ballast
  // if we have a Real Polar from XCSoar
  if (real_polar_valid) return;

  float loading_factor= (ballast >0)? sqrt(ballast) : 1;
  float deg_inv = (degradation>0)? 1.0/degradation : 1;

  polar.a= deg_inv * ideal_polar.a / loading_factor;
  polar.b= deg_inv * ideal_polar.b;
  polar.c= deg_inv * ideal_polar.c * loading_factor;
}


void setPolar(float a, float b, float c, float w){
  ideal_polar.a=a;
  ideal_polar.b=b;
  ideal_polar.c=c;
  ideal_polar.w=w;
  updateRealPolar();
} 

void setIdealPolar(float a, float b, float c){
  ideal_polar.a=a;
  ideal_polar.b=b;
  ideal_polar.c=c;
  updateRealPolar();
} 

void setRealPolar(float a, float b, float c){
  polar.a=a;
  polar.b=b;
  polar.c=c;
  real_polar_valid = true;
} 

void setBallast(float b){
	ballast=b;
	updateRealPolar();
}

void setDegradation(float d){
	degradation=d;
	updateRealPolar();
}

void initSTF(){
 setPolar(POL_A,POL_B,POL_C,POL_W);
}
