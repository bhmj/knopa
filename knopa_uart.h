#ifndef _KNOPA_UART_H
#define _KNOPA_UART_H

#include "kmem.h"
#include "os_type.h"
#include "osapi.h"

#define UART0 0
#define UART1 1

typedef enum {
    EMPTY,
    UNDER_WRITE,
    WRITE_OVER
} RcvMsgBuffState;



typedef enum {
    BAUD_RATE_300 = 300,
    BAUD_RATE_600 = 600,
    BAUD_RATE_1200 = 1200,
    BAUD_RATE_2400 = 2400,
    BAUD_RATE_4800 = 4800,
    BAUD_RATE_9600   = 9600,
    BAUD_RATE_19200  = 19200,
    BAUD_RATE_38400  = 38400,
    BAUD_RATE_57600  = 57600,
    BAUD_RATE_74880  = 74880,
    BAUD_RATE_115200 = 115200,
    BAUD_RATE_230400 = 230400,
    BAUD_RATE_460800 = 460800,
    BAUD_RATE_921600 = 921600,
    BAUD_RATE_1843200 = 1843200,
    BAUD_RATE_3686400 = 3686400,
} UartBaudRate;

typedef enum {
    FIVE_BITS = 0x0,
    SIX_BITS = 0x1,
    SEVEN_BITS = 0x2,
    EIGHT_BITS = 0x3
} UartBitsNum4Char;

typedef enum {
    STICK_PARITY_DIS   = 0,
    STICK_PARITY_EN    = 1
} UartExistParity;

typedef enum {
    NONE_BITS = 0x2,
    ODD_BITS   = 1,
    EVEN_BITS = 0
} UartParityMode;

typedef enum {
    ONE_STOP_BIT             = 0x1,
    ONE_HALF_STOP_BIT        = 0x2,
    TWO_STOP_BIT             = 0x3
} UartStopBitsNum;

typedef enum {
    NONE_CTRL,
    HARDWARE_CTRL,
    XON_XOFF_CTRL
} UartFlowCtrl;

typedef struct {
    uint32     RcvBuffSize;
    uint8     *pRcvMsgBuff;
    uint8     *pWritePos;
    uint8     *pReadPos;
    uint8      TrigLvl; //JLU: may need to pad
    RcvMsgBuffState  BuffState;
} RcvMsgBuff;

typedef struct {
    uint32   TrxBuffSize;
    uint8   *pTrxBuff;
} TrxMsgBuff;

typedef enum {
    BAUD_RATE_DET,
    WAIT_SYNC_FRM,
    SRCH_MSG_HEAD,
    RCV_MSG_BODY,
    RCV_ESC_CHAR,
} RcvMsgState;

typedef struct {
    UartBaudRate      baud_rate;
    UartBitsNum4Char  data_bits;
    UartExistParity   exist_parity;
    UartParityMode    parity;
    UartStopBitsNum   stop_bits;
    UartFlowCtrl      flow_ctrl;
    RcvMsgBuff        rcv_buff;
    TrxMsgBuff        trx_buff;
    RcvMsgState       rcv_state;
    int               received;
    int               buff_uart_no;  //indicate which uart use tx/rx buffer
} UartDevice;

// ---------------------------------------------------------------------------

#define at_recvTaskPrio        0
#define at_recvTaskQueueLen    64


extern UartDevice UartDev;

LOCAL struct UartBuffer* pTxBuffer = NULL;
LOCAL struct UartBuffer* pRxBuffer = NULL;

#define uart_recvTaskPrio         0
#define uart_recvTaskQueueLen    10
os_event_t uart_recvTaskQueue[uart_recvTaskQueueLen];


// -- User fns ---------------------------------------------------------------

void FLASH_CODE init_uart (UartBaudRate uart0_br, UartBaudRate uart1_br, uint8 uart_out);

#endif