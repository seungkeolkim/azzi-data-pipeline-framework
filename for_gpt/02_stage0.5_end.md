# Continuation / Context Summary (Stage 0.5 완료 + Stage 0.6/1.5 추가 정의)
> 목적: 다음 채팅 세션에서 이 문서 + 전체 소스코드 압축 + PPT를 입력으로 주면  
> “설계 의도/개발 철학/단계(Stage) 정의/현재 상태/다음 작업”이 100% 복원되도록 한다.  
> **한 달 뒤의 나는 남이다** 기준으로, 길고 과하게 적는다.

---

## 0. 큰 목표 (Big Picture)
우리가 만들고 싶은 것은 “고정형 DeepStream 파이프라인”이 아니라:

- **동적으로 구성/교체 가능한 DAG 기반 Stream Pipeline Framework**
- 입력은 영상(RTSP 등)을 포함하되 영상에만 제한하지 않는 “범용 스트림”을 지향
- 장기적으로는:
  - 노드(모델) 추가/삭제
  - DAG 교체
  - 모델 교체
  - 채널별 서로 다른 DAG
  - 그리고 “과거 프레임/윈도우 상태가 미래 프레임 처리에 영향을 주는” 구조
  를 지원한다.

핵심은 “데이터 자체”보다 **Meta 중심 설계**다:
- FrameMetadata / FrameBuffer / ObjectMetadata
- Channel 단위의 Runtime Meta (time window 포함)
- Node 단위의 Runtime State (counters / 상태)

---

## 1. 개발 철학 / 컨벤션 (매우 중요, 강한 합의)
### 1.1 약자 사용 금지 (도메인 개념에 약자 금지)
- **직접 만든 개념/도메인 타입에는 약자 사용 금지**
- 허용 가능한 수준의 약자:
  - 표준 라이브러리 `std`
  - 컴퓨터비전 일반 약자 `cv` (단, 우리 도메인 타입명에는 금지)
  - Interface를 의미하는 `I*` 같은 컨벤션은 남발 금지
- 타입/변수명은 F12 눌러야 의미가 드러나는 이름 금지.
  - `object_handle`처럼 “그 자체로 역할이 보이는” 이름 사용.

### 1.2 주석은 과할 정도로
- “한 달 뒤의 나는 남이다” 기준으로 설명한다.
- 특히 C++은 문맥 복원이 어렵기 때문에:
  - 소유권(ownership)
  - 수명(lifecycle)
  - lock 범위
  - drop 정책
  - stage 경계
  를 코드 근처에 남긴다.

### 1.3 소유권/정합성 가드레일 (Stage 0 철학)
- **Store가 메모리의 유일 소유자(single owner)**  
  노드는 handle/포인터/ref만 가진다.
- 동일 데이터가 중복 생성되어 정합성 문제가 발생하면 실패로 간주한다.
- OutputNode는 명확한 **release chain의 종착점**이어야 한다:
  - object → buffer → frame
- Queue는 데이터 복사가 아니라 **포인터만 전달**한다.

### 1.4 Drop 정책 (운영 안정성 우선)
- Queue overflow 시 **DropOldestItem**
- drop은 파이프라인을 중단하지 않고 “해당 프레임만 유실”
- sink 실패(파일 I/O 등)는 해당 프레임 drop으로 처리(시스템 전체 영향 금지)

### 1.5 Stage 경계는 반드시 문서/코드에 명시
- “어떤 변화가 들어오면 Stage가 바뀌는지”를 분명히 한다.
- 다음 stage에서 할 일을 stage 0.x에 억지로 끼워 넣지 않는다(Too much 방지).

---

## 2. Stage 정의 (업데이트 버전: 0, 0.5, 0.6, 1, 1.5, 2)
초기에는 Stage 0/1/2(+0.5 optional)였지만, 진행 과정에서 추가 Stage가 필요해졌다.

### Stage 0 (Pre-MVP skeleton)
목표:
- decode(dummy/synthetic) → dummy detection → output
- 파이프라인이 “돌아가기만 하는지” 확인
- 구조/계약/ownership/thread/queue 안정성 검증이 목적

포함:
- Node=thread
- 포인터 큐 기반 연결
- FrameMetadata/Buffer/Object의 store 소유권 계약
- Output의 release chain

비포함:
- 실제 RTSP ingest
- 실제 decode
- 실제 모델 inference
- GPU 최적화

---

