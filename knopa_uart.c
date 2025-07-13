#include <c_types.h>
#include "eagle_soc.h"
#include "uart_register.h"
#include "knopa_uart.h"
#include "knopa_dbg.h"

#define RX_BUFF_SIZE    256
#define TX_BUFF_SIZE    100

#define UART0   0
#define UART1   1


uint8 uart_dbg = 0;


// ----------------------------------------------------------------------------

void FLASH_CODE uart_rx_intr_enable(uint8 uart_no)
{
#if 1
    SET_PERI_REG_MASK(UART_INT_ENA(uart_no), UART_RXFIFO_FULL_INT_ENA|UART_RXFIFO_TOUT_INT_ENA);
#else
    ETS_UART_INTR_ENABLE();
#endif
}

// ----------------------------------------------------------------------------

LOCAL void FLASH_CODE uart0_rx_intr_handler(void *para)
{
  /* uart0 and uart1 intr combine togther, when interrupt occur, see reg 0x3ff20020, bit2, bit0 represents
    * uart1 and uart0 respectively
    */
//  RcvMsgBuff *pRxBuff = (RcvMsgBuff *)para;
  uint8 RcvChar;
  uint8 uart_no = UART0;//UartDev.buff_uart_no;

//  if (UART_RXFIFO_FULL_INT_ST != (READ_PERI_REG(UART_INT_ST(uart_no)) & UART_RXFIFO_FULL_INT_ST))
//  {
//    return;
//  }
//  if (UART_RXFIFO_FULL_INT_ST == (READ_PERI_REG(UART_INT_ST(uart_no)) & UART_RXFIFO_FULL_INT_ST))
//  {
////    at_recvTask();
//    RcvChar = READ_PERI_REG(UART_FIFO(uart_no)) & 0xFF;
//    system_os_post(at_recvTaskPrio, NULL, RcvChar);
//    WRITE_PERI_REG(UART_INT_CLR(uart_no), UART_RXFIFO_FULL_INT_CLR);
//  }
  if(UART_FRM_ERR_INT_ST == (READ_PERI_REG(UART_INT_ST(uart_no)) & UART_FRM_ERR_INT_ST))
  {
    dbg_printf(0, "FRM_ERR\n");
    WRITE_PERI_REG(UART_INT_CLR(uart_no), UART_FRM_ERR_INT_CLR);
  }

  if(UART_RXFIFO_FULL_INT_ST == (READ_PERI_REG(UART_INT_ST(uart_no)) & UART_RXFIFO_FULL_INT_ST))
  {
    dbg_printf(0, "fifo full\n");
    ETS_UART_INTR_DISABLE();/////////

    system_os_post(at_recvTaskPrio, 0, 0);

//    WRITE_PERI_REG(UART_INT_CLR(uart_no), UART_RXFIFO_FULL_INT_CLR);
//    while (READ_PERI_REG(UART_STATUS(uart_no)) & (UART_RXFIFO_CNT << UART_RXFIFO_CNT_S))
//    {
////      at_recvTask();
//      RcvChar = READ_PERI_REG(UART_FIFO(uart_no)) & 0xFF;
//      system_os_post(at_recvTaskPrio, NULL, RcvChar);
//    }
  }
  else if(UART_RXFIFO_TOUT_INT_ST == (READ_PERI_REG(UART_INT_ST(uart_no)) & UART_RXFIFO_TOUT_INT_ST))
  {
    ETS_UART_INTR_DISABLE();/////////

//    dbg_printf(2, "stat:%02X",*(uint8 *)UART_INT_ENA(uart_no));
    system_os_post(at_recvTaskPrio, 0, 0);

//    WRITE_PERI_REG(UART_INT_CLR(uart_no), UART_RXFIFO_TOUT_INT_CLR);
////    dbg_printf(2, "rx time over\r\n");
//    while (READ_PERI_REG(UART_STATUS(uart_no)) & (UART_RXFIFO_CNT << UART_RXFIFO_CNT_S))
//    {
////      dbg_printf(2, "process recv\r\n");
////      at_recvTask();
//      RcvChar = READ_PERI_REG(UART_FIFO(uart_no)) & 0xFF;
//      system_os_post(at_recvTaskPrio, NULL, RcvChar);
//    }
  }

//  WRITE_PERI_REG(UART_INT_CLR(uart_no), UART_RXFIFO_FULL_INT_CLR);

//  if (READ_PERI_REG(UART_STATUS(uart_no)) & (UART_RXFIFO_CNT << UART_RXFIFO_CNT_S))
//  {
//    RcvChar = READ_PERI_REG(UART_FIFO(uart_no)) & 0xFF;
//    at_recvTask();
//    *(pRxBuff->pWritePos) = RcvChar;

//    system_os_post(at_recvTaskPrio, NULL, RcvChar);

//    //insert here for get one command line from uart
//    if (RcvChar == '\r')
//    {
//      pRxBuff->BuffState = WRITE_OVER;
//    }
//
//    pRxBuff->pWritePos++;
//
//    if (pRxBuff->pWritePos == (pRxBuff->pRcvMsgBuff + RX_BUFF_SIZE))
//    {
//      // overflow ...we may need more error handle here.
//      pRxBuff->pWritePos = pRxBuff->pRcvMsgBuff ;
//    }
//  }
}

