/*
 * GccApplication1.c
 *
 * Created: 2019-08-28 오후 12:13:16
 * Author : CDSL
 */ 

#include "mcu_init.h"
#include "dataType.h"


//////////////////////////////////////////////////////////////////////////////////////////////////////////////


volatile int32_t g_Cnt, g_preCnt;

// 위치 제어기 관련 변수
/************************************************************************/
/* 
g_Pdes: 목표 각도 값
g_Pcur: 측정한 각도값

g_Perr: 위치 제어기 오차




                                                                     */
/************************************************************************/
volatile double g_Pdes = 0., g_Ppre; 
volatile double g_Pcur, g_Pvcur;
volatile double g_Perr;

// 속도 제어기 관련 변수
volatile double g_Vcur, g_Vpre;
volatile double g_Vdes = 0.2;
volatile double g_Verr;
volatile double g_Vlimit = 1.;

// 전류 제어기 관련 변수
volatile double g_Ccur;
volatile double g_Cdes;
volatile double g_Cerr;
volatile double g_Cerr_sum;
volatile double g_Climit = 1.;

volatile double g_ADC;
volatile int g_SendFlag = 0;
volatile int g_Direction;

volatile int cur_control = 0;
volatile double g_vel_control;
volatile double g_pos_control;
volatile unsigned char g_TimerCnt;

volatile unsigned char checkSize;
volatile unsigned char g_buf[256], g_BufWriteCnt, g_BufReadCnt;

volatile Packet_t g_PacketBuffer;
volatile unsigned char g_PacketMode;
volatile unsigned char g_ID = 1;

// 1번 타이머 ,모터 관련 부분
void SetDutyCW(double v){
	
	while(TCNT1  == 0);

	int ocr = v * (200. / 24.) + 200; // v는 전압을 의미
	
	if(ocr > OCR_MAX)	ocr = OCR_MAX;
	else if(ocr < OCR_MIN)	ocr = OCR_MIN;
	//OCR1A = OCR1B = ocr;
	
	OCR1A = OCR3B = ocr + 8;		//1 H
	OCR1B = OCR3A = ocr - 8;		//1 L
}


void InitLS7366(){
	
	PORTB = 0x00;
	SPI_MasterSend(SELECT_MDR0 | WR_REG);
	SPI_MasterSend(X4_QUAD | FREE_RUN | DISABLE_INDEX | SYNCHRONOUS_INDEX |FILTER_CDF_1);
	PORTB = 0x01;
	
	PORTB = 0x00;
	SPI_MasterSend(SELECT_MDR1 | WR_REG);
	SPI_MasterSend(FOUR_BYTE_COUNT_MODE | ENABLE_COUNTING);
	PORTB = 0x01;
	
	PORTB = 0x00;
	SPI_MasterSend(SELECT_CNTR | CLR_REG);
	PORTB = 0x01;
}



int getADC(char ch){

	ADMUX = (ADMUX & 0xf0) + ch;
	ADCSRA |= 0x40;
	while(!(ADCSRA & 0x10));
	return ADC;
}




ISR(USART0_RX_vect){

	g_buf[g_BufWriteCnt++] = UDR0;
}

/************************************************************************/
/*
 
타이머 오버 플로우 인터럽트:
내부적으로 TCNT가 존재하여 크리스탈이 만들어내는 신호에서 카운팅을 하는데
이 카운팅값의 상한을 넘게되면 오버플로우 인터럽트가 발생한다.
따라서 특정 주기마다 어떠한 동작이 이루어지도록 하기위해 이 인터럽트를 사용하는데,
8bit MCU의 경우 TCNT값을 256까지 셀 수 있고
prescaler 분주비를 이용하여 제어주기를 설정한다.
즉, 클럭이 내보내는 신호에서 분주비에 따라 클럭 신호의 주기가 길어진다.
기본적으로 8MHz의 신호를 내보내므로 주기는 1/(8*10^6)이고
1024로 설정하게 되면 t = 1024 / (8*10^6) 이 된다.
따라서 원하는 제어주기가 예를 들어 10ms(0.01s) 이면 10ms / t 가 TCNT가 세어야하는
카운팅 값이 된다. 계산해보면 78.125가 나오고 이는 78을 카운트해야하므로 256 - 78로 설정해두면
78번 카운트하여 256이 될때 인터럽트가 발생한다.

                                                                     */
