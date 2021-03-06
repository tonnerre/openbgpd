/*	$OpenBSD$ */

/*
 * Copyright (c) 2003, 2004 Henning Brauer <henning@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifndef __BGPD_H__
#define	__BGPD_H__

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <net/route.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <net/pfkeyv2.h>

#include <poll.h>
#include <stdarg.h>
#include <string.h>

#include <imsg.h>

#define	BGP_VERSION			4
#define	BGP_PORT			179
#define	CONFFILE			"/etc/bgpd.conf"
#define	BGPD_USER			"_bgpd"
#define	PEER_DESCR_LEN			32
#define	PFTABLE_LEN			16
#define	TCP_MD5_KEY_LEN			80
#define	IPSEC_ENC_KEY_LEN		32
#define	IPSEC_AUTH_KEY_LEN		20

#define	MAX_PKTSIZE			4096
#define	MIN_HOLDTIME			3
#define	READ_BUF_SIZE			65535
#define	RT_BUF_SIZE			16384
#define	MAX_RTSOCK_BUF			128 * 1024

#define	BGPD_OPT_VERBOSE		0x0001
#define	BGPD_OPT_VERBOSE2		0x0002
#define	BGPD_OPT_NOACTION		0x0004
#define	BGPD_OPT_FORCE_DEMOTE		0x0008

#define	BGPD_FLAG_NO_FIB_UPDATE		0x0001
#define	BGPD_FLAG_NO_EVALUATE		0x0002
#define	BGPD_FLAG_REFLECTOR		0x0004
#define	BGPD_FLAG_REDIST_STATIC		0x0008
#define	BGPD_FLAG_REDIST_CONNECTED	0x0010
#define	BGPD_FLAG_REDIST6_STATIC	0x0020
#define	BGPD_FLAG_REDIST6_CONNECTED	0x0040
#define	BGPD_FLAG_NEXTHOP_BGP		0x0080
#define	BGPD_FLAG_NEXTHOP_DEFAULT	0x1000
#define	BGPD_FLAG_DECISION_MASK		0x0f00
#define	BGPD_FLAG_DECISION_ROUTEAGE	0x0100
#define	BGPD_FLAG_DECISION_TRANS_AS	0x0200
#define	BGPD_FLAG_DECISION_MED_ALWAYS	0x0400

#define	BGPD_LOG_UPDATES		0x0001

#define	SOCKET_NAME			"/var/run/bgpd.sock"

#define	F_BGPD_INSERTED		0x0001
#define	F_KERNEL		0x0002
#define	F_CONNECTED		0x0004
#define	F_NEXTHOP		0x0008
#define	F_DOWN			0x0010
#define	F_STATIC		0x0020
#define	F_DYNAMIC		0x0040
#define	F_REJECT		0x0080
#define	F_BLACKHOLE		0x0100
#define	F_LONGER		0x0200
#define	F_CTL_DETAIL		0x1000	/* only used by bgpctl */
#define	F_CTL_ADJ_IN		0x2000
#define	F_CTL_ADJ_OUT		0x4000

/*
 * Limit the number of control messages generated by the RDE and queued in
 * session engine. The RDE limit defines how many imsg are generated in
 * one poll round. Then if the SE limit is hit the RDE control socket will no
 * longer be polled.
 */
#define RDE_RUNNER_ROUNDS	100
#define SESSION_CTL_QUEUE_MAX	10000

enum {
	PROC_MAIN,
	PROC_SE,
	PROC_RDE
} bgpd_process;

enum reconf_action {
	RECONF_NONE,
	RECONF_KEEP,
	RECONF_REINIT,
	RECONF_DELETE
};

/* Address Family Numbers as per RFC 1700 */
#define	AFI_UNSPEC	0
#define	AFI_IPv4	1
#define	AFI_IPv6	2

/* Subsequent Address Family Identifier as per RFC 4760 */
#define	SAFI_NONE	0
#define	SAFI_UNICAST	1
#define	SAFI_MULTICAST	2
#define	SAFI_MPLS	4
#define	SAFI_MPLSVPN	128

