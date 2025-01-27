#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <fcntl.h>
#include <pthread.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#include <linux/if.h>
#include <dirent.h>
#include <signal.h>
#include <endian.h>

#ifndef MIN
#define MIN(a, b)	((a) < (b)? (a): (b))
#endif
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

#define DBG(fmt, arg...) do { printf(fmt, ##arg); } while(0)
#define SYSCHECK(c) do{if((c)<0) {DBG("%s %d error: '%s' (code: %d)\n", __func__, __LINE__, strerror(errno), errno); return -1;}}while(0)
#define cfmakenoblock(fd) do{fcntl(fd, F_SETFL, fcntl(fd,F_GETFL) | O_NONBLOCK);}while(0)

typedef struct _QCQMI_HDR
{
   uint8_t  IFType;
   uint16_t Length;
   uint8_t  CtlFlags;  // reserved
   uint8_t  QMIType;
   uint8_t  ClientId;
} __attribute__ ((packed)) QCQMI_HDR, *PQCQMI_HDR;

typedef struct _QMICTL_SYNC_REQ_MSG
{
   uint8_t  CtlFlags;        // QMICTL_FLAG_REQUEST
   uint8_t  TransactionId;
   uint16_t QMICTLType;      // QMICTL_CTL_SYNC_REQ
   uint16_t Length;          // 0
} __attribute__ ((packed)) QMICTL_SYNC_REQ_MSG, *PQMICTL_SYNC_REQ_MSG;

typedef struct _QMICTL_SYNC_RESP_MSG
{
   uint8_t  CtlFlags;        // QMICTL_FLAG_RESPONSE
   uint8_t  TransactionId;
   uint16_t QMICTLType;      // QMICTL_CTL_SYNC_RESP
   uint16_t Length;
   uint8_t  TLVType;         // QCTLV_TYPE_RESULT_CODE
   uint16_t TLVLength;       // 0x0004
   uint16_t QMIResult;
   uint16_t QMIError;
} __attribute__ ((packed)) QMICTL_SYNC_RESP_MSG, *PQMICTL_SYNC_RESP_MSG;

typedef struct _QMICTL_SYNC_IND_MSG
{
   uint8_t  CtlFlags;        // QMICTL_FLAG_INDICATION
   uint8_t  TransactionId;
   uint16_t QMICTLType;      // QMICTL_REVOKE_CLIENT_ID_IND
   uint16_t Length;
} __attribute__ ((packed)) QMICTL_SYNC_IND_MSG, *PQMICTL_SYNC_IND_MSG;

typedef struct _QMICTL_GET_CLIENT_ID_REQ_MSG
{
   uint8_t  CtlFlags;        // QMICTL_FLAG_REQUEST
   uint8_t  TransactionId;
   uint16_t QMICTLType;      // QMICTL_GET_CLIENT_ID_REQ
   uint16_t Length;
   uint8_t  TLVType;         // QCTLV_TYPE_REQUIRED_PARAMETER
   uint16_t TLVLength;       // 1
   uint8_t  QMIType;         // QMUX type
} __attribute__ ((packed)) QMICTL_GET_CLIENT_ID_REQ_MSG, *PQMICTL_GET_CLIENT_ID_REQ_MSG;

typedef struct _QMICTL_GET_CLIENT_ID_RESP_MSG
{
   uint8_t  CtlFlags;        // QMICTL_FLAG_RESPONSE
   uint8_t  TransactionId;
   uint16_t QMICTLType;      // QMICTL_GET_CLIENT_ID_RESP
   uint16_t Length;
   uint8_t  TLVType;         // QCTLV_TYPE_RESULT_CODE
   uint16_t TLVLength;       // 0x0004
   uint16_t QMIResult;       // result code
   uint16_t QMIError;        // error code
   uint8_t  TLV2Type;        // QCTLV_TYPE_REQUIRED_PARAMETER
   uint16_t TLV2Length;      // 2
   uint8_t  QMIType;
   uint8_t  ClientId;
} __attribute__ ((packed)) QMICTL_GET_CLIENT_ID_RESP_MSG, *PQMICTL_GET_CLIENT_ID_RESP_MSG;

typedef struct _QMICTL_RELEASE_CLIENT_ID_REQ_MSG
{
   uint8_t  CtlFlags;        // QMICTL_FLAG_REQUEST
   uint8_t  TransactionId;
   uint16_t QMICTLType;      // QMICTL_RELEASE_CLIENT_ID_REQ
   uint16_t Length;
   uint8_t  TLVType;         // QCTLV_TYPE_REQUIRED_PARAMETER
   uint16_t TLVLength;       // 0x0002
   uint8_t  QMIType;
   uint8_t  ClientId;
} __attribute__ ((packed)) QMICTL_RELEASE_CLIENT_ID_REQ_MSG, *PQMICTL_RELEASE_CLIENT_ID_REQ_MSG;

typedef struct _QMICTL_RELEASE_CLIENT_ID_RESP_MSG
{
   uint8_t  CtlFlags;        // QMICTL_FLAG_RESPONSE
   uint8_t  TransactionId;
   uint16_t QMICTLType;      // QMICTL_RELEASE_CLIENT_ID_RESP
   uint16_t Length;
   uint8_t  TLVType;         // QCTLV_TYPE_RESULT_CODE
   uint16_t TLVLength;       // 0x0004
   uint16_t QMIResult;       // result code
   uint16_t QMIError;        // error code
   uint8_t  TLV2Type;        // QCTLV_TYPE_REQUIRED_PARAMETER
   uint16_t TLV2Length;      // 2
   uint8_t  QMIType;
   uint8_t  ClientId;
} __attribute__ ((packed)) QMICTL_RELEASE_CLIENT_ID_RESP_MSG, *PQMICTL_RELEASE_CLIENT_ID_RESP_MSG;

// QMICTL Control Flags
#define QMICTL_CTL_FLAG_CMD     0x00
#define QMICTL_CTL_FLAG_RSP     0x01
#define QMICTL_CTL_FLAG_IND     0x02

typedef struct _QCQMICTL_MSG_HDR
{
   uint8_t  CtlFlags;  // 00-cmd, 01-rsp, 10-ind
   uint8_t  TransactionId;
   uint16_t QMICTLType;
   uint16_t Length;
} __attribute__ ((packed)) QCQMICTL_MSG_HDR, *PQCQMICTL_MSG_HDR;

#define QCQMICTL_MSG_HDR_SIZE sizeof(QCQMICTL_MSG_HDR)

typedef struct _QCQMICTL_MSG_HDR_RESP
{
   uint8_t  CtlFlags;  // 00-cmd, 01-rsp, 10-ind
   uint8_t  TransactionId;
   uint16_t QMICTLType;
   uint16_t Length;
   uint8_t  TLVType;          // 0x02 - result code
   uint16_t TLVLength;        // 4
   uint16_t QMUXResult;       // QMI_RESULT_SUCCESS
                            // QMI_RESULT_FAILURE
   uint16_t QMUXError;        // QMI_ERR_INVALID_ARG
                            // QMI_ERR_NO_MEMORY
                            // QMI_ERR_INTERNAL
                            // QMI_ERR_FAULT
} __attribute__ ((packed)) QCQMICTL_MSG_HDR_RESP, *PQCQMICTL_MSG_HDR_RESP;

typedef struct _QMICTL_GET_VERSION_REQ_MSG
{
   uint8_t  CtlFlags;        // QMICTL_FLAG_REQUEST
   uint8_t  TransactionId;
   uint16_t QMICTLType;      // QMICTL_GET_VERSION_REQ
   uint16_t Length;          // 0
   uint8_t  TLVType;         // QCTLV_TYPE_REQUIRED_PARAMETER
   uint16_t TLVLength;       // var
   uint8_t  QMUXTypes;       // List of one byte QMUX_TYPE values
                           // 0xFF returns a list of versions for all
                           // QMUX_TYPEs implemented on the device
} __attribute__ ((packed)) QMICTL_GET_VERSION_REQ_MSG, *PQMICTL_GET_VERSION_REQ_MSG;

typedef struct _QMUX_TYPE_VERSION_STRUCT
{
   uint8_t  QMUXType;
   uint16_t MajorVersion;
   uint16_t MinorVersion;
} __attribute__ ((packed)) QMUX_TYPE_VERSION_STRUCT, *PQMUX_TYPE_VERSION_STRUCT;

