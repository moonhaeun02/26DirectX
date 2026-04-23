#include <iostream>
#include <chrono> // C++ 표준 시간 라이브러리: 나노초 단위까지 정밀 측정 가능
#include <thread> // CPU를 잠시 쉬게 하는 sleep_for 기능을 위해 포함

/* * [시험 대비 핵심 요약]
 * 1. steady_clock: 시스템 시계가 바뀌어도 영향을 받지 않는 '게임용 절대 시계'
 * 2. time_point: 특정 '시점' (셀카 찍은 순간)
 * 3. duration: '시간 간격' (셀카 찍고 다음 셀카 찍을 때까지 걸린 시간)
 * 4. count(): duration이라는 객체에서 실제 숫자(float)를 뽑아내는 함수
 */

class CPPGameTimer {
public:
    CPPGameTimer() {
        // [초기화] 타이머가 생성되는 순간의 시간을 '이전 시간'으로 기록해둠
        // 이렇게 해야 첫 번째 Update() 호출 시 델타타임 계산이 가능함
        prevTime = std::chrono::steady_clock::now();
    }

    // 매 프레임(while 루프)마다 한 번씩 호출됨
    float Update() {
        // 1. [현재 시점 기록] 이번 프레임이 시작된 '지금' 시간을 측정
        // auto는 std::chrono::steady_clock::time_point 타입을 자동으로 찾아줌
        auto currentTime = std::chrono::steady_clock::now();

        /* * [C++ 스타일 강의 포인트 2: Duration 계산]
         * - 두 시점(time_point)을 빼면 기간(duration)이 나옴.
         * - duration_cast를 통해 우리가 원하는 단위(초, 밀리초 등)로 변환함.
         */
         // [계산] 현재 시간 - 이전 시간 = 한 프레임이 돌아가는 데 걸린 '순수 시간'
        std::chrono::duration<float> frameTime = currentTime - prevTime;

        // 2. [수치 변환] 계산된 시간 객체(frameTime)에서 float 숫자만 추출
        // 예: 0.166667초 같은 값을 deltaTime 멤버 변수에 저장
        deltaTime = frameTime.count();

        // 3. [시점 갱신] 중요! 이번에 측정한 '현재 시간'은 다음 프레임에선 '이전 시간'이 됨
        // 이 줄이 없으면 델타타임이 매 프레임 누적되어 엄청나게 커짐 (캐릭터 순간이동 발생)
        prevTime = currentTime;

        return deltaTime;
    }

    // 외부에서 계산된 델타타임을 읽어갈 때 사용 (상태값 반환)
    float GetDeltaTime() const { return deltaTime; }

private:
    // 이전 프레임의 시간을 기억하는 장소 (기준점)
    std::chrono::steady_clock::time_point prevTime;

    // 계산된 프레임 간격 (초 단위)
    float deltaTime = 0.0f;
};

int main() {
    // 1. 타이머 인스턴스 생성 (생성자 호출됨)
    CPPGameTimer timer;

    std::cout << "C++ STL chrono 타이머 테스트 시작" << std::endl;

    // 10프레임 동안 테스트 진행
    for (int i = 0; i < 10; ++i) {
        // [업데이트] 매 루프마다 걸린 시간을 측정함
        float dt = timer.Update();

        // 출력되는 dt값은 아래 sleep_for로 멈춘 시간 + 코드 실행 시간이 나옴
        std::cout << i + 1 << "번째 프레임 간격(dt): " << dt << "s" << std::endl;

        // [테스트를 위해 의도적으로 루프를 지연시킴] 
        // 현실 게임으로 치면 '그래픽을 그리느라 CPU가 고생한 시간'을 흉내 낸 것
        // 100ms = 0.1초 동안 멈춤
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    /*
     * [시험 포인트: 델타타임 활용법]
     * 별의 위치를 옮길 때: posX += Speed * dt;
     * 1. 빠른 컴 (dt가 0.01): 쪼금씩 자주 더함.
     * 2. 느린 컴 (dt가 0.1): 한 번에 많이 더함.
     * 결과적으로 1초 뒤 두 컴의 캐릭터 위치는 똑같음!
     */

    return 0;
}