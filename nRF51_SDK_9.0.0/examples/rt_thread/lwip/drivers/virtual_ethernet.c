#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <rthw.h>
#include <rtdevice.h>
#include "lwipopts.h"
#include <netif/ethernetif.h>

#include "virtual_ethernet.h"

#define VIRTUAL_ETHERNET_RX_BUFSZ 512
rt_uint8_t rx_tx_buffer[VIRTUAL_ETHERNET_RX_BUFSZ];

struct virtual_ethernet
{
	/* inherit from ethernet device */
    struct eth_device parent;
    struct rt_ringbuffer rx_rb;
} virtual_ethernet_device;

static rt_err_t rt_virtual_ethernet_init (rt_device_t dev)
{
    /* UART Initialization and Enable */
    /** @snippet [Configure UART RX and TX pin] */

    return RT_EOK;
}

static rt_err_t rt_virtual_ethernet_open(rt_device_t dev, rt_uint16_t oflag)
{
    return RT_EOK;
}

static rt_err_t rt_virtual_ethernet_close(rt_device_t dev)
{
    return RT_EOK;
}

static rt_size_t rt_virtual_ethernet_read(rt_device_t dev, rt_off_t pos, void* buffer, rt_size_t size)
{
    rt_size_t length;
    struct virtual_ethernet *ve = (struct virtual_ethernet*)dev;
    /* interrupt receive */
    rt_base_t level;

    RT_ASSERT(ve != RT_NULL);

    /* disable interrupt */
    level = rt_hw_interrupt_disable();
    length = rt_ringbuffer_get(&(ve->rx_rb), buffer, size);
    /* enable interrupt */
    rt_hw_interrupt_enable(level);

    return length;
}

static rt_size_t rt_virtual_ethernet_write(rt_device_t dev, rt_off_t pos, const void* buffer, rt_size_t size)
{
    char *ptr;
    ptr = (char*) buffer;

    if (dev->open_flag & RT_DEVICE_FLAG_STREAM)
    {
        /* stream mode */
        while (size)
        {
            if (*ptr == '\n')
            {
//                NRF_UART0->TXD = (uint8_t)'\r';

//                // Wait for TXD data to be sent.
//                while (NRF_UART0->EVENTS_TXDRDY != 1) ;

//                NRF_UART0->EVENTS_TXDRDY = 0;
            }

//            NRF_UART0->TXD = (uint8_t)(*ptr);

//            // Wait for TXD data to be sent.
//            while (NRF_UART0->EVENTS_TXDRDY != 1) ;

//            NRF_UART0->EVENTS_TXDRDY = 0;

            ptr ++;
            size --;
        }
    }
    else
    {
        while ( size != 0 )
        {
//            NRF_UART0->TXD = (uint8_t)(*ptr);

//            // Wait for TXD data to be sent.
//            while (NRF_UART0->EVENTS_TXDRDY != 1) ;

//            NRF_UART0->EVENTS_TXDRDY = 0;

            ptr++;
            size--;
        }
    }

    return (rt_size_t) ptr - (rt_size_t) buffer;
}

/* reception packet. */
struct pbuf *virtual_ethernet_rx(rt_device_t dev)
{
	struct pbuf* p;
//	rt_uint32_t size;
//	rt_uint32_t Index;

//	/* init p pointer */
//	p = RT_NULL;

//	/* lock EMAC device */
//	rt_sem_take(&sem_lock, RT_WAITING_FOREVER);

//	Index = LPC_EMAC->RxConsumeIndex;
//	if(Index != LPC_EMAC->RxProduceIndex)
//	{
//		size = (RX_STAT_INFO(Index) & 0x7ff)+1;
//		if (size > ETH_FRAG_SIZE) size = ETH_FRAG_SIZE;

//		/* allocate buffer */
//		p = pbuf_alloc(PBUF_LINK, size, PBUF_RAM);
//		if (p != RT_NULL)
//		{
//			struct pbuf* q;
//			rt_uint8_t *ptr;

//			ptr = (rt_uint8_t*)RX_BUF(Index);
//			for (q = p; q != RT_NULL; q= q->next)
//			{
//				memcpy(q->payload, ptr, q->len);
//				ptr += q->len;
//			}
//		}
//		
//		/* move Index to the next */
//		if(++Index > LPC_EMAC->RxDescriptorNumber)
//			Index = 0;

//		/* set consume index */
//		LPC_EMAC->RxConsumeIndex = Index;
//	}
//	else
//	{
//		/* Enable RxDone interrupt */
//		LPC_EMAC->IntEnable = INT_RX_DONE | INT_TX_DONE;
//	}

//	/* unlock EMAC device */
//	rt_sem_release(&sem_lock);

