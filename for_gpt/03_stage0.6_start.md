# Stage 0.6 작업 체크리스트 & Codex 지시 프롬프트
(Implementation Guide + Codex Instruction)

## 목적
Stage 0.6은 구조 전환의 핵심 단계다.
이 문서는 실제 구현 작업을 시작하기 직전에 사용하는
작업 체크리스트 + Codex 지시서 역할을 동시에 한다.

주의: 모든 주석은 반드시 한글로 작성한다.
영어 주석이 섞이는 것은 금지한다.

---

## 0. 절대 불변 전제 (Stage 0.6 전체 공통)
- Release 빌드에서 파이프라인은 어떤 이유로도 종료되면 안 된다
- 디버깅 목적의 assert는 Debug 빌드에서만 존재
- Release 빌드에서는 assert 제거
- Store 단일 소유권 + zero-copy 전달
- Store가 메모리의 유일 소유자
- Node / Queue는 handle 또는 주소만 전달
- std::vector 복사 전달 금지 (Store 내부 구현으로만 허용)
- Queue overflow 시 DropOldestItem
- Stage 1/2의 기능을 Stage 0.6에 억지로 넣지 않는다

---

## 1. Stage 0.6 작업 범위 요약
### Stage 0.6에서 반드시 할 것
- InputSourceNode / DecodeNode 물리적 분리
- Pull / Push 입력을 모두 수용하는 InputSource 추상화 뼈대
- zero-copy 전달을 위한 신규 Store 생성
- Payload 타입(InputSourceQueueItem) 고정
- Thread 모델 유지 (node instance 1개 = thread 1개)

### Stage 0.6에서 하지 않는 것
- 실제 RTSP ingest
- 실제 decode (FFmpeg / GStreamer / NVDEC)
- 고급 queue empty 처리
- PTS/DTS 정책 적용
- 동적 thread/instance 증감

---

## 2. 파일 / 클래스 단위 작업 체크리스트
### 2.1 신규 Store: InputSourceDataBufferStore
목적
- InputSource에서 생성되는 raw / encoded 데이터를 담는 전용 Store
- 기존 FrameBufferStore는 decode 완료 프레임 전용이므로 사용 금지

구현 체크리스트
- 파일 위치 예시
  - stores/input_source_data_buffer_store.h
  - stores/input_source_data_buffer_store.cpp
- Store가 메모리의 유일 소유자
- 외부로는 handle 기반 view만 제공

API (Stage 0.6 고정)
- Acquire(size_bytes)
- View(handle) -> pointer + size
- Release(handle)
- HostMemory만 지원 (DeviceMemory는 TODO 주석)

필수 한글 주석 (강제)
- "zero-copy 전달은 절대 불변 원칙"
- "향후 DeviceMemory / NVDEC / DMA 확장 가능"
- "이 Store는 InputSource 전용이며 decode 완료 프레임용이 아님"

### 2.2 Payload 타입: InputSourceQueueItem
목적
- InputSource -> Decode 사이를 흐르는 큐 아이템
- 현재는 raw byte chunk
- 미래에는 frame / access unit으로 승격 가능

구현 체크리스트
- PascalCase: InputSourceQueueItem
- 포함 정보
  - channel_identifier
  - buffer_handle (InputSourceDataBufferStore)
  - size_bytes
  - sequence_number
  - TimestampPair (wall / monotonic)
  - PTS/DTS 필드 (optional, 비어 있음)
- bytes를 직접 소유하지 않음

필수 한글 주석
- "현재 구현은 raw byte chunk"
- "frame/access unit 승격 가능성 TODO"
- "하나의 frame이 여러 queue slot에 걸칠 수 있음"

### 2.3 InputSource 카테고리 Base Node
목적
- 다양한 입력 방식(Pull / Push)을 수용하는 추상 기반 클래스
- Stage 0.6에서는 dummy 구현 허용

핵심 설계
- InputSourceMode
  - Pull
  - Push
- Pull:
  - 내부 thread 루프에서 poll/read
