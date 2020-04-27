#include "nmea_parser.h"
#include "def.h"

extern int g_debug;
extern int g_foreground;
extern FILE *fp_console;

void parse_NMEA_sensor(char* message, t_sensor_context* sensors)
{
  // expect 1 NMEA sentence at a time!
  // sentence must be terninated with '\0', '\n', or '\r'
  char *val;
  float fv;
  static char buffer[2001];
  const char delimiter[]=",*";
  char *ptr=NULL;

  // copy string and initialize strtok function
  strncpy(buffer, message, strlen(message));
  ptr = strtok(buffer, delimiter);

  if (strcmp(ptr,"$POV") == 0) ptr = strtok(NULL, delimiter);
  else ptr = NULL;

  while (ptr != NULL) {
    if (*ptr == '\n' || *ptr == '\r') return; // end of sentence

    val = strtok(NULL, delimiter);
    if (val == NULL) return; // there is no value after the qualifier

    fv = atof(val);

    if (strcmp(ptr,"E") == 0) {
      // TE vario value
      sensors->e = fv;

    } else if (strcmp(ptr,"Q") == 0) {
      // Airspeed value
      sensors->q = fv;
    }

    // get next part of string
    ptr = strtok(NULL, delimiter);
  }
}

void parse_NMEA_command(char* message)
{
  // expect 1 NMEA sentence at a time!
  // sentence must be terninated with '\0', '\n', or '\r'
  char *val;
  static char buffer[100];
  const char delimiter[]=",*";
  char *ptr;
  t_polar polar;

  // copy string and initialize strtok function
  strncpy(buffer, message, strlen(message));
  ptr = strtok(buffer, delimiter);

  if (ptr && (strcmp(ptr,"$POV") == 0)) ptr = strtok(NULL, delimiter);
  else ptr = NULL;

  if (ptr && (strcmp(ptr,"C") == 0)) ptr = strtok(NULL, delimiter);
  else ptr = NULL;

  while (ptr != NULL) {
    switch (*ptr) {
    case '\n':
    case '\r':
      return; // end of sentence

    case 'M':
      if (strcmp(ptr,"MC") == 0) {
        //Set McCready
        //printf("Set McCready\n");
        val = strtok(NULL, delimiter);
        if (val == NULL) return;
        setMC(atof(val));
        debug_print("Get McCready Value: %f\n",atof(val));
      }
      break;

    case 'W':
      if (strcmp(ptr,"WL") == 0) {
        //Set Wingload/Ballast
        // we are getting total_mass / dry_mass
        // we should get total_mass / reference_mass
        // TODO: fix this in XCSoar
        val = strtok(NULL, delimiter);
        if (val == NULL) return;
        setBallast(atof(val));
        debug_print("Get Ballast Value: %f\n",atof(val));
      }
      break;

    case 'B':
      if (strcmp(ptr,"BU") == 0) {
        //Set Bugs
        val = strtok(NULL, delimiter);
        if (val == NULL) return;
        setDegradation(atof(val));
        debug_print("Get Bugs Value: %f\n",atof(val));
      }
      break;

    case 'P':
      if (strcmp(ptr,"POL") == 0) {
        // UNDOCUMENTED feature
        //Set Polar
        val = strtok(NULL, delimiter);
        if (val == NULL) return;
        polar.a=atof(val);
        val = strtok(NULL, delimiter);
        if (val == NULL) return;
        polar.b=atof(val);
        val = strtok(NULL, delimiter);
        if (val == NULL) return;
        polar.c=atof(val);
        val = strtok(NULL, delimiter);
        if (val == NULL) return;
        polar.w=atof(val);
        setPolar(polar.a, polar.b, polar.c, polar.w);
        debug_print("Get Polar POL: %f, %f, %f, %f\n",
                    polar.a,polar.b,polar.c,polar.w);
      }
      break;

    case 'I':
      if (strcmp(ptr,"IPO") == 0) {
        //Set ideal Polar
        val = strtok(NULL, delimiter);
        if (val == NULL) return;
        polar.a=atof(val);
        val = strtok(NULL, delimiter);
        if (val == NULL) return;
        polar.b=atof(val);
        val = strtok(NULL, delimiter);
        if (val == NULL) return;
        polar.c=atof(val);
        setIdealPolar(polar.a, polar.b, polar.c);
        debug_print("Get Polar IPO: %f, %f, %f\n",
                    polar.a,polar.b,polar.c);
      }
      break;

    case 'R':
      if (strcmp(ptr,"RPO") == 0) {
        //Set real Polar
        val = strtok(NULL, delimiter);
        if (val == NULL) return;
        polar.a=atof(val);
        val = strtok(NULL, delimiter);
        if (val == NULL) return;
        polar.b=atof(val);
        val = strtok(NULL, delimiter);
        if (val == NULL) return;
        polar.c=atof(val);
        setRealPolar(polar.a, polar.b, polar.c);
        debug_print("Get Polar RPO: %f, %f, %f\n",
                    polar.a,polar.b,polar.c);
      }
      break;

    case 'N':
      if (strcmp(ptr,"NA") == 0) {
        // Requested data not avaliable
        // skip the rest of the sentence
        return;
      }
      break;

    case 'S':
      if (strcmp(ptr,"STF") == 0) {
        // Set STF Mode
        debug_print("Set STF Mode\n");
        set_vario_mode(stf);
      }
      break;

    case 'V':
      if (strcmp(ptr,"VAR") == 0) {
        // Set Vario Mode
        debug_print("Set Vario Mode\n");
        set_vario_mode(vario);

      } else if (strcmp(ptr,"VU") == 0) {
        // volume up
        debug_print("Volume up\n");
        change_volume(+10.0);

      } else if (strcmp(ptr,"VD") == 0) {
        // volume down
        debug_print("Volume down\n");
        change_volume(-10.0);

      } else if (strcmp(ptr,"VM") == 0) {
        // Toggle Mute
        debug_print("Toggle Mute\n");
        toggle_mute();
      }
      break;
    }
    // get next part of string
    ptr = strtok(NULL, delimiter);
  }
}