typedef struct _QMICTL_GET_VERSION_RESP_MSG
{
   uint8_t  CtlFlags;        // QMICTL_FLAG_RESPONSE
   uint8_t  TransactionId;
   uint16_t QMICTLType;      // QMICTL_GET_VERSION_RESP
   uint16_t Length;
   uint8_t  TLVType;         // QCTLV_TYPE_RESULT_CODE
   uint16_t TLVLength;       // 0x0004
   uint16_t QMIResult;
   uint16_t QMIError;
   uint8_t  TLV2Type;        // QCTLV_TYPE_REQUIRED_PARAMETER
   uint16_t TLV2Length;      // var
   uint8_t  NumElements;     // Num of QMUX_TYPE_VERSION_STRUCT
   QMUX_TYPE_VERSION_STRUCT TypeVersion[0];
} __attribute__ ((packed)) QMICTL_GET_VERSION_RESP_MSG, *PQMICTL_GET_VERSION_RESP_MSG;


typedef struct _QMICTL_MSG
{
   union
   {
      // Message Header
      QCQMICTL_MSG_HDR                             QMICTLMsgHdr;
      QCQMICTL_MSG_HDR_RESP                             QMICTLMsgHdrRsp;

      // QMICTL Message
      //QMICTL_SET_INSTANCE_ID_REQ_MSG               SetInstanceIdReq;
      //QMICTL_SET_INSTANCE_ID_RESP_MSG              SetInstanceIdRsp;
      QMICTL_GET_VERSION_REQ_MSG                   GetVersionReq;
      QMICTL_GET_VERSION_RESP_MSG                  GetVersionRsp;
      QMICTL_GET_CLIENT_ID_REQ_MSG                 GetClientIdReq;
      QMICTL_GET_CLIENT_ID_RESP_MSG                GetClientIdRsp;
      //QMICTL_RELEASE_CLIENT_ID_REQ_MSG             ReleaseClientIdReq;
      QMICTL_RELEASE_CLIENT_ID_RESP_MSG            ReleaseClientIdRsp;
      //QMICTL_REVOKE_CLIENT_ID_IND_MSG              RevokeClientIdInd;
      //QMICTL_INVALID_CLIENT_ID_IND_MSG             InvalidClientIdInd;
      //QMICTL_SET_DATA_FORMAT_REQ_MSG               SetDataFormatReq;
      //QMICTL_SET_DATA_FORMAT_RESP_MSG              SetDataFormatRsp;
      QMICTL_SYNC_REQ_MSG                          SyncReq;
      QMICTL_SYNC_RESP_MSG                         SyncRsp;
      QMICTL_SYNC_IND_MSG                          SyncInd;
   };
} __attribute__ ((packed)) QMICTL_MSG, *PQMICTL_MSG;

typedef struct _QCQMUX_MSG_HDR
{
   uint8_t  CtlFlags;      // 0: single QMUX Msg; 1:
   uint16_t TransactionId;
   uint16_t Type;
   uint16_t Length;
   uint8_t payload[0];
} __attribute__ ((packed)) QCQMUX_MSG_HDR, *PQCQMUX_MSG_HDR;

typedef struct _QCQMUX_MSG_HDR_RESP
{
   uint8_t  CtlFlags;      // 0: single QMUX Msg; 1:
   uint16_t TransactionId;
   uint16_t Type;
   uint16_t Length;
   uint8_t  TLVType;          // 0x02 - result code
   uint16_t TLVLength;        // 4
   uint16_t QMUXResult;       // QMI_RESULT_SUCCESS
                            // QMI_RESULT_FAILURE
   uint16_t QMUXError;        // QMI_ERR_INVALID_ARG
                            // QMI_ERR_NO_MEMORY
                            // QMI_ERR_INTERNAL
                            // QMI_ERR_FAULT
} __attribute__ ((packed)) QCQMUX_MSG_HDR_RESP, *PQCQMUX_MSG_HDR_RESP;

typedef struct _QMI_WDA_SET_DATA_FORMAT
{
   uint16_t Type;             // QMUX type 0x0000
   uint16_t Length;
} __attribute__ ((packed)) QMI_WDA_SET_DATA_FORMAT, *PQMI_WDA_SET_DATA_FORMAT;

typedef struct _QMI_WDA_SET_DATA_FORMAT_TLV_QOS
{
   uint8_t  TLVType;
   uint16_t TLVLength;
   uint8_t  QOSSetting;
} __attribute__ ((packed)) QMI_WDA_SET_DATA_FORMAT_TLV_QOS, *PQMI_WDA_SET_DATA_FORMAT_TLV_QOS;

typedef struct _QMI_WDA_SET_DATA_FORMAT_TLV
{
   uint8_t  TLVType;
   uint16_t TLVLength;
   uint32_t  Value;
} __attribute__ ((packed)) QMI_WDA_SET_DATA_FORMAT_TLV, *PQMI_WDA_SET_DATA_FORMAT_TLV;

typedef struct _QMIWDS_ENDPOINT_TLV
{
   uint8_t  TLVType;
   uint16_t TLVLength;
   uint32_t  ep_type;
   uint32_t  iface_id;
} __attribute__ ((packed)) QMIWDS_ENDPOINT_TLV, *PQMIWDS_ENDPOINT_TLV;

typedef uint32_t UINT;
typedef struct {
    UINT size;
    UINT rx_urb_size;
    UINT ep_type;
    UINT iface_id;
    UINT mux_id;
    UINT ul_data_aggregation_max_datagrams; //0x17
    UINT ul_data_aggregation_max_size ;//0x18
    UINT dl_minimum_padding; //0x1A
} QMAP_SETTING;

typedef struct {
    unsigned int size;
    unsigned int rx_urb_size;
    unsigned int ep_type;
    unsigned int iface_id;
    unsigned int qmap_mode;
    unsigned int qmap_version;
    unsigned int dl_minimum_padding;
    char ifname[8][16];
    unsigned char mux_id[8];
} RMNET_INFO;

typedef struct _QMI_WDA_SET_DATA_FORMAT_REQ_MSG
{
    uint8_t  CtlFlags;      // 0: single QMUX Msg; 1:
    uint16_t TransactionId;
    uint16_t Type;
    uint16_t Length;
    QMI_WDA_SET_DATA_FORMAT_TLV_QOS QosDataFormatTlv;
    QMI_WDA_SET_DATA_FORMAT_TLV UnderlyingLinkLayerProtocolTlv;
    QMI_WDA_SET_DATA_FORMAT_TLV UplinkDataAggregationProtocolTlv;
    QMI_WDA_SET_DATA_FORMAT_TLV DownlinkDataAggregationProtocolTlv;
    QMI_WDA_SET_DATA_FORMAT_TLV DownlinkDataAggregationMaxDatagramsTlv;
    QMI_WDA_SET_DATA_FORMAT_TLV DownlinkDataAggregationMaxSizeTlv;
    QMIWDS_ENDPOINT_TLV epTlv;
#if 1
    QMI_WDA_SET_DATA_FORMAT_TLV DlMinimumPassingTlv;
    QMI_WDA_SET_DATA_FORMAT_TLV UplinkDataAggregationMaxDatagramsTlv;
    QMI_WDA_SET_DATA_FORMAT_TLV UplinkDataAggregationMaxSizeTlv;
#endif
} __attribute__ ((packed)) QMI_WDA_SET_DATA_FORMAT_REQ_MSG, *PQMI_WDA_SET_DATA_FORMAT_REQ_MSG;

typedef struct _QCQMUX_TLV
{
   uint8_t Type;
   uint16_t Length;
   uint8_t  Value[0];
} __attribute__ ((packed)) QCQMUX_TLV, *PQCQMUX_TLV;

typedef struct _QMUX_MSG
{
   union
   {
      // Message Header
      QCQMUX_MSG_HDR                           QMUXMsgHdr;
      QCQMUX_MSG_HDR_RESP                      QMUXMsgHdrResp;
      QMI_WDA_SET_DATA_FORMAT_REQ_MSG      SetDataFormatReq;
    };
} __attribute__ ((packed)) QMUX_MSG, *PQMUX_MSG;

typedef struct _QCQMIMSG {
    QCQMI_HDR QMIHdr;
    union {
        QMICTL_MSG CTLMsg;
        QMUX_MSG MUXMsg;
    };
} __attribute__ ((packed)) QCQMIMSG, *PQCQMIMSG;


