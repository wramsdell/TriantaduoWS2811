#include <FlexIO_t4.h>
#include "TDWS2811.h"
#define FLEXMODULE 0
#define NUMLEDS 1000
uint32_t serialInterruptEnable;
DMAChannel dmaChannel;
DMASetting dmaSetting[4];
uint8_t activeBuffer=0;
uint32_t frameBuffer[2][24*NUMLEDS]={{0xFFFFFFFF,0,0xFFFFFFFF,0,0xFFFFFFFF,0,0xFFFFFFFF,0,0xFFFFFFFF},{0,0xFFFFFFFF,0,0xFFFFFFFF,0,0xFFFFFFFF,0,0xFFFFFFFF,0}};
volatile const uint32_t zeros[40]={0};
volatile const uint32_t ones[2]={0xFFFFFFFF,0xFFFFFFFF};

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("Start Setup");
  
  FlexIOHandler *pFlex = FlexIOHandler::flexIOHandler_list[FLEXMODULE];

  configurePins();
  configurePll(pFlex);
  configureFlexIO(pFlex);
  configureDma(pFlex);
  Serial.printf("Setup complete!");
}

void loop() {
  // put your main code here, to run repeatedly:
  digitalWrite(2, !digitalRead(2));
  digitalWrite(13, !digitalRead(13));
  flipBuffers();
  delay(500);

}

void shifterIsr(void) {
  digitalWriteFast(0, HIGH);
  int i=0;
  for (i=0;i<1000;i++) __asm__ __volatile__ ("nop\n\t");
  digitalWriteFast(0, LOW);
  IMXRT_FLEXIO1_S.SHIFTBUFBIS[0]+=1;
  IMXRT_FLEXIO1_S.SHIFTBUFBIS[0]-=1;}

void dmaIsr(void) {
  digitalWriteFast(1, HIGH);
  int i=0;
  for (i=0;i<1000;i++) __asm__ __volatile__ ("nop\n\t");
  digitalWriteFast(1, LOW);
  dmaSetting[1].TCD->CSR &= ~DMA_TCD_CSR_INTMAJOR;  //Have to do this because DMASetting doesn't have a "disable interrupt" function
  if (activeBuffer==0) {
    activeBuffer=1;
    dmaSetting[0].sourceBuffer(frameBuffer[1],sizeof(frameBuffer[1]));
  }
  else {
    activeBuffer=0;
    dmaSetting[0].sourceBuffer(frameBuffer[0],sizeof(frameBuffer[0]));
  }
  dmaChannel.clearInterrupt();
  for (i=0;i<10;i++) __asm__ __volatile__ ("nop\n\t");  //Some race condition between clearInterrupt() and the return of the ISR.  If we don't delay here, the ISR will fire again.
}

void configurePins(void) {
  pinMode(0, OUTPUT);
  pinMode(1, OUTPUT);
  pinMode(2, OUTPUT);
  pinMode(3, OUTPUT);
  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);
  pinMode(6, OUTPUT);
  pinMode(7, OUTPUT);
  pinMode(8, OUTPUT);
  pinMode(13, OUTPUT);
}
void configureFlexIO(FlexIOHandler *pFlex) {
  IMXRT_FLEXIO_t *p = &pFlex->port();
  const FlexIOHandler::FLEXIO_Hardware_t *hw = &pFlex->hardware();
  
  uint8_t res;
  pFlex->mapIOPinToFlexPin(2);
  pFlex->mapIOPinToFlexPin(3);
  pFlex->mapIOPinToFlexPin(4);
  pFlex->mapIOPinToFlexPin(5);
  pFlex->setIOPinToFlexMode(2);
  pFlex->setIOPinToFlexMode(3);
  pFlex->setIOPinToFlexMode(4);
  pFlex->setIOPinToFlexMode(5);

  hw->clock_gate_register |= hw->clock_gate_mask;
                            
  Serial.printf("0x%08X\n",p->PARAM);
  p->CTRL = FLEXIO_CTRL_FLEXEN;

  p->SHIFTCTL[0] = 0x00830602;
  p->SHIFTCTL[1] = 0x00800002;
  p->SHIFTCTL[2] = 0x00800002;
  p->SHIFTCTL[3] = 0x00800002;
  p->SHIFTCTL[4] = 0x00800002;

  p->SHIFTCFG[0] = 0x00000100;
  p->SHIFTCFG[1] = 0x00000100;
  p->SHIFTCFG[2] = 0x00000100;
  p->SHIFTCFG[3] = 0x00000100;
  p->SHIFTCFG[4] = 0x00000100;

  p->TIMCFG[0] =   0x00000200;
  p->TIMCFG[1] =   0x00000100;

  p->TIMCTL[0] =   0x01C30401;
  p->TIMCTL[1] =   0x00030503;

  p->TIMCMP[0] =   0x0000FF00;
  p->TIMCMP[1] =   0x0000001F;

  p->SHIFTBUF[0] = 0xFFFFFFFF;
//  p->SHIFTBUFBIS[1] = 0x00000081;
  p->SHIFTBUFBIS[1] = 0xAAAAAAAA;
  p->SHIFTBUF[2] = 0x00000000;
  p->SHIFTBUF[3] = 0x00000000;
  p->SHIFTBUF[4] = 0x0000000F;

}

