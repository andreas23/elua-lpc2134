// Platform-dependent functions

#include "platform.h"
#include "type.h"
#include "devman.h"
#include "genstd.h"
#include "stacks.h"
#include <reent.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include "91x_scu.h"
#include "91x_fmi.h"
#include "91x_gpio.h"
#include "91x_uart.h"
#include "91x_tim.h"
#include "common.h"
#include "platform_conf.h"
#include "91x_vic.h"
#include "lrotable.h"
#include "91x_i2c.h"
#include "91x_wiu.h"
#include "buf.h"
#include "elua_adc.h"
#include "91x_adc.h"

// ****************************************************************************
// Platform initialization
const GPIO_TypeDef* port_data[] = { GPIO0, GPIO1, GPIO2, GPIO3, GPIO4, GPIO5, GPIO6, GPIO7, GPIO8, GPIO9 };
static const TIM_TypeDef* timer_data[] = { TIM0, TIM1, TIM2, TIM3 };

static void platform_setup_adcs();

static void platform_config_scu()
{     
  volatile u16 i = 0xFFFF;
  while (i-- > 0);  
  
   // SCU initialization
  SCU_MCLKSourceConfig(SCU_MCLK_OSC);
  SCU_PLLFactorsConfig(192,25,2);            /* PLL = 96 MHz */
  SCU_PLLCmd(ENABLE);                        /* PLL Enabled  */
  SCU_MCLKSourceConfig(SCU_MCLK_PLL);        /* MCLK = PLL   */  
  
  SCU_PFQBCCmd( ENABLE );

  /* Set the RCLK Clock divider to max speed*/
  SCU_RCLKDivisorConfig(SCU_RCLK_Div1);
  /* Set the PCLK Clock to MCLK/2 */
  SCU_PCLKDivisorConfig(SCU_PCLK_Div2);
  /* Set the HCLK Clock to MCLK */
  SCU_HCLKDivisorConfig(SCU_HCLK_Div1);
  
  // Enable VIC clock
  SCU_AHBPeriphClockConfig(__VIC, ENABLE);
  SCU_AHBPeriphReset(__VIC, DISABLE);
                 
  // Enable the UART clocks
  SCU_APBPeriphClockConfig(__UART_ALL, ENABLE);

  // Enable the timer clocks
  SCU_APBPeriphClockConfig(__TIM01, ENABLE);
  SCU_APBPeriphReset(__TIM01, DISABLE);
  SCU_APBPeriphClockConfig(__TIM23, ENABLE);
  SCU_APBPeriphReset(__TIM23, DISABLE);

  // Enable the GPIO clocks  
  SCU_APBPeriphClockConfig(__GPIO_ALL, ENABLE);  

  // Enable the WIU clock
  SCU_APBPeriphClockConfig(__WIU, ENABLE);
  SCU_APBPeriphReset(__WIU, DISABLE);

  // Enable the I2C clocks
  SCU_APBPeriphClockConfig(__I2C0, ENABLE);
  SCU_APBPeriphReset(__I2C0, DISABLE);
  SCU_APBPeriphClockConfig(__I2C1, ENABLE);
  SCU_APBPeriphReset(__I2C1, DISABLE);
  
  // Enable the ADC clocks
  SCU_APBPeriphClockConfig(__ADC, ENABLE);
}

// Port/pin definitions of the eLua UART connection for different boards
#define UART_RX_IDX   0
#define UART_TX_IDX   1

#ifdef ELUA_BOARD_STRE912
static const GPIO_TypeDef* uart_port_data[] = { GPIO5, GPIO5 };
static const u8 uart_pin_data[] = { GPIO_Pin_1, GPIO_Pin_0 };
#else // STR9-comStick
static const GPIO_TypeDef* uart_port_data[] = { GPIO3, GPIO3 };
static const u8 uart_pin_data[] = { GPIO_Pin_2, GPIO_Pin_3 };
#endif

// Dummy interrupt handlers avoid spurious interrupts (AN2593)
static void dummy_int_handler()
{
  VIC0->VAR = 0xFF;
  VIC1->VAR = 0xFF;
}  