- Push:
  - 외부 경계(TCP socket / REST 등)에서 호출 가능
  - thread-safe 진입점 필요

구현 체크리스트
- Base class 생성
- 실제 입력 구현 없음 (dummy OK)
- 멤버로 InputSourceMode 보유
- 상태 조회 accessor 제공
  - IDLE / ERROR / DISCONNECTED 등
- try/catch로 예외를 잡고 pipeline 비종료 유지

필수 한글 주석 (매우 중요)
- "외부 TCP/REST push 가능성을 전제로 설계"
- "Push 모드는 외부 경계에서 호출될 수 있음"
- "Stage 0.6에서는 구현하지 않지만 반드시 이 구조를 유지해야 함"
- "한 달 뒤의 나는 남이다 - 절대 구조를 합치지 말 것"

### 2.4 DecodeNode 분리
목적
- InputSource 책임과 decode 책임을 물리적으로 분리
- HW decode / device memory 확장을 위한 기반 확보

구현 체크리스트
- DecodeNode는 독립 thread
- 입력은 InputSourceQueueItem
- Stage 0.6에서는 stub decode 허용
- FrameMetadata / FrameBuffer 생성은 기존 계약 유지

필수 한글 주석
- "InputSource와 Decode는 반드시 분리"
- "향후 MemMoveNode 삽입 가능성"
- "NVDEC / DeviceMemory 대응 구조"

### 2.5 Thread / Queue 연결 구조
고정 구조
- InputSourceNode (thread)
  -> Queue<InputSourceQueueItem>
    -> DecodeNode (thread)
      -> Queue<FrameMetadata/FrameBuffer>
        -> Detection
        -> Output

구현 원칙
- node instance 1개 = thread 1개
- queue는 포인터/handle만 전달
- DropOldestItem 정책 유지

---

## 3. Stage 0.6 완료 조건 (Definition of Done)
- InputSourceNode / DecodeNode가 서로 다른 thread로 동작
- InputSourceDataBufferStore 생성 및 사용
- InputSourceQueueItem을 통한 전달 동작
- Dummy InputSource에서도 파이프라인이 정상 동작
- 예외 발생 시에도 파이프라인이 종료되지 않음
- 모든 신규 코드에 한글 주석이 충분히 포함됨

---

## 4. Codex 지시 프롬프트 (그대로 복사해서 사용)
아래 텍스트를 Codex에 그대로 붙여 넣어 사용한다.

```text
Codex Prompt

당신은 C++ 기반 데이터 파이프라인 프레임워크의 Stage 0.6 구현을 맡는다.
아래 조건을 절대적으로 지켜라.

공통 규칙

모든 주석은 반드시 한글
영어 주석 금지

약자 사용 금지 (표준 라이브러리 제외)

한 달 뒤의 개발자가 처음 보는 코드라고 가정하고 설명 주석을 과하게 작성

설계 전제

Release 빌드에서 파이프라인은 어떤 이유로도 종료되면 안 된다

Debug 전용 assert는 Release에서 제거

Store 단일 소유권 + zero-copy(handle 전달) 절대 유지

구현 범위

InputSourceNode / DecodeNode 구조 분리

Pull / Push 입력을 모두 지원하는 InputSource Base class

신규 Store: InputSourceDataBufferStore

Acquire(size_bytes) API

Payload 타입: InputSourceQueueItem

실제 RTSP / decode 구현은 하지 않는다

Stage 1/2 기능은 절대 추가하지 않는다

구현 스타일

구조적 확장을 강하게 의식한 설계

Dummy 구현이라도 인터페이스와 주석은 최종 형태를 전제로 작성

"나중에 고치자"가 아니라 "지금은 이렇게 고정한다"를 주석으로 명시
```

---

## 5. 마지막 확인
이 문서는 다음 용도로 그대로 사용 가능하다.
- Stage 0.6 시작 전 README
- Codex 입력 프롬프트
- 다음 ChatGPT 세션 컨텍스트
- 설계 의사결정 증빙 문서
