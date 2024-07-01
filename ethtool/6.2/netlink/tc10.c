/*
 * tc10.c - netlink implementation of tc10 commands
 *
 * Implementation of "ethtool --set-tc10 <dev>"
 */

#include <errno.h>
#include <string.h>
#include <stdio.h>

#include "../internal.h"
#include "../common.h"
#include "netlink.h"
#include "parser.h"

/* TC10_SET */

static const struct lookup_entry_u8 tc10_mode[] = {
	{ .arg = "sleep",	.val = ETHTOOL_A_TC10_SLEEP},
	{ .arg = "wake",	.val = ETHTOOL_A_TC10_WAKE },
	{}
};

static const struct param_parser stc10_params[] = {
	{
		.arg		= "tc10",
		.type		= ETHTOOL_A_TC10_MODE,
		.handler	= nl_parse_lookup_u8,
		.handler_data	= tc10_mode,
		.min_argc	= 1,
	},
	{}
};

int nl_stc10(struct cmd_context *ctx)
{
	struct nl_context *nlctx = ctx->nlctx;
	struct nl_msg_buff *msgbuff;
	struct nl_socket *nlsk;
	int ret;

	if (netlink_cmd_check(ctx, ETHTOOL_MSG_TC10_SET, false))
		return -EOPNOTSUPP;
	if (!ctx->argc) {
		fprintf(stderr, "ethtool (--set-tc10): parameters missing\n");
		return 1;
	}

	nlctx->cmd = "--set-tc10";
	nlctx->argp = ctx->argp;
	nlctx->argc = ctx->argc;
	nlctx->devname = ctx->devname;
	nlsk = nlctx->ethnl_socket;
	msgbuff = &nlsk->msgbuff;

	ret = msg_init(nlctx, msgbuff, ETHTOOL_MSG_TC10_SET,
		       NLM_F_REQUEST | NLM_F_ACK);
	if (ret < 0)
		return 2;
	if (ethnla_fill_header(msgbuff, ETHTOOL_A_TC10_HEADER,
			       ctx->devname, 0))
		return -EMSGSIZE;

	ret = nl_parser(nlctx, stc10_params, NULL, PARSER_GROUP_MSG, NULL);
	if (ret < 0)
		return 1;

	ret = nlsock_sendmsg(nlsk, NULL);
	if (ret < 0)
		return 76;
	ret = nlsock_process_reply(nlsk, nomsg_reply_cb, nlctx);
	if (ret == 0)
		return 0;
	else
		return nlctx->exit_code ?: 76;
}