// Plaform specific GPIO UART setup
static void platform_gpio_uart_setup()
{
  GPIO_InitTypeDef GPIO_InitStructure;

  GPIO_StructInit( &GPIO_InitStructure );
  // RX
  GPIO_InitStructure.GPIO_Direction = GPIO_PinInput;
  GPIO_InitStructure.GPIO_Pin = uart_pin_data[ UART_RX_IDX ]; 
  GPIO_InitStructure.GPIO_Type = GPIO_Type_PushPull ;
  GPIO_InitStructure.GPIO_IPConnected = GPIO_IPConnected_Enable;
  GPIO_InitStructure.GPIO_Alternate = GPIO_InputAlt1  ;
  GPIO_Init( ( GPIO_TypeDef* )uart_port_data[ UART_RX_IDX ], &GPIO_InitStructure );
  // TX
  GPIO_InitStructure.GPIO_Pin = uart_pin_data[ UART_TX_IDX ];
  GPIO_InitStructure.GPIO_Alternate = GPIO_OutputAlt3  ;
  GPIO_Init( ( GPIO_TypeDef* )uart_port_data[ UART_TX_IDX ], &GPIO_InitStructure );
}

int platform_init()
{
  unsigned i;
  TIM_InitTypeDef tim;
  TIM_TypeDef* base;  
        
  // System configuration
  platform_config_scu();
  
  // PIO setup
  for( i = 0; i < 10; i ++ )
    GPIO_DeInit( ( GPIO_TypeDef* )port_data[ i ] );
    
  // Initialize VIC
  VIC_DeInit();
  VIC0->DVAR = ( u32 )dummy_int_handler;
  VIC1->DVAR = ( u32 )dummy_int_handler;

  // Enablue WIU
  WIU_DeInit();
  
  // Initialize all external interrupts
  VIC_Config( EXTIT0_ITLine, VIC_IRQ, 1 );
  VIC_Config( EXTIT1_ITLine, VIC_IRQ, 2 );
  VIC_Config( EXTIT2_ITLine, VIC_IRQ, 3 );
  VIC_Config( EXTIT3_ITLine, VIC_IRQ, 4 );
  VIC_ITCmd( EXTIT0_ITLine, ENABLE );
  VIC_ITCmd( EXTIT1_ITLine, ENABLE );
  VIC_ITCmd( EXTIT2_ITLine, ENABLE );
  VIC_ITCmd( EXTIT3_ITLine, ENABLE );
  // Enable interrupt generation on WIU
  WIU->CTRL |= 2; 

  // UART setup
  platform_gpio_uart_setup();
  platform_uart_setup( CON_UART_ID, CON_UART_SPEED, 8, PLATFORM_UART_PARITY_NONE, PLATFORM_UART_STOPBITS_1 );

  // Initialize timers
  for( i = 0; i < 4; i ++ )
  {
    base = ( TIM_TypeDef* )timer_data[ i ];
    TIM_DeInit( base );
    TIM_StructInit( &tim );
    tim.TIM_Clock_Source = TIM_CLK_APB;
    tim.TIM_Prescaler = 255;      
    TIM_Init( base, &tim );    
    TIM_CounterCmd( base, TIM_START );
  }
  
#ifdef BUILD_ADC
  // Setup ADCs
  platform_setup_adcs();
#endif
  
  cmn_platform_init();

  return PLATFORM_OK;
}

// ****************************************************************************
// PIO functions

pio_type platform_pio_op( unsigned port, pio_type pinmask, int op )
{
  GPIO_TypeDef* base = ( GPIO_TypeDef* )port_data[ port ];
  GPIO_InitTypeDef data;
  pio_type retval = 1;
  
  GPIO_StructInit( &data );
  switch( op )
  {
    case PLATFORM_IO_PORT_SET_VALUE:    
      GPIO_Write( base, ( u8 )pinmask );
      break;
      
    case PLATFORM_IO_PIN_SET:
      GPIO_WriteBit( base, ( u8 )pinmask, Bit_SET );
      break;
      
    case PLATFORM_IO_PIN_CLEAR:
      GPIO_WriteBit( base, ( u8 )pinmask, Bit_RESET );
      break;
      
    case PLATFORM_IO_PORT_DIR_OUTPUT:
      pinmask = 0xFF;     
    case PLATFORM_IO_PIN_DIR_OUTPUT:
      data.GPIO_Direction = GPIO_PinOutput;
      data.GPIO_Type = GPIO_Type_PushPull ;
      data.GPIO_Alternate=GPIO_OutputAlt1;
      data.GPIO_Pin = ( u8 )pinmask;
      GPIO_Init(base, &data);
      break;
      
    case PLATFORM_IO_PORT_DIR_INPUT:
      pinmask = 0xFF;     
    case PLATFORM_IO_PIN_DIR_INPUT:
      data.GPIO_Pin = ( u8 )pinmask;
      GPIO_Init(base, &data);
      break;    
            
    case PLATFORM_IO_PORT_GET_VALUE:
      retval = GPIO_Read( base );
      break;
      
    case PLATFORM_IO_PIN_GET:
      retval = GPIO_ReadBit( base, ( u8 )pinmask );
      break;
      
    default:
      retval = 0;
      break;
  }
  return retval;
}