void configurePll (FlexIOHandler *pFlex) {
  //Set up PLL5 (also known as "PLL_VIDEO" and "PLL_528"), connect to FlexIO1
  const FlexIOHandler::FLEXIO_Hardware_t *hw = &pFlex->hardware();
  uint32_t pllVideo;
  /* Disable Video PLL output before initial Video PLL. */
  CCM_ANALOG_PLL_VIDEO &= ~CCM_ANALOG_PLL_VIDEO_ENABLE_MASK;
  /* Bypass PLL first */
  CCM_ANALOG_PLL_VIDEO = (CCM_ANALOG_PLL_VIDEO & (~CCM_ANALOG_PLL_VIDEO_BYPASS_CLK_SRC_MASK)) |
                          CCM_ANALOG_PLL_VIDEO_BYPASS_MASK | CCM_ANALOG_PLL_VIDEO_BYPASS_CLK_SRC(0);
  CCM_ANALOG_PLL_VIDEO_NUM = CCM_ANALOG_PLL_VIDEO_NUM_A(8);
  CCM_ANALOG_PLL_VIDEO_DENOM = CCM_ANALOG_PLL_VIDEO_DENOM_B(12);
  pllVideo = (CCM_ANALOG_PLL_VIDEO & (~(CCM_ANALOG_PLL_VIDEO_DIV_SELECT_MASK | CCM_ANALOG_PLL_VIDEO_POWERDOWN_MASK))) |
             CCM_ANALOG_PLL_VIDEO_ENABLE_MASK |CCM_ANALOG_PLL_VIDEO_DIV_SELECT(42);
//    Uncomment the following for full speed
  pllVideo |= CCM_ANALOG_PLL_VIDEO_POST_DIV_SELECT(2);
//    Uncomment the following for 1/4 speed
//  pllVideo |= CCM_ANALOG_PLL_VIDEO_POST_DIV_SELECT(4);
  CCM_ANALOG_MISC2 = (CCM_ANALOG_MISC2 & (~CCM_ANALOG_MISC2_VIDEO_DIV_MASK)) | CCM_ANALOG_MISC2_VIDEO_DIV(2);
  CCM_ANALOG_PLL_VIDEO = pllVideo;
  while ((CCM_ANALOG_PLL_VIDEO & CCM_ANALOG_PLL_VIDEO_LOCK_MASK) == 0)
  {
  }
  /* Disable bypass for Video PLL. */
  CCM_ANALOG_PLL_VIDEO &= ~(uint32_t(CCM_ANALOG_PLL_VIDEO_BYPASS_MASK));
  Serial.printf("0x%08X\n",CCM_ANALOG_PLL_VIDEO_NUM);
  Serial.printf("0x%08X\n",CCM_ANALOG_PLL_VIDEO_DENOM);
  Serial.printf("0x%08X\n",CCM_ANALOG_PLL_VIDEO);

  //Disable FlexIO clock gate
  hw->clock_gate_register &= ~(uint32_t(hw->clock_gate_mask));
  /* Set FLEXIO1_CLK_PRED. */
  CCM_CDCDR = (CCM_CDCDR & ~(CCM_CDCDR_FLEXIO1_CLK_SEL(3) | CCM_CDCDR_FLEXIO1_CLK_PRED(7) | CCM_CDCDR_FLEXIO1_CLK_PODF(7))) 
//    Uncomment the following for 1/8 speed
//        | CCM_CDCDR_FLEXIO1_CLK_SEL(2) | CCM_CDCDR_FLEXIO1_CLK_PRED(4) | CCM_CDCDR_FLEXIO1_CLK_PODF(7);
//    Uncomment the following for full speed
      | CCM_CDCDR_FLEXIO1_CLK_SEL(2) | CCM_CDCDR_FLEXIO1_CLK_PRED(4) | CCM_CDCDR_FLEXIO1_CLK_PODF(0);
}

