# 1. 통신 수정
현재 ODE에서 각도, 속도, Torq값을 넣으면 그 값 그대로 아래에 출력이 됨, 이 이유는 atmega main.c에서 통신 부분을 보면 알 수 있음. 조교님이 주신 코드는 입력한 값을 그대로 송신하는 것
이기 때문에 ODE에서 현재 모터의 각도값, 속도값, Current값을 그래프로 그리고 출력하려면 송신 부분을 수정해야함


# 2. 