// ****************************************************************************
// UART

static const UART_TypeDef* uarts[] = { UART0, UART1, UART2 };

u32 platform_uart_setup( unsigned id, u32 baud, int databits, int parity, int stopbits )
{
  UART_InitTypeDef UART_InitStructure;
  UART_TypeDef* p_uart = ( UART_TypeDef* )uarts[ id ];
    
  // Then configure UART parameters
  switch( databits )
  {
    case 5:
      UART_InitStructure.UART_WordLength = UART_WordLength_5D;
      break;      
    case 6:
      UART_InitStructure.UART_WordLength = UART_WordLength_6D;
      break;      
    case 7:
      UART_InitStructure.UART_WordLength = UART_WordLength_7D;
      break;      
    case 8:
      UART_InitStructure.UART_WordLength = UART_WordLength_8D;
      break;
  }
  if( stopbits == PLATFORM_UART_STOPBITS_1 )
    UART_InitStructure.UART_StopBits = UART_StopBits_1;    
  else
    UART_InitStructure.UART_StopBits = UART_StopBits_2;
  if( parity == PLATFORM_UART_PARITY_EVEN )
    UART_InitStructure.UART_Parity = UART_Parity_Even;
  else if( parity == PLATFORM_UART_PARITY_ODD )
    UART_InitStructure.UART_Parity = UART_Parity_Odd;
  else
    UART_InitStructure.UART_Parity = UART_Parity_No;
  UART_InitStructure.UART_BaudRate = baud;
  UART_InitStructure.UART_HardwareFlowControl = UART_HardwareFlowControl_None;
  UART_InitStructure.UART_Mode = UART_Mode_Tx_Rx;
  UART_InitStructure.UART_FIFO = UART_FIFO_Enable;
  UART_InitStructure.UART_TxFIFOLevel = UART_FIFOLevel_1_2; /* FIFO size 16 bytes, FIFO level 8 bytes */
  UART_InitStructure.UART_RxFIFOLevel = UART_FIFOLevel_1_2; /* FIFO size 16 bytes, FIFO level 8 bytes */

  UART_DeInit( p_uart );
  UART_Init( p_uart , &UART_InitStructure );
  UART_Cmd( p_uart, ENABLE );
  
  return baud;
}

void platform_uart_send( unsigned id, u8 data )
{
  UART_TypeDef* p_uart = ( UART_TypeDef* )uarts[ id ];

//  while( UART_GetFlagStatus( STR9_UART, UART_FLAG_TxFIFOFull ) == SET );
  UART_SendData( p_uart, data );
  while( UART_GetFlagStatus( p_uart, UART_FLAG_TxFIFOFull ) != RESET );  
}

int platform_s_uart_recv( unsigned id, s32 timeout )
{
  UART_TypeDef* p_uart = ( UART_TypeDef* )uarts[ id ];

  if( timeout == 0 )
  {
    // Return data only if already available
    if( UART_GetFlagStatus( p_uart, UART_FLAG_RxFIFOEmpty ) != SET )
      return UART_ReceiveData( p_uart );
    else
      return -1;
  }
  while( UART_GetFlagStatus( p_uart, UART_FLAG_RxFIFOEmpty ) == SET );
  return UART_ReceiveData( p_uart ); 
}

// ****************************************************************************
// Timer

// Helper: get timer clock
static u32 platform_timer_get_clock( unsigned id )
{
  return ( SCU_GetPCLKFreqValue() * 1000 ) / ( TIM_GetPrescalerValue( ( TIM_TypeDef* )timer_data[ id ] ) + 1 );
}

// Helper: set timer clock
static u32 platform_timer_set_clock( unsigned id, u32 clock )
{
  u32 baseclk = SCU_GetPCLKFreqValue() * 1000;
  TIM_TypeDef* base = ( TIM_TypeDef* )timer_data[ id ];      
  u64 bestdiv;
  
  bestdiv = ( ( u64 )baseclk << 16 ) / clock;
  if( bestdiv & 0x8000 )
    bestdiv += 0x10000;
  bestdiv >>= 16;
  if( bestdiv > 256 )
    bestdiv = 256;
  TIM_PrescalerConfig( base, bestdiv - 1 );
  return baseclk / bestdiv;
}

