#ifndef SPEEDTOFLY
#define SPEEDTOFLY

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
float getNet(float v_sink, float ias);
float getIAS(float q);
void setMC(float mc);
float getMC(void);
void setPolar(float a, float b, float c, float w);
t_polar* getPolar(void);
t_polar* getIdealPolar(void);
void setRealPolar(float a, float b, float c);
void setBallast(float b);
float getBallast(void);
void setDegradation(float d);
float getDegradation(void);
void initSTF(void);
#endif
