/* UEFI Network interface.
 *
 * This implements the simplest possible polled network interface
 * ontop of the EFI NIC protocol.
 */
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/inetdevice.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/route.h>
#include <net/route.h>
#include "efiwrapper.h"
#include "efinet.h"
#include "efidhcp4.h"

typedef struct {
	spinlock_t lock;
	EFI_SIMPLE_NETWORK_PROTOCOL * uefi_nic;
	EFI_HANDLE uefi_handle;
	int id;
	int up;
	struct net_device * dev;
	struct net_device_stats stats;
	struct sk_buff * rx_skb;
} uefi_nic_t;

#define MAX_NICS 16
static struct timer_list uefi_rx_timer;
static uefi_nic_t * uefi_nics[MAX_NICS];
static int uefi_nic_count;

static int uefi_net_rx(uefi_nic_t * nic)
{
	struct sk_buff * skb;
	UINTN pkt_len = 1500;
	unsigned long flags;
	int status;

	// if we are no longer up, stop scheduling
	if (!nic->up)
		return 0;

	if (nic->rx_skb == NULL)
	{
		// replenish our rx packet and align it
		nic->rx_skb = netdev_alloc_skb(nic->dev, pkt_len + 5);
		skb_reserve(nic->rx_skb, NET_IP_ALIGN);
	}

	spin_lock_irqsave(&nic->lock, flags);
	skb = nic->rx_skb;

	status = nic->uefi_nic->Receive(
		nic->uefi_nic,
		NULL, // header size, no processing required
		&pkt_len,
		skb_tail_pointer(skb),
		NULL, // src addr
		NULL, // dst addr,
		NULL // proto
	);

	// if we have successful in receiving the packet
	// remove this skb from the rx pool
	if (status == 0)
		nic->rx_skb = NULL;

	spin_unlock_irqrestore(&nic->lock, flags);

	if (status == 6) // EFI_NOT_READY)
	{
		// no packet to receive
	} else
	if (status == 0)
	{
		// success! update the actual length received
		// which has already been copied into the buffer
		skb_put(skb, pkt_len);
		skb->protocol = eth_type_trans(skb, nic->dev);
		//printk("uefi%d: rx %lld bytes proto %d\n", nic->id, pkt_len, skb->protocol);
		netif_rx(skb);
	} else {
		printk("uefi%d: error %d\n", nic->id, status);
	}

	return 1;
}

static void uefi_net_poll(struct timer_list * timer)
{
	uefi_memory_map_add();

	// try to clear the queues on the NICs
	for(int loops = 0 ; loops < 10 ; loops++)
	{
		int count = 0;
		for(int i = 0 ; i < uefi_nic_count ; i++)
			count += uefi_net_rx(uefi_nics[i]);
		if (count == 0)
			break;
	}

	// reschedule our selves to check again
	mod_timer(timer, jiffies + msecs_to_jiffies(5));
}


static int uefi_net_open(struct net_device * dev)
{
	uefi_nic_t * nic = netdev_priv(dev);
	int status;

	uefi_memory_map_add();

	status = nic->uefi_nic->Start(nic->uefi_nic);

	// 0 == success, 20 == already started
	if (status != 0 && status != 20)
	{
		printk("uefi%d: start returned %d\n", nic->id, status);
		return -1;
	}

	printk("uefi%d: started nic\n", nic->id);
	nic->up = 1;

	return 0;
}

static int uefi_net_stop(struct net_device * dev)
{
	uefi_nic_t * nic = netdev_priv(dev);
	int status;

	uefi_memory_map_add();

	nic->up = 0; // we'll stop scheduling timers

	status = nic->uefi_nic->Shutdown(nic->uefi_nic);
	if (status != 0)
	{
		printk("uefi%d: stop returned %d\n", nic->id, status);
		return -1;
	}

	printk("uefi%d: stopped nic\n", nic->id);
	return 0;
}

static netdev_tx_t uefi_net_xmit(struct sk_buff * skb, struct net_device * dev)
{
	uefi_nic_t * nic = netdev_priv(dev);
	unsigned long flags;
	int status;

	uefi_memory_map_add();

	spin_lock_irqsave(&nic->lock, flags);

	status = nic->uefi_nic->Transmit(
		nic->uefi_nic,
		0,		// HeaderSize 0 == packet is fully formed
		skb->len,	// BufferSize
		skb->data,	// Buffer,
		NULL,		// SrcAddr, unused since HeaderSize == 0
		NULL,		// DstAddr, unused
		NULL		// Protocol, unused
	);

	spin_unlock_irqrestore(&nic->lock, flags);

	dev_consume_skb_any(skb);

	if (status != 0)
	{
		// should return NETDEV_TX_BUSY, but :shrug:?
		if (status == 9)
			return NETDEV_TX_OK;

		printk("uefi%d: tx failed %d\n", nic->id, status);
		return -1;
	}

	return NETDEV_TX_OK;
}