void platform_s_timer_delay( unsigned id, u32 delay_us )
{
  TIM_TypeDef* base = ( TIM_TypeDef* )timer_data[ id ];  
  u32 freq;
  timer_data_type final;
  
  freq = platform_timer_get_clock( id );
  final = ( ( u64 )delay_us * freq ) / 1000000;
  if( final > 2 )
    final -= 2;
  else
    final = 0;
  if( final > 0xFFFF )
    final = 0xFFFF;
  TIM_CounterCmd( base, TIM_STOP );
  TIM_CounterCmd( base, TIM_CLEAR );  
  TIM_CounterCmd( base, TIM_START );  
  while( TIM_GetCounterValue( base ) >= 0xFFFC );
  while( TIM_GetCounterValue( base ) < final );  
}
      
u32 platform_s_timer_op( unsigned id, int op, u32 data )
{
  u32 res = 0;
  TIM_TypeDef* base = ( TIM_TypeDef* )timer_data[ id ];  

  switch( op )
  {
    case PLATFORM_TIMER_OP_START:
      TIM_CounterCmd( base, TIM_STOP );
      TIM_CounterCmd( base, TIM_CLEAR );  
      TIM_CounterCmd( base, TIM_START );  
      while( TIM_GetCounterValue( base ) >= 0xFFFC );        
      break;
      
    case PLATFORM_TIMER_OP_READ:
      res = TIM_GetCounterValue( base );
      break;
      
    case PLATFORM_TIMER_OP_GET_MAX_DELAY:
      res = platform_timer_get_diff_us( id, 0, 0xFFFF );
      break;
      
    case PLATFORM_TIMER_OP_GET_MIN_DELAY:
      res = platform_timer_get_diff_us( id, 0, 1 );
      break;      
      
    case PLATFORM_TIMER_OP_SET_CLOCK:
      res = platform_timer_set_clock( id, data );
      break;
      
    case PLATFORM_TIMER_OP_GET_CLOCK:
      res = platform_timer_get_clock( id );
      break;
  }
  return res;
}


// *****************************************************************************
// ADC specific functions and variables

#ifdef BUILD_ADC

ADC_InitTypeDef ADC_InitStructure;

int platform_adc_check_timer_id( unsigned id, unsigned timer_id )
{
  return 0; // This platform does not support direct timer triggering
}

void platform_adc_stop( unsigned id )
{  
  elua_adc_ch_state *s = adc_get_ch_state( id );
  elua_adc_dev_state *d = adc_get_dev_state( 0 );
  
  s->op_pending = 0;
  INACTIVATE_CHANNEL( d, id );

  // If there are no more active channels, stop the sequencer
  if( d->ch_active == 0 )
    d->running = 0;
}


void ADC_IRQHandler( void )
{
  elua_adc_dev_state *d = adc_get_dev_state( 0 );
  elua_adc_ch_state *s;

  if ( ADC_GetFlagStatus( ADC_FLAG_ECV ) )
  {
    d->seq_ctr = 0;
    while( d->seq_ctr < d->seq_len )
    {
      s = d->ch_state[ d->seq_ctr ];
      d->sample_buf[ d->seq_ctr ] = ( u16 )ADC_GetConversionValue( s->id );
      s->value_fresh = 1;
    
      // Fill in smoothing buffer until warmed up
      if ( s->logsmoothlen > 0 && s->smooth_ready == 0)
	adc_smooth_data( s->id );
#if defined( BUF_ENABLE_ADC )
      else if ( s->reqsamples > 1 )
      {
	buf_write( BUF_ID_ADC, s->id, ( t_buf_data* )s->value_ptr );
	s->value_fresh = 0;
      }
#endif

      // If we have the number of requested samples, stop sampling
      if ( adc_samples_available( s->id ) >= s->reqsamples && s->freerunning == 0 )
	platform_adc_stop( s->id );

      d->seq_ctr++;
    }
    d->seq_ctr = 0;
    ADC_ClearFlag( ADC_FLAG_ECV );
  }
  
  if( d->running == 1 )
    adc_update_dev_sequence( 0 );

  if ( d->clocked == 0 && d->running == 1 )
  {
    ADC_ConversionCmd( ADC_Conversion_Start );
  }

  VIC0->VAR = 0xFF;
}