// ----------------------------------------------------------------------------
//
//  Write a single char to specified UART (WITH WAIT)
//
LOCAL STATUS FLASH_CODE uart_tx_one_char(uint8 uart, uint8 TxChar)
{
  while (true) {
    uint32 fifo_cnt = (READ_PERI_REG(UART_STATUS(uart)) >> UART_TXFIFO_CNT_S) & UART_TXFIFO_CNT;
    if (fifo_cnt < 126) break;
    os_delay_us(10);
  }

  WRITE_PERI_REG(UART_FIFO(uart) , TxChar);
  return OK;
}

// ----------------------------------------------------------------------------

LOCAL void FLASH_CODE uart_write_char(char c)
{
  if (c == '\n') {
    uart_tx_one_char(uart_dbg, '\r');
    uart_tx_one_char(uart_dbg, '\n');
  } else if (c != '\r') {
    uart_tx_one_char(uart_dbg, c);
  }
}

// ----------------------------------------------------------------------------
//  Config UART:
//    saet baud rate
//    set input buffer size
//    attach UART0 interrupt handler (for input)
//    clear RX and TX FIFO
//    set RX FIFO trigger for UART0
//
void FLASH_CODE uart_config (uint8 uart_no)
{

  dbg_printf(1, "UART setup...\n");
  if (uart_no == UART1)
  {
    // transmit only
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_U1TXD_BK);
  }
  else
  {
    /* rcv_buff size is 0x100 */
    dbg_printf(2, "UART0 receive buffer size: %u\n", UartDev.rcv_buff.RcvBuffSize);
    ETS_UART_INTR_ATTACH(uart0_rx_intr_handler,  &(UartDev.rcv_buff)); //
    PIN_PULLUP_DIS(PERIPHS_IO_MUX_U0TXD_U);              // not pulled up (zeroed?)
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD); // assign transmit function to U0TXD pin
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDO_U, FUNC_U0RTS);  // assign RTS control to MTDO pin
  }

  uart_div_modify(uart_no, UART_CLK_FREQ / (UartDev.baud_rate));

  // set up UART transfer params
  WRITE_PERI_REG(
    UART_CONF0(uart_no),            // (0x60000000 + (uart_no)*0xf00) + 0x20
    //
    UartDev.exist_parity |                        // parity (y/n)
    UartDev.parity |                              // barity bits
    (UartDev.stop_bits << UART_STOP_BIT_NUM_S) |  // stop bits
    (UartDev.data_bits << UART_BIT_NUM_S)         // data bits
  );

  //clear rx and tx fifo, not ready
  SET_PERI_REG_MASK   (UART_CONF0(uart_no), UART_RXFIFO_RST | UART_TXFIFO_RST);
  CLEAR_PERI_REG_MASK (UART_CONF0(uart_no), UART_RXFIFO_RST | UART_TXFIFO_RST);

  //set rx fifo trigger