/************************************************************************/

/************************************************************************/
/* 
t = 64/16M = 0.000004
5ms / 0.000004 = 1250 번 cnt 

                                                                     */
/************************************************************************/




//PWM 제어
ISR(TIMER3_OVF_vect){
	


	
}



//ISR(TIMER3_OVF_vect){
// 제어기 구현,  TIMER는 먼저 1ms로 돌아감 //0.001048의 제어주기
// prescaler = 64, 16Mhz mcu -> 64/16,000,000 = 0.000004 -> 0.0005(5ms)/0.000004 = 125번
// 전류 제어기의 주기는 200Hz -> 5ms 이므로 OVF는 2000Hz -> 0.5ms 
ISR(TIMER0_OVF_vect){
	
	//256 - 39?
	TCNT0 = 256 - 125; //256 - 125 -> 정상임 125번하면 OVF는 0.5ms의 주기를 가짐
	//TCNT3 = 65536 - 125;		
	//Read LS7366
	int32_t cnt;
	
	PORTC = 0x01;
	
	g_ADC = getADC(0);
	
	PORTB = 0x00;
	SPI_MasterSend(SELECT_OTR | LOAD_REG);
	PORTB = 0x01;
			
	PORTB = 0x00;
	SPI_MasterSend(SELECT_OTR | RD_REG);
	cnt = SPI_MasterRecv();		cnt = cnt<< 8;
	cnt |= SPI_MasterRecv();	cnt = cnt<< 8;
	cnt |= SPI_MasterRecv();	cnt = cnt<< 8;
	cnt |= SPI_MasterRecv();
	PORTB = 0x01;
	g_Cnt = -cnt;
	
	PORTC = 0x03;

	//각도 측정	
	g_Pcur = (g_Cnt / (4096. * 81.)) * 2 * M_PI; //현재 측정한 각도값 출력, Rad 단위


	
	
	//TO DO: 아마도 TIMER의 주기일듯
	// 100번 카운트 될때마다 초기화 - 1ms마다 1번 카운트됨 100*1ms = 0.1s
	if((g_TimerCnt % 100) == 0){
		
		g_TimerCnt = 0;
		// 속도 제어기 연산
		
	}
	//제어기의 연산 주기와 관련있음, 현재 OVF 주기는 0.5ms이므로 0.5ms * 10 = 5ms
	if((g_TimerCnt % 10) == 0){
		
		g_Vcur = (g_Pcur - g_Pvcur) / 0.005; //현재 위치와 이전 위치를 빼고 dt를 나눠서 모터 속도값 계산, 제어주기가 0.005 yes
		g_Pvcur = g_Pcur;
	}
	
	g_TimerCnt++;
	
	// 목표 Current값인데 왜 (-)?, 
	g_Cdes = -0.1;
	
	//확인하려면 데이터 시트 확인 
	// 모든 변수를 써야 정확

	//전류 측정	
	g_Ccur = -( ((g_ADC / 1024. * 5.) - 2.5) * 10.); //현재 측정한 전류값
	g_Cerr = g_Cdes - g_Ccur; //전류 오차

	// 0.1: K_p, 1.5: K_i 제어기 게인은 실제 시스템에 따라서 바뀔 수 있는 부분이라고 함
	cur_control = g_Cerr * 0.1 + g_Cerr_sum * 1.5; //현재 입력값
	cur_control += g_Vcur * 0.0683; //0.0683은 토크 상수, g_Vcur은 측정한 모터 속도값
	
	g_Cerr_sum += g_Cerr;
	
	// Kp_c(전류 제어기 p gain): 0.8268428 = 0.827
	// Kp_i: 2,211.616
	// Kp_a: 1.0 / Kp_c = 1.0 / 0.827
	// g_Cerr_sum은 오차를 적분한 결과에 해당
	// g_Cerr 자체는 전류 오차를 의미
	//I-term anti - Q1) 입력 전류가 24V여서 (I게인은 각 제어기의 제어주기를 곱해서 사용)
	// 전류제어기의 제어주기는 0.005 초(5ms)에 해당 - 1000/5 = 200Hz -> TIMER OVF INTERRUPT -> 2000Hz -> 0.5ms
	if(cur_control > 24){
		cur_control = 24;
		g_Cerr_sum -= (cur_control - 24) * 1. / 0.0827 / 3.;
	}
	else if(cur_control < -24){
		cur_control = -24;
		g_Cerr_sum -= (cur_control + 24) * 1. / 0.0827 / 3.;
	}
	
	//위치 제어기의 전압은 cur_control
	SetDutyCW(cur_control);
	
	/////////////////////////////////////////
	
	g_SendFlag++;

}