static void platform_setup_adcs()
{
  unsigned id;
  
  for( id = 0; id < NUM_ADC; id ++ )
    adc_init_ch_state( id );
  
  VIC_Config(ADC_ITLine, VIC_IRQ, 0);
  VIC_ITCmd(ADC_ITLine, ENABLE);
  
  ADC_StructInit(&ADC_InitStructure);

  /* Configure the ADC  structure in continuous mode conversion */
  ADC_DeInit();             /* ADC Deinitialization */
  ADC_InitStructure.ADC_Channel_0_Mode = ADC_NoThreshold_Conversion;
  ADC_InitStructure.ADC_Scan_Mode = ENABLE;
  ADC_InitStructure.ADC_Conversion_Mode = ADC_Single_Mode;
  
  ADC_Cmd( ENABLE );
  ADC_PrescalerConfig( 0x2 );
  ADC_Init( &ADC_InitStructure );

  ADC_ITConfig(ADC_IT_ECV, ENABLE);

  platform_adc_setclock( 0, 0 );
}


// NOTE: On this platform, there is only one ADC, clock settings apply to the whole device
u32 platform_adc_setclock( unsigned id, u32 frequency )
{
  elua_adc_dev_state *d = adc_get_dev_state( 0 );
  
  // No clocked conversions supported
  d->clocked = 0;

  return 0;
}

const int adc_gpio_chan[] = { GPIO_ANAChannel0, GPIO_ANAChannel1, GPIO_ANAChannel2, GPIO_ANAChannel3, GPIO_ANAChannel4, GPIO_ANAChannel5, GPIO_ANAChannel6, GPIO_ANAChannel7 };

// Prepare Hardware Channel
int platform_adc_update_sequence( )
{ 
  elua_adc_dev_state *d = adc_get_dev_state( 0 );
    
  ADC_Cmd( DISABLE );
  ADC_DeInit();
  
  ADC_InitStructure.ADC_Channel_0_Mode = ADC_No_Conversion;
  ADC_InitStructure.ADC_Channel_1_Mode = ADC_No_Conversion;
  ADC_InitStructure.ADC_Channel_2_Mode = ADC_No_Conversion;
  ADC_InitStructure.ADC_Channel_3_Mode = ADC_No_Conversion;
  ADC_InitStructure.ADC_Channel_4_Mode = ADC_No_Conversion;
  ADC_InitStructure.ADC_Channel_5_Mode = ADC_No_Conversion;
  ADC_InitStructure.ADC_Channel_6_Mode = ADC_No_Conversion;
  ADC_InitStructure.ADC_Channel_7_Mode = ADC_No_Conversion;

  d->seq_ctr = 0;
  while( d->seq_ctr < d->seq_len )
  {
    GPIO_ANAPinConfig( adc_gpio_chan[ d->ch_state[ d->seq_ctr ]->id ], ENABLE );

    // This is somewhat terrible, but the API doesn't provide an alternative
    switch( d->ch_state[ d->seq_ctr ]->id )
    {
      case 0:
        ADC_InitStructure.ADC_Channel_0_Mode = ADC_NoThreshold_Conversion;
        break;
      case 1:
        ADC_InitStructure.ADC_Channel_1_Mode = ADC_NoThreshold_Conversion;
        break;
      case 2:
        ADC_InitStructure.ADC_Channel_2_Mode = ADC_NoThreshold_Conversion;
        break;
      case 3:
        ADC_InitStructure.ADC_Channel_3_Mode = ADC_NoThreshold_Conversion;
        break;
      case 4:
        ADC_InitStructure.ADC_Channel_4_Mode = ADC_NoThreshold_Conversion;
        break;
      case 5:
        ADC_InitStructure.ADC_Channel_5_Mode = ADC_NoThreshold_Conversion;
        break;
      case 6:
        ADC_InitStructure.ADC_Channel_6_Mode = ADC_NoThreshold_Conversion;
        break;
      case 7:
        ADC_InitStructure.ADC_Channel_7_Mode = ADC_NoThreshold_Conversion;
        break;
    }
    d->seq_ctr++;
  }
  d->seq_ctr = 0;
  
  ADC_Cmd( ENABLE );
  ADC_PrescalerConfig( 0x2 );
  ADC_Init( &ADC_InitStructure );

  ADC_ITConfig( ADC_IT_ECV, ENABLE );

  return PLATFORM_OK;
}