struct aid {
	u_int16_t	 afi;
	sa_family_t	 af;
	u_int8_t	 safi;
	char		*name;
};

extern const struct aid aid_vals[];

#define	AID_UNSPEC	0
#define	AID_INET	1
#define	AID_INET6	2
#define	AID_VPN_IPv4	3
#define	AID_MAX		4

#define AID_VALS	{					\
	/* afi, af, safii, name */				\
	{ AFI_UNSPEC, AF_UNSPEC, SAFI_NONE, "unspec"},		\
	{ AFI_IPv4, AF_INET, SAFI_UNICAST, "IPv4 unicast" },	\
	{ AFI_IPv6, AF_INET6, SAFI_UNICAST, "IPv6 unicast" },	\
	{ AFI_IPv4, AF_INET, SAFI_MPLSVPN, "IPv4 vpn" }		\
}

#define AID_PTSIZE	{				\
	0,						\
	sizeof(struct pt_entry4), 			\
	sizeof(struct pt_entry6),			\
	sizeof(struct pt_entry_vpn4)			\
}

struct vpn4_addr {
	u_int64_t	rd;
	struct in_addr	addr;
	u_int8_t	labelstack[21];	/* max that makes sense */
	u_int8_t	labellen;
	u_int8_t	pad1;
	u_int8_t	pad2;
};

#define BGP_MPLS_BOS	0x01

struct bgpd_addr {
	union {
		struct in_addr		v4;
		struct in6_addr		v6;
		struct vpn4_addr	vpn4;
		/* maximum size for a prefix is 256 bits */
		u_int8_t		addr8[32];
		u_int16_t		addr16[16];
		u_int32_t		addr32[8];
	} ba;		    /* 128-bit address */
	u_int32_t	scope_id;	/* iface scope id for v6 */
	u_int8_t	aid;
#define	v4	ba.v4
#define	v6	ba.v6
#define	vpn4	ba.vpn4
#define	addr8	ba.addr8
#define	addr16	ba.addr16
#define	addr32	ba.addr32
};

#define	DEFAULT_LISTENER	0x01
#define	LISTENER_LISTENING	0x02

struct listen_addr {
	TAILQ_ENTRY(listen_addr)	 entry;
	struct sockaddr_storage		 sa;
	int				 fd;
	enum reconf_action		 reconf;
	u_int8_t			 flags;
};

TAILQ_HEAD(listen_addrs, listen_addr);
TAILQ_HEAD(filter_set_head, filter_set);

struct bgpd_config {
	struct filter_set_head			 connectset;
	struct filter_set_head			 connectset6;
	struct filter_set_head			 staticset;
	struct filter_set_head			 staticset6;
	struct listen_addrs			*listen_addrs;
	char					*csock;
	char					*rcsock;
	int					 opts;
	int					 flags;
	int					 log;
	u_int					 rtableid;
	u_int32_t				 bgpid;
	u_int32_t				 clusterid;
	u_int32_t				 as;
	u_int16_t				 short_as;
	u_int16_t				 holdtime;
	u_int16_t				 min_holdtime;
	u_int16_t				 connectretry;
};

enum announce_type {
	ANNOUNCE_UNDEF,
	ANNOUNCE_SELF,
	ANNOUNCE_NONE,
	ANNOUNCE_DEFAULT_ROUTE,
	ANNOUNCE_ALL
};

enum enforce_as {
	ENFORCE_AS_UNDEF,
	ENFORCE_AS_OFF,
	ENFORCE_AS_ON
};

enum auth_method {
	AUTH_NONE,
	AUTH_MD5SIG,
	AUTH_IPSEC_MANUAL_ESP,
	AUTH_IPSEC_MANUAL_AH,
	AUTH_IPSEC_IKE_ESP,
	AUTH_IPSEC_IKE_AH
};

