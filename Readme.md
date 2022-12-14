# 0. 고려해야할 사항
(이 부분은 만약에 막히면 원인으로 선택할 수 있는 선택지를 의미함)

Timer 이용시 주의
1. Timer 1 PWM A, PWM B 사용 - 강의자료에 그렇게 되어있음

1. 현재 크리스탈이 12Mhz로 되어있는데 코드의 MCU 제어주기는 16Mhz로 되어있음
2. Set -> 통신 -> 모터 회전 -> main 함수에 있는 조건 분기문에 쓰면 되지않을까
3. 위치 제어기가 속도 제어기 보다 더 쉽다?

# 해야할 것
## 1. Set 버튼을 누르면 Motor가 동작. 버튼을 눌렀을때 Motor가 돌아가면 안되고 Set을 눌렀을때 모터가 동작해야함
예상 해결 방법) SetDuty() 함수에서 모터가 정지하도록 하는 전압값을 넣어야함. 이때 전압을 넣기 위해선 PWM 그리고 바이폴라 방식에 대해서 알아야함

## 2. ODE 좌표축 설정
예상 해결 방법) 이 부분은 조금 조사가 필요

## 3. Graph 수정 - Target과 Current 그래프가 둘다 필요한데 이거는 이미 visual 코드에 써있을 듯? 잘 모르겠따


# 1. 통신 수정
현재 ODE에서 각도, 속도, Torq값을 넣으면 그 값 그대로 아래에 출력이 됨, 이 이유는 atmega main.c에서 통신 부분을 보면 알 수 있음. 조교님이 주신 코드는 입력한 값을 그대로 송신하는 것
이기 때문에 ODE에서 현재 모터의 각도값, 속도값, Current값을 그래프로 그리고 출력하려면 송신 부분을 수정해야함

''' 

    //송신 부분: atmega128에서 현재 pos, velo, cur값을 전송

		if(g_SendFlag > 19){
			g_SendFlag = 0;			

				
			packet.data.id = g_ID;
			packet.data.size = sizeof(Packet_data_t);
			packet.data.mode = 3;
			packet.data.check = 0;
			
			//만약 현재 모터의 값을 받아오게 하고 싶으면 이부분을 수정해야함. g_Vlimit, g_Climit, g_Pdes가 아닌 g_Vcurrent, g_Ccurrent, g_Pcurrent
      // 이부분을 수정 목표값, limit값이 아닌 현재 값을 송신할 수 있도록 해야함
			packet.data.pos = g_Pdes * 1000; 
			packet.data.velo = g_Vlimit * 1000;
			packet.data.cur = g_Climit * 1000; 
      //여기까지
			
			for (int i = 8; i < sizeof(Packet_t); i++)
			packet.data.check += packet.buffer[i];
			
			for(int i=0; i<packet.data.size; i++){
				TransUart0(packet.buffer[i]);
			}
		}


'''


# 2. 그외에 알아낸 것들(이건 그냥 잡담 틀린 것도 있을듯)
4. Set이되면 포트가 연결되어있을때 패킷을 보냄(ODE에서 - Dlg의 OnBnClickedBtnSet()) ->
OnBnClickedBtnSet()에서 SET_SYSTEM_MEMORY("Comm1Work_Controller_Target", motor_data); 라는 함수를 실행하면 통신 변수를 생성함
-> CommWork에서 생성자 CWorkBase(name) 호출시 생성한 통신데이터를 _memname_tar = name + "_Controller_Target"; 이라는 이름으로 CREATE_SYSTEM_MEMORY(_memname_tar, ControlData_t);
을 호출하여 패킷을 생성함 이 패킷에는 타겟 전압값, 전류값, 속도값이 포함되어있음
->  CCommWork::_execute() 에서 GET_SYSTEM_MEMORY(_memname_tar, _target);로 통신 변수 _memname_tar에 타겟값(Ode에서 postion, velocity ,current값)을 불러옴
-> _sendPacket.data.check = 0;
		_sendPacket.data.pos = _target.position * 1000;
		_sendPacket.data.velo = _target.velocity * 1000;
		_sendPacket.data.cur = _target.current * 1000; 로 전송 패킷 설정
->  _comm.Write((char*)_sendPacket.buffer, sizeof(Packet_t));로 송신
 atmega의 main문에서 읽음
->  case2가 정상적으로 수신했을때의 동작을 의미함

5. 
Set버튼을 누르면 ode_data에 입력한 각도 속도 값으로 넣고 Jointdata에 넣음 -> Graph는 JointData를 받아와서 출력함 -> 입력한 값에 따라서 그래프에 뿌려줌

6. 
Set 버튼을 누르면 motor_data에 입력한 position, velocity, current값을 넣음(Dlg에서 Set 부분에서 잘보면 current값이 TarTorq에 지정되어있음)
motor_data.current = atof(str.GetBuffer()) / 0.0683; 로 되어있는데 여기서 0.0683은 토크 상수 Ke 이고 (토크) = Ke * i 라서 i = (토크) /Ke가 됨
따라서 motor_data에 들어가는 current는 전류, ODE에 출력되는 Torq는 토크가 맞음

 
7.
OnTimer가 언제 실행되는건지는 모르겠는데 일단 회색 칸 그니까 m_editCurPos, m_editCurVel, m_editCurTorq는 OnTimer에서 이루어짐
그리고 해당 값을 motor_data에서 뿌려줌

8. 