int platform_adc_start_sequence()
{ 
    elua_adc_dev_state *d = adc_get_dev_state( 0 );

    // Only force update and initiate if we weren't already running
    // changes will get picked up during next interrupt cycle
    if ( d->running != 1 )
    {
      // Bail if we somehow were trying to set up clocked conversion
      if( d->clocked == 1 )
        return PLATFORM_ERR;

      adc_update_dev_sequence( 0 );

      d->running = 1;

      ADC_ClearFlag( ADC_FLAG_ECV );

      ADC_ITConfig( ADC_IT_ECV, ENABLE );

      ADC_ConversionCmd( ADC_Conversion_Start );
    }

    return PLATFORM_OK;
  }


#endif // ifdef BUILD_ADC


// ****************************************************************************
// PWM functions

u32 platform_pwm_setup( unsigned id, u32 frequency, unsigned duty )
{
  TIM_TypeDef* p_timer = ( TIM_TypeDef* )timer_data[ id ];
  u32 base = SCU_GetPCLKFreqValue() * 1000;
  u32 div = ( base / 256 ) / frequency;
  TIM_InitTypeDef tim;

  TIM_DeInit( p_timer );
  tim.TIM_Mode = TIM_PWM;
  tim.TIM_Clock_Source = TIM_CLK_APB;       
  tim.TIM_Prescaler = 0xFF;       
  tim.TIM_Pulse_Level_1 = TIM_HIGH;   
  tim.TIM_Period_Level = TIM_LOW;    
  tim.TIM_Full_Period = div;
  tim.TIM_Pulse_Length_1 = ( div * duty ) / 100;
  TIM_Init( p_timer, &tim );

  return base / div;
}

static u32 platform_pwm_set_clock( unsigned id, u32 clock )
{
  TIM_TypeDef* p_timer = ( TIM_TypeDef* )timer_data[ id ];
  u32 base = ( SCU_GetPCLKFreqValue() * 1000 );
  u32 div = base / clock;

  TIM_PrescalerConfig( p_timer, ( u8 )div - 1 );
  return base / div;
}

u32 platform_pwm_op( unsigned id, int op, u32 data )
{
  u32 res = 0;
  TIM_TypeDef* p_timer = ( TIM_TypeDef* )timer_data[ id ];

  switch( op )
  {
    case PLATFORM_PWM_OP_START:
      TIM_CounterCmd( p_timer, TIM_START );
      break;

    case PLATFORM_PWM_OP_STOP:
      TIM_CounterCmd( p_timer, TIM_STOP );
      break;

    case PLATFORM_PWM_OP_SET_CLOCK:
      res = platform_pwm_set_clock( id, data );
      break;

    case PLATFORM_PWM_OP_GET_CLOCK:
      res = ( SCU_GetPCLKFreqValue() * 1000 ) / ( TIM_GetPrescalerValue( p_timer ) + 1 );
      break;
  }

  return res;
}

// ****************************************************************************
// I2C support
static const GPIO_TypeDef* i2c_port_data[] = { GPIO1, GPIO2 };
static const I2C_TypeDef* i2cs[] = { I2C0, I2C1 };
static const u8 i2c_clock_pin[] = { GPIO_Pin_4, GPIO_Pin_2 };
static const u8 i2c_data_pin[] = { GPIO_Pin_6, GPIO_Pin_3 };

u32 platform_i2c_setup( unsigned id, u32 speed )
{
  GPIO_InitTypeDef GPIO_InitStructure;
  I2C_InitTypeDef I2C_InitStructure;

  // Setup PIO
  GPIO_StructInit( &GPIO_InitStructure );
  GPIO_InitStructure.GPIO_Pin = i2c_clock_pin[ id ] | i2c_data_pin[ id ]; 
  GPIO_InitStructure.GPIO_Type = GPIO_Type_OpenCollector;
  GPIO_InitStructure.GPIO_IPConnected = GPIO_IPConnected_Enable;
  GPIO_InitStructure.GPIO_Alternate = id == 0 ? GPIO_OutputAlt3 : GPIO_OutputAlt2;
  GPIO_Init( ( GPIO_TypeDef* )i2c_port_data[ id ], &GPIO_InitStructure );
 
  // Setup and interface
  I2C_StructInit( &I2C_InitStructure );
  I2C_InitStructure.I2C_GeneralCall = I2C_GeneralCall_Disable;
  I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;
  I2C_InitStructure.I2C_CLKSpeed = speed;
  I2C_InitStructure.I2C_OwnAddress = 0XA0 + id; // dummy, shouldn't matter
  I2C_Init( ( I2C_TypeDef* )i2cs[ id ], &I2C_InitStructure );

  // Return actual speed
  return speed;
}