struct peer_auth {
	char			md5key[TCP_MD5_KEY_LEN];
	char			auth_key_in[IPSEC_AUTH_KEY_LEN];
	char			auth_key_out[IPSEC_AUTH_KEY_LEN];
	char			enc_key_in[IPSEC_ENC_KEY_LEN];
	char			enc_key_out[IPSEC_ENC_KEY_LEN];
	u_int32_t		spi_in;
	u_int32_t		spi_out;
	enum auth_method	method;
	u_int8_t		md5key_len;
	u_int8_t		auth_alg_in;
	u_int8_t		auth_alg_out;
	u_int8_t		auth_keylen_in;
	u_int8_t		auth_keylen_out;
	u_int8_t		enc_alg_in;
	u_int8_t		enc_alg_out;
	u_int8_t		enc_keylen_in;
	u_int8_t		enc_keylen_out;
};

struct capabilities {
	int8_t	mp[AID_MAX];	/* multiprotocol extensions, RFC 4760 */
	int8_t	refresh;	/* route refresh, RFC 2918 */
	int8_t	restart;	/* graceful restart, RFC 4724 */
	int8_t	as4byte;	/* draft-ietf-idr-as4bytes-13 */
};

struct peer_config {
	struct bgpd_addr	 remote_addr;
	struct bgpd_addr	 local_addr;
	struct peer_auth	 auth;
	struct capabilities	 capabilities;
	char			 group[PEER_DESCR_LEN];
	char			 descr[PEER_DESCR_LEN];
	char			 rib[PEER_DESCR_LEN];
	char			 if_depend[IFNAMSIZ];
	char			 demote_group[IFNAMSIZ];
	u_int32_t		 id;
	u_int32_t		 groupid;
	u_int32_t		 remote_as;
	u_int32_t		 local_as;
	u_int32_t		 max_prefix;
	enum announce_type	 announce_type;
	enum enforce_as		 enforce_as;
	enum reconf_action	 reconf_action;
	u_int16_t		 max_prefix_restart;
	u_int16_t		 holdtime;
	u_int16_t		 min_holdtime;
	u_int16_t		 local_short_as;
	u_int8_t		 template;
	u_int8_t		 remote_masklen;
	u_int8_t		 cloned;
	u_int8_t		 ebgp;		/* 1 = ebgp, 0 = ibgp */
	u_int8_t		 distance;	/* 1 = direct, >1 = multihop */
	u_int8_t		 passive;
	u_int8_t		 down;
	u_int8_t		 announce_capa;
	u_int8_t		 reflector_client;
	u_int8_t		 softreconfig_in;
	u_int8_t		 softreconfig_out;
	u_int8_t		 ttlsec;	/* TTL security hack */
	u_int8_t		 flags;
	u_int8_t		 pad[3];
};

#define PEERFLAG_TRANS_AS	0x01

struct network_config {
	struct bgpd_addr	prefix;
	struct filter_set_head	attrset;
	u_int8_t		prefixlen;
};

TAILQ_HEAD(network_head, network);

struct network {
	struct network_config	net;
	TAILQ_ENTRY(network)	entry;
};