	return p;
}

/* EtherNet Device Interface */
/* transmit packet. */
rt_err_t virtual_ethernet_tx( rt_device_t dev, struct pbuf* p)
{
//	rt_uint32_t Index, IndexNext;
//	struct pbuf *q;
//	rt_uint8_t *ptr;

//	/* calculate next index */
//	IndexNext = LPC_EMAC->TxProduceIndex + 1;
//	if(IndexNext > LPC_EMAC->TxDescriptorNumber) IndexNext = 0;

//	/* check whether block is full */
//	while (IndexNext == LPC_EMAC->TxConsumeIndex)
//	{
//		rt_err_t result;
//		rt_uint32_t recved;
//		
//		/* there is no block yet, wait a flag */
//		result = rt_event_recv(&tx_event, 0x01, 
//			RT_EVENT_FLAG_AND | RT_EVENT_FLAG_CLEAR, RT_WAITING_FOREVER, &recved);

//		RT_ASSERT(result == RT_EOK);
//	}

//	/* lock EMAC device */
//	rt_sem_take(&sem_lock, RT_WAITING_FOREVER);

//	/* get produce index */
//	Index = LPC_EMAC->TxProduceIndex;

//	/* calculate next index */
//	IndexNext = LPC_EMAC->TxProduceIndex + 1;
//	if(IndexNext > LPC_EMAC->TxDescriptorNumber)
//		IndexNext = 0;

//	/* copy data to tx buffer */
//	q = p;
//	ptr = (rt_uint8_t*)TX_BUF(Index);
//	while (q)
//	{
//		memcpy(ptr, q->payload, q->len);
//		ptr += q->len;
//		q = q->next;
//	}

//	TX_DESC_CTRL(Index) &= ~0x7ff;
//	TX_DESC_CTRL(Index) |= (p->tot_len - 1) & 0x7ff;

//	/* change index to the next */
//	LPC_EMAC->TxProduceIndex = IndexNext;

//	/* unlock EMAC device */
//	rt_sem_release(&sem_lock);

	return RT_EOK;
}

void rt_hw_virtual_ethernet_init(void)
{
    struct virtual_ethernet* ethernet;

    /* get uart device */
    ethernet = &virtual_ethernet_device;

    /* device initialization */
//    ethernet->parent.type = RT_Device_Class_NetIf;
    rt_ringbuffer_init(&(ethernet->rx_rb), rx_tx_buffer, sizeof(rx_tx_buffer));

    /* device interface */
    ethernet->parent.parent.init 	    = rt_virtual_ethernet_init;
    ethernet->parent.parent.open 	    = rt_virtual_ethernet_open;
    ethernet->parent.parent.close      = rt_virtual_ethernet_close;
    ethernet->parent.parent.read 	    = rt_virtual_ethernet_read;
    ethernet->parent.parent.write      = rt_virtual_ethernet_write;
    ethernet->parent.parent.control    = RT_NULL;
    ethernet->parent.parent.user_data  = RT_NULL;
	
	ethernet->parent.eth_rx = virtual_ethernet_rx;
	ethernet->parent.eth_tx = virtual_ethernet_tx;

	eth_device_init(&(ethernet->parent), "veth0");
}

INIT_DEVICE_EXPORT(rt_hw_virtual_ethernet_init);
