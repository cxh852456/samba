#include "../libcli/netlogon.h"

/* The following definitions come from libsmb/clidgram.c  */

bool send_getdc_request(struct messaging_context *msg_ctx,
			const struct sockaddr_storage *dc_ss,
			const char *domain_name,
			const struct dom_sid *sid,
			uint32_t nt_version,
			int dgm_id);
bool receive_getdc_response(TALLOC_CTX *mem_ctx,
			    const struct sockaddr_storage *dc_ss,
			    const char *domain_name,
			    int dgm_id,
			    uint32_t *nt_version,
			    const char **dc_name,
			    struct netlogon_samlogon_response **reply);