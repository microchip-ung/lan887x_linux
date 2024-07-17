// SPDX-License-Identifier: GPL-2.0-only

#include "netlink.h"
#include "common.h"
#include <linux/phy.h>

struct tc10_req_info {
	struct ethnl_req_info		base;
};

struct tc10_reply_data {
	struct ethnl_reply_data                 base;
	enum ethtool_tc10_state			tc10_state;
};

#define TC10_STATE_REPDATA(__reply_base) \
	container_of(__reply_base, struct tc10_reply_data, base)

const struct nla_policy ethnl_tc10_get_policy[ETHTOOL_A_TC10_HEADER + 1] = {
	[ETHTOOL_A_TC10_HEADER]	= NLA_POLICY_NESTED(ethnl_header_policy),
};

static int tc10_get_tc10_state(struct net_device *dev)
{
	struct phy_device *phydev = dev->phydev;
	int ret;

	if (!phydev)
		return -EOPNOTSUPP;

	mutex_lock(&phydev->lock);
	if (!phydev->drv || !phydev->drv->get_tc10_state)
		ret = -EOPNOTSUPP;
	else
		ret = phydev->drv->get_tc10_state(phydev);
	mutex_unlock(&phydev->lock);

	return ret;
};

static int tc10_prepare_data(const struct ethnl_req_info *req_base,
			     struct ethnl_reply_data *reply_base,
				 const struct genl_info *info)
{
	struct tc10_reply_data *data = TC10_STATE_REPDATA(reply_base);
	struct net_device *dev = reply_base->dev;
	int ret;

	ret = ethnl_ops_begin(dev);
	if (ret < 0)
		return ret;

	ret = tc10_get_tc10_state(dev);
	if (ret < 0 && ret != -EOPNOTSUPP)
		goto out;
	data->tc10_state = ret;

out:
	ethnl_ops_complete(dev);
	return ret;
}

static int tc10_reply_size(const struct ethnl_req_info *req_base,
			   const struct ethnl_reply_data *reply_base)
{
	struct tc10_reply_data *data = TC10_STATE_REPDATA(reply_base);
	int len;

	len = nla_total_size(sizeof(u8)); /* TC10_STATE_LINK */

	if (data->tc10_state != -EOPNOTSUPP)
		len += nla_total_size(sizeof(u8));

	return len;
}

static int tc10_fill_reply(struct sk_buff *skb,
			   const struct ethnl_req_info *req_base,
			   const struct ethnl_reply_data *reply_base)
{
	struct tc10_reply_data *data = TC10_STATE_REPDATA(reply_base);

	if (data->tc10_state != -EOPNOTSUPP &&
	    nla_put_u8(skb, ETHTOOL_A_TC10_STATE, data->tc10_state))
		return -EMSGSIZE;

	return 0;
}

const struct ethnl_request_ops ethnl_tc10_request_ops = {
	.request_cmd		= ETHTOOL_MSG_TC10_STATE_GET,
	.reply_cmd		= ETHTOOL_MSG_TC10_STATE_GET_REPLY,
	.hdr_attr		= ETHTOOL_A_TC10_HEADER,
	.req_info_size		= sizeof(struct tc10_req_info),
	.reply_data_size	= sizeof(struct tc10_reply_data),

	.prepare_data		= tc10_prepare_data,
	.reply_size		= tc10_reply_size,
	.fill_reply		= tc10_fill_reply,
};

/* TC10_SET */

const struct nla_policy ethnl_tc10_set_policy[] = {
	[ETHTOOL_A_TC10_HEADER]         =
		NLA_POLICY_NESTED(ethnl_header_policy),
	[ETHTOOL_A_TC10_MODE]          = { .type = NLA_U8},
};

int ethnl_set_tc10(struct sk_buff *skb, struct genl_info *info)
{
	struct ethnl_req_info req_info = {};
	struct nlattr **tb = info->attrs;
	const struct ethtool_ops *ops;
	struct net_device *dev;
	bool mod = false;
	u8 tc10 = 0;
	int ret;

	ret = ethnl_parse_header_dev_get(&req_info, tb[ETHTOOL_A_TC10_HEADER],
					 genl_info_net(info), info->extack,
					 true);
	if (ret < 0)
		return ret;
	dev = req_info.dev;
	ops = dev->ethtool_ops;
	ret = -EOPNOTSUPP;
	if (!ops->set_tc10)
		goto out_dev;

	rtnl_lock();
	ret = ethnl_ops_begin(dev);
	if (ret < 0)
		goto out_rtnl;

	if (tb[ETHTOOL_A_TC10_MODE])
		ethnl_update_u8(&tc10, tb[ETHTOOL_A_TC10_MODE], &mod);

	ret = 0;
	if (!mod)
		goto out_ops;

	ret = dev->ethtool_ops->set_tc10(dev, tc10);
	if (ret < 0)
		goto out_ops;
out_ops:
	ethnl_ops_complete(dev);
out_rtnl:
	rtnl_unlock();
out_dev:
	ethnl_parse_header_dev_put(&req_info);
	return ret;
}
