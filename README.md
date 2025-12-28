# AZZI Data Pipeline Framework

Stage 0.5 데모 파이프라인은 store 단일 소유권 철학을 유지하면서 관찰 가능성(카운터, 메타, 로깅)을 확인하기 위한 스켈레톤이다.

## 빌드
```bash
cmake -S . -B build
cmake --build build
```

## 실행
```bash
./build/azzi_pipeline
```

## 구성 요소
- **FrameStore**: 프레임/버퍼/객체를 단일 소유한다. 노드는 `FrameHandle`만 주고받으며 OutputNode가 release chain을 마무리한다.
- **BoundedPointerQueue**: 포화 시 가장 오래된 항목을 drop하고 호출자에게 알려준다.
- **ChannelRuntimeMetaStore**: 채널 단위 글로벌 메타데이터를 mutex로 보호하며, TTL helper를 통해 존재 여부를 판정할 수 있다.
- **NodeRuntimeStateStore**: 노드 인스턴스별 in/out/drop/error 카운터를 관리한다.
- **Dummy 노드들**: decode → detection → output 순으로 연결되어 meta write/read, drop 로깅, release chain을 검증한다.
