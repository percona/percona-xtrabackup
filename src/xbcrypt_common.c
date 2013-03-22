/******************************************************
Copyright (c) 2011 Percona Ireland Ltd.

Encryption configuration file interface for XtraBackup.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

*******************************************************/

#include <my_base.h>
#include "common.h"
#include "xbcrypt.h"
#include <gcrypt.h>


my_bool
xb_crypt_read_key_file(const char *filename, void** key, uint *keylength)
{
	FILE *fp;

	if (!(fp = my_fopen(filename, O_RDONLY, MYF(0)))) {
		msg("%s:%s: unable to open config file \"%s\", errno(%d)\n",
			my_progname, __FUNCTION__, filename, my_errno);
		return FALSE;
	}

	fseek(fp, 0 , SEEK_END);
	*keylength = ftell(fp);
	rewind(fp);
	*key = my_malloc(*keylength, MYF(MY_FAE));
	*keylength = fread(*key, 1, *keylength, fp);
	my_fclose(fp, MYF(0));
	return TRUE;
}
