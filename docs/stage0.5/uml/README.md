# Stage 0.5 – Observability & Global Meta Skeleton

## 목적 (Why Stage 0.5 Exists)

Stage 0.5는 **Stage 0의 파이프라인 골격을 유지한 채**,  
다음 두 가지를 검증/고정하기 위해 존재한다.

1. **관찰 가능성(Observability)**  
   - 노드/채널 단위 처리량, drop, 에러를 “중앙 Store”에서 관측 가능하게 만든다.
   - 디버그/운영 시 “무슨 일이 일어났는지”를 추적할 수 있어야 한다.

2. **프레임을 넘어서는 Global Meta 뼈대**  
   - A 노드가 만든 결과를 C 노드가 읽고,
   - 그 결과가 다시 A(또는 다른 노드)의 미래 동작에 영향을 줄 수 있는 구조를 만든다.
   - time-window(TTL) 기반 상태 유지(예: 차량이 잠깐 안 보인 것이 오탐인지, 실제 소실인지).

> ⚠️ Stage 0.5는 **실제 RTSP ingest를 파이프라인 입력으로 연결하지 않는다**.  
> 실제 RTSP 프레임을 흘리는 순간부터는 Stage 1로 간주한다.

---

## Stage 정의 요약 (경계 명확화)

- **Stage 0**  
  decode(dummy/synthetic) → dummy detection → output  
  → 파이프라인 구조/소유권/큐 안정성 검증

- **Stage 0.5 (현재)**  
  Stage 0 구조 유지 +  
  **Global Meta / Node Runtime State / Logger** 추가  
  → “관찰 가능성 + 상호 영향 기반”의 뼈대 고정

- **Stage 1**  
  실제 decode + 실제 detection (CPU + ONNX Runtime)  
  → InputSourceNode가 파이프라인 입력으로 연결됨

- **Stage 2**  
  TensorRT + GPU 최적화  
  → device-resident pipeline, copy 최소화

---

## 핵심 설계 가드레일 (절대 깨지면 안 되는 것)

### 1. 소유권 규칙 (Stage 0 철학 유지)
- **Store가 메모리의 유일한 소유자**
- 노드는 포인터/handle만 전달
- 동일 데이터 중복 생성으로 정합성 문제가 생기면 실패로 간주
- OutputNode는 **release chain의 종착점**
  - object → buffer → frame

### 2. Queue 규칙
- Queue에는 **포인터만** 들어간다.
- Overflow 정책은 **DropOldestItem**
- drop된 포인터는 push 호출자가 반환받아 정리한다.

### 3. Meta Store 규칙 (Stage 0.5 핵심)
- key는 **ChannelIdentifier** (source/stream의 정체성)
- `read()`는 **find-only** (절대 create 하지 않는다)
- `write()`는 create 허용
- 값 복사 반환 금지, **구조체 전체 ref 접근 허용**

### 4. Node Runtime State 규칙
- 노드 내부에 카운터를 소유하지 않는다.
- node_instance_id → 중앙 Store에서 조회/갱신
- 다른 노드가 과거/현재 상태를 참고할 수 있어야 한다.

---

## 반드시 알아야 할 주의 사항 (중요)

### Callback under lock (Stage 0.5 한정)
- ChannelRuntimeMetaStore / NodeRuntimeStateStore의 read/write는
  **mutex를 잡은 상태에서 callback을 실행**한다.
- Stage 0.5에서는 단순/안전성을 우선한다.

⚠️ **컨벤션**
- callback은 반드시 짧아야 한다.
- callback 내부에서 다른 store로 재진입(read/write) 금지.

> 장기적으로는 lock 범위를 줄이거나
> atomic/lock-free/RCU/sharding 등을 검토할 수 있으나,
> **Stage 0.5에서는 구현하지 않는다**.

---

## UML 문서 구성 및 권장 열람 순서

Stage 0.5의 동작을 이해하기 위해 아래 UML들을 제공한다.  
기존 UML은 전부 제거하고, 이 세트를 기준으로 한다.

### 권장 열람 순서 (처음 보는 사람 기준)

1. **00_stage0_5_overview_component.puml**  
   → 전체 구성 요소와 관계를 한 장에 파악

2. **01_pipeline_sequence_end_to_end.puml**  
   → decode → detection → output의 실제 실행 흐름

3. **05_class_diagram_core_types.puml**  
   → 핵심 타입, Store 소유권, handle/포인터 관계

4. **08_state_machine_frame_lifecycle.puml**  
   → FrameMetadata 수명과 release chain

5. **03_channel_runtime_meta_ttl_sequence.puml**  
   → A(write) ↔ C(read), TTL 기반 time-window 동작

6. **04_node_runtime_state_store_sequence.puml**  
   → 노드 인스턴스별 카운터/상태 관측 방식

7. **02_queue_dropoldest_sequence.puml**  
   → DropOldestItem 발생 시 동작

8. **07_activity_stage0_5_runtime_flow.puml**  
   → 스레드/큐/스토어 관점의 전체 런타임 흐름

9. **10_extension_points_future_stage1_2.puml**  
   → Stage 1 / Stage 2로의 확장 방향

---

## 최소 필독 UML 세트 (시간 없을 때)

아래 **5개만 읽어도 Stage 0.5를 이해할 수 있다**:

- `00_stage0_5_overview_component.puml`
- `01_pipeline_sequence_end_to_end.puml`
- `05_class_diagram_core_types.puml`
- `08_state_machine_frame_lifecycle.puml`
- `03_channel_runtime_meta_ttl_sequence.puml`

---

## Stage 1로 넘어가는 명확한 기준

다음 중 하나라도 들어오면 **Stage 1로 전환**한다.

- 실제 RTSP ingest가 파이프라인 입력으로 연결됨
- InputSourceNode가 synthetic이 아닌 실제 source를 다룸
- decode 결과가 “실제 영상 데이터”를 의미하기 시작함

Stage 0.5에서는 **구조와 관찰 가능성**만 고정한다.

---

## 기억해야 할 한 문장 요약

> **Stage 0.5는 성능을 올리는 단계가 아니라,  
> “무엇이 일어났는지 정확히 알 수 있게 만들고  
> 프레임을 넘어서는 상태를 다룰 수 있는 뼈대를 고정하는 단계”다.**
