
#include <LPC17xx.h>
#include "lpc17xx_gpio.h"
#include "lpc17xx_adc.h"
#include "lpc17xx_dac.h"
#include "lpc17xx_gpdma.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_timer.h"
#include "tono_alarma.h"

#define DMA_SIZE 25424
#define NUM_SINE_SAMPLE 25424
#define SINE_FREQ_IN_HZ 2000
#define PCLK_DAC_IN_MHZ 25	//CCLK divided by 4


#define SBIT_WordLenght    0x00u
#define SBIT_DLAB          0x07u
#define SBIT_FIFO          0x00u
#define SBIT_RxFIFO        0x01u
#define SBIT_TxFIFO        0x02u

#define SBIT_RDR           0x00u
#define SBIT_THRE          0x05u

uint8_t tono[NUM_SINE_SAMPLE];
uint16_t tempMed = 0;


void confADC(){
	ADC_Init(LPC_ADC, 200000); //ADC a frecuencia maxima
	//ADC_IntConfig(LPC_ADC, ADC_ADGINTEN, DISABLE); // interrupciones individuales de los canales
	//ADC_IntConfig(LPC_ADC, ADC_ADINTEN0, ENABLE); // habilito la interrupcion por AD0.0
	ADC_ChannelCmd(LPC_ADC, ADC_CHANNEL_0, ENABLE); // habilito el canal 0
	//ADC_BurstCmd(LPC_ADC, ENABLE); // inicio cuando ocurre un match en el MAT0.1
	//NVIC_EnableIRQ(ADC_IRQn);
}

void confPIN(){
	PINSEL_CFG_Type pinCfg;
	pinCfg.Portnum = PINSEL_PORT_0;	// configuro el Pin 23 del puerto 0
	pinCfg.Pinnum = PINSEL_PIN_23;
	pinCfg.Funcnum = PINSEL_FUNC_1;	//Funcion 1 = ADC0.0
	pinCfg.Pinmode = PINSEL_PINMODE_TRISTATE; // Modo de Tristate ya que es ADC
	pinCfg.OpenDrain = PINSEL_PINMODE_NORMAL;	// Sin OpenDrain
	PINSEL_ConfigPin(&pinCfg);

	pinCfg.Portnum = PINSEL_PORT_1;	// configuro el Pin 29 del puerto 1
	pinCfg.Pinnum = PINSEL_PIN_29;
	pinCfg.Funcnum = PINSEL_FUNC_3;	//Funcion 3 = MAT0.1
	pinCfg.Pinmode = PINSEL_PINMODE_TRISTATE; // Modo de Tristate ya que es ADC
	pinCfg.OpenDrain = PINSEL_PINMODE_NORMAL;
	PINSEL_ConfigPin(&pinCfg);

	pinCfg.Funcnum = 2;
	pinCfg.OpenDrain = 0;
	pinCfg.Pinmode = 0;
	pinCfg.Pinnum = 26;
	pinCfg.Portnum = 0;
	PINSEL_ConfigPin(&pinCfg);
}

void confTIMER(){
	TIM_Cmd(LPC_TIM0, ENABLE);

	TIM_TIMERCFG_Type timerCfg;
	timerCfg.PrescaleOption = TIM_PRESCALE_USVAL; // Timer cuenta microsegundos
	timerCfg.PrescaleValue = 200; // Cuenta cada 250 milisegundos (250000 microseg)
	TIM_Init(LPC_TIM0, TIM_TIMER_MODE, &timerCfg);

	TIM_MATCHCFG_Type matchCfg;
	matchCfg.MatchChannel = 1;	//canal de match MAT0.1
	matchCfg.IntOnMatch = ENABLE; // interrumpo con
	matchCfg.StopOnMatch = DISABLE; // sigo contando despues del match
	matchCfg.ResetOnMatch = ENABLE; // reseteo el timer para volver a contar
	matchCfg.ExtMatchOutputType = TIM_EXTMATCH_NOTHING; // no hago nada
	matchCfg.MatchValue = 1; // cuenta 5 veces un segudno y hace match
	TIM_ConfigMatch(LPC_TIM0, &matchCfg);

	NVIC_EnableIRQ(TIMER0_IRQn);
}

void confDMA(){
	GPDMA_LLI_Type DMA_LLI_Struct;
	//Prepare DMA link list time structure
	DMA_LLI_Struct.SrcAddr = (uint32_t)tono;
	DMA_LLI_Struct.DstAddr = (uint32_t)&(LPC_DAC->DACR);
	DMA_LLI_Struct.NextLLI = (uint32_t)&DMA_LLI_Struct;
	DMA_LLI_Struct.Control = DMA_SIZE
			& ~(7<<18) //source width 32 bits
			& ~(7<<21) //dest. widht 32 bits
			& (1<<26);//source increment
	/* GPDMA block section -------------------------
	 * Initialize GPDMA controller */
	GPDMA_Init();
	GPDMA_Channel_CFG_Type GPDMACfg;
	//Setup GPDMA channel
	//channel 0
	GPDMACfg.ChannelNum = 0;
	//Source memory
	GPDMACfg.SrcMemAddr = (uint32_t)(tono);
	//Destination memory - unused because the transfer is to peripherals
	GPDMACfg.DstMemAddr = 0;
	//Transfer size
	GPDMACfg.TransferSize = DMA_SIZE;
	//Transfer type
	GPDMACfg.TransferType = GPDMA_TRANSFERTYPE_M2P;
	//Source connection - unused
	GPDMACfg.SrcConn = 0;
	//Destination connection
	GPDMACfg.DstConn = GPDMA_CONN_DAC;
	//Linker List Item - unused
	GPDMACfg.DMALLI = (uint32_t)&DMA_LLI_Struct;
	//Setup channel with given parameter
	GPDMA_Setup(&GPDMACfg);
	return;
}

