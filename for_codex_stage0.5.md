# [AZZI Data Pipeline Framework] Stage 0.5 작업 지시서 (Codex용)

## 0. 목적
Stage 0의 구조/계약(Store 소유권, handle/포인터 큐, release chain)을 깨지 않고,
Stage 0.5에서 “관찰 가능성(로깅/카운터) + Global Meta(프레임을 넘어 유지되는 time-window 상태)”의 뼈대를 추가한다.

중요: Stage 0.5에서는 실제 RTSP ingest/decode를 파이프라인 입력으로 연결하지 않는다.
(실제 RTSP 프레임을 흘리는 순간은 Stage 1로 간주)

---

## 1. Stage 정의 (변경 금지: 문서/코드 공통 기준)
- Stage 0: decode(dummy/synthetic) → dummy detection → output 뼈대만 검증
- Stage 0.5 (optional): Stage 0 구조 유지 + 가시성(로깅/카운터/메타) 보강
- Stage 1: 실제 decode + 실제 detection 추가, CPU + ONNX Runtime 기반, device 이동은 크게 고려하지 않음
- Stage 2: TensorRT로 교체, GPU 최적화 및 device-resident 방향으로 진행

---

## 2. 절대 지켜야 하는 컨벤션/가드레일 (1달 뒤의 나는 남이다)
### 2.1 네이밍/가독성
- 약자 최소화. 도메인 개념에는 약자 금지.
- 변수명/타입명은 의미가 즉시 드러나야 함.
- “F12 눌러야 알 수 있는 이름” 금지.

### 2.2 주석/문서
- 주석은 과할 정도로. “한 달 뒤의 나는 남이다” 기준.
- 단계(Stage) 경계와 ‘왜 이 설계인지’를 코드 주변에 남길 것.

### 2.3 소유권/정합성 (Stage 0 철학 유지)
- Store가 유일 소유자(single owner). 노드는 ref/handle/포인터만 가진다.
- 동일 데이터의 중복 생성으로 정합성 문제가 생기면 실패로 간주.
- OutputNode의 release chain(object → buffer → frame) 유지.

### 2.4 Stage 0.5 범위 제한
- 구조 변경은 최소화.
- “global meta store / node runtime store / logger” 추가가 핵심.
- InputSource/Decode 분리는 Stage 0.5 2차 커밋(선택). 1차 커밋에서 과도 리팩터 금지.

---

## 3. 현재 Stage 0 코드에서 이미 좋은 것 (훼손 금지)
- FrameMetadata/FrameBuffer/ObjectMetadata: store 소유 + handle/포인터 전달 계약
- BoundedPointerQueue + DropOldestItem에서 drop된 포인터 반환 구조
- TimestampPair(wall + monotonic) 기록 인프라
- OutputNode의 명확한 release chain
- ChannelState에 존재하는 채널 단위 atomic counters(decoded/dropped/output)

---

## 4. Stage 0.5에서 추가해야 할 것 (핵심)
### 4.1 ChannelRuntimeMeta (+ Store)
목적: 프레임을 넘어 유지되는 채널/소스 단위 상태(time window, TTL)를 store가 소유하고,
A 노드가 기록한 meta를 C 노드가 읽고, 필요 시 C가 다시 A의 동작을 바꾸는 기반을 만든다.

#### Key(중요)
- 새 SourceId 타입 만들지 말 것.
- 기존 `ChannelIdentifier`를 key로 사용 (현재 Stage 0에서 사실상 source/stream의 ID 역할을 함).

#### 구조체(단일 struct, 나중에 분리 가능)
- global/frame/window 구분은 Stage 0.5에서 분리하지 말고 “단일 구조체”에 통합
- TTL 기반 time window는 구조체 내부의 last_seen + helper로 구현
  - last_seen_vehicle_monotonic_ns
  - last_seen_person_monotonic_ns
  - IsVehiclePresent(now) / IsPersonPresent(now) (TTL 판정)