int main(void){
	
	Packet_t packet;
	packet.data.header[0] = packet.data.header[1] = packet.data.header[2] = packet.data.header[3] = 0xFE;
	
	InitIO();
	
	//Uart
	InitUart0();
	
	//SPI
	InitSPI();
	
	//Timer
	InitTimer0(); 
	InitTimer1();
	InitTimer3();


	TCNT1 = TCNT3 = 0;
	SetDutyCW(0.);
	
	//ADC
	InitADC();
	
	//LS7366
	InitLS7366();
	
	//TCNT3 = 65536 - 125;
	TCNT0 = 256 - 125;
	sei();

	unsigned char check = 0;
	
    while (1) {
		for(;g_BufReadCnt != g_BufWriteCnt; g_BufReadCnt++){
			
			switch(g_PacketMode){
			case 0:
				
				if (g_buf[g_BufReadCnt] == 0xFF) {
					checkSize++;
					if (checkSize == 4) {
						g_PacketMode = 1;
					}
				}
				else {
					checkSize = 0;
				}
				break;
				
			case 1:

				g_PacketBuffer.buffer[checkSize++] = g_buf[g_BufReadCnt];
				
				if (checkSize == 8) {
					if(g_PacketBuffer.data.id == g_ID){

						g_PacketMode = 2;
					}
					else{
						g_PacketMode = 0;
						checkSize = 0;
					}
				}

				break;
			
			case 2:
				
				g_PacketBuffer.buffer[checkSize++] = g_buf[g_BufReadCnt];
				check += g_buf[g_BufReadCnt];
				
				if (checkSize == g_PacketBuffer.data.size) {

					if(check == g_PacketBuffer.data.check){

						switch(g_PacketBuffer.data.mode){
								
							//수신 부분: case 2: 정상적으로 수신했을때
							case 2:
							// 이 변수들 확인해서 사용 - 어떻게 확인? -> 현재 ODE에서 값을 보내면 g_Pdes. gVlmit, g_Climit 값이 바뀜
								// 이후에 atmega에서 송신을 수행하면 해당값을 echo로 보내줌
							g_Pdes = g_PacketBuffer.data.pos / 1000.;
							g_Vlimit = g_PacketBuffer.data.velo / 1000.;
							g_Climit = g_PacketBuffer.data.cur / 1000.;
							break;
							}
					}
					
					check = 0;
					g_PacketMode = 0;
					checkSize = 0;
				}
				else if(checkSize > g_PacketBuffer.data.size || checkSize > sizeof(Packet_t)) {
					TransUart0('f');
					check = 0;
					g_PacketMode = 0;
					checkSize = 0;
				}
			}
		}
	    
		// 송신 부분: atmega128에서 현재 pos, velo, cur값을 전송
		if(g_SendFlag > 19){
			g_SendFlag = 0;			

				
			packet.data.id = g_ID;
			packet.data.size = sizeof(Packet_data_t);
			packet.data.mode = 3;
			packet.data.check = 0;
			
			//만약 현재 모터의 값을 받아오게 하고 싶으면 이부분을 수정해야함. g_Vlimit, g_Climit, g_Pdes가 아닌 g_Vcurrent, g_Ccurrent, g_Pcurrent
			packet.data.pos = g_Pdes * 1000; 
			packet.data.velo = g_Vlimit * 1000;
			packet.data.cur = g_Climit * 1000;  
			
			for (int i = 8; i < sizeof(Packet_t); i++)
			packet.data.check += packet.buffer[i];
			
			for(int i=0; i<packet.data.size; i++){
				TransUart0(packet.buffer[i]);
			}
		}
	}
		
}

