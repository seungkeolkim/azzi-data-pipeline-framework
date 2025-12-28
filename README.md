# Stream Pipeline Framework (Work in Progress)

## 1. 무엇을 만들고 있는가

이 저장소는 **동적 스트림 파이프라인 프레임워크**를 만들기 위한 출발점이다.

기존 DeepStream과 같은 고정된 영상 분석 파이프라인이 아니라,
다음과 같은 시스템을 만드는 것이 최종 목표다.

- 파이프라인은 **Node들의 DAG(방향 비순환 그래프)** 로 표현된다.
- 각 Node는 **worker(thread)** 로 실행된다.
- 실행 중인 상태에서도:
  - 파이프라인 구조(DAG)를 교체할 수 있고
  - Node를 추가/삭제할 수 있으며
  - 모델이나 처리 단계를 교체할 수 있다.
- 영상(RTSP, TCP frame 등)을 주요 대상으로 하지만,
  **영상에 한정되지 않는 범용 스트림 처리 프레임워크**를 지향한다.

장기적으로는:
- SDK → Framework → Platform → Marketplace
- 사용자가 Airflow의 YAML이나 파이프라인 GUI처럼
  **자유롭게 스트림 파이프라인을 구성·배포**할 수 있는 구조를 목표로 한다.

---

## 2. 범위 및 로드맵

### Stage 0 (완료, 동결됨)

목표:
- 파이프라인의 **골격과 계약**을 검증한다.
- 성능보다 구조적 안정성과 명확성을 우선한다.

구성:
- 단일 입력 소스 (현재는 synthetic frame)
- `Decode → DummyDetection → Output` E2E 파이프라인
- thread / queue / store lifecycle 검증

Stage 0에서 의도적으로 하지 않은 것:
- 실제 RTSP ingest / FFmpeg / GStreamer 연동
- CUDA / zero-copy 최적화
- Triton inference server
- Tracking lifecycle (shadow age, probation age 등)
- 고급 memory allocator

현재 상태:
- CMake + Ninja 빌드 성공
- 약 10초 실행 후 정상 종료
- deadlock / crash 없음

> **Stage 0은 동결되었다.**
> 이후의 복잡한 수정은 Stage 1 이후로 미룬다.

---

### Stage 0.5 (선택 사항)
- Logger 또는 debug print 도입
- Node별 상태/카운터/간단한 타이밍 출력
- 기존 계약을 깨지 않는 선에서 관찰 가능성만 추가

---

### Stage 1 (다음 단계)
- `InputSourceNode`와 `DecodeNode` 분리
- RTSP (`rtsp://localhost:544/video_name`) 입력 지원
- `EncodedVideoPacket` 타입 도입
- DecodeNode는 decode 책임만 가지도록 단순화

---

### Stage 2 (향후)
- TensorRT 기반 추론
- GPU-resident decode / preprocess / infer
- Host↔Device copy 최소화 (zero-copy 지향)
- 동시성 및 backpressure 최적화

---

## 3. 핵심 아키텍처 원칙 (강한 합의)

### 3.1 Node = Worker(Thread)
- 각 Node는 하나의 worker(thread)로 실행된다.
- Node 간 연결은 **함수 호출이 아니라 Queue 기반**이다.
- 초기 단계에서는 throughput보다 **구조의 명확성**을 우선한다.

---

### 3.2 Queue에는 데이터가 아니라 포인터만 전달한다
- Queue에는 frame payload와 같은 큰 데이터를 담지 않는다.
- Queue는 오직:
  - `FrameMetadata*`
  - 또는 기타 가벼운 포인터/핸들만 전달한다.
- 실제 데이터는 Store가 관리한다.

---

### 3.3 Store가 메모리를 소유한다 (명시적 ownership)
메모리 소유권과 수명 관리를 명확히 하기 위해 Store를 분리한다.

- **FrameMetadataStore**
  - `FrameMetadata` 객체 소유
- **FrameBufferStore**
  - 프레임 버퍼(Host/GPU 메모리) 소유
- **ObjectMetadataStore**
  - `ObjectMetadata` 객체 소유

Node의 책임:
- acquire → 사용 → release
- Store가 소유한 메모리를 직접 소유하거나 전달하지 않는다.

---

### 3.4 Drop 정책: 실시간 우선
- Bounded queue의 기본 정책은 **DropOldestItem**이다.
- 오래된 프레임보다 최신 프레임을 우선한다.
- blocking으로 전체 파이프라인이 멈추는 것을 피한다.

---

### 3.5 Sink 실패는 파이프라인을 멈추지 않는다
- 파일 I/O, 출력 지연 등 sink 실패는:
  - 해당 프레임만 drop
  - 전체 시스템 중단 금지

---

## 4. 개발 컨벤션 (반드시 지켜야 함)

### 4.1 약자 사용 금지 (개념적 이름)
- 프로젝트에서 새로 정의하는 개념에는 약자를 사용하지 않는다.
- 한 달 뒤의 나는 남이다.
- 코드만 보고 의미가 바로 드러나야 한다.

예:
- `ObjectMetadataStoreInterface` (O)
- `object_handle` (O)
- `handle` (X)
- `ObjStore` (X)

표준 라이브러리 약자(`std`, `cv` 등)는 예외로 허용한다.

---

### 4.2 주석은 항상 과할 정도로
- C++은 읽기 어렵다.
- 모든 구조체, 계약, 소유권, 의도는 주석으로 설명한다.
- “무엇을 하는지”보다 “왜 이렇게 설계했는지”를 남긴다.

---

### 4.3 Result / Status는 컨텍스트별로 분리
- 전역 `Status`, `Result` 사용 금지
- Queue, Store, Node 등 각 컴포넌트별로
  의미가 분명한 Result enum을 정의한다.

---

## 5. Stage 0 현재 동작 요약

Node 구성:
- **DecodeNode**
  - Stage 0에서는 입력 획득 + decode(현재는 synthetic frame)
- **DummyDetectionNode**
  - 프레임마다 중앙 고정 bbox 1개 생성
- **OutputNode**
  - JSONL 출력
  - PPM 이미지 출력(옵션)
  - 모든 release chain의 최종 지점

중요 원칙:
- Stage 0는 “완성”이 아니라 “검증” 단계다.
- 더 고도화하지 않고, 구조를 유지한 채 Stage 1로 넘어간다.

---

## 6. 빌드 및 실행 (Docker 기준)

1. Dev Docker 이미지 빌드:
```bash
sh run_build_docker_dev_container.sh
```

2. Dev Docker 이미지 실행 및 접속:
```bash
sh run_dev_container.sh
```

3. Pipeline 빌드 (Dev Container 내부):
```bash
sh run_build_program_inside_dev_container.sh 
```

4. Pipeline 실행 (Dev Container 내부):
```bash
sh run_pipeline_inside_dev_container.sh
```