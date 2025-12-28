# Stage 0.6 Kickoff README  
(설계 결정사항 / 컨벤션 / 작업 범위 정리)

> 목적  
> Stage 0.5 완료 상태에서 Stage 0.6을 시작하기 위한 **설계 의도 / 개발 철학 / 결정사항 / 최소 구현 범위**를  
> “한 달 뒤의 나는 남이다” 기준으로 복원한다.  
>  
> 이 문서는 다음 채팅 세션, Codex, 혹은 다른 구현자에게 그대로 전달 가능한  
> **설계 계약서 + 작업 지시서** 역할을 한다.

---

## 0. Big Picture (상위 목표)

우리가 만들고 싶은 것은 고정형 DeepStream 파이프라인이 아니라:

- **동적으로 구성/교체 가능한 DAG 기반 Stream Pipeline Framework**
- 입력은 영상(RTSP 등)에 국한되지 않는 **범용 스트림**
  - TCP push
  - REST push
  - 센서 데이터
  - 파일/기타 스트림
- 장기적으로는 GUI 기반 drag-drop으로:
  - 노드 추가/삭제/교체
  - DAG 교체
  - 채널별 서로 다른 DAG
  - “과거 프레임/윈도우 상태가 미래 프레임 처리에 영향을 주는” 구조
  를 지원한다.

핵심은 데이터 자체가 아니라 **Meta 중심 설계**다.

- FrameMetadata / FrameBuffer / ObjectMetadata
- Channel 단위 Runtime Meta (TTL / time window)
- Node 단위 Runtime State (counter / state)

---

## 1. 개발 철학 / 컨벤션 (강한 합의)

### 1.1 약자 사용 금지
- 직접 만든 도메인 타입 / 클래스 / 변수명에 약자 사용 금지
- 허용 예외:
  - 표준 라이브러리 (`std`)
- 의미가 F12 없이는 드러나지 않는 이름 금지

### 1.2 주석은 과할 정도로
- “한 달 뒤의 나는 남이다” 기준
- 특히 아래 항목은 **코드 근처에 반드시 주석으로 남긴다**
  - 소유권(ownership)
  - 수명(lifecycle)
  - lock 범위
  - drop 정책
  - stage 경계

### 1.3 Store 단일 소유권 + zero-copy 전달 (절대 불변)
- **Store가 메모리의 유일 소유자(single owner)**
- 노드/큐는 handle/주소만 전달
- Stage 0.6에서도 이 원칙은 절대 깨지면 안 된다  
  (임시 vector 복사 등 금지)

### 1.4 Drop 정책
- Queue overflow 시 **DropOldestItem**
- drop은 파이프라인 중단 없이 해당 아이템만 유실
- sink 실패도 전체 시스템 중단 없이 drop 처리

### 1.5 파이프라인 비종료 원칙
- Release 빌드에서는 **어떤 이유로도 파이프라인 종료 금지**
- Debug 전용 assert는 Release 빌드에서 제거되어야 함

---

## 2. Stage 0.5 현재 상태 요약

구현 완료:
- ChannelRuntimeMeta (+ TTL)
- ChannelRuntimeMetaStore (mutex backend, read=find-only)
- NodeRuntimeStateStore
- DummyDetection → meta write
- Output → meta read + TTL 기반 로그 출력

누락(절반 달성 평가 이유):
- DecodeNode 내부에 InputSource 책임이 섞여 있음
- InputSource / Decode 구조 분리 미구현

---

## 3. Stage 0.6 목표 및 범위 (확정)

### 3.1 Stage 0.6의 목적
Stage 0.6은 성능 단계가 아니라 **구조 분리 단계**다.

- InputSourceNode / DecodeNode **구조적 분리**
- Pull / Push 입력을 모두 수용 가능한 **InputSource 추상화 뼈대**
- HW decode / NVDEC / DeviceMemory 확장을 고려한 **노드 분리 기반 확보**
- zero-copy / Store 단일 소유권 원칙을 Stage 0.6에서도 유지

### 3.2 Stage 0.6 Non-Goals (하지 않는 것)
- 실제 RTSP ingest 구현
- 실제 decode 구현 (FFmpeg / GStreamer / NVDEC)
- 고급 queue empty 처리 정책
- PTS/DTS 기반 time sync
- 동적 thread/instance 증감 구현

---

## 4. 핵심 설계 결정사항 (Checkpoint)

### 4.1 Node / Thread 모델
- 기본 원칙: **node instance 1개당 thread 1개**
- Stage 0.6에서:
  - InputSourceNode = 독립 thread
  - DecodeNode = 독립 thread
- 이유:
  - HW decode / device memory 최적화 대비
  - 장기적으로 InputSource → MemMove → Decode 삽입 가능성 확보

> 동적 instance/thread 증감은 장기 목표로 유지  
> Stage 0.6에서는 구조적 여지만 남긴다

---