### Stage 0.5 (Observability + Global Meta skeleton)  **(현재 절반 달성 상태)**
목표:
- Stage 0 구조 유지(깨지면 안 됨)
- “무슨 일이 일어나는지” 관찰 가능하게 만들기
- 프레임을 넘어서는 **채널 단위 Runtime Meta(time window/TTL)** 뼈대 추가
- 노드 단위 **Runtime State(counters/state)** 뼈대 추가

핵심 구현 컨셉:
- `ChannelRuntimeMeta` (단일 구조체 안에 global/frame/window를 일단 통합)
- TTL(time window) 기반 last_seen
- `ChannelRuntimeMetaStore` (옵션 A: mutex backend)
  - **read는 find-only (create 금지)**  
    read로 엔트리를 만들면 hallucination성 state 생성 위험이 있음.
  - write는 create 허용
- `NodeRuntimeStateStore` (node_instance_id → counters/state)
  - 노드 내부 소유 금지
  - 다른 노드에서 조회 가능해야 함

주의(중요):
- 현재 구현은 store read/write가 **lock을 잡은 채 callback을 실행**한다.
  - Stage 0.5에서는 단순/안전성 우선으로 유지
  - 단, callback은 반드시 짧게, store 재진입 금지
  - 장기적으로 개선 필요(아래 “후보” 참조)

현재 상태 평가:
- “meta lifecycle / TTL / A(write)→C(read) / node counters store”는 동작한다.
- 하지만, 원래 Stage 0.5에서 하기로 했던 **DecodeNode의 InputSource 분리**가 구현/전달에서 누락되어 Stage 0.5는 “절반 달성”으로 평가한다.

---

### Stage 0.6 (신설) — InputSourceNode + DecodeNode 분리 (RTSP 1개 한정)
배경:
- 원래 Stage 1에서 하려던 “실제 RTSP 입력”은 Stage 1의 시작으로 간주한다.
- 그러나 DecodeNode 내부에 input source 책임이 섞여 있으면 구조적으로 확장 불가능하다.
- Stage 0.5에서 분리하기로 했으나 누락되었으므로, **Stage 0.6으로 명확히 분리**한다.

목표:
- DecodeNode를 다음 두 노드로 분리한다.
  1) **InputSource 처리 Node**: (우선 RTSP 1개만)
  2) **Decode Node**: 입력(패킷/프레임 단위)을 받아 decode 결과를 FrameMetadata로 만든다.

범위 제한 (중요):
- Stage 0.6은 “구조 분리”가 목적이다.
- **실제 decode를 완성하지 않아도 된다.**
  - RTSP에서 받는 데이터가 있더라도
  - decode를 실제로 풀거나(FFmpeg/GStreamer)
  - 실제 프레임을 생성하는 것은 Stage 1에서 본격화할 수 있다.
- 단, InputSource와 Decode의 “계약”은 고정해야 한다.
  - 예: EncodedVideoPacket(또는 유사 개념) 구조체/handle
  - timestamp/sequence/channel_identifier 포함

RTSP 1개 한정의 의미:
- source 하나(단일 채널) 기준으로 InputSource→Decode 연결이 성립하면 됨
- multi source는 Stage 1.5에서 다룬다.

권장 성공 기준:
- 파이프라인 thread 구조는 다음과 같이 된다:
  - InputSourceNode(thread) → Queue(packet*) → DecodeNode(thread) → Queue(frame*) → Detection → Output
- DropOldestItem/backpressure 정책이 **InputSource→Decode 구간에서도 동일 철학으로 적용**된다.

---

### Stage 1 (Real decode + real detection, CPU + ONNX Runtime)
정의(경계):
- Stage 1은 “실제 RTSP ingest + 실제 decode”가 파이프라인 입력으로 연결되는 순간 시작한다.
- device 간 이동은 크게 고려하지 않고, CPU 기반으로 완주한다.

목표:
- 실제 모델을 붙여도 구조/계약이 유지되는지 검증
- ORT 기반 inference adaptor 도입(우선 CPU)
- preprocess/postprocess 계약 확정

중요:
- CPU 구현으로 굳어지지 않게, 텐서 계약은 TRT 친화적으로 유지(NCHW 등).

---

### Stage 1.5 (신설) — InputSource 2개 이상 (Multi source)
목표:
- 입력 source가 2개 이상일 때도 framework 구조가 깨지지 않는지 검증
- 각 source/channel에 대해:
  - ChannelIdentifier 분리
  - ChannelRuntimeMetaStore key 분리(채널 별 독립)
  - NodeRuntimeStateStore(노드 인스턴스 관측) 유지