- downstream→upstream 제어용 정책 필드는 일단 존재만 하게:
  - decode_fps_limit (default -1)
  - detection_skip_frames (default 0)
- struct_version, struct_size_bytes 포함 (ABI 변화 감지용)

#### Store API (옵션 A: mutex)
- store가 lock 범위를 통제해야 함 (데이터 레이스 방지)
- “값 복사 반환 금지”, “구조체 전체 ref 접근”은 허용
- 권장 API(closure 기반):
  - Read(channel_id, fn(const ChannelRuntimeMeta&))
  - Write(channel_id, fn(ChannelRuntimeMeta&))
- backend는 mutex (옵션 A)로 시작.
- atomic/lock-free 전환은 후보로만 남김.

### 4.2 NodeRuntimeStateStore (노드 인스턴스별 카운터/상태)
목적: 동적 DAG에서 “모든 노드가 모든 stream을 거치지 않는다”를 고려하여,
노드 내부가 아닌 store가 노드 단위 관찰 가능성을 제공한다.

#### 요구사항
- 노드 내부에 counters를 소유하지 말 것 (store에서 조회/갱신)
- key는 node_instance_id (runner가 생성 시 부여, Stage 0.5는 정적이어도 OK)
- 최소 counters: in/out/drop/error (atomic)

#### 참고
- 현재 ChannelState counters는 유지한다(채널 운영지표로 유효).
- NodeRuntimeStateStore는 “노드/인스턴스 관측”을 추가로 제공하는 것.

### 4.3 Logger/Observability
- Node start/stop, drop(reason), error 를 최소 로그로 남김
- 과도한 프레임별 로그 금지: N프레임마다 샘플링 로그 또는 1~2초 summary 로그
- drop은 queue가 아니라 push 호출자(현재 DropOldest 반환이 있는 지점)에서 확정적으로 로깅 가능

---

## 5. 구현 위치/폴더 정책 (중요)
- repo에 이미 `metadata` 폴더가 있으므로 그 구조를 존중해서 추가한다.
- 새로운 최상위 폴더(예: meta/) 만들지 말 것.
- 기존 store 인터페이스/구현 패턴을 그대로 따른다:
  - *StoreInterface + SimpleStore(옵션 A)*

---

## 6. 최소 검증 시나리오 (Stage 0.5 성공 기준)
1) DummyDetectionNode(A)가 ChannelRuntimeMetaStore.Write(...)로 vehicle 관측을 기록
   - frame_id 패턴 등으로 “관측/비관측”을 만들어도 됨
2) OutputNode(C)가 ChannelRuntimeMetaStore.Read(...)로 TTL 기반 vehicle_present를 판정하고
   - 로그/메타 출력(샘플링)으로 확인
3) DropOldest 발생 시 drop 카운터 증가 + 로그 확인
4) 빌드/실행 성공

---

## 7. 커밋 전략 (권장)
### Commit 1: 인프라만 추가
- ChannelRuntimeMeta + Store(인터페이스/간단 구현)
- NodeRuntimeStateStore
- Logger(또는 기존 로그 체계 확장)
- 기존 노드 수정 최소/없음
- 빌드 성공 확인

### Commit 2: 노드 삽입(최소)
- dummy_detection_node.cpp: meta write + node counters 갱신
- output_node.cpp: meta read(TTL) + node counters 갱신
- 실행 로그 확인

### Commit 3 (선택): Stage 0.5-2
- SyntheticInputSourceNode 도입 + DecodeNode 책임 분리(실제 RTSP 주입은 아직 금지)

---

## 8. 후보로만 유지할 항목 (지금 구현 금지, 문서에만 기록)
- GlobalMetaStore backend를 mutex ↔ atomic/lock-free로 전환 가능한 구조
- 빌드 타임 YAML 설정 → 컴파일 타임 #define로 backend 정책 선택
- 이 항목은 다음 stage 요약/README에 반드시 포함할 것
