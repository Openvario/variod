#include "stf.h"

float getSTF(t_polar* p, float v_sink, float mc){

  return sqrt((p->c+mc+v_sink)/p->a);
}