enum imsg_type {
	IMSG_NONE,
	IMSG_CTL_END,
	IMSG_CTL_RELOAD,
	IMSG_CTL_FIB_COUPLE,
	IMSG_CTL_FIB_DECOUPLE,
	IMSG_CTL_NEIGHBOR_UP,
	IMSG_CTL_NEIGHBOR_DOWN,
	IMSG_CTL_NEIGHBOR_CLEAR,
	IMSG_CTL_NEIGHBOR_RREFRESH,
	IMSG_CTL_KROUTE,
	IMSG_CTL_KROUTE6,
	IMSG_CTL_KROUTE_ADDR,
	IMSG_CTL_RESULT,
	IMSG_CTL_SHOW_NEIGHBOR,
	IMSG_CTL_SHOW_NEXTHOP,
	IMSG_CTL_SHOW_INTERFACE,
	IMSG_CTL_SHOW_RIB,
	IMSG_CTL_SHOW_RIB_AS,
	IMSG_CTL_SHOW_RIB_PREFIX,
	IMSG_CTL_SHOW_RIB_ATTR,
	IMSG_CTL_SHOW_RIB_COMMUNITY,
	IMSG_CTL_SHOW_NETWORK,
	IMSG_CTL_SHOW_NETWORK6,
	IMSG_CTL_SHOW_RIB_MEM,
	IMSG_CTL_SHOW_TERSE,
	IMSG_CTL_SHOW_TIMER,
	IMSG_CTL_LOG_VERBOSE,
	IMSG_NETWORK_ADD,
	IMSG_NETWORK_REMOVE,
	IMSG_NETWORK_FLUSH,
	IMSG_NETWORK_DONE,
	IMSG_FILTER_SET,
	IMSG_RECONF_CONF,
	IMSG_RECONF_RIB,
	IMSG_RECONF_PEER,
	IMSG_RECONF_FILTER,
	IMSG_RECONF_LISTENER,
	IMSG_RECONF_DONE,
	IMSG_UPDATE,
	IMSG_UPDATE_ERR,
	IMSG_SESSION_ADD,
	IMSG_SESSION_UP,
	IMSG_SESSION_DOWN,
	IMSG_MRT_OPEN,
	IMSG_MRT_REOPEN,
	IMSG_MRT_CLOSE,
	IMSG_KROUTE_CHANGE,
	IMSG_KROUTE_DELETE,
	IMSG_KROUTE6_CHANGE,
	IMSG_KROUTE6_DELETE,
	IMSG_NEXTHOP_ADD,
	IMSG_NEXTHOP_REMOVE,
	IMSG_NEXTHOP_UPDATE,
	IMSG_PFTABLE_ADD,
	IMSG_PFTABLE_REMOVE,
	IMSG_PFTABLE_COMMIT,
	IMSG_REFRESH,
	IMSG_IFINFO,
	IMSG_DEMOTE
};

struct demote_msg {
	char		 demote_group[IFNAMSIZ];
	int		 level;
};

enum ctl_results {
	CTL_RES_OK,
	CTL_RES_NOSUCHPEER,
	CTL_RES_DENIED,
	CTL_RES_NOCAP,
	CTL_RES_PARSE_ERROR,
	CTL_RES_NOMEM
};

/* needed for session.h parse prototype */
LIST_HEAD(mrt_head, mrt);

/* error codes and subcodes needed in SE and RDE */
enum err_codes {
	ERR_HEADER = 1,
	ERR_OPEN,
	ERR_UPDATE,
	ERR_HOLDTIMEREXPIRED,
	ERR_FSM,
	ERR_CEASE
};

enum suberr_update {
	ERR_UPD_UNSPECIFIC,
	ERR_UPD_ATTRLIST,
	ERR_UPD_UNKNWN_WK_ATTR,
	ERR_UPD_MISSNG_WK_ATTR,
	ERR_UPD_ATTRFLAGS,
	ERR_UPD_ATTRLEN,
	ERR_UPD_ORIGIN,
	ERR_UPD_LOOP,
	ERR_UPD_NEXTHOP,
	ERR_UPD_OPTATTR,
	ERR_UPD_NETWORK,
	ERR_UPD_ASPATH
};

enum suberr_cease {
	ERR_CEASE_MAX_PREFIX = 1,
	ERR_CEASE_ADMIN_DOWN,
	ERR_CEASE_PEER_UNCONF,
	ERR_CEASE_ADMIN_RESET,
	ERR_CEASE_CONN_REJECT,
	ERR_CEASE_OTHER_CHANGE,
	ERR_CEASE_COLLISION,
	ERR_CEASE_RSRC_EXHAUST
};

struct kroute {
	struct in_addr	prefix;
	struct in_addr	nexthop;
	u_int16_t	flags;
	u_int16_t	labelid;
	u_short		ifindex;
	u_int8_t	prefixlen;
	u_int8_t	priority;
};

struct kroute6 {
	struct in6_addr	prefix;
	struct in6_addr	nexthop;
	u_int16_t	flags;
	u_int16_t	labelid;
	u_short		ifindex;
	u_int8_t	prefixlen;
	u_int8_t	priority;
};