### 4.2 InputSource는 Pull / Push 모두 지원
- Pull:
  - node 내부 thread에서 poll/read
- Push:
  - 외부 경계에서 데이터 주입
  - TCP socket 기반 push 가능
  - REST push 가능
- Stage 0.6에서는 dummy 구현 가능하나,
  - Base class 설계에 Pull/Push 모드가 반드시 존재해야 함
  - InputSourceMode(Pull | Push)를 멤버로 가짐
  - 외부 경계 호출을 전제로 thread-safety 주석 필수

---

### 4.3 에러 처리 골조
- 다양한 에러 발생 가능 (disconnect, timeout, parse error 등)
- Stage 0.6 목표:
  - try/catch 기반 비종료 골조
  - 노드 상태 조회 가능한 accessor 제공
- queue empty 처리 정책은 Stage 1 이후로 이관

---

### 4.4 InputSource → Decode 데이터 단위
- 장기 목표:
  - EncodedVideoFrame / AccessUnit
- 현실 제약:
  - TCP stream에서 frame 경계 불명확한 경우 존재
- Stage 0.6 결정:
  - **raw byte chunk로 구현 고정**
  - TODO 주석으로:
    - frame/access unit 승격
    - 하나의 frame이 여러 queue slot을 차지할 수 있음
    명시

---

### 4.5 Payload 타입 이름
- InputSource → Decode 큐 아이템 타입:
  - **`InputSourceQueueItem` (PascalCase)**
- 구현 디테일에 묶이지 않는 중립적 이름

---

### 4.6 미디어 타임스탬프
- PTS/DTS 필드는 Stage 0.6에서 **존재만**
- 실제 사용은 Stage 1에서 처리

---

### 4.7 신규 Store 생성
- 기존 `FrameBufferStore`:
  - decode 완료 프레임 전용
- encoded/raw 입력용 Store는 별도 필요
- 신규 Store 이름:
  - **`InputSourceDataBufferStore`**

---

### 4.8 InputSourceDataBufferStore API
- Stage 0.6에서는 단순안 채택
  - **Acquire(size_bytes)**
- DeviceMemory / NVDEC 연계는 Stage 1/2에서 확장
- 확장 가능성은 주석으로 명시

---

## 5. 강제 주석 지침 (미래 확장용)

Stage 0.6에서는 모든 노드를 제너릭하게 완성하지 않는다.  
대신 반드시 다음 내용을 **코드 주석으로 강하게 남긴다**.

### 5.1 노드 분류(Category)
장기적으로 노드는 다음 대분류를 가진다:

- InputSource
- Decode
- MemMove
- UserLogic
- Output
- Etc

각 Category는:
- 공통 Base class (Origin class)
- 다양한 파생 구현 노드

---

### 5.2 포트 / 계약 기반 연결
- GUI drag-drop 연결은 “이름”이 아니라 **입출력 계약**으로 판단
- 같은 interface, 다른 동작의 노드 조합 가능해야 함
- zero-copy / handle 전달 원칙은 모든 Category에 공통 적용

---

## 6. Stage 0.6 성공 기준 (관측)

### 최소 로그 기준
1. Thread lifecycle
   - `[InputSourceNode] started / stopped`
   - `[DecodeNode] started / stopped`

2. 상태 관측
   - InputSourceNode 상태(IDLE / ERROR 등) 조회 가능

3. 에러 주입 테스트
   - DummyInputSource에서 예외 발생
   - catch 후 상태/카운터 갱신
   - 파이프라인 지속 동작

---

## 7. Stage 0.6 구현 가이드 요약

- InputSource Base class
  - Pull / Push 모드 지원
  - 외부 경계 호출 전제
  - 상태 accessor 제공

- DecodeNode 분리
  - InputSourceQueueItem 입력 처리
  - stub 구현 허용

- 신규 Store
  - InputSourceDataBufferStore
  - zero-copy / handle 전달
  - Acquire(size_bytes)

- Payload
  - InputSourceQueueItem
  - buffer store handle만 보유
  - raw byte chunk 기반
  - 미래 frame/access unit TODO 명시

---

## 8. 핵심 요약 문장 (다음 채팅/코덱스 전달용)

Stage 0.6은 실제 RTSP/decode 구현 단계가 아니라  
InputSourceNode와 DecodeNode의 구조적 분리와  
Pull/Push 입력을 모두 수용하는 InputSource 추상화 뼈대를 만드는 단계다.  

모든 데이터 전달은 Store 단일 소유권 기반(handle/주소 기반) zero-copy를 절대 유지한다.  
InputSourceQueueItem은 raw byte chunk 구현으로 고정하되  
미래 frame/access unit 승격을 위한 TODO를 강하게 남긴다.  

encoded/raw 입력을 위해 InputSourceDataBufferStore를 신규 생성하고  
Acquire(size_bytes)로 단순 시작한다.  

Release 빌드에서 파이프라인은 어떤 이유로도 종료되면 안 된다.
