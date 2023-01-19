/*  vario_app - Audio Vario application - http://www.openvario.org/
    Copyright (C) 2014  The openvario project
    A detailed list of copyright holders can be found in the file "AUTHORS"

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 3
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#include "utils.h"

#include <stdlib.h>
#include <string.h>


// copies n floats to fv[]
bool read_float_from_sentence(int n,float fv[],char *str,const char *delim)
{
	char *comma;
	if (n > NUM_FV) return false;

	for (int i=0; i < n; i++) {
		if (i!=n-1){
			fv[i] = strtof(str,&comma);
			str=comma;
			++str;
		}
		else{
			fv[i] = strtof(str,NULL);
		}
		if(fv[i]==0.0f){
			return false;
		}
	}
	return true;
}

