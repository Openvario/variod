#include "stf.h"
#include <stdio.h>

float mc_val;
t_polar polar, ideal_polar;
// remember if we have received the Real Polar from XCSoar, yet.
bool real_polar_valid = false;
float ballast;
float degradation;

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

float getMC(){
  return mc_val;
}

/***********************************************
 * formulas for calculating real polar from ideal polar
 * are various places, for instance:
 *
 * Streckensegelflug, Helmut Reichmann, ISBN 3-613-02479-9
 * Page 188 ff
 *
 * Idaflieg.de various papers
 *
 * In XCSoar:
 * GlidePolar::Update() in XCSoar/src/Engine/GlideSolvers/GlidePolar.cpp
 *********************************************/
static void updateRealPolar(){
	// we don't use Ideal Polar and degradation and ballast
	// if we have the Real Polar from XCSoar
	if (real_polar_valid) return;

	// TODO: this formula for loading_factor is incorrect
	// it should be loading_factor = sqrt(total_mass/reference_mass)
	// with total_mass = dry_mass + ballast
	// reference_mass is the mass at which the polar was measured.
	float loading_factor= (ideal_polar.w >0)? sqrt((ideal_polar.w + ballast) / ideal_polar.w) : 1;
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

t_polar* getPolar(){
	return &polar;
}

t_polar* getIdealPolar(){
	return &ideal_polar;
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

float getBallast() {
	return ballast;
}

void setDegradation(float d){
	degradation=d;
	updateRealPolar();
}

float getDegradation(){
	return degradation;
}

void initSTF(){
	setPolar(POL_A,POL_B,POL_C,POL_W);
}