// QMUX Message Definitions -- QMI SDU
#define QMUX_CTL_FLAG_SINGLE_MSG    0x00
#define QMUX_CTL_FLAG_COMPOUND_MSG  0x01
#define QMUX_CTL_FLAG_TYPE_CMD      0x00
#define QMUX_CTL_FLAG_TYPE_RSP      0x02
#define QMUX_CTL_FLAG_TYPE_IND      0x04
#define QMUX_CTL_FLAG_MASK_COMPOUND 0x01
#define QMUX_CTL_FLAG_MASK_TYPE     0x06 // 00-cmd, 01-rsp, 10-ind

#define USB_CTL_MSG_TYPE_QMI 0x01

#define QMICTL_FLAG_REQUEST    0x00
#define QMICTL_FLAG_RESPONSE   0x01
#define QMICTL_FLAG_INDICATION 0x02

// QMICTL Type
#define QMICTL_SET_INSTANCE_ID_REQ    0x0020
#define QMICTL_SET_INSTANCE_ID_RESP   0x0020
#define QMICTL_GET_VERSION_REQ        0x0021
#define QMICTL_GET_VERSION_RESP       0x0021
#define QMICTL_GET_CLIENT_ID_REQ      0x0022
#define QMICTL_GET_CLIENT_ID_RESP     0x0022
#define QMICTL_RELEASE_CLIENT_ID_REQ  0x0023
#define QMICTL_RELEASE_CLIENT_ID_RESP 0x0023
#define QMICTL_REVOKE_CLIENT_ID_IND   0x0024
#define QMICTL_INVALID_CLIENT_ID_IND  0x0025
#define QMICTL_SET_DATA_FORMAT_REQ    0x0026
#define QMICTL_SET_DATA_FORMAT_RESP   0x0026
#define QMICTL_SYNC_REQ               0x0027
#define QMICTL_SYNC_RESP              0x0027
#define QMICTL_SYNC_IND               0x0027

#define QCTLV_TYPE_REQUIRED_PARAMETER 0x01

// Define QMI Type
typedef enum _QMI_SERVICE_TYPE
{
   QMUX_TYPE_CTL  = 0x00,
   QMUX_TYPE_WDS  = 0x01,
   QMUX_TYPE_DMS  = 0x02,
   QMUX_TYPE_NAS  = 0x03,
   QMUX_TYPE_QOS  = 0x04,
   QMUX_TYPE_WMS  = 0x05,
   QMUX_TYPE_PDS  = 0x06,
   QMUX_TYPE_UIM  = 0x0B,
   QMUX_TYPE_WDS_IPV6  = 0x11,
   QMUX_TYPE_WDA  = 0x1A,
   QMUX_TYPE_MAX  = 0xFF,
   QMUX_TYPE_ALL  = 0xFF
} QMI_SERVICE_TYPE;

#define QMI_WDA_SET_DATA_FORMAT_REQ      0x0020
#define QMI_WDA_SET_DATA_FORMAT_RESP     0x0020

struct qlistnode
{
    struct qlistnode *next;
    struct qlistnode *prev;
};

#define qnode_to_item(node, container, member) \
    (container *) (((char*) (node)) - offsetof(container, member))

#define qlist_for_each(node, list) \
    for (node = (list)->next; node != (list); node = node->next)

#define qlist_empty(list) ((list) == (list)->next)
#define qlist_head(list) ((list)->next)
#define qlist_tail(list) ((list)->prev)

typedef struct {
    struct qlistnode qnode;
    uint8_t ClientFd;
    QCQMIMSG qmi[0];
} QMI_PROXY_MSG;

typedef struct {
    struct qlistnode qnode;
    uint8_t QMIType;
    uint8_t ClientId;
    unsigned AccessTime;
} QMI_PROXY_CLINET;

typedef struct {
    struct qlistnode qnode;
    struct qlistnode client_qnode;
    uint8_t ClientFd;
    unsigned AccessTime;
} QMI_PROXY_CONNECTION;

static void qlist_init(struct qlistnode *node)
{
    node->next = node;
    node->prev = node;
}

static void qlist_add_tail(struct qlistnode *head, struct qlistnode *item)
{
    item->next = head;
    item->prev = head->prev;
    head->prev->next = item;
    head->prev = item;
}

static void qlist_remove(struct qlistnode *item)
{
    item->next->prev = item->prev;
    item->prev->next = item->next;
}

static int qmi_proxy_quit = 0;
static pthread_t thread_id = 0;
static int cdc_wdm_fd = -1;
static int qmi_proxy_server_fd = -1;
static struct qlistnode qmi_proxy_connection;
static struct qlistnode qmi_proxy_ctl_msg;
static PQCQMIMSG s_pCtlReq;
static PQCQMIMSG s_pCtlRsq;
static pthread_mutex_t s_ctlmutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t s_ctlcond = PTHREAD_COND_INITIALIZER;

static void get_driver_rmnet_info(char *ifname, RMNET_INFO *rmnet_info) {
    int ifc_ctl_sock;
    struct ifreq ifr;
    int rc;
    int request = 0x89F3;
    unsigned char data[512];

    memset(rmnet_info, 0x00, sizeof(*rmnet_info));

    ifc_ctl_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (ifc_ctl_sock <= 0) {
        DBG("socket() failed: %s\n", strerror(errno));
        return;
    }
    
    memset(&ifr, 0, sizeof(struct ifreq));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    ifr.ifr_name[IFNAMSIZ - 1] = 0;    
    ifr.ifr_ifru.ifru_data = (void *)data;
        
    rc = ioctl(ifc_ctl_sock, request, &ifr);
    if (rc < 0) {
        DBG("ioctl(0x%x, rmnet_info) failed: %s, rc=%d", request, strerror(errno), rc);
    }
    else {
        memcpy(rmnet_info, data, sizeof(*rmnet_info));
    }

    close(ifc_ctl_sock);
}

static void set_driver_ul_params_setting(char *ifname, QMAP_SETTING *qmap_settings) {
    int ifc_ctl_sock;
    struct ifreq ifr;
    int rc;
    int request = 0x89F2;

    ifc_ctl_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (ifc_ctl_sock <= 0) {
        DBG("socket() failed: %s\n", strerror(errno));
        return;
    }
    
    memset(&ifr, 0, sizeof(struct ifreq));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    ifr.ifr_name[IFNAMSIZ - 1] = 0;    
    ifr.ifr_ifru.ifru_data = (void *)qmap_settings;
        
    rc = ioctl(ifc_ctl_sock, request, &ifr);
    if (rc < 0) {
        DBG("ioctl(0x%x, qmap_settings) failed: %s, rc=%d\n", request, strerror(errno), rc);
    }

    close(ifc_ctl_sock);	
}

static void setTimespecRelative(struct timespec *p_ts, long long msec)
{
    struct timeval tv;
    const int NS_PER_S = 1000 * 1000 * 1000;
    gettimeofday(&tv, (struct timezone *) NULL);

    /* what's really funny about this is that I know
       pthread_cond_timedwait just turns around and makes this
       a relative time again */
    p_ts->tv_sec = tv.tv_sec + (msec / 1000);
    p_ts->tv_nsec = (tv.tv_usec + (msec % 1000) * 1000L ) * 1000L;
    /* assuming tv.tv_usec < 10^6 */
    if (p_ts->tv_nsec >= NS_PER_S) {
        p_ts->tv_sec++;
        p_ts->tv_nsec -= NS_PER_S;
    }
}

static int pthread_cond_timeout_np(pthread_cond_t *cond, pthread_mutex_t * mutex, unsigned msecs) {
    if (msecs != 0) {
        struct timespec ts;
        setTimespecRelative(&ts, msecs);
        return pthread_cond_timedwait(cond, mutex, &ts);
    } else {
        return pthread_cond_wait(cond, mutex);
    }
}

static int create_local_server(const char *name) {
    int sockfd = -1;
    int reuse_addr = 1;
    struct sockaddr_un sockaddr;
    socklen_t alen;

    /*Create server socket*/
    SYSCHECK(sockfd = socket(AF_LOCAL, SOCK_STREAM, 0));

    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.sun_family = AF_LOCAL;
    sockaddr.sun_path[0] = 0;
    memcpy(sockaddr.sun_path + 1, name, strlen(name) );

    alen = strlen(name) + offsetof(struct sockaddr_un, sun_path) + 1;
    SYSCHECK(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr,sizeof(reuse_addr)));
    if(bind(sockfd, (struct sockaddr *)&sockaddr, alen) < 0) {
        close(sockfd);
        DBG("%s bind %s errno: %d (%s)\n", __func__, name, errno, strerror(errno));
        return -1;
    }

    DBG("local server: %s sockfd = %d\n", name, sockfd);
    cfmakenoblock(sockfd);
    listen(sockfd, 1);

    return sockfd;
}

