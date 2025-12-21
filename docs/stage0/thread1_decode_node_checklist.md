# Stage 0 – Thread 1 (DecodeNode) 설계 체크리스트 v0.1

## 목적

이 문서는 Stage 0에서 Thread 1(DecodeNode)이 반드시 지켜야 하는
**책임 범위, 데이터 계약, drop/정리 규칙**을 구현 전에 고정하기 위한 기술 문서이다.

구현 전에 문서로 합의함으로써:
- Thread 2/3 추가 시 책임 충돌을 방지하고
- Drop / Buffer release / Metadata lifecycle 혼선을 제거하며
- Stage 2(TensorRT, GPU resident)로 확장 시 구조 변경을 최소화한다.

---

## 1. Stage 0 전제 및 운영 원칙

### 1.1 Stage 0 전제
- 단일 input source (1 channel)
- 실시간 RTSP만 가정
- playback / external timestamp는 **문서상 인지하되 구현하지 않음**
- 성능 최적화보다 구조/계약 검증이 우선

### 1.2 핵심 합의 원칙
- Queue overflow 정책: **DropOldestItem**
- Queue에는 실제 데이터가 아니라 **FrameMetadata 포인터만 전달**
- 프레임 payload는 **handle 기반 접근**
- Sink 실패/지연은 전체 파이프라인 중단 없이 **drop**
- Stage 0에서는 Dummy Detection 사용

---

## 2. Thread 1 (DecodeNode) 역할 정의

### 2.1 Thread 1이 하는 일

1. 입력 스트림(RTSP 또는 stub)에서 프레임을 획득
2. FrameMetadataStoreInterface에서 FrameMetadata 포인터 acquire
3. FrameBufferStoreInterface에서 frame buffer handle acquire
4. frame buffer에 픽셀 데이터 채움 (실 decode 또는 dummy fill)
5. FrameMetadata 필수 필드 세팅
6. FrameMetadata 포인터를 output queue에 push
7. queue overflow 발생 시 drop-oldest 처리 수행

### 2.2 Thread 1이 하지 않는 일 (금지)

- ObjectMetadata 생성
- ObjectMetadata lifecycle 관리
- Output 파일 저장 (image/video/json)
- Tracking, occlusion, shadow/probation 정책

---

## 3. Thread 1 입출력 인터페이스

### 3.1 Inputs
- ChannelState
- GlobalState
- FrameMetadataStoreInterface
- FrameBufferStoreInterface
- BoundedPointerQueue<FrameMetadata*>

### 3.2 Outputs
- 정상 프레임: FrameMetadata* → queue
- overflow 발생 시: drop된 FrameMetadata* 즉시 정리 후 store 반환

---

## 4. FrameMetadata 필드 작성 규칙

Thread 1은 **큐에 push하기 전에** 아래 필드들을 반드시 채워야 한다.

### 4.1 식별자 및 기본 정보

- channel_identifier  
  - 값: ChannelState.channel_identifier

- frame_identifier  
  - 채널 단위 증가값
  - ChannelState 또는 DecodeNode 내부 카운터로 관리 (구현 시 결정)

- stream_time_base  
  - Stage 0: StreamTimeBase::Realtime

- presentation_timestamp  
  - Stage 0: -1 허용
  - RTSP decode 도입 후 PTS 저장 예정

### 4.2 Frame buffer handle

- frame_buffer_handle  
  - FrameBufferStoreInterface::acquire 결과
  - acquire 실패 시:
    - FrameMetadata 즉시 반환
    - 해당 프레임 drop

### 4.3 Timestamp (decode)

- decode_completed_timestamp  
  - Stage 0 구현: buffer fill 완료 직후 기록

#### TODO (중요, 문서 기록만)
- 시스템 영향 최소화를 위해
  - “프레임 수신 시점(receive timestamp)”
  - “decode 완료 시점”
  을 분리 기록하는 것이 이상적이다.
- 그러나 Stage 0에서는 복잡도 증가를 피하기 위해
  단일 decode_completed_timestamp만 사용한다.