void configureInterrupts(FlexIOHandler *pFlex) {
//Set up interrupts
  IMXRT_FLEXIO_t *p = &pFlex->port();
  const FlexIOHandler::FLEXIO_Hardware_t *hw = &pFlex->hardware();
  attachInterruptVector(hw->flex_irq, shifterIsr);
  NVIC_ENABLE_IRQ(hw->flex_irq);
  p->SHIFTSIEN |= 0X00000001;
}

void configureDma(FlexIOHandler *pFlex) {
  /* A whole bunch of stuff to get DMA working.  Also need to set the applicable bit of the
   *  Shifter Status DMA Enable (SHIFTSDEN) register, see page 3051 of the Reference Manual
   *  FlexIO1 is on DMA Mux channel 0 (use DMAMUX_SOURCE_FLEXIO1_REQUEST0)
   *  
   *  IMXRT_DMA_TCD_t Looks relevant
   */
  IMXRT_FLEXIO_t *p = &pFlex->port();
  const FlexIOHandler::FLEXIO_Hardware_t *hw = &pFlex->hardware();
  p->SHIFTSDEN |= 0X00000002;

  dmaSetting[0].sourceBuffer(frameBuffer[0],sizeof(frameBuffer[0]));
  dmaSetting[0].destination(p->SHIFTBUFBIS[1]);
  dmaSetting[0].replaceSettingsOnCompletion(dmaSetting[1]);

  dmaSetting[1].sourceBuffer(zeros,sizeof(zeros));
  dmaSetting[1].destination(p->SHIFTBUF[0]);
  dmaSetting[1].replaceSettingsOnCompletion(dmaSetting[2]);

  dmaSetting[2].sourceBuffer(zeros,sizeof(zeros));
  dmaSetting[2].destination(p->SHIFTBUF[1]);
  dmaSetting[2].replaceSettingsOnCompletion(dmaSetting[3]);

  dmaSetting[3].sourceBuffer(ones,sizeof(ones));
  dmaSetting[3].destination(p->SHIFTBUF[0]);
  dmaSetting[3].replaceSettingsOnCompletion(dmaSetting[0]);

  dmaChannel.disable();
  dmaChannel=dmaSetting[0];
  dmaChannel.triggerAtHardwareEvent(hw->shifters_dma_channel[1]);
  dmaChannel.attachInterrupt(dmaIsr);
  
  dumpDMA_TCD(&dmaChannel);
  dmaChannel.enable();
}

void flipBuffers(void){
  dmaSetting[1].TCD->CSR |= DMA_TCD_CSR_INTMAJOR;
}

void dumpDMA_TCD(DMAChannel *dma)
{
  Serial.printf("DMA object's base address = %x\nDMA TCD base address = %x\n", (uint32_t)dma, (uint32_t)dma->TCD);

  Serial.printf("SA: 0x%08x\nSO: %d\nAT: 0x%04x\nNB: 0x%08x\nSL: %d\nDA: 0x%08x\nDO: %d\nCI: %d\nDL: 0x%08x\nCS: 0x%08x\nBI: 0x%08x\n", (uint32_t)dma->TCD->SADDR,
    dma->TCD->SOFF, dma->TCD->ATTR, dma->TCD->NBYTES, dma->TCD->SLAST, (uint32_t)dma->TCD->DADDR, 
    dma->TCD->DOFF, dma->TCD->CITER, dma->TCD->DLASTSGA, dma->TCD->CSR, dma->TCD->BITER);
}