struct kroute_nexthop {
	union {
		struct kroute		kr4;
		struct kroute6		kr6;
	} kr;
	struct bgpd_addr	nexthop;
	struct bgpd_addr	gateway;
	u_int8_t		valid;
	u_int8_t		connected;
};

struct kif {
	char			 ifname[IFNAMSIZ];
	u_int64_t		 baudrate;
	int			 flags;
	u_short			 ifindex;
	u_int8_t		 media_type;
	u_int8_t		 link_state;
	u_int8_t		 nh_reachable;	/* for nexthop verification */
};

struct session_up {
	struct bgpd_addr	local_addr;
	struct bgpd_addr	remote_addr;
	struct capabilities	capa;
	u_int32_t		remote_bgpid;
	u_int16_t		short_as;
};

struct pftable_msg {
	struct bgpd_addr	addr;
	char			pftable[PFTABLE_LEN];
	u_int8_t		len;
};

struct ctl_show_nexthop {
	struct bgpd_addr	addr;
	struct kif		kif;
	union {
		struct kroute		kr4;
		struct kroute6		kr6;
	} kr;
	u_int8_t		valid;
	u_int8_t		krvalid;;
};

struct ctl_neighbor {
	struct bgpd_addr	addr;
	char			descr[PEER_DESCR_LEN];
	int			show_timers;
};

struct kroute_label {
	struct kroute	kr;
	char		label[RTLABEL_LEN];
};

struct kroute6_label {
	struct kroute6	kr;
	char		label[RTLABEL_LEN];
};

#define	F_RIB_ELIGIBLE	0x01
#define	F_RIB_ACTIVE	0x02
#define	F_RIB_INTERNAL	0x04
#define	F_RIB_ANNOUNCE	0x08

struct ctl_show_rib {
	struct bgpd_addr	true_nexthop;
	struct bgpd_addr	exit_nexthop;
	struct bgpd_addr	prefix;
	struct bgpd_addr	remote_addr;
	char			descr[PEER_DESCR_LEN];
	time_t			lastchange;
	u_int32_t		remote_id;
	u_int32_t		local_pref;
	u_int32_t		med;
	u_int32_t		prefix_cnt;
	u_int32_t		active_cnt;
	u_int32_t		rib_cnt;
	u_int16_t		aspath_len;
	u_int16_t		flags;
	u_int8_t		prefixlen;
	u_int8_t		origin;
	/* plus a aspath_len bytes long aspath */
};

struct ctl_show_rib_prefix {
	struct bgpd_addr	prefix;
	time_t			lastchange;
	u_int16_t		flags;
	u_int8_t		prefixlen;
};

enum as_spec {
	AS_NONE,
	AS_ALL,
	AS_SOURCE,
	AS_TRANSIT,
	AS_PEER,
	AS_EMPTY
};

struct filter_as {
	enum as_spec	type;
	u_int32_t	as;
};

struct filter_community {
	int			as;
	int			type;
};

struct filter_extcommunity {
	u_int8_t	type;
	u_int8_t	subtype;	/* if extended type */
	union {
		struct ext_as {
			u_int16_t	as;
			u_int32_t	val;
		}		ext_as;
		struct ext_as4 {
			u_int32_t	as4;
			u_int16_t	val;
		}		ext_as4;
		struct ext_ip {
			struct in_addr	addr;
			u_int16_t	val;
		}		ext_ip;
		u_int64_t	ext_opaq;	/* only 48 bits */
	}		data;
};


struct ctl_show_rib_request {
	char			rib[PEER_DESCR_LEN];
	struct ctl_neighbor	neighbor;
	struct bgpd_addr	prefix;
	struct filter_as	as;
	struct filter_community community;
	u_int32_t		peerid;
	pid_t			pid;
	u_int16_t		flags;
	enum imsg_type		type;
	u_int8_t		prefixlen;
	u_int8_t		aid;
};

enum filter_actions {
	ACTION_NONE,
	ACTION_ALLOW,
	ACTION_DENY
};