static struct net_device_stats * uefi_net_stats(struct net_device * dev)
{
	uefi_nic_t * nic = netdev_priv(dev);
	struct net_device_stats * stats = &nic->stats;
	EFI_NETWORK_STATISTICS uefi_stats;
	UINTN uefi_stats_size = sizeof(uefi_stats);
	unsigned long flags;

	uefi_memory_map_add();

	spin_lock_irqsave(&nic->lock, flags);

	nic->uefi_nic->Statistics(
		nic->uefi_nic,
		0, // do not reset
		&uefi_stats_size,
		&uefi_stats
	);

	spin_unlock_irqrestore(&nic->lock, flags);

	// translate the stats to Linux from UEFI
	stats->rx_bytes		= uefi_stats.RxTotalBytes;
	stats->rx_packets	= uefi_stats.RxTotalFrames;
	stats->rx_errors	= uefi_stats.RxTotalFrames - uefi_stats.RxGoodFrames;
	stats->rx_dropped	= uefi_stats.RxDroppedFrames;
	stats->rx_length_errors	= uefi_stats.RxUndersizeFrames;
	stats->rx_over_errors	= uefi_stats.RxOversizeFrames;
	stats->rx_crc_errors	= uefi_stats.RxCrcErrorFrames;

	stats->tx_bytes		= uefi_stats.TxTotalBytes;
	stats->tx_packets	= uefi_stats.TxTotalFrames;
	stats->tx_errors	= uefi_stats.TxTotalFrames - uefi_stats.RxGoodFrames;
	stats->tx_dropped	= uefi_stats.TxDroppedFrames;

	stats->multicast	= uefi_stats.RxMulticastFrames;
	stats->collisions	= uefi_stats.Collisions;

	// there are other stats that could be copied?
	return stats;
}


static struct net_device_ops uefi_nic_ops = {
	.ndo_open	= uefi_net_open,
	.ndo_stop	= uefi_net_stop,
	.ndo_start_xmit	= uefi_net_xmit,
	.ndo_get_stats	= uefi_net_stats,
	//.ndo_poll_controller = uefi_net_poll,
};

static void
set_sockaddr(struct sockaddr_in *sin, __be32 addr, __be16 port)
{
	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = addr;
	sin->sin_port = port;
}

static int
uefi_nic_set_address(
	uefi_nic_t * uefi_nic,
	EFI_IPv4_ADDRESS * efi_myaddr,
	EFI_IPv4_ADDRESS * efi_netmask,
	EFI_IPv4_ADDRESS * efi_gateway
)
{
	struct net_device * dev = uefi_nic->dev;

	// copied from net/ipv4/ipconfig.c ic_setup_if() and ic_setup_routes()
	struct rtentry rm;
	struct ifreq ir;
	struct sockaddr_in * sin = (void *) &ir.ifr_ifru.ifru_addr;
	int err;

	__be32 myaddr = *(__be32*) efi_myaddr->Addr;
	__be32 netmask = *(__be32*) efi_netmask->Addr;
	__be32 gateway = *(__be32*) efi_gateway->Addr;

	memset(&ir, 0, sizeof(ir));
	strcpy(ir.ifr_ifrn.ifrn_name, dev->name);

	set_sockaddr(sin, myaddr, 0);
	if ((err = devinet_ioctl(&init_net, SIOCSIFADDR, &ir)) < 0)
	{
		pr_err("uefi-ip-config: unable to set interface address (%d)\n", err);
		return -1;
	}

	set_sockaddr(sin, netmask, 0);
	if ((err = devinet_ioctl(&init_net, SIOCSIFNETMASK, &ir)) < 0)
	{
		pr_err("uefi-ip-config: unable to set interface netmask (%d)\n", err);
		return -1;
	}

	set_sockaddr(sin, myaddr | ~netmask, 0);
	if ((err = devinet_ioctl(&init_net, SIOCSIFBRDADDR, &ir)) < 0)
	{
		pr_err("uefi-ip-config: unable to set interface broadcast (%d)\n", err);
		return -1;
	}

	// bring up the network before adding the route
	// this will call our init routine, which will schedule our timers
	if ((err = dev_change_flags(dev, dev->flags | IFF_UP, NULL)) < 0)
	{
		pr_err("uefi-ip-config: can not bring up network (%d)\n", err);
		return -1;
	}

	// configure the default route
	memset(&rm, 0, sizeof(rm));
	set_sockaddr((struct sockaddr_in *) &rm.rt_dst, 0, 0);
	set_sockaddr((struct sockaddr_in *) &rm.rt_genmask, 0, 0);
	set_sockaddr((struct sockaddr_in *) &rm.rt_gateway, gateway, 0);
	rm.rt_flags = RTF_UP | RTF_GATEWAY;
	if ((err = ip_rt_ioctl(&init_net, SIOCADDRT, &rm)) < 0) {
		pr_err("uefi-ip-config: cannot add default route (%d)\n", err);
		return -1;
	}

	return 0;
}

