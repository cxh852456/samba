/*
   Unix SMB/CIFS implementation.
   Authentication utility functions
   Copyright (C) Simo Sorce 2010

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_AUTH

#ifdef HAVE_KRB5
NTSTATUS get_user_from_kerberos_info(TALLOC_CTX *mem_ctx,
				     const char *cli_name,
				     const char *princ_name,
				     struct PAC_LOGON_INFO *logon_info,
				     bool *is_mapped,
				     bool *mapped_to_guest,
				     char **ntuser,
				     char **ntdomain,
				     char **username,
				     struct passwd **_pw)
{
	NTSTATUS status;
	char *domain = NULL;
	char *realm = NULL;
	char *user = NULL;
	char *p;
	fstring fuser;
	fstring unixuser;
	struct passwd *pw = NULL;

	DEBUG(3, ("Kerberos ticket principal name is [%s]\n", princ_name));

	p = strchr_m(princ_name, '@');
	if (!p) {
		DEBUG(3, ("[%s] Doesn't look like a valid principal\n",
			  princ_name));
		return NT_STATUS_LOGON_FAILURE;
	}

	user = talloc_strndup(mem_ctx, princ_name, p - princ_name);
	if (!user) {
		return NT_STATUS_NO_MEMORY;
	}

	realm = talloc_strdup(talloc_tos(), p + 1);
	if (!realm) {
		return NT_STATUS_NO_MEMORY;
	}

	if (!strequal(realm, lp_realm())) {
		DEBUG(3, ("Ticket for foreign realm %s@%s\n", user, realm));
		if (!lp_allow_trusted_domains()) {
			return NT_STATUS_LOGON_FAILURE;
		}
	}

	if (logon_info && logon_info->info3.base.domain.string) {
		domain = talloc_strdup(mem_ctx,
					logon_info->info3.base.domain.string);
		if (!domain) {
			return NT_STATUS_NO_MEMORY;
		}
		DEBUG(10, ("Domain is [%s] (using PAC)\n", domain));
	} else {

		/* If we have winbind running, we can (and must) shorten the
		   username by using the short netbios name. Otherwise we will
		   have inconsistent user names. With Kerberos, we get the
		   fully qualified realm, with ntlmssp we get the short
		   name. And even w2k3 does use ntlmssp if you for example
		   connect to an ip address. */

		wbcErr wbc_status;
		struct wbcDomainInfo *info = NULL;

		DEBUG(10, ("Mapping [%s] to short name using winbindd\n",
			   realm));

		wbc_status = wbcDomainInfo(realm, &info);

		if (WBC_ERROR_IS_OK(wbc_status)) {
			domain = talloc_strdup(mem_ctx,
						info->short_name);
			wbcFreeMemory(info);
		} else {
			DEBUG(3, ("Could not find short name: %s\n",
				  wbcErrorString(wbc_status)));
			domain = talloc_strdup(mem_ctx, realm);
		}
		if (!domain) {
			return NT_STATUS_NO_MEMORY;
		}
		DEBUG(10, ("Domain is [%s] (using Winbind)\n", domain));
	}

	/* We have to use fstring for this - map_username requires it. */
	fstr_sprintf(fuser, "%s%c%s", domain, *lp_winbind_separator(), user);

	*is_mapped = map_username(fuser);

	pw = smb_getpwnam(mem_ctx, fuser, unixuser, true);
	if (pw) {
		/* if a real user check pam account restrictions */
		/* only really perfomed if "obey pam restriction" is true */
		/* do this before an eventual mapping to guest occurs */
		status = smb_pam_accountcheck(pw->pw_name, cli_name);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(1, ("PAM account restrictions prevent user "
				  "[%s] login\n", unixuser));
			return status;
		}
	}
	if (!pw) {

		/* this was originally the behavior of Samba 2.2, if a user
		   did not have a local uid but has been authenticated, then
		   map them to a guest account */

		if (lp_map_to_guest() == MAP_TO_GUEST_ON_BAD_UID) {
			*mapped_to_guest = true;
			fstrcpy(fuser, lp_guestaccount());
			pw = smb_getpwnam(mem_ctx, fuser, unixuser, true);
		}

		/* extra sanity check that the guest account is valid */
		if (!pw) {
			DEBUG(1, ("Username %s is invalid on this system\n",
				  fuser));
			return NT_STATUS_LOGON_FAILURE;
		}
	}

	*username = talloc_strdup(mem_ctx, unixuser);
	if (!*username) {
		return NT_STATUS_NO_MEMORY;
	}
	*ntuser = user;
	*ntdomain = domain;
	*_pw = pw;

	return NT_STATUS_OK;
}
#else /* HAVE_KRB5 */
NTSTATUS get_user_from_kerberos_info(TALLOC_CTX *mem_ctx,
				     const char *cli_name,
				     const char *princ_name,
				     struct PAC_LOGON_INFO *logon_info,
				     bool *is_mapped,
				     bool *mapped_to_guest,
				     char **ntuser,
				     char **ntdomain,
				     char **username,
				     struct passwd **_pw)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}
#endif /* HAVE_KRB5 */