static void accept_qmi_connection(int serverfd) {
    int clientfd = -1;
    unsigned char addr[128];
    socklen_t alen = sizeof(addr);
    QMI_PROXY_CONNECTION *qmi_con;

    clientfd = accept(serverfd, (struct sockaddr *)addr, &alen);

    qmi_con = (QMI_PROXY_CONNECTION *)malloc(sizeof(QMI_PROXY_CONNECTION));
    if (qmi_con) {
        qlist_init(&qmi_con->qnode);
        qlist_init(&qmi_con->client_qnode);
        qmi_con->ClientFd= clientfd;
        qmi_con->AccessTime = 0;
        DBG("+++ ClientFd=%d\n", qmi_con->ClientFd);
        qlist_add_tail(&qmi_proxy_connection, &qmi_con->qnode);
    }

    cfmakenoblock(clientfd);
}

static void cleanup_qmi_connection(int clientfd) {
    struct qlistnode *con_node, *qmi_node;

    qlist_for_each(con_node, &qmi_proxy_connection) {
        QMI_PROXY_CONNECTION *qmi_con = qnode_to_item(con_node, QMI_PROXY_CONNECTION, qnode);

        if (qmi_con->ClientFd == clientfd) {
            while (!qlist_empty(&qmi_con->client_qnode)) {
                QMI_PROXY_CLINET *qmi_client = qnode_to_item(qlist_head(&qmi_con->client_qnode), QMI_PROXY_CLINET, qnode);

                DBG("xxx ClientFd=%d QMIType=%d ClientId=%d\n", qmi_con->ClientFd, qmi_client->QMIType, qmi_client->ClientId);

                qlist_remove(&qmi_client->qnode);
                free(qmi_client);
            }

            qlist_for_each(qmi_node, &qmi_proxy_ctl_msg) {
                QMI_PROXY_MSG *qmi_msg = qnode_to_item(qmi_node, QMI_PROXY_MSG, qnode);

                if (qmi_msg->ClientFd == qmi_con->ClientFd) {
                    qlist_remove(&qmi_msg->qnode);
                    free(qmi_msg);
                    break;
                 }
            }

            DBG("--- ClientFd=%d\n", qmi_con->ClientFd);
            close(qmi_con->ClientFd);
            qlist_remove(&qmi_con->qnode);
            free(qmi_con);
            break;
        }
    }
}

static void get_client_id(QMI_PROXY_CONNECTION *qmi_con, PQMICTL_GET_CLIENT_ID_RESP_MSG pClient) {
    if (pClient->QMIResult == 0 && pClient->QMIError == 0) {
        QMI_PROXY_CLINET *qmi_client = (QMI_PROXY_CLINET *)malloc(sizeof(QMI_PROXY_CLINET));

        qlist_init(&qmi_client->qnode);
        qmi_client->QMIType = pClient->QMIType;
        qmi_client->ClientId = pClient->ClientId;
        qmi_client->AccessTime = 0;

        DBG("+++ ClientFd=%d QMIType=%d ClientId=%d\n", qmi_con->ClientFd, qmi_client->QMIType, qmi_client->ClientId);
        qlist_add_tail(&qmi_con->client_qnode, &qmi_client->qnode);
    }
}

static void release_client_id(QMI_PROXY_CONNECTION *qmi_con, PQMICTL_RELEASE_CLIENT_ID_RESP_MSG pClient) {
    struct qlistnode *client_node;

    if (pClient->QMIResult == 0 && pClient->QMIError == 0) {
        qlist_for_each (client_node, &qmi_con->client_qnode) {
            QMI_PROXY_CLINET *qmi_client = qnode_to_item(client_node, QMI_PROXY_CLINET, qnode);

            if (pClient->QMIType == qmi_client->QMIType && pClient->ClientId == qmi_client->ClientId) {
                DBG("--- ClientFd=%d QMIType=%d ClientId=%d\n", qmi_con->ClientFd, qmi_client->QMIType, qmi_client->ClientId);
                qlist_remove(&qmi_client->qnode);
                free(qmi_client);
                break;
            }
        }
    }
}

static int verbose_debug = 0;
static int send_qmi_to_cdc_wdm(PQCQMIMSG pQMI) {
    struct pollfd pollfds[]= {{cdc_wdm_fd, POLLOUT, 0}};
    ssize_t ret = 0;

    do {
        ret = poll(pollfds, sizeof(pollfds)/sizeof(pollfds[0]), 5000);
    } while ((ret < 0) && (errno == EINTR));

    if (pollfds[0].revents & POLLOUT) {
        ssize_t size = le16toh(pQMI->QMIHdr.Length) + 1;
        ret = write(cdc_wdm_fd, pQMI, size);
        if (verbose_debug)
        {
            ssize_t i;
            printf("w %d %zd: ", cdc_wdm_fd, ret);
            for (i = 0; i < size; i++)
                printf("%02x ", ((uint8_t *)pQMI)[i]);
            printf("\n");
        }
    }

    return ret;
}

static int send_qmi_to_client(PQCQMIMSG pQMI, int clientFd) {
    struct pollfd pollfds[]= {{clientFd, POLLOUT, 0}};
    ssize_t ret = 0;

    do {
        ret = poll(pollfds, sizeof(pollfds)/sizeof(pollfds[0]), 5000);
    } while ((ret < 0) && (errno == EINTR));

    if (pollfds[0].revents & POLLOUT) {
        ssize_t size = le16toh(pQMI->QMIHdr.Length) + 1;
        ret = write(clientFd, pQMI, size);
        if (verbose_debug)
        {
            ssize_t i;
            printf("w %d %zd: ", clientFd, ret);
            for (i = 0; i < 16; i++)
                printf("%02x ", ((uint8_t *)pQMI)[i]);
            printf("\n");
        }
    }

    return ret;
}

static int modem_reset_flag = 0;
static void recv_qmi(PQCQMIMSG pQMI, unsigned size) {
    struct qlistnode *con_node, *client_node;

    if (qmi_proxy_server_fd <= 0) {
        pthread_mutex_lock(&s_ctlmutex);

        if (s_pCtlReq != NULL) {
            if (pQMI->QMIHdr.QMIType == QMUX_TYPE_CTL
                && s_pCtlReq->CTLMsg.QMICTLMsgHdrRsp.TransactionId == pQMI->CTLMsg.QMICTLMsgHdrRsp.TransactionId) {
                s_pCtlRsq = malloc(size);
                memcpy(s_pCtlRsq, pQMI, size);
                pthread_cond_signal(&s_ctlcond);
            }
            else if (pQMI->QMIHdr.QMIType != QMUX_TYPE_CTL
                && s_pCtlReq->MUXMsg.QMUXMsgHdr.TransactionId == pQMI->MUXMsg.QMUXMsgHdr.TransactionId) {
                s_pCtlRsq = malloc(size);
                memcpy(s_pCtlRsq, pQMI, size);
                pthread_cond_signal(&s_ctlcond);
            }
        }

        pthread_mutex_unlock(&s_ctlmutex);
    }
    else if (pQMI->QMIHdr.QMIType == QMUX_TYPE_CTL) {
        if (pQMI->CTLMsg.QMICTLMsgHdr.CtlFlags == QMICTL_CTL_FLAG_RSP) {
            if (!qlist_empty(&qmi_proxy_ctl_msg)) {
                QMI_PROXY_MSG *qmi_msg = qnode_to_item(qlist_head(&qmi_proxy_ctl_msg), QMI_PROXY_MSG, qnode);

                qlist_for_each(con_node, &qmi_proxy_connection) {
                    QMI_PROXY_CONNECTION *qmi_con = qnode_to_item(con_node, QMI_PROXY_CONNECTION, qnode);

                    if (qmi_con->ClientFd == qmi_msg->ClientFd) {
                        send_qmi_to_client(pQMI, qmi_msg->ClientFd);

                        if (le16toh(pQMI->CTLMsg.QMICTLMsgHdrRsp.QMICTLType) == QMICTL_GET_CLIENT_ID_RESP)
                            get_client_id(qmi_con, &pQMI->CTLMsg.GetClientIdRsp);
                        else if ((le16toh(pQMI->CTLMsg.QMICTLMsgHdrRsp.QMICTLType) == QMICTL_RELEASE_CLIENT_ID_RESP) ||
                                (le16toh(pQMI->CTLMsg.QMICTLMsgHdrRsp.QMICTLType) == QMICTL_REVOKE_CLIENT_ID_IND)) {
                            release_client_id(qmi_con, &pQMI->CTLMsg.ReleaseClientIdRsp);
                            if (le16toh(pQMI->CTLMsg.QMICTLMsgHdrRsp.QMICTLType) == QMICTL_REVOKE_CLIENT_ID_IND)
                                modem_reset_flag = 1;
                        }
                        else {
                        }
                    }
                }

                qlist_remove(&qmi_msg->qnode);
                free(qmi_msg);
            }
        }

        if (!qlist_empty(&qmi_proxy_ctl_msg)) {
            QMI_PROXY_MSG *qmi_msg = qnode_to_item(qlist_head(&qmi_proxy_ctl_msg), QMI_PROXY_MSG, qnode);

            qlist_for_each(con_node, &qmi_proxy_connection) {
                QMI_PROXY_CONNECTION *qmi_con = qnode_to_item(con_node, QMI_PROXY_CONNECTION, qnode);

                if (qmi_con->ClientFd == qmi_msg->ClientFd) {
                    send_qmi_to_cdc_wdm(qmi_msg->qmi);
                }
            }
        }
    }
    else  {
        qlist_for_each(con_node, &qmi_proxy_connection) {
            QMI_PROXY_CONNECTION *qmi_con = qnode_to_item(con_node, QMI_PROXY_CONNECTION, qnode);

            qlist_for_each(client_node, &qmi_con->client_qnode) {
                QMI_PROXY_CLINET *qmi_client = qnode_to_item(client_node, QMI_PROXY_CLINET, qnode);
                if (pQMI->QMIHdr.QMIType == qmi_client->QMIType) {
                    if (pQMI->QMIHdr.ClientId == 0 || pQMI->QMIHdr.ClientId == qmi_client->ClientId) {
                        send_qmi_to_client(pQMI, qmi_con->ClientFd);
                    }
                }
            }
        }
    }
}