void platform_i2c_send_start( unsigned id )
{
  I2C_TypeDef *pi2c = ( I2C_TypeDef* )i2cs[ id ];

  //while( I2C_GetFlagStatus( pi2c, I2C_FLAG_BUSY ) );
  I2C_GenerateStart( pi2c, ENABLE );
  while( I2C_CheckEvent( pi2c, I2C_EVENT_MASTER_MODE_SELECT ) != SUCCESS );
}

void platform_i2c_send_stop( unsigned id )
{
  I2C_TypeDef *pi2c = ( I2C_TypeDef* )i2cs[ id ];

  I2C_GenerateSTOP( pi2c, ENABLE );
  while( I2C_GetFlagStatus( pi2c, I2C_FLAG_BUSY ) );
}

int platform_i2c_send_address( unsigned id, u16 address, int direction )
{
  I2C_TypeDef *pi2c = ( I2C_TypeDef* )i2cs[ id ];
  u16 flags;

  I2C_Send7bitAddress( pi2c, address, direction == PLATFORM_I2C_DIRECTION_TRANSMITTER ? I2C_MODE_TRANSMITTER : I2C_MODE_RECEIVER );
  while( 1 )
  {
    flags = I2C_GetLastEvent( pi2c );
    if( flags & I2C_FLAG_AF )
      return 0;
    if( flags == I2C_EVENT_MASTER_MODE_SELECTED )
      break;
  }
  I2C_ClearFlag( pi2c, I2C_FLAG_ENDAD );
  if( direction == PLATFORM_I2C_DIRECTION_TRANSMITTER )
  {
    while( I2C_CheckEvent( pi2c, I2C_EVENT_MASTER_BYTE_TRANSMITTED ) != SUCCESS );
    I2C_ClearFlag( pi2c, I2C_FLAG_BTF );
  }
  return 1;
}

int platform_i2c_send_byte( unsigned id, u8 data )
{
  I2C_TypeDef *pi2c = ( I2C_TypeDef* )i2cs[ id ];
  u16 flags;

  I2C_SendData( pi2c, data ); 
  while( 1 )
  {
    flags = I2C_GetLastEvent( pi2c );
    if( flags & I2C_FLAG_AF )
      return 0;
    if( flags == I2C_EVENT_MASTER_BYTE_TRANSMITTED )
      break;
  }
  I2C_ClearFlag( pi2c, I2C_FLAG_BTF );
  return 1;
}

int platform_i2c_recv_byte( unsigned id, int ack )
{
  I2C_TypeDef *pi2c = ( I2C_TypeDef* )i2cs[ id ];

  I2C_AcknowledgeConfig( pi2c, ack ? ENABLE : DISABLE );
  if( !ack )
    I2C_GenerateSTOP( pi2c, ENABLE );
  while( I2C_CheckEvent( pi2c, I2C_EVENT_MASTER_BYTE_RECEIVED ) != SUCCESS );
  return I2C_ReceiveData( pi2c );
}

// ****************************************************************************
// EXTINT handlers and support functions

static const u8 exint_group_to_gpio[] = { 3, 5, 6, 7 };

// Convert an EXINT source number to a GPIO ID
static u16 exint_src_to_gpio( u32 exint )
{
  return PLATFORM_IO_ENCODE( exint_group_to_gpio[ exint >> 3 ], exint & 0x07, 0 ); 
}

// Convert a GPIO ID to a EXINT number
static int exint_gpio_to_src( pio_type piodata )
{
  u16 port = PLATFORM_IO_GET_PORT( piodata );
  u16 pin = PLATFORM_IO_GET_PIN( piodata );
  unsigned i;

  for( i = 0; i < sizeof( exint_group_to_gpio ) / sizeof( u8 ); i ++ )
    if( exint_group_to_gpio[ i ] == port )
      break;
  // Restrictions: only the specified port(s) have interrupt capabilities
  //               for port 0 (GPIO3), only pins 2..7 have interrupt capabilities
  if( ( i == sizeof( exint_group_to_gpio ) / sizeof( u8 ) ) || ( ( i == 0 ) && ( pin < 2 ) ) )
    return -1;
  return ( i << 3 ) + pin;
}