- 동적 DAG의 완전 구현은 아직 아니지만,
  - “한 노드가 모든 stream을 거치지 않는다”를 현실적으로 체감하는 단계

권장 성공 기준:
- 2개 이상의 RTSP(or file) source를 연결
- source별 queue/노드 인스턴스가 어떻게 구성되는지 명시
- 최소한 로그/카운터에서 “source별로 무엇이 발생하는지” 구분 가능

---

### Stage 2 (TensorRT + GPU optimization)
목표:
- TRT adaptor 구현
- GPU preprocess
- host-device copy 최소화
- CUDA stream/context pool 관리
- 성능 측정 및 병목 제거

---

## 3. 체크포인트 (서로 다른 의미로 구분)
### 체크포인트 1: 설계/의사결정 사항
- Stage 0.5는 옵션 A(mutex) 기반으로 GlobalMetaStore 성격 구현
- NodeRuntimeStateStore(노드 인스턴스 카운터/상태) 추가
- A↔C 영향 및 time-window(TTL) 유지 컨셉 확립
- atomic/lock-free 전환은 후보로만 유지
- 빌드 타임 YAML → 컴파일 타임 #define 전환(backend 정책 선택)도 후보로만 유지  
  → 나중에 README/요약에 반드시 포함

### 체크포인트 2: Codex가 만든 Stage 0.5 결과물
- ChannelRuntimeMeta(+TTL), ChannelRuntimeMetaStore(mutex)
- NodeRuntimeStateStore
- DummyDetection에서 meta write, Output에서 meta read/TTL log 동작
- 단, 4.1 수정 필요:
  - read()는 create하면 안 됨(find-only)
- InputSource/Decode 분리는 반영되지 않아 Stage 0.5는 절반 달성 평가

---

## 4. Stage 0.5의 “남겨둔 설계 포인트” (현재 결정 상태)
### 4.1 read()는 find-only (0.5 종료 시점 현재 수정완료)
- 동의: read가 create하면 hallucination성 메타 생성 위험
- 수정: read는 find-only, 없으면 callback 호출 없이 return

### 4.2 callback under lock (향후 개선 포인트로 강하게 명시, 지금은 유지)
- 정합성 이슈 때문에 단순히 lock을 풀기 어렵다.
- Stage 0.5에서는 유지하되,
  - callback 짧게
  - store 재진입 금지
  를 강한 컨벤션으로 남긴다.

### 4.3 decode_fps_limit / detection_skip_frames는 공간만 유지
- 적절: 자리만 확보하고 Stage 1 이후에서 사용

### 4.4 error_count/drop_count 정책 강화는 Stage 1 이후로 이관
- Stage 1에서 실제 I/O 실패/에러의 의미가 생기면 정리

---

## 5. 앞으로의 작업 순서 (새 기준: Stage 0.6 먼저)
우선순위는 다음과 같다.

1) **Stage 0.6 착수 (핵심)**
   - DecodeNode 책임을 InputSourceNode + DecodeNode로 분리
   - RTSP 1개 한정
   - 실제 decode 완성은 Stage 1에서 본격화 가능하나,
     InputSource→Decode 계약(패킷 타입, timestamp, channel_identifier 등)은 Stage 0.6에서 고정한다.

2) Stage 0.6 이후 Stage 1 → Stage 1.5 → Stage 2 순으로 진행

---

## 6. 다음 세션에 전달될 입력(복원 재료)
다음 채팅에서는 아래 3개를 입력으로 제공한다.

1) 전체 소스코드 압축(tar/zip)
2) 본 문서(이 Markdown)
3) PPT (컨셉/비즈니스 로직 및 meta 활용 시나리오)

다음 세션의 첫 문장 예시:
> “아래 Context Summary를 전제로 Stage 0.6(InputSource/Decode 분리)부터 진행하고 싶다.  
> Stage 0.5는 meta/counter는 동작하나 decode 분리가 누락되어 절반 달성 상태다.”

---

## 7. 금지 사항 / 경고 (다음 세션에서도 동일)
- Codex로 bulk 개발을 진행하지 않는다.
  - 이유: 구현의 정확성 검증/테스트가 너무 어려움.
- “Stage 0.x”에서 Stage 1/2 성능 최적화나 과도한 리팩터를 하지 않는다.
  - Too much 작업 방지
- 약자 사용 금지, 주석 과다 원칙, 소유권/정합성 가드레일은 항상 유지한다.