enum directions {
	DIR_IN = 1,
	DIR_OUT
};

enum from_spec {
	FROM_ALL,
	FROM_ADDRESS,
	FROM_DESCR,
	FROM_GROUP
};

enum comp_ops {
	OP_NONE,
	OP_RANGE,
	OP_XRANGE,
	OP_EQ,
	OP_NE,
	OP_LE,
	OP_LT,
	OP_GE,
	OP_GT
};

struct filter_peers {
	u_int32_t	peerid;
	u_int32_t	groupid;
	u_int16_t	ribid;
};

/* special community type */
#define	COMMUNITY_ERROR			-1
#define	COMMUNITY_ANY			-2
#define	COMMUNITY_NEIGHBOR_AS		-3
#define	COMMUNITY_UNSET			-4
#define	COMMUNITY_WELLKNOWN		0xffff
#define	COMMUNITY_NO_EXPORT		0xff01
#define	COMMUNITY_NO_ADVERTISE		0xff02
#define	COMMUNITY_NO_EXPSUBCONFED	0xff03
#define	COMMUNITY_NO_PEER		0xff04	/* RFC 3765 */

/* extended community definitions */
#define EXT_COMMUNITY_IANA		0x80
#define EXT_COMMUNITY_TRANSITIVE	0x40
#define EXT_COMMUNITY_VALUE		0x3f
/* extended types */
#define EXT_COMMUNITY_TWO_AS		0	/* 2 octet AS specific */
#define EXT_COMMUNITY_IPV4		1	/* IPv4 specific */
#define EXT_COMMUNITY_FOUR_AS		2	/* 4 octet AS specific */
#define EXT_COMMUNITY_OPAQUE		3	/* opaque ext community */
/* sub types */
#define EXT_COMMUNITY_ROUTE_TGT		2	/* RFC 4360 & RFC4364 */
#define EXT_CUMMUNITY_ROUTE_ORIG	3	/* RFC 4360 & RFC4364 */
#define EXT_COMMUNITY_OSPF_DOM_ID	5	/* RFC 4577 */
#define EXT_COMMUNITY_OSPF_RTR_TYPE	6	/* RFC 4577 */
#define EXT_COMMUNITY_OSPF_RTR_ID	7	/* RFC 4577 */
#define EXT_COMMUNITY_BGP_COLLECT	8	/* RFC 4384 */
/* other handy defines */
#define EXT_COMMUNITY_OPAQUE_MAX	0xffffffffffffULL

struct ext_comm_pairs {
	u_int8_t	type;
	u_int8_t	subtype;
	u_int8_t	transitive;	/* transitive bit needs to be set */
};

#define IANA_EXT_COMMUNITIES	{					\
	{ EXT_COMMUNITY_TWO_AS, EXT_COMMUNITY_ROUTE_TGT, 0 },		\
	{ EXT_COMMUNITY_TWO_AS, EXT_CUMMUNITY_ROUTE_ORIG, 0 },		\
	{ EXT_COMMUNITY_TWO_AS, EXT_COMMUNITY_OSPF_DOM_ID, 0 },		\
	{ EXT_COMMUNITY_TWO_AS, EXT_COMMUNITY_BGP_COLLECT, 0 },		\
	{ EXT_COMMUNITY_FOUR_AS, EXT_COMMUNITY_ROUTE_TGT, 0 },		\
	{ EXT_COMMUNITY_FOUR_AS, EXT_CUMMUNITY_ROUTE_ORIG, 0 },		\
	{ EXT_COMMUNITY_IPV4, EXT_COMMUNITY_ROUTE_TGT, 0 },		\
	{ EXT_COMMUNITY_IPV4, EXT_CUMMUNITY_ROUTE_ORIG, 0 },		\
	{ EXT_COMMUNITY_IPV4, EXT_COMMUNITY_OSPF_RTR_ID, 0 },		\
	{ EXT_COMMUNITY_OPAQUE, EXT_COMMUNITY_OSPF_RTR_TYPE, 0 }	\
}