// External interrupt handlers
static void exint_irq_handler( int group )
{
  u32 bmask;
  u32 pr = WIU->PR;
  u32 mr = WIU->MR;
  u32 tr = WIU->TR;
  u32 shift = group << 3;
  unsigned i;

  // Check interrupt mask
  if( ( ( pr >> shift ) & 0xFF ) == 0 )
  {
    VIC1->VAR = 0xFF;
    return;
  }

  // Iterate through all the bits in the mask, queueing interrupts as needed
  for( i = 0, bmask = 1 << shift; i < 8; i ++, bmask <<= 1 )
    if( ( pr & bmask ) && ( mr & bmask ) )
    {
      // Enqueue interrupt
      if( tr & bmask )
        elua_int_add( INT_GPIO_POSEDGE, exint_src_to_gpio( shift + i ) );
      else
        elua_int_add( INT_GPIO_NEGEDGE, exint_src_to_gpio( shift + i ) );
      // Then clear it
      WIU->PR  = bmask;
    }

  // Clear interrupt source
  VIC1->VAR = 0xFF;
}

void EXTIT0_IRQHandler()
{
  exint_irq_handler( 0 );
}

void EXTIT1_IRQHandler()
{
  exint_irq_handler( 1 );
}

void EXTIT2_IRQHandler()
{
  exint_irq_handler( 2 );
}

void EXTIT3_IRQHandler()
{
  exint_irq_handler( 3 );
}

// ****************************************************************************
// CPU functions

// Helper: return the status of a specific interrupt (enabled/disabled)
static int platform_cpuh_get_int_status( elua_int_id id, elua_int_resnum resnum )
{
  int temp;
  u32 mask;
  
  if( id == INT_GPIO_POSEDGE || id == INT_GPIO_NEGEDGE )
  {
    if( ( temp = exint_gpio_to_src( resnum ) ) == -1 )
    {
      fprintf( stderr, "Error: not a valid source for an external interrupt\n" );
      return 0;
    }
    mask = 1 << temp;
    if( WIU->MR & mask )
    {
      if( id == INT_GPIO_POSEDGE )
        return ( WIU->TR & mask ) != 0;
      else
        return ( WIU->TR & mask ) == 0;
    }
    else
      return 0;
  } 
  else
    fprintf( stderr, "Error: %d not a valid interrupt ID\n", id );
  return 0;
}

int platform_cpu_set_interrupt( elua_int_id id, elua_int_resnum resnum, int status )
{
  int crt_status = platform_cpuh_get_int_status( id, resnum );
  int temp;
  u32 mask;
  
  if( id == INT_GPIO_POSEDGE || id == INT_GPIO_NEGEDGE )
  {
    if( ( temp = exint_gpio_to_src( resnum ) ) == -1 )
    {
      fprintf( stderr, "Error: not a valid source for an external interrupt\n" );
      return 0;
    }
    mask = 1 << temp;
    if( status == PLATFORM_CPU_ENABLE )
    {
      // Set edge type
      if( id == INT_GPIO_POSEDGE )
        WIU->TR |= mask;
      else
        WIU->TR &= ~mask;
      // Clear interrupt flag?
      // WIU->PR = mask;
      // Enable interrupt
      WIU->MR |= mask;
    }     
    else
      WIU->MR &= ~mask; 
  }
  else
    fprintf( stderr, "Error: %d not a valid interrupt ID\n", id );
  return crt_status;
}

int platform_cpu_get_interrupt( elua_int_id id, elua_int_resnum resnum )
{
  return platform_cpuh_get_int_status( id, resnum );
}

// ****************************************************************************
// Platform specific modules go here

#define MIN_OPT_LEVEL 2
#include "lrodefs.h"
extern const LUA_REG_TYPE str9_pio_map[];

const LUA_REG_TYPE platform_map[] =
{
#if LUA_OPTIMIZE_MEMORY > 0
  { LSTRKEY( "pio" ), LROVAL( str9_pio_map ) },
#endif
  { LNILKEY, LNILVAL }
};

LUALIB_API int luaopen_platform( lua_State *L )
{
#if LUA_OPTIMIZE_MEMORY > 0
  return 0;
#else // #if LUA_OPTIMIZE_MEMORY > 0
  luaL_register( L, PS_LIB_TABLE_NAME, platform_map );

  // Setup the new tables inside platform table
  lua_newtable( L );
  luaL_register( L, NULL, str9_pio_map );
  lua_setfield( L, -2, "pio" );

  return 1;
#endif // #if LUA_OPTIMIZE_MEMORY > 0
}

