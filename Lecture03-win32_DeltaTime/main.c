#include <windows.h>
#include <stdio.h>

/* * [포인트 1: 왜 QueryPerformanceCounter인가?]
 * - 일반적인 time()이나 GetTickCount()는 정밀도가 낮음 (약 10~16ms 단위).
 * - 게임은 1ms(0.001초) 이하의 정밀도가 필요하므로, CPU의 '클럭 진동수'를 직접 이용하는 고해상도 타이머를 씀.
 */

 // [시험 대비: LARGE_INTEGER란?]
 // 64비트 정수를 담기 위한 Windows 전용 구조체야. 
 // 내부의 .QuadPart를 사용하면 64비트 정수 값을 통째로(long long처럼) 다룰 수 있어.
typedef struct {
    LARGE_INTEGER frequency; // [심장 박동수] 1초당 CPU가 진동하는 횟수 (고정값, 시스템 부팅 시 결정됨)
    LARGE_INTEGER prevTime;  // [기준점] 이전 프레임에서 측정한 '총 누적 진동 횟수'
    double deltaTime;        // [결과값] 두 시점 사이의 시간 간격 (초 단위, 예: 0.016s)
} CGameTimer;

// 타이머 초기화: 시스템의 성능 주파수를 먼저 알아내야 함.
void InitTimer(CGameTimer* timer) {
    // 1. [시스템 성능 측정] 하드웨어가 1초에 몇 번 진동하는지(Frequency) 가져와 저장
    // 이 값은 프로그램 실행 중에 변하지 않는 상수로 취급해.
    QueryPerformanceFrequency(&timer->frequency);

    // 2. [시작점 기록] 현재까지 흐른 총 진동 횟수(Counter)를 가져와 이전 기록으로 저장
    QueryPerformanceCounter(&timer->prevTime);

    timer->deltaTime = 0.0;
}

// 델타 타임 업데이트: 매 루프(프레임)마다 호출함.
void UpdateTimer(CGameTimer* timer) {
    LARGE_INTEGER currentTime;

    // 1. [현재 시점 측정] 현재 시점까지의 총 누적 진동 횟수를 새로 가져옴
    QueryPerformanceCounter(&currentTime);

    /* * [포인트 2: 델타 타임 계산 공식]
     * (현재 카운트 - 이전 카운트) = 이번 프레임 동안 실제로 발생한 '진동 횟수 차이'
     * (발생한 진동 횟수 / 초당 진동 횟수) = 실제 흐른 시간(초 단위)
     */
     // (현재 틱 - 이전 틱) / 초당 틱수 = 흐른 시간
     // QuadPart는 64비트 정수이므로 나눗셈 시 정밀도를 위해 double로 캐스팅(Casting) 필수!
    timer->deltaTime = (double)(currentTime.QuadPart - timer->prevTime.QuadPart) / (double)timer->frequency.QuadPart;

    // 2. [시점 갱신] 다음 프레임 계산을 위해 현재 시간을 '이전 시간'으로 갱신
    // 이 과정이 있어야 매 프레임마다 '직전 프레임과의 차이'만 순수하게 잴 수 있음.
    timer->prevTime = currentTime;
}

int main() {
    CGameTimer myTimer;
    InitTimer(&myTimer); // 1초당 진동수와 시작 시각을 세팅

    printf("C 스타일 고해상도 타이머 시작 (Ctrl+C로 종료)\n");

    int i = 0;
    for (i = 0; i < 10; ++i) {
        // [루프 시작] 시간을 재고 출력하는 과정
        UpdateTimer(&myTimer);

        // 델타타임의 역수(1 / dt)를 취하면 '1초에 몇 프레임이 도는지'인 FPS를 구할 수 있음.
        printf("DeltaTime: %f sec (FPS: %f)\n", myTimer.deltaTime, 1.0 / myTimer.deltaTime);

        // [테스트] 100ms(0.1초) 대기. 
        // 실제 DeltaTime은 0.1xxx초 정도로 찍힐 것임 (대기 시간 + 코드 실행 시간)
        Sleep(100);

        // 실제 게임 루프라면 여기서 playerPos += speed * myTimer.deltaTime; 로직이 들어감.
        // 이것이 '프레임 독립적 이동(Frame Rate Independence)'의 핵심 공식!
    }

    return 0;
}


/*
1. QueryPerformanceFrequency와 Counter의 차이가 뭐야?
- Frequency: 하드웨어의 초당 진동수(고정값). 시간을 구하기 위한 나눗셈의 분모로 쓰여.
- Counter: 지금까지 흐른 총 진동수(변하는 값). 시간의 시점을 기록할 때 쓰여.

2. 왜 LARGE_INTEGER와 QuadPart를 써?
- 시스템 진동수는 숫자가 너무 커서 일반적인 int나 long으로는 담을 수 없기 때문이야. 그래서 64비트 큰 정수인 LARGE_INTEGER를 쓰고, 그 값을 숫자처럼 계산할 때 .QuadPart를 꺼내 쓰는 거지.

3. deltaTime 계산에서 double로 형변환(Casting)하는 이유는?
- 정수끼리 나누면 소수점이 다 잘려나가서 0이 나올 수 있어. 0.016 같은 정밀한 초 단위 값을 얻으려면 반드시 실수형(double)으로 계산해야 해.

4. FPS는 어떻게 계산해?
- 1.0 / deltaTime. 즉, 1초를 한 프레임에 걸린 시간으로 나누면 1초에 몇 번이나 화면을 그릴 수 있는지(Frame Per Second)가 나와.
*/