struct filter_prefix {
	struct bgpd_addr	addr;
	u_int8_t		len;
};

struct filter_prefixlen {
	enum comp_ops		op;
	u_int8_t		aid;
	u_int8_t		len_min;
	u_int8_t		len_max;
};

struct filter_match {
	struct filter_prefix	prefix;
	struct filter_prefixlen	prefixlen;
	struct filter_as	as;
	struct filter_community	community;
};

TAILQ_HEAD(filter_head, filter_rule);

struct filter_rule {
	TAILQ_ENTRY(filter_rule)	entry;
	char				rib[PEER_DESCR_LEN];
	struct filter_peers		peer;
	struct filter_match		match;
	struct filter_set_head		set;
	enum filter_actions		action;
	enum directions			dir;
	u_int8_t			quick;
};

enum action_types {
	ACTION_SET_LOCALPREF,
	ACTION_SET_RELATIVE_LOCALPREF,
	ACTION_SET_MED,
	ACTION_SET_RELATIVE_MED,
	ACTION_SET_WEIGHT,
	ACTION_SET_RELATIVE_WEIGHT,
	ACTION_SET_PREPEND_SELF,
	ACTION_SET_PREPEND_PEER,
	ACTION_SET_NEXTHOP,
	ACTION_SET_NEXTHOP_REJECT,
	ACTION_SET_NEXTHOP_BLACKHOLE,
	ACTION_SET_NEXTHOP_NOMODIFY,
	ACTION_SET_NEXTHOP_SELF,
	ACTION_SET_COMMUNITY,
	ACTION_DEL_COMMUNITY,
	ACTION_SET_EXT_COMMUNITY,
	ACTION_DEL_EXT_COMMUNITY,
	ACTION_PFTABLE,
	ACTION_PFTABLE_ID,
	ACTION_RTLABEL,
	ACTION_RTLABEL_ID,
	ACTION_SET_ORIGIN
};

struct filter_set {
	TAILQ_ENTRY(filter_set)		entry;
	union {
		u_int8_t		prepend;
		u_int16_t		id;
		u_int32_t		metric;
		int32_t			relative;
		struct bgpd_addr	nexthop;
		struct filter_community	community;
		struct filter_extcommunity	ext_community;
		char			pftable[PFTABLE_LEN];
		char			rtlabel[RTLABEL_LEN];
		u_int8_t		origin;
	} action;
	enum action_types		type;
};

struct rde_rib {
	SIMPLEQ_ENTRY(rde_rib)	entry;
	char			name[PEER_DESCR_LEN];
	u_int16_t		id;
	u_int16_t		flags;
};
SIMPLEQ_HEAD(rib_names, rde_rib);
extern struct rib_names ribnames;

/* 4-byte magic AS number */
#define AS_TRANS	23456

struct rde_memstats {
	int64_t		path_cnt;
	int64_t		prefix_cnt;
	int64_t		rib_cnt;
	int64_t		pt_cnt[AID_MAX];
	int64_t		nexthop_cnt;
	int64_t		aspath_cnt;
	int64_t		aspath_size;
	int64_t		aspath_refs;
	int64_t		attr_cnt;
	int64_t		attr_refs;
	int64_t		attr_data;
	int64_t		attr_dcnt;
};

/* prototypes */
/* bgpd.c */
void		 send_nexthop_update(struct kroute_nexthop *);
void		 send_imsg_session(int, pid_t, void *, u_int16_t);
int		 bgpd_redistribute(int, struct kroute *, struct kroute6 *);
int		 bgpd_filternexthop(struct kroute *, struct kroute6 *);

/* log.c */
void		 log_init(int);
void		 log_verbose(int);
void		 vlog(int, const char *, va_list);
void		 log_peer_warn(const struct peer_config *, const char *, ...);
void		 log_peer_warnx(const struct peer_config *, const char *, ...);
void		 log_warn(const char *, ...);
void		 log_warnx(const char *, ...);
void		 log_info(const char *, ...);
void		 log_debug(const char *, ...);
void		 fatal(const char *) __dead;
void		 fatalx(const char *) __dead;