static void uefi_nic_autoconfig(uefi_nic_t * nic)
{
	EFI_DHCP4_PROTOCOL * dhcp4 = uefi_locate_and_handle_protocol(&EFI_DHCP4_PROTOCOL_GUID);
	EFI_DHCP4_MODE_DATA config;
	int status;

	if (!dhcp4)
		return;

	status = dhcp4->GetModeData(dhcp4, &config);
	if (status != 0)
		return;

	if (config.State != Dhcp4Bound)
	{
		printk("UEFI DHCP not bound to an address, skipping\n");
		return;
	}

	printk("uefi-dhcp ip=%d.%d.%d.%d router=%d.%d.%d.%d subnet=%d.%d.%d.%d state=%d\n",
		config.ClientAddress.Addr[0],
		config.ClientAddress.Addr[1],
		config.ClientAddress.Addr[2],
		config.ClientAddress.Addr[3],
		config.RouterAddress.Addr[0],
		config.RouterAddress.Addr[1],
		config.RouterAddress.Addr[2],
		config.RouterAddress.Addr[3],
		config.SubnetMask.Addr[0],
		config.SubnetMask.Addr[1],
		config.SubnetMask.Addr[2],
		config.SubnetMask.Addr[3],
		nic->uefi_nic->Mode->State
	);

	uefi_nic_set_address(
		nic,
		&config.ClientAddress,
		&config.SubnetMask,
		&config.RouterAddress
	);
}

static int uefi_nic_create(int id, EFI_HANDLE handle, EFI_SIMPLE_NETWORK_PROTOCOL * uefi_nic)
{
	struct net_device * dev;
	uefi_nic_t * nic;
	dev = alloc_etherdev(sizeof(uefi_nic_t));

	if (!dev)
	{
		printk("unable to allocate net dev\n");
		return -1;
	}

	nic = netdev_priv(dev);
	if (!nic)
	{
		printk("private data is null?\n");
		return -1;
	}

	memset(nic, 0, sizeof(*nic));

	spin_lock_init(&nic->lock);
	nic->dev = dev;
	nic->uefi_nic = uefi_nic;
	nic->uefi_handle = handle;
	nic->id = id;
	nic->up = 0;
	nic->rx_skb = NULL;

	memcpy(dev->dev_addr, uefi_nic->Mode->CurrentAddress.Addr, ETH_ALEN);
	dev->netdev_ops = &uefi_nic_ops;

	register_netdevice(dev);

	printk("%d: type=%d media=%d addr=%02x:%02x:%02x:%02x:%02x:%02x\n",
		nic->id,
		uefi_nic->Mode->IfType,
		uefi_nic->Mode->MediaPresent,
		dev->dev_addr[0],
		dev->dev_addr[1],
		dev->dev_addr[2],
		dev->dev_addr[3],
		dev->dev_addr[4],
		dev->dev_addr[5]
	);

	// store it in our array of active NIC
	uefi_nics[uefi_nic_count++] = nic;

	uefi_nic_autoconfig(nic);

	return 0;
}


/*
static struct rtnl_link_ops uefi_nic_link_ops = {
	.kind		= "uefi",
	.setup		= uefi_nic_setup,
	.validate	= uefi_nic_validate,
};
*/

int uefi_nic_init(void)
{
	EFI_HANDLE handles[64];
	int handle_count = uefi_locate_handles(&EFI_SIMPLE_NETWORK_PROTOCOL_GUID, handles, 64);

	printk("found %d NIC handles\n", handle_count);

	for(int i = 0 ; i < handle_count ; i++)
	{
		EFI_HANDLE handle = handles[i];

		EFI_SIMPLE_NETWORK_PROTOCOL * nic = uefi_handle_protocol(&EFI_SIMPLE_NETWORK_PROTOCOL_GUID, handle);
		if (!nic)
			continue;

		uefi_nic_create(i, handle, nic);

		// only create the first one; they are duplicates?
		break;
	}

	timer_setup(&uefi_rx_timer, uefi_net_poll, 0);
	mod_timer(&uefi_rx_timer, jiffies + msecs_to_jiffies(10));

	return 0;
}

int uefi_nic_exit(void)
{
	for(int i = 0 ; i < uefi_nic_count ; i++)
	{
		uefi_nic_t * nic = uefi_nics[i];
		printk("uefi%d: shutdown nic\n", i);
		nic->uefi_nic->Shutdown(nic->uefi_nic);
	}

	return 0;
}