void confDAC(){
	uint32_t tmp;
	DAC_CONVERTER_CFG_Type DAC_ConverterConfigStruct;
	DAC_ConverterConfigStruct.CNT_ENA = SET;
	DAC_ConverterConfigStruct.DMA_ENA = SET;

	DAC_Init(LPC_DAC);

	tmp = (PCLK_DAC_IN_MHZ*1000000)/(SINE_FREQ_IN_HZ*NUM_SINE_SAMPLE); //Valor del timer asociado al DAC
	//tmp: tiempo que transcurre entre una muestra y la siguiente. Numero de cuentas del timer, n° adimensional
	DAC_SetDMATimeOut(LPC_DAC, tmp);
	DAC_ConfigDAConverterControl(LPC_DAC, &DAC_ConverterConfigStruct);
	return;
}

void confGPIO(){
	LPC_GPIO0->FIODIR |= 3<<21;
}

/* Function to initialize the UART0 at specifief baud rate */
void uart_init(uint32_t baudrate)
{
    uint32_t var_UartPclk_u32,var_Pclk_u32,var_RegValue_u32;

    LPC_PINCON->PINSEL0 &= ~0x000000F0;
    LPC_PINCON->PINSEL0 |= 0x00000050;            // Enable TxD0 P0.2 and p0.3

    LPC_UART0->FCR = (1<<SBIT_FIFO) | (1<<SBIT_RxFIFO) | (1<<SBIT_TxFIFO); // Enable FIFO and reset Rx/Tx FIFO buffers
    LPC_UART0->LCR = (0x03<<SBIT_WordLenght) | (1<<SBIT_DLAB);             // 8bit data, 1Stop bit, No parity


    /** Baud Rate Calculation :
       PCLKSELx registers contains the PCLK info for all the clock dependent peripherals.
       Bit6,Bit7 contains the Uart Clock(ie.UART_PCLK) information.
       The UART_PCLK and the actual Peripheral Clock(PCLK) is calculated as below.
       (Refer data sheet for more info)
       UART_PCLK    PCLK
         0x00       SystemFreq/4
         0x01       SystemFreq
         0x02       SystemFreq/2
         0x03       SystemFreq/8
     **/

    var_UartPclk_u32 = (LPC_SC->PCLKSEL0 >> 6) & 0x03;

    switch( var_UartPclk_u32 )
    {
    case 0x00:
        var_Pclk_u32 = SystemCoreClock/4;
        break;
    case 0x01:
        var_Pclk_u32 = SystemCoreClock;
        break;
    case 0x02:
        var_Pclk_u32 = SystemCoreClock/2;
        break;
    case 0x03:
        var_Pclk_u32 = SystemCoreClock/8;
        break;
    }


    var_RegValue_u32 = ( var_Pclk_u32 / (16 * baudrate ));
    LPC_UART0->DLL =  var_RegValue_u32 & 0xFF;
    LPC_UART0->DLM = (var_RegValue_u32 >> 0x08) & 0xFF;

    LPC_UART0->LCR &= ~ (1<<7);  // Clear DLAB after setting DLL,DLM
}

/* Function to Receive a char */
char uart_RxChar()
{
    char ch;
    while(!(LPC_UART0->LSR & 1));  // Wait till the data is received
    ch = LPC_UART0->RBR;                                // Read received data
    return ch;
}

int main()
{
	confPIN();
	confADC();
	confTIMER();
	confDAC();
    char ch;

    uart_init(9400);  // Initialize the UART0 for 9600 baud rate

    GPDMA_ChannelCmd(0, ENABLE);

    while(1)
    {
        //Finally receive a char and transmit it infinitely
        ch = uart_RxChar();

        if(ch == '1'){
        			GPIO_SetValue(0, 0x00200000);

        			confDMA();

        		}
        		else if(ch == '2'){
        			GPIO_SetValue(0, 0x00400000);
        		    GPDMA_ChannelCmd(0, DISABLE);
        		}
        		else if(ch == '3'){
        			GPIO_ClearValue(0, 0x00200000);
        		    GPDMA_ChannelCmd(0, DISABLE);

        		}
        		else if(ch == '4'){
        			GPIO_ClearValue(0, 0x00400000);
        		    GPDMA_ChannelCmd(0, DISABLE);

        		}
        		ch = '0';
    }
}

void TIMER0_IRQHandler(){
	ADC_StartCmd(LPC_ADC, ADC_START_NOW);
	while(!LPC_ADC->ADDR0 >> 31){};
	tempMed = ADC_ChannelGetData(LPC_ADC, 0); //guardo el resultado en la variable de temperatura
	TIM_ClearIntPending(LPC_TIM0, TIM_MR1_INT);

}