static int send_qmi(PQCQMIMSG pQMI, unsigned size, int clientfd) {
    if (qmi_proxy_server_fd <= 0) {
        send_qmi_to_cdc_wdm(pQMI);
    }
    else if (pQMI->QMIHdr.QMIType == QMUX_TYPE_CTL) {
        QMI_PROXY_MSG *qmi_msg;

        if (qlist_empty(&qmi_proxy_ctl_msg))
            send_qmi_to_cdc_wdm(pQMI);

        qmi_msg = malloc(sizeof(QMI_PROXY_MSG) + size);
        qlist_init(&qmi_msg->qnode);
        qmi_msg->ClientFd = clientfd;
        memcpy(qmi_msg->qmi, pQMI, size);
        qlist_add_tail(&qmi_proxy_ctl_msg, &qmi_msg->qnode);
    }
    else {
        send_qmi_to_cdc_wdm(pQMI);
    }

    return 0;
}

static int send_qmi_timeout(PQCQMIMSG pRequest, PQCQMIMSG *ppResponse, unsigned mseconds) {
    int ret;

    pthread_mutex_lock(&s_ctlmutex);

    s_pCtlReq = pRequest;
    s_pCtlRsq = NULL;
    if (ppResponse) *ppResponse = NULL;

    send_qmi_to_cdc_wdm(pRequest);
    ret = pthread_cond_timeout_np(&s_ctlcond, &s_ctlmutex, mseconds);
    if (!ret) {
        if (s_pCtlRsq && ppResponse) {
            *ppResponse = s_pCtlRsq;
        } else if (s_pCtlRsq) {
            free(s_pCtlRsq);
        }
    } else {
        DBG("%s ret=%d\n", __func__, ret);
    }

    s_pCtlReq = NULL;
    pthread_mutex_unlock(&s_ctlmutex);

    return ret;
}

static PQCQMUX_TLV qmi_find_tlv (PQCQMIMSG pQMI, uint8_t TLVType) {
    int Length = 0;

    while (Length < le16toh(pQMI->MUXMsg.QMUXMsgHdr.Length)) {
        PQCQMUX_TLV pTLV = (PQCQMUX_TLV)(&pQMI->MUXMsg.QMUXMsgHdr.payload[Length]);

        //DBG("TLV {%02x, %04x}\n", pTLV->Type, pTLV->Length);
        if (pTLV->Type == TLVType) {
            return pTLV;
        }

        Length += (le16toh((pTLV->Length)) + sizeof(QCQMUX_TLV));
    }

   return NULL;
}