- Stage 1/2에서 timestamp 구조 확장 검토.

### 4.4 Drop 관련 필드 초기화

- dropped = false
- drop_reason = nullptr

### 4.5 Object handle 초기화

- object_handles.clear()
- pool 재사용 시 잔존 데이터 방지를 위해 필수

---

## 5. Frame buffer 처리 표준 절차

1. FrameMetadata acquire
2. FrameBufferHandle acquire
3. FrameBufferView view 획득
4. view.data_pointer에 픽셀 데이터 채움
5. FrameMetadata 필드 세팅
6. FrameMetadata 포인터 queue push

Stage 0에서는 실제 decode가 없어도
“버퍼가 실제로 채워지는 흐름”을 유지한다.

---

## 6. Queue overflow (DropOldestItem) 처리 규칙

### 6.1 push 수행

- push_outcome = frame_metadata_queue.push(frame_metadata_pointer)

### 6.2 DropOldest 발생 시

push_outcome.dropped_old_pointer != nullptr 인 경우:

1. (선택) drop 표시
   - dropped = true
   - drop_reason = "frame_queue_full_drop_oldest"

2. frame buffer 정리
   - frame_buffer_store.release(dropped_old_pointer.frame_buffer_handle)

3. FrameMetadata 반환
   - frame_metadata_store.release(dropped_old_pointer)

### 6.3 ObjectMetadata 관련 단순화 계약 (Stage 0)

- Thread 1이 drop하는 FrameMetadata에는
  object_handles가 비어 있음을 계약으로 보장한다.
- ObjectMetadataStoreInterface는 Thread 2 이후 단계에서만 관여한다.
- Stage 1 이후 재검토 가능.

---

## 7. Throttling (Sampling) 규칙

Stage 0 합의:
- throttling은 decode 단계에서 수행

선택지:
1. 시간 기반 throttling (권장, 장기적으로 자연스러움)
2. N 프레임 중 1 프레임 (단순)

Stage 0 구현에서는 (2)로 시작 가능.

---

## 8. 실패 및 예외 처리 원칙

- 어떤 실패도 전체 파이프라인 중단을 유발하지 않는다.
- 실패는 해당 프레임 drop으로 격리한다.

실패 예:
- buffer acquire 실패
- buffer view 실패
- decode 실패
- queue closed

재연결, 재시도 정책은 Stage 0 범위에서 제외한다.

---

## 9. Thread 1 책임 요약 (한 문장)

Thread 1은  
“프레임을 받아서, 메타와 버퍼를 확보하고, 필수 정보만 채운 뒤,
정해진 drop 규칙에 따라 **안전하게 큐로 흘려보내는 것**까지만 책임진다.”

---

## 10. UML (PlantUML)

### 10.1 Sequence Diagram – DecodeNode 정상 흐름 + DropOldest

```plantuml
@startuml
title Stage 0 - Thread 1 (DecodeNode) Sequence

actor InputSource
participant DecodeNode
participant FrameMetadataStore
participant FrameBufferStore
participant FrameQueue

DecodeNode -> FrameMetadataStore : acquire()
FrameMetadataStore --> DecodeNode : FrameMetadata*

DecodeNode -> FrameBufferStore : acquire()
FrameBufferStore --> DecodeNode : FrameBufferHandle

DecodeNode -> FrameBufferStore : view(handle)
FrameBufferStore --> DecodeNode : FrameBufferView

DecodeNode -> InputSource : read/decode frame
InputSource --> DecodeNode : pixels

DecodeNode -> DecodeNode : fill buffer
DecodeNode -> DecodeNode : set metadata fields

DecodeNode -> FrameQueue : push(FrameMetadata*)
FrameQueue --> DecodeNode : PushOutcome

alt DropOldest
    DecodeNode -> FrameBufferStore : release(dropped.frame_buffer_handle)
    DecodeNode -> FrameMetadataStore : release(dropped)
end

@enduml