/* parse.y */
int	 cmdline_symset(char *);

/* config.c */
int	 host(const char *, struct bgpd_addr *, u_int8_t *);

/* kroute.c */
int		 kr_init(int, u_int);
int		 kr_change(struct kroute_label *);
int		 kr_delete(struct kroute_label *);
int		 kr6_change(struct kroute6_label *);
int		 kr6_delete(struct kroute6_label *);
void		 kr_shutdown(void);
void		 kr_fib_couple(void);
void		 kr_fib_decouple(void);
int		 kr_dispatch_msg(void);
int		 kr_nexthop_add(struct bgpd_addr *);
void		 kr_nexthop_delete(struct bgpd_addr *);
void		 kr_show_route(struct imsg *);
void		 kr_ifinfo(char *);
int		 kr_reload(void);

/* control.c */
void	control_cleanup(const char *);
int	control_imsg_relay(struct imsg *);

/* pftable.c */
int	pftable_exists(const char *);
int	pftable_add(const char *);
int	pftable_clear_all(void);
int	pftable_addr_add(struct pftable_msg *);
int	pftable_addr_remove(struct pftable_msg *);
int	pftable_commit(void);

/* name2id.c */
u_int16_t	 rib_name2id(const char *);
const char	*rib_id2name(u_int16_t);
void		 rib_unref(u_int16_t);
void		 rib_ref(u_int16_t);
u_int16_t	 rtlabel_name2id(const char *);
const char	*rtlabel_id2name(u_int16_t);
void		 rtlabel_unref(u_int16_t);
void		 rtlabel_ref(u_int16_t);
u_int16_t	 pftable_name2id(const char *);
const char	*pftable_id2name(u_int16_t);
void		 pftable_unref(u_int16_t);
void		 pftable_ref(u_int16_t);


/* rde_filter.c */
void		 filterset_free(struct filter_set_head *);
int		 filterset_cmp(struct filter_set *, struct filter_set *);
const char	*filterset_name(enum action_types);

/* util.c */
const char	*log_addr(const struct bgpd_addr *);
const char	*log_in6addr(const struct in6_addr *);
const char	*log_sockaddr(struct sockaddr *);
const char	*log_as(u_int32_t);
const char	*log_ext_subtype(u_int8_t);
int		 aspath_snprint(char *, size_t, void *, u_int16_t);
int		 aspath_asprint(char **, void *, u_int16_t);
size_t		 aspath_strlen(void *, u_int16_t);
void		 inet6applymask(struct in6_addr *, const struct in6_addr *,
		    int);
const char	*aid2str(u_int8_t);
int		 aid2afi(u_int8_t, u_int16_t *, u_int8_t *);
int		 afi2aid(u_int16_t, u_int8_t, u_int8_t *);
sa_family_t	 aid2af(u_int8_t);
int		 af2aid(sa_family_t, u_int8_t, u_int8_t *);
struct sockaddr	*addr2sa(struct bgpd_addr *, u_int16_t);
void		 sa2addr(struct sockaddr *, struct bgpd_addr *);

/* here */

static inline in_addr_t
prefixlen2mask(u_int8_t prefixlen)
{
	if (prefixlen == 0)
		return (0);

	return (0xffffffff << (32 - prefixlen));
}

static inline u_int8_t
mask2prefixlen(in_addr_t ina)
{
	if (ina == 0)
		return (0);
	else
		return (33 - ffs(ntohl(ina)));
}

static inline struct in6_addr *
prefixlen2mask6(u_int8_t prefixlen)
{
	static struct in6_addr	mask;
	int			i;

	bzero(&mask, sizeof(mask));

	for (i = 0; i < prefixlen / 8; i++)
		mask.s6_addr[i] = 0xff;

	i = prefixlen % 8;
	if (i)
		mask.s6_addr[prefixlen / 8] = 0xff00 >> i;

	return (&mask);
}

#endif /* __BGPD_H__ */