static int qmi_proxy_init(char *qmi_device, char *ifname) {
    unsigned i;
    int ret;
    QCQMIMSG _QMI;
    PQCQMIMSG pQMI = &_QMI;
    PQCQMIMSG pRsp;
    uint8_t TransactionId = 0xC1;
    uint8_t WDAClientId = 0;
    unsigned rx_urb_size = 0;
    unsigned ep_type, iface_id;
    unsigned qmap_version;
    QMAP_SETTING qmap_settings = {0};
    RMNET_INFO rmnet_info;

    DBG("%s enter ifname %s\n", __func__, ifname);

    pQMI->QMIHdr.IFType   = USB_CTL_MSG_TYPE_QMI;
    pQMI->QMIHdr.CtlFlags = 0x00;
    pQMI->QMIHdr.QMIType  = QMUX_TYPE_CTL;
    pQMI->QMIHdr.ClientId= 0x00;

    pQMI->CTLMsg.QMICTLMsgHdr.CtlFlags = QMICTL_FLAG_REQUEST;

    for (i = 0; i < 10; i++) {
        pQMI->CTLMsg.SyncReq.TransactionId = TransactionId++;
        pQMI->CTLMsg.SyncReq.QMICTLType = QMICTL_SYNC_REQ;
        pQMI->CTLMsg.SyncReq.Length = 0;

        pQMI->QMIHdr.Length =
            htole16(le16toh(pQMI->CTLMsg.QMICTLMsgHdr.Length) + sizeof(QCQMI_HDR) + sizeof(QCQMICTL_MSG_HDR) - 1);

        ret = send_qmi_timeout(pQMI, NULL, 1000);
        if (!ret)
            break;
    }

    if (ret)
        goto qmi_proxy_init_fail;

    pQMI->CTLMsg.GetVersionReq.TransactionId = TransactionId++;
    pQMI->CTLMsg.GetVersionReq.QMICTLType = htole16(QMICTL_GET_VERSION_REQ);
    pQMI->CTLMsg.GetVersionReq.Length = htole16(0x0004);
    pQMI->CTLMsg.GetVersionReq.TLVType= QCTLV_TYPE_REQUIRED_PARAMETER;
    pQMI->CTLMsg.GetVersionReq.TLVLength = htole16(0x0001);
    pQMI->CTLMsg.GetVersionReq.QMUXTypes = QMUX_TYPE_ALL;

    pQMI->QMIHdr.Length = htole16(le16toh(pQMI->CTLMsg.QMICTLMsgHdr.Length) + sizeof(QCQMI_HDR) + sizeof(QCQMICTL_MSG_HDR) - 1);

    ret = send_qmi_timeout(pQMI, &pRsp, 3000);
    if (ret || (pRsp == NULL))
        goto qmi_proxy_init_fail;

    if (pRsp) {
        uint8_t  NumElements = 0;
        if (pRsp->CTLMsg.QMICTLMsgHdrRsp.QMUXResult ||pRsp->CTLMsg.QMICTLMsgHdrRsp.QMUXError) {
            DBG("QMICTL_GET_VERSION_REQ QMUXResult=%d, QMUXError=%d\n",
                le16toh(pRsp->CTLMsg.QMICTLMsgHdrRsp.QMUXResult), le16toh(pRsp->CTLMsg.QMICTLMsgHdrRsp.QMUXError));
            goto qmi_proxy_init_fail;
        }

        for (NumElements = 0; NumElements < pRsp->CTLMsg.GetVersionRsp.NumElements; NumElements++) {
            if (verbose_debug)
            DBG("QMUXType = %02x Version = %d.%d\n",
                pRsp->CTLMsg.GetVersionRsp.TypeVersion[NumElements].QMUXType,
                le16toh(pRsp->CTLMsg.GetVersionRsp.TypeVersion[NumElements].MajorVersion),
                le16toh(pRsp->CTLMsg.GetVersionRsp.TypeVersion[NumElements].MinorVersion));
        }
    }
    free(pRsp);

    pQMI->CTLMsg.GetClientIdReq.TransactionId = TransactionId++;
    pQMI->CTLMsg.GetClientIdReq.QMICTLType = htole16(QMICTL_GET_CLIENT_ID_REQ);
    pQMI->CTLMsg.GetClientIdReq.Length = htole16(0x0004);
    pQMI->CTLMsg.GetClientIdReq.TLVType= QCTLV_TYPE_REQUIRED_PARAMETER;
    pQMI->CTLMsg.GetClientIdReq.TLVLength = htole16(0x0001);
    pQMI->CTLMsg.GetClientIdReq.QMIType = QMUX_TYPE_WDA;

    pQMI->QMIHdr.Length = htole16(le16toh(pQMI->CTLMsg.QMICTLMsgHdr.Length) + sizeof(QCQMI_HDR) + sizeof(QCQMICTL_MSG_HDR) - 1);

    ret = send_qmi_timeout(pQMI, &pRsp, 3000);
    if (ret || (pRsp == NULL))
        goto qmi_proxy_init_fail;

    if (pRsp) {
        if (pRsp->CTLMsg.QMICTLMsgHdrRsp.QMUXResult ||pRsp->CTLMsg.QMICTLMsgHdrRsp.QMUXError) {
            DBG("QMICTL_GET_CLIENT_ID_REQ QMUXResult=%d, QMUXError=%d\n",
                le16toh(pRsp->CTLMsg.QMICTLMsgHdrRsp.QMUXResult), le16toh(pRsp->CTLMsg.QMICTLMsgHdrRsp.QMUXError));
            goto qmi_proxy_init_fail;
        }

        WDAClientId = pRsp->CTLMsg.GetClientIdRsp.ClientId;
        if (verbose_debug) DBG("WDAClientId = %d\n", WDAClientId);
    }
    free(pRsp);

    rx_urb_size = 32*1024; //must same as rx_urb_size defined in GobiNet&qmi_wwan driver //SDX24&SDX55 support 32KB
    if(strncmp(qmi_device, "/dev/mhi_", strlen("/dev/mhi_")) == 0)
        ep_type = 0x03;
    else
        ep_type = 0x02;
    iface_id = 0x05;
    qmap_version = 5;

    qmap_settings.size = sizeof(qmap_settings);
    qmap_settings.ul_data_aggregation_max_datagrams = 16;
    qmap_settings.ul_data_aggregation_max_size = 8*1024;
    qmap_settings.dl_minimum_padding = 16;

    get_driver_rmnet_info(ifname, &rmnet_info);
    if (rmnet_info.size) {
        rx_urb_size = rmnet_info.rx_urb_size;
        ep_type = rmnet_info.ep_type;
        iface_id = rmnet_info.iface_id;
        qmap_version = rmnet_info.qmap_version;
        qmap_settings.dl_minimum_padding = rmnet_info.dl_minimum_padding;
    }
    qmap_settings.dl_minimum_padding = 0; //no effect when register to real netowrk

    pQMI->QMIHdr.IFType   = USB_CTL_MSG_TYPE_QMI;
    pQMI->QMIHdr.CtlFlags = 0x00;
    pQMI->QMIHdr.QMIType  = QMUX_TYPE_WDA;
    pQMI->QMIHdr.ClientId= WDAClientId;

    pQMI->MUXMsg.QMUXMsgHdr.CtlFlags = QMICTL_FLAG_REQUEST;
    pQMI->MUXMsg.QMUXMsgHdr.TransactionId = htole16(TransactionId++);

    pQMI->MUXMsg.SetDataFormatReq.Type = htole16(QMI_WDA_SET_DATA_FORMAT_REQ);
    pQMI->MUXMsg.SetDataFormatReq.Length = htole16(sizeof(QMI_WDA_SET_DATA_FORMAT_REQ_MSG) - sizeof(QCQMUX_MSG_HDR));

//Indicates whether the Quality of Service(QOS) data format is used by the client.
    pQMI->MUXMsg.SetDataFormatReq.QosDataFormatTlv.TLVType = 0x10;
    pQMI->MUXMsg.SetDataFormatReq.QosDataFormatTlv.TLVLength = htole16(0x0001);
    pQMI->MUXMsg.SetDataFormatReq.QosDataFormatTlv.QOSSetting = 0; /* no-QOS header */
//Underlying Link Layer Protocol
    pQMI->MUXMsg.SetDataFormatReq.UnderlyingLinkLayerProtocolTlv.TLVType = 0x11;
    pQMI->MUXMsg.SetDataFormatReq.UnderlyingLinkLayerProtocolTlv.TLVLength = htole16(4);
    pQMI->MUXMsg.SetDataFormatReq.UnderlyingLinkLayerProtocolTlv.Value = htole32(0x02);     /* Set IP mode */
//Uplink (UL) data aggregation protocol to be used for uplink data transfer.
    pQMI->MUXMsg.SetDataFormatReq.UplinkDataAggregationProtocolTlv.TLVType = 0x12;
    pQMI->MUXMsg.SetDataFormatReq.UplinkDataAggregationProtocolTlv.TLVLength = htole16(4);
    pQMI->MUXMsg.SetDataFormatReq.UplinkDataAggregationProtocolTlv.Value = htole32(qmap_version); //UL QMAP is enabled
//Downlink (DL) data aggregation protocol to be used for downlink data transfer
    pQMI->MUXMsg.SetDataFormatReq.DownlinkDataAggregationProtocolTlv.TLVType = 0x13;
    pQMI->MUXMsg.SetDataFormatReq.DownlinkDataAggregationProtocolTlv.TLVLength = htole16(4);
    pQMI->MUXMsg.SetDataFormatReq.DownlinkDataAggregationProtocolTlv.Value = htole32(qmap_version); //UL QMAP is enabled
//Maximum number of datagrams in a single aggregated packet on downlink
    pQMI->MUXMsg.SetDataFormatReq.DownlinkDataAggregationMaxDatagramsTlv.TLVType = 0x15;
    pQMI->MUXMsg.SetDataFormatReq.DownlinkDataAggregationMaxDatagramsTlv.TLVLength = htole16(4);
    pQMI->MUXMsg.SetDataFormatReq.DownlinkDataAggregationMaxDatagramsTlv.Value = htole32((rx_urb_size>2048)?(rx_urb_size/1024):1);
//Maximum size in bytes of a single aggregated packet allowed on downlink
    pQMI->MUXMsg.SetDataFormatReq.DownlinkDataAggregationMaxSizeTlv.TLVType = 0x16;
    pQMI->MUXMsg.SetDataFormatReq.DownlinkDataAggregationMaxSizeTlv.TLVLength = htole16(4);
    pQMI->MUXMsg.SetDataFormatReq.DownlinkDataAggregationMaxSizeTlv.Value = htole32(rx_urb_size);
//Peripheral End Point ID
    pQMI->MUXMsg.SetDataFormatReq.epTlv.TLVType = 0x17;
    pQMI->MUXMsg.SetDataFormatReq.epTlv.TLVLength = htole16(8);
    pQMI->MUXMsg.SetDataFormatReq.epTlv.ep_type = htole32(ep_type); // DATA_EP_TYPE_BAM_DMUX
    pQMI->MUXMsg.SetDataFormatReq.epTlv.iface_id = htole32(iface_id);

#if 1
//Maximum number of datagrams in a single aggregated packet on uplink
    pQMI->MUXMsg.SetDataFormatReq.DlMinimumPassingTlv.TLVType = 0x19;
    pQMI->MUXMsg.SetDataFormatReq.DlMinimumPassingTlv.TLVLength = htole16(4);
    pQMI->MUXMsg.SetDataFormatReq.DlMinimumPassingTlv.Value = htole32(qmap_settings.dl_minimum_padding);

//Maximum number of datagrams in a single aggregated packet on uplink
    pQMI->MUXMsg.SetDataFormatReq.UplinkDataAggregationMaxDatagramsTlv.TLVType = 0x1B;
    pQMI->MUXMsg.SetDataFormatReq.UplinkDataAggregationMaxDatagramsTlv.TLVLength = htole16(4);
    pQMI->MUXMsg.SetDataFormatReq.UplinkDataAggregationMaxDatagramsTlv.Value = htole32(qmap_settings.ul_data_aggregation_max_datagrams);

//Maximum size in bytes of a single aggregated packet allowed on downlink
    pQMI->MUXMsg.SetDataFormatReq.UplinkDataAggregationMaxSizeTlv.TLVType = 0x1C;
    pQMI->MUXMsg.SetDataFormatReq.UplinkDataAggregationMaxSizeTlv.TLVLength = htole16(4);
    pQMI->MUXMsg.SetDataFormatReq.UplinkDataAggregationMaxSizeTlv.Value = htole32(qmap_settings.ul_data_aggregation_max_size);
#endif

    pQMI->QMIHdr.Length = htole16(le16toh(pQMI->MUXMsg.QMUXMsgHdr.Length) + sizeof(QCQMI_HDR) + sizeof(QCQMUX_MSG_HDR) - 1);

    ret = send_qmi_timeout(pQMI, &pRsp, 3000);
    if (ret || (pRsp == NULL))
        goto qmi_proxy_init_fail;

    if (pRsp) {
        PQMI_WDA_SET_DATA_FORMAT_TLV pFormat;

        if (pRsp->MUXMsg.QMUXMsgHdrResp.QMUXResult || pRsp->MUXMsg.QMUXMsgHdrResp.QMUXError) {
            DBG("QMI_WDA_SET_DATA_FORMAT_REQ QMUXResult=%d, QMUXError=%d\n",
                le16toh(pRsp->MUXMsg.QMUXMsgHdrResp.QMUXResult), le16toh(pRsp->MUXMsg.QMUXMsgHdrResp.QMUXError));
            goto qmi_proxy_init_fail;
        }

        pFormat = (PQMI_WDA_SET_DATA_FORMAT_TLV)qmi_find_tlv(pRsp, 0x11);
        if (pFormat)
            DBG("link_prot %d\n", le32toh(pFormat->Value));
        pFormat = (PQMI_WDA_SET_DATA_FORMAT_TLV)qmi_find_tlv(pRsp, 0x12);
        if (pFormat)
            DBG("ul_data_aggregation_protocol %d\n", le32toh(pFormat->Value));
        pFormat = (PQMI_WDA_SET_DATA_FORMAT_TLV)qmi_find_tlv(pRsp, 0x13);
        if (pFormat)
            DBG("dl_data_aggregation_protocol %d\n", le32toh(pFormat->Value));
        pFormat = (PQMI_WDA_SET_DATA_FORMAT_TLV)qmi_find_tlv(pRsp, 0x15);
        if (pFormat)
            DBG("dl_data_aggregation_max_datagrams %d\n", le32toh(pFormat->Value));
        pFormat = (PQMI_WDA_SET_DATA_FORMAT_TLV)qmi_find_tlv(pRsp, 0x16);
        if (pFormat) {
            DBG("dl_data_aggregation_max_size %d\n", le32toh(pFormat->Value));
            rx_urb_size = le32toh(pFormat->Value);
        }
        pFormat = (PQMI_WDA_SET_DATA_FORMAT_TLV)qmi_find_tlv(pRsp, 0x17);
        if (pFormat) {
            qmap_settings.ul_data_aggregation_max_datagrams = MIN(qmap_settings.ul_data_aggregation_max_datagrams, le32toh(pFormat->Value));
            DBG("ul_data_aggregation_max_datagrams %d\n", qmap_settings.ul_data_aggregation_max_datagrams);
        }
        pFormat = (PQMI_WDA_SET_DATA_FORMAT_TLV)qmi_find_tlv(pRsp, 0x18);
        if (pFormat) {
            qmap_settings.ul_data_aggregation_max_size = MIN(qmap_settings.ul_data_aggregation_max_size, le32toh(pFormat->Value));
            DBG("ul_data_aggregation_max_size %d\n", qmap_settings.ul_data_aggregation_max_size);
        }

        if (qmap_settings.ul_data_aggregation_max_datagrams > 1) {
            set_driver_ul_params_setting(ifname, &qmap_settings);
        }
    }
    free(pRsp);

    DBG("%s finished, rx_urb_size is %u\n", __func__, rx_urb_size);
    return 0;

qmi_proxy_init_fail:
    DBG("%s failed\n", __func__);
    return -1;
}