//  WRITE_PERI_REG(UART_CONF1(uart_no),
//                 ((UartDev.rcv_buff.TrigLvl & UART_RXFIFO_FULL_THRHD) << UART_RXFIFO_FULL_THRHD_S) |
//                 ((96 & UART_TXFIFO_EMPTY_THRHD) << UART_TXFIFO_EMPTY_THRHD_S) |
//                 UART_RX_FLOW_EN);
  if (uart_no == UART0)
  {
    //set rx fifo trigger
    WRITE_PERI_REG(
      UART_CONF1(uart_no),          // (0x60000000 + (uart_no)*0xf00) + 0x24
      //
      ((0x10 & UART_RXFIFO_FULL_THRHD) << UART_RXFIFO_FULL_THRHD_S) |
      ((0x10 & UART_RX_FLOW_THRHD) << UART_RX_FLOW_THRHD_S) |
      UART_RX_FLOW_EN |
      (0x02 & UART_RX_TOUT_THRHD) << UART_RX_TOUT_THRHD_S |
      UART_RX_TOUT_EN
    );
    SET_PERI_REG_MASK(UART_INT_ENA(uart_no), UART_RXFIFO_TOUT_INT_ENA | UART_FRM_ERR_INT_ENA);
  }
  else
  {
    WRITE_PERI_REG(
      UART_CONF1(uart_no),
      ((UartDev.rcv_buff.TrigLvl & UART_RXFIFO_FULL_THRHD) << UART_RXFIFO_FULL_THRHD_S)
    );
  }

  // DEBUG:
  uint8 tx_fifo_len = (READ_PERI_REG(UART_STATUS(uart_no))>>UART_TXFIFO_CNT_S)&UART_TXFIFO_CNT;
  uint8 rx_fifo_len = (READ_PERI_REG(UART_STATUS(uart_no))>>UART_RXFIFO_CNT_S)&UART_RXFIFO_CNT;
  dbg_printf(2, "UART%u receive FIFO buffer: %u\n", (int)uart_no, (int)rx_fifo_len);
  dbg_printf(2, "UART%u transmit FIFO buffer: %u\n", (int)uart_no, (int)tx_fifo_len);

  //clear all interrupts
  WRITE_PERI_REG(UART_INT_CLR(uart_no), 0xffff);
  //enable RX interrupt
  SET_PERI_REG_MASK(UART_INT_ENA(uart_no), UART_RXFIFO_FULL_INT_ENA);
}

// ----------------------------------------------------------------------------
//
//
LOCAL void FLASH_CODE uart_recvTask(os_event_t *events)
{
    if(events->sig == 0) {
//    #if  UART_BUFF_EN
//        Uart_rx_buff_enq();
//    #else
      uint8 fifo_len = (READ_PERI_REG(UART_STATUS(UART0))>>UART_RXFIFO_CNT_S)&UART_RXFIFO_CNT;
      uint8 d_tmp = 0;
      uint8 idx = 0;
      for (idx=0; idx<fifo_len; idx++) {
        d_tmp = READ_PERI_REG(UART_FIFO(UART0)) & 0xFF; // get char
        uart_tx_one_char(UART0, d_tmp); // echo!
      }
      WRITE_PERI_REG(UART_INT_CLR(UART0), UART_RXFIFO_FULL_INT_CLR|UART_RXFIFO_TOUT_INT_CLR);
      uart_rx_intr_enable(UART0);
//    #endif
    }else if (events->sig == 1) {
//    #if UART_BUFF_EN
   //already move uart buffer output to uart empty interrupt
        //tx_start_uart_buffer(UART0);
//    #else

//    #endif
    }
}

// ----------------------------------------------------------------------------
//  Init UART
//
void FLASH_CODE init_uart (UartBaudRate uart0_br, UartBaudRate uart1_br, uint8 uart_out)
{
  // this is an example to process uart data from task, please change the priority to fit your application task if exists
  system_os_task(uart_recvTask, uart_recvTaskPrio, uart_recvTaskQueue, uart_recvTaskQueueLen);  //demo with a task to process the uart data

  UartDev.baud_rate = uart0_br;
  uart_config(UART0);
  UartDev.baud_rate = uart1_br;
  uart_config(UART1);

  ETS_UART_INTR_ENABLE(); // enable UART interrupts

  // install uart putc callback
//  uart_dbg = uart_out;
//  os_install_putc1((void *)uart_write_char);
}
