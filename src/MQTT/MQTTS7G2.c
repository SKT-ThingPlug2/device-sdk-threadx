/*******************************************************************************
* Copyright (c) 2017 SKT Corp.
*******************************************************************************/

#include "MQTTS7G2.h"
#include "nx_api.h"
#include "common_data.h"
#include "nx_dns.h"

static unsigned long MilliTimer = 0;
uint8_t init_flag = 0;
extern NX_DNS g_dns0;
static unsigned int SecTimer = 0;
uint8_t milli_cnt = 0;

void set_tmp_time(unsigned int t)
{
    SecTimer = t;
}
unsigned int ntp_time(void)
{
	return SecTimer;
}

void SysTickIntHandler(timer_callback_args_t * p_args) {
  MilliTimer += 10;
  milli_cnt++;
  if(milli_cnt == 100) {
	  SecTimer++;
	  milli_cnt = 0;
  }
}

char expired(Timer* timer) {
  long left = timer->end_time - MilliTimer;
  return (left < 0);
}

void countdown_ms(Timer* timer, unsigned int timeout) {
  timer->end_time = MilliTimer + timeout;
}

void countdown(Timer* timer, unsigned int timeout) {
  timer->end_time = MilliTimer + (timeout * 1000);
}

int left_ms(Timer* timer) {
  long left = timer->end_time - MilliTimer;
  return (left < 0) ? 0 : left;
}

void InitTimer(Timer* timer) {
  timer->end_time = 0;
}

NX_PACKET *read_packet_ptr = NULL;
ULONG prepend_offset = 0;

// Network Headers & Variables & Functions
int s7g2_read(Network* n, unsigned char* buffer, int len, int timeout_ms)
{
  ULONG copied_idx = 0;
  Timer t;
  countdown_ms(&t , timeout_ms);
 
  while(len != 0){
    ULONG left_len;
    ULONG recv_len = 0;
    ULONG bytes_copied = 0;
    
    if(prepend_offset == 0){
      nx_tcp_socket_receive(&n->my_socket, &read_packet_ptr, timeout_ms);
    }
    nx_packet_length_get(read_packet_ptr, &recv_len);
    left_len = recv_len - prepend_offset;
    if(recv_len == 0){
        if(expired(&t))break;
    }

    
    if( left_len >= len ) {
      nx_packet_data_extract_offset(read_packet_ptr, prepend_offset, &buffer[copied_idx], len, &bytes_copied);
      if( left_len == bytes_copied ){
        prepend_offset = 0;
        nx_packet_release(read_packet_ptr);
        read_packet_ptr = NULL;
      } else {
        prepend_offset += bytes_copied;
      }
    } else {
      nx_packet_data_extract_offset(read_packet_ptr, prepend_offset, &buffer[copied_idx], left_len, &bytes_copied);
      prepend_offset = 0;
      nx_packet_release(read_packet_ptr);
      read_packet_ptr = NULL;
    }
    len -= bytes_copied;
    copied_idx += bytes_copied;
  }
  
  return copied_idx;
}

int s7g2_write(Network* n, unsigned char* buffer, int len, int timeout_ms)
{
  UINT status;
  NX_PACKET       *packet_ptr;
  status =  nx_packet_allocate(n->my_packet_pool, &packet_ptr, NX_TCP_PACKET, timeout_ms);
  if(status) return -1;
  status = nx_packet_data_append(packet_ptr, buffer, len, n->my_packet_pool, timeout_ms);
  if(status) return -1;
  status =  nx_tcp_socket_send(&n->my_socket, packet_ptr, timeout_ms);
  if(status) return -1;
  nx_packet_release(packet_ptr);
  return len;
}

static int connect_flag = 0;

void s7g2_disconnect(Network* n) 
{
  n->byte_ip = 0;
  nx_tcp_socket_disconnect(&n->my_socket, 200);
  nx_tcp_client_socket_unbind(&n->my_socket);
  nx_tcp_socket_delete(&n->my_socket);
  connect_flag = 0;
}


void NewNetwork(Network* n) {
  n->my_ip = &g_ip0;
  n->my_packet_pool = &g_packet_pool0;
  n->byte_ip = 0;
  n->mqttread = s7g2_read;
  n->mqttwrite = s7g2_write;
  n->disconnect = s7g2_disconnect;
}

int ip_check(char *ip) 
{ 
    if(strcmp(ip, " ") == 0) 
    { 
        return -1; 
    } 
 
    int len = strlen(ip); 
 
    if( len > 15 || len < 7 ) 
        return -1; 
 
    int nNumCount = 0; 
    int nDotCount = 0; 
    int i = 0; 
 
    for( i=0; i<len; i++) 
    { 
        if(ip[i] < '0' || ip[i] > '9') 
        { 
            if(ip[i] == '.') 
            { 
                ++nDotCount; 
                nNumCount = 0; 
            } 
            else 
                return -1; 
        } 
        else 
        { 
            if(++nNumCount > 3) 
                return -1; 
        } 
    } 
 
    if(nDotCount != 3) 
        return -1; 
 
    return 1; 
} 

int ConnectNetwork(Network* n, char* addr, unsigned int port)
{
  UINT status;
  UCHAR record_buffer[200];
  UINT record_count;

  connect_flag = 0;
  status = nx_tcp_socket_create(n->my_ip, &n->my_socket, "MqttSocket", NX_IP_NORMAL, NX_DONT_FRAGMENT, NX_IP_TIME_TO_LIVE, 512, NX_NULL, NX_NULL);
  if (status) {
      return -1;
  }
  status =  nx_tcp_client_socket_bind(&n->my_socket, NX_ANY_PORT, 10000);
  if (status) {
      nx_tcp_socket_delete(&n->my_socket);
      return -1;
  }

  if( ip_check(addr) == 1) {
      int ip1,ip2,ip3,ip4;
      sscanf(addr,"%d.%d.%d.%d",&ip1,&ip2,&ip3,&ip4);
      n->byte_ip = IP_ADDRESS(ip1,ip2,ip3,ip4);
  } else {
      nx_dns_server_add(&g_dns0, IP_ADDRESS(8,8,8,8)); 
      nx_dns_ipv4_address_by_name_get(&g_dns0, (UCHAR*)addr, (VOID *)&record_buffer[0], 200, &record_count, 400); 
      n->byte_ip = *((ULONG *)(record_buffer)); 
  }
  status =  nx_tcp_client_socket_connect(&n->my_socket, n->byte_ip, port, 2000);
 if (status) { 
      nx_tcp_client_socket_unbind(&n->my_socket); 
      nx_tcp_socket_delete(&n->my_socket); 
      return -1; 
  } 
  connect_flag = 1; 
  return 0;
}

int IsNetworkConnected(Network* n)
{
  return connect_flag;
}

void NetworkDisconnect(Network* n)
{
    n->disconnect(n);
}
