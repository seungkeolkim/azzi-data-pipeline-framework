# Stage 0.6 실행 상태 기록

이 문서는 Stage 0.6의 실제 동작 상태를 요약한 런타임 문서다.
Stage 0.5와 다르게, InputSource 분리 후의 실행 흐름을 명시한다.

## 현재 동작 구성
- DummyInputSourceNode가 일정 주기로 입력 바이트를 생성한다
- InputSourceQueueItem을 통해 DecodeNode로 전달한다
- DecodeNode는 입력 아이템을 소비하고 더미 프레임을 생성한다
- DummyDetectionNode는 더미 객체를 붙이고 OutputNode로 전달한다
- OutputNode는 JSONL과 PPM을 출력하고 해제 체인을 수행한다

## 실행 로그 기대값
- 노드 시작 로그
  - DummyInputSourceNode started
  - DecodeNode started
  - DummyDetectionNode started
  - OutputNode started
- 출력 노드에서 TTL 기반 로그가 주기적으로 출력된다
- 종료 시 각 노드 stopped 로그가 출력된다

## 출력 파일
- output/frames.jsonl
  - 프레임별 메타데이터 레코드
- output/frame_*.ppm
  - 샘플 프레임 이미지

## 실패 격리 규칙
- 입력 소스 오류는 해당 아이템 드롭으로 격리한다
- 큐 overflow는 DropOldestItem으로 처리한다
- 파일 쓰기 실패는 해당 프레임 드롭으로 처리한다
- Release 빌드에서 파이프라인 종료는 허용하지 않는다

## 확인 체크리스트
- InputSource -> Decode -> Detection -> Output 연결이 로그로 확인된다
- frames.jsonl과 ppm 파일이 생성된다
- 드롭 로그가 과도하게 발생하지 않는다