static void qmi_start_server(const char* servername) {
    qmi_proxy_server_fd = create_local_server(servername);
    printf("%s: qmi_proxy_server_fd = %d\n", __func__, qmi_proxy_server_fd);
    if (qmi_proxy_server_fd == -1) {
        DBG("%s Failed to create %s, errno: %d (%s)\n", __func__, "qmi-proxy", errno, strerror(errno));
    }
}

static void qmi_close_server(const char* servername) {
    if (qmi_proxy_server_fd != -1) {
        DBG("%s %s close server\n", __func__, servername);
        close(qmi_proxy_server_fd);
        qmi_proxy_server_fd = -1;
    }
}

static void *qmi_proxy_loop(void *param)
{
    (void)param;
    static uint8_t qmi_buf[2048];
    PQCQMIMSG pQMI = (PQCQMIMSG)qmi_buf;
    struct qlistnode *con_node;
    QMI_PROXY_CONNECTION *qmi_con;

    DBG("%s enter thread_id %p\n", __func__, (void *)pthread_self());

    qlist_init(&qmi_proxy_connection);
    qlist_init(&qmi_proxy_ctl_msg);

    while (cdc_wdm_fd > 0 && qmi_proxy_quit == 0) {
        struct pollfd pollfds[2+64];
        int ne, ret, nevents = 0;
        ssize_t nreads;

        pollfds[nevents].fd = cdc_wdm_fd;
        pollfds[nevents].events = POLLIN;
        pollfds[nevents].revents= 0;
        nevents++;

        if (qmi_proxy_server_fd > 0) {
            pollfds[nevents].fd = qmi_proxy_server_fd;
            pollfds[nevents].events = POLLIN;
            pollfds[nevents].revents= 0;
            nevents++;
        }

        qlist_for_each(con_node, &qmi_proxy_connection) {
            qmi_con = qnode_to_item(con_node, QMI_PROXY_CONNECTION, qnode);

            pollfds[nevents].fd = qmi_con->ClientFd;
            pollfds[nevents].events = POLLIN;
            pollfds[nevents].revents= 0;
            nevents++;

            if (nevents == (sizeof(pollfds)/sizeof(pollfds[0])))
                break;
        }

#if 0
        DBG("poll ");
        for (ne = 0; ne < nevents; ne++) {
            DBG("%d ", pollfds[ne].fd);
        }
        DBG("\n");
#endif

        do {
            //ret = poll(pollfds, nevents, -1);
            ret = poll(pollfds, nevents, (qmi_proxy_server_fd > 0) ? -1 : 200);
         } while (ret < 0 && errno == EINTR && qmi_proxy_quit == 0);

        if (ret < 0) {
            DBG("%s poll=%d, errno: %d (%s)\n", __func__, ret, errno, strerror(errno));
            goto qmi_proxy_loop_exit;
        }

        for (ne = 0; ne < nevents; ne++) {
            int fd = pollfds[ne].fd;
            short revents = pollfds[ne].revents;

            if (revents & (POLLERR | POLLHUP | POLLNVAL)) {
                DBG("%s poll fd = %d, revents = %04x\n", __func__, fd, revents);
                if (fd == cdc_wdm_fd) {
                    goto qmi_proxy_loop_exit;
                } else if(fd == qmi_proxy_server_fd) {

                } else {
                    cleanup_qmi_connection(fd);
                }

                continue;
            }

            if (!(pollfds[ne].revents & POLLIN)) {
                continue;
            }

            if (fd == qmi_proxy_server_fd) {
                accept_qmi_connection(fd);
            }
            else if (fd == cdc_wdm_fd) {
                nreads = read(fd, pQMI, sizeof(qmi_buf));
                if (verbose_debug)
                {
                    ssize_t i;
                    printf("r %d %zd: ", fd, nreads);
                    for (i = 0; i < nreads; i++)
                        printf("%02x ", ((uint8_t *)pQMI)[i]);
                    printf("\n");
                }
                if (nreads <= 0) {
                    DBG("%s read=%d errno: %d (%s)\n",  __func__, (int)nreads, errno, strerror(errno));
                    goto qmi_proxy_loop_exit;
                }

                if (nreads != (le16toh(pQMI->QMIHdr.Length) + 1)) {
                    DBG("%s nreads=%d,  pQCQMI->QMIHdr.Length = %d\n",  __func__, (int)nreads, le16toh(pQMI->QMIHdr.Length));
                    continue;
                }

                recv_qmi(pQMI, nreads);
                if (modem_reset_flag)
                    goto qmi_proxy_loop_exit;
            }
            else {
                nreads = read(fd, pQMI, sizeof(qmi_buf));
                if (verbose_debug)
                {
                    ssize_t i;
                    printf("r %d %zd: ", fd, nreads);
                    for (i = 0; i < 16; i++)
                        printf("%02x ", ((uint8_t *)pQMI)[i]);
                    printf("\n");
                }
                if (nreads <= 0) {
                    DBG("%s read=%d errno: %d (%s)",  __func__, (int)nreads, errno, strerror(errno));
                    cleanup_qmi_connection(fd);
                    break;
                }

                if (nreads != (le16toh(pQMI->QMIHdr.Length) + 1)) {
                    DBG("%s nreads=%d,  pQCQMI->QMIHdr.Length = %d\n",  __func__, (int)nreads, le16toh(pQMI->QMIHdr.Length));
                    continue;
                }

                send_qmi(pQMI, nreads, fd);
            }
        }
    }

qmi_proxy_loop_exit:
    while (!qlist_empty(&qmi_proxy_connection)) {
        QMI_PROXY_CONNECTION *qmi_con = qnode_to_item(qlist_head(&qmi_proxy_connection), QMI_PROXY_CONNECTION, qnode);

        cleanup_qmi_connection(qmi_con->ClientFd);
    }

    DBG("%s exit, thread_id %p\n", __func__, (void *)pthread_self());

    return NULL;
}

