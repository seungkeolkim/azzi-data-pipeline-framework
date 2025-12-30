# Stage 0.6 문서 안내

이 폴더는 Stage 0.6 구조 분리의 설계 의도를 빠르게 복원하기 위한 UML 모음과 설명 문서다.
Stage 0.5 문서 스타일을 참고하되, InputSource 분리와 신규 Store 중심으로 재구성했다.

## UML 목록과 핵심 포인트

1) uml/00_stage0_6_overview_component.puml
- 전체 구성 개요
- InputSource -> Decode -> Detection -> Output 연결과 Store 관계가 한 장에 나온다

2) uml/01_input_source_to_decode_sequence.puml
- 입력 소스에서 디코드로 전달되는 핵심 시퀀스
- Pull/Push 두 경로 모두 포함

3) uml/02_queue_dropoldest_sequence.puml
- 큐가 가득 찼을 때 DropOldestItem 동작을 명시
- 드롭된 항목의 해제 책임이 호출자에게 있음을 강조

4) uml/03_input_source_state_machine.puml
- InputSource의 상태 전이 흐름
- Error/Disconnected를 분리해 관측 가능성을 확보

5) uml/04_store_lifecycle_sequence.puml
- InputSourceDataBufferStore의 소유권/수명 흐름
- 입력 노드와 디코드 노드 사이의 handle 전달 계약을 강조

6) uml/05_frame_metadata_lifecycle.puml
- FrameMetadata와 FrameBuffer의 해제 체인
- Output에서 해제가 완료되는 계약을 강조

7) uml/07_input_source_queue_item_class.puml
- InputSourceQueueItem의 필드 정의
- raw byte chunk 고정, PTS/DTS 슬롯 보존

8) uml/08_thread_model_overview.puml
- 노드 인스턴스 1개 = 스레드 1개 원칙
- 구조적 분리 의도를 시각화

## 추천 읽기 순서
1. 00_stage0_6_overview_component
2. 08_thread_model_overview
3. 01_input_source_to_decode_sequence
4. 04_store_lifecycle_sequence
5. 07_input_source_queue_item_class
6. 02_queue_dropoldest_sequence
7. 05_frame_metadata_lifecycle
8. 03_input_source_state_machine

## 보충 문서
- 실행 상태와 실제 동작 흐름은 `README_RUNTIME.md`에 별도로 기록했다.