static int dir_get_child(const char *dirname, char *buff, unsigned bufsize)
{
    struct dirent *entptr = NULL;
    DIR *dirptr = opendir(dirname);
    if (!dirptr)
        goto error;
    while ((entptr = readdir(dirptr))) {
        if (entptr->d_name[0] == '.')
            continue;
        snprintf(buff, bufsize, "%s", entptr->d_name);
        break;
    }

    closedir(dirptr);
    return 0;
error:
    buff[0] = '\0';
    if (dirptr) closedir(dirptr);
    return -1;
}

static int mhidevice_detect(char *device_name, char *ifname) {
    if (!access("/sys/class/net/pcie_mhi0", F_OK))
        strcpy(ifname, "pcie_mhi0");
    else if (!access("/sys/class/net/rmnet_mhi0", F_OK))
        strcpy(ifname, "rmnet_mhi0");
    else {
        goto error;
    }

    if (!access("/dev/mhi_QMI0", F_OK)) {
        strcpy(device_name, "/dev/mhi_QMI0");
    }
    else {
        goto error;
    }

    return 0;
error:
    return -1;
}

static int qmidevice_detect(char *device_name, char *ifname) {
    struct dirent* ent = NULL;
    DIR *pDir;

    char dir[255] = "/sys/bus/usb/devices";
    pDir = opendir(dir);
    if (pDir)  {
        struct {
            char subdir[255 * 3];
            char qmifile[255];
            char ifname[255];
        } *pl;
        char qmidevice[255] = {'\0'};

        pl = (typeof(pl)) malloc(sizeof(*pl));
        memset(pl, 0x00, sizeof(*pl));

        while ((ent = readdir(pDir)) != NULL)  {
            char idVendor[4+1] = {0};
            char idProduct[4+1] = {0};
            int fd = 0;

            memset(pl, 0x00, sizeof(*pl));
            snprintf(pl->subdir, sizeof(pl->subdir), "%s/%s/idVendor", dir, ent->d_name);
            fd = open(pl->subdir, O_RDONLY);
            if (fd > 0) {
                read(fd, idVendor, 4);
                close(fd);
             }

            snprintf(pl->subdir, sizeof(pl->subdir), "%s/%s/idProduct", dir, ent->d_name);
            fd  = open(pl->subdir, O_RDONLY);
            if (fd > 0) {
                read(fd, idProduct, 4);
                close(fd);
            }

            if (strncasecmp(idVendor, "05c6", 4) && strncasecmp(idVendor, "1e0e", 4))
                continue;

            DBG("Find %s/%s idVendor=%s idProduct=%s\n", dir, ent->d_name, idVendor, idProduct);
            snprintf(pl->subdir, sizeof(pl->subdir), "%s/%s:1.5/usbmisc", dir, ent->d_name);
            if (access(pl->subdir, R_OK)) {
                snprintf(pl->subdir, sizeof(pl->subdir), "%s/%s:1.5/usb", dir, ent->d_name);
                if (access(pl->subdir, R_OK)) {
                    DBG("no GobiQMI/usbmic/usb found in %s/%s:1.5\n", dir, ent->d_name);
                    continue;
                }
            }

            dir_get_child(pl->subdir, pl->qmifile, sizeof(pl->qmifile));
            snprintf(qmidevice, sizeof(qmidevice), "/dev/%.*s", 100, pl->qmifile);
            strcpy(device_name, qmidevice);

            snprintf(pl->subdir, sizeof(pl->subdir), "%s/%s:1.5/net", dir, ent->d_name);
            dir_get_child(pl->subdir, pl->ifname, sizeof(pl->ifname));
            strcpy(ifname, pl->ifname);
        }
        closedir(pDir);
        free(pl);
        if (device_name[0] == '\0' || ifname[0] == '\0') {
            goto error;
        }
        return 0;
    }

error:
    return -1;
}

static void usage(void) {
    DBG(" -d <device_name>                      A valid qmi device\n"
            "                                       default /dev/cdc-wdm0, but cdc-wdm0 may be invalid\n"
            " -i <netcard_name>                     netcard name\n"
            " -v                                    Will show all details\n");
}

static void sig_action(int sig) {
    if (qmi_proxy_quit == 0) {
        qmi_proxy_quit = 1;
        if (thread_id)
            pthread_kill(thread_id, sig);
    }
}

int main(int argc, char *argv[]) {
    int opt;
    char cdc_wdm[32+1] = {'\0'};
    char ifname[32+1] = {'\0'};
    int retry_times = 0;
	char servername[64] = {0};

    optind = 1;

    signal(SIGINT, sig_action);

    while ( -1 != (opt = getopt(argc, argv, "d:i:vh"))) {
        switch (opt) {
            case 'd':
                strcpy(cdc_wdm, optarg);
                break;
            case 'v':
                verbose_debug = 1;
                break;
            case 'i':
                strcpy(ifname, optarg);
                break;
            default:
                usage();
                return 0;
        }
    }

    if (cdc_wdm[0] == '\0' || ifname[0] == '\0') {
        if(qmidevice_detect(cdc_wdm, ifname) && mhidevice_detect(cdc_wdm, ifname)) {
            DBG("network interface '%s' or qmidev '%s' is not exist\n", ifname, cdc_wdm);
            return -1;
        }

    }

   if (cdc_wdm[0] == '\0' || ifname[0] == '\0') {
        DBG("network interface '%s' or qmidev '%s' is not exist\n", ifname, cdc_wdm);
        return -1;
   }

    sprintf(servername, "qmi-proxy%d", cdc_wdm[strlen(cdc_wdm)-1]-'0');

    if (access(cdc_wdm, R_OK | W_OK)) {
        DBG("Fail to access %s, errno: %d (%s). break\n", cdc_wdm, errno, strerror(errno));
        return -1;
    }

    while (qmi_proxy_quit == 0) {
        if (access(cdc_wdm, R_OK | W_OK)) {
            DBG("Fail to access %s, errno: %d (%s). continue\n", cdc_wdm, errno, strerror(errno));
            // wait device
            sleep(3);
            continue;
        }

        DBG("Will use cdc-wdm %s\n", cdc_wdm);

        cdc_wdm_fd = open(cdc_wdm, O_RDWR | O_NONBLOCK | O_NOCTTY);
        if (cdc_wdm_fd == -1) {
            DBG("Failed to open %s, errno: %d (%s). break\n", cdc_wdm, errno, strerror(errno));
            return -1;
        }
        cfmakenoblock(cdc_wdm_fd);

        /* no qmi_proxy_loop lives, create one */
        pthread_create(&thread_id, NULL, qmi_proxy_loop, NULL);
        /* try to redo init if failed, init function must be successfully */
        while (qmi_proxy_init(cdc_wdm, ifname) != 0) {
            if (retry_times < 5) {
                DBG("fail to init proxy, try again in 2 seconds.\n");
                sleep(2);
                retry_times++;
            } else {
                DBG("has failed too much times, restart the modem and have a try...\n");
                break;
            }
            /* break loop if modem is detached */
            if (access(cdc_wdm, F_OK|R_OK|W_OK))
                break;
        }
        retry_times = 0;
        qmi_start_server(servername);
        pthread_join(thread_id, NULL);

        /* close local server at last */
        qmi_close_server(servername);
        close(cdc_wdm_fd);
        /* DO RESTART IN 20s IF MODEM RESET ITSELF */
        if (modem_reset_flag) {
            unsigned int time_to_wait = 20;
            while (time_to_wait) {
                time_to_wait = sleep(time_to_wait);
            }
        }
    }

    return 0;
